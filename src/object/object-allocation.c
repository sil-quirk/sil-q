/* File: object/object-allocation.c */

#include "angband.h"
#include "externs.h"
#include "object/object-allocation.h"
#include "object/object-internal.h"


void get_obj_num_prep(void)
{
    int i;

    /* Get the entry */
    alloc_entry* table = alloc_kind_table;

    /* Scan the allocation table */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Accept objects which pass the restriction, if any */
        if (!get_obj_num_hook)
        {
            // damaged items only on skeletons
            if (kind_is_damaged_item(table[i].index))
                table[i].prob2 = 0;
            else
                table[i].prob2 = table[i].prob1;
        }
        else if ((*get_obj_num_hook)(table[i].index))
        {
            /* Accept this object */
            table[i].prob2 = table[i].prob1;
        }
        /* Do not use this object */
        else
        {
            /* Decline this object */
            table[i].prob2 = 0;
        }
    }
}

/*
 * Choose an object kind that seems "appropriate" to the given level
 *
 * This function uses the "prob2" field of the "object allocation table",
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an "appropriate" object, in
 * a relatively efficient manner.
 *
 * It is (slightly) more likely to acquire an object of the given level
 * than one of a lower level.  This is done by choosing several objects
 * appropriate to the given level and keeping the "hardest" one.
 *
 * Note that if no objects are "appropriate", then this function will
 * fail, and return zero, but this should *almost* never happen.
 * (but it does happen with certain themed items occasionally). -JG
 */
s16b get_obj_num(int level)
{
    int i, j, p;

    int k_idx;

    long value, total;

    object_kind* k_ptr;

    alloc_entry* table = alloc_kind_table;

    /* Boost level */
    if (level > 0)
    {
        /* Occasional "boost" */
        if (one_in_(GREAT_OBJ))
        {
            // most of the time, choose a new deeper depth, weighted towards the
            // current depth
            if (level < MORGOTH_DEPTH)
            {
                int x = rand_range(level + 1, MORGOTH_DEPTH);
                int y = rand_range(level + 1, MORGOTH_DEPTH);

                level = MIN(x, y);
            }

            // but if it was already very deep, just increment it
            else
            {
                level++;
            }
        }
    }

    /* Reset total */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Objects are sorted by depth */
        if (table[i].level > level)
            break;

        /* Default */
        table[i].prob3 = 0;

        /* Get the index */
        k_idx = table[i].index;

        /* Get the actual kind */
        k_ptr = &k_info[k_idx];

        /* Hack -- prevent embedded chests*/
        if ((object_generation_mode == OB_GEN_MODE_CHEST)
            && (k_ptr->tval == TV_CHEST))
            continue;

        /* Accept */
        table[i].prob3 = table[i].prob2;

        /* Total */
        total += table[i].prob3;
    }

    /* No legal objects */
    if (total <= 0)
        return (0);

    /* Pick an object */
    value = rand_int(total);

    /* Find the object */
    for (i = 0; i < alloc_kind_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* Power boost */
    p = rand_int(100);

    /* Try for a "better" object once (50%) or twice (10%) */
    if (p < 60)
    {
        /* Save old */
        j = i;

        /* Pick a object */
        value = rand_int(total);

        /* Find the monster */
        for (i = 0; i < alloc_kind_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3)
                break;

            /* Decrement */
            value = value - table[i].prob3;
        }

        /* Keep the "best" one */
        if (table[i].level < table[j].level)
            i = j;
    }

    /* Try for a "better" object twice (10%) */
    if (p < 10)
    {
        /* Save old */
        j = i;

        /* Pick a object */
        value = rand_int(total);

        /* Find the object */
        for (i = 0; i < alloc_kind_size; i++)
        {
            /* Found the entry */
            if (value < table[i].prob3)
                break;

            /* Decrement */
            value = value - table[i].prob3;
        }

        /* Keep the "best" one */
        if (table[i].level < table[j].level)
            i = j;
    }

    /* Result */
    return (table[i].index);
}

/*
 * Known is true when the "attributes" of an object are "known".
 *
 * These attributes include tohit, todam, toac, cost, and pval (charges).
 *
 * Note that "knowing" an object gives you everything that an "awareness"
 * gives you, and much more.  In fact, the player is always "aware" of any
 * item which he "knows", except items in stores.
 *
 * But having full knowledge of, say, one "staff of Sanctity", does not, by
 * itself, give you knowledge, or even awareness, of other "staffs of Sanctity".
 * It happens that most "identify" routines (including "buying from a shop")
 * will make the player "aware" of the object as well as "know" it.
 *
 * This routine also removes any inscriptions generated by "feelings".
 */
