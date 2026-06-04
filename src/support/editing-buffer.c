#include "angband.h"
#include "support/editing-buffer.h"
#include "externs.h"

/*
 * Initialize a editing_buffer structure. It takes a pointer to a valid
 * structure, an optional string used to initialize the contents of the
 * buffer and a maximum buffer size (it must include an extra space for an
 * ending '\0').
 */
void editing_buffer_init(
    editing_buffer* eb_ptr, const char* buf, size_t max_size)
{
    size_t len = 0;

    if (!eb_ptr)
        return;

    if (buf)
        len = strlen(buf);

    /* Alloc a clean buffer */
    eb_ptr->buf = mem_alloc_array(max_size, char);

    /* Copy the initial string, if any */
    if (len > 0)
        SDL_strlcpy(eb_ptr->buf, buf, sizeof(eb_ptr->buf));

    /* Initialize the remaining fields */
    eb_ptr->pos = len;

    /* Important, we keep one space unused to ensure the correctness of the
     * "print" function */
    eb_ptr->max_size = max_size - 1;
    eb_ptr->gap_size = eb_ptr->max_size - len;
}

/*
 * Free the resources used by the editing_buffer structure.
 */
void editing_buffer_destroy(editing_buffer* eb_ptr)
{
    /* Destroy the buffer */
    if (eb_ptr && eb_ptr->buf)
    {
        mem_free_null(eb_ptr->buf);
        eb_ptr->buf = NULL;
    }
}

/*
 * Puts a character on the buffer. Returns a non-zero value if it succeds.
 */
int editing_buffer_put_chr(editing_buffer* eb_ptr, char ch)
{
    if (!eb_ptr)
        return 0;

    /* Do not have space */
    if (eb_ptr->gap_size < 1)
        return 0;

    /* Copy the character. Advance the "cursor" */
    eb_ptr->buf[eb_ptr->pos++] = ch;

    /* We have less space */
    --eb_ptr->gap_size;

    return 1;
}
/*
 * Changes the position of the "cursor" in the buffer.
 * Valid values for "new_pos" are from 0 to EDITING_BUFFER_LEN(eb_ptr).
 * BEWARE: the type of "new_pos" is "size_t" (unsigned).
 * Returns a non-zero value if it succeds.
 */
int editing_buffer_set_position(editing_buffer* eb_ptr, size_t new_pos)
{
    if (!eb_ptr)
        return 0;

    /* Valid position? */
    if (new_pos > EDITING_BUFFER_LEN(eb_ptr))
        return 0;

    /* Trivial */
    if (new_pos == eb_ptr->pos)
        return 1;

    /* Easy case, we change only the "cursor" */
    if (eb_ptr->gap_size < 1)
    {
        eb_ptr->pos = new_pos;
        return 1;
    }

    /* Move the gap. Note that if "new_pos" defers of "pos" by +-1, only
     * one character is moved (fast keyboard arrows) */

    /* First case. "new_pos" is after the gap */
    while (eb_ptr->pos < new_pos)
    {
        eb_ptr->buf[eb_ptr->pos] = eb_ptr->buf[eb_ptr->pos + eb_ptr->gap_size];

        /* Important, keep the gap clean */
        eb_ptr->buf[eb_ptr->pos + eb_ptr->gap_size] = '\0';
        ++eb_ptr->pos;
    }

    /* Second case. "new_pos" is before the gap */
    while (eb_ptr->pos > new_pos)
    {
        --eb_ptr->pos;
        eb_ptr->buf[eb_ptr->pos + eb_ptr->gap_size] = eb_ptr->buf[eb_ptr->pos];

        /* Important, keep the gap clean */
        eb_ptr->buf[eb_ptr->pos] = '\0';
    }

    return 1;
}

/*
 * Hack - Efficient printing function.
 */
void editing_buffer_display(editing_buffer* eb_ptr, int x, int y, byte col)
{
    if (!eb_ptr)
        return;

    Term_erase(x, y, (int)eb_ptr->max_size);

    /* Print the beginning of the buffer */
    /* In many cases, it is all we have to do */

    /* Here is the reason why the gap should be "clean" */
    /* It ensures an ending '\0' */
    Term_putstr(x, y, -1, col, eb_ptr->buf);

    /* Unless this happens */

    /* Here is the reason why we reserved one space in editing_buffer_init */
    /* Again, it ensures an ending '\0' */
    if ((eb_ptr->pos < EDITING_BUFFER_LEN(eb_ptr)) && (eb_ptr->gap_size > 0))
        Term_putstr(x + eb_ptr->pos, y, -1, col,
            eb_ptr->buf + eb_ptr->pos + eb_ptr->gap_size);
}

/*
 * Deletes the character under the "cursor". Returns 1 if it succeds.
 */
int editing_buffer_delete(editing_buffer* eb_ptr)
{
    if (!eb_ptr)
        return 0;

    /* We are at the end of the buffer */
    if (eb_ptr->pos == EDITING_BUFFER_LEN(eb_ptr))
        return 0;

    /* Important, keep the gap clean */
    eb_ptr->buf[eb_ptr->pos + eb_ptr->gap_size] = '\0';

    /* We have more space */
    ++eb_ptr->gap_size;

    return 1;
}

/*
 * Removes all the contents of the buffer.
 */
void editing_buffer_clear(editing_buffer* eb_ptr)
{
    if (!eb_ptr)
        return;

    /* Clear the buffer */
    memset(eb_ptr->buf, 0, sizeof(char) * eb_ptr->max_size);

    /* Reinitialize the remaining fields but "max_size" */
    eb_ptr->pos = 0;
    eb_ptr->gap_size = eb_ptr->max_size;
}

/*
 * Obtains a copy of the contents of the buffer.
 */
void editing_buffer_get_all(editing_buffer* eb_ptr, char buf[], size_t max_size)
{
    size_t i, n = EDITING_BUFFER_LEN(eb_ptr);
    if (!eb_ptr)
        return;

    /* Note the use of EDITING_BUFFER_GET to ignore the gap */
    for (i = 0; (i < n) && (i < max_size - 1); i++)
    {
        buf[i] = EDITING_BUFFER_GET(eb_ptr, i);
    }

    /* Terminate the string */
    buf[i] = '\0';
}

/*
 * Inserts a string in the buffer. Returns the number of written characters.
 * "n" is the maximum number of characters to write, -1 means all the string.
 */
int editing_buffer_put_str(editing_buffer* eb_ptr, const char* str, int n)
{
    const char* p_str;

    if (!eb_ptr || !str)
        return 0;

    for (p_str = str; *p_str; p_str++)
    {
        /* We do not have space */
        if (eb_ptr->gap_size < 1)
            break;

        /* Check max input size */
        if ((n >= 0) && (p_str - str >= n))
            break;

        /* Insert the character. Advance the cursor */
        eb_ptr->buf[eb_ptr->pos++] = *p_str;

        /* We have less space */
        --eb_ptr->gap_size;
    }

    return (p_str - str);
}
