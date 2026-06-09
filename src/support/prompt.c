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

static void format_check_prompt(char* buf, size_t buflen, cptr prompt,
    cptr suffix)
{
    int term_wid = active_term_width();
    int suffix_wid = suffix ? (int)strlen(suffix) : 0;
    int prompt_wid = term_wid - suffix_wid;

    if (!buf || !buflen)
        return;

    if (!prompt)
        prompt = "";
    if (!suffix)
        suffix = "";

    if (prompt_wid < 0)
        prompt_wid = 0;

    strnfmt(buf, buflen, "%.*s%s", prompt_wid, prompt, suffix);
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
 * Request a "quantity" from the user
 *
 * Allow "p_ptr->command_arg" to specify a quantity
 */
#define QUANTITY_CLICK_DECREASE -1001
#define QUANTITY_CLICK_CONFIRM  -1002
#define QUANTITY_CLICK_INCREASE -1003
#define QUANTITY_CLICK_ALL      -1004
#define QUANTITY_CLICK_ZERO     -1005
#define QUANTITY_CLICK_CANCEL   -1006

static void quantity_prompt_draw(cptr prompt, int current, int max,
    int touch_category)
{
    char header[180];
    char actions[120];
    char pick_token[32];
    cptr cancel_token = "[Cancel]";
    int term_wid = 80;

    if (Term)
    {
        int term_hgt = 0;
        Term_get_size(&term_wid, &term_hgt);
        (void)term_hgt;
    }

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    ui_menu_click_set_touch_category(touch_category);

    strnfmt(header, sizeof(header), "%s%d/%d", prompt, current, max);
    prt(header, 0, 0);

    strnfmt(pick_token, sizeof(pick_token), "[Pick %d]", current);
    strnfmt(actions, sizeof(actions), "[-] %s [+] [All] [0] [Cancel]",
        pick_token);

    if ((int)strlen(actions) > term_wid)
    {
        SDL_strlcpy(pick_token, "[OK]", sizeof(pick_token));
        SDL_strlcpy(actions, "[-] [OK] [+] [All] [0] [Esc]",
            sizeof(actions));
        cancel_token = "[Esc]";
    }

    prt(actions, 1, 0);
    ui_menu_click_add_text_token(QUANTITY_CLICK_DECREASE, 0, 1, actions,
        "[-]");
    ui_menu_click_add_text_token(QUANTITY_CLICK_CONFIRM, 0, 1, actions,
        pick_token);
    ui_menu_click_add_text_token(QUANTITY_CLICK_INCREASE, 0, 1, actions,
        "[+]");
    ui_menu_click_add_text_token(QUANTITY_CLICK_ALL, 0, 1, actions, "[All]");
    ui_menu_click_add_text_token(QUANTITY_CLICK_ZERO, 0, 1, actions, "[0]");
    ui_menu_click_add_text_token(QUANTITY_CLICK_CANCEL, 0, 1, actions,
        cancel_token);

    Term_fresh();
}

static s16b get_quantity_aux(cptr prompt, int max, int touch_category,
    bool force_prompt)
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
        int ch;

        if (!prompt)
        {
            strnfmt(prompt_buf, sizeof(prompt_buf), "Quantity (0-%d): ", max);
            prompt = prompt_buf;
        }

        if (max < 0)
            max = 0;

        current = MAX(0, MIN(current, max));

        while (!done)
        {
            quantity_prompt_draw(prompt, current, max, touch_category);

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
                        ch = '\r';
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

        prt("", 0, 0);
        prt("", 1, 0);
        ui_menu_click_clear();

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
    return get_quantity_aux(prompt, max, SDL_TOUCH_MENU_CATEGORY_OTHER, false);
}

s16b get_quantity_touch_category(cptr prompt, int max, int touch_category)
{
    return get_quantity_aux(prompt, max, touch_category, false);
}

s16b get_quantity_touch_category_force_prompt(cptr prompt, int max,
    int touch_category)
{
    return get_quantity_aux(prompt, max, touch_category, true);
}

/*
 * Hack - duplication of get_check prompt to give option of setting destroyed
 * option to squelch.
 *
 * 0 - No
 * 1 = Yes
 * 2 = third option
 *
 * The "prompt" should take the form "Query? "
 *
 * Note that a compact confirmation suffix is appended to the prompt.
 */
int get_check_other(cptr prompt, char other)
{
    char ch;
    char buf[160];
    char suffix[32];
    bool steamdeck = steamdeck_controls_active();

    /*default set to no*/
    int result = 0;

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Hack -- Build a "useful" prompt */
    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        prompt_controller_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        prompt_controller_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(suffix, sizeof(suffix), "[%s/%s/%c] ", confirm_label,
            back_label, other);
    }
    else
    {
        strnfmt(suffix, sizeof(suffix), "[y/n/%c/sp] ", other);
    }
    format_check_prompt(buf, sizeof(buf), prompt, suffix);

    ui_menu_click_clear_pending_hover();

    /* Prompt for it */
    prt(buf, 0, 0);

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
        if (strchr("YyNn", ch))
            break;
        if (prompt_confirm_key(ch))
            break;
        if (ch == toupper(other))
            break;
        if (ch == tolower(other))
            break;
        bell("Illegal response to question!");
    }

    /* Erase the prompt */
    prt("", 0, 0);

    /* Normal negation */
    if ((ch == 'Y') || (ch == 'y') || prompt_confirm_key(ch))
        result = 1;
    /*other option*/
    else if ((ch == toupper(other)) || (ch == tolower(other)))
        result = 2;
    /*all else default to no*/

    /* Success */
    return (result);
}

/*
 * Verify something with the user
 *
 * The "prompt" should take the form "Query? "
 *
 * Note that a compact confirmation suffix is appended to the prompt.
 */
bool get_check(cptr prompt)
{
    char ch;

    /* Paranoia XXX XXX XXX */
    message_flush();

    ui_menu_click_clear_pending_hover();

    /* Ask through the modal yes/no question menu instead of a top-line prompt */
    sdl_touch_pane_begin_yes_no_prompt(prompt);
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
    prt("", 0, 0);
    Term_fresh();

    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y') && !prompt_confirm_key(ch))
        return (false);

    /* Success */
    return (true);
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
 * Give a prompt, then get a choice withing a certain range.
 */
int get_menu_choice(s16b max, char* prompt)
{
    int choice = -1;
    int highlight = 0;
    int ch;
    bool steamdeck = steamdeck_controls_active();
    bool done = false;
    char prompt_buf[160];

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        prompt_controller_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        prompt_controller_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt_buf, sizeof(prompt_buf),
            "%s D-pad choose, %s select, %s cancel", prompt, confirm_label,
            back_label);
    }
    else
        SDL_strlcpy(prompt_buf, prompt, sizeof(prompt_buf));

    prt(prompt_buf, 0, 0);

    while (!done)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        for (int i = 0; i < max; i++)
            ui_menu_click_add_full_row(i, i + 1);

        if (steamdeck && max > 0)
        {
            for (int i = 0; i < max; i++)
                Term_putstr(0, i + 1, 1,
                    (i == highlight) ? TERM_L_BLUE : TERM_SLATE,
                    (i == highlight) ? ">" : " ");
            Term_gotoxy(0, highlight + 1);
            Term_fresh();
        }

        ch = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action)
                && clicked_choice >= 0 && clicked_choice < max)
            {
                highlight = clicked_choice;
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                choice = clicked_choice;
                done = true;
                continue;
            }
        }

        /* Letters are used for selection */
        if (!steamdeck && isalpha((unsigned char)ch))
        {
            if (islower((unsigned char)ch))
            {
                choice = A2I(ch);
            }
            else
            {
                choice = ch - 'A' + 26;
            }

            /* Validate input */
            if ((choice > -1) && (choice < max))
            {
                done = true;
            }

            else
            {
                bell("Illegal response to question!");
            }
        }
        else if (steamdeck && max > 0 && (ch == '8'
#ifdef ARROW_UP
            || ch == ARROW_UP
#endif
            ))
        {
            highlight = (highlight + max - 1) % max;
        }
        else if (steamdeck && max > 0 && (ch == '2'
#ifdef ARROW_DOWN
            || ch == ARROW_DOWN
#endif
            ))
        {
            highlight = (highlight + 1) % max;
        }
        else if (steamdeck && max > 0
            && ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
                || (ch == steamdeck_confirm_key())))
        {
            choice = highlight;
            done = true;
        }

        /* Allow user to exit the fuction */
        else if ((ch == ESCAPE) || (steamdeck && ch == steamdeck_back_key()))
        {
            /* Mark as no choice made */
            choice = -1;

            done = true;
        }
        else if (steamdeck && isalpha((unsigned char)ch))
        {
            bell("Use D-pad and confirm to select in this mode.");
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");
    }

    /* Clear the prompt */
    ui_menu_click_clear();
    prt("", 0, 0);

    /* Return */
    return (choice);
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
 * Pause for user response
 *
 * This function is stupid.  XXX XXX XXX
 */
void pause_line(int row)
{
    prt("", row, 0);
    put_str("(press any key)", row, 23);
    (void)inkey();
    prt("", row, 0);
}
