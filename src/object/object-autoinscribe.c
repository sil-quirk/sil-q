/* File: object/object-autoinscribe.c */

/*
 * Autoinscription support: a small per-object-kind database that stamps a
 * stored inscription onto items of that kind as they are seen/identified.
 * Used mainly to let the player label otherwise-unidentified consumables
 * (potions, herbs).
 *
 * This was split out of the old squelch.c when the squelch subsystem was
 * removed; only the autoinscription half survives.
 */

#include "angband.h"
#include "externs.h"

static cptr get_autoinscription(s16b kindIdx)
{
    int i;

    for (i = 0; i < inscriptionsCount; i++)
    {
        if (kindIdx == inscriptions[i].kindIdx)
        {
            return quark_str(inscriptions[i].inscriptionIdx);
        }
    }

    return 0;
}

extern int do_cmd_autoinscribe_item(s16b k_idx)
{
    char tmp[80] = "";
    cptr curInscription = get_autoinscription(k_idx);

    if (curInscription)
    {
        SDL_strlcpy(tmp, curInscription, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = 0;
    }

    /* Get a new inscription (possibly empty) via the modal input panel */
    if (get_string_panel("Autoinscription:", tmp, sizeof(tmp)))
    {
        /* Save the inscription */
        add_autoinscription(k_idx, tmp);

        /* Inscribe stuff */
        p_ptr->notice |= (PN_AUTOINSCRIBE);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);

        return 1;
    }

    return 0;
}

int get_autoinscription_index(s16b k_idx)
{
    int i;

    for (i = 0; i < inscriptionsCount; i++)
    {
        if (k_idx == inscriptions[i].kindIdx)
        {
            return i;
        }
    }

    return -1;
}

/*Put the autoinscription on an object*/
int apply_autoinscription(object_type* o_ptr)
{
    cptr note = get_autoinscription(o_ptr->k_idx);
    cptr existingInscription = quark_str(o_ptr->obj_note);

    /* Don't inscribe objects if there is no autoinscription to do! */
    if (!note)
    {
        return (0);
    }

    /* Don't re-inscribe if it's already correctly inscribed */
    if (existingInscription && streq(note, existingInscription))
    {
        return (0);
    }

    o_ptr->obj_note = note[0] == 0 ? 0 : quark_add(note);

    return (1);
}

int remove_autoinscription(s16b kind)
{
    int i = get_autoinscription_index(kind);

    /* It's not here, */
    if (i == -1)
        return 0;

    while (i < inscriptionsCount - 1)
    {
        inscriptions[i] = inscriptions[i + 1];
        i++;
    }

    inscriptionsCount--;

    return 1;
}

/*
 *  Uninscribes an object if its inscription matches the given autoinscription
 */
void unapply_autoinscription(object_type* o_ptr, cptr note)
{
    cptr existingInscription = quark_str(o_ptr->obj_note);

    /* Remove the inscription if it matches the autoinscription */
    if (existingInscription && streq(note, existingInscription))
    {
        /* Remove the inscription */
        o_ptr->obj_note = 0;
    }

    return;
}

/*
 *  Removes an autoinscription from the database and from all objects of that
 * kind
 */
extern void obliterate_autoinscription(s16b kind)
{
    int i;
    int j = get_autoinscription_index(kind);
    cptr note = get_autoinscription(kind);
    object_type* o_ptr;

    /* Abort if there is no autoinscription for that object kind */
    if (j == -1)
        return;

    // Go through all objects in the dungeon and inventory...
    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        // Don't remove inscriptions from different object kinds.
        if (o_ptr->k_idx != kind)
            continue;

        /* Apply an autoinscription */
        unapply_autoinscription(o_ptr, note);
    }
    for (i = INVEN_PACK; i > 0; i--)
    {
        // Don't remove inscriptions from different object kinds.
        if (inventory[i].k_idx != kind)
            continue;

        unapply_autoinscription(&inventory[i], note);
    }

    remove_autoinscription(kind);

    return;
}

void autoinscribe_dungeon(void)
{
    int i;
    object_type* o_ptr;

    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Apply an autoinscription */
        apply_autoinscription(o_ptr);
    }
}

void autoinscribe_ground(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    s16b this_o_idx, next_o_idx = 0;

    /* Scan the pile of objects */
    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        /* Get the next object */
        next_o_idx = o_list[this_o_idx].next_o_idx;

        /* Apply an autoinscription */
        apply_autoinscription(&o_list[this_o_idx]);
    }
}

void autoinscribe_pack(void)
{
    int i;

    for (i = INVEN_PACK; i > 0; i--)
    {
        /* Skip empty items */
        if (!inventory[i].k_idx)
            continue;

        apply_autoinscription(&inventory[i]);
    }
}

int add_autoinscription(s16b kind, cptr inscription)
{
    int index;

    if (kind == 0)
    {
        /* paranoia */
        return 0;
    }

    if (!inscription || inscription[0] == 0)
    {
        return remove_autoinscription(kind);
    }

    index = get_autoinscription_index(kind);

    if (index == -1)
    {
        index = inscriptionsCount;
    }

    if (index >= AUTOINSCRIPTIONS_MAX)
    {
        msg_format("This inscription (%s) cannot be added, "
                   "because the inscription array is full!",
            inscription);
        return 0;
    }

    inscriptions[index].kindIdx = kind;
    inscriptions[index].inscriptionIdx = quark_add(inscription);

    if (index == inscriptionsCount)
    {
        /* Only increment count if inscription added to end of array */
        inscriptionsCount++;
    }

    // add inscriptions to pack and dungeon
    autoinscribe_pack();
    autoinscribe_dungeon();

    return 1;
}
