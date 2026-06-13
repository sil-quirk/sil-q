#include "angband.h"
#include "metarun-internal.h"

static int metarun_effect_wrap_width(int term_width)
{
    int wrap_width = term_width - 2;
    if (wrap_width < 5) wrap_width = 5;
    return wrap_width;
}

static int metarun_count_effect_lines(cptr text, int wrap_width, int indent)
{
    if (!text || !*text) return 0;

    if (sdl_is_story_font_enabled()) {
        return count_wrapped_lines_story(text, wrap_width, indent);
    }

    return count_wrapped_lines(text, wrap_width, indent);
}

static int metarun_active_effect_block_lines(int id, int term_width)
{
    const int text_col = 4;
    const int wrap_width = metarun_effect_wrap_width(term_width);
    const curse_type *cu = &cu_info[id];
    int stacks = CURSE_GET(id);
    bool is_blessing = (stacks < 0);
    bool seen = CURSE_SEEN(id);
    int lines = 1; /* Name */

    cptr desc = is_blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
        : (cu->text ? cu_text + cu->text : NULL);
    if (desc && *desc) {
        lines += metarun_count_effect_lines(desc, wrap_width, text_col);
    }

    if (seen) {
        cptr power = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);

        if (power && *power) {
            char effect_line[1024];
            strnfmt(effect_line, sizeof(effect_line), "Effect: %s", power);
            lines += metarun_count_effect_lines(effect_line, wrap_width, text_col);
        }
    } else {
        lines += 1;
    }

    lines += 1; /* Blank line between effects */
    return lines;
}

static int metarun_render_active_effect_block(int id, int row, int term_width)
{
    const int name_col = 2;
    const int text_col = 4;
    const int wrap_width = metarun_effect_wrap_width(term_width);
    int stacks = CURSE_GET(id);
    bool is_blessing = (stacks < 0);
    int magnitude = is_blessing ? -stacks : stacks;
    bool seen = CURSE_SEEN(id);

    const curse_type *cu = &cu_info[id];
    cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
    byte name_attr = is_blessing ? TERM_L_GREEN : TERM_L_RED;

    char buf[120];
    strnfmt(buf, sizeof(buf), "%s x%d", name, magnitude);
    Term_putstr(name_col, row++, -1, name_attr, buf);

    text_out_hook = text_out_to_screen;
    text_out_indent = text_col;
    text_out_wrap = wrap_width;

    cptr desc = is_blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
        : (cu->text ? cu_text + cu->text : NULL);
    if (desc && *desc) {
        Term_gotoxy(text_col, row);
        text_out_c(TERM_SLATE, desc);
        row += metarun_count_effect_lines(desc, wrap_width, text_col);
    }

    if (seen) {
        cptr power = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);

        if (power && *power) {
            char effect_line[1024];
            strnfmt(effect_line, sizeof(effect_line), "Effect: %s", power);
            Term_gotoxy(text_col, row);
            text_out_c(name_attr, effect_line);
            row += metarun_count_effect_lines(effect_line, wrap_width, text_col);
        }
    } else {
        Term_putstr(text_col, row++, -1, TERM_L_DARK, "(Effect not yet identified)");
    }

    row++;
    return row;
}

/* Show all active curses in a dedicated screen with pagination */
void show_all_active_curses(void)
{
    int term_height, term_width;
    screen_save();
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
    }

    /* Get actual terminal dimensions */
    Term_get_size(&term_width, &term_height);

    /* Count active effects and build list */
    int active_count = 0;
    int active_ids[64];
    for (int id = 0; id < z_info->cu_max && active_count < 64; id++) {
        if (CURSE_GET(id) != 0) {
            active_ids[active_count++] = id;
        }
    }

    if (active_count == 0) {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== All Active Effects ===");
        Term_putstr(2, 3, -1, TERM_L_DARK, "No active curses or blessings");
        if (steamdeck) {
            char hint_buf[64];
            strnfmt(hint_buf, sizeof(hint_buf), "Press [%s] to return.", accept_label);
            Term_putstr(2, 5, -1, TERM_L_DARK, hint_buf);
        } else if (sdl_touch_only_device_active()) {
            Term_putstr(2, 5, -1, TERM_L_DARK, "Tap to return");
        } else {
            Term_putstr(2, 5, -1, TERM_L_DARK, "Press any key to return.");
        }
        ui_menu_click_begin();
        ui_menu_click_add_full_row('\r', 5);
        metarun_wait_hidden();
        ui_menu_click_clear();
        screen_load();
        return;
    }

    int available_lines = term_height - 4;
    if (available_lines < 1) available_lines = 1;

    int page_starts[64];
    int total_pages = 0;
    int lines_used = 0;
    page_starts[0] = 0;

    for (int i = 0; i < active_count; i++) {
        int block_lines = metarun_active_effect_block_lines(active_ids[i], term_width);

        if (lines_used > 0 && lines_used + block_lines > available_lines) {
            total_pages++;
            page_starts[total_pages] = i;
            lines_used = 0;
        }

        lines_used += block_lines;
    }

    total_pages++;
    int current_page = 0;

    void (*old_text_out_hook)(byte, cptr) = text_out_hook;
    int old_text_out_indent = text_out_indent;
    int old_text_out_wrap = text_out_wrap;

    while (true) {
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        /* Title with page info */
        char title_buf[80];
        if (total_pages > 1) {
            snprintf(title_buf, sizeof title_buf, "=== Active Effects (Page %d/%d) ===",
                     current_page + 1, total_pages);
        } else {
            SDL_strlcpy(title_buf, "=== All Active Effects ===", sizeof title_buf);
        }
        Term_putstr(2, 1, -1, TERM_YELLOW, title_buf);

        int start_idx = page_starts[current_page];
        int end_idx = (current_page + 1 < total_pages) ? page_starts[current_page + 1] : active_count;

        int row = 3;
        for (int i = start_idx; i < end_idx; i++) {
            row = metarun_render_active_effect_block(active_ids[i], row, term_width);
        }

        /* Footer with navigation instructions */
        char footer_buf[100];
        char back_label[16] = "";
        if (steamdeck) {
            /* Steam Deck UI: A=ok, B=back */
            metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
            if (total_pages > 1) {
                char prompt_full[100];
                char prompt_short[80];
                const char* variants[2];

                snprintf(prompt_full, sizeof prompt_full,
                         "D-pad navigate  [%s] ok  [%s] back",
                         accept_label, back_label);
                snprintf(prompt_short, sizeof prompt_short,
                         "[%s] ok  [%s] back", accept_label, back_label);
                variants[0] = prompt_full;
                variants[1] = prompt_short;
                terminal_prompt_pick_variant(footer_buf, sizeof footer_buf,
                    term_width, false, variants, N_ELEMENTS(variants));
            } else {
                snprintf(footer_buf, sizeof footer_buf, "[%s] ok  [%s] back",
                         accept_label, back_label);
            }
        } else if (sdl_touch_only_device_active()) {
            SDL_strlcpy(footer_buf, "Tap to return", sizeof footer_buf);
        } else {
            if (total_pages > 1) {
                const char* variants[] = {
                    "Dir left/right pages  Any key returns",
                    "Dir pages  Any key returns",
                    "Any key returns"
                };
                terminal_prompt_pick_variant(footer_buf, sizeof footer_buf,
                    term_width, false, variants, N_ELEMENTS(variants));
            } else {
                SDL_strlcpy(footer_buf, "Press any key to return.", sizeof footer_buf);
            }
        }

        Term_putstr(0, term_height - 1, -1, TERM_L_DARK, footer_buf);
        ui_menu_click_add_text_token(-1, 0, term_height - 1, footer_buf,
            "back");
        ui_menu_click_add_text_token(-1, 0, term_height - 1, footer_buf,
            "return");
        ui_menu_click_add_text_token(-2, 0, term_height - 1, footer_buf,
            "right");
        ui_menu_click_add_text_token(-3, 0, term_height - 1, footer_buf,
            "left");
        ui_menu_click_add_text_token(-1, 0, term_height - 1, footer_buf,
            "ok");
        Term_fresh();

        /* Get input */
        char key = metarun_inkey_hidden();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '6';
                else if (clicked_choice == -3)
                    key = '4';
            }
        }

        if (key == UI_MENU_CLICK_WAKE_KEY) {
            ui_menu_click_clear();
            continue;
        }

        /* Arrow navigation: 6 = right, 4 = left (keypad directions) */
        if (total_pages > 1 && key == '6') {
            /* Next page */
            current_page = (current_page + 1) % total_pages;
        } else if (total_pages > 1 && key == '4') {
            /* Previous page */
            current_page = (current_page + total_pages - 1) % total_pages;
        } else if (steamdeck && key == steamdeck_back_key()) {
            /* B button = back in Steam Deck mode */
            break;
        } else if (steamdeck && (key == steamdeck_confirm_key() || key == '\r' || key == '\n')) {
            /* A button = confirm/close in Steam Deck mode */
            break;
        } else if (!steamdeck) {
            /* Exit on any key in non-Steam Deck mode */
            break;
        }
    }

    text_out_hook = old_text_out_hook;
    text_out_indent = old_text_out_indent;
    text_out_wrap = old_text_out_wrap;

    ui_menu_click_clear();
    screen_load();
}

void show_known_curses_menu(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_CURSES);
}
