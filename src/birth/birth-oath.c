/* File: birth/birth-oath.c */

#include "angband.h"
#include "birth/birth-internal.h"

static bool oath_menu_use_compact_layout(void)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    return (wid < 80) || (hgt < 24);
}

static int oath_selectable_max_id(void)
{
    int max_oath_id = OATH_LIGHT;

    if (!z_info)
        return max_oath_id;
    if (z_info->oath_max <= 1)
        return 0;
    if (max_oath_id >= z_info->oath_max)
        max_oath_id = z_info->oath_max - 1;
    if (max_oath_id < 0)
        max_oath_id = 0;

    return max_oath_id;
}

static int oath_collect_visible(int available_mask, int* visible_oaths, int max_visible)
{
    int visible_count = 0;
    int max_oath_id = oath_selectable_max_id();

    if (visible_oaths && visible_count < max_visible)
        visible_oaths[visible_count] = 0;
    visible_count++;

    for (int i = 1; i <= max_oath_id; i++)
    {
        if (!(available_mask & (1 << (i - 1))) && !oath_banned(i))
            continue;

        if (visible_oaths && visible_count < max_visible)
            visible_oaths[visible_count] = i;

        visible_count++;
    }

    return visible_count;
}

static bool oath_option_selectable(int oath_id, int available_mask)
{
    if (oath_id == 0)
        return true;

    return ((available_mask & (1 << (oath_id - 1))) != 0) && !oath_banned(oath_id);
}

static void oath_move_highlight(int* highlight, int direction, int available_mask)
{
    int oath_max = oath_selectable_max_id() + 1;
    int original = *highlight;
    int next = *highlight;

    if (oath_max <= 0)
    {
        *highlight = 0;
        return;
    }

    do
    {
        next += direction;
        if (next < 0)
            next = oath_max - 1;
        if (next >= oath_max)
            next = 0;

        if ((next == 0)
            || (available_mask & (1 << (next - 1)))
            || oath_banned(next))
        {
            *highlight = next;
            return;
        }
    } while (next != original);
}

static void oath_center_putstr(int row, byte attr, cptr text)
{
    int wid = 80;
    int hgt = 24;
    int col;

    if (!text)
        text = "";

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;

    col = (wid - utf8_display_width_n(text, (int)strlen(text))) / 2;
    if (col < 0)
        col = 0;

    Term_putstr(col, row, -1, attr, text);
}

static void oath_draw_page_indicator(int page, int page_count, int wid, int row)
{
    char page_buf[16];
    int col;

    if (page_count <= 1)
        return;

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", page + 1, page_count);
    col = wid - (int)strlen(page_buf) - 1;
    if (col < 0)
        col = 0;

    Term_putstr(col, row, -1, TERM_WHITE, page_buf);
}

static void oath_putstr_fit(int col, int row, int max_width, byte attr, cptr text)
{
    char buf[256];
    int text_width;
    int copy_len;

    if (max_width <= 0)
        return;

    if (!text)
        text = "";

    text_width = utf8_display_width_n(text, (int)strlen(text));
    if (text_width <= max_width)
    {
        Term_putstr(col, row, -1, attr, text);
        return;
    }

    if (max_width <= 3)
    {
        copy_len = birth_utf8_prefix_len(text, max_width);
        if (copy_len >= (int)sizeof(buf))
            copy_len = (int)sizeof(buf) - 1;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
    }
    else
    {
        copy_len = birth_utf8_prefix_len(text, max_width - 3);
        if (copy_len >= (int)sizeof(buf))
            copy_len = (int)sizeof(buf) - 1;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        SDL_strlcat(buf, "...", sizeof(buf));
    }

    Term_putstr(col, row, -1, attr, buf);
}

static void oath_render_virtual_line(int col, int max_width, int* draw_row,
    int row_limit, int* virtual_row, int skip_lines, byte color, cptr text,
    bool render)
{
    if (!text)
        text = "";

    if ((*virtual_row >= skip_lines) && render && (*draw_row < row_limit))
        oath_putstr_fit(col, *draw_row, max_width, color, text);

    if ((*virtual_row >= skip_lines) && (*draw_row < row_limit))
        (*draw_row)++;

    (*virtual_row)++;
}

static void oath_render_virtual_wrapped_text(cptr text, int col, int max_width,
    int* draw_row, int row_limit, int* virtual_row, int skip_lines,
    byte color, bool render)
{
    char line_buffer[512];
    const char* text_ptr = text;

    if (!text || !text[0])
        return;

    if (max_width <= 0)
    {
        int term_width = 80;
        int term_height = 24;

        Term_get_size(&term_width, &term_height);
        (void)term_height;
        if (term_width < 1)
            term_width = 80;
        max_width = term_width - col - 2;
    }

    if (max_width < 8)
        max_width = 8;

    while (*text_ptr)
    {
        const char* line_start;
        const char* line_end;
        const char* last_space = NULL;
        cptr next_ptr;
        int cols = 0;
        int copy_len;

        while (*text_ptr == ' ')
            text_ptr++;

        if (*text_ptr == '\n')
        {
            oath_render_virtual_line(col, max_width, draw_row, row_limit,
                virtual_row, skip_lines, color, "", render);
            text_ptr++;
            continue;
        }

        line_start = text_ptr;
        line_end = text_ptr;

        while (*line_end && *line_end != '\n')
        {
            int char_len = utf8_sequence_len(line_end);
            int char_width;

            if (char_len <= 0)
                break;

            char_width = utf8_display_width_n(line_end, char_len);
            if (char_width > 0 && cols + char_width > max_width)
                break;

            if (*line_end == ' ')
                last_space = line_end;

            cols += char_width;
            line_end += char_len;
        }

        if (*line_end == '\n')
        {
            next_ptr = line_end + 1;
        }
        else if (*line_end && last_space && last_space > line_start)
        {
            next_ptr = last_space;
            line_end = last_space;
        }
        else
        {
            next_ptr = line_end;
        }

        while (line_end > line_start && line_end[-1] == ' ')
            line_end--;

        copy_len = (int)(line_end - line_start);
        if (copy_len >= (int)sizeof(line_buffer))
            copy_len = (int)sizeof(line_buffer) - 1;
        copy_len = utf8_safe_prefix_len(line_start, copy_len);
        if (copy_len < 0)
            copy_len = 0;

        SDL_memcpy(line_buffer, line_start, (size_t)copy_len);
        line_buffer[copy_len] = '\0';
        oath_render_virtual_line(col, max_width, draw_row, row_limit,
            virtual_row, skip_lines, color, line_buffer, render);

        text_ptr = next_ptr;
    }
}

static int oath_render_detail_content(int oath_id, int col, int start_row,
    int max_width, int row_limit, int skip_lines, bool render)
{
    int draw_row = start_row;
    int virtual_row = 0;
    char line_buf[768];

    if (oath_id < 0 || !z_info || oath_id >= z_info->oath_max)
        return 0;

    if (oath_banned(oath_id) && oath_id > 0)
    {
        char* banned_text = oath_banned_text(oath_id);

        oath_render_virtual_line(col, max_width, &draw_row, row_limit, &virtual_row,
            skip_lines, TERM_L_RED, "OATH BROKEN", render);

        if (banned_text && banned_text[0])
        {
            oath_render_virtual_wrapped_text(banned_text, col, max_width,
                &draw_row, row_limit, &virtual_row, skip_lines, TERM_RED, render);
        }
        else
        {
            oath_render_virtual_wrapped_text(
                "Thy oath lies shattered, and thy name is marked in shame for this age.",
                col, max_width, &draw_row, row_limit, &virtual_row, skip_lines,
                TERM_RED, render);
        }

        return virtual_row;
    }

    if (oath_id == 0)
    {
        oath_render_virtual_wrapped_text("Walk free of binding words.", col,
            max_width, &draw_row, row_limit, &virtual_row, skip_lines,
            TERM_SLATE, render);
        oath_render_virtual_wrapped_text(
            "Take no oath and remain unbound by sacred vows.", col, max_width,
            &draw_row, row_limit, &virtual_row, skip_lines, TERM_SLATE, render);
        return virtual_row;
    }

    if (oath_description(oath_id) && oath_description(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Description: %s",
            oath_description(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_SLATE, render);
    }

    if (oath_pledge(oath_id) && oath_pledge(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Pledge: %s", oath_pledge(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_L_BLUE, render);
    }

    if (oath_reward_text(oath_id) && oath_reward_text(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Reward: %s",
            oath_reward_text(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_L_GREEN, render);
    }

    if (oath_forbidden(oath_id) && oath_forbidden(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Forbidden: %s",
            oath_forbidden(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_L_RED, render);
    }

    return virtual_row;
}

static void oath_add_semantic_detail_line(cptr label, cptr text, byte attr)
{
    char line[1024];

    if (!text || !text[0])
        return;
    if (label && label[0])
        strnfmt(line, sizeof(line), "%s: %s", label, text);
    else
        SDL_strlcpy(line, text, sizeof(line));
    sdl_character_sheet_screen_add_select_detail(line, attr, "");
}

static void oath_add_semantic_details(int oath_id)
{
    if (oath_id < 0 || !z_info || oath_id >= z_info->oath_max)
        return;

    if (oath_banned(oath_id) && oath_id > 0)
    {
        char* banned_text = oath_banned_text(oath_id);

        oath_add_semantic_detail_line(NULL, "OATH BROKEN", TERM_L_RED);
        oath_add_semantic_detail_line(NULL,
            (banned_text && banned_text[0])
                ? banned_text
                : "Thy oath lies shattered, and thy name is marked in shame for this age.",
            TERM_RED);
        return;
    }

    if (oath_id == 0)
    {
        oath_add_semantic_detail_line(NULL, "Walk free of binding words.",
            TERM_SLATE);
        oath_add_semantic_detail_line(NULL,
            "Take no oath and remain unbound by sacred vows.", TERM_SLATE);
        return;
    }

    oath_add_semantic_detail_line("Description", oath_description(oath_id),
        TERM_SLATE);
    oath_add_semantic_detail_line("Pledge", oath_pledge(oath_id),
        TERM_L_BLUE);
    oath_add_semantic_detail_line("Reward", oath_reward_text(oath_id),
        TERM_L_GREEN);
    oath_add_semantic_detail_line("Forbidden", oath_forbidden(oath_id),
        TERM_L_RED);
}

static void oath_render_wrapped_block(cptr text, int col, int max_width,
    int* draw_row, int row_limit, byte color)
{
    int virtual_row = 0;

    oath_render_virtual_wrapped_text(text, col, max_width, draw_row, row_limit,
        &virtual_row, 0, color, true);
}

static void oath_draw_compact_list_summary(int oath_id, int row, int prompt_row,
    int max_width)
{
    char line_buf[512];
    byte name_attr = TERM_L_BLUE;

    if (row >= prompt_row || max_width <= 0)
        return;

    if (oath_id == 0)
        name_attr = TERM_WHITE;
    else if (oath_banned(oath_id))
        name_attr = TERM_L_RED;

    oath_putstr_fit(2, row++, max_width, name_attr, oath_name_str(oath_id));

    if (row >= prompt_row)
        return;

    if (oath_id == 0)
    {
        oath_putstr_fit(2, row, max_width, TERM_SLATE,
            "No oath. No restrictions.");
        return;
    }

    if (oath_banned(oath_id))
    {
        oath_putstr_fit(2, row, max_width, TERM_RED,
            "Broken oath: unavailable for this metarun.");
        return;
    }

    if (oath_reward_text(oath_id) && oath_reward_text(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Reward: %s",
            oath_reward_text(oath_id));
        oath_render_wrapped_block(line_buf, 2, max_width, &row, prompt_row,
            TERM_L_GREEN);
    }

    if (row >= prompt_row)
        return;

    if (oath_forbidden(oath_id) && oath_forbidden(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Forbidden: %s",
            oath_forbidden(oath_id));
        oath_render_wrapped_block(line_buf, 2, max_width, &row, prompt_row,
            TERM_L_RED);
    }
}

/*
 * Oath selection screen.
 *
 * Wide screens keep the split list/details layout. Compact screens use a
 * dedicated list page plus a full-width details page with vertical scrolling.
 */
NavResult select_oath(void)
{
    enum {
        OATH_CLICK_BACK = -1,
        OATH_CLICK_SELECT = -2,
        OATH_CLICK_DETAILS = -3,
        OATH_CLICK_LIST = -4
    };
    int available_mask = get_available_oaths_mask();

    /* If no oaths are available, skip oath selection */
    if (available_mask == 0)
    {
        p_ptr->oath_type = 0; /* No oath */
        log_debug("No oaths available, skipping oath selection");
        return NAV_OK;
    }

    int highlight = 1; /* Start highlighting first available oath */
    int choice = 0;
    int page = 0;
    int detail_scroll = 0;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    /* Find first available oath to highlight */
    for (int i = 1; i <= oath_selectable_max_id(); i++)
    {
        if (available_mask & (1 << (i - 1)))
        {
            highlight = i;
            break;
        }
    }

    while (true)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;
        int visible_oaths[16];
        int visible_count;
        int detail_max_scroll = 0;
        bool compact;
        bool semantic_menu = false;
        char key;

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;

        compact = oath_menu_use_compact_layout();
        if (!compact)
            page = 0;

        visible_count = oath_collect_visible(available_mask, visible_oaths,
            (int)N_ELEMENTS(visible_oaths));
        if (visible_count > (int)N_ELEMENTS(visible_oaths))
            visible_count = (int)N_ELEMENTS(visible_oaths);

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        semantic_menu = sdl_character_sheet_screen_begin_select(highlight,
            "Choose your Oath");
        if (semantic_menu)
        {
            compact = false;
            page = 0;
            detail_scroll = 0;
            sdl_character_sheet_screen_set_select_menu_style(true);
            sdl_character_sheet_screen_set_select_dynamic_description(true);

            for (int i = 0; i < visible_count; i++)
            {
                int oath_id = visible_oaths[i];
                byte attr;
                char label[128];
                bool show_letter = menu_letters
                    && !sdl_touch_only_device_active();

                if (oath_banned(oath_id) && oath_id > 0)
                    attr = TERM_L_RED;
                else if (oath_id == 0)
                    attr = TERM_WHITE;
                else
                    attr = TERM_L_BLUE;

                if (show_letter)
                    strnfmt(label, sizeof(label), "%c) %s", 'a' + i,
                        oath_name_str(oath_id));
                else
                    SDL_strlcpy(label, oath_name_str(oath_id), sizeof(label));
                sdl_character_sheet_screen_add_select_row(oath_id, label,
                    attr, "");
            }

            oath_add_semantic_details(highlight);
            sdl_character_sheet_screen_commit_select(highlight);
        }
        else if (compact)
        {
            oath_center_putstr(0, TERM_L_BLUE,
                (page == 0) ? "Choose your Oath" : "Oath Details");
            oath_draw_page_indicator(page, 2, wid, 0);

            if (page == 0)
            {
                int list_row = 4;

                Term_putstr(2, 2, -1, TERM_WHITE, "Available Oaths");

                for (int i = 0; i < visible_count; i++)
                {
                    int oath_id = visible_oaths[i];
                    byte attr;
                    char buf[96];

                    if (oath_banned(oath_id) && oath_id > 0)
                        attr = (highlight == oath_id) ? TERM_L_RED : TERM_RED;
                    else
                        attr = (highlight == oath_id) ? TERM_L_BLUE : TERM_WHITE;

                    if (!menu_letters)
                        strnfmt(buf, sizeof(buf), "   %s", oath_name_str(oath_id));
                    else
                        strnfmt(buf, sizeof(buf), "%c) %s", 'a' + i, oath_name_str(oath_id));
                    Term_putstr(2, list_row + i, -1, attr, buf);
                    ui_menu_click_add(oath_id, 2, list_row + i, wid - 4);
                }

                oath_draw_compact_list_summary(highlight, list_row + visible_count + 1,
                    prompt_row, wid - 4);

                if (steamdeck)
                {
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_buf[96];

                    birth_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    birth_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    {
                        char prompt_full[96];
                        char prompt_short[80];
                        const char* variants[2];

                        strnfmt(prompt_full, sizeof(prompt_full),
                            "D-pad Nav/Page  %s Select  %s Back",
                            confirm_label, back_label);
                        strnfmt(prompt_short, sizeof(prompt_short),
                            "%s Select  %s Back", confirm_label, back_label);
                        variants[0] = prompt_full;
                        variants[1] = prompt_short;
                        terminal_prompt_pick_variant(prompt_buf,
                            sizeof(prompt_buf), wid - 4, false, variants,
                            N_ELEMENTS(variants));
                    }
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE, prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
                else if (sdl_touch_only_device_active())
                {
                    char prompt_buf[96];
                    /* Oaths are tappable rows; keep Details/Select/Back words
                     * present so their tap tokens still register on touch. */
                    const char* variants[] = {
                        "Tap an oath to Select  Details  Back",
                        "Tap to Select  Details  Back",
                        "Tap to Select  Back"
                    };

                    terminal_prompt_pick_variant(prompt_buf,
                        sizeof(prompt_buf), wid - 4, false, variants,
                        N_ELEMENTS(variants));
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                        prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_DETAILS, 2,
                        prompt_row, prompt_buf, "Details");
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
                else
                {
                    char prompt_buf[96];
                    const char* variants[] = {
                        "Dir Nav  Right Details  Enter Select  Esc Back",
                        "Dir Nav  Enter Select  Esc Back",
                        "Enter Select  Esc Back"
                    };

                    terminal_prompt_pick_variant(prompt_buf,
                        sizeof(prompt_buf), wid - 4, false, variants,
                        N_ELEMENTS(variants));
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                        prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_DETAILS, 2,
                        prompt_row, prompt_buf, "Details");
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
            }
            else
            {
                int content_row = 3;
                int visible_rows = prompt_row - content_row;
                int total_lines;

                Term_putstr(2, 1, -1, oath_banned(highlight) ? TERM_L_RED : TERM_L_BLUE,
                    oath_name_str(highlight));

                total_lines = oath_render_detail_content(highlight, 2, content_row,
                    wid - 4, prompt_row, 0, false);

                if (visible_rows < 0)
                    visible_rows = 0;

                detail_max_scroll = (total_lines > visible_rows)
                    ? (total_lines - visible_rows)
                    : 0;

                if (detail_scroll > detail_max_scroll)
                    detail_scroll = detail_max_scroll;

                (void)oath_render_detail_content(highlight, 2, content_row, wid - 4,
                    prompt_row, detail_scroll, true);

                if (detail_max_scroll > 0)
                {
                    char scroll_buf[32];

                    strnfmt(scroll_buf, sizeof(scroll_buf), "Scroll %d/%d",
                        detail_scroll + 1, detail_max_scroll + 1);
                    oath_putstr_fit(2, 2, wid - 4, TERM_SLATE, scroll_buf);
                }

                if (steamdeck)
                {
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_buf[96];

                    birth_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    birth_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    {
                        char prompt_full[96];
                        char prompt_short[80];
                        const char* variants[2];

                        strnfmt(prompt_full, sizeof(prompt_full),
                            "D-pad Scroll/Page  %s Select  %s Back",
                            confirm_label, back_label);
                        strnfmt(prompt_short, sizeof(prompt_short),
                            "%s Select  %s Back", confirm_label, back_label);
                        variants[0] = prompt_full;
                        variants[1] = prompt_short;
                        terminal_prompt_pick_variant(prompt_buf,
                            sizeof(prompt_buf), wid - 4, false, variants,
                            N_ELEMENTS(variants));
                    }
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE, prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
                else if (sdl_touch_only_device_active())
                {
                    char prompt_buf[96];
                    /* Keep List/Select/Back words present for their tap
                     * tokens; the detail text scrolls by swipe. */
                    const char* variants[] = {
                        "Swipe to scroll  Tap List  Select  Back",
                        "Tap List  Select  Back",
                        "Tap Select  Back"
                    };

                    terminal_prompt_pick_variant(prompt_buf,
                        sizeof(prompt_buf), wid - 4, false, variants,
                        N_ELEMENTS(variants));
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                        prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_LIST, 2,
                        prompt_row, prompt_buf, "List");
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
                else
                {
                    char prompt_buf[96];
                    const char* variants[] = {
                        "Dir Scroll  Left List  Enter Select  Esc Back",
                        "Dir Scroll  Enter Select  Esc Back",
                        "Enter Select  Esc Back"
                    };

                    terminal_prompt_pick_variant(prompt_buf,
                        sizeof(prompt_buf), wid - 4, false, variants,
                        N_ELEMENTS(variants));
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                        prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_LIST, 2,
                        prompt_row, prompt_buf, "List");
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
            }
        }
        else
        {
            int footer_row = prompt_row - 3;
            int details_col = COL_DESCRIPTION - 2;
            int details_width;
            int list_width;

            if (details_col < 20)
                details_col = 20;
            if (details_col > wid - 12)
                details_col = wid - 12;

            details_width = wid - details_col - 2;
            list_width = details_col - 4;

            oath_center_putstr(0, TERM_L_BLUE, "Choose your Oath");
            Term_putstr(2, 2, -1, TERM_WHITE, "Available Oaths");
            oath_putstr_fit(details_col, 2, details_width, TERM_WHITE,
                "Oath Details");

            for (int i = 0; i < visible_count; i++)
            {
                int oath_id = visible_oaths[i];
                byte attr;
                char buf[96];

                if (oath_banned(oath_id) && oath_id > 0)
                    attr = (highlight == oath_id) ? TERM_L_RED : TERM_RED;
                else
                    attr = (highlight == oath_id) ? TERM_L_BLUE : TERM_WHITE;

                if (!menu_letters)
                    strnfmt(buf, sizeof(buf), "   %s", oath_name_str(oath_id));
                else
                    strnfmt(buf, sizeof(buf), "%c) %s", 'a' + i, oath_name_str(oath_id));
                oath_putstr_fit(2, 4 + i, list_width, attr, buf);
                ui_menu_click_add(oath_id, 2, 4 + i, list_width);
            }

            (void)oath_render_detail_content(highlight, details_col, 4,
                details_width, prompt_row, 0, true);

            if (footer_row >= 0)
            {
                oath_putstr_fit(2, footer_row, wid - 4, TERM_SLATE,
                    "Oaths grant power, but they bind your actions.");
            }
            if (footer_row + 1 < prompt_row)
            {
                oath_putstr_fit(2, footer_row + 1, wid - 4, TERM_SLATE,
                    "Breaking an oath brings curse and shame.");
            }

            if (steamdeck)
            {
                char confirm_label[16];
                char back_label[16];
                char prompt_buf[128];

                birth_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                {
                    char prompt_full[128];
                    char prompt_short[80];
                    const char* variants[2];

                    strnfmt(prompt_full, sizeof(prompt_full),
                        "D-pad Navigate  %s Select  %s Back",
                        confirm_label, back_label);
                    strnfmt(prompt_short, sizeof(prompt_short),
                        "%s Select  %s Back", confirm_label, back_label);
                    variants[0] = prompt_full;
                    variants[1] = prompt_short;
                    terminal_prompt_pick_variant(prompt_buf,
                        sizeof(prompt_buf), wid - 4, false, variants,
                        N_ELEMENTS(variants));
                }
                oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE, prompt_buf);
                ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                    prompt_row, prompt_buf, "Select");
                ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                    prompt_row, prompt_buf, "Back");
            }
            else if (sdl_touch_only_device_active())
            {
                char prompt_buf[96];
                const char* variants[] = {
                    "Tap an oath to Select  Back",
                    "Tap to Select  Back",
                    "Tap to Select"
                };

                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    wid - 4, false, variants, N_ELEMENTS(variants));
                oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                    prompt_buf);
                ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                    prompt_row, prompt_buf, "Select");
                ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                    prompt_row, prompt_buf, "Back");
            }
            else
            {
                char prompt_buf[96];
                const char* variants[] = {
                    "Dir Navigate  Enter Select  Esc Back",
                    "Enter Select  Esc Back"
                };

                terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                    wid - 4, false, variants, N_ELEMENTS(variants));
                oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                    prompt_buf);
                ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                    prompt_row, prompt_buf, "Select");
                ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                    prompt_row, prompt_buf, "Back");
            }
        }

        Term_fresh();
        key = inkey();
        bool click_generated_command = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();

                if (clicked_choice >= 0 && clicked_choice < z_info->oath_max)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != highlight)
                    {
                        highlight = clicked_choice;
                        detail_scroll = 0;
                        continue;
                    }

                    if (click_action == UI_MENU_CLICK_SECONDARY
                        && compact && page == 0)
                    {
                        page = 1;
                        detail_scroll = 0;
                        continue;
                    }

                    key = '\r';
                    click_generated_command = true;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else
                {
                    switch (clicked_choice)
                    {
                    case OATH_CLICK_BACK: key = ESCAPE; click_generated_command = true; break;
                    case OATH_CLICK_SELECT: key = '\r'; click_generated_command = true; break;
                    case OATH_CLICK_DETAILS: key = '6'; click_generated_command = true; break;
                    case OATH_CLICK_LIST: key = '4'; click_generated_command = true; break;
                    default: break;
                    }
                }
            }
        }

        if (!click_generated_command)
            key = (char)steamdeck_menu_key(key, '4', '6');

        if (steamdeck && key == steamdeck_back_key())
        {
            sdl_character_sheet_screen_hide();
            ui_menu_click_clear();
            return NAV_BACK; /* Go back to character creation */
        }
        if (key == ESCAPE || key == 'q')
        {
            sdl_character_sheet_screen_hide();
            ui_menu_click_clear();
            return NAV_BACK; /* Go back to character creation */
        }

        if (compact && key == '4')
        {
            page = 0;
            continue;
        }

        if (compact && key == '6')
        {
            page = 1;
            continue;
        }

        if (birth_confirm_input(key, steamdeck)
            || (!compact && key == '6'))
        {
            /* Select current highlighted option */
            if (oath_option_selectable(highlight, available_mask))
            {
                choice = highlight;
                break;
            }
        }

        if (menu_letters && key >= 'a' && key < 'a' + visible_count)
        {
            int display_pos = key - 'a';

            if (display_pos >= 0 && display_pos < visible_count
                && oath_option_selectable(visible_oaths[display_pos], available_mask))
            {
                choice = visible_oaths[display_pos];
                break;
            }

            continue;
        }

        if (key == '8')
        {
            if (compact && page == 1)
            {
                if (detail_scroll > 0)
                    detail_scroll--;
                continue;
            }

            oath_move_highlight(&highlight, -1, available_mask);
            detail_scroll = 0;
        }

        if (key == '2')
        {
            if (compact && page == 1)
            {
                if (detail_scroll < detail_max_scroll)
                    detail_scroll++;
                continue;
            }

            oath_move_highlight(&highlight, 1, available_mask);
            detail_scroll = 0;
        }
    }

    /* Set the chosen oath */
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    p_ptr->oath_type = choice;
    
    /* Grant corresponding oath special ability using oath.txt data */
    if (choice > 0 && choice < z_info->oath_max) 
    {
        oath_type *oath_ptr = &oath_info[choice];
        
        /* Apply ability reward from A: field in oath.txt */
        if (oath_ptr->reward_type > 0 && oath_ptr->reward_value > 0)
        {
            int skill_category = oath_ptr->reward_type;
            int ability_id = oath_ptr->reward_value;
            
            if (skill_category >= 0 && skill_category < S_MAX
                && ability_id >= 0 && ability_id < ABILITIES_MAX)
            {
                /* Grant the ability specified in oath.txt */
                p_ptr->have_ability[skill_category][ability_id] = true;
                p_ptr->innate_ability[skill_category][ability_id] = true;
                p_ptr->active_ability[skill_category][ability_id] = true;

                log_debug("Granted oath %d abilities from data: skill=%d, ability=%d",
                          choice, skill_category, ability_id);
            }
            else
            {
                log_warn("Oath %d ability out of bounds: skill=%d (max %d), ability=%d (max %d)",
                         choice, skill_category, S_MAX - 1, ability_id, ABILITIES_MAX - 1);
            }
        }
        else
        {
            log_debug("No ability reward found for oath %d", choice);
        }
    }
    
    if (choice == 0) {
        log_debug("No oath selected");
    } else {
        log_debug("Oath selected: %s (%d)", oath_name_str(choice), choice);
    }
    
    return NAV_OK;
}

