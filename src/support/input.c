#include "angband.h"
#include "support/input.h"
#include "externs.h"
#include "support/macro.h"
#include "support/movement-input.h"
#include "ui/menu-click.h"

/*
 * Flush all pending input.
 *
 * Actually, remember the flush, using the "inkey_xtra" flag, and in the
 * next call to "inkey()", perform the actual flushing, for efficiency,
 * and correctness of the "inkey()" function.
 */
void flush(void)
{
    movement_input_clear_commands();

    /* Do it later */
    inkey_xtra = true;
}

/*
 * Flush all pending input if the flush_failure option is set.
 */
void flush_fail(void) { flush(); }

/*
 * Local variable -- we are inside a "macro action"
 *
 * Do not match any macros until "ascii 30" is found.
 */
static bool parse_macro = false;

/*
 * Local variable -- we are inside a "macro trigger"
 *
 * Strip all keypresses until a low ascii value is found.
 */
static bool parse_under = false;

/*
 * Helper function called only from "inkey()"
 *
 * This function does almost all of the "macro" processing.
 *
 * We use the "Term_key_push()" function to handle "failed" macros, as well
 * as "extra" keys read in while choosing the proper macro, and also to hold
 * the action for the macro, plus a special "ascii 30" character indicating
 * that any macro action in progress is complete.  Embedded macros are thus
 * illegal, unless a macro action includes an explicit "ascii 30" character,
 * which would probably be a massive hack, and might break things.
 *
 * Only 500 (0+1+2+...+29+30) milliseconds may elapse between each key in
 * the macro trigger sequence.  If a key sequence forms the "prefix" of a
 * macro trigger, 500 milliseconds must pass before the key sequence is
 * known not to be that macro trigger.  XXX XXX XXX
 */
static char inkey_aux(void)
{
    int k, n;
    int p = 0, w = 0;

    char ch;

    cptr pat, act;

    char buf[1024];

    /* Wait for a keypress */
    (void)(Term_inkey(&ch, true, true));

    /* End "macro action" */
    if (ch == 30)
        parse_macro = false;

    /* Inside "macro action" */
    if (ch == 30)
        return (ch);

    /* Inside "macro action" */
    if (parse_macro)
        return (ch);

    /* Inside "macro trigger" */
    if (parse_under)
        return (ch);

    /* Save the first key, advance */
    buf[p++] = ch;
    buf[p] = '\0';

    /* Check for possible macro */
    k = macro_find_check(buf);

    /* No macro pending */
    if (k < 0)
        return (ch);

    /* Wait for a macro, or a timeout */
    while (true)
    {
        /* Check for pending macro */
        k = macro_find_maybe(buf);

        /* No macro pending */
        if (k < 0)
            break;

        /* Check for (and remove) a pending key */
        if (0 == Term_inkey(&ch, false, true))
        {
            /* Append the key */
            buf[p++] = ch;
            buf[p] = '\0';

            /* Restart wait */
            w = 0;
        }

        /* No key ready */
        else
        {
            /* Increase "wait" */
            w += 10;

            /* Excessive delay */
            if (w >= 100)
                break;

            /* Delay */
            Term_xtra(TERM_XTRA_DELAY, w);
        }
    }

    /* Check for available macro */
    k = macro_find_ready(buf);

    /* No macro available */
    if (k < 0)
    {
        /* Push all the keys back on the queue */
        while (p > 0)
        {
            /* Push the key, notice over-flow */
            if (Term_key_push(buf[--p]))
                return (0);
        }

        /* Wait for (and remove) a pending key */
        (void)Term_inkey(&ch, true, true);

        /* Return the key */
        return (ch);
    }

    /* Get the pattern */
    pat = macro__pat[k];

    /* Get the length of the pattern */
    n = strlen(pat);

    /* Push the "extra" keys back on the queue */
    while (p > n)
    {
        /* Push the key, notice over-flow */
        if (Term_key_push(buf[--p]))
            return (0);
    }

    /* Begin "macro action" */
    parse_macro = true;

    /* Push the "end of macro action" key */
    if (Term_key_push(30))
        return (0);

    /* Get the macro action */
    act = macro__act[k];

    /* Get the length of the action */
    n = strlen(act);

    /* Push the macro "action" onto the key queue */
    while (n > 0)
    {
        /* Push the key, notice over-flow */
        if (Term_key_push(act[--n]))
            return (0);
    }

    /* Hack -- Force "inkey()" to call us again */
    return (0);
}

/*
 * Mega-Hack -- special "inkey_next" pointer.  XXX XXX XXX
 *
 * This special pointer allows a sequence of keys to be "inserted" into
 * the stream of keys returned by "inkey()".  This key sequence will not
 * trigger any macros.  It is used in Angband to handle "keymaps".
 */
static cptr inkey_next = NULL;
static bool inkey_text_cursor_requested = false;

void inkey_request_text_cursor(void)
{
    inkey_text_cursor_requested = true;
}

bool inkey_next_active(void)
{
    return inkey_next != NULL;
}

void inkey_next_set(cptr keys)
{
    inkey_next = keys;
}

/*
 * Get a keypress from the user.
 *
 * This function recognizes a few "global parameters".  These are variables
 * which, if set to true before calling this function, will have an effect
 * on this function, and which are always reset to false by this function
 * before this function returns.  Thus they function just like normal
 * parameters, except that most calls to this function can ignore them.
 *
 * If "inkey_xtra" is true, then all pending keypresses will be flushed,
 * and any macro processing in progress will be aborted.  This flag is
 * set by the "flush()" function, which does not actually flush anything
 * itself, but rather, triggers delayed input flushing via "inkey_xtra".
 *
 * If "inkey_scan" is true, then we will immediately return "zero" if no
 * keypress is available, instead of waiting for a keypress.
 *
 * If "inkey_base" is true, then all macro processing will be bypassed.
 * If "inkey_base" and "inkey_scan" are both true, then this function will
 * not return immediately, but will wait for a keypress for as long as the
 * normal macro matching code would, allowing the direct entry of macro
 * triggers.  The "inkey_base" flag is extremely dangerous!
 *
 * If "inkey_flag" is true, then we will assume that we are waiting for a
 * normal command, and we will only show the cursor if "hilite_player" is
 * true (or if the player is in a store), instead of always showing the
 * cursor.  The various "main-xxx.c" files should avoid saving the game
 * in response to a "menu item" request unless "inkey_flag" is true, to
 * prevent savefile corruption.
 *
 * Saved/special screens render their selections themselves, so their legacy
 * terminal cursor is always hidden.  A real free-text editor may opt in to a
 * one-call text cursor with inkey_request_text_cursor().
 *
 * If we are waiting for a keypress, and no keypress is ready, then we will
 * refresh (once) the window which was active when this function was called.
 *
 * Note that "back-quote" is automatically converted into "escape" for
 * convenience on machines with no "escape" key.  This is done after the
 * macro matching, so the user can still make a macro for "backquote".
 *
 * Note the special handling of "ascii 30" (ctrl-caret, aka ctrl-shift-six)
 * and "ascii 31" (ctrl-underscore, aka ctrl-shift-minus), which are used to
 * provide support for simple keyboard "macros".  These keys are so strange
 * that their loss as normal keys will probably be noticed by nobody.  The
 * "ascii 30" key is used to indicate the "end" of a macro action, which
 * allows recursive macros to be avoided.  The "ascii 31" key is used by
 * some of the "main-xxx.c" files to introduce macro trigger sequences.
 *
 * Hack -- we use "ascii 29" (ctrl-right-bracket) as a special "magic" key,
 * which can be used to give a variety of "sub-commands" which can be used
 * any time.  These sub-commands could include commands to take a picture of
 * the current screen, to start/stop recording a macro action, etc.
 *
 * If "term_screen" is not active, we will make it active during this
 * function, so that the various "main-xxx.c" files can assume that input
 * is only requested (via "Term_inkey()") when "term_screen" is active.
 *
 * Mega-Hack -- This function is used as the entry point for clearing the
 * "signal_count" variable, and of the "character_saved" variable.
 *
 * Hack -- Note the use of "inkey_next" to allow "keymaps" to be processed.
 *
 */
char inkey(void)
{
    bool cursor_state;
    bool text_cursor_requested = inkey_text_cursor_requested;
    bool suppress_special_cursor = character_icky > 0
        && !text_cursor_requested;

    char kk;

    char ch = 0;

    bool done = false;

    term* old = Term;

    /* Hack -- Use the "inkey_next" pointer */
    if (inkey_next && *inkey_next && !inkey_xtra)
    {
        /* Get next character, and advance */
        ch = *inkey_next++;

        /* Cancel the various "global parameters" */
        inkey_base = inkey_xtra = inkey_flag = inkey_scan = false;
        inkey_text_cursor_requested = false;
        ui_key_wait_dismiss_clear();

        /* Accept result */
        return (ch);
    }

    /* Forget pointer */
    inkey_next = NULL;

    /* Hack -- handle delayed "flush()" */
    if (inkey_xtra)
    {
        /* End "macro action" */
        parse_macro = false;

        /* End "macro trigger" */
        parse_under = false;

        /* Forget old keypresses */
        Term_flush();
    }

    /* Get the cursor state */
    (void)Term_get_cursor(&cursor_state);

    if (suppress_special_cursor)
        (void)Term_set_cursor(false);

    /* Show the cursor for explicit text entry, or for normal gameplay input.
     * Special screens use their rendered row highlight instead. */
    if (!inkey_scan
        && (text_cursor_requested
            || (!suppress_special_cursor && !hide_cursor
                && (!inkey_flag
                    || (hilite_player && panel_contains(p_ptr->py, p_ptr->px))
                    || (hilite_target && target_sighted()
                        && panel_contains(
                            p_ptr->target_row, p_ptr->target_col))))))
    {
        /* Show the cursor */
        (void)Term_set_cursor(true);
    }

    /* Hack -- Activate main screen */
    Term_activate(term_screen);

    /* (banner redraw countdown moved to per-turn logic in dungeon.c) */

    /* Get a key */
    while (!ch)
    {
        /* Hack -- Handle "inkey_scan" */
        if (!inkey_base && inkey_scan && (0 != Term_inkey(&kk, false, false)))
        {
            break;
        }

        /* Hack -- Flush output once when no key ready */
        if (!done && (0 != Term_inkey(&kk, false, false)))
        {
            /* Hack -- activate proper term */
            Term_activate(old);

            /* Flush output */
            Term_fresh();

            /* Hack -- activate main screen */
            Term_activate(term_screen);

            /* Mega-Hack -- reset saved flag */
            character_saved = false;

            /* Mega-Hack -- reset signal counter */
            signal_count = 0;

            /* Only once */
            done = true;
        }

        /* Hack -- Handle "inkey_base" */
        if (inkey_base)
        {
            int w = 0;

            /* Wait forever */
            if (!inkey_scan)
            {
                /* Wait for (and remove) a pending key */
                if (0 == Term_inkey(&ch, true, true))
                {
                    /* Done */
                    break;
                }

                /* Oops */
                break;
            }

            /* Wait */
            while (true)
            {
                /* Check for (and remove) a pending key */
                if (0 == Term_inkey(&ch, false, true))
                {
                    /* Done */
                    break;
                }

                /* No key ready */
                else
                {
                    /* Increase "wait" */
                    w += 10;

                    /* Excessive delay */
                    if (w >= 100)
                        break;

                    /* Delay */
                    Term_xtra(TERM_XTRA_DELAY, w);
                }
            }

            /* Done */
            break;
        }

        /* Get a key (see above) */
        ch = inkey_aux();

        /* Handle "control-right-bracket" */
        if (ch == 29)
        {
            /* Strip this key */
            ch = 0;

            /* Continue */
            continue;
        }

        /* Treat back-quote as escape */
        if (ch == '`')
            ch = ESCAPE;

        /* End "macro trigger" */
        if (parse_under && (ch <= 32))
        {
            /* Strip this key */
            ch = 0;

            /* End "macro trigger" */
            parse_under = false;
        }

        /* Handle "control-caret" */
        if (ch == 30)
        {
            /* Strip this key */
            ch = 0;
        }

        /* Handle "control-underscore" */
        else if (ch == 31)
        {
            /* Strip this key */
            ch = 0;

            /* Begin "macro trigger" */
            parse_under = true;
        }

        /* Inside "macro trigger" */
        else if (parse_under)
        {
            /* Strip this key */
            ch = 0;
        }
    }

    /* Hack -- restore the term */
    Term_activate(old);

    /* Do not resurrect a stale list cursor after special-screen input. */
    Term_set_cursor(suppress_special_cursor ? false : cursor_state);

    /* Cancel the various "global parameters" */
    inkey_base = inkey_xtra = inkey_flag = inkey_scan = false;
    inkey_text_cursor_requested = false;
    ui_key_wait_dismiss_clear();

    /* (no banner countdown updates here; handled per turn) */

    /* Return the keypress */
    return (ch);
}

char inkey_movement_context(u16b context)
{
    u16b previous_context = movement_input_active_context();
    char ch;

    movement_input_set_active_context(context);
    ch = inkey();
    movement_input_set_active_context(previous_context);

    return ch;
}

#ifdef ALLOW_REPEAT

#define REPEAT_MAX 20

/* Number of chars saved */
static int repeat__cnt = 0;

/* Current index */
static int repeat__idx = 0;

/* Saved "stuff" */
static int repeat__key[REPEAT_MAX];

/*
 * Push data.
 */
void repeat_push(int what)
{
    /* Too many keys */
    if (repeat__cnt == REPEAT_MAX)
        return;

    /* Push the "stuff" */
    repeat__key[repeat__cnt++] = what;

    /* Prevents us from pulling keys */
    ++repeat__idx;
}

/*
 * Pull data.
 */
bool repeat_pull(int* what)
{
    /* All out of keys */
    if (repeat__idx == repeat__cnt)
        return (false);

    /* Grab the next key, advance */
    *what = repeat__key[repeat__idx++];

    /* Success */
    return (true);
}

void repeat_clear(void)
{
    /* Start over from the failed pull */
    if (repeat__idx)
        repeat__cnt = --repeat__idx;
    /* Paranoia */
    else
        repeat__cnt = repeat__idx;

    return;
}

/*
 * Repeat previous command, or begin memorizing new command.
 */
void repeat_check(void)
{
    int what;

    /* Ignore some commands */
    if (p_ptr->command_cmd == ESCAPE)
        return;
    if (p_ptr->command_cmd == ' ')
        return;
    if (p_ptr->command_cmd == '\n')
        return;
    if (p_ptr->command_cmd == '\r')
        return;

    /* Repeat Last Command */
    if (p_ptr->command_cmd == 'n')
    {
        /* Reset */
        repeat__idx = 0;

        /* Get the command */
        if (repeat_pull(&what))
        {
            /* Save the command */
            p_ptr->command_cmd = what;
        }
    }

    /* Start saving new command */
    else
    {
        /* Reset */
        repeat__cnt = 0;
        repeat__idx = 0;

        /* Get the current command */
        what = p_ptr->command_cmd;

        /* Save this command */
        repeat_push(what);
    }
}

#endif /* ALLOW_REPEAT */
