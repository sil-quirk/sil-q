/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "ui/story.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
/* =============================================================
 * Story display helpers & updated print_story() implementation
 * =============================================================
 *  Added features
 *    1.  Optional `last_parts` argument - when >0 only the *last*
 *        N matching story chapters are shown.
 *    2.  Optional `fade_in` boolean - when true, paragraphs are
 *        displayed with a colour fade-in effect.
 *
 *  Timing (per @Roman request 2025-07-30, amended)
 *    * 125 ms between colour steps
 *    * 1 second pause after each paragraph/"block"
 *
 *  UI tweaks 2025-07-30 - v4
 *    * Prints a **visible blank line** between paragraphs (not just
 *      a row counter increment).
 *    * [Esc] now finishes the *current* page instantly (no fades /
 *      delays) and proceeds through the rest of the story without
 *      further prompts, auto-scrolling as needed. Nothing skipped.
 *    * Footer fully wipes the bottom line before drawing its prompt
 *      to avoid text overlap.
 *
 * ===========================================================*/


/* -------------------------------------------------------------
 * Helper: colour fade-in paragraph printer
 * Returns true if completed normally, false if interrupted by Esc
 * ----------------------------------------------------------- */
static bool story_fast_forward_key(char ch)
{
    return (ch == ESCAPE)
        || (steamdeck_controls_active() && ch == steamdeck_back_key());
}

/* Return values: 0=completed normally, 1=other key pressed (skip paragraph), 2=ESC pressed (fast-forward) */
static int print_paragraph_fade(cptr text, int row, int indent,
                                 int wrap_width)
{
    const byte fade_cols[] = {
        TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, TERM_WHITE
    };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

    for (int s = 0; s < steps; s++)
    {
        /* Check for key press during fade */
        char ch;
        if (Term_inkey(&ch, false, false) == 0) /* Non-blocking check */
        {
            /* Consume the key */
            Term_inkey(&ch, false, true); /* Remove the key from queue */
            /* Show final text state immediately */
            text_out_indent = indent;
            text_out_wrap   = wrap_width;
            Term_gotoxy(indent, row);
            text_out_to_screen(TERM_WHITE, text);
            text_out_wrap   = 0;
            text_out_indent = 0;
            Term_fresh();
            /* Return different codes for ESC vs other keys */
            return story_fast_forward_key(ch) ? 2 : 1;
        }

        text_out_indent = indent;
        text_out_wrap   = wrap_width;
        Term_gotoxy(indent, row);
        text_out_to_screen(fade_cols[s], text);
        text_out_wrap   = 0;
        text_out_indent = 0;
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }

    /* Check for key press during final delay */
    char ch;
    if (Term_inkey(&ch, false, false) == 0) /* Non-blocking check */
    {
        /* Consume the key */
        Term_inkey(&ch, false, true); /* Remove the key from queue */
        /* Return different codes for ESC vs other keys */
        return story_fast_forward_key(ch) ? 2 : 1;
    }

    Term_xtra(TERM_XTRA_DELAY, 1000);
    return 0; /* Completed normally */
}

/* -------------------------------------------------------------
 * Public helper: fade-in a single line/paragraph at a row
 * ----------------------------------------------------------- */
void print_fade_line(cptr text, int row, int indent)
{
    int wid, h;
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent - 1;
    if (wrap_width < 10) wrap_width = 10;
    /* Reuse the paragraph fade; ignore return value here (non-interactive hint) */
    (void)print_paragraph_fade(text, row, indent, wrap_width);
}

/* -------------------------------------------------------------
 * Public helper: fade-in centered text, wrapping at (wid - 15)
 *  - Vertically centers the block of 1..N wrapped lines
 *  - Horizontally centers each line
 *  - Wraps on word boundaries; if a single word exceeds the
 *    width, it will be hard-split to avoid overflow
 * ----------------------------------------------------------- */
void print_fade_centered(cptr text)
{
    if (!text || !*text) return;

    int wid, h;
    Term_get_size(&wid, &h);

    int max_width = wid - 15;
    if (max_width < 10) max_width = (wid > 2 ? wid - 2 : wid);
    if (max_width < 1) max_width = 1;

    /* Simple word-wrapping into a small fixed buffer */
    enum { MAX_LINES = 32, MAX_LEN = 255 };
    char lines[MAX_LINES][MAX_LEN + 1];
    int  nlines = 0;

    const char *p = text;
    while (*p && nlines < MAX_LINES)
    {
        /* Start a new line */
        int linelen = 0;
        lines[nlines][0] = '\0';

        /* Skip leading spaces */
        while (*p && isspace((unsigned char)*p)) p++;

        while (*p)
        {
            /* Identify next word */
            const char *w = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            int wlen = (int)(p - w);

            /* If word does not fit on empty line: hard split */
            if (wlen > max_width && linelen == 0)
            {
                int take = (wlen > max_width) ? max_width : wlen;
                if (take > MAX_LEN) take = MAX_LEN;
                memcpy(lines[nlines], w, (size_t)take);
                linelen = take;
                lines[nlines][linelen] = '\0';

                /* Move pointer back to remaining part of the word */
                w += take;
                wlen -= take;
                p = w; /* continue from remainder */
                break; /* line filled */
            }

            /* Would adding this word (plus space if needed) fit? */
            int need = (linelen ? 1 : 0) + wlen;
            if (linelen + need <= max_width && linelen + need <= MAX_LEN)
            {
                if (linelen)
                    lines[nlines][linelen++] = ' ';
                memcpy(lines[nlines] + linelen, w, (size_t)wlen);
                linelen += wlen;
                lines[nlines][linelen] = '\0';
            }
            else
            {
                /* Doesn't fit: rewind so the word is processed on the next line */
                p = w;
                break;
            }

            /* Skip spaces to next word */
            while (*p && isspace((unsigned char)*p)) { if (*p == '\n') break; p++; }

            /* Stop at hard newline to keep author's breaks */
            if (*p == '\n') { p++; break; }
        }

        nlines++;

        /* Respect explicit newline(s) by collapsing consecutive breaks */
        while (*p == '\n') p++;
    }

    if (nlines == 0) return;

    /* Vertically center the block */
    int start_row = (h - nlines) / 2;
    if (start_row < 0) start_row = 0;

    /* Print each line centered with fade */
    for (int i = 0; i < nlines; i++)
    {
        int len = (int)strlen(lines[i]);
        if (len > wid) len = wid; /* paranoia */
        int indent = (wid - len) / 2;
        if (indent < 0) indent = 0;
        int wrap_width = wid - indent - 1;
        if (wrap_width < len) wrap_width = len; /* print as-is */
        (void)print_paragraph_fade(lines[i], start_row + i, indent, wrap_width);
    }
}

static bool banner_messages_use_stairs(void)
{
#if defined(__ANDROID__) || defined(SIL_IOS)
    const bool default_value = false;
#else
    const bool default_value = true;
#endif

    if (!op_ptr)
        return default_value;

    return op_ptr->opt[OPT_banner_message_stairs];
}

/* -------------------------------------------------------------
 * Public helper: show banner text at a row, left-aligned with indent.
 *  - Starts at the provided row (no vertical centering)
 *  - Left aligned at column >= 14
 *  - Optional stair layout offsets each subsequent line by 2 columns
 *  - Wraps dynamically per line width to ensure nothing is cut off
 *  - Can either fade lines in or draw them immediately
 *  - Optional line_delay adds the old staged banner pause between lines
 * ----------------------------------------------------------- */
void print_fade_centered_at_row(cptr text, int row_start, bool fade_in,
    bool line_delay)
{
    if (!text || !*text) return;

    int wid, h;
    bool stair_layout = banner_messages_use_stairs();
    Term_get_size(&wid, &h);

    /* Force to second row (index 1) if the caller requests anything above it */
    if (row_start < 1) row_start = 1;
    if (row_start >= h) return; /* off-screen */

    sdl_story_font_enable();
    log_debug("Depth banner: story font enabled");

    /* Dynamic per-line wrapping and printing */
    enum { MAX_LINES2 = 32, MAX_LEN2 = 255 };
    const char *p = text;
    int printed_lines = 0;
    /* Removed tracking arrays (line_start_cols/line_lengths) as they were
       only used for a future erase effect that is no longer implemented. */

    /* Start at the requested column; align to left-half in bigtile to avoid residuals */
    int base_indent = 14;
    if (use_bigtile)
    {
        /* Left halves are at COL_MAP, COL_MAP+2, ...; if we hit a right half, bump */
        if (((base_indent - COL_MAP) & 1) != 0) base_indent++;
    }

    while (*p && printed_lines < MAX_LINES2 && (row_start + printed_lines) < h)
    {
        int indent = base_indent + (stair_layout ? (2 * printed_lines) : 0);
        int avail;

        if (indent >= wid - 1)
            break; /* nothing to show */

        avail = wid - indent - 1;
        if (avail < 8)
            avail = 8; /* minimal width */

        char buf[MAX_LEN2 + 1];
        int  linelen = 0;
        buf[0] = '\0';

        /* Skip leading spaces/newlines */
        while (*p && (unsigned char)*p <= ' ') {
            if (*p == '\n') { p++; break; }
            p++; 
        }

        while (*p)
        {
            if (*p == '\n') { p++; break; }
            const char *w = p;
            while (*p && *p != '\n' && !isspace((unsigned char)*p)) p++;
            int wlen = (int)(p - w);

            if (wlen > avail && linelen == 0)
            {
                int take = (wlen > avail) ? avail : wlen;
                if (take > MAX_LEN2) take = MAX_LEN2;
                memcpy(buf, w, (size_t)take);
                linelen = take;
                buf[linelen] = '\0';
                w += take; /* remainder for next loop */
                p = w; 
                break;
            }

            int need = (linelen ? 1 : 0) + wlen;
            if (linelen + need <= avail && linelen + need <= MAX_LEN2)
            {
                if (linelen) buf[linelen++] = ' ';
                memcpy(buf + linelen, w, (size_t)wlen);
                linelen += wlen; buf[linelen] = '\0';
            }
            else
            {
                /* Defer word to the next line */
                p = w;
                break;
            }

            while (*p && isspace((unsigned char)*p)) { if (*p == '\n') break; p++; }
            if (*p == '\n') { p++; break; }
        }

        if (linelen == 0) break; /* nothing collected */

        if (fade_in)
        {
            print_fade_line(buf, row_start + printed_lines, indent);
        }
        else
        {
            /* Show this line directly in orange. */
            c_put_str(TERM_ORANGE, buf, row_start + printed_lines, indent);
            Term_fresh();
        }

    /* Tracking of per-line geometry removed (unused). */
        printed_lines++;

        /* Add a small gap between wrapped lines for delayed banners only. */
        if (!fade_in && line_delay && *p && (row_start + printed_lines) < h)
            Term_xtra(TERM_XTRA_DELAY, 800);
    }

    log_debug("Depth banner: story font disabled");
    sdl_story_font_disable();

    /* Do not explicitly erase: allow natural redraws to overwrite the text */
}

/* -------------------------------------------------------------
 * print_story() - paging, subset & fade-in options
 * ----------------------------------------------------------- */
static void story_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void story_print_hint(int indent, int h)
{
    if (steamdeck_controls_active()) {
        char next_label[16];
        char esc_label[16];
        char prompt_buf[80];

        story_prompt_label(steamdeck_confirm_key(), "A", next_label,
            sizeof(next_label));
        story_prompt_label(steamdeck_back_key(), "B", esc_label,
            sizeof(esc_label));

        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] next  *  [%s] fast forward", next_label, esc_label);
        Term_putstr(indent, h - 1, -1, TERM_SLATE, prompt_buf);
    } else {
        Term_putstr(indent, h - 1, -1, TERM_SLATE, "[Enter] next  *  [Esc] fast forward");
    }
}

static void story_touch_confirm_begin(int h)
{
    if (h < 1)
        return;

    ui_menu_click_begin();
    ui_menu_click_set_outside_cancel_enabled(true);
    for (int row = 0; row < h; row++)
        ui_menu_click_add_full_row('\r', row);
}

static void story_touch_confirm_end(void)
{
    ui_menu_click_clear();
}

void print_story(int last_parts, bool fade_in)
{
    int wid, h;
    const int indent = 2;
    bool fast_forward = false;
    bool show_page_instantly = false;
    bool _saved_cursor_state = false;
    bool _saved_hide_cursor = false;

    log_debug("=== Starting story display (parts=%d, fade_in=%s) ===", last_parts, fade_in ? "true" : "false");
    log_debug("last_parts=%d, fade_in=%s", last_parts, fade_in ? "true" : "false");

    /* Convenience macro to keep the bottom-line hint fresh */
#define REDRAW_HINT() \
    story_print_hint(indent, h)

    /* Build list of matching entries ------------------------ */
    int sils   = metar.silmarils;
    byte rt    = metar.type;
    int total  = 0;
    int max_st = z_info->st_max;
    static int sel_idx[1024];
    if (max_st > (int)N_ELEMENTS(sel_idx)) max_st = (int)N_ELEMENTS(sel_idx);

    log_debug("Building story list: sils=%d, rt=%d, max_st=%d", sils, rt, max_st);

    for (int i = 0; i < max_st; i++)
    {
        story_type *st = &st_info[i];
        if (!st->name && !st->text) continue;
        if (st->st_type != 0)               continue; /* opening */
        if (!(st->runtypes == 0 ||
              (rt < 32 && (st->runtypes & (1UL << rt)))))
            continue;                                   /* run-type */
        if (st->order <= (byte)sils) {
            sel_idx[total++] = i;
            log_trace("Added story %d (order=%d) to selection", i, st->order);
        }
    }

    log_debug("Found %d matching stories for display", total);
    
    if (total == 0) {
        log_debug("No stories match criteria - sils=%d, rt=%d", sils, rt);
        return;
    }

    /* Stable insertion sort by order ------------------------ */
    for (int i = 1; i < total; i++)
    {
        int key = sel_idx[i];
        byte key_ord = st_info[key].order;
        int j = i - 1;
        while (j >= 0 && st_info[sel_idx[j]].order > key_ord)
        {
            sel_idx[j + 1] = sel_idx[j];
            j--; }
        sel_idx[j + 1] = key;
    }

    /* Restrict to the last N parts if requested ------------- */
    int start = (last_parts > 0 && last_parts < total) ? total - last_parts : 0;
    log_debug("Story range: start=%d, total=%d", start, total);

    /* Screen prep ------------------------------------------- */
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    Term_get_size(&wid, &h);
    Term_clear();
    /* Hide the cursor during story display and restore it at the end */
    (void)Term_get_cursor(&_saved_cursor_state);
    /* Prevent inkey() from showing the cursor while story is active */
    _saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    (void)Term_set_cursor(false);

    sdl_story_font_enable();  // Enable for entire story display

    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
    int row = 2;
    REDRAW_HINT();

    /* Main loop -------------------------------------------- */
    for (int idx = start; idx < total; idx++)
    {
        story_type *st = &st_info[sel_idx[idx]];

        /* Calculate wrap width and get text */
        int wrap_width = wid - indent - 1;
        if (wrap_width < 20) wrap_width = 20;
        cptr text = st_text + st->text;
        
        /* Check if we need to paginate BEFORE rendering this story */
        /* Calculate actual space needed based on text content */
        int text_lines = count_wrapped_lines_story(text, wrap_width, indent);
        
        /* Space needed: 1 for heading + text_lines + 1 for blank line */
        int estimated_space_needed = 1 + text_lines + 1;
        
        if (row + estimated_space_needed >= h - 2)
        {
            if (!fast_forward)
            {
                show_page_instantly = false;
                REDRAW_HINT();
                story_touch_confirm_begin(h);
                char ch = inkey();
                story_touch_confirm_end();
                if (story_fast_forward_key(ch))
                {
                    fast_forward = true;
                    fade_in = false;
                    Term_erase(0, h - 1, wid);
                    log_debug("User pressed ESC - enabling fast forward mode");
                }
                else
                {
                    row = 2;
                    Term_clear();
                    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
                    REDRAW_HINT();
                }
            }
            else
            {
                row = 2;
                Term_clear();
                Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
            }
        }

        /* Heading */
        Term_putstr(indent, row, -1, TERM_L_BLUE, st_name + st->name);
        row++;
        
        /* Body */

        if (fade_in && !fast_forward && !show_page_instantly)
        {
            /* print_paragraph_fade returns:
             *   0 = completed normally
             *   1 = other key pressed (skip this paragraph)
             *   2 = ESC pressed (enable fast-forward) */
            story_touch_confirm_begin(h);
            int fade_result = print_paragraph_fade(text, row, indent, wrap_width);
            story_touch_confirm_end();
            if (fade_result == 2) {
                /* ESC pressed - enable fast-forward mode */
                fast_forward = true;
                fade_in = false;
                log_debug("ESC pressed during fade - enabling fast forward mode");
            }
            /* If fade_result == 1, just continue to next paragraph normally */
        }
        else
        {
            text_out_indent = indent;
            text_out_wrap   = wrap_width;
            Term_gotoxy(indent, row);
            text_out_to_screen(TERM_WHITE, text);
            text_out_wrap   = 0;
            text_out_indent = 0;
            if (!fast_forward && !show_page_instantly) 
                Term_xtra(TERM_XTRA_DELAY, 1000);
        }

        /* Get actual cursor position after text rendering */
        int cursor_x, cursor_y;
        Term_locate(&cursor_x, &cursor_y);
    /* Advance row past rendered text */
    row = cursor_y + 1;

        /* Check if we'll have room for the blank line before adding it */
        bool will_add_blank_line = (idx < total - 1);
        
        /* Pagination logic - check if we need to paginate BEFORE adding blank line */
    int space_needed = will_add_blank_line ? 1 : 0;
        
        bool paginated = false;
        if (row + space_needed >= h - 2)
        {
            paginated = true;
            if (!fast_forward)
            {
                /* Reset show_page_instantly for next page */
                show_page_instantly = false;
                
                REDRAW_HINT();
                story_touch_confirm_begin(h);
                char ch = inkey();
                story_touch_confirm_end();
                if (story_fast_forward_key(ch))
                {
                    fast_forward = true;
                    fade_in      = false;   /* Disable delays for rest of story */
                    Term_erase(0, h - 1, wid); /* clear hint line */
                    log_debug("User pressed ESC - enabling fast forward mode");
                }
                else /* Enter */
                {
                    row = 2;
                    Term_clear();
                    sdl_story_font_enable();
                    Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
                    sdl_story_font_disable();
                    REDRAW_HINT();
                    continue;
                }
            }
            else
            {
                /* fast-forward mode: auto-clear, no waits */
                row = 2;
                Term_clear();
                Term_putstr(indent, 0, -1, TERM_YELLOW, "=== The Tale So Far ===");
            }
        }

        /* Add visible blank spacer line between paragraphs if there's room and we didn't paginate */
        if (will_add_blank_line && !paginated)
        {
            Term_putstr(indent, row, -1, TERM_WHITE, "");
            row++;
        }
    }

    /* Footer ------------------------------------------------ */
    Term_erase(0, h - 1, wid); /* clear bottom line entirely */
    if (steamdeck_controls_active()) {
        char next_label[16];
        char prompt_buf[64];
        story_prompt_label(steamdeck_confirm_key(), "A", next_label,
            sizeof(next_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] continue", next_label);
        Term_putstr(indent, h - 1, -1, TERM_L_WHITE, prompt_buf);
    } else {
        Term_putstr(indent, h - 1, -1, TERM_L_WHITE,
                    "[Press any key to continue]");
    }
    story_touch_confirm_begin(h);
    (void)inkey();
    story_touch_confirm_end();
    
    /* Flush any queued keypresses that accumulated during the story */
    Term_flush();
    
    sdl_story_font_disable();  // Disable after story display
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    /* Restore previous cursor visibility and hide_cursor flag */
    (void)Term_set_cursor(_saved_cursor_state);
    hide_cursor = _saved_hide_cursor;

    log_debug("Story display completed");

#undef REDRAW_HINT
}

