/* File: object1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

static void flavor_assign_fixed(void)
{
    int i, j;

    for (i = 0; i < z_info->flavor_max; i++)
    {
        flavor_type* flavor_ptr = &flavor_info[i];

        /* Skip random flavors */
        if (flavor_ptr->sval == SV_UNKNOWN)
            continue;

        for (j = 0; j < z_info->k_max; j++)
        {
            /* Skip other objects */
            if ((k_info[j].tval == flavor_ptr->tval)
                && (k_info[j].sval == flavor_ptr->sval))
            {
                /* Store the flavor index */
                k_info[j].flavor = i;
            }
        }
    }
}

static void flavor_assign_random(byte tval)
{
    int i, j;
    int flavor_count = 0;
    int choice;

    /* Count the random flavors for the given tval */
    for (i = 0; i < z_info->flavor_max; i++)
    {
        if ((flavor_info[i].tval == tval)
            && (flavor_info[i].sval == SV_UNKNOWN))
        {
            flavor_count++;
        }
    }

    for (i = 0; i < z_info->k_max; i++)
    {
        /* Skip other object types */
        if (k_info[i].tval != tval)
            continue;

        /* Skip objects that already are flavored */
        if (k_info[i].flavor != 0)
            continue;

        /* HACK - Ordinary food is "boring" */
        if ((tval == TV_FOOD) && (k_info[i].sval >= SV_FOOD_MIN_FOOD))
            continue;

        if (!flavor_count)
            quit_fmt("Not enough flavors for tval %d.", tval);

        /* Select a flavor */
        choice = rand_int(flavor_count);

        /* Find and store the flavor */
        for (j = 0; j < z_info->flavor_max; j++)
        {
            /* Skip other tvals */
            if (flavor_info[j].tval != tval)
                continue;

            /* Skip assigned svals */
            if (flavor_info[j].sval != SV_UNKNOWN)
                continue;

            if (choice == 0)
            {
                /* Store the flavor index */
                k_info[i].flavor = j;

                /* Mark the flavor as used */
                flavor_info[j].sval = k_info[i].sval;

                /* One less flavor to choose from */
                flavor_count--;

                break;
            }

            choice--;
        }
    }
}

// Is the current day between Easter Sunday?
// if so, herbs become easter eggs
bool easter_time(void)
{
    time_t c; // time variables
    struct tm* tp; //

    c = time((time_t*)0);
    tp = localtime(&c);

    if (TRUE)
    {
        int y = tp->tm_year + 1900;
        int a = y % 19;
        int b = y / 100;
        int c = y % 100;
        int d = b / 4;
        int e = b % 4;
        int f = (b + 8) / 25;
        int g = (b - f + 1) / 3;
        int h = (19 * a + b - d - g + 15) % 30;
        int i = c / 4;
        int k = c % 4;
        int l = (32 + 2 * e + 2 * i - h - k) % 7;
        int m = (a + 11 * h + 22 * l) / 451;
        int month = (h + l - 7 * m + 114) / 31; // counting from 1
        int day = ((h + l - 7 * m + 114) % 31) + 1; // counting from 1

        // we need to add 1 to the month as they count from 0
        if ((tp->tm_mon + 1 == month) && (tp->tm_mday == day))
            return (TRUE);
    }

    return (FALSE);
}

/*
 * Prepare the "variable" part of the "k_info" array.
 *
 * The "color"/"metal"/"type" of an item is its "flavor".
 * For the most part, flavors are assigned randomly each game.
 *
 * Initialize descriptions for the "colored" objects, including:
 * Rings, Amulets, Staffs, Horns, Food, Potions, Scrolls.
 *
 * The first 4 entries for potions are fixed (Miruvor, unused, Orcish Liquor,
 * unused).
 *
 * Hack -- make sure everything stays the same for each saved game
 * This is accomplished by the use of a saved "random seed".
 * Since no other functions are called while the special
 * seed is in effect, so this function is pretty "safe".
 */
void flavor_init(void)
{
    int i;

    /* Hack -- Use the "simple" RNG */
    Rand_quick = TRUE;

    /* Hack -- Induce consistant flavors */
    Rand_value = seed_flavor;

    flavor_assign_fixed();

    flavor_assign_random(TV_RING);
    flavor_assign_random(TV_AMULET);
    flavor_assign_random(TV_STAFF);
    flavor_assign_random(TV_HORN);
    flavor_assign_random(TV_FOOD);
    flavor_assign_random(TV_POTION);

    /* Hack -- Use the "complex" RNG */
    Rand_quick = FALSE;

    /* Analyze every object */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /*Skip "empty" objects*/
        if (!k_ptr->name)
            continue;

        /*No flavor yields aware*/
        if (!k_ptr->flavor)
            k_ptr->aware = TRUE;

        // Easter Eggs
        if (easter_time() && (k_ptr->tval == TV_FOOD) && k_ptr->flavor)
        {
            k_ptr->flavor += 20;
        }
    }
}

#ifdef ALLOW_AUTOMATON_GRAPHICS
extern void init_translate_visuals(void);
#endif /* ALLOW_AUTOMATON_GRAPHICS */

/*
 * Reset the "visual" lists
 *
 * This involves resetting various things to their "default" state.
 *
 * If the "prefs" flag is TRUE, then we will also load the appropriate
 * "user pref file" based on the current setting of the "use_graphics"
 * flag.  This is useful for switching "graphics" on/off.
 *
 * The features, objects, and monsters, should all be encoded in the
 * relevant "font.pref" and/or "graf.prf" files.  XXX XXX XXX
 *
 * The "prefs" parameter is no longer meaningful.  XXX XXX XXX
 */
void reset_visuals(bool unused)
{
    int i;

    /* Unused parameter */
    (void)unused;

    /* Extract default attr/char code for features */
    for (i = 0; i < z_info->f_max; i++)
    {
        feature_type* f_ptr = &f_info[i];

        /* Assume we will use the underlying values */
        f_ptr->x_attr = f_ptr->d_attr;
        f_ptr->x_char = f_ptr->d_char;
    }

    /* Extract default attr/char code for objects */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Default attr/char */
        k_ptr->x_attr = k_ptr->d_attr;
        k_ptr->x_char = k_ptr->d_char;
    }

    /* Extract default attr/char code for monsters */
    for (i = 0; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];

        /* Default attr/char */
        r_ptr->x_attr = r_ptr->d_attr;
        r_ptr->x_char = r_ptr->d_char;
    }

    /* Extract default attr/char code for flavors */
    for (i = 0; i < z_info->flavor_max; i++)
    {
        flavor_type* flavor_ptr = &flavor_info[i];

        /* Default attr/char */
        flavor_ptr->x_attr = flavor_ptr->d_attr;
        flavor_ptr->x_char = flavor_ptr->d_char;
    }

    /* Extract attr/chars for inventory objects (by tval) */
    for (i = 0; i < 128; i++)
    {
        /* Default to 'light dark' */
        tval_to_attr[i] = TERM_L_DARK;
    }

    /* Graphic symbols */
    if (use_graphics)
    {
        /* Process "graf.prf" */
        process_pref_file("graf.prf");
    }

    /* Normal symbols */
    else
    {
        /* Process "font.prf" */
        process_pref_file("font.prf");
    }

#ifdef ALLOW_AUTOMATON_GRAPHICS
    /* Initialize the translation table for the automaton */
    init_translate_visuals();
#endif /* ALLOW_AUTOMATON_GRAPHICS */
}

/*
 * Modes of object_flags_aux()
 */
#define OBJECT_FLAGS_FULL 1 /* Full info */
#define OBJECT_FLAGS_KNOWN 2 /* Only flags known to the player */

/*
 * Obtain the "flags" for an item
 */
static void object_flags_aux(
    int mode, const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    object_kind* k_ptr;

    if (mode == OBJECT_FLAGS_KNOWN)
    {
        /* Clear */
        (*f1) = (*f2) = (*f3) = 0L;

        /* Must be identified */
        if (!object_known_p(o_ptr))
            return;
    }

    k_ptr = &k_info[o_ptr->k_idx];

    /* Base object */
    (*f1) = k_ptr->flags1;
    (*f2) = k_ptr->flags2;
    (*f3) = k_ptr->flags3;

    if (mode == OBJECT_FLAGS_FULL)
    {
        /* Artefact */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            (*f1) |= a_ptr->flags1;
            (*f2) |= a_ptr->flags2;
            (*f3) |= a_ptr->flags3;
        }
    }

    /* Ego-item */
    if (o_ptr->name2)
    {
        ego_item_type* e_ptr = &e_info[o_ptr->name2];

        (*f1) |= e_ptr->flags1;
        (*f2) |= e_ptr->flags2;
        (*f3) |= e_ptr->flags3;
    }

    if (mode == OBJECT_FLAGS_KNOWN)
    {
        /* Obvious artefact flags */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            /* Obvious flags (pval) */
            (*f1) = (a_ptr->flags1 & (TR1_PVAL_MASK));
            (*f3) = (a_ptr->flags3 & (TR3_IGNORE_MASK));
        }
    }

    if (mode == OBJECT_FLAGS_KNOWN)
    {
        /* Artefact, *ID'ed or spoiled */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            (*f1) = a_ptr->flags1;
            (*f2) = a_ptr->flags2;
            (*f3) = a_ptr->flags3;
        }
    }
}

/*
 * Obtain the "flags" for an item
 */
void object_flags(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    object_flags_aux(OBJECT_FLAGS_FULL, o_ptr, f1, f2, f3);
}

/*
 * Obtain the "flags" for an item which are known to the player
 */
void object_flags_known(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    object_flags_aux(OBJECT_FLAGS_KNOWN, o_ptr, f1, f2, f3);
}

/*
 * Efficient version of '(T) += sprintf((T), "%c", (C))'
 */
#define object_desc_chr_macro(T, C)                                            \
    do                                                                         \
    {                                                                          \
        /* Copy the char */                                                    \
        *(T)++ = (C);                                                          \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%s", (S))'
 */
#define object_desc_str_macro(T, S)                                            \
    do                                                                         \
    {                                                                          \
        cptr s = (S);                                                          \
                                                                               \
        /* Copy the string */                                                  \
        while (*s)                                                             \
            *(T)++ = *s++;                                                     \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%u", (N))'
 */
#define object_desc_num_macro(T, N)                                            \
    do                                                                         \
    {                                                                          \
        int n = (N);                                                           \
                                                                               \
        int p;                                                                 \
                                                                               \
        /* Find "size" of "n" */                                               \
        for (p = 1; n >= p * 10; p = p * 10) /* loop */                        \
            ;                                                                  \
                                                                               \
        /* Dump each digit */                                                  \
        while (p >= 1)                                                         \
        {                                                                      \
            /* Dump the digit */                                               \
            *(T)++ = I2D(n / p);                                               \
                                                                               \
            /* Remove the digit */                                             \
            n = n % p;                                                         \
                                                                               \
            /* Process next digit */                                           \
            p = p / 10;                                                        \
        }                                                                      \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%+d", (I))'
 */
#define object_desc_int_macro(T, I)                                            \
    do                                                                         \
    {                                                                          \
        int i = (I);                                                           \
                                                                               \
        /* Negative */                                                         \
        if (i < 0)                                                             \
        {                                                                      \
            /* Take the absolute value */                                      \
            i = 0 - i;                                                         \
                                                                               \
            /* Use a "minus" sign */                                           \
            *(T)++ = '-';                                                      \
        }                                                                      \
                                                                               \
        /* Positive (or zero) */                                               \
        else                                                                   \
        {                                                                      \
            /* Use a "plus" sign */                                            \
            *(T)++ = '+';                                                      \
        }                                                                      \
                                                                               \
        /* Dump the number itself */                                           \
        object_desc_num_macro(T, i);                                           \
                                                                               \
    } while (0)

/*
 * Strip an "object name" into a buffer.
 */
void strip_name(char* buf, int k_idx)
{
    char* t;

    object_kind* k_ptr = &k_info[k_idx];

    cptr str = (k_name + k_ptr->name);

    /* Skip past leading characters */
    while ((*str == ' ') || (*str == '&'))
        str++;

    /* Copy useful chars */
    for (t = buf; *str; str++)
    {
        if (*str != '~')
            *t++ = *str;
    }

    /* Terminate the new name */
    *t = '\0';
}

/*
 * Creates a description of the item "o_ptr", and stores it in "buf".
 *
 * One can choose the "verbosity" of the description, including whether
 * or not the "number" of items should be described, and how much detail
 * should be used when describing the item.
 *
 * The given "buf" should be at least 80 chars long to hold the longest
 * possible description, which can get pretty long, including inscriptions,
 * such as:
 * "no more Maces of Disruption (Defender) (+10,+10) [+5] (+3 to stealth)".

 * Note that the object description will be clipped to fit into the given
 * buffer size.
 *
 * Note the use of "object_desc_int_macro()" and "object_desc_num_macro()"
 * and "object_desc_str_macro()" and "object_desc_chr_macro()" as extremely
 * efficient, portable, versions of some common "sprintf()" commands (without
 * the bounds checking or termination writing), which allow a pointer to
 * efficiently move through a buffer while modifying it in various ways.
 *
 * Various improper uses and/or placements of "&" or "~" characters can
 * easily induce out-of-bounds memory accesses.  Some of these could be
 * easily checked for, if efficiency was not a concern.
 *
 * Note that all special items (when known) append an "Ego-Item Name", unless
 * the item is also an artefact, which should never happen.
 *
 * Note that all artefacts (when known) append an "Artefact Name", so we
 * have special processing for "Specials" (artefact Lites, Rings, Amulets).
 * The "Specials" never use "modifiers" if they are "known", since they
 * have special "descriptions", such as "The Necklace of the Dwarves".
 *
 * Special Lite's use the "k_info" base-name (Phial, Star, or Arkenstone),
 * plus the artefact name, just like any other artefact, if known.
 *
 * Special Ring's and Amulet's, if not "aware", use the same code as normal
 * rings and amulets, and if "aware", use the "k_info" base-name (Ring or
 * Amulet or Necklace).  They will NEVER "append" the "k_info" name.  But,
 * they will append the artefact name, just like any artefact, if known.
 *
 * None of the Special Rings/Amulets are "EASY_KNOW", though they could be,
 * at least, those which have no "pluses", such as the three artefact lites.
 *
 * The "pluralization" rules are extremely hackish, in fact, for efficiency,
 * we only handle things like "torch"/"torches" and "cutlass"/"cutlasses",
 * and we would not handle "box"/"boxes", or "knife"/"knives", correctly.
 * Of course, it would be easy to add rules for these forms.
 *
 * If "pref" is true then a "numeric" prefix will be pre-pended, else is is
 * assumed that a string such as "The" or "Your" will be pre-pended later.
 *
 * Modes ("pref" is TRUE):
 *  -1 -- Chain Mail
 *   0 -- Chain Mail of Death
 *   1 -- A Cloak of Death [1,+3]
 *   2 -- An Amulet of Death [1,+3] <+2>
 *   3 -- 5 Rings of Death [1,+3] <+2> {nifty}
 *
 * Modes ("pref" is FALSE):
 *  -1 -- Chain Mail
 *   0 -- Chain Mail of Death
 *   1 -- Cloak of Death [1,+3]
 *   2 -- Amulet of Death [1,+3] <+2>
 *   3 -- Rings of Death [1,+3] <+2> {nifty}
 */
void object_desc(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    cptr basenm;
    cptr modstr;

    bool aware;
    bool known;

    bool flavor;

    bool append_name;

    char* b;

    char* t;

    cptr s;

    cptr u;
    cptr v;

    char a1 = '<', a2 = '>';
    char p1 = '(', p2 = ')';
    char b1 = '[', b2 = ']';
    char c1 = '{', c2 = '}';

    char discount_buf[80];

    char tmp_buf[128];

    u32b f1, f2, f3;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Extract some flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* See if the object is "aware" */
    aware = (object_aware_p(o_ptr) ? TRUE : FALSE);

    /* See if the object is "known" */
    known = (object_known_p(o_ptr) ? TRUE : FALSE);

    /* See if the object is "flavored" */
    flavor = (k_ptr->flavor ? TRUE : FALSE);

    /* We have seen the object */
    if (aware)
        k_ptr->everseen = TRUE;

    /* Object is being listed in the object knowledge or smithing screen */
    if (o_ptr->ident & IDENT_SPOIL)
    {
        /* Don't show flavors */
        flavor = FALSE;

        /* Pretend known and aware */
        aware = TRUE;
        known = TRUE;
    }

    /* Player has now seen the item
     *
     * This code must be exactly here to properly handle objects in
     * stores (fake assignment to "aware", see above) and unaware objects
     * in the dungeon.
     */
    if (aware)
        k_ptr->everseen = TRUE;

    /* Assume no name appending */
    append_name = FALSE;

    /* Extract default "base" string */
    basenm = (k_name + k_ptr->name);

    /* Assume no "modifier" string */
    modstr = "";

    /* Analyze the object */
    switch (o_ptr->tval)
    {
    /* Some objects are easy to describe */
    case TV_SKELETON:
    case TV_METAL:
    case TV_NOTE:
    case TV_FLASK:
    case TV_CHEST:
    {
        break;
    }

    /* Missiles/Bows/Weapons */
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    {
        break;
    }

    /* Armour */
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_SHIELD:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        break;
    }

    /* Lights (including a few "Specials") */
    case TV_LIGHT:
    {
        break;
    }

    /* Amulets (including a few "Specials") */
    case TV_AMULET:
    {
        /* Hack -- Known artefacts */
        if (artefact_p(o_ptr) && aware)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = TRUE;
        basenm = (flavor ? "& # Amulet~" : "& Amulet~");

        break;
    }

    /* Rings (including a few "Specials") */
    case TV_RING:
    {
        /* Hack -- Known artefacts */
        if (artefact_p(o_ptr) && aware)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = TRUE;
        basenm = (flavor ? "& # Ring~" : "& Ring~");

        break;
    }

    /* Staffs */
    case TV_STAFF:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = TRUE;
        basenm = (flavor ? "& # Staff~" : "& Staff~");

        break;
    }

    /* Horns */
    case TV_HORN:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = TRUE;
        basenm = (flavor ? "& # Horn~" : "& Horn~");

        break;
    }

    /* Potions */
    case TV_POTION:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = TRUE;
        basenm = (flavor ? "& # Potion~" : "& Potion~");

        break;
    }

    /* Food */
    case TV_FOOD:
    {
        /* Ordinary food is "boring" */
        if (o_ptr->sval >= SV_FOOD_MIN_FOOD)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = TRUE;
        basenm = (flavor ? "& # Herb~" : "& Herb~");

        // Easter Eggs
        if (easter_time())
            basenm = (flavor ? "& # Easter Egg~" : "& Easter Egg~");

        break;
    }

    /* Hack -- Default -- Used in the "inventory" routine */
    default:
    {
        my_strcpy(buf, "(nothing)", max);
        return;
    }
    }

    /* Start dumping the result */
    t = b = tmp_buf;

    /* Begin */
    s = basenm;

    /* Handle objects which sometimes use "a" or "an" */
    if (*s == '&')
    {
        /* Paranoia XXX XXX XXX */
        /* ASSERT(s[1] == ' '); */

        /* Skip the ampersand and the following space */
        s += 2;

        /* No prefix */
        if (!pref)
        {
            /* Nothing */
        }

        /* Hack -- None left */
        else if (o_ptr->number <= 0)
        {
            object_desc_str_macro(t, "no more ");
        }

        /* Extract the number */
        else if (o_ptr->number > 1)
        {
            object_desc_num_macro(t, o_ptr->number);
            object_desc_chr_macro(t, ' ');
        }

        /* Hack -- The only one of its kind */
        else if (known && artefact_p(o_ptr))
        {
            object_desc_str_macro(t, "The ");
        }

        /* Hack -- A single one, and next character will be a vowel */
        else if ((*s == '#') ? is_a_vowel(modstr[0]) : is_a_vowel(*s))
        {
            object_desc_str_macro(t, "an ");
        }

        /* A single one, and next character will be a non-vowel */
        else
        {
            object_desc_str_macro(t, "a ");
        }
    }

    /* Handle objects which never use "a" or "an" */
    else
    {
        /* No pref */
        if (!pref)
        {
            /* Nothing */
        }

        /* Hack -- all gone */
        else if (o_ptr->number <= 0)
        {
            object_desc_str_macro(t, "no more ");
        }

        /* Prefix a number if required */
        else if (o_ptr->number > 1)
        {
            object_desc_num_macro(t, o_ptr->number);
            object_desc_chr_macro(t, ' ');
        }

        /* Hack -- The only one of its kind */
        else if (known && artefact_p(o_ptr))
        {
            object_desc_str_macro(t, "The ");
        }

        /* Hack -- A single item, so no prefix needed */
        else
        {
            /* Nothing */
        }
    }

    /* Paranoia XXX XXX XXX */
    /* ASSERT(*s != '~'); */

    /* Copy the string */
    for (; *s; s++)
    {
        /* Pluralizer */
        if (*s == '~')
        {
            /* Add a plural if needed */
            if ((o_ptr->number != 1) && !(known && artefact_p(o_ptr)))
            {
                char k = t[-1];

                /* Hack -- "Cutlass-es" and "Torch-es" */
                if ((k == 's') || (k == 'h'))
                    *t++ = 'e';

                /* Add an 's' */
                *t++ = 's';
            }
        }

        /* Modifier */
        else if (*s == '#')
        {
            /* Append the modifier */
            object_desc_str_macro(t, modstr);
        }

        /* Normal */
        else
        {
            /* Copy */
            *t++ = *s;
        }
    }

    /* No more details wanted */
    if (mode < 0)
        goto object_desc_done;

    /* Append the "kind name" to the "base name" */
    if (append_name)
    {
        object_desc_str_macro(t, " of ");
        object_desc_str_macro(t, (k_name + k_ptr->name));
    }

    /* Hack -- Append "Artefact" or "Special" names */
    if (known)
    {
        /* Grab any artefact name */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            object_desc_chr_macro(t, ' ');
            object_desc_str_macro(t, a_ptr->name);
        }

        /* Grab any special item name */
        else if (o_ptr->name2)
        {
            ego_item_type* e_ptr = &e_info[o_ptr->name2];

            object_desc_chr_macro(t, ' ');
            object_desc_str_macro(t, (e_name + e_ptr->name));
        }
    }

    /* No more details wanted */
    if (mode < 1)
        goto object_desc_done;

    /* Hack -- Chests and skeletons must be described in detail */
    if (o_ptr->tval == TV_SKELETON)
    {
        cptr tail = "";

        /* May be "searched" */
        if (!o_ptr->pval)
        {
            tail = " (searched)";
        }

        object_desc_str_macro(t, tail);
    }
    else if (o_ptr->tval == TV_CHEST)
    {
        cptr tail = "";

        /* Not searched yet */
        if (!known || (o_ptr->sval == SV_CHEST_PRESENT))
        {
            /* Nothing */
        }

        /* May be "empty" */
        else if (!o_ptr->pval)
        {
            tail = " (empty)";
        }

        /* May be "disarmed" */
        else if (o_ptr->pval < 0)
        {
            if (chest_traps[0 - o_ptr->pval])
            {
                tail = " (disarmed)";
            }
            else
            {
                tail = " (unlocked)";
            }
        }

        /* Describe the traps, if any */
        else
        {
            /* Describe the traps */
            switch (chest_traps[o_ptr->pval])
            {
            case 0:
            {
                tail = " (Locked)";
                break;
            }
            case CHEST_GAS_CONF:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_GAS_STUN:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_GAS_POISON:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_NEEDLE_HALLU:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_NEEDLE_ENTRANCE:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_NEEDLE_LOSE_STR:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_FLAME:
            {
                tail = " (Flame Trap)";
                break;
            }
            default:
            {
                tail = " (Multiple Traps)";
                break;
            }
            }
        }

        /* Append the tail */
        object_desc_str_macro(t, tail);
    }

    /* Dump base weapon info */
    switch (o_ptr->tval)
    {
    /* Missiles */
    case TV_ARROW:
    {
        /* Append a "hit" string if nonzero */
        if (o_ptr->att != 0)
        {
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);
            object_desc_int_macro(t, o_ptr->att);
            object_desc_chr_macro(t, p2);
        }

        /* All done */
        break;
    }

    /* Weapons */
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    case TV_BOW:
    {
        /* Append a "hit,damage" string */
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, p1);
        object_desc_int_macro(t, o_ptr->att);
        object_desc_chr_macro(t, ',');
        object_desc_num_macro(t, o_ptr->dd);
        object_desc_chr_macro(t, 'd');
        /* Bonus for 'hand and a half' weapons like the bastard sword when used
         * with two hands */
        object_desc_num_macro(t, o_ptr->ds + hand_and_a_half_bonus(o_ptr));
        object_desc_chr_macro(t, p2);

        /* All done */
        break;
    }

    default: /* not a weapon */
    {
        /* don't display for unidentified rings */
        if ((o_ptr->tval == TV_RING) && !object_known_p(o_ptr))
        {
            break;
        }
        if (o_ptr->att)
        {
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);
            object_desc_int_macro(t, o_ptr->att);
            object_desc_chr_macro(t, p2);
        }
    }
    }

    /* show evasion/protection info */

    /* but don't display for unidentified rings */
    if ((o_ptr->tval == TV_RING) && !object_known_p(o_ptr))
    {
        // do nothing
    }
    else if (o_ptr->pd && o_ptr->ps)
    {
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, b1);
        object_desc_int_macro(t, o_ptr->evn);
        object_desc_chr_macro(t, ',');
        object_desc_num_macro(t, o_ptr->pd);
        object_desc_chr_macro(t, 'd');
        object_desc_num_macro(t, o_ptr->ps);
        object_desc_chr_macro(t, b2);
    }
    else if (o_ptr->evn)
    {
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, b1);
        object_desc_int_macro(t, o_ptr->evn);
        object_desc_chr_macro(t, b2);
    }

    /* No more details wanted */
    if (mode < 2)
        goto object_desc_done;

    /* Hack -- Staffs have charges */
    if (o_ptr->tval == TV_STAFF)
    {
        if (known)
        {
            /* Dump " (N charges)" */
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);

            if (p_ptr->active_ability[S_WIL][WIL_CHANNELING])
            {
                object_desc_num_macro(t, o_ptr->pval);
            }
            else
            {
                object_desc_num_macro(
                    t, o_ptr->pval / CHANNELING_CHARGE_MULTIPLIER);
            }

            /*write out the word charge(s) as appropriate*/
            object_desc_str_macro(t, " charge");
            if (o_ptr->pval != 1)
            {
                object_desc_chr_macro(t, 's');
            }
            object_desc_chr_macro(t, p2);
        }

        else if ((o_ptr->xtra1 > 0) && !(o_ptr->ident & (IDENT_EMPTY)))
        {
            /* Dump " (used N times)" */
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);

            /*write out the word charge(s) as appropriate*/
            object_desc_str_macro(t, "used ");
            object_desc_num_macro(t, o_ptr->xtra1);
            object_desc_str_macro(t, " time");
            if (o_ptr->xtra1 != 1)
            {
                object_desc_chr_macro(t, 's');
            }
            object_desc_chr_macro(t, p2);
        }
    }

    /* Hack -- Process Lanterns/Torches */
    if (fuelable_light_p(o_ptr))
    {
        /* Hack -- Turns of light for normal lites */
        object_desc_str_macro(t, " (");
        object_desc_num_macro(t, o_ptr->timeout);
        object_desc_str_macro(t, " turns)");
    }

    /* Dump "pval" flags for wearable items */
    if (known && (f1 & (TR1_PVAL_MASK)))
    {
        cptr tail = "";
        cptr tail2 = "";

        /* Start the display */
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, a1);

        /* Dump the "pval" itself */
        object_desc_int_macro(t, o_ptr->pval);

        /* Add the descriptor */
        object_desc_str_macro(t, tail);
        object_desc_str_macro(t, tail2);

        /* Finish the display */
        object_desc_chr_macro(t, a2);
    }

    /* Indicate "charging" objects, but not horns or lights */
    if (known && o_ptr->timeout && !fuelable_light_p(o_ptr))
    {
        /* Hack -- Dump " (charging)" if relevant */
        object_desc_str_macro(t, " (recharging)");
    }

    /* No more details wanted */
    if (mode < 3)
        goto object_desc_done;

    /* Use standard inscription */
    if (o_ptr->obj_note)
    {
        u = quark_str(o_ptr->obj_note);
    }

    /* Use nothing */
    else
    {
        u = NULL;
    }

    /* Use special inscription, if any */
    if (o_ptr->discount >= INSCRIP_NULL)
    {
        if ((o_ptr->discount != INSCRIP_AVERAGE)
            && (o_ptr->discount != INSCRIP_GOOD_STRONG))
        {
            v = inscrip_text[o_ptr->discount - INSCRIP_NULL];
        }
        else
        {
            v = NULL;
        }
    }

    /* Use "cursed" if the item is known to be cursed */
    else if (cursed_p(o_ptr) && known)
    {
        v = "cursed";
    }

    /* Hack -- Use "empty" for empty wands/staffs */
    else if (!known && (o_ptr->ident & (IDENT_EMPTY)))
    {
        v = "empty";
    }

    /* Use "tried" if the object has been tested unsuccessfully */
    else if (!aware && object_tried_p(o_ptr))
    {
        v = "tried";
    }

    /* Use the discount, if any */
    else if (o_ptr->discount > 0)
    {
        char* q = discount_buf;
        object_desc_num_macro(q, o_ptr->discount);
        object_desc_str_macro(q, "% off");
        *q = '\0';
        v = discount_buf;
    }

    /* Nothing */
    else
    {
        v = NULL;
    }

    /* Inscription */
    if (u || v)
    {
        /* Begin the inscription */
        *t++ = ' ';
        *t++ = c1;

        /* Standard inscription */
        if (u)
        {
            /* Append the inscription */
            while ((t < b + 75) && *u)
                *t++ = *u++;
        }

        /* Special inscription too */
        if (u && v && (t < b + 75))
        {
            /* Separator */
            *t++ = ',';
            *t++ = ' ';
        }

        /* Special inscription */
        if (v)
        {
            /* Append the inscription */
            while ((t < b + 75) && *v)
                *t++ = *v++;
        }

        /* Terminate the inscription */
        *t++ = c2;
    }

object_desc_done:

    /* Terminate */
    *t = '\0';

    /* Copy the string over */
    my_strcpy(buf, tmp_buf, max);
}

/*
 * Describe an item and pretend the item is fully known and has no flavor.
 */
void object_desc_spoil(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    /* Make a backup */
    object_copy(i_ptr, o_ptr);

    /* Set it to display as identified but without flavour */
    i_ptr->ident |= IDENT_SPOIL;

    /* Describe */
    object_desc(buf, max, i_ptr, pref, mode);
}

/*
 * Describe an item's random attributes for "character dumps"
 */
void identify_random_gen(const object_type* o_ptr)
{
    /* Set hooks for character dump */
    object_info_out_flags = object_flags_known;

    /* Set the indent/wrap */
    text_out_indent = 3;
    text_out_wrap = 65;

    /* Dump the info */
    if (object_info_out(o_ptr))
        text_out("\n");

    /* Reset indent/wrap */
    text_out_indent = 0;
    text_out_wrap = 0;
}

/*
 * Convert an inventory index into a one character label.
 *
 * Note that the label does NOT distinguish inven/equip.
 */
char index_to_label(int i)
{
    /* Indexes for "inven" are easy */
    if (i < INVEN_WIELD)
        return (I2A(i));

    /* Indexes for "equip" are offset */
    return (I2A(i - INVEN_WIELD));
}

/*
 * Convert a label into the index of an item in the "inven".
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_inven(int c)
{
    int i;

    /* Convert */
    i = (islower((unsigned char)c) ? A2I(c) : -1);

    /* Verify the index */
    if ((i < 0) || (i > INVEN_PACK))
        return (-1);

    /* Empty slots can never be chosen */
    if (!inventory[i].k_idx)
        return (-1);

    /* Return the index */
    return (i);
}

/*
 * Convert a label into the index of a item in the "equip".
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_equip(int c)
{
    int i;

    /* Convert */
    i = (islower((unsigned char)c) ? A2I(c) : -1) + INVEN_WIELD;

    /* Verify the index */
    if ((i < INVEN_WIELD) || (i >= INVEN_TOTAL))
        return (-1);

    /* Empty slots can never be chosen */
    if (!inventory[i].k_idx)
        return (-1);

    /* Return the index */
    return (i);
}

/*
 * Determine which equipment slot (if any) an item likes
 */
s16b wield_slot(const object_type* o_ptr)
{
    /* Slot for equipment */
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    {
        return (INVEN_WIELD);
    }

    case TV_BOW:
    {
        return (INVEN_BOW);
    }

    case TV_RING:
    {
        /* Use the right hand first */
        if (!inventory[INVEN_RIGHT].k_idx)
            return (INVEN_RIGHT);

        /* Use the left hand for swapping (by default) */
        return (INVEN_LEFT);
    }

    case TV_AMULET:
    {
        return (INVEN_NECK);
    }

    case TV_LIGHT:
    {
        return (INVEN_LITE);
    }

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    {
        return (INVEN_BODY);
    }

    case TV_CLOAK:
    {
        return (INVEN_OUTER);
    }

    case TV_SHIELD:
    {
        return (INVEN_ARM);
    }

    case TV_CROWN:
    case TV_HELM:
    {
        return (INVEN_HEAD);
    }

    case TV_GLOVES:
    {
        return (INVEN_HANDS);
    }

    case TV_BOOTS:
    {
        return (INVEN_FEET);
    }

    case TV_ARROW:
    {
        // Use the first similar quiver if there is one
        if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
            return (INVEN_QUIVER1);
        if (object_similar(&inventory[INVEN_QUIVER2], o_ptr))
            return (INVEN_QUIVER2);

        // Use the 2nd quiver if it is the only empty one
        if (!inventory[INVEN_QUIVER2].k_idx && inventory[INVEN_QUIVER1].k_idx)
            return (INVEN_QUIVER2);

        // Use the 1st quiver otherwise
        return (INVEN_QUIVER1);
    }
    }

    /* No slot available */
    return (-1);
}

/*
 * Return a string mentioning how a given item is carried
 */
cptr describe_empty_slot(int i)
{
    cptr p;

    /* Examine the location */
    switch (i)
    {
    case INVEN_WIELD:
        p = "(no weapon)";
        break;
    case INVEN_BOW:
        p = "(no bow)";
        break;
    case INVEN_LEFT:
        p = "(no left ring)";
        break;
    case INVEN_RIGHT:
        p = "(no right ring)";
        break;
    case INVEN_NECK:
        p = "(no amulet)";
        break;
    case INVEN_LITE:
        p = "(no light source)";
        break;
    case INVEN_BODY:
        p = "(no body armour)";
        break;
    case INVEN_OUTER:
        p = "(no cloak)";
        break;
    case INVEN_ARM:
        p = "(no shield)";
        break;
    case INVEN_HEAD:
        p = "(no helmet)";
        break;
    case INVEN_HANDS:
        p = "(no gloves)";
        break;
    case INVEN_FEET:
        p = "(no boots)";
        break;
    case INVEN_QUIVER1:
        p = "(empty 1st quiver)";
        break;
    case INVEN_QUIVER2:
        p = "(empty 2nd quiver)";
        break;
    default:
        p = "(empty slot)";
        break;
    }

    /* Return the result */
    return (p);
}

/*
 * Return a string mentioning how a given item is carried
 */
cptr mention_use(int i)
{
    cptr p;

    /* Examine the location */
    switch (i)
    {
    case INVEN_WIELD:
        p = "Wielding";
        break;
    case INVEN_BOW:
        p = "Shooting";
        break;
    case INVEN_LEFT:
        p = "Left ring";
        break;
    case INVEN_RIGHT:
        p = "Right ring";
        break;
    case INVEN_NECK:
        p = "Around neck";
        break;
    case INVEN_LITE:
        p = "Light";
        break;
    case INVEN_BODY:
        p = "On body";
        break;
    case INVEN_OUTER:
        p = "About body";
        break;
    case INVEN_ARM:
        p = "Off-hand";
        break;
    case INVEN_HEAD:
        p = "On head";
        break;
    case INVEN_HANDS:
        p = "On hands";
        break;
    case INVEN_FEET:
        p = "On feet";
        break;
    case INVEN_QUIVER1:
        p = "1st quiver";
        break;
    case INVEN_QUIVER2:
        p = "2nd quiver";
        break;
    default:
        p = "In pack";
        break;
    }

    /* Return the result */
    return (p);
}

/*
 * Return a string describing how a given item is being worn.
 * Currently, only used for items in the equipment, not inventory.
 */
cptr describe_use(int i)
{
    cptr p;

    switch (i)
    {
    case INVEN_WIELD:
        p = "wielding";
        break;
    case INVEN_BOW:
        p = "wielding";
        break;
    case INVEN_LEFT:
        p = "wearing on your left hand";
        break;
    case INVEN_RIGHT:
        p = "wearing on your right hand";
        break;
    case INVEN_NECK:
        p = "wearing around your neck";
        break;
    case INVEN_LITE:
        p = "using to light the way";
        break;
    case INVEN_BODY:
        p = "wearing on your body";
        break;
    case INVEN_OUTER:
        p = "wearing on your back";
        break;
    case INVEN_ARM:
        p = "wearing on your arm";
        break;
    case INVEN_HEAD:
        p = "wearing on your head";
        break;
    case INVEN_HANDS:
        p = "wearing on your hands";
        break;
    case INVEN_FEET:
        p = "wearing on your feet";
        break;
    case INVEN_QUIVER1:
        p = "carrying in your quiver";
        break;
    case INVEN_QUIVER2:
        p = "carrying in your quiver";
        break;
    default:
        p = "carrying in your pack";
        break;
    }

    /* Return the result */
    return p;
}

/*
 * Check an item against the item tester info
 */
bool item_tester_okay(const object_type* o_ptr)
{
    /* Hack -- allow listing empty slots */
    if (item_tester_full)
        return (TRUE);

    /* Require an item */
    if (!o_ptr->k_idx)
        return (FALSE);

    /* Check the tval */
    if (item_tester_tval)
    {
        if (!(item_tester_tval == o_ptr->tval))
            return (FALSE);
    }

    /* Check the hook */
    if (item_tester_hook)
    {
        if (!(*item_tester_hook)(o_ptr))
            return (FALSE);
    }

    /* Assume okay */
    return (TRUE);
}

/*
 * Get the indexes of objects at a given floor location.
 *
 * Return the number of object indexes acquired.
 *
 * Never acquire more than "size" object indexes, and never return a
 * number bigger than "size", even if more floor objects exist.
 *
 * Valid flags are any combination of the bits:
 *
 *   0x01 -- Verify item tester
 *   0x02 -- Marked items only
 */
int scan_floor(int* items, int size, int y, int x, int mode)
{
    int this_o_idx, next_o_idx;

    int num = 0;

    /* Sanity */
    if (!in_bounds(y, x))
        return (0);

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Verify item tester */
        if ((mode & 0x01) && !item_tester_okay(o_ptr))
            continue;

        /* Marked items only */
        if ((mode & 0x02) && !o_ptr->marked)
            continue;

        /* Accept this item */
        items[num++] = this_o_idx;

        /* Enforce size limit */
        if (num >= size)
            break;
    }

    /* Result */
    return (num);
}

/*
 * Choice window "shadow" of the "show_inven()" function
 */
void display_inven(void)
{
    register int i, n, z = 0;

    object_type* o_ptr;

    byte attr;

    char tmp_val[80];

    char o_name[80];

    bool floor_item = FALSE;

    int w = MIN(Term->wid, 100);
    int col = w - 11;
    int offset = use_bigtile ? 6 : 5;

    /* Find the "final" slot */
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Track */
        z = i + 1;
    }

    /* Display the pack (and the floor) */
    for (i = 0; i <= z; i++)
    {
        if (i == z)
        {
            // get item from floor
            o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            i = INVEN_WIELD;
            if (!o_ptr->k_idx)
                continue;
            floor_item = TRUE;
        }
        else
        {
            // get item from inventory
            o_ptr = &inventory[i];
        }

        /* Start with an empty "index" */
        tmp_val[0] = tmp_val[1] = tmp_val[2] = ' ';

        /* Is this item "acceptable"? */
        if (item_tester_okay(o_ptr))
        {
            // first, do this for inventory items...
            if (!floor_item)
            {
                // does the current command even allow inventory items to be
                // used?
                if ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_INVEN)))
                {
                    /* Prepare a "label" */
                    tmp_val[0] = index_to_label(i);

                    /* Bracket the "label" --(-- */
                    tmp_val[1] = ')';
                }
            }
            // now for the floor item (if any)
            else
            {
                // does the current command even allow floor items to be used?
                if ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_FLOOR)))
                {
                    /* Prepare a "label" */
                    tmp_val[0] = '-';

                    /* Bracket the "label" --(-- */
                    tmp_val[1] = ')';
                }
            }
        }

        // use white if the inventory is being queried, slate if the equipment
        // is
        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_INVEN)))
            attr = TERM_WHITE;
        else
            attr = TERM_SLATE;

        /* Display the index (or blank space) */
        Term_putstr(0, i, 3, attr, tmp_val);

        /* Display the symbol */
        Term_putch(3, i, object_attr(o_ptr), object_char(o_ptr));
        if (use_bigtile)
        {
            Term_putch(4, i, 255, -1);
        }

        /* Obtain an item description */
        object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

        /* Obtain the length of the description */
        n = strlen(o_name);

        /* Get inventory color */
        attr = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        /* Display the entry itself */
        Term_putch(offset - 1, i, attr, ' ');
        Term_putstr(offset, i, n, attr, o_name);

        /* Erase the rest of the line */
        Term_erase(offset + n, i, 255);

        /* Display the weight if needed */
        if (o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
            Term_putstr(col, i, -1, attr, tmp_val);
        }
    }

    /* Erase the rest of the window */
    for (i = z; i < Term->hgt; i++)
    {
        if ((i != INVEN_WIELD) || !floor_item)
        {
            /* Erase the line */
            Term_erase(0, i, 255);
        }
    }
}

/*
 * Choice window "shadow" of the "show_equip()" function
 */
void display_equip(void)
{
    register int i, n;
    object_type* o_ptr;
    byte attr;
    int armour_weight = 0;

    char tmp_val[80];

    char o_name[80];

    int w = MIN(Term->wid, 100);
    int col = w - 11;
    int offset = use_bigtile ? 6 : 5;

    /* Display the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        /* Examine the item */
        o_ptr = &inventory[i];

        /* Start with an empty "index" */
        tmp_val[0] = tmp_val[1] = tmp_val[2] = ' ';

        /* Is this item "acceptable"? */
        if (item_tester_okay(o_ptr))
        {
            // does the current command even allow equipment to be used?
            if ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_EQUIP)))
            {
                /* Prepare an "index" */
                tmp_val[0] = index_to_label(i);

                /* Bracket the "index" --)-- */
                tmp_val[1] = ')';
            }
        }

        // use white if the equipment is being queried, slate if the inventory
        // is
        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_EQUIP)))
            attr = TERM_WHITE;
        else
            attr = TERM_SLATE;

        /* Display the index (or blank space) */
        Term_putstr(0, i - INVEN_WIELD, 3, attr, tmp_val);

        /* Display the symbol */
        if (!use_graphics && !o_ptr->tval) {
            /* object_attr() for an empty slot gives '\0'.  Use ' ' instead. */
            Term_putch(3, i - INVEN_WIELD, attr, ' ');
        } else {
            Term_putch(3, i - INVEN_WIELD, object_attr(o_ptr),
                object_char(o_ptr));
        }
        if (use_bigtile)
        {
            Term_putch(4, i - INVEN_WIELD, 255, -1);
        }

        /* Obtain an item description */
        if (o_ptr->tval)
        {
            object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
        }
        else
        {
            sprintf(o_name, "%s", describe_empty_slot(i));
        }

        /* Obtain the length of the description */
        n = strlen(o_name);

        /* Get inventory color */
        attr = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        /* Display the entry itself */
        Term_putch(offset - 1, i - INVEN_WIELD, attr, ' ');
        Term_putstr(offset, i - INVEN_WIELD, n, attr, o_name);

        /* Erase the rest of the line */
        Term_erase(offset + n, i - INVEN_WIELD, 255);

        /* Display the weight (if needed) */
        if (o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb ", wgt / 10, wgt % 10);
            if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            {
                Term_putstr(col, i - INVEN_WIELD, -1, TERM_SLATE, tmp_val);
                armour_weight += wgt;
            }
            else
            {
                Term_putstr(col, i - INVEN_WIELD, -1, attr, tmp_val);
            }
        }

        // Term_erase(w - 12 + strlen(mention_use(i)), i - INVEN_WIELD, 255);
    }

    /* Erase the rest of the window */
    for (i = INVEN_TOTAL - INVEN_WIELD; i < Term->hgt; i++)
    {
        /* Clear that line */
        Term_erase(0, i, 255);
    }

    /* Put in the total weight */
    Term_putstr(col, INVEN_TOTAL - INVEN_WIELD, -1, TERM_L_DARK, "--------");
    sprintf(
        tmp_val, "armour: %3d.%1d lb", armour_weight / 10, armour_weight % 10);
    Term_putstr(
        col - 8, INVEN_TOTAL - INVEN_WIELD + 1, -1, TERM_SLATE, tmp_val);
}

/*
 * Display the inventory.
 *
 * Hack -- do not display "trailing" empty slots
 */
void show_inven(void)
{
    int i, j, k, l, z = 0;
    int col, len, lim;

    object_type* o_ptr;

    char o_name[80];

    char tmp_val[80];

    int out_index[24];
    byte out_color[24];
    char out_desc[24][80];

    /* Default length */
    len = 79 - 50;

    /* Maximum space allowed for descriptions */
    lim = 79 - 3;

    /* Require space for weight (if needed) */
    if (show_weights)
        lim -= 9;

    /* Find the "final" slot */
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Track */
        z = i + 1;
    }

    /* Display the inventory */
    for (k = 0, i = 0; i < z; i++)
    {
        o_ptr = &inventory[i];

        /* Is this item acceptable? */
        if (!item_tester_okay(o_ptr))
            continue;

        /* Describe the object */
        object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

        /* Hack -- enforce max length */
        o_name[lim] = '\0';

        /* Save the index */
        out_index[k] = i;

        /* Get inventory color */
        if (weapon_glows(o_ptr))
            out_color[k] = TERM_L_BLUE;
        else
            out_color[k] = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        /* Save the object description */
        my_strcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        /* Find the predicted "line length" */
        l = strlen(out_desc[k]) + 5;

        /* Be sure to account for the weight */
        if (show_weights)
            l += 9;

        /* Maintain the maximum length */
        if (l > len)
            len = l;

        /* Advance to next "line" */
        k++;
    }

    /* Find the column to start in */
    col = (len > 76) ? 0 : (79 - len);

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = out_index[j];

        /* Get the item */
        o_ptr = &inventory[i];

        /* Clear the line */
        prt("", j + 1, col ? col - 2 : col);

        /* Prepare an index --(-- */
        sprintf(tmp_val, "%c)", index_to_label(i));

        /* Clear the line with the (possibly indented) index */
        put_str(tmp_val, j + 1, col);

        /* Display the entry itself */
        c_put_str(out_color[j], out_desc[j], j + 1, col + 3);

        /* Display the weight if needed */
        if (show_weights)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
            c_put_str(out_color[j], tmp_val, j + 1, 71);
        }
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < 23))
        prt("", j + 1, col ? col - 2 : col);
}

/*
 * Display the equipment.
 */
void show_equip(void)
{
    int i, j, k, l;
    int col, len, lim;

    object_type* o_ptr;

    char tmp_val[80];

    char o_name[80];

    int out_index[24];
    byte out_color[24];
    char out_desc[24][80];

    int armour_weight = 0;

    /* Default length */
    len = 79 - 50;

    /* Maximum space allowed for descriptions */
    lim = 79 - 3;

    /* Require space for labels */
    lim -= (14 + 2);

    /* Require space for weight (if needed) */
    if (show_weights)
        lim -= 9;

    /* Scan the equipment list */
    for (k = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Is this item acceptable? */
        if (!item_tester_okay(o_ptr))
            continue;

        /* Description */
        object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

        /* Truncate the description */
        o_name[lim] = 0;

        /* Save the index */
        out_index[k] = i;

        /* Get inventory color */
        out_color[k] = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        /* Save the description */
        my_strcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        /* Extract the maximal length (see below) */
        l = strlen(out_desc[k]) + (2 + 3);

        /* Increase length for labels */
        l += (12 + 2);

        /* Increase length for weight (if needed) */
        if (show_weights)
            l += 9;

        /* Maintain the max-length */
        if (l > len)
            len = l;

        /* Advance the entry */
        k++;
    }

    /* Hack -- Find a column to start in */
    col = (len > 76) ? 0 : (79 - len);

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = out_index[j];

        /* Get the item */
        o_ptr = &inventory[i];

        /* Clear the line */
        prt("", j + 1, col ? col - 2 : col);

        /* Prepare an index --(-- */
        sprintf(tmp_val, "%c)", index_to_label(i));

        /* Clear the line with the (possibly indented) index */
        put_str(tmp_val, j + 1, col);

        /* Mention the use */
        strnfmt(tmp_val, sizeof(tmp_val), "%-12s: ", mention_use(i));
        put_str(tmp_val, j + 1, col + 3);

        /* Display the entry itself */
        c_put_str(out_color[j], out_desc[j], j + 1, col + 3 + 12 + 2);

        /* Display the weight if needed */
        if (show_weights && o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%d lb", wgt / 10, wgt % 10);

            if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            {
                c_put_str(TERM_SLATE, tmp_val, j + 1, 71);
                armour_weight += wgt;
            }
            else
            {
                c_put_str(out_color[j], tmp_val, j + 1, 71);
            }
        }
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < 23))
        prt("", j + 1, col ? col - 2 : col);

    /* Put in the total weight */
    if (armour_weight)
    {
        int col = 52;
        /* Blank the line for the total */
        prt("", j + 2, col ? col - 2 : col);
        c_put_str(TERM_L_DARK, "--------", INVEN_TOTAL - INVEN_WIELD + 1, 71);
        sprintf(tmp_val, "armour: %3d.%1d lb", armour_weight / 10,
            armour_weight % 10);
        c_put_str(TERM_SLATE, tmp_val, INVEN_TOTAL - INVEN_WIELD + 2, 71 - 8);
        /* Make a new "shadow" below the list (only if needed) */
        if (j && (j + 3 < 23))
            prt("", j + 3, col ? col - 2 : col);
    }
}

/*
 * Display a list of the items on the floor at the given location.
 */
void show_floor(const int* floor_list, int floor_num)
{
    int i, j, k, l;
    int col, len, lim;

    object_type* o_ptr;

    char o_name[80];

    char tmp_val[80];

    int out_index[MAX_FLOOR_STACK];
    byte out_color[MAX_FLOOR_STACK];
    char out_desc[MAX_FLOOR_STACK][80];

    /* Default length */
    len = 79 - 50;

    /* Maximum space allowed for descriptions */
    lim = 79 - 3;

    /* Require space for weight (if needed) */
    if (show_weights)
        lim -= 9;

    /* Display the inventory */
    for (k = 0, i = 0; i < floor_num; i++)
    {
        o_ptr = &o_list[floor_list[i]];

        /* Is this item acceptable? */
        if (!item_tester_okay(o_ptr))
            continue;

        /* Describe the object */
        object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

        /* Hack -- enforce max length */
        o_name[lim] = '\0';

        /* Save the index */
        out_index[k] = i;

        /* Get inventory color */
        out_color[k] = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        /* Save the object description */
        my_strcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        /* Find the predicted "line length" */
        l = strlen(out_desc[k]) + 5;

        /* Be sure to account for the weight */
        if (show_weights)
            l += 9;

        /* Maintain the maximum length */
        if (l > len)
            len = l;

        /* Advance to next "line" */
        k++;
    }

    /* Find the column to start in */
    col = (len > 76) ? 0 : (79 - len);

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = floor_list[out_index[j]];

        /* Get the item */
        o_ptr = &o_list[i];

        /* Clear the line */
        prt("", j + 1, col ? col - 2 : col);

        /* Prepare an index --(-- */
        sprintf(tmp_val, "%c)", index_to_label(out_index[j]));

        /* Clear the line with the (possibly indented) index */
        put_str(tmp_val, j + 1, col);

        /* Display the entry itself */
        c_put_str(out_color[j], out_desc[j], j + 1, col + 3);

        /* Display the weight if needed */
        if (show_weights)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
            c_put_str(out_color[j], tmp_val, j + 1, 71);
        }
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < 23))
        prt("", j + 1, col ? col - 2 : col);
}

/*
 * Flip "inven" and "equip" in any sub-windows
 */
void toggle_inven_equip(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Unused */
        if (!angband_term[j])
            continue;

        /* Flip inven to equip */
        if (op_ptr->window_flag[j] & (PW_INVEN))
        {
            /* Flip flags */
            op_ptr->window_flag[j] &= ~(PW_INVEN);
            op_ptr->window_flag[j] |= (PW_EQUIP);

            /* Window stuff */
            p_ptr->window |= (PW_EQUIP);
        }

        /* Flip inven to equip */
        else if (op_ptr->window_flag[j] & (PW_EQUIP))
        {
            /* Flip flags */
            op_ptr->window_flag[j] &= ~(PW_EQUIP);
            op_ptr->window_flag[j] |= (PW_INVEN);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN);
        }
    }
}

/*
 * Verify the choice of an item.
 *
 * The item can be negative to mean "item on floor".
 */
static bool verify_item(cptr prompt, int item)
{
    char o_name[80];

    char out_val[160];

    object_type* o_ptr;

    /* Inventory */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Floor */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Describe */
    object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

    /* Prompt */
    strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

    /* Query */
    return (get_check(out_val));
}

/*
 * Hack -- allow user to "prevent" certain choices.
 *
 * The item can be negative to mean "item on floor".
 */
static bool get_item_allow(int item)
{
    cptr s;

    object_type* o_ptr;

    /* Inventory */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Floor */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* No inscription */
    if (!o_ptr->obj_note)
        return (TRUE);

    /* Find a '!' */
    s = strchr(quark_str(o_ptr->obj_note), '!');

    /* Process preventions */
    while (s)
    {
        /* Check the "restriction" */
        if ((s[1] == p_ptr->command_cmd) || (s[1] == '*'))
        {
            /* Verify the choice */
            if (!verify_item("Really try", item))
                return (FALSE);
        }

        /* Find another '!' */
        s = strchr(s + 1, '!');
    }

    /* Allow it */
    return (TRUE);
}

/*
 * Verify the "okayness" of a given item.
 *
 * The item can be negative to mean "item on floor".
 */
static bool get_item_okay(int item)
{
    object_type* o_ptr;

    /* Inventory */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Floor */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Verify the item */
    return (item_tester_okay(o_ptr));
}

/*
 * Find the "first" inventory object with the given "tag".
 *
 * A "tag" is a char "n" appearing as "@n" anywhere in the
 * inscription of an object.
 *
 * Also, the tag "@xn" will work as well, where "n" is a tag-char,
 * and "x" is the "current" p_ptr->command_cmd code.
 *
 * Also works with '[' for first valid choice and ']' for last valid choice.
 */
static int get_tag(int* cp, char tag)
{
    int i;
    cptr s;

    /* Check every object */
    for (i = 0; i < INVEN_TOTAL; ++i)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip empty inscriptions */
        if (!o_ptr->obj_note)
            continue;

        /* Find a '@' */
        s = strchr(quark_str(o_ptr->obj_note), '@');

        /* Process all tags */
        while (s)
        {
            /* Check the normal tags */
            if (s[1] == tag)
            {
                /* Save the actual inventory ID */
                *cp = i;

                /* Success */
                return (TRUE);
            }

            /* Check the special tags */
            if ((s[1] == p_ptr->command_cmd) && (s[2] == tag))
            {
                /* Save the actual inventory ID */
                *cp = i;

                /* Success */
                return (TRUE);
            }

            /* Find another '@' */
            s = strchr(s + 1, '@');
        }
    }

    /* No such tag */
    return (FALSE);
}

/*
 * Let the user select an item, save its "index"
 *
 * Return TRUE only if an acceptable item was chosen by the user.
 *
 * The selected item must satisfy the "item_tester_hook()" function,
 * if that hook is set, and the "item_tester_tval", if that value is set.
 *
 * All "item_tester" restrictions are cleared before this function returns.
 *
 * The user is allowed to choose acceptable items from the equipment,
 * inventory, or floor, respectively, if the proper flag was given,
 * and there are any acceptable items in that location.
 *
 * The equipment or inventory are displayed (even if no acceptable
 * items are in that location) if the proper flag was given.
 *
 * If there are no acceptable items available anywhere, and "str" is
 * not NULL, then it will be used as the text of a warning message
 * before the function returns.
 *
 * Note that the user must press "-" to specify the item on the floor,
 * and there is no way to "examine" the item on the floor, while the
 * use of "capital" letters will "examine" an inventory/equipment item,
 * and prompt for its use.
 *
 * If a legal item is selected from the inventory, we save it in "cp"
 * directly (0 to 35), and return TRUE.
 *
 * If a legal item is selected from the floor, we save it in "cp" as
 * a negative (-1 to -511), and return TRUE.
 *
 * If no item is available, we do nothing to "cp", and we display a
 * warning message, using "str" if available, and return FALSE.
 *
 * If no item is selected, we do nothing to "cp", and return FALSE.
 *
 * Global "p_ptr->command_new" is used when viewing the inventory or equipment
 * to allow the user to enter a command while viewing those screens, and
 * also to induce "auto-enter" of stores, and other such stuff.
 *
 * Global "p_ptr->command_see" may be set before calling this function to start
 * out in "browse" mode.  It is cleared before this function returns.
 *
 * Global "p_ptr->command_wrk" is used to choose between equip/inven/floor
 * listings.  It is equal to USE_INVEN or USE_EQUIP or USE_FLOOR, except
 * when this function is first called, when it is equal to zero, which will
 * cause it to be set to USE_INVEN.
 *
 * We always erase the prompt when we are done, leaving a blank line,
 * or a warning message, if appropriate, if no items are available.
 *
 * Note that only "acceptable" floor objects get indexes, so between two
 * commands, the indexes of floor objects may change.  XXX XXX XXX
 */
bool get_item(int* cp, cptr pmt, cptr str, int mode)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    char which;

    int i, j;
    int k = INVEN_WIELD; // a default value to soothe compilation warnings

    int i1, i2;
    int e1, e2;
    int f1, f2;

    bool done, item;

    bool oops = FALSE;

    bool use_inven = ((mode & (USE_INVEN)) ? TRUE : FALSE);
    bool use_equip = ((mode & (USE_EQUIP)) ? TRUE : FALSE);
    bool use_floor = ((mode & (USE_FLOOR)) ? TRUE : FALSE);

    bool allow_inven = FALSE;
    bool allow_equip = FALSE;
    bool allow_floor = FALSE;

    bool toggle = FALSE;

    char tmp_val[160];
    char out_val[160];

    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

#ifdef ALLOW_REPEAT

    /* Get the item index */
    if (repeat_pull(cp))
    {
        /* Verify the item */
        if (get_item_okay(*cp))
        {
            /* Forget the item_tester_tval restriction */
            item_tester_tval = 0;

            /* Forget the item_tester_hook restriction */
            item_tester_hook = NULL;

            /* Success */
            return (TRUE);
        }
        else
        {
            /* Invalid repeat - reset it */
            repeat_clear();
        }
    }

#endif /* ALLOW_REPEAT */

    // save the mode in a global variable version
    p_ptr->get_item_mode = mode;

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Not done */
    done = FALSE;

    /* No item selected */
    item = FALSE;

    /* Full inventory */
    i1 = 0;
    i2 = INVEN_PACK - 1;

    /* Forbid inventory */
    if (!use_inven)
        i2 = -1;

    /* Restrict inventory indexes */
    while ((i1 <= i2) && (!get_item_okay(i1)))
        i1++;
    while ((i1 <= i2) && (!get_item_okay(i2)))
        i2--;

    /* Accept inventory */
    if (i1 <= i2)
        allow_inven = TRUE;

    /* Full equipment */
    e1 = INVEN_WIELD;
    e2 = INVEN_TOTAL - 1;

    /* Forbid equipment */
    if (!use_equip)
        e2 = -1;

    /* Restrict equipment indexes */
    while ((e1 <= e2) && (!get_item_okay(e1)))
        e1++;
    while ((e1 <= e2) && (!get_item_okay(e2)))
        e2--;

    /* Accept equipment */
    if (e1 <= e2)
        allow_equip = TRUE;

    /* Scan all objects in the grid */
    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, py, px, 0x00);

    /* Full floor */
    f1 = 0;
    f2 = floor_num - 1;

    /* Forbid floor */
    if (!use_floor)
        f2 = -1;

    /* Restrict floor indexes */
    while ((f1 <= f2) && (!get_item_okay(0 - floor_list[f1])))
        f1++;
    while ((f1 <= f2) && (!get_item_okay(0 - floor_list[f2])))
        f2--;

    /* Accept floor */
    if (f1 <= f2)
        allow_floor = TRUE;

    /* Require at least one legal choice */
    if (!allow_inven && !allow_equip && !allow_floor)
    {
        /* Cancel p_ptr->command_see */
        p_ptr->command_see = FALSE;

        /* Oops */
        oops = TRUE;

        /* Done */
        done = TRUE;
    }

    /* Analyze choices */
    else
    {
        /* Hack -- Start on equipment if requested */
        if (p_ptr->command_see && (p_ptr->command_wrk == (USE_EQUIP))
            && use_equip)
        {
            p_ptr->command_wrk = (USE_EQUIP);
        }

        /* Use inventory if allowed */
        else if (use_inven)
        {
            p_ptr->command_wrk = (USE_INVEN);
        }

        /* Use equipment if allowed */
        else if (use_equip)
        {
            p_ptr->command_wrk = (USE_EQUIP);
        }

        /* Use floor if allowed */
        else if (use_floor)
        {
            p_ptr->command_wrk = (USE_FLOOR);
        }

        /* Hack -- Use (empty) inventory */
        else
        {
            p_ptr->command_wrk = (USE_INVEN);
        }
    }

    /* Option to always show a list */
    if (auto_display_lists)
    {
        p_ptr->command_see = TRUE;
    }

    /* Start out in "display" mode */
    if (p_ptr->command_see)
    {
        /* Save screen */
        screen_save();
    }

    /* Repeat until done */
    while (!done)
    {
        int ni = 0;
        int ne = 0;

        /* Scan windows */
        for (j = 0; j < ANGBAND_TERM_MAX; j++)
        {
            /* Unused */
            if (!angband_term[j])
                continue;

            /* Count windows displaying inven */
            if (op_ptr->window_flag[j] & (PW_INVEN))
                ni++;

            /* Count windows displaying equip */
            if (op_ptr->window_flag[j] & (PW_EQUIP))
                ne++;
        }

        /* Toggle if needed */
        if (((p_ptr->command_wrk == (USE_EQUIP)) && ni && !ne)
            || ((p_ptr->command_wrk == (USE_INVEN)) && !ni && ne))
        {
            /* Toggle */
            toggle_inven_equip();

            /* Track toggles */
            toggle = !toggle;
        }

        /* Update */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);

        /* Redraw windows */
        window_stuff();

        /* Viewing inventory */
        if (p_ptr->command_wrk == (USE_INVEN))
        {
            /* Redraw if needed */
            if (p_ptr->command_see)
                show_inven();

            /* Begin the prompt */
            sprintf(out_val, "Inven:");

            /* List choices */
            if (i1 <= i2)
            {
                /* Build the prompt */
                sprintf(
                    tmp_val, " %c-%c,", index_to_label(i1), index_to_label(i2));

                /* Append */
                my_strcat(out_val, tmp_val, sizeof(out_val));
            }

            /* Indicate ability to "view" */
            if (!p_ptr->command_see)
                my_strcat(out_val, " * to see,", sizeof(out_val));

            /* Indicate legality of "toggle" */
            if (use_equip)
                my_strcat(out_val, " / for Equip,", sizeof(out_val));

            /* Indicate legality of the "floor" */
            if (allow_floor)
                my_strcat(out_val, " - for floor,", sizeof(out_val));
        }

        /* Viewing equipment */
        else if (p_ptr->command_wrk == (USE_EQUIP))
        {
            /* Redraw if needed */
            if (p_ptr->command_see)
                show_equip();

            /* Begin the prompt */
            sprintf(out_val, "Equip:");

            /* List choices */
            if (e1 <= e2)
            {
                /* Build the prompt */
                sprintf(
                    tmp_val, " %c-%c,", index_to_label(e1), index_to_label(e2));

                /* Append */
                my_strcat(out_val, tmp_val, sizeof(out_val));
            }

            /* Indicate ability to "view" */
            if (!p_ptr->command_see)
                my_strcat(out_val, " * to see,", sizeof(out_val));

            /* Indicate legality of "toggle" */
            if (use_inven)
                my_strcat(out_val, " / for Inven,", sizeof(out_val));

            /* Indicate legality of the "floor" */
            if (allow_floor)
                my_strcat(out_val, " - for floor,", sizeof(out_val));
        }

        /* Viewing floor */
        else
        {
            /* Redraw if needed */
            if (p_ptr->command_see)
                show_floor(floor_list, floor_num);

            /* Begin the prompt */
            sprintf(out_val, "Floor:");

            /* List choices */
            if (f1 <= f2)
            {
                /* Build the prompt */
                sprintf(tmp_val, " %c-%c,", I2A(f1), I2A(f2));

                /* Append */
                my_strcat(out_val, tmp_val, sizeof(out_val));
            }

            /* Indicate ability to "view" */
            if (!p_ptr->command_see)
                my_strcat(out_val, " * to see,", sizeof(out_val));

            /* Append */
            if (use_inven)
                my_strcat(out_val, " / for Inven,", sizeof(out_val));

            /* Append */
            else if (use_equip)
                my_strcat(out_val, " / for Equip,", sizeof(out_val));
        }

        /* Finish the prompt */
        my_strcat(out_val, " ESC", sizeof(out_val));

        /* Build the prompt */
        strnfmt(tmp_val, sizeof(tmp_val), "(%s) %s", out_val, pmt);

        /* Show the prompt */
        prt(tmp_val, 0, 0);

        /* Get a key */
        which = inkey();

        /* Parse it */
        switch (which)
        {
        case ESCAPE:
        {
            done = TRUE;
            break;
        }

        case '*':
        case '?':
        case ' ':
        {
            /* Hide the list */
            if (p_ptr->command_see)
            {
                /* Flip flag */
                p_ptr->command_see = FALSE;

                /* Load screen */
                screen_load();
            }

            /* Show the list */
            else
            {
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = TRUE;
            }

            break;
        }

        case '/':
        {
            /* Toggle to inventory */
            if (use_inven && (p_ptr->command_wrk != (USE_INVEN)))
            {
                p_ptr->command_wrk = (USE_INVEN);
            }

            /* Toggle to equipment */
            else if (use_equip && (p_ptr->command_wrk != (USE_EQUIP)))
            {
                p_ptr->command_wrk = (USE_EQUIP);
            }

            /* No toggle allowed */
            else
            {
                bell("Cannot switch item selector!");
                break;
            }

            /* Hack -- Fix screen */
            if (p_ptr->command_see)
            {
                /* Load screen */
                screen_load();

                /* Save screen */
                screen_save();
            }

            /* Need to redraw */
            break;
        }

        case '-':
        {
            /* Paranoia */
            if (!allow_floor)
            {
                bell("Cannot select floor!");
                break;
            }

            /* Check each legal object */
            for (i = 0; i < floor_num; ++i)
            {
                /* Special index */
                k = 0 - floor_list[i];

                /* Skip non-okay objects */
                if (!get_item_okay(k))
                    continue;

                /* Allow player to "refuse" certain actions */
                if (!get_item_allow(k))
                    continue;

                /* Accept that choice */
                (*cp) = k;
                item = TRUE;
                done = TRUE;
                break;
            }

            break;
        }

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
            /* Look up the tag */
            if (!get_tag(&k, which))
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Hack -- Validate the item */
            if ((k < INVEN_WIELD) ? !allow_inven : !allow_equip)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = TRUE;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = TRUE;
            done = TRUE;
            break;
        }

        case '[':
        case ']':
        {
            bool item_found = FALSE;

            /* Convert letter to inventory index */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                for (i = INVEN_PACK; i >= 0; i--)
                {
                    if (get_item_okay(i) && ((which == '[') || !item_found))
                    {
                        k = i;
                        item_found = TRUE;
                    }
                }
            }

            /* Convert letter to equipment index */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
                {
                    if (get_item_okay(i) && ((which == ']') || !item_found))
                    {
                        k = i;
                        item_found = TRUE;
                    }
                }
            }

            /* Hack -- Validate the item */
            if ((k < INVEN_WIELD) ? !allow_inven : !allow_equip)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Validate the item */
            if (!item_found)
            {
                bell("No valid items found.");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = TRUE;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = TRUE;
            done = TRUE;
            break;
        }

        case '\n':
        case '\r':
        {
            /* Choose "default" inventory item */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                if (i1 != i2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = i1;
            }

            /* Choose "default" equipment item */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                if (e1 != e2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = e1;
            }

            /* Choose "default" floor item */
            else
            {
                if (f1 != f2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = 0 - floor_list[f1];
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (default)!");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = TRUE;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = TRUE;
            done = TRUE;
            break;
        }

        default:
        {
            bool verify;

            /* Note verify */
            verify = (isupper((unsigned char)which) ? TRUE : FALSE);

            /* Lowercase */
            which = tolower((unsigned char)which);

            /* Convert letter to inventory index */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                k = label_to_inven(which);

                if (k < 0)
                {
                    bell("Illegal object choice (inven)!");
                    break;
                }
            }

            /* Convert letter to equipment index */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                k = label_to_equip(which);

                if (k < 0)
                {
                    bell("Illegal object choice (equip)!");
                    break;
                }
            }

            /* Convert letter to floor index */
            else
            {
                k = (islower((unsigned char)which) ? A2I(which) : -1);

                if (k < 0 || k >= floor_num)
                {
                    bell("Illegal object choice (floor)!");
                    break;
                }

                /* Special index */
                k = 0 - floor_list[k];
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (normal)!");
                break;
            }

            /* Verify the item */
            if (verify && !verify_item("Try", k))
            {
                done = TRUE;
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = TRUE;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = TRUE;
            done = TRUE;
            break;
        }
        }
    }

    /* Fix the screen if necessary */
    if (p_ptr->command_see)
    {
        /* Load screen */
        screen_load();

        /* Hack -- Cancel "display" */
        p_ptr->command_see = FALSE;
    }

    // Forget whether inventory or equipment was being examined
    p_ptr->command_wrk = 0;

    // Forget whether inventory or equipment or floor or combinations were
    // examinable
    p_ptr->get_item_mode = 0;

    /* Forget the item_tester_tval restriction */
    item_tester_tval = 0;

    /* Forget the item_tester_hook restriction */
    item_tester_hook = NULL;

    /* Clean up */
    /* Toggle again if needed */
    if (toggle)
        toggle_inven_equip();

    /* Update */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Window stuff */
    window_stuff();

    /* Clear the prompt line */
    prt("", 0, 0);

    /* Warning if needed */
    if (oops && str)
        msg_print(str);

#ifdef ALLOW_REPEAT

    /* Save item if available */
    if (item)
        repeat_push(*cp);

#endif /* ALLOW_REPEAT */

    /* Result */
    return (item);
}
