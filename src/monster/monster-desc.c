/* File: monster-desc.c */

#include "monster-internal.h"

/*
 * Display visible monsters in a window
 */
void display_monlist(void)
{
    int idx, n;
    int line = 0;

    char* m_name;
    char buf[80];

    monster_type* m_ptr;
    monster_race* r_ptr;

    u16b* race_counts;

    if (p_ptr->image)
    {
        /* Erase the rest of the window */
        for (idx = 0; idx < Term->hgt; idx++)
        {
            /* Erase the line */
            Term_erase(0, idx, 255);
        }
        Term_putstr(
            3, 3, 35, TERM_L_WHITE, "What you see is not to be believed.");

        return;
    }

    /* Allocate the array */
    race_counts = mem_alloc_array(z_info->r_max, u16b);

    /* Iterate over mon_list */
    for (idx = 1; idx < mon_max; idx++)
    {
        m_ptr = &mon_list[idx];

        /* Only visible monsters */
        if (!m_ptr->ml)
            continue;

        /* Bump the count for this race */
        race_counts[m_ptr->r_idx]++;
    }

    /* Iterate over mon_list ( again :-/ ) */
    for (idx = 1; idx < mon_max; idx++)
    {
        m_ptr = &mon_list[idx];

        n = 0;

        /* Only visible monsters */
        if (!m_ptr->ml)
            continue;

        /* Do each race only once */
        if (!race_counts[m_ptr->r_idx])
            continue;

        /* Get monster race */
        r_ptr = &r_info[m_ptr->r_idx];

        // Start a line
        Term_putstr(0, line, 1, TERM_WHITE, " ");

        /* Append the "standard" attr/char info */
        Term_addch(r_ptr->d_attr, r_ptr->d_char);

        n += 2;

        if (use_graphics)
        {
            /* Append the "optional" attr/char info */
            Term_addstr(-1, TERM_WHITE, " / ");

            Term_addch(r_ptr->x_attr, r_ptr->x_char);
            n += 4;

            if (use_bigtile)
            {
                if (r_ptr->x_attr & 0x80)
                    Term_addch(255, -1);
                else
                    Term_addch(0, ' ');

                n++;
            }
        }

        /* Add race count */
        sprintf(buf, "%3d  ", race_counts[m_ptr->r_idx]);
        Term_addstr(strlen(buf), TERM_WHITE, buf);
        n += 5;

        /* Don't do this race again */
        race_counts[m_ptr->r_idx] = 0;

        /* Get the monster name */
        m_name = r_name + r_ptr->name;

        /* Obtain the length of the description */
        n += strlen(m_name);

        /* Display the entry itself */
        Term_addstr(strlen(m_name), TERM_WHITE, m_name);

        /* Erase the rest of the line */
        Term_erase(n, line, 255);

        /* Bump line counter */
        line++;
    }

    /* Free the race counters */
    mem_free_null(race_counts);

    /* Erase the rest of the window */
    for (idx = line; idx < Term->hgt; idx++)
    {
        /* Erase the line */
        Term_erase(0, idx, 255);
    }
}

/*
 * Build a string describing a monster in some way.
 *
 * We can correctly describe monsters based on their visibility.
 * We can force all monsters to be treated as visible or invisible.
 * We can build nominatives, objectives, possessives, or reflexives.
 * We can selectively pronominalize hidden, visible, or all monsters.
 * We can use definite or indefinite descriptions for hidden monsters.
 * We can use definite or indefinite descriptions for visible monsters.
 *
 * Pronominalization involves the gender whenever possible and allowed,
 * so that by cleverly requesting pronominalization / visibility, you
 * can get messages like "You hit someone.  She screams in agony!".
 *
 * Reflexives are acquired by requesting Objective plus Possessive.
 *
 * I am assuming that no monster name is more than 65 characters long,
 * so that "char desc[80];" is sufficiently large for any result, even
 * when the "offscreen" notation is added.
 *
 * Note that the "possessive" for certain unique monsters will look
 * really silly, as in "Morgoth, Lord of Darkness's".  We should
 * perhaps add a flag to "remove" any "descriptives" in the name.
 *
 * Note that "offscreen" monsters will get a special "(offscreen)"
 * notation in their name if they are visible but offscreen.  This
 * may look silly with possessives, as in "the rat's (offscreen)".
 * Perhaps the "offscreen" descriptor should be abbreviated.
 *
 * Mode Flags:
 *   0x01 --> Objective (or Reflexive)
 *   0x02 --> Possessive (or Reflexive)
 *   0x04 --> Use indefinites for hidden monsters ("something")
 *   0x08 --> Use indefinites for visible monsters ("a kobold")
 *   0x10 --> Pronominalize hidden monsters
 *   0x20 --> Pronominalize visible monsters
 *   0x40 --> Assume the monster is hidden
 *   0x80 --> Assume the monster is visible
 *
 * Useful Modes:
 *   0x00 --> Full nominative name ("the kobold") or "it"
 *   0x04 --> Full nominative name ("the kobold") or "something"
 *   0x80 --> Banishment resistance name ("the kobold")
 *   0x88 --> Killing name ("a kobold")
 *   0x22 --> Possessive, genderized if visable ("his") or "its"
 *   0x23 --> Reflexive, genderized if visable ("himself") or "itself"
 */
void monster_desc(char* desc, size_t max, const monster_type* m_ptr, int mode)
{
    cptr res;
    monster_race* r_ptr;
    cptr name;
    bool seen, pron;

    if (p_ptr->image)
    {
        r_ptr = &r_info[m_ptr->image_r_idx];
    }
    else
    {
        r_ptr = &r_info[m_ptr->r_idx];
    }

    name = (r_name + r_ptr->name);

    /* Can we "see" it (forced, or not hidden + visible) */
    seen = ((mode & (0x80)) || (!(mode & (0x40)) && m_ptr->ml));

    /* Sexed Pronouns (seen and forced, or unseen and allowed) */
    pron = ((seen && (mode & (0x20))) || (!seen && (mode & (0x10))));

    /* First, try using pronouns, or describing hidden monsters */
    if (!seen || pron)
    {
        /* an encoding of the monster "sex" */
        int kind = 0x00;

        /* Extract the gender (if applicable) */
        if (r_ptr->flags1 & (RF1_FEMALE))
            kind = 0x20;
        else if (r_ptr->flags1 & (RF1_MALE))
            kind = 0x10;

        /* Ignore the gender (if desired) */
        if (!m_ptr || !pron)
            kind = 0x00;

        /* Assume simple result */
        res = "it";

        /* Brute force: split on the possibilities */
        switch (kind + (mode & 0x07))
        {
        /* Neuter, or unknown */
        case 0x00:
            res = "it";
            break;
        case 0x01:
            res = "it";
            break;
        case 0x02:
            res = "its";
            break;
        case 0x03:
            res = "itself";
            break;
        case 0x04:
            res = "something";
            break;
        case 0x05:
            res = "something";
            break;
        case 0x06:
            res = "something's";
            break;
        case 0x07:
            res = "itself";
            break;

        /* Male (assume human if vague) */
        case 0x10:
            res = "he";
            break;
        case 0x11:
            res = "him";
            break;
        case 0x12:
            res = "his";
            break;
        case 0x13:
            res = "himself";
            break;
        case 0x14:
            res = "someone";
            break;
        case 0x15:
            res = "someone";
            break;
        case 0x16:
            res = "someone's";
            break;
        case 0x17:
            res = "himself";
            break;

        /* Female (assume human if vague) */
        case 0x20:
            res = "she";
            break;
        case 0x21:
            res = "her";
            break;
        case 0x22:
            res = "her";
            break;
        case 0x23:
            res = "herself";
            break;
        case 0x24:
            res = "someone";
            break;
        case 0x25:
            res = "someone";
            break;
        case 0x26:
            res = "someone's";
            break;
        case 0x27:
            res = "herself";
            break;
        }

        /* Copy the result */
        SDL_strlcpy(desc, res, max);
    }

    /* Handle visible monsters, "reflexive" request */
    else if ((mode & 0x02) && (mode & 0x01))
    {
        /* The monster is visible, so use its gender */
        if (r_ptr->flags1 & (RF1_FEMALE))
            SDL_strlcpy(desc, "herself", max);
        else if (r_ptr->flags1 & (RF1_MALE))
            SDL_strlcpy(desc, "himself", max);
        else
            SDL_strlcpy(desc, "itself", max);
    }

    /* Handle all other visible monster requests */
    else
    {
        /* It could be a Unique */
        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Start with the name (thus nominative and objective) */
            SDL_strlcpy(desc, name, max);
        }

        /* It could be an indefinite monster */
        else if (mode & 0x08)
        {
            /* XXX Check plurality for "some" */

            /* Indefinite monsters need an indefinite article */
            SDL_strlcpy(desc, is_a_vowel(name[0]) ? "an " : "a ", max);
            SDL_strlcat(desc, name, max);
        }

        /* It could be a normal, definite, monster */
        else
        {
            /* Definite monsters need a definite article */
            SDL_strlcpy(desc, "the ", max);
            SDL_strlcat(desc, name, max);
        }

        /* Handle the Possessive as a special afterthought */
        if (mode & 0x02)
        {
            /* XXX Check for trailing "s" */

            /* Simply append "apostrophe" and "s" */
            SDL_strlcat(desc, "'s", max);
        }

        /* Mention "offscreen" monsters XXX XXX */
        if (!panel_contains(m_ptr->fy, m_ptr->fx))
        {
            /* Append special notation */
            SDL_strlcat(desc, " (offscreen)", max);
        }
    }
}

/*
 * Build a string describing a monster race, currently used for quests.
 *
 * Assumes a singular monster.  This may need to be run through the
 * plural_aux function in the quest.c file.  (Changes "wolf" to
 * wolves, etc.....)
 *
 * I am assuming that no monster name is more than 65 characters long,
 * so that "char desc[80];" is sufficiently large for any result, even
 * when the "offscreen" notation is added.
 *
 */
void monster_desc_race(char* desc, size_t max, int r_idx)
{
    monster_race* r_ptr = &r_info[r_idx];

    cptr name = (r_name + r_ptr->name);

    /* Write the name */
    SDL_strlcpy(desc, name, max);
}

/*
 * Take note that the given monster just dropped some treasure
 *
 * Note that learning the "CHEST/GOOD"/"GREAT" flags gives information
 * about the treasure (even when the monster is killed for the first
 * time, such as uniques, and the treasure has not been examined yet).
 *
 * This "indirect" method is used to prevent the player from learning
 * exactly how much treasure a monster can drop from observing only
 * a single example of a drop.  This method actually observes how many
 * items are dropped, and remembers that information to be
 * described later by the monster recall code.
 */
void lore_treasure(int m_idx, int num_item)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    /* Note the number of things dropped */
    if (num_item > l_ptr->drop_item)
        l_ptr->drop_item = num_item;

    /* Hack -- memorize the chest/good/great/superb/artefact flags */
    if (r_ptr->flags1 & (RF1_DROP_CHEST))
        l_ptr->flags1 |= (RF1_DROP_CHEST);
    if (r_ptr->flags1 & (RF1_DROP_GOOD))
        l_ptr->flags1 |= (RF1_DROP_GOOD);
    if (r_ptr->flags1 & (RF1_DROP_GREAT))
        l_ptr->flags1 |= (RF1_DROP_GREAT);
    if (r_ptr->flags2 & (RF2_DROP_SUPERB))
        l_ptr->flags2 |= (RF2_DROP_SUPERB);
    if (r_ptr->flags3 & (RF3_DROP_ARTEFACT))
        l_ptr->flags3 |= (RF3_DROP_ARTEFACT);

    /* Update monster recall window */
    if (p_ptr->monster_race_idx == m_ptr->r_idx)
    {
        /* Window stuff */
        p_ptr->window |= (PW_MONSTER);
    }
}

