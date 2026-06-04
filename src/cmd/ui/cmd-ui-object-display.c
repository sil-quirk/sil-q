#include "angband.h"
#include "externs.h"

void display_koff(int k_idx)
{
    int y;

    object_type* i_ptr;
    object_type object_type_body;

    char o_name[80];

    /* Erase the window */
    for (y = 0; y < Term->hgt; y++)
    {
        /* Erase the line */
        Term_erase(0, y, 255);
    }

    /* No info */
    if (!k_idx)
        return;

    if (p_ptr && p_ptr->image)
    {
        return;
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Prepare the object */
    object_wipe(i_ptr);

    /* Prepare the object */
    object_prep(i_ptr, k_idx);

    /* Describe */
    object_desc_spoil(o_name, sizeof(o_name), i_ptr, false, 0);

    /* Mention the object name */
    Term_putstr(0, 0, -1, TERM_WHITE, o_name);
}
