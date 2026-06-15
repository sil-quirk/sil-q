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
    int highlight)
{
    char letter[4];

    sdl_question_menu_begin(title);
    if ((anchor_y >= 0) && (anchor_x >= 0))
        sdl_question_menu_set_anchor_grid(anchor_y, anchor_x);
    if (desc && desc[0])
        sdl_question_menu_set_desc(desc);

    for (int i = 0; i < count; i++)
    {
        if (options[i].key)
        {
            strnfmt(letter, sizeof(letter), "%c)", options[i].key);
            sdl_question_menu_add_entry(i, letter, options[i].label,
                options[i].attr);
        }
        else
        {
            sdl_question_menu_add_entry(i, "", options[i].label,
                options[i].attr);
        }
    }

    sdl_question_menu_set_highlight(highlight);
    sdl_question_menu_finish();
}

static int ui_question_ask_aux(cptr title, cptr desc,
    const ui_question_option* options, int count, int anchor_y, int anchor_x,
    int default_index, bool repaint_background)
{
    int highlight;
    int result = -1;
    bool done = false;
    bool steamdeck = steamdeck_controls_active();
    bool saved_hide_cursor = hide_cursor;
    char which;

    if (!options || count <= 0)
        return -1;

    highlight = ((default_index >= 0) && (default_index < count))
        ? default_index
        : 0;

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
            highlight);

        which = inkey();

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

                    result = clicked_choice;
                    done = true;
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
            result = highlight;
            done = true;
            break;
        }

        case '2': /* down: next answer */
        {
            highlight = (highlight + 1) % count;
            break;
        }

        case '8': /* up: previous answer */
        {
            highlight = (highlight + count - 1) % count;
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
                    result = i;
                    done = true;
                    matched = true;
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
        default_index, true);
}

int ui_question_ask_overlay(cptr title, cptr desc,
    const ui_question_option* options, int count, int anchor_y, int anchor_x,
    int default_index)
{
    return ui_question_ask_aux(title, desc, options, count, anchor_y, anchor_x,
        default_index, false);
}
