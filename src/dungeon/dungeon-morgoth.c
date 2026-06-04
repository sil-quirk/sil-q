/* File: dungeon/dungeon-morgoth.c */

#include "angband.h"
#include "dungeon-internal.h"

void morgoth_prompt_controller_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (!buf[0] || streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

static bool morgoth_prompt_confirm_key(int ch)
{
    if (steamdeck_controls_active() && ch == steamdeck_confirm_key())
        return true;

    return portable_controls_active()
        && ((ch == ' ') || (ch == '\r') || (ch == '\n'));
}

static bool morgoth_prompt_cancel_key(int ch)
{
    if (steamdeck_controls_active() && ch == steamdeck_back_key()
        && ch != steamdeck_confirm_key())
        return true;

    return (ch == ESCAPE);
}

bool confirm_enter_morgoth_hall(void)
{
    char ch;
    int wid, hgt;
    int title_row;
    int body_row;
    bool compact;

    static const char* text_wide[] = {
        "Beyond this passage lies the black hall of Morgoth Bauglir,",
        "the Dark Enemy, and the last of the Iron Hells.",
        "",
        "If you pass within, you may not return until you bear a Silmaril.",
        "Steel yourself: to enter is to choose doom or glory.",
        NULL,
    };
    static const char* text_compact[] = {
        "Beyond this passage lies the black hall",
        "of Morgoth Bauglir, the Dark Enemy,",
        "and the last of the Iron Hells.",
        "",
        "Pass within; return only with a Silmaril.",
        "Steel yourself: enter and choose doom or glory.",
        NULL,
    };
    const char** text;

    /* Paranoia */
    message_flush();

    /* Get terminal size */
    Term_get_size(&wid, &hgt);
    compact = (wid < 64) || (hgt <= 18);
    text = compact ? text_compact : text_wide;
    title_row = (hgt <= 16) ? 0 : (compact ? 1 : 2);
    body_row = (hgt <= 16) ? 2 : (compact ? 3 : 6);

    /* Save screen */
    screen_save();
    Term_clear();

    /* Title */
    {
        const char* title = "The Iron Gates of Angband";
        int col = (wid - (int)strlen(title)) / 2;
        if (col < 1)
            col = 1;
        Term_putstr(col, title_row, -1, TERM_L_RED, title);
    }

    /* Body */
    {
        int row = body_row;
        for (int i = 0; text[i] && row < hgt - 5; ++i)
        {
            const char* line = text[i];
            if (!line[0])
            {
                row++;
                continue;
            }

            int len = (int)strlen(line);
            int col = (wid - len) / 2;
            if (col < 1)
                col = 1;

            byte attr = (i == 3) ? TERM_L_RED : TERM_WHITE;
            Term_putstr(col, row, -1, attr, line);
            row++;
        }
    }

    bool steamdeck = steamdeck_controls_active();

    /* Prompt */
    {
        char prompt[80];

        if (steamdeck)
        {
            char confirm_label[16];
            char back_label[16];

            morgoth_prompt_controller_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            morgoth_prompt_controller_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(prompt, sizeof(prompt), "Enter Morgoth's hall? [%s/%s]",
                confirm_label, back_label);
        }
        else
        {
            SDL_strlcpy(prompt, portable_controls_active()
                    ? "Enter Morgoth's hall? [y/n/sp]"
                    : "Enter Morgoth's hall? [y/n]",
                sizeof(prompt));
        }

        int col = (wid - (int)strlen(prompt)) / 2;
        if (col < 1)
            col = 1;
        Term_putstr(col, hgt - 3, -1, TERM_YELLOW, prompt);
    }
    sdl_touch_pane_begin_yes_no_prompt_lower("Enter Morgoth's hall?");
    Term_fresh();

    /* Get an acceptable answer */
    while (true)
    {
        ch = inkey();
        if (quick_messages)
            break;
        if (morgoth_prompt_cancel_key(ch))
            break;
        if (strchr("YyNn", ch) || morgoth_prompt_confirm_key(ch))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }

    /* Restore screen */
    sdl_touch_pane_end_yes_no_prompt();
    screen_load();

    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y') && !morgoth_prompt_confirm_key(ch))
        return (false);

    return (true);
}

bool preconfirm_enter_morgoth_hall(void)
{
    if (!confirm_enter_morgoth_hall())
        return false;
    morgoth_entry_preconfirmed = true;
    return true;
}
