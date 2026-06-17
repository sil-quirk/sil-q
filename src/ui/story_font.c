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
    int term_wid = 80;
    int term_hgt = 24;
    int cell_width = sdl_get_cell_width();
    int wrap_pixels = wrap_cols * cell_width;
    int indent_pixels = indent * cell_width;
    int space_pixels = sdl_story_font_text_width(" ", 1);
    if (space_pixels <= 0)
        space_pixels = cell_width;

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    (void)term_hgt;

    int lines = 1;
    int x = indent;
    int x_pixels = indent_pixels;
    cptr s = str;

    while (*s)
    {
        /* Handle newlines */
        if (*s == '\n')
        {
            x = indent;
            x_pixels = indent_pixels;
            lines++;
            s++;
            continue;
        }

        /* Skip leading spaces */
        while (*s == ' ')
        {
            x++;
            x_pixels += space_pixels;
            s++;
        }

        if (!*s)
            break;

        /* Find the end of the current word.  The length is in bytes because
         * SDL_ttf expects UTF-8 byte strings and the terminal grid still
         * stores one byte per cell. */
        cptr word_start = s;
        int word_bytes = 0;
        while (s[word_bytes] && s[word_bytes] != ' ' && s[word_bytes] != '\n')
            word_bytes++;

        if (word_bytes == 0)
            continue;

        /* Measure the word in pixels */
        int word_pixels = sdl_story_font_text_width(word_start, word_bytes);
        bool exceeds_pixels = (x > indent && (x_pixels + word_pixels) > wrap_pixels);
        bool exceeds_columns = (x + word_bytes >= term_wid);

        if (exceeds_pixels || exceeds_columns)
        {
            x = indent;
            x_pixels = indent_pixels;
            lines++;
        }

        /* Advance by the word's pixel width */
        x += word_bytes;
        x_pixels += word_pixels;

        /* Move past the word */
        s += word_bytes;
    }

    return lines;
}

static bool story_term_is_main(void)
{
    return (Term && term_screen && (Term == term_screen));
}

bool story_inventory_enabled(void)
{
    return story_term_is_main() ? true : story_inventory_lists_pane;
}

bool story_equipment_enabled(void)
{
    return story_term_is_main() ? true : story_equipment_lists_pane;
}

bool story_look_enabled(void) { return true; }
bool story_character_enabled(void) { return true; }

bool story_monster_desc_enabled(void)
{
    return story_term_is_main() ? story_monster_desc_main : story_monster_desc_pane;
}

bool story_object_desc_enabled(void) { return story_object_desc; }

void story_font_term_push_slot(bool active, bool grid, int slot, story_font_term_state* prev)
{
    if (!prev)
        return;

    prev->t = Term;
    prev->active = (Term ? Term->story_font_active : false);
    prev->grid = (Term ? Term->story_font_grid : false);
    prev->slot = (Term ? Term->story_font_slot : 0);

    if (Term)
    {
        Term->story_font_active = active;
        Term->story_font_grid = grid;
        Term->story_font_slot = slot;
    }
}

void story_font_term_push(bool active, bool grid, story_font_term_state* prev)
{
    story_font_term_push_slot(active, grid, STORY_FONT_SLOT_DEFAULT, prev);
}

void story_font_term_pop(story_font_term_state* prev)
{
    if (!prev)
        return;

    if (prev->t)
    {
        prev->t->story_font_active = prev->active;
        prev->t->story_font_grid = prev->grid;
        prev->t->story_font_slot = prev->slot;
    }
}

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
    int indent_pixels = text_out_indent * cell_width;
    int space_pixels = sdl_story_font_text_width(" ", 1);
    if (space_pixels <= 0)
        space_pixels = cell_width;

    log_trace("=== text_out_to_screen_story START ===");
    log_trace("Story wrapping: wid=%d, wrap_cols=%d, cell_width=%d, wrap_pixels=%d", wid, wrap_cols, cell_width,
        wrap_pixels);
    log_trace("Initial cursor: x=%d, y=%d, text_out_indent=%d, text='%.50s'", x, y, text_out_indent, str);

    int current_x_pixels = indent_pixels;

    s = str;
    while (*s)
    {
        if (*s == '\n')
        {
            x = text_out_indent;
            current_x_pixels = indent_pixels;
            y++;
            Term_erase(x, y, 255);
            s++;
            continue;
        }

        while (*s == ' ')
        {
            Term_addch(a, ' ');
            x++;
            current_x_pixels += space_pixels;
            s++;
        }

        cptr word_start = s;
        int word_bytes = 0;
        while (s[word_bytes] && s[word_bytes] != ' ' && s[word_bytes] != '\n')
            word_bytes++;

        if (word_bytes == 0)
            continue;

        int word_pixels = sdl_story_font_text_width(word_start, word_bytes);

        bool exceeds_pixels = (x > text_out_indent && (current_x_pixels + word_pixels) > wrap_pixels);
        bool exceeds_columns = (x + word_bytes >= wid);

        log_trace(
            "Word: '%.*s' (%d bytes), pixels=%d, current_x_pixels=%d, current_term_col=%d, exceeds_pixels=%s, exceeds_columns=%s",
            word_bytes, word_start, word_bytes, word_pixels, current_x_pixels, x,
            exceeds_pixels ? "YES" : "NO", exceeds_columns ? "YES" : "NO");

        if (exceeds_pixels || exceeds_columns)
        {
            x = text_out_indent;
            current_x_pixels = indent_pixels;
            y++;
            Term_erase(x, y, 255);
            log_trace("Wrapped to next line, x=%d, current_x_pixels=%d", x, current_x_pixels);
        }

        Term_addstr(word_bytes, a, word_start);
        x += word_bytes;

        log_trace("After word output: term_col=%d, pixel_pos=%d, word_pixels=%d, new_pixel_pos=%d", x,
            current_x_pixels, word_pixels, current_x_pixels + word_pixels);

        current_x_pixels += word_pixels;
        s += word_bytes;
    }

    log_trace("=== text_out_to_screen_story END === Final term_col=%d, final_pixel_pos=%d", x, current_x_pixels);
}

static void overlay_story_seed_line(char* line, int line_size, int y, int x,
    int* line_len)
{
    int len;

    if (!line || line_size <= 0 || !line_len)
        return;

    len = MAX(text_out_indent, x);
    if (len >= line_size)
        len = line_size - 1;

    for (int i = 0; i < len; i++)
        line[i] = ' ';

    if (Term && Term->scr && y >= 0 && y < Term->hgt)
    {
        int start = MAX(0, text_out_indent);
        int end = MIN(len, Term->wid);

        for (int i = start; i < end; i++)
        {
            char ch = Term->scr->c[y][i];
            line[i] = ch ? ch : ' ';
        }
    }

    line[len] = '\0';
    *line_len = len;
}

/*
 * Like text_out_to_screen_story, but wraps to the description overlay panel's
 * pixel width using the active term's story-font slot.  The terminal grid still
 * advances one column per byte (the overlay flows the captured columns
 * proportionally at render time); only the wrap decision is pixel-based, so the
 * proportional text fills the panel instead of breaking at the monospace
 * column budget.
 */
void text_out_to_screen_overlay_story(byte a, cptr str)
{
    int x, y;
    int wid, h;
    cptr s;

    /* Mirror of the characters written to the current grid row, measured as a
     * whole so the wrap matches how the overlay renders the line (which also
     * measures contiguous runs rather than summing isolated words). */
    char line[1024];
    int line_len = 0;

    (void)Term_get_size(&wid, &h);
    (void)Term_locate(&x, &y);

    int slot = (Term ? Term->story_font_slot : 0);
    int wrap_pixels = sdl_description_overlay_text_px();
    if (wrap_pixels <= 0)
        wrap_pixels = wid * sdl_get_cell_width();

    overlay_story_seed_line(line, sizeof(line), y, x, &line_len);

    s = str;
    while (*s)
    {
        if (*s == '\n')
        {
            x = text_out_indent;
            y++;
            Term_erase(x, y, 255);
            overlay_story_seed_line(line, sizeof(line), y, x, &line_len);
            s++;
            continue;
        }

        /* Count the run of spaces that precedes the next word. */
        int space_bytes = 0;
        while (s[space_bytes] == ' ')
            space_bytes++;

        cptr word_start = s + space_bytes;
        int word_bytes = 0;
        while (word_start[word_bytes] && word_start[word_bytes] != ' '
            && word_start[word_bytes] != '\n')
        {
            word_bytes++;
        }

        if (word_bytes == 0)
        {
            /* Trailing spaces with no following word: emit and stop. */
            for (int i = 0; i < space_bytes; i++)
            {
                Term_addch(a, ' ');
                x++;
            }
            s += space_bytes;
            continue;
        }

        /* Would the spaces + word fit on the current line? Measure the whole
         * prospective line, not the word alone. */
        bool fits = true;
        if (line_len > text_out_indent)
        {
            if (line_len + space_bytes + word_bytes >= (int)sizeof(line))
            {
                fits = false;
            }
            else
            {
                int probe_len = line_len;
                char saved = line[probe_len];
                for (int i = 0; i < space_bytes; i++)
                    line[probe_len + i] = ' ';
                memcpy(line + probe_len + space_bytes, word_start, word_bytes);
                int probe_total = probe_len + space_bytes + word_bytes;
                line[probe_total] = '\0';
                int probe_px =
                    sdl_description_overlay_story_text_width(line, probe_total, slot);
                line[probe_len] = saved;
                line[line_len] = '\0';
                if (probe_px > wrap_pixels)
                    fits = false;
            }
        }

        bool exceeds_columns = (x + space_bytes + word_bytes >= wid);

        if ((!fits || exceeds_columns) && line_len > text_out_indent)
        {
            /* Wrap: drop the inter-word spaces, start the word on a new line. */
            x = text_out_indent;
            y++;
            Term_erase(x, y, 255);
            overlay_story_seed_line(line, sizeof(line), y, x, &line_len);
        }
        else
        {
            /* Keep the spaces between words on the same line. */
            for (int i = 0; i < space_bytes; i++)
            {
                Term_addch(a, ' ');
                x++;
                if (line_len < (int)sizeof(line) - 1)
                    line[line_len++] = ' ';
            }
            line[line_len] = '\0';
        }

        Term_addstr(word_bytes, a, word_start);
        x += word_bytes;
        for (int i = 0; i < word_bytes && line_len < (int)sizeof(line) - 1; i++)
            line[line_len++] = word_start[i];
        line[line_len] = '\0';

        s += space_bytes + word_bytes;
    }
}

static void story_print_text_internal(int row, int col, int max_cols, byte attr, cptr text, bool force_grid)
{
    int term_wid = 0;
    int term_hgt = 0;

    if (!text)
        text = "";

    if (!Term)
        return;

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid <= 0 || term_hgt <= 0)
        return;

    if (row < 0 || row >= term_hgt || col < 0 || col >= term_wid)
        return;

    if (max_cols > 0)
    {
        int remaining = term_wid - col;
        if (remaining <= 0)
            return;
        if (max_cols > remaining)
            max_cols = remaining;
    }

    if (max_cols > 0)
        Term_erase(col, row, max_cols);

    if (sdl_is_story_font_enabled())
    {
        log_debug("story_print_text: STORY FONT at row=%d col=%d max_cols=%d text='%.50s'", row, col, max_cols, text);
        log_trace("story_print_text: text_out_indent (BEFORE)=%d, text_out_wrap (BEFORE)=%d", text_out_indent,
            text_out_wrap);

        bool previous_grid = (Term ? Term->story_font_grid : false);
        bool restore_grid = false;
        if (force_grid != previous_grid)
        {
            if (Term)
                Term->story_font_grid = force_grid;
            restore_grid = true;
        }

        int old_indent = text_out_indent;
        int old_wrap = text_out_wrap;
        void (*old_hook)(byte, cptr) = text_out_hook;

        if (max_cols > 0)
            text_out_wrap = MIN(term_wid, col + max_cols);
        else
            text_out_wrap = term_wid;

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

        if (restore_grid && Term)
            Term->story_font_grid = previous_grid;
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

    story_font_term_state prev;
    story_font_term_push(false, (Term ? Term->story_font_grid : false), &prev);
    c_put_str(attr, text, row, col);
    story_font_term_pop(&prev);
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
