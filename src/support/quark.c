#include "angband.h"
#include "support/quark.h"

/*
 * The "quark" package
 *
 * This package is used to reduce the memory usage of object inscriptions.
 *
 * We use dynamic string allocation because otherwise it is necessary to
 * pre-guess the amount of quark activity.  We limit the total number of
 * quarks, but this is much easier to "expand" as needed.  XXX XXX XXX
 *
 * Two objects with the same inscription will have the same "quark" index.
 *
 * Some code uses "zero" to indicate the non-existance of a quark.
 *
 * Note that "quark zero" is NULL and should never be "dereferenced".
 *
 * ToDo: Add reference counting for quarks, so that unused quarks can
 * be overwritten.
 *
 * ToDo: Automatically resize the array if necessary.
 */

/*
 * The number of quarks (first quark is NULL)
 */
static s16b quark__num = 1;

/*
 * The array[QUARK_MAX] of pointers to the quarks
 */
static cptr* quark__str;

/*
 * Add a new "quark" to the set of quarks.
 */
s16b quark_add(cptr str)
{
    int i;

    /* Look for an existing quark */
    for (i = 1; i < quark__num; i++)
    {
        /* Check for equality */
        if (streq(quark__str[i], str))
            return (i);
    }

    /* Hack -- Require room XXX XXX XXX */
    if (quark__num == QUARK_MAX)
        return (0);

    /* New quark */
    i = quark__num++;

    /* Add a new quark */
    quark__str[i] = str_dup(str);

    /* Return the index */
    return (i);
}

/*
 * This function looks up a quark
 */
cptr quark_str(s16b i)
{
    cptr q;

    /* Verify */
    if ((i < 0) || (i >= quark__num))
        i = 0;

    /* Get the quark */
    q = quark__str[i];

    /* Return the quark */
    return (q);
}

/*
 * Initialize the "quark" package
 */
errr quarks_init(void)
{
    /* Quark variables */
    quark__str = mem_alloc_array(QUARK_MAX, cptr);

    /* Success */
    return (0);
}

/*
 * Free the "quark" package
 */
errr quarks_free(void)
{
    int i;

    /* Free the "quarks" */
    for (i = 1; i < quark__num; i++)
    {
        str_free(quark__str[i]);
    }

    /* Free the list of "quarks" */
    mem_free_null(quark__str);

    /* Success */
    return (0);
}
