/* File: dungeon/dungeon-story.c */

#include "angband.h"
#include "dungeon-internal.h"

static bool story_intro_skip_requested(void)
{
    char check_key;

    if (Term_inkey(&check_key, false, false) == 0)
    {
        Term_inkey(&check_key, false, true);
        if (check_key == ESCAPE || check_key == '\n' || check_key == '\r'
            || check_key == ' ' || check_key == (char)INPUT_BIND_CONFIRM)
            return true;
        if (steamdeck_controls_active()
            && (check_key == steamdeck_confirm_key()
                || check_key == steamdeck_back_key()))
            return true;
    }

    return false;
}

static void story_intro_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    morgoth_prompt_controller_label(binding, fallback, buf, buflen);
}

static bool story_intro_back_key(int ch)
{
    return steamdeck_controls_active() && ch == steamdeck_back_key()
        && ch != steamdeck_confirm_key();
}

static void story_intro_touch_confirm_begin(int h)
{
    if (h < 1)
        return;

    ui_menu_click_begin();
    ui_menu_click_set_outside_cancel_enabled(true);
    for (int row = 0; row < h; row++)
        ui_menu_click_add_full_row('\r', row);
}

static void story_intro_touch_confirm_end(void)
{
    ui_menu_click_clear();
}

enum
{
    STORY_INTRO_CLICK_FINISH = '\r'
};

static int story_intro_prompt_hit_width(cptr text)
{
    int width = text ? (int)strlen(text) : 0;

    if (width < 1)
        return 1;

    if (sdl_is_story_font_enabled() && !sdl_is_story_font_grid())
    {
        int cell_width = sdl_get_cell_width();
        int pixel_width = sdl_story_font_text_width(text, width);

        if (cell_width > 0 && pixel_width > 0)
        {
            int story_width = (pixel_width + cell_width - 1) / cell_width;

            if (story_width > width)
                width = story_width;
        }
    }

    return width;
}

static void story_intro_final_prompt_put(int choice, int col, int row,
    cptr text, bool highlighted)
{
    Term_putstr(col, row, -1, highlighted ? TERM_L_BLUE : TERM_L_WHITE, text);
    ui_menu_click_add(choice, col, row, story_intro_prompt_hit_width(text));
}

static void story_intro_final_prompt_draw(int h)
{
    int finish_row = (h >= 1) ? h - 1 : 0;
    int hover_choice = 0;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
    (void)ui_menu_click_get_hover_choice(&hover_choice);

    Term_erase(0, finish_row, 255);

    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char prompt_buf[96];

        story_intro_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] finish", confirm_label);
        story_intro_final_prompt_put(STORY_INTRO_CLICK_FINISH, 15, finish_row,
            prompt_buf, hover_choice == STORY_INTRO_CLICK_FINISH);
    }
    else if (sdl_touch_only_device_active())
    {
        story_intro_final_prompt_put(STORY_INTRO_CLICK_FINISH, 15, finish_row,
            "Tap here to finish",
            hover_choice == STORY_INTRO_CLICK_FINISH);
    }
    else
    {
        story_intro_final_prompt_put(STORY_INTRO_CLICK_FINISH, 15, finish_row,
            "(press any key to finish)",
            hover_choice == STORY_INTRO_CLICK_FINISH);
    }

    Term_fresh();
}

static char story_intro_final_prompt_inkey(int h)
{
    char key;

    while (true)
    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;
        bool saved_hide_cursor = hide_cursor;

        story_intro_final_prompt_draw(h);

        hide_cursor = true;
        key = inkey();
        hide_cursor = saved_hide_cursor;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            ui_menu_click_clear();
            if (click_action == UI_MENU_CLICK_HOVER)
                continue;

            key = (char)clicked_choice;
        }
        else if (key == UI_MENU_CLICK_WAKE_KEY)
        {
            ui_menu_click_clear();
            continue;
        }
        else
        {
            ui_menu_click_clear();
        }

        return key;
    }
}

static int story_intro_count_paragraph_rows(cptr text, int wrap_width)
{
    int rows = 0;
    int col = 0;
    bool line_has_content = false;
    bool pending_space = false;
    cptr s = text ? text : "";

    if (wrap_width < 1)
        wrap_width = 1;

    while (*s)
    {
        int word_len = 0;

        if (*s == '\n')
        {
            col = 0;
            line_has_content = false;
            pending_space = false;
            s++;
            continue;
        }

        if (*s == ' ' || *s == '\t')
        {
            pending_space = line_has_content;
            s++;
            continue;
        }

        while (s[word_len] && s[word_len] != ' ' && s[word_len] != '\t' && s[word_len] != '\n')
            word_len++;

        if (pending_space && line_has_content)
        {
            if (col + 1 + word_len > wrap_width)
            {
                col = 0;
                line_has_content = false;
            }
            else
            {
                col++;
            }
            pending_space = false;
        }

        for (int i = 0; i < word_len; ++i)
        {
            if (col >= wrap_width)
            {
                col = 0;
                line_has_content = false;
            }

            if (!line_has_content)
            {
                rows++;
                line_has_content = true;
            }

            col++;
        }

        s += word_len;
    }

    return (rows > 0) ? rows : 1;
}

static void story_intro_putch(int x, int y, char ch, bool *skipped)
{
    if (!*skipped && story_intro_skip_requested())
        *skipped = true;

    Term_putch(x, y, TERM_WHITE, ch);

    if (!*skipped)
    {
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 30);
    }
}

static bool story_intro_render_paragraph(cptr text, int indent, int wrap_width, int *row)
{
    int col = 0;
    bool line_has_content = false;
    bool pending_space = false;
    bool skipped = false;
    cptr s = text ? text : "";

    if (!row)
        return false;

    if (wrap_width < 1)
        wrap_width = 1;

    while (*s)
    {
        int word_len = 0;

        if (*s == '\n')
        {
            (*row)++;
            col = 0;
            line_has_content = false;
            pending_space = false;
            s++;
            continue;
        }

        if (*s == ' ' || *s == '\t')
        {
            pending_space = line_has_content;
            s++;
            continue;
        }

        while (s[word_len] && s[word_len] != ' ' && s[word_len] != '\t' && s[word_len] != '\n')
            word_len++;

        if (pending_space && line_has_content)
        {
            if (col + 1 + word_len > wrap_width)
            {
                (*row)++;
                col = 0;
                line_has_content = false;
            }
            else
            {
                story_intro_putch(indent + col, *row, ' ', &skipped);
                col++;
            }
            pending_space = false;
        }

        for (int i = 0; i < word_len; ++i)
        {
            if (col >= wrap_width)
            {
                (*row)++;
                col = 0;
                line_has_content = false;
            }

            story_intro_putch(indent + col, *row, s[i], &skipped);
            col++;
            line_has_content = true;
        }

        s += word_len;
    }

    if (skipped)
        Term_fresh();

    return skipped;
}

/**
 * Introductory narrative display, one paragraph per prompt.
 * Implemented as a static function to restrict linkage.
 */
void print_story_intro(void)
{
    bool story_intro_story_font = true;
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_story_font_enable();
    sdl_story_font_set_slot(STORY_FONT_SLOT_SECONDARY);
    sdl_music_play_main_full();
    int wid, h;
    const int indent = 2;

    /* Narrative paragraphs as valid C string literals with embedded \n */
    cptr intro_texts[] = {
        "You awaken in darkness.\n"
        "No name. No memory.\n"
        "Only a quiet ache of courage deep inside you,\n"
        "like embers buried beneath ash.\n",

        "Far below, Morgoth waits upon his throne-\n"
        "iron-dark and crowned in flame.\n"
        "Upon his brow shine three Silmarils, stolen stars.\n"
        "He senses your stirring. He knows you will come.\n",

        "Far above, beyond the shadows of Angband,\n"
        "the Valar watch silently.\n"
        "They offer no guidance, yet their presence\n"
        "fills you with strength-and dread.\n",

        "You will return many times, each death and rebirth\n"
        "etched into the endless stone halls of Mandos.\n"
        "Each fall will draw your spirit deeper into shadow,\n"
        "closer to a doom from which you cannot escape.\n",

        "Yet each victory-each Silmaril wrested from Morgoth's crown-\n"
        "will brighten the Valar's hope,\n"
        "even as your soul grows thinner,\n"
        "your strength fading with every triumph.\n",

        "You envy the Edain, whose Gift from Ilúvatar\n"
        "frees them from the bonds of Mandos and the world.\n"
        "Yet you do not know if such release can ever be yours.\n"
        "You do not know who-or even what-you truly are.\n",

        "For each time you awaken,\n"
        "you will carry the names of heroes beloved and feared-\n"
        "bright spirits, fiery hearts, proud kings and exiles,\n"
        "wanderers beneath sun and stars,\n"
        "whose courage you borrow, but whose fates are not your own.\n",

        "This is the trial set by the Valar:\n"
        "to walk the narrow way between shadow and light,\n"
        "to bear the borrowed glory of the great,\n"
        "and to win back at last\n"
        "the name that was taken from you.\n",

        "Now the path before you opens,\nand your trial begins.\n"
    };

    int total = sizeof(intro_texts) / sizeof(intro_texts[0]);
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent;

    /* Start on a blank screen */
    Term_clear();
    int row = 1;

    for (int idx = 0; idx < total; idx++) {
        const char *s = intro_texts[idx];
        int lines_needed = story_intro_count_paragraph_rows(s, wrap_width) + 1;
        bool skipped;

        /* Check if we have enough space for the whole paragraph */
        if (row + lines_needed >= h - 1) {
            if (steamdeck_controls_active())
            {
                char confirm_label[16];
                char back_label[16];
                char prompt_buf[80];

                story_intro_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                story_intro_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "[%s] continue  [%s] skip", confirm_label, back_label);
                Term_putstr(15, h - 1, -1, TERM_L_WHITE, prompt_buf);
            }
            else if (sdl_touch_only_device_active())
            {
                Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(Tap to continue)");
            }
            else
            {
                Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key)");
            }
            hide_cursor = true;
            {
                story_intro_touch_confirm_begin(h);
                char k = inkey();
                story_intro_touch_confirm_end();
                if (k == 'S' || story_intro_back_key(k)) { /* Capital S skips the intro entirely */
                    Term_clear();
                    goto cleanup_intro;
                }
            }
            Term_clear();
            row = 1;
        }

        story_intro_touch_confirm_begin(h);
        skipped = story_intro_render_paragraph(s, indent, wrap_width, &row);
        story_intro_touch_confirm_end();

        /* Leave one blank line after each paragraph */
        row++;

        /* 1 second pause after paragraph (skip if we already skipped typewriter) */
        if (!skipped) {
            Term_xtra(TERM_XTRA_DELAY, 1000);
        }
    }

    /* Handle final input */
    char key = story_intro_final_prompt_inkey(h);
    if (key == 'S' || story_intro_back_key(key)) {
        Term_clear();
        goto cleanup_intro;
    }
    Term_clear();

    /* Flush any queued keypresses that accumulated during the intro */
    Term_flush();

cleanup_intro:
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    if (story_intro_story_font)
        sdl_story_font_reset();
    
    return;
}
