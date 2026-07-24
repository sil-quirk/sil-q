#include "angband.h"
#include "externs.h"
#include "sdl-config.h"
#include "ui/menu-click.h"
#include "ui/question.h"

/*
 * Rebuild the overlay panel for the current loop iteration.
 */
static void ui_question_show(cptr title, cptr desc,
    const ui_question_option* options, int count, int anchor_y, int anchor_x,
    int highlight, int* scroll_offset, bool scroll_follow_highlight,
    const ui_question_button* buttons, int button_count)
{
    char letter[4];

    sdl_question_menu_begin(title);
    sdl_question_menu_set_scroll_offset_target(scroll_offset,
        scroll_follow_highlight);
    if ((anchor_y >= 0) && (anchor_x >= 0))
        sdl_question_menu_set_anchor_grid(anchor_y, anchor_x);
    if (desc && desc[0])
        sdl_question_menu_set_desc(desc);

    for (int i = 0; i < count; i++)
    {
        byte attr = options[i].disabled ? TERM_L_DARK : options[i].attr;

        if (options[i].key)
        {
            strnfmt(letter, sizeof(letter), "%c)", options[i].key);
            sdl_question_menu_add_entry(i, letter, options[i].label,
                attr);
        }
        else
        {
            sdl_question_menu_add_entry(i, "", options[i].label,
                attr);
        }
    }
    for (int i = 0; buttons && i < button_count; i++)
        sdl_question_menu_add_button(buttons[i].choice, buttons[i].label,
            buttons[i].disabled ? TERM_L_DARK : buttons[i].attr);

    sdl_question_menu_set_highlight(highlight);
    sdl_question_menu_finish();
}

static const ui_question_button* ui_question_find_button(
    const ui_question_button* buttons, int button_count, int choice)
{
    for (int i = 0; buttons && i < button_count; i++)
    {
        if (buttons[i].choice == choice)
            return &buttons[i];
    }

    return NULL;
}

static int ui_question_next_enabled(const ui_question_option* options,
    int count, int highlight, int direction)
{
    for (int offset = 1; offset <= count; offset++)
    {
        int candidate = (highlight + direction * offset) % count;

        if (candidate < 0)
            candidate += count;
        if (!options[candidate].disabled)
            return candidate;
    }

    return highlight;
}

static void ui_question_unavailable(void)
{
    bell("That choice is not available.");
}

static int ui_question_ask_aux(cptr title, cptr desc,
    const ui_question_option* options, int count, int anchor_y, int anchor_x,
    int default_index, bool repaint_background,
    const ui_question_button* buttons, int button_count)
{
    int highlight;
    int result = -1;
    bool done = false;
    bool steamdeck = steamdeck_controls_active();
    bool saved_hide_cursor = hide_cursor;
    bool scroll_follow_highlight = true;
    int scroll_offset = 0;
    char which;

    if (!options || count <= 0)
        return -1;

    highlight = ((default_index >= 0) && (default_index < count))
        ? default_index
        : 0;
    if (options[highlight].disabled)
        highlight = ui_question_next_enabled(options, count, highlight, 1);

    if (repaint_background)
    {
        /* Flush pending -more- prompts before taking over input */
        message_flush();

        /* request_command may have erased the top line; repaint the map so the
         * overlay sits over a clean view while we block for input. */
        p_ptr->redraw |= (PR_MAP);
        handle_stuff();
    }
    Term_fresh();

    /* The overlay panel owns the selection; never show the term cursor */
    hide_cursor = true;

    while (!done)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_question_show(title, desc, options, count, anchor_y, anchor_x,
            highlight, &scroll_offset, scroll_follow_highlight, buttons,
            button_count);

        which = inkey();
        if (sdl_question_menu_take_touch_scrolled())
            scroll_follow_highlight = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if ((clicked_choice >= 0) && (clicked_choice < count))
                {
                    highlight = clicked_choice;

                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    if (options[clicked_choice].disabled)
                    {
                        ui_question_unavailable();
                        continue;
                    }

                    result = clicked_choice;
                    done = true;
                }
                else
                {
                    const ui_question_button* button = ui_question_find_button(
                        buttons, button_count, clicked_choice);

                    if (button)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        if (button->disabled)
                        {
                            ui_question_unavailable();
                            continue;
                        }

                        result = clicked_choice;
                        done = true;
                    }
                }
                continue;
            }
            else if (which == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }
        }

        if ((which == ESCAPE)
            || (steamdeck && which == steamdeck_back_key()))
        {
            done = true;
            continue;
        }

        if (steamdeck && which == steamdeck_confirm_key())
            which = '\r';

        switch (which)
        {
        case '\r':
        case '\n':
        case ' ':
        case '6': /* right: select the highlighted answer */
        {
            if (options[highlight].disabled)
                ui_question_unavailable();
            else
            {
                result = highlight;
                done = true;
            }
            break;
        }

        case '2': /* down: next answer */
        {
            highlight = ui_question_next_enabled(options, count, highlight, 1);
            scroll_follow_highlight = true;
            break;
        }

        case '8': /* up: previous answer */
        {
            highlight = ui_question_next_enabled(options, count, highlight, -1);
            scroll_follow_highlight = true;
            break;
        }

        default:
        {
            bool matched = false;

            for (int i = 0; i < count; i++)
            {
                if (options[i].key
                    && (tolower((unsigned char)which)
                        == tolower((unsigned char)options[i].key)))
                {
                    matched = true;
                    highlight = i;
                    if (options[i].disabled)
                        ui_question_unavailable();
                    else
                    {
                        result = i;
                        done = true;
                    }
                    break;
                }
            }
            for (int i = 0; !matched && buttons && i < button_count; i++)
            {
                if (buttons[i].key
                    && (tolower((unsigned char)which)
                        == tolower((unsigned char)buttons[i].key)))
                {
                    matched = true;
                    if (buttons[i].disabled)
                        ui_question_unavailable();
                    else
                    {
                        result = buttons[i].choice;
                        done = true;
                    }
                    break;
                }
            }

            if (!matched)
                bell("Illegal response to question!");
            break;
        }
        }
    }

    hide_cursor = saved_hide_cursor;
    sdl_question_menu_clear();
    ui_menu_click_clear();
    Term_fresh();

    return result;
}

int ui_question_ask(cptr title, cptr desc, const ui_question_option* options,
    int count, int anchor_y, int anchor_x, int default_index)
{
    return ui_question_ask_aux(title, desc, options, count, anchor_y, anchor_x,
        default_index, true, NULL, 0);
}

int ui_question_ask_overlay(cptr title, cptr desc,
    const ui_question_option* options, int count, int anchor_y, int anchor_x,
    int default_index)
{
    return ui_question_ask_aux(title, desc, options, count, anchor_y, anchor_x,
        default_index, false, NULL, 0);
}

int ui_question_ask_overlay_buttons(cptr title, cptr desc,
    const ui_question_option* options, int count,
    const ui_question_button* buttons, int button_count, int anchor_y,
    int anchor_x, int default_index)
{
    return ui_question_ask_aux(title, desc, options, count, anchor_y, anchor_x,
        default_index, false, buttons, button_count);
}
