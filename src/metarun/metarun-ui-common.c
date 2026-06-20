#include "angband.h"
#include "metarun-internal.h"

void metarun_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

int metarun_term_width(void)
{
    int wid = 80;
    int hgt = 24;

    if (Term)
        Term_get_size(&wid, &hgt);
    (void)hgt;

    if (wid < 1)
        wid = 80;

    return wid;
}

char metarun_inkey_hidden(void)
{
    bool old_hide_cursor = hide_cursor;
    char key;

    hide_cursor = true;
    key = inkey();
    hide_cursor = old_hide_cursor;

    return key;
}

void metarun_wait_hidden(void)
{
    (void)metarun_inkey_hidden();
}

void print_heading_fade(cptr title, byte final_attr)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    int w, h;
    Term_get_size(&w, &h);

    // Center the heading
    int title_len = strlen(title);
    int start_col = (w - title_len) / 2;
    if (start_col < 1) start_col = 1;

    sdl_story_font_enable();

    for (int s = 0; s < steps; s++)
    {
        c_prt(fade_cols[s], title, 2, start_col);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 150);
    }
    Term_xtra(TERM_XTRA_DELAY, 500); // Extra pause after heading

    sdl_story_font_disable();
}

bool print_paragraph_fade(cptr txt, byte final_attr, int row)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

    text_out_hook   = text_out_to_screen;
    text_out_indent = 2;
    text_out_wrap   = metarun_term_width() - 4;

    sdl_story_font_enable();

    for (int s = 0; s < steps; s++)
    {
        // Check for ESC key to skip fade
        char ch;
        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            // Show final state immediately and return interrupted status
            Term_gotoxy(2, row);
            text_out_c(final_attr, txt);
            text_out("\n");
            Term_fresh();
            sdl_story_font_disable();
            return false;
        }

        Term_gotoxy(2, row);
        text_out_c(fade_cols[s], txt);
        text_out("\n");
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }

    Term_xtra(TERM_XTRA_DELAY, 1000); // Pause after paragraph

    sdl_story_font_disable();
    return true;
}

void print_paragraph(cptr txt, byte attr)
{
    text_out_hook   = text_out_to_screen;
    text_out_indent = 1;
    text_out_wrap   = metarun_term_width() - 2;

    sdl_story_font_enable();

    Term_addstr(0, attr, "");
    text_out_c(attr, txt);
    text_out("\n");

    sdl_story_font_disable();
}

void wait_for_keypress_with_prompt(cptr prompt)
{
    int w, h;
    Term_get_size(&w, &h);

    // Clear bottom line and show prompt
    Term_erase(0, h - 1, w);
    if (prompt)
    {
        c_prt(TERM_L_WHITE, prompt, h - 1, 2);
    }
    else
    {
        char buf[48];

        any_key_prompt_text(buf, sizeof(buf), "continue");
        c_prt(TERM_L_WHITE, buf, h - 1, 2);
    }
    Term_fresh();

    metarun_wait_hidden();

    // Clear the prompt line
    Term_erase(0, h - 1, w);
}

cptr curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;
    /* Strip common prefixes for cleaner display */
    if (strncmp(raw, "Curse of ", 9) == 0) raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0) raw += 8;
    return raw;
}

cptr blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name) {
        cptr raw = cu_name + cu_info[idx].blessing_name;
        /* Strip "Blessing of " prefix for consistency */
        if (strncmp(raw, "Blessing of ", 12) == 0) raw += 12;
        return raw;
    }
    return curse_display_name(idx);
}

/*
 * Copy `name` into `buf` and pad it with trailing spaces until it spans `cols`
 * display columns.  This mirrors printf's "%-*s" minimum-width field, but it
 * counts glyph columns instead of bytes (via utf8_display_width_n), so any
 * column printed after the name stays aligned even when curse and blessing
 * names contain multibyte UTF-8 symbols.  Names already wider than `cols` are
 * left unpadded, exactly as a minimum-width field would leave them.  Returns
 * `buf` for convenient use inside a format argument list.
 */
cptr metarun_display_pad(char *buf, size_t size, cptr name, int cols)
{
    int width;
    size_t len;

    if (!buf || size == 0)
        return "";
    if (!name)
        name = "";

    SDL_strlcpy(buf, name, size);
    len = strlen(buf);
    width = utf8_display_width_n(buf, (int)len);

    while (width < cols && len + 1 < size) {
        buf[len++] = ' ';
        width++;
    }
    buf[len] = '\0';
    return buf;
}
