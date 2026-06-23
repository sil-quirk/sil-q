#include "angband.h"
#include "support/message.h"
#include "externs.h"

/*
 * The "message memorization" package.
 *
 * Each call to "message_add(s)" will add a new "most recent" message
 * to the "message recall list", using the contents of the string "s".
 *
 * The number of memorized messages is available as "message_num()".
 *
 * Old messages can be retrieved by "message_str(age)", where the "age"
 * of the most recently memorized message is zero, and the oldest "age"
 * which is available is "message_num() - 1".  Messages outside this
 * range are returned as the empty string.
 *
 * The messages are stored in a special manner that maximizes "efficiency",
 * that is, we attempt to maximize the number of semi-sequential messages
 * that can be retrieved, given a limited amount of storage space, without
 * causing the memorization of new messages or the recall of old messages
 * to be too expensive.
 *
 * We keep a buffer of chars to hold the "text" of the messages, more or
 * less in the order they were memorized, and an array of offsets into that
 * buffer, representing the actual messages, but we allow the "text" to be
 * "shared" by two messages with "similar" ages, as long as we never cause
 * sharing to reach too far back in the the buffer.
 *
 * The implementation is complicated by the fact that both the array of
 * offsets, and the buffer itself, are both treated as "circular arrays"
 * for efficiency purposes, but the strings may not be "broken" across
 * the ends of the array.
 *
 * When we want to memorize a new message, we attempt to "reuse" the buffer
 * space by checking for message duplication within the recent messages.
 *
 * Otherwise, if we need more buffer space, we grab a full quarter of the
 * total buffer space at a time, to keep the reclamation code efficient.
 *
 * The "message_add()" function is rather "complex", because it had to be
 * extremely efficient, both in space and time, for use with the Angband borg.
 */

/*
 * The next "free" index to use
 */
static u16b message__next;

/*
 * The index of the oldest message (none yet)
 */
static u16b message__last;

/*
 * The next "free" offset
 */
static u16b message__head;

/*
 * The offset to the oldest used char (none yet)
 */
static u16b message__tail;

/*
 * The array[MESSAGE_MAX] of offsets, by index
 */
static u16b* message__ptr;

/*
 * The array[MESSAGE_BUF] of chars, by offset
 */
static char* message__buf;

/*
 * The array[MESSAGE_MAX] of u16b for the types of messages
 */
static u16b* message__type;

/*
 * The array[MESSAGE_MAX] of u16b for the count of messages
 */
static u16b* message__count;

/*
 * The array[MESSAGE_MAX] of sequence ids for unified log ordering
 */
static u32b* message__sequence;

/*
 * Shared sequence id for unified message/combat log ordering
 */
static u32b log_history__next_sequence = 1;

/*
 * Table of colors associated to message-types
 */
static byte message__color[MSG_MAX];

/*
 * Calculate the index of a message
 */
static s16b message_age2idx(int age)
{
    return ((message__next + MESSAGE_MAX - (age + 1)) % MESSAGE_MAX);
}

/*
 * How many messages are "available"?
 */
s16b message_num(void)
{
    /* Determine how many messages are "available" */
    return (message_age2idx(message__last - 1));
}

/*
 * Recall the "text" of a saved message
 */
cptr message_str(s16b age)
{
    static char buf[1024];
    s16b x;
    u16b o;
    cptr s;

    /* Forgotten messages have no text */
    if ((age < 0) || (age >= message_num()))
        return ("");

    /* Get the "logical" index */
    x = message_age2idx(age);

    /* Get the "offset" for the message */
    o = message__ptr[x];

    /* Get the message text */
    s = &message__buf[o];

    /* HACK - Handle repeated messages */
    if (message__count[x] > 1)
    {
        strnfmt(buf, sizeof(buf), "%s <%dx>", s, message__count[x]);
        s = buf;
    }

    /* Return the message text */
    return (s);
}

/*
 * Recall the "type" of a saved message
 */
u16b message_type(s16b age)
{
    s16b x;

    /* Paranoia */
    if (!message__type)
        return (MSG_GENERIC);

    /* Forgotten messages are generic */
    if ((age < 0) || (age >= message_num()))
        return (MSG_GENERIC);

    /* Get the "logical" index */
    x = message_age2idx(age);

    /* Return the message type */
    return (message__type[x]);
}

/*
 * Recall the "color" of a message type
 */
static byte message_type_color(u16b type)
{
    byte color = message__color[type];

    if (color == TERM_DARK)
        color = TERM_WHITE;

    return (color);
}

/*
 * Recall the "color" of a saved message
 */
byte message_color(s16b age) { return message_type_color(message_type(age)); }

void log_history_note_sequence(u32b sequence)
{
    if (sequence == 0)
        return;

    if (sequence >= log_history__next_sequence)
    {
        log_history__next_sequence = sequence + 1;
        if (log_history__next_sequence == 0)
            log_history__next_sequence = 1;
    }
}

/*
 * Recall the unified log sequence of a saved message
 */
u32b message_sequence(s16b age)
{
    s16b x;

    /* Paranoia */
    if (!message__sequence)
        return (0);

    /* Forgotten messages have no sequence */
    if ((age < 0) || (age >= message_num()))
        return (0);

    /* Get the "logical" index */
    x = message_age2idx(age);

    /* Return the sequence */
    return (message__sequence[x]);
}

/*
 * Allocate the next shared sequence id for unified log ordering
 */
u32b log_history_next_sequence(void)
{
    u32b sequence = log_history__next_sequence++;

    if (log_history__next_sequence == 0)
        log_history__next_sequence = 1;

    return (sequence);
}

void message_set_latest_sequence(u32b sequence)
{
    s16b x;

    if (!message__sequence || sequence == 0 || message_num() <= 0)
        return;

    x = message_age2idx(0);
    message__sequence[x] = sequence;
    log_history_note_sequence(sequence);
}

errr message_color_define(u16b type, byte color)
{
    /* Ignore illegal types */
    if (type >= MSG_MAX)
        return (1);

    /* Store the color */
    message__color[type] = color;

    /* Success */
    return (0);
}

/*
 * Add a new message, with great efficiency
 *
 * We must ignore long messages to prevent internal overflow, since we
 * assume that we can always get enough space by advancing "message__tail"
 * by one quarter the total buffer space.
 *
 * We must not attempt to optimize using a message index or buffer space
 * which is "far away" from the most recent entries, or we will lose a lot
 * of messages when we "expire" the old message index and/or buffer space.
 */
void message_add(cptr str, u16b type)
{
    int k, i, x, o;
    size_t n;

    cptr s;

    cptr u;
    char* v;

    /*** Step 1 -- Analyze the message ***/

    /* Hack -- Ignore "non-messages" */
    if (!str)
        return;

    /* Message length */
    n = strlen(str);

    /* Hack -- Ignore "long" messages */
    if (n >= MESSAGE_BUF / 4)
        return;

    /*** Step 2 -- Attempt to optimize ***/

    /* Get the "logical" last index */
    x = message_age2idx(0);

    /* Get the "offset" for the last message */
    o = message__ptr[x];

    /* Get the message text */
    s = &message__buf[o];

    /* Last message repeated? */
    if (streq(str, s))
    {
        /* Increase the message count */
        message__count[x]++;

        /* Treat repeated messages as a new occurrence in the unified log */
        message__sequence[x] = log_history_next_sequence();

        /* Success */
        return;
    }

    /*** Step 3 -- Attempt to optimize ***/

    /* Limit number of messages to check */
    k = message_num() / 4;

    /* Limit number of messages to check */
    if (k > 32)
        k = 32;

    /* Start just after the most recent message */
    i = message__next;

    /* Check the last few messages for duplication */
    for (; k; k--)
    {
        u16b q;

        cptr old;

        /* Back up, wrap if needed */
        if (i-- == 0)
            i = MESSAGE_MAX - 1;

        /* Stop before oldest message */
        if (i == message__last)
            break;

        /* Index */
        o = message__ptr[i];

        /* Extract "distance" from "head" */
        q = (message__head + MESSAGE_BUF - o) % MESSAGE_BUF;

        /* Do not optimize over large distances */
        if (q >= MESSAGE_BUF / 4)
            continue;

        /* Get the old string */
        old = &message__buf[o];

        /* Continue if not equal */
        if (!streq(str, old))
            continue;

        /* Get the next available message index */
        x = message__next;

        /* Advance 'message__next', wrap if needed */
        if (++message__next == MESSAGE_MAX)
            message__next = 0;

        /* Kill last message if needed */
        if (message__next == message__last)
        {
            /* Advance 'message__last', wrap if needed */
            if (++message__last == MESSAGE_MAX)
                message__last = 0;
        }

        /* Assign the starting address */
        message__ptr[x] = message__ptr[i];

        /* Store the message type */
        message__type[x] = type;

        /* Store the message count */
        message__count[x] = 1;

        /* Store the unified log sequence */
        message__sequence[x] = log_history_next_sequence();

        /* Success */
        return;
    }

    /*** Step 4 -- Ensure space before end of buffer ***/

    /* Kill messages, and wrap, if needed */
    if (message__head + (n + 1) >= MESSAGE_BUF)
    {
        /* Kill all "dead" messages */
        for (i = message__last; true; i++)
        {
            /* Wrap if needed */
            if (i == MESSAGE_MAX)
                i = 0;

            /* Stop before the new message */
            if (i == message__next)
                break;

            /* Get offset */
            o = message__ptr[i];

            /* Kill "dead" messages */
            if (o >= message__head)
            {
                /* Track oldest message */
                message__last = i + 1;
            }
        }

        /* Wrap "tail" if needed */
        if (message__tail >= message__head)
            message__tail = 0;

        /* Start over */
        message__head = 0;
    }

    /*** Step 5 -- Ensure space for actual characters ***/

    /* Kill messages, if needed */
    if (message__head + (n + 1) > message__tail)
    {
        /* Advance to new "tail" location */
        message__tail += (MESSAGE_BUF / 4);

        /* Kill all "dead" messages */
        for (i = message__last; true; i++)
        {
            /* Wrap if needed */
            if (i == MESSAGE_MAX)
                i = 0;

            /* Stop before the new message */
            if (i == message__next)
                break;

            /* Get offset */
            o = message__ptr[i];

            /* Kill "dead" messages */
            if ((o >= message__head) && (o < message__tail))
            {
                /* Track oldest message */
                message__last = i + 1;
            }
        }
    }

    /*** Step 6 -- Grab a new message index ***/

    /* Get the next available message index */
    x = message__next;

    /* Advance 'message__next', wrap if needed */
    if (++message__next == MESSAGE_MAX)
        message__next = 0;

    /* Kill last message if needed */
    if (message__next == message__last)
    {
        /* Advance 'message__last', wrap if needed */
        if (++message__last == MESSAGE_MAX)
            message__last = 0;
    }

    /*** Step 7 -- Insert the message text ***/

    /* Assign the starting address */
    message__ptr[x] = message__head;

    /* Inline 'strcpy(message__buf + message__head, str)' */
    v = message__buf + message__head;
    for (u = str; *u;)
        *v++ = *u++;
    *v = '\0';

    /* Advance the "head" pointer */
    message__head += (n + 1);

    /* Store the message type */
    message__type[x] = type;

    /* Store the message count */
    message__count[x] = 1;

    /* Store the unified log sequence */
    message__sequence[x] = log_history_next_sequence();
}

/*
 * Initialize the "message" package
 */
errr messages_init(void)
{
    /* Message variables */
    message__ptr = mem_alloc_array(MESSAGE_MAX, u16b);
    message__buf = mem_alloc_array(MESSAGE_BUF, char);
    message__type = mem_alloc_array(MESSAGE_MAX, u16b);
    message__count = mem_alloc_array(MESSAGE_MAX, u16b);
    message__sequence = mem_alloc_array(MESSAGE_MAX, u32b);

    /* Init the message colors to white */
    memset(message__color, TERM_WHITE, sizeof(byte) * MSG_MAX);
    message__color[MSG_BELL] = TERM_ORANGE;
    message__color[MSG_HITPOINT_WARN] = TERM_ORANGE;

    /* Reset the message ring */
    message__next = 0;
    message__last = 0;
    message__head = 0;

    /* Hack -- No messages yet */
    message__tail = MESSAGE_BUF;

    /* Success */
    return (0);
}

/*
 * Free the "message" package
 */
void messages_free(void)
{
    /* Free the messages */
    mem_free_null(message__ptr);
    mem_free_null(message__buf);
    mem_free_null(message__type);
    mem_free_null(message__count);
    mem_free_null(message__sequence);
}

bool ui_message_line_enabled(void);

/*
 * Hack -- flush
 */
static void msg_flush(int x)
{
    byte a = TERM_L_BLUE;

    if (!ui_message_line_enabled())
        return;

    /* Pause for response */
    Term_putstr(x, 0, -1, a, "-more-");

    /* Place the cursor on the player or target */
    if (hilite_player)
        move_cursor_relative(p_ptr->py, p_ptr->px);
    if (hilite_target && target_sighted())
        move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

    if (!auto_more)
    {
        /* Get an acceptable keypress */
        while (1)
        {
            char ch;
            ch = inkey();
            if (quick_messages)
                break;
            if ((ch == ESCAPE) || (ch == ' '))
                break;
            if ((ch == '\n') || (ch == '\r'))
                break;
            bell("Illegal response to a 'more' prompt!");
        }
    }

    /* Clear the line */
    Term_erase(0, 0, 255);
}

bool ui_message_line_enabled(void)
{
    return false;
}

static int message_column = 0;

void message_line_reset_column(void)
{
    message_column = 0;
}

/*
 * Output a message to the top line of the screen.
 *
 * Break long messages into multiple pieces (40-72 chars).
 *
 * Allow multiple short messages to "share" the top line.
 *
 * Prompt the user to make sure he has a chance to read them.
 *
 * These messages are memorized for later reference (see above).
 *
 * We could do a "Term_fresh()" to provide "flicker" if needed.
 *
 * The global "msg_flag" variable can be cleared to tell us to "erase" any
 * "pending" messages still on the screen, instead of using "msg_flush()".
 * This should only be done when the user is known to have read the message.
 *
 * We must be very careful about using the "msg_print()" functions without
 * explicitly calling the special "msg_print(NULL)" function, since this may
 * result in the loss of information if the screen is cleared, or if anything
 * is displayed on the top line.
 *
 * Hack -- Note that "msg_print(NULL)" will clear the top line even if no
 * messages are pending.
 */
static void msg_print_aux(u16b type, cptr msg)
{
    int n;
    char* t;
    char buf[1024];
    byte color;
    int w, h;
    int available_width;

    /* Obtain the size */
    (void)Term_get_size(&w, &h);
    available_width = w - 8;
    if (available_width < 1)
        available_width = 1;

    /* Hack -- Reset */
    if (!msg_flag)
        message_column = 0;

    /* Message Length */
    n = (msg ? strlen(msg) : 0);

    /* Hack -- flush when requested or needed */
    if (message_column && (!msg || ((message_column + n) > available_width)))
    {
        /* Flush */
        msg_flush(message_column);

        /* Forget it */
        msg_flag = false;

        /* Reset */
        message_column = 0;

    }

    /* No message */
    if (!msg)
    {
        return;
    }

    /* Paranoia */
    if (n > 1000)
        return;

    /* Memorize the message (if legal) */
    if (character_generated && !p_ptr->is_dead)
        message_add(msg, type);

    /* Window stuff */
    p_ptr->window |= (PW_MESSAGE);

    if (!ui_message_line_enabled())
    {
        msg_flag = false;
        message_column = 0;
        return;
    }

    /* Copy it */
    SDL_strlcpy(buf, msg, sizeof(buf));

    /* Analyze the buffer */
    t = buf;

    /* Get the color of the message */
    color = message_type_color(type);

    /* With auto-more enabled, intermediate chunks are never visible. */
    if (auto_more && (n > available_width))
    {
        t += (n - available_width);
        n = available_width;
    }

    /* Split message */
    while (n > available_width)
    {
        char oops;

        int check, split;

        /* Default split */
        split = available_width;

        /* Find the "best" split point */
        for (check = (w / 2); check < available_width; check++)
        {
            /* Found a valid split point */
            if (t[check] == ' ')
                split = check;
        }

        /* Save the split character */
        oops = t[split];

        /* Split the message */
        t[split] = '\0';

        /* Display part of the message */
        Term_putstr(0, 0, split, color, t);

        /* Flush it */
        msg_flush(split + 1);

        /* Restore the split character */
        t[split] = oops;

        /* Insert a space */
        t[--split] = ' ';

        /* Prepare to recurse on the rest of "buf" */
        t += split;
        n -= split;
    }

    /* Display the tail of the message */
    Term_putstr(message_column, 0, n, color, t);

    /* Remember the message */
    msg_flag = true;

    /* Remember the position */
    message_column += n + 1;

    /* Optional refresh */
    if (fresh_after)
        Term_fresh();
}

/*
 * Print a message in the default color (white)
 */
void msg_print(cptr msg) { msg_print_aux(MSG_GENERIC, msg); }

/*
 * Display a formatted message, using "vstrnfmt()" and "msg_print()".
 */
void msg_format(cptr fmt, ...)
{
    va_list vp;

    char buf[1024];

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args, save the length */
    (void)vstrnfmt(buf, sizeof(buf), fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Display */
    msg_print_aux(MSG_GENERIC, buf);
}

/*
 * Display a message many times, using "vstrnfmt()" and "msg_print()".
 */
void msg_debug(cptr fmt, ...)
{
    va_list vp;

    char buf[1024];
    char buf2[1030]; /* Slightly larger to accommodate "<< >>" wrapper */

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args, save the length */
    (void)vstrnfmt(buf, sizeof(buf), fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    snprintf(buf2, sizeof(buf2), "<< %s >>", buf);

    /* Display */
    msg_print_aux(MSG_GENERIC, buf2);
    message_flush();
}

/*
 * Display a message and play the associated sound.
 *
 * The "extra" parameter is currently unused.
 */
void message(u16b message_type, s16b extra, cptr message)
{
    /* Unused parameter */
    (void)extra;

    sound(message_type);

    msg_print_aux(message_type, message);
}

/*
 * Display a formatted message and play the associated sound.
 *
 * The "extra" parameter is currently unused.
 */
void message_format(u16b message_type, s16b extra, cptr fmt, ...)
{
    va_list vp;

    char buf[1024];

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args, save the length */
    (void)vstrnfmt(buf, sizeof(buf), fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Display */
    message(message_type, extra, buf);
}

/*
 * Print the queued messages.
 */
void message_flush(void)
{
    if (!ui_message_line_enabled())
    {
        msg_flag = false;
        message_column = 0;
        return;
    }

    /* Hack -- Reset */
    if (!msg_flag)
        message_column = 0;

    /* Flush when needed */
    if (message_column)
    {
        /* Print pending messages */
        msg_flush(message_column);

        /* Forget it */
        msg_flag = false;

        /* Reset */
        message_column = 0;
    }
}

void message_discard_pending(void)
{
    msg_flag = false;
    message_column = 0;

    if (!Term)
        return;

    if (!ui_message_line_enabled())
    {
        return;
    }

    Term_erase(0, 0, 255);
}

