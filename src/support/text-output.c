#include "angband.h"
#include "support/text-output.h"
#include "externs.h"
#include "log/log.h"
#include "support/utf8.h"
#include "ui/story_font.h"

/*
 * Print some (colored) text to the screen at the current cursor position,
 * automatically "wrapping" existing text (at spaces) when necessary to
 * avoid placing any text into the last column, and clearing every line
 * before placing any text in that line.  Also, allow "newline" to force
 * a "wrap" to the next line.  Advance the cursor as needed so sequential
 * calls to this function will work correctly.
 *
 * Once this function has been called, the cursor should not be moved
 * until all the related "text_out()" calls to the window are complete.
 *
 * This function will correctly handle any width up to the maximum legal
 * value of 256, though it works best for a standard 80 character width.
 */
int count_wrapped_lines(cptr str, int wrap_width, int indent)
{
    int x = indent;
    int lines = 1;
    bool have_space_on_line = false;
    int chars_since_space = 0;
    cptr s;

    if (!str)
        return lines;

    for (s = str; *s; )
    {
        int char_len;
        int char_width;
        bool is_space;

        if (*s == '\n')
        {
            x = indent;
            lines++;
            have_space_on_line = false;
            chars_since_space = 0;
            s++;
            continue;
        }

        char_len = utf8_sequence_len(s);
        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(s, char_len);
        is_space = (*s == ' ');

        /*
         * Mirror text_out_to_screen(): wrap on a non-space once we have
         * reached the wrap boundary, and carry the current trailing word to
         * the next line when there is a prior break space on this line.
         */
        if ((x >= wrap_width - 1) && !is_space && (char_width > 0))
        {
            int moved_chars = 0;

            if (have_space_on_line && (chars_since_space > 0))
                moved_chars = chars_since_space;

            x = indent + moved_chars;
            lines++;
            have_space_on_line = false;
            chars_since_space = moved_chars;
        }

        x += char_width;

        if (is_space)
        {
            have_space_on_line = true;
            chars_since_space = 0;
        }
        else if (char_width > 0)
        {
            chars_since_space += char_width;
        }

        s += char_len;
    }

    return lines;
}

void text_out_to_screen(byte a, cptr str)
{
    bool has_utf8;
    bool story_enabled;
    bool story_grid;

    if (!str)
        str = "";

    has_utf8 = utf8_has_non_ascii(str);

    /* If story font is enabled, use pixel-based wrapping.  Grid-aligned story
     * text falls back to free rendering for UTF-8 so SDL_ttf sees intact
     * multibyte sequences.  Normal UI UTF-8 stays on the monospace path. */
    extern bool sdl_is_story_font_enabled(void);
    story_enabled = sdl_is_story_font_enabled();
    story_grid = (story_enabled && Term && Term->story_font_grid);
    if (story_enabled && (!story_grid || has_utf8))
    {
        if (story_grid && has_utf8)
        {
            story_font_term_state prev;
            story_font_term_push(true, false, &prev);
            text_out_to_screen_story(a, str);
            story_font_term_pop(&prev);
        }
        else
        {
            text_out_to_screen_story(a, str);
        }
        return;
    }

    if (story_enabled && Term && Term->story_font_grid)
    {
        log_debug("text_out_to_screen: grid story text using cell wrapping "
            "at row=%d col=%d wrap=%d text='%.50s'",
            Term->scr ? Term->scr->cy : -1, Term->scr ? Term->scr->cx : -1,
            text_out_wrap, str ? str : "");
    }

    int x, y;

    int wid, h;

    int wrap;

    cptr s;

    /* Obtain the size */
    (void)Term_get_size(&wid, &h);

    /* Obtain the cursor */
    (void)Term_locate(&x, &y);

    /* Use special wrapping boundary? */
    if ((text_out_wrap > 0) && (text_out_wrap < wid))
        wrap = text_out_wrap;
    else
        wrap = wid;

    /* Process the string */
    for (s = str; *s; s++)
    {
        char ch;

        /* Force wrap */
        if (*s == '\n')
        {
        // /* DEBUG: confirm we saw a newline */
        // fprintf(log_file, "text_out: saw \\n at offset %ld, x=%d,y=%d\n", (long)(s - str), x, y);

            /* Wrap */
            x = text_out_indent;
            y++;

            /* Clear line, move cursor */
            Term_erase(x, y, 255);

            continue;
        }

        /* Clean up the char */
        ch = (((unsigned char)*s >= 0x80) || isprint((unsigned char)*s)) ? *s : ' ';

        /* Wrap words as needed */
        if ((x >= wrap - 1) && (ch != ' '))
        {
            int i, n = 0;

            byte av[256];
            char cv[256];

            /* Wrap word */
            if (x < wrap)
            {
                /* Scan existing text */
                for (i = wrap - 2; i >= 0; i--)
                {
                    /* Grab existing attr/char */
                    Term_what(i, y, &av[i], &cv[i]);

                    /* Break on space */
                    if (cv[i] == ' ')
                        break;

                    /* Track current word */
                    n = i;
                }
            }

            /* Special case */
            if (n == 0)
                n = wrap;

            /* Clear line */
            Term_erase(n, y, 255);

            /* Wrap */
            x = text_out_indent;
            y++;

            /* Clear line, move cursor */
            Term_erase(x, y, 255);

            /* Wrap the word (if any) */
            for (i = n; i < wrap - 1; i++)
            {
                /* Dump */
                Term_addch(av[i], cv[i]);

                /* Advance (no wrap) */
                if (++x > wrap)
                    x = wrap;
            }
        }

        /* Dump */
        Term_addch(a, ch);

        /* Advance */
        if (++x > wrap)
            x = wrap;
    }
}

/*
 * Write text to the screen with story font wrapping based on pixel width.
 * This version wraps proportional fonts to fill the available terminal width
 * instead of wrapping based on monospace character count.
 */
void text_out_to_file(byte a, cptr str)
{
    /* Current position on the line */
    static int pos = 0;

    /* Wrap width */
    int wrap = (text_out_wrap ? text_out_wrap : 75);

    /* Current location within "str" */
    cptr s = str;

    /* Unused parameter */
    (void)a;

    /* Process the string */
    while (*s)
    {
        char ch;
        int n = 0;
        int len = wrap - pos;
        int l_space = -1;

        /* If we are at the start of the line... */
        if (pos == 0)
        {
            int i;

            /* Output the indent */
            for (i = 0; i < text_out_indent; i++)
            {
                unsigned char space = ' ';
                SDL_WriteIO(text_out_file, &space, 1);
                pos++;
            }
        }

        /* Find length of line up to next newline or end-of-string */
        while ((n < len) && !((s[n] == '\n') || (s[n] == '\0')))
        {
            /* Mark the most recent space in the string */
            if (s[n] == ' ')
                l_space = n;

            /* Increment */
            n++;
        }

        /* If we have encountered no spaces */
        if ((l_space == -1) && (n == len))
        {
            /* If we are at the start of a new line */
            if (pos == text_out_indent)
            {
                len = n;
            }
            /* HACK - Output punctuation at the end of the line */
            else if ((s[0] == ' ') || (s[0] == ',') || (s[0] == '.'))
            {
                len = 1;
            }
            else
            {
                /* Begin a new line */
                unsigned char newline = '\n';
                SDL_WriteIO(text_out_file, &newline, 1);

                /* Reset */
                pos = 0;

                continue;
            }
        }
        else
        {
            /* Wrap at the newline */
            if ((s[n] == '\n') || (s[n] == '\0'))
                len = n;

            /* Wrap at the last space */
            else
                len = l_space;
        }

        /* Write that line to file */
        for (n = 0; n < len; n++)
        {
            /* Ensure the character is printable */
            ch = (((unsigned char)s[n] >= 0x80) || isprint((unsigned char)s[n])) ? s[n] : ' ';

            /* Write out the character */
            unsigned char byte = ch;
            SDL_WriteIO(text_out_file, &byte, 1);

            /* Increment */
            pos++;
        }

        /* Move 's' past the stuff we've written */
        s += len;

        /* If we are at the end of the string, end */
        if (*s == '\0')
            return;

        /* Skip newlines */
        if (*s == '\n')
            s++;

        /* Begin a new line */
        unsigned char newline = '\n';
        SDL_WriteIO(text_out_file, &newline, 1);

        /* Reset */
        pos = 0;

        /* Skip whitespace */
        while (*s == ' ')
            s++;
    }

    /* We are done */
    return;
}

/*
 * Output text to the screen or to a file depending on the selected
 * text_out hook.
 */
void text_out(cptr str) { text_out_c(TERM_WHITE, str); }

/*
 * Output text to the screen (in color) or to a file depending on the
 * selected hook.
 */
void text_out_c(byte a, cptr str) { text_out_hook(a, str); }
