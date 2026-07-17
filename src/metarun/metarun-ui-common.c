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

typedef enum metarun_poetry_fade_part {
    METARUN_POETRY_FADE_TITLE,
    METARUN_POETRY_FADE_BODY,
    METARUN_POETRY_FADE_TRANSITION
} metarun_poetry_fade_part;

static bool metarun_poetry_fade_alpha(metarun_poetry_fade_part part,
    int duration_ms, bool allow_skip)
{
    const int frame_ms = 16;

    for (int elapsed = 0; elapsed < duration_ms; elapsed += frame_ms)
    {
        byte alpha = (byte)((elapsed * 255) / MAX(1, duration_ms));
        int delay_ms = MIN(frame_ms, duration_ms - elapsed);

        if (allow_skip)
        {
            char ch;

            if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
                return false;
        }

        if (part == METARUN_POETRY_FADE_TITLE)
            sdl_poetry_screen_set_alpha(alpha, 0, 0, 0);
        else if (part == METARUN_POETRY_FADE_BODY)
            sdl_poetry_screen_set_alpha(255, alpha, 0, 0);
        else
            sdl_poetry_screen_set_alpha(255, 255, alpha, 0);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, delay_ms);
    }

    return true;
}

static bool metarun_poetry_fade_block(int block, byte final_attr,
    bool outcome_colour_reveal, int duration_ms, bool allow_skip)
{
    const int frame_ms = 16;

    /* Reveal an unresolved roll in white, then change once to its semantic
     * result colour after the fully opaque white state has been readable. */
    sdl_poetry_screen_set_block_attr(block,
        outcome_colour_reveal ? TERM_WHITE : final_attr);

    for (int elapsed = 0; elapsed < duration_ms; elapsed += frame_ms)
    {
        byte alpha = (byte)((elapsed * 255) / MAX(1, duration_ms));
        int delay_ms = MIN(frame_ms, duration_ms - elapsed);

        if (allow_skip)
        {
            char ch;

            if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
                return false;
        }

        sdl_poetry_screen_set_block_alpha(block, alpha);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, delay_ms);
    }

    if (outcome_colour_reveal)
    {
        /* Hold the fully opaque unresolved result before the single outcome
         * colour change. */
        sdl_poetry_screen_set_block_alpha(block, 255);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 500);
    }

    return true;
}

/* Present a full-window poetic interlude with semantic pixel layout.  The
 * caller supplies semantic colours and prose; SDL owns wrapping and placement. */
void metarun_show_poetry_scene(cptr title, byte title_attr, cptr body,
    byte body_attr, cptr transition, byte transition_attr, cptr prompt)
{
    bool body_fast_forward = false;
    bool transition_fast_forward = false;

    sdl_poetry_screen_begin(title, body, transition, prompt);
    if (!sdl_poetry_screen_active())
    {
        log_error("Unable to open mandatory SDL poetry screen");
        return;
    }

    sdl_story_font_enable();

    sdl_poetry_screen_update(true, title_attr, false, body_attr,
        false, transition_attr, false);
    sdl_poetry_screen_set_alpha(0, 0, 0, 0);
    (void)metarun_poetry_fade_alpha(METARUN_POETRY_FADE_TITLE, 450,
        false);
    sdl_poetry_screen_set_alpha(255, 0, 0, 0);
    Term_fresh();
    Term_xtra(TERM_XTRA_DELAY, 500);

    if (body && body[0])
    {
        sdl_poetry_screen_update(true, title_attr, true, body_attr,
            false, transition_attr, false);
        body_fast_forward = !metarun_poetry_fade_alpha(
            METARUN_POETRY_FADE_BODY, 500, true);
    }
    sdl_poetry_screen_update(true, title_attr, body && body[0], body_attr,
        false, transition_attr, false);
    sdl_poetry_screen_set_alpha(255, 255, 0, 0);
    Term_fresh();
    if (!body_fast_forward)
        Term_xtra(TERM_XTRA_DELAY, 1000);

    if (transition && transition[0])
    {
        sdl_poetry_screen_update(true, title_attr,
            body && body[0], body_attr, true, transition_attr, false);
        transition_fast_forward = !metarun_poetry_fade_alpha(
            METARUN_POETRY_FADE_TRANSITION, 500, true);
    }
    sdl_poetry_screen_update(true, title_attr, body && body[0], body_attr,
        transition && transition[0], transition_attr, false);
    sdl_poetry_screen_set_alpha(255, 255, 255, 0);
    Term_fresh();
    if (transition && transition[0] && !transition_fast_forward)
        Term_xtra(TERM_XTRA_DELAY, 1000);

    sdl_poetry_screen_update(true, title_attr, body && body[0], body_attr,
        transition && transition[0], transition_attr, true);
    sdl_poetry_screen_set_alpha(255, 255, 255, 255);
    Term_fresh();
    ui_key_wait_dismiss_begin('\r');
    metarun_wait_hidden();
    ui_key_wait_dismiss_clear();

    sdl_poetry_screen_hide();
    sdl_story_font_disable();
}

/* Present one of the original multi-paragraph epilogue screens.  Ordinary
 * prose keeps its semantic colour while fading by alpha.  Treachery and
 * kinslaying rolls fade naturally in white, then settle once on their
 * pass/fail colour. */
void metarun_show_poetry_blocks(cptr title, byte title_attr,
    cptr blocks[], const byte block_attrs[],
    const bool block_outcome_reveals[], int block_count, cptr prompt,
    int hold_ms, bool wait_for_key, bool immediate, bool* fast_forward)
{
    bool skip_remaining = fast_forward && *fast_forward;

    if (!blocks || !block_attrs || block_count < 1)
        return;
    sdl_poetry_screen_begin_blocks(title, prompt);
    if (!sdl_poetry_screen_active())
    {
        log_error("Unable to open mandatory SDL poetry-block screen");
        return;
    }

    for (int i = 0; i < block_count; i++)
    {
        if (sdl_poetry_screen_add_block(blocks[i], block_attrs[i]) < 0)
        {
            sdl_poetry_screen_hide();
            return;
        }
    }

    sdl_story_font_enable();
    if (immediate)
    {
        sdl_poetry_screen_update(true, title_attr, false, TERM_WHITE, false,
            TERM_WHITE, false);
        sdl_poetry_screen_set_alpha(255, 0, 0, 0);
        for (int i = 0; i < block_count; i++)
        {
            sdl_poetry_screen_set_block_visible(i, true);
            sdl_poetry_screen_set_block_attr(i, block_attrs[i]);
            sdl_poetry_screen_set_block_alpha(i, 255);
        }
        Term_fresh();
    }
    else
    {
        sdl_poetry_screen_update(true, title_attr, false, TERM_WHITE, false,
            TERM_WHITE, false);
        sdl_poetry_screen_set_alpha(0, 0, 0, 0);
        (void)metarun_poetry_fade_alpha(METARUN_POETRY_FADE_TITLE, 450,
            false);
        sdl_poetry_screen_set_alpha(255, 0, 0, 0);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 500);

        for (int i = 0; i < block_count; i++)
        {
            bool completed_fade = false;

            sdl_poetry_screen_set_block_visible(i, true);
            if (!skip_remaining)
            {
                completed_fade = metarun_poetry_fade_block(i,
                    block_attrs[i],
                    block_outcome_reveals && block_outcome_reveals[i],
                    500, true);
            }
            sdl_poetry_screen_set_block_attr(i, block_attrs[i]);
            sdl_poetry_screen_set_block_alpha(i, 255);
            Term_fresh();

            if (!skip_remaining && !completed_fade)
            {
                skip_remaining = true;
                if (fast_forward)
                    *fast_forward = true;
            }
            else if (!skip_remaining)
            {
                Term_xtra(TERM_XTRA_DELAY, 1000);
            }
        }
    }

    if (hold_ms > 0)
        Term_xtra(TERM_XTRA_DELAY, hold_ms);

    if (wait_for_key)
    {
        sdl_poetry_screen_set_prompt(prompt, true);
        sdl_poetry_screen_set_alpha(255, 0, 0, 255);
        Term_fresh();
        ui_key_wait_dismiss_begin('\r');
        metarun_wait_hidden();
        ui_key_wait_dismiss_clear();
    }

    sdl_poetry_screen_hide();
    sdl_story_font_disable();
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
