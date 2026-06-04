/* File: spell/spell-monster.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

/*
 * Slow monsters
 */
bool slow_monsters(int power) { return (project_los(GF_SLOW, 0, 0, power, false)); }

/*
 * Sleep monsters
 */
bool sleep_monsters(int power) { return (project_los(GF_SLEEP, 0, 0, power, false)); }

/*
 * Wake up all monsters, and speed up "los" monsters.
 */
void wake_all_monsters(int who)
{
    int i;

    /* Aggravate everyone */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Skip aggravating monster (or player) */
        if (i == who)
            continue;

        // Alert it
        set_alertness(m_ptr, MAX(m_ptr->alertness, ALERTNESS_VERY_ALERT));

        /*possibly update the monster health bar*/
        if (p_ptr->health_who == i)
            p_ptr->redraw |= (PR_HEALTHBAR);
    }
}

/*
 * Set the aggressive flag on nearby monsters (using the sound metric).
 */
bool make_aggressive(void)
{
    int i;
    int notice = false;

    for (i = 1; i < mon_max; i++)
    {
        /* Check the i'th monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if ((m_ptr->alertness >= ALERTNESS_ALERT)
            && (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) <= 10))
        {
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

            // notice if the monster is visible
            if (m_ptr->ml)
                notice = true;

            if ((r_ptr->flags2 & (RF2_SMART))
                && ((r_ptr->flags1 & (RF1_FRIENDS))
                    || (r_ptr->flags1 & (RF1_FRIEND))
                    || (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
                    || (r_ptr->flags1 & (RF1_ESCORT))
                    || (r_ptr->flags1 & (RF1_ESCORTS))
                    || (r_ptr->flags4 & (RF4_SHRIEK))))
            {
                tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);

                // notice if you hear them shout
                notice = true;
            }
        }
    }

    return (notice);
}

/*
 * Delete all non-unique monsters of a given "type" from the level
 */
bool banishment(void)
{
    int i;

    char typ;

    /* Mega-Hack -- Get a monster symbol */
    if (!get_com("Choose a monster race (by symbol) to banish: ", &typ))
        return false;

    /* Delete the monsters of that "type" */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Skip Unique Monsters */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Skip "wrong" monsters */
        if (r_ptr->d_char != typ)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);

        /* Take some damage */
        killer_mark_other(SCORE_KILLER_SELF);
        take_hit(dieroll(4), "the strain of casting Banishment");
    }

    /* Success */
    return true;
}

/*
 * Delete all nearby (non-unique) monsters
 */
bool mass_banishment(void)
{
    int i;

    bool result = false;

    /* Delete the (nearby) monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Skip unique monsters */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Skip distant monsters */
        if (m_ptr->cdis > MAX_SIGHT)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);

        /* Take some damage */
        killer_mark_other(SCORE_KILLER_SELF);
        take_hit(dieroll(3), "the strain of casting Mass Banishment");

        /* Note effect */
        result = true;
    }

    return (result);
}

