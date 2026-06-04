/* File: dungeon/dungeon-identify.c */

#include "angband.h"
#include "dungeon-internal.h"

/*
 * Return a "feeling" (or NULL) about an item.  Method 1 (Weak).
 * Sil - this method can't distinguish artefacts from ego items
 */
int value_check_aux1(const object_type* o_ptr)
{
    /* Artefacts */
    if (artefact_p(o_ptr))
    {
        /* Normal */
        return (INSCRIP_EXCELLENT);
    }

    /* Ego-Items */
    if (ego_item_p(o_ptr))
    {
        /* Normal */
        return (INSCRIP_EXCELLENT);
    }

    /* Default to "average" */
    return (INSCRIP_AVERAGE);
}

/*
 * Returns true if this object can be pseudo-ided.
 */
bool can_be_pseudo_ided(const object_type* o_ptr)
{
    /* Valid "tval" codes */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
        break;
    }
    case TV_LIGHT:
    {
        if (o_ptr->sval == SV_LIGHT_LANTERN)
            return (true);
        if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            return (true);
        if (o_ptr->sval == SV_LIGHT_FEANORIAN)
            return (true);
        break;
    }
    }
    return (false);
}

/*
 * Pseudo-id an item
 */
void pseudo_id(object_type* o_ptr)
{
    int feel;

    char o_name[80];

    /* Skip non-sense machines */
    if (!can_be_pseudo_ided(o_ptr))
        return;

    /* It is known, no information needed */
    if (object_known_p(o_ptr))
        return;

    feel = value_check_aux1(o_ptr);

    /* Skip non-feelings */
    if (!feel)
        return;

    /* Get an object description */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Sense the object */
    o_ptr->discount = feel;

    /* The object has been "sensed" */
    o_ptr->ident |= (IDENT_SENSE);
}

void pseudo_id_everything(void)
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

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Pseudo-id it */
        pseudo_id(o_ptr);
    }
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Pseudo-id it */
        pseudo_id(o_ptr);
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    handle_stuff();
}

void id_everything(void)
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

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        ident(o_ptr);
    }
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        ident(o_ptr);
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    handle_stuff();
}

/*
 * automatically identify items of {special} types that the player knows about
 */
void id_known_specials(void)
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

        /* Automatically identify any special items you have seen before */
        if (object_has_ego(o_ptr) && !object_known_p(o_ptr))
        {
            bool all_aware = true;
            byte ego_pfx = object_ego_prefix(o_ptr);
            byte ego_sfx = object_ego_suffix(o_ptr);

            if (ego_pfx && !e_info[ego_pfx].aware)
                all_aware = false;
            if (ego_sfx && !e_info[ego_sfx].aware)
                all_aware = false;

            if (!object_uses_smithing_difficulty(o_ptr))
            {
                if (all_aware)
                    ident(o_ptr);
            }
        }
    }
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Automatically identify any special items you have seen before */
        if (object_has_ego(o_ptr) && !object_known_p(o_ptr))
        {
            bool all_aware = true;
            byte ego_pfx = object_ego_prefix(o_ptr);
            byte ego_sfx = object_ego_suffix(o_ptr);

            if (ego_pfx && !e_info[ego_pfx].aware)
                all_aware = false;
            if (ego_sfx && !e_info[ego_sfx].aware)
                all_aware = false;

            if (!object_uses_smithing_difficulty(o_ptr))
            {
                if (all_aware)
                    ident(o_ptr);
            }
        }
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    handle_stuff();
}
