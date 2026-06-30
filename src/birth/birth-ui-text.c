/* File: birth/birth-ui-text.c */

#include "angband.h"
#include "birth/birth-internal.h"

/*
 * Clear the previous question
 */
void clear_question(void)
{
    int i;

Term_erase(TOTAL_AUX_COL, 0, 255);

    for (i = QUESTION_ROW; i < TABLE_ROW; i++)
    {
        /* Clear line, position cursor */
        Term_erase(0, i, 255);
    }
}

void birth_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

bool birth_confirm_input(int ch, bool steamdeck)
{
    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == INPUT_BIND_CONFIRM)
        return true;

    if (steamdeck && ch == steamdeck_confirm_key())
        return true;

    return false;
}

bool birth_confirm_unspent_stat_points(int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    int message_row;
    int prompt_row;
    char key;
    char buf[160];
    char warning_buf[160];
    bool confirmed = false;

    if (points_left <= 0)
        return true;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    prompt_row = birth_prompt_row();
    message_row = prompt_row - 1;
    if (message_row < 0)
        message_row = 0;

    Term_erase(0, message_row, 255);
    Term_erase(0, prompt_row, 255);

    strnfmt(warning_buf, sizeof(warning_buf),
        "%d unassigned attribute point%s. Continue anyway?", points_left,
        (points_left == 1) ? "" : "s");
    birth_put_str_fit(TERM_YELLOW, warning_buf, message_row, 1);

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));

        strnfmt(buf, sizeof(buf), "%s continue  %s keep allocating",
            confirm_label, back_label);
    }
    else
    {
        if (sdl_touch_only_device_active())
            strnfmt(buf, sizeof(buf),
                "Tap Yes to continue  No to keep allocating");
        else
            strnfmt(buf, sizeof(buf),
                "Enter/y continue  n/Esc keep allocating");
    }

    birth_put_str_fit(TERM_SLATE, buf, prompt_row, 1);
    ui_scroll_area_clear();
    ui_menu_click_begin();
    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, confirm_label);
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "continue");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, back_label);
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "keep allocating");
    }
    else
    {
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "SPACE/y");
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "Enter/y");
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "Yes");
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "continue");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "n/ESC");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "n/Esc");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "No");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "keep allocating");
    }
    sdl_touch_pane_begin_yes_no_prompt(warning_buf);
    Term_fresh();

    while (1)
    {
        int clicked_choice = -1;

        hide_cursor = true;
        key = inkey();
        hide_cursor = false;
        key = (char)steamdeck_menu_key(key, 0, 0);

        if (ui_menu_click_take(&clicked_choice))
        {
            confirmed = (clicked_choice != 0);
            break;
        }

        if (birth_confirm_input(key, steamdeck))
        {
            confirmed = true;
            break;
        }
        if (key == 'y' || key == 'Y')
        {
            confirmed = true;
            break;
        }
        if (key == 'n' || key == 'N' || key == ESCAPE)
            break;

        bell("Confirm or cancel the stat warning.");
    }

    sdl_touch_pane_end_yes_no_prompt();
    ui_menu_click_clear();
    return confirmed;
}

int character_choice_index_by_name(cptr choice_name)
{
    int character_idx;

    if (!choice_name)
        return -1;

    for (character_idx = 0; character_idx < z_info->c_max; character_idx++)
    {
        if (!strcmp(choice_name, c_name + c_info[character_idx].name))
            return character_idx;
    }

    return -1;
}

bool character_description_has_room(void)
{
    int min_description_rows = 8;
    int available_rows = birth_prompt_row() - birth_description_base_row();

    return (available_rows >= min_description_rows);
}

bool character_selection_tight_height(void)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    return (hgt <= 18);
}

bool character_flags_need_compact_layout(void)
{
    int wid = 80;
    int hgt = 24;
    int min_flags_wid = TOTAL_AUX_COL + 21 + 20;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;

    return (wid < min_flags_wid) || character_selection_tight_height();
}

cptr character_selection_header_text(bool character_phase)
{
    if (character_phase && character_flags_need_compact_layout())
        return "Character:";

    return "Character Selection:";
}

void draw_character_selection_header(bool character_phase)
{
    cptr header = character_selection_header_text(character_phase);

    Term_erase(0, HEADER_ROW, 255);
    Term_putstr(QUESTION_COL, HEADER_ROW, -1, TERM_L_BLUE, header);
}

int birth_prompt_row(void)
{
    int wid = 80;
    int hgt = 24;
    int row;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    row = hgt - 1;
    if (row < TABLE_ROW)
        row = TABLE_ROW;

    return row;
}

int birth_description_base_row(void)
{
    int wid = 80;
    int hgt = 24;
    int row;
    int min_row = TABLE_ROW + A_MAX + 3;
    int max_row = birth_prompt_row() - 1;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    row = hgt - 5;
    if (row > DESCRIPTION_ROW)
        row = DESCRIPTION_ROW;
    if (row < min_row)
        row = min_row;
    if (row > max_row)
        row = max_row;
    if (row < TABLE_ROW + 1)
        row = TABLE_ROW + 1;

    return row;
}

int choice_description_row(int visible_rows, bool allow_full_description_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = birth_description_base_row();
    int min_row;

    (void)wid;

    if (allow_full_description_screen)
        return row;

    Term_get_size(&wid, &hgt);
    if (hgt < 1)
        hgt = 24;

    if (hgt > 20)
        return row;

    /* On short screens, use the first free row after the list. */
    min_row = TABLE_ROW + visible_rows;
    row = min_row;

    if (row > birth_prompt_row() - 1)
        row = birth_prompt_row() - 1;
    if (row < min_row)
        row = min_row;

    return row;
}

int birth_wrap_col(int indent)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;
    if (indent < 0)
        indent = 0;

    if (wid <= indent + 1)
        return indent + 1;

    return wid - 1;
}

cptr birth_wrap_line(cptr text, int width, char *buf, size_t buflen);

/*
 * UTF-8 text is stored as bytes in Term but packed into display cells by SDL.
 * Force rows that mix old/new accented text to repaint even when byte cells
 * look unchanged.
 */
void birth_invalidate_cells(int col, int row, int width)
{
    if (!Term || !Term->old || row < 0 || row >= Term->hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= Term->wid || width <= 0)
        return;
    if (col + width > Term->wid)
        width = Term->wid - col;

    for (int x = col; x < col + width; x++)
    {
        Term->old->a[row][x] = 255;
        Term->old->c[row][x] = 0;
        Term->old->ta[row][x] = 255;
        Term->old->tc[row][x] = 0;
        Term->old->story[row][x] = 255;
    }

    if (row < Term->y1)
        Term->y1 = row;
    if (row > Term->y2)
        Term->y2 = row;
    if (col < Term->x1[row])
        Term->x1[row] = col;
    if (col + width - 1 > Term->x2[row])
        Term->x2[row] = col + width - 1;
}

int birth_utf8_prefix_len(cptr text, int max_cols)
{
    int bytes = 0;
    int cols = 0;

    if (!text || max_cols <= 0)
        return 0;

    while (text[bytes])
    {
        int char_len = utf8_sequence_len(text + bytes);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        cols += char_width;
        bytes += char_len;
    }

    return bytes;
}

int birth_wrapped_line_count(cptr text, int indent)
{
    if (!text || !text[0])
        return 0;

    return count_wrapped_lines(text, birth_wrap_col(indent), indent);
}

void birth_put_wrapped_text(byte attr, cptr text, int row, int col)
{
    int wid = 80;
    int hgt = 24;
    int width;
    char line_buf[512];
    cptr rest;

    if (!text || !text[0])
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    if (col < 0)
        col = 0;
    if (col >= wid || row >= hgt)
        return;

    width = birth_wrap_col(col) - col;
    if (width < 1)
        width = 1;

    rest = text;
    while (rest && rest[0] && row < hgt)
    {
        rest = birth_wrap_line(rest, width, line_buf, sizeof(line_buf));
        Term_erase(col, row, width);
        Term_putstr(col, row, -1, attr, line_buf);
        birth_invalidate_cells(col, row, width);
        row++;
    }
}

void birth_put_str_fit(byte attr, cptr text, int row, int col)
{
    int wid = 80;
    int hgt = 24;
    int max_len;
    char buf[256];
    int text_width;
    int copy_len;

    if (!text || !text[0])
        return;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;
    if (col < 0)
        col = 0;
    if (col >= wid)
        return;

    max_len = wid - col;
    if (max_len < 1)
        return;

    text_width = utf8_display_width_n(text, (int)strlen(text));
    if (text_width <= max_len)
    {
        SDL_strlcpy(buf, text, sizeof(buf));
    }
    else
    {
        copy_len = birth_utf8_prefix_len(text, (max_len > 3) ? (max_len - 3) : max_len);
        if (copy_len < 0)
            copy_len = 0;
        if (copy_len >= (int)sizeof(buf))
            copy_len = (int)sizeof(buf) - 1;

        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        if (max_len > 3)
            SDL_strlcat(buf, "...", sizeof(buf));
    }

    Term_putstr(col, row, -1, attr, buf);
}

cptr birth_wrap_line(cptr text, int width, char *buf, size_t buflen)
{
    cptr start;
    cptr end;
    cptr last_space = NULL;
    cptr next;
    int cols = 0;
    size_t copy_len;

    if (!buf || buflen == 0)
        return text;

    buf[0] = '\0';

    if (!text)
        return text;

    while (*text == ' ')
        text++;

    if (!text[0])
        return text;

    if (width < 1)
        width = 1;

    start = text;
    end = start;

    while (*end && *end != '\n')
    {
        int char_len = utf8_sequence_len(end);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(end, char_len);
        if (char_width > 0 && cols + char_width > width)
            break;

        if (*end == ' ')
            last_space = end;
        cols += char_width;
        end += char_len;
    }

    if (*end == '\n' || !*end)
    {
        next = end;
    }
    else if (last_space && last_space > start)
    {
        end = last_space;
        next = last_space;
    }
    else
    {
        next = end;
    }

    while (end > start && end[-1] == ' ')
        end--;

    copy_len = (size_t)(end - start);
    if (copy_len >= buflen)
        copy_len = buflen - 1;
    copy_len = (size_t)utf8_safe_prefix_len(start, (int)copy_len);

    memcpy(buf, start, copy_len);
    buf[copy_len] = '\0';

    while (*next == ' ')
        next++;

    if (*next == '\n')
        next++;

    while (*next == ' ')
        next++;

    return next;
}

int birth_wrapped_entry_lines(cptr entries[], int entry_n, int width,
    int max_entries)
{
    int total = 0;
    int limit = entry_n;
    char line_buf[64];

    if (max_entries >= 0 && max_entries < limit)
        limit = max_entries;

    for (int i = 0; i < limit; ++i)
    {
        cptr rest = entries[i];

        while (rest && rest[0])
        {
            rest = birth_wrap_line(rest, width, line_buf, sizeof(line_buf));
            total++;
        }
    }

    return total;
}

static int choice_description_line_count(cptr text)
{
    return birth_wrapped_line_count(text, 2);
}

int choice_visible_capacity(int num, cptr text,
    bool allow_full_description_screen)
{
    int wid = 80;
    int term_hgt = 24;
    int description_row = birth_description_base_row();
    int hgt = birth_prompt_row() - TABLE_ROW - 1;

    Term_get_size(&wid, &term_hgt);
    (void)wid;
    if (term_hgt < 1)
        term_hgt = 24;
    if (hgt < 0)
        hgt = 0;

    if (allow_full_description_screen)
    {
        bool compact_flags = character_flags_need_compact_layout();
        bool very_short = (term_hgt <= 20);

        if (compact_flags && very_short)
        {
            int max_hgt = (description_row - 2) - TABLE_ROW;

            if (max_hgt < 0)
                max_hgt = 0;
            if (hgt > max_hgt)
                hgt = max_hgt;
        }
    }
    else if (term_hgt <= 20)
    {
        int min_description_rows = (term_hgt <= 18) ? 5 : 4;
        int description_rows = choice_description_line_count(text);
        int max_hgt;

        if (description_rows > min_description_rows)
            min_description_rows = description_rows;

        max_hgt = birth_prompt_row() - TABLE_ROW - 2 - min_description_rows;
        if (max_hgt < 0)
            max_hgt = 0;
        if (hgt > max_hgt)
            hgt = max_hgt;
    }

    if (num < 1)
        return 0;
    if (num < hgt + 1)
        return num;

    return hgt + 1;
}

int choice_description_fit_row(int row, int visible_rows, cptr text)
{
    int min_row = TABLE_ROW + visible_rows + 1;
    int max_row = birth_prompt_row() - 1;
    int text_rows = choice_description_line_count(text);

    if (text_rows > 0)
    {
        int fit_row = birth_prompt_row() - text_rows;

        if (fit_row < max_row)
            max_row = fit_row;
    }

    if (max_row < TABLE_ROW + 1)
        max_row = TABLE_ROW + 1;
    if (max_row < min_row)
        max_row = min_row;
    if (row > max_row)
        row = max_row;
    if (row < min_row)
        row = min_row;

    return row;
}
