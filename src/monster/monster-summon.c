/* File: monster-summon.c */

#include "monster-internal.h"

/*
 * Hack -- the "type" of the current "summon specific"
 */
static int summon_specific_type = 0;

/*
 * Hack -- help decide if a monster race is "okay" to summon
 */
static bool summon_specific_okay(int r_idx)
{
    monster_race* r_ptr = &r_info[r_idx];

    bool okay = false;

    /* Hack -- no specific type specified */
    if (!summon_specific_type)
        return (true);

    /* Check our requirements */
    switch (summon_specific_type)
    {
    case SUMMON_ANT:
    {
        okay = false;
        break;
    }

    case SUMMON_SPIDER:
    {
        okay = ((r_ptr->d_char == 'M') && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_HOUND:
    {
        okay = ((r_ptr->d_char == 'C') && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_BIRD_BAT:
    {
        okay = ((r_ptr->d_char == 'b') && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_AINU:
    {
        okay = false;
        break;
    }

    case SUMMON_RAUKO:
    {
        okay = ((r_ptr->flags3 & (RF3_RAUKO))
            && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_UNDEAD:
    {
        okay = ((r_ptr->flags3 & (RF3_UNDEAD))
            && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_DRAGON:
    {
        okay = ((r_ptr->flags3 & (RF3_DRAGON))
            && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_HI_DEMON:
    {
        okay = ((r_ptr->flags3 & (RF3_RAUKO))
            && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_HI_UNDEAD:
    {
        okay = ((r_ptr->flags3 & (RF3_UNDEAD))
            && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_HI_DRAGON:
    {
        okay = (r_ptr->d_char == 'D');
        break;
    }

    case SUMMON_WRAITH:
    {
        okay = ((r_ptr->d_char == 'W') && (r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_UNIQUE:
    {
        if ((r_ptr->flags1 & (RF1_UNIQUE)) != 0)
            okay = true;
        break;
    }

    case SUMMON_HI_UNIQUE:
    {
        if (((r_ptr->flags1 & (RF1_UNIQUE)) != 0)
            && (r_ptr->level > (MORGOTH_DEPTH / 2)))
            okay = true;
        break;
    }

    case SUMMON_KIN:
    {
        okay = ((r_ptr->d_char == summon_kin_type)
            && !(r_ptr->flags1 & (RF1_UNIQUE)));
        break;
    }

    case SUMMON_ANIMAL:
    {
        okay = false;
        break;
    }

    case SUMMON_BERTBILLTOM:
    {
        okay = false;
        break;
    }

    case SUMMON_THIEF:
    {
        okay = false;
        break;
    }

    default:
    {
        break;
    }
    }

    /* Result */
    return (okay);
}

/*
 * Place a monster (of the specified "type") near the given
 * location.  Return true if a monster was actually summoned.
 *
 * We will attempt to place the monster up to 20 times before giving up.
 *
 * Note: SUMMON_UNIQUE and SUMMON_WRAITH (XXX) will summon Uniques
 * Note: SUMMON_HI_UNDEAD and SUMMON_HI_DRAGON may summon Uniques
 * Note: None of the other summon codes will ever summon Uniques.
 *
 * We usually do not summon monsters greater than the given depth.  -LM-
 *
 * Note that we use the new "monster allocation table" creation code
 * to restrict the "get_mon_num()" function to the set of "legal"
 * monsters, making this function much faster and more reliable.
 *
 * Note that this function may not succeed, though this is very rare.
 */
bool summon_specific(int y1, int x1, int lev, int type)
{
    int i, x, y, r_idx;

    bool (*get_mon_num_hook_temp)(int r_idx) = get_mon_num_hook;

    /* Look for a location */
    for (i = 0; i < 20; ++i)
    {
        /* Pick a distance */
        int d = (i / 15) + 1;

        /* Pick a location */
        scatter(&y, &x, y1, x1, d, 0);

        /* Require "empty" floor grid */
        if (!cave_empty_bold(y, x))
            continue;

        /* Hack -- no summon on glyph of warding */
        if (cave_glyph(y, x))
            continue;

        /* Okay */
        break;
    }

    /* Failure */
    if (i == 20)
        return (false);

    /* Save the "summon" type */
    summon_specific_type = type;

    /* Require "okay" monsters */
    get_mon_num_hook = summon_specific_okay;

    /* Prepare allocation table */
    get_mon_num_prep();

    /* Pick a monster, using the given level */
    r_idx = get_mon_num(lev, false, true, false);

    /* Restore the previous hook */
    get_mon_num_hook = get_mon_num_hook_temp;

    /* Prepare allocation table */
    get_mon_num_prep();

    /* Handle failure */
    if (!r_idx)
        return (false);

    /* Attempt to place the monster (awake, allow groups) */
    if (!place_monster_aux(y, x, r_idx, false, true))
        return (false);

    /* Success */
    return (true);
}

/*
 * Let the given monster attempt to reproduce.
 *
 * Note that "reproduction" REQUIRES empty space.
 */
bool reproduce_monster(int old_m_idx, int new_r_idx)
{
    monster_type* old_m_ptr = &mon_list[old_m_idx];
    monster_race* new_r_ptr = &r_info[new_r_idx];

    int i, y, x;

    bool result = false;

    u16b grid[8];
    int grids = 0;

    /* Scan the adjacent floor grids */
    for (i = 0; i < 8; i++)
    {
        y = old_m_ptr->fy + ddy_ddd[i];
        x = old_m_ptr->fx + ddx_ddd[i];

        /* Must be fully in bounds */
        if (!in_bounds_fully(y, x))
            continue;

        /* This grid is OK for this monster (should monsters be able to dig?) */
        if (cave_exist_mon(new_r_ptr, y, x, false, false))
        {
            /* Save this grid */
            grid[grids++] = GRID(y, x);
        }
    }

    /* No grids available */
    if (!grids)
        return (false);

    /* Pick a grid at random */
    i = rand_int(grids);

    /* Get the coordinates */
    y = GRID_Y(grid[i]);
    x = GRID_X(grid[i]);

    /* Create a new monster (awake, no groups) */
    result = place_monster_aux(y, x, new_r_idx, false, false);

    /* Result */
    return (result);
}

/*
 * Dump a message describing a monster's reaction to damage.
 *
 * Historically, this function gave a description (visual or auditory) of
 * a monster's reaction in order to give you an idea of their health level.
 *
 * Now it only gives a message if the monster is unseen, and the primary
 * purpose is to show that there is indeed a monster in the dark corridor
 * getting hurt.
 *
 * Note that while the monsters 'cry out', it doesn't wake any monsters or
 * anything, as the idea is that it makes no more noise than regular melee
 * combat. It is just that in melee combat, we wouldn't want to spam up the
 * screen with messages about noises.
 */
void message_pain(int m_idx, int dam)
{
    long oldhp, newhp, tmp;
    int percentage;

    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    char m_name[80];

    // Ignore the monster if it is visible
    if (m_ptr->ml)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Note -- subtle fix -CFT */
    newhp = (long)(m_ptr->hp);
    oldhp = newhp + (long)(dam);
    tmp = (newhp * 100L) / oldhp;
    percentage = (int)(tmp);

    /* Wolves */
    if (strchr("C", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a snarl.");
        else if (percentage > 33)
            msg_print("You hear a yelp.");
        else
            msg_print("You hear a feeble yelp.");
    }

    /* Serpents, Dragons, Centipedes */
    else if (strchr("sScdD", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a hiss.");
        else if (percentage > 33)
            msg_print("You hear a furious hissing.");
        else
            msg_print("You hear thrashing about.");
    }

    /* Felines */
    else if (strchr("f", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a feline snarl.");
        else if (percentage > 33)
            msg_print("You hear a mewling sound.");
        else
            msg_print("You hear a pitiful mewling.");
    }

    /* Insects, Spiders */
    else if (strchr("IM", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear an angry droning.");
        else if (percentage > 33)
            msg_print("You hear a scuttling sound.");
        else
            msg_print("You hear a skittering sound.");
    }

    /* Birds, Bats, Vampires */
    else if (strchr("bv", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a squeal.");
        else if (percentage > 33)
            msg_print("You hear a shrieks.");
        else
            msg_print("You hear erratic fluttering.");
    }

    /* Humanoid monsters */
    else if (strchr("@oTGV", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a grunt.");
        else if (percentage > 33)
            msg_print("You hear a cry of pain.");
        else
            msg_print("You hear a feeble cry.");
    }

    /* Some other monsters */
    else if (strchr("HRN", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a strange grunt.");
        else if (percentage > 33)
            msg_print("You hear a terrible cry.");
        else
            msg_print("You hear a unnatural cry.");
    }

    // m, w are silent
}



