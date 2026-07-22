#include "angband.h"
#include "support/prompt.h"
#include "externs.h"
#include "sdl-config.h"
#include "support/input.h"
#include "support/feedback.h"
#include "ui/menu-click.h"

/*
 * Get some input at the cursor location.
 *
 * The buffer is assumed to have been initialized to a default string.
 * Note that this string is often "empty" (see below).
 *
 * The default buffer is displayed in yellow until cleared, which happens
 * on the first keypress, unless that keypress is Return.
 *
 * Normal chars clear the default and append the char.
 * Backspace clears the default or deletes the final char.
 * Return accepts the current buffer contents and returns true.
 * Escape clears the buffer and the window and returns false.
 *
 * Note that 'len' refers to the size of the buffer.  The maximum length
 * of the input is 'len-1'.
 */
static int active_term_width(void)
{
    int wid = 80;
    int hgt = 24;

    if (Term)
        Term_get_size(&wid, &hgt);

    if (wid < 1)
        wid = 80;

    return wid;
}

bool terminal_prompt_fits(cptr prompt, int max_width, bool use_story_font)
{
    size_t len;

    if (!prompt)
        return true;

    if (max_width <= 0)
        return false;

    len = strlen(prompt);
    if (len > (size_t)max_width)
        return false;

    if (use_story_font && sdl_is_story_font_enabled())
    {
        int cell_width = sdl_get_cell_width();

        if (cell_width <= 0)
            return true;

        return sdl_story_font_text_width(prompt, (int)len)
            <= max_width * cell_width;
    }

    return true;
}

void terminal_prompt_trim(char* prompt, int max_width, bool use_story_font)
{
    size_t len;

    if (!prompt)
        return;

    if (max_width <= 0)
    {
        prompt[0] = '\0';
        return;
    }

    len = strlen(prompt);
    while (len > 0 && !terminal_prompt_fits(prompt, max_width, use_story_font))
    {
        prompt[--len] = '\0';
        while (len > 0 && isspace((unsigned char)prompt[len - 1]))
            prompt[--len] = '\0';
    }
}

void terminal_prompt_pick_variant(char* out, size_t out_size, int max_width,
    bool use_story_font, const char* const variants[], size_t variant_count)
{
    cptr fallback = "";

    if (!out || out_size == 0)
        return;

    out[0] = '\0';

    if (!variants || variant_count == 0)
        return;

    for (size_t i = 0; i < variant_count; i++)
    {
        cptr variant = variants[i];

        if (!variant)
            continue;

        fallback = variant;
        if (terminal_prompt_fits(variant, max_width, use_story_font))
        {
            SDL_strlcpy(out, variant, out_size);
            return;
        }
    }

    SDL_strlcpy(out, fallback, out_size);
    terminal_prompt_trim(out, max_width, use_story_font);
}

void terminal_prompt_put_variant(int col, int row, int max_width, byte attr,
    bool use_story_font, const char* const variants[], size_t variant_count)
{
    char prompt[256];
    int term_wid = active_term_width();
    int width = max_width;

    if (col < 0)
        col = 0;

    if (width <= 0 || col + width > term_wid)
        width = term_wid - col;
    if (width < 0)
        width = 0;

    terminal_prompt_pick_variant(prompt, sizeof(prompt), width,
        use_story_font, variants, variant_count);
    Term_putstr(col, row, width, attr, prompt);
}

static void prompt_controller_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (!buf[0] || streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

static bool prompt_confirm_key(int ch)
{
    if (steamdeck_controls_active() && ch == steamdeck_confirm_key())
        return true;

    /* Space/Enter always confirm a yes/no question, on every platform, so the
     * modal panel behaves the same regardless of the active control scheme. */
    return (ch == ' ') || (ch == '\r') || (ch == '\n');
}

static bool prompt_cancel_key(int ch)
{
    if (steamdeck_controls_active() && ch == steamdeck_back_key()
        && ch != steamdeck_confirm_key())
        return true;

    return (ch == ESCAPE);
}

bool askfor_aux(char* buf, size_t len)
{
    int y, x;
    int term_wid = active_term_width();

    size_t k = 0;

    char ch = '\0';

    bool done = false;

    /* Locate the cursor */
    Term_locate(&x, &y);

    /* Paranoia */
    if ((x < 0) || (x >= term_wid))
        x = 0;

    /* Restrict the length */
    if ((size_t)x + len > (size_t)term_wid)
        len = (size_t)(term_wid - x);
    if (len < 1)
        len = 1;

    /* Truncate the default entry */
    buf[len - 1] = '\0';

    /* Display the default answer */
    Term_erase(x, y, (int)len);
    Term_putstr(x, y, -1, TERM_YELLOW, buf);

    /* Process input */
    while (!done)
    {
        /* Place cursor */
        Term_gotoxy(x + k, y);

        /* Get a key */
        inkey_request_text_cursor();
        ch = inkey();

        /* Analyze the key */
        switch (ch)
        {
        case ESCAPE:
        {
            k = 0;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            k = strlen(buf);
            done = true;
            break;
        }

        case 0x7F:
        case '\010':
        {
            if (k > 0)
                k--;
            break;
        }

        default:
        {
            if ((k < len - 1) && (isprint((unsigned char)ch)))
            {
                buf[k++] = ch;
            }
            else
            {
                bell("Illegal edit key!");
            }
            break;
        }
        }

        /* Terminate */
        buf[k] = '\0';

        /* Update the entry */
        Term_erase(x, y, (int)len);
        Term_putstr(x, y, -1, TERM_WHITE, buf);
    }

    /* Done */
    return (ch != ESCAPE);
}

/*
 * A reimplementation of askfor_aux, but allows for random names
 *
 * Sil-y: this is poor style...
 */
bool askfor_name(char* buf, size_t len)
{
    int y, x;
    int term_wid = active_term_width();

    size_t k = 0;

    char ch = '\0';

    bool done = false;
    bool new_default_name = false;

    /* Locate the cursor */
    Term_locate(&x, &y);

    /* Paranoia */
    if ((x < 0) || (x >= term_wid))
        x = 0;

    /* Restrict the length */
    if ((size_t)x + len > (size_t)term_wid)
        len = (size_t)(term_wid - x);
    if (len < 1)
        len = 1;

    /* Truncate the default entry */
    buf[len - 1] = '\0';

    /* Display the default answer */
    Term_erase(x, y, (int)len);
    Term_putstr(x, y, -1, TERM_YELLOW, buf);

    /* Process input */
    while (!done)
    {
        /* Place cursor */
        Term_gotoxy(x + k, y);

        /* Get a key */
        inkey_request_text_cursor();
        ch = inkey();

        /* Analyze the key */
        switch (ch)
        {
        case ESCAPE:
        {
            k = 0;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            k = strlen(buf);
            done = true;
            break;
        }

        case 0x7F:
        case '\010':
        {
            if (k > 0)
                k--;
            break;
        }

        case '\t':
        {
            /*get the random name, display for approval. */
            make_random_name(buf, len);

            new_default_name = true;
            k = 0;
            break;
        }

        default:
        {
            if ((k < len - 1) && (isprint((unsigned char)ch)))
            {
                buf[k++] = ch;
            }
            else
            {
                bell("Illegal edit key!");
            }
            break;
        }
        }

        if (new_default_name)
        {
            /* Display the random name */
            Term_erase(x, y, (int)len);
            Term_putstr(x, y, -1, TERM_YELLOW, buf);

            new_default_name = false;
        }
        else
        {
            /* Terminate */
            buf[k] = '\0';

            /* Update the entry */
            Term_erase(x, y, (int)len);
            Term_putstr(x, y, -1, TERM_WHITE, buf);
        }
    }

    /* Done */
    return (ch != ESCAPE);
}

/*
 * Prompt for a string from the user.
 *
 * The "prompt" should take the form "Prompt: ".
 *
 * See "askfor_aux" for some notes about "buf" and "len", and about
 * the return value of this function.
 */
bool term_get_string(cptr prompt, char* buf, size_t len)
{
    bool res;

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Display prompt */
    prt(prompt, 0, 0);

    /* Ask the user for a string */
    res = askfor_aux(buf, len);

    /* Clear prompt */
    prt("", 0, 0);

    /* Result */
    return (res);
}

/*
 * Choice ids for the free-text entry panel rows (>= 0 so the question menu
 * treats them as clickable).
 */
#define STRING_PANEL_CLICK_CONFIRM 2001
#define STRING_PANEL_CLICK_CLEAR   2002
#define STRING_PANEL_CLICK_CANCEL  2003

/*
 * Draw the text-entry overlay panel: the prompt is the title, the current
 * text (with a trailing caret) is the highlighted confirm row, plus Clear and
 * Cancel rows so mouse/touch can drive it.
 */
static void string_panel_draw(cptr prompt, const char* text, int touch_category)
{
    char line[120];

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    ui_menu_click_set_touch_category(touch_category);

    sdl_question_menu_begin(prompt);

    /* Show the current text with a caret (just the caret when empty) */
    strnfmt(line, sizeof(line), "%s_", (text && text[0]) ? text : "");
    sdl_question_menu_add_entry(STRING_PANEL_CLICK_CONFIRM, "", line,
        TERM_L_BLUE);

    sdl_question_menu_add_entry(STRING_PANEL_CLICK_CLEAR, "", "Clear",
        TERM_L_WHITE);
    sdl_question_menu_add_entry(STRING_PANEL_CLICK_CANCEL, "", "Cancel",
        TERM_SLATE);

    sdl_question_menu_set_highlight(STRING_PANEL_CLICK_CONFIRM);
    sdl_question_menu_finish();

    Term_fresh();
}

/*
 * Prompt the player for a string using the modal overlay panel instead of the
 * legacy top-row editor.  "buf" supplies the default text and receives the
 * result; typing edits it, Enter/confirm accepts, Esc/Cancel aborts.  Returns
 * false when cancelled (leaving the entered text in "buf" regardless).
 */
bool get_string_panel(cptr prompt, char* buf, size_t len)
{
    size_t k;
    bool done = false;
    bool canceled = false;
    bool saved_hide_cursor = hide_cursor;

    if (!buf || len < 1)
        return false;

    /* Start the caret after any default text */
    buf[len - 1] = '\0';
    k = strlen(buf);

    /* The overlay owns the display; never show the term cursor */
    hide_cursor = true;

    while (!done)
    {
        int ch;

        string_panel_draw(prompt ? prompt : "Enter text:", buf,
            SDL_TOUCH_MENU_CATEGORY_OTHER);

        ch = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                switch (clicked_choice)
                {
                case STRING_PANEL_CLICK_CONFIRM:
                    ch = '\r';
                    break;
                case STRING_PANEL_CLICK_CLEAR:
                    k = 0;
                    buf[0] = '\0';
                    continue;
                case STRING_PANEL_CLICK_CANCEL:
                    ch = ESCAPE;
                    break;
                default:
                    break;
                }
            }
        }

        switch (ch)
        {
        case UI_MENU_CLICK_WAKE_KEY:
            break;

        case ESCAPE:
            canceled = true;
            done = true;
            break;

        case '\r':
        case '\n':
            done = true;
            break;

        case '\b':
        case 0x7F:
            if (k > 0)
                k--;
            buf[k] = '\0';
            break;

        default:
            if (isprint((unsigned char)ch) && (k < len - 1))
            {
                buf[k++] = (char)ch;
                buf[k] = '\0';
            }
            break;
        }
    }

    hide_cursor = saved_hide_cursor;
    sdl_question_menu_clear();
    ui_menu_click_clear();
    Term_fresh();

    return (!canceled);
}

/*
 * Request a "quantity" from the user
 *
 * Allow "p_ptr->command_arg" to specify a quantity
 */
/* Choice ids for the quantity overlay rows (must be >= 0: the question
 * menu treats negative choices as non-clickable info lines). */
#define QUANTITY_CLICK_DECREASE 1001
#define QUANTITY_CLICK_CONFIRM  1002
#define QUANTITY_CLICK_INCREASE 1003
#define QUANTITY_CLICK_ALL      1004
#define QUANTITY_CLICK_ZERO     1005
#define QUANTITY_CLICK_CANCEL   1006

/*
 * Show the quantity picker as a question overlay panel (never the top
 * message rows).  The +/- rows adjust the count without closing; digits
 * and 8/2 still edit it from the keyboard.
 */
static void quantity_prompt_draw(cptr prompt, cptr action, int current,
    int max, int touch_category)
{
    char line[80];

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    ui_menu_click_set_touch_category(touch_category);

    sdl_question_menu_begin(prompt);

    strnfmt(line, sizeof(line), "%s %d",
        (action && action[0]) ? action : "Take", current);
    sdl_question_menu_add_entry(QUANTITY_CLICK_CONFIRM, "", line,
        TERM_L_BLUE);
    sdl_question_menu_add_entry(QUANTITY_CLICK_INCREASE, "+)", "More",
        TERM_L_WHITE);
    sdl_question_menu_add_entry(QUANTITY_CLICK_DECREASE, "-)", "Fewer",
        TERM_L_WHITE);
    strnfmt(line, sizeof(line), "All (%d)", max);
    sdl_question_menu_add_entry(QUANTITY_CLICK_ALL, "a)", line,
        TERM_L_WHITE);
    sdl_question_menu_add_entry(QUANTITY_CLICK_ZERO, "0)", "None",
        TERM_L_WHITE);
    sdl_question_menu_add_entry(QUANTITY_CLICK_CANCEL, "", "Cancel",
        TERM_SLATE);

    sdl_question_menu_set_highlight(QUANTITY_CLICK_CONFIRM);
    sdl_question_menu_finish();

    Term_fresh();
}

static s16b get_quantity_aux(cptr prompt, cptr action, int max,
    int touch_category, bool force_prompt)
{
    int amt = (max > 0) ? max : 1;

    /* Use "command_arg" */
    if (p_ptr->command_arg)
    {
        amt = p_ptr->command_arg;
        p_ptr->command_arg = 0;
    }

#ifdef ALLOW_REPEAT

    else if (!force_prompt && (max != 1) && repeat_pull(&amt))
    {
        /* use repeated value */
    }

#endif /* ALLOW_REPEAT */

    else if (force_prompt || max != 1)
    {
        char prompt_buf[80];
        char entry_buf[16] = "";
        int entry_len = 0;
        int current = amt;
        bool done = false;
        bool canceled = false;
        bool saved_hide_cursor = hide_cursor;
        int ch;

        if (!prompt)
        {
            strnfmt(prompt_buf, sizeof(prompt_buf), "How many? (0-%d)", max);
            prompt = prompt_buf;
        }

        if (max < 0)
            max = 0;

        current = MAX(0, MIN(current, max));

        /* The overlay panel owns the selection; never show the term cursor */
        hide_cursor = true;

        while (!done)
        {
            quantity_prompt_draw(prompt, action, current, max,
                touch_category);

            ch = inkey();

            {
                int clicked_choice = 0;
                int click_action = UI_MENU_CLICK_PRIMARY;

                if (ui_menu_click_take_action(&clicked_choice, &click_action))
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    switch (clicked_choice)
                    {
                    case QUANTITY_CLICK_DECREASE:
                        ch = '-';
                        break;
                    case QUANTITY_CLICK_CONFIRM:
                        ch = '\r';
                        break;
                    case QUANTITY_CLICK_INCREASE:
                        ch = '+';
                        break;
                    case QUANTITY_CLICK_ALL:
                        current = max;
                        entry_len = 0;
                        entry_buf[0] = '\0';
                        ch = '\r';
                        break;
                    case QUANTITY_CLICK_ZERO:
                        current = 0;
                        entry_len = 0;
                        entry_buf[0] = '\0';
                        ch = UI_MENU_CLICK_WAKE_KEY;
                        break;
                    case QUANTITY_CLICK_CANCEL:
                        ch = ESCAPE;
                        break;
                    default:
                        break;
                    }
                }
            }

            switch (ch)
            {
            case UI_MENU_CLICK_WAKE_KEY:
                break;

            case ESCAPE:
                canceled = true;
                done = true;
                break;

            case '\r':
            case '\n':
            case ' ':
#ifdef KC_ENTER
            case KC_ENTER:
#endif
                done = true;
                break;

            case '+':
            case '=':
            case '8':
            case 'k':
            case 'K':
#ifdef ARROW_UP
            case ARROW_UP:
#endif
                if (max > 0)
                {
                    if (current >= max)
                        current = 0;
                    else
                        current++;
                }
                else
                {
                    current = 0;
                }
                entry_len = 0;
                entry_buf[0] = '\0';
                break;

            case '-':
            case '_':
            case '2':
            case 'j':
            case 'J':
#ifdef ARROW_DOWN
            case ARROW_DOWN:
#endif
                if (max > 0)
                {
                    if (current > 0)
                        current--;
                    else
                        current = max;
                }
                else
                {
                    current = 0;
                }
                entry_len = 0;
                entry_buf[0] = '\0';
                break;

#ifdef KC_PGUP
            case KC_PGUP:
                if (max > 0)
                {
                    current += 10;
                    if (current > max)
                        current = 0;
                }
                else
                {
                    current = 0;
                }
                entry_len = 0;
                entry_buf[0] = '\0';
                break;
#endif

#ifdef KC_PGDOWN
            case KC_PGDOWN:
                if (max > 0)
                {
                    current -= 10;
                    if (current < 0)
                        current = max;
                }
                else
                {
                    current = 0;
                }
                entry_len = 0;
                entry_buf[0] = '\0';
                break;
#endif

            case 'a':
            case 'A':
                current = max;
                entry_len = 0;
                entry_buf[0] = '\0';
                done = true;
                break;

            case '\b':
            case 0x7F:
                if (entry_len > 0)
                {
                    entry_buf[--entry_len] = '\0';
                    current = entry_len ? MAX(0, MIN(atoi(entry_buf), max)) : 0;
                }
                else
                {
                    bell("Nothing to erase.");
                }
                break;

            default:
                if (isdigit((unsigned char)ch))
                {
                    if (entry_len < (int)sizeof(entry_buf) - 1)
                    {
                        entry_buf[entry_len++] = (char)ch;
                        entry_buf[entry_len] = '\0';
                        current = MAX(0, MIN(atoi(entry_buf), max));
                    }
                    else
                    {
                        bell("Quantity too large.");
                    }
                }
                else
                {
                    bell("Illegal response to quantity prompt!");
                }
                break;
            }
        }

        hide_cursor = saved_hide_cursor;
        sdl_question_menu_clear();
        ui_menu_click_clear();
        Term_fresh();

        if (canceled)
            return (0);

        amt = current;
    }

    if (amt > max)
        amt = max;

    if (amt < 0)
        amt = 0;

#ifdef ALLOW_REPEAT

    if (amt)
        repeat_push(amt);

#endif /* ALLOW_REPEAT */

    return (amt);
}

s16b get_quantity(cptr prompt, int max)
{
    return get_quantity_aux(prompt, "Take", max,
        SDL_TOUCH_MENU_CATEGORY_OTHER, false);
}

s16b get_quantity_action(cptr prompt, cptr action, int max)
{
    return get_quantity_aux(prompt, action, max,
        SDL_TOUCH_MENU_CATEGORY_OTHER, false);
}

s16b get_quantity_touch_category(cptr prompt, int max, int touch_category)
{
    return get_quantity_aux(prompt, "Take", max, touch_category, false);
}

s16b get_quantity_touch_category_action(cptr prompt, cptr action, int max,
    int touch_category)
{
    return get_quantity_aux(prompt, action, max, touch_category, false);
}

s16b get_quantity_touch_category_force_prompt(cptr prompt, int max,
    int touch_category)
{
    return get_quantity_aux(prompt, "Take", max, touch_category, true);
}

s16b get_quantity_touch_category_force_prompt_action(cptr prompt, cptr action,
    int max, int touch_category)
{
    return get_quantity_aux(prompt, action, max, touch_category, true);
}

/*
 * Verify something with the user
 *
 * The "prompt" should take the form "Query? "
 *
 * Global questions centre the modal yes/no panel; local questions (about a
 * specific map grid) anchor it next to that grid.  Full-screen confirms that
 * already draw their own descriptive text pass lower=true so the panel sits at
 * the bottom of the screen instead of covering that text.
 */
static bool get_check_aux(cptr prompt, int anchor_y, int anchor_x, bool lower)
{
    char ch;
    bool saved_hide_cursor = hide_cursor;

    /* Paranoia XXX XXX XXX */
    message_flush();

    ui_menu_click_clear_pending_hover();

    /* Ask through the modal yes/no question menu instead of a top-line prompt */
    if ((anchor_y >= 0) && (anchor_x >= 0))
        sdl_touch_pane_begin_yes_no_prompt_near(prompt, anchor_y, anchor_x);
    else if (lower)
        sdl_touch_pane_begin_yes_no_prompt_lower(prompt);
    else
        sdl_touch_pane_begin_yes_no_prompt(prompt);

    /* The modal panel owns the selection; do not expose the term cursor. */
    hide_cursor = true;
    Term_fresh();

    /* Get an acceptable answer */
    while (true)
    {
        ch = inkey();
        if (ch == UI_MENU_CLICK_WAKE_KEY)
            continue;
        if (quick_messages)
            break;
        if (prompt_cancel_key(ch))
            break;
        if (strchr("YyNn", ch) || prompt_confirm_key(ch))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }

    /* Erase the prompt */
    sdl_touch_pane_end_yes_no_prompt();
    hide_cursor = saved_hide_cursor;
    prt("", 0, 0);
    Term_fresh();

    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y') && !prompt_confirm_key(ch))
        return (false);

    /* Success */
    return (true);
}

bool get_check(cptr prompt)
{
    return get_check_aux(prompt, -1, -1, false);
}

/*
 * Yes/no question for a full-screen confirm that already shows its own text
 * (e.g. the Blitz intro): the panel is anchored to the bottom of the screen so
 * it does not cover that text, matching the "Enter Morgoth's hall?" prompt.
 */
bool get_check_lower(cptr prompt)
{
    return get_check_aux(prompt, -1, -1, true);
}

/*
 * Local yes/no question about a specific map grid (a trap, door, stair,
 * chasm, monster, ...): the panel spawns next to that grid.
 */
bool get_check_near(int y, int x, cptr prompt)
{
    return get_check_aux(prompt, y, x, false);
}

/*
 * Multiline version of get_check() for long oath confirmation prompts
 * Displays text with proper word wrapping and fade effects
 */
bool get_check_oath_multiline(cptr prompt)
{
    char ch;
    int wid, h;
    char confirm_prompt[80];
    
    /* Paranoia */
    message_flush();
    
    /* Get terminal size */
    Term_get_size(&wid, &h);
    
    /* Save screen */
    screen_save();
    Term_clear();
    
    /* Title */
    Term_putstr((wid - 24) / 2, 2, -1, TERM_L_RED, "Breaking a Sacred Oath");
    
    /* Display the oath confirmation prompt with word wrapping */
    if (prompt && prompt[0]) {
        char* desc_ptr = (char*)prompt;
        char line_buffer[80];
        int row = 5;
        int max_width = 70; /* Leave margins */
        
        while (*desc_ptr && row < h - 4) {
            int line_len = 0;
            char* line_start = desc_ptr;
            
            /* Find the longest line that fits */
            while (*desc_ptr && line_len < max_width) {
                if (*desc_ptr == ' ') {
                    /* Potential break point */
                    if (line_len > 0 && line_len + 1 < max_width) {
                        memcpy(line_buffer, line_start, (size_t)line_len);
                        line_buffer[line_len] = '\0';
                    }
                }
                line_len++;
                desc_ptr++;
            }
            
            /* Back up to last space if we exceeded width */
            if (line_len >= max_width && *desc_ptr) {
                while (desc_ptr > line_start && *desc_ptr != ' ') {
                    desc_ptr--;
                    line_len--;
                }
                if (*desc_ptr == ' ') desc_ptr++; /* Skip the space */
            }
            
            /* Copy the line */
            int actual_len = desc_ptr - line_start;
            if (actual_len > 79) actual_len = 79;
            memcpy(line_buffer, line_start, (size_t)actual_len);
            line_buffer[actual_len] = '\0';
            
            /* Remove trailing space */
            while (actual_len > 0 && line_buffer[actual_len - 1] == ' ') {
                actual_len--;
                line_buffer[actual_len] = '\0';
            }
            
            /* Display centered line */
            if (actual_len > 0) {
                int start_col = (wid - actual_len) / 2;
                if (start_col < 1) start_col = 1;
                Term_putstr(start_col, row, -1, TERM_WHITE, line_buffer);
                row++;
            }
            
            /* Skip whitespace for next line */
            while (*desc_ptr && *desc_ptr == ' ') desc_ptr++;
            
            if (!*desc_ptr) break;
        }
    }
    
    /* Prompt at bottom */
    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char back_label[16];

        prompt_controller_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        prompt_controller_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(confirm_prompt, sizeof(confirm_prompt),
            "Are you certain? [%s/%s]", confirm_label, back_label);
    }
    else
    {
        SDL_strlcpy(confirm_prompt, "Are you certain? [y/n/sp]",
            sizeof(confirm_prompt));
    }
    int prompt_col = (wid - (int)strlen(confirm_prompt)) / 2;
    if (prompt_col < 1)
        prompt_col = 1;
    Term_putstr(prompt_col, h - 3, -1, TERM_YELLOW, confirm_prompt);
    ui_menu_click_clear_pending_hover();
    sdl_touch_pane_begin_yes_no_prompt("Are you certain?");
    Term_fresh();
    
    /* Get an acceptable answer */
    while (true)
    {
        ch = inkey();
        if (ch == UI_MENU_CLICK_WAKE_KEY)
            continue;
        if (quick_messages)
            break;
        if (prompt_cancel_key(ch))
            break;
        if (strchr("YyNn", ch) || prompt_confirm_key(ch))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }
    
    /* Restore screen */
    sdl_touch_pane_end_yes_no_prompt();
    screen_load();
    
    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y') && !prompt_confirm_key(ch))
        return (false);
    
    /* Success */
    return (true);
}

/*
 * Prompts for a keypress
 *
 * The "prompt" should take the form "Command: "
 *
 * Returns true unless the character is "Escape"
 */
bool get_com(cptr prompt, char* command)
{
    char ch;

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Display a prompt */
    prt(prompt, 0, 0);

    /* Get a key */
    ch = inkey();

    /* Clear the prompt */
    prt("", 0, 0);

    /* Save the command */
    *command = ch;

    /* Done */
    return (ch != ESCAPE);
}

/*
 * Compose a control-appropriate "press any key" style prompt into buf.
 *
 * Controller users see the confirm-button label (e.g. "[A] continue"); on
 * keyboard and touch-only devices the text keeps the literal "press any key"
 * wording, which the touch layer recognises (see sdl_screen_shows_any_key_
 * prompt) so that a tap anywhere dismisses the prompt.  "action" is an
 * optional verb phrase such as "continue" or "return"; pass NULL for the
 * bare form.
 */
void any_key_prompt_text(char* buf, size_t len, cptr action)
{
    if (!buf || !len)
        return;

    if (steamdeck_controls_active())
    {
        char label[16];

        prompt_controller_label(steamdeck_confirm_key(), "A", label,
            sizeof(label));
        strnfmt(buf, len, "[%s] %s", label,
            (action && action[0]) ? action : "continue");
    }
    else if (action && action[0])
    {
        strnfmt(buf, len, "(press any key to %s)", action);
    }
    else
    {
        SDL_strlcpy(buf, "(press any key)", len);
    }
}

/*
 * Pause for user response
 *
 * This function is stupid.  XXX XXX XXX
 */
void pause_line(int row)
{
    char buf[48];

    any_key_prompt_text(buf, sizeof(buf), NULL);
    prt("", row, 0);
    put_str(buf, row, 23);
    (void)inkey();
    prt("", row, 0);
}
