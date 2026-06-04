#include "angband.h"
#include "externs.h"
#include "melee/melee-movement.h"
#include "melee/melee-util.h"

int get_scent(int y, int x)
{
    int age;
    int scent;

    /* Check Bounds */
    if (!(in_bounds(y, x)))
        return (-1);

    /* Sent trace? */
    scent = cave_when[y][x];

    /* No scent at all */
    if (!scent)
        return (-1);

    /* Get age of scent */
    age = scent - scent_when;

    if (age > SMELL_STRENGTH)
        return (-1);

    /* Return the age of the scent */
    return (age);
}

bool cave_exist_mon(
    monster_race* r_ptr, int y, int x, bool occupied_ok, bool can_dig)
{
    int feat;

    /* Check Bounds */
    if (!in_bounds(y, x))
        return (false);

    /* Check location */
    feat = cave_feat[y][x];

    /* The grid is already occupied. */
    if (cave_m_idx[y][x] != 0)
    {
        if (!occupied_ok)
            return (false);
    }

    /* Glyphs -- must break first */
    if (cave_glyph(y, x))
        return (false);

    /*** Check passability of various features. ***/

    // only flying creatures can pass chasms
    if (cave_feat[y][x] == FEAT_CHASM)
    {
        if (r_ptr->flags2 & (RF2_FLYING))
            return (true);
        else
            return (false);
    }

    /* Feature is not a wall */
    if (!(cave_info[y][x] & (CAVE_WALL)))
        return (true);

    /* Feature is a wall */
    else
    {
        /* Permanent walls are never OK */
        if (feat == FEAT_WALL_PERM)
            return (false);

        /* Monster can pass through walls */
        if (r_ptr->flags2 & (RF2_PASS_WALL))
            return (true);

        /* Monster can bore through walls, and is allowed to. */
        if ((r_ptr->flags2 & (RF2_KILL_WALL)) && (can_dig))
            return (true);

        /* Monster can dig through walls, and is allowed to. */
        if ((r_ptr->flags2 & (RF2_TUNNEL_WALL)) && (can_dig))
            return (true);

        // Some monsters can pass under doors
        if (cave_any_closed_door_bold(y, x)
            && (r_ptr->flags2 & (RF2_PASS_DOOR)))
        {
            return (true);
        }

        else
            return (false);
    }
}

/*
 * Can the monster enter this grid?  How easy is it for them to do so?
 *
 * Returns the percentage chance of success.
 *
 * The code that uses this function sometimes assumes that it will never
 * return a value greater than 100.
 *
 * The usage of level to determine whether one monster can push past
 * another is a tad iffy, but ensures that orc soldiers can always
 * push past other orc soldiers.
 */
int cave_passable_mon(monster_type* m_ptr, int y, int x, bool* bash)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Assume nothing in the grid other than the terrain hinders movement */
    int move_chance = 100;

    int feat;

    /* Check Bounds */
    if (!in_bounds(y, x))
        return (0);

    /* Check location */
    feat = cave_feat[y][x];

    /* Permanent walls are never passable */
    if (feat == FEAT_WALL_PERM)
        return (0);

    /* The grid is occupied by the player. */
    if (cave_m_idx[y][x] < 0)
    {
        /* Monster has no melee blows - character's grid is off-limits. */
        if (r_ptr->flags1 & (RF1_NEVER_BLOW))
            return (0);

        /* Any monster with melee blows can attack the character. */
        else
            return (100);
    }

    /* The grid is occupied by a monster. */
    else if (cave_m_idx[y][x] > 0)
    {
        monster_type* n_ptr = &mon_list[cave_m_idx[y][x]];
        monster_race* nr_ptr = &r_info[n_ptr->r_idx];

        /* Some creatures can kill weaker monsters */
        if ((r_ptr->flags2 & (RF2_KILL_BODY))
            && (!(nr_ptr->flags1 & (RF1_UNIQUE)))
            && (r_ptr->level > nr_ptr->level))
        {
            move_chance = 100;
        }

        /* All can attempt to push past monsters that can move */
        else if (!(nr_ptr->flags1 & (RF1_NEVER_MOVE))
            && !(nr_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            // It is easy to push past unwary or sleeping monsters
            if ((n_ptr->alertness < ALERTNESS_ALERT)
                && (m_ptr->wandering_idx != n_ptr->wandering_idx))
                move_chance = 80;

            // It is easy for non-fleeing monsters to push past fleeing monsters
            else if ((n_ptr->stance == STANCE_FLEEING)
                && (m_ptr->stance != STANCE_FLEEING))
                move_chance = 80;

            // It is easy for fleeing monsters to push past non-fleeing monsters
            else if ((n_ptr->stance != STANCE_FLEEING)
                && (m_ptr->stance == STANCE_FLEEING))
                move_chance = 80;

            // It is easy to push past weaker monsters
            else if (r_ptr->level > nr_ptr->level)
                move_chance = 80;

            // It is quite hard to push past monsters of equal strength
            else if (r_ptr->level == nr_ptr->level)
                move_chance = 20;

            // It is very difficult to move past alert, unafraid, stronger
            // monsters
            else
                move_chance = 10;
        }

        /* Cannot do anything to clear away the other monster */
        else
            return (0);
    }

    /* Glyphs */
    if (feat == FEAT_GLYPH)
    {
        // a simulated Will check
        int difficulty = (c_info[p_ptr->pcharacter].flags & UNQ_SNG_MEL) ? 40 : 20;
        int break_chance = success_chance(10, monster_skill(m_ptr, S_WIL), difficulty);

        // can always attack the player if the player is standing on the glyph
        if ((p_ptr->py == y) && (p_ptr->px == x))
            break_chance = 100;

        // unwary monsters won't break glyphs
        if (m_ptr->alertness < ALERTNESS_ALERT)
            break_chance = 0;

        /* Glyphs are hard to break */
        if (move_chance > break_chance)
            move_chance = break_chance;
    }
    // only flying creatures can pass chasms
    else if (feat == FEAT_CHASM)
    {
        if (!(r_ptr->flags2 & (RF2_FLYING)))
            return (0);
    }
    // Light sensitive creatures and undead cannot pass sunlight
    else if (feat == FEAT_SUNLIGHT)
    {
        if ((r_ptr->flags3 & (RF3_HURT_LITE)) || (r_ptr->flags3 & (RF3_UNDEAD)))
            return (0);
    }

    /*** Check passability of various features. ***/

    /* Feature is not a wall */
    if (!(cave_info[y][x] & (CAVE_WALL)))
    {
        /* Any monster can handle floors, except glyphs and chasms, which are
         * handled above */
        return (move_chance);
    }

    /* Feature is a 'wall', including doors */
    else
    {
        /* Granite, Quartz, Rubble */
        if (((feat >= FEAT_QUARTZ) && (feat <= FEAT_WALL_SOLID))
            || (feat == FEAT_RUBBLE))
        {
            /* Impassible except for monsters that move through walls */
            if ((r_ptr->flags2 & (RF2_PASS_WALL))
                || (r_ptr->flags2 & (RF2_KILL_WALL)))
                return (move_chance);

            // alert unafraid monsters can slowly tunnel through walls
            else if ((r_ptr->flags2 & (RF2_TUNNEL_WALL))
                && (m_ptr->alertness >= ALERTNESS_ALERT)
                && (m_ptr->stance != STANCE_FLEEING))
                return (move_chance);

            else
                return (0);
        }

        /* Doors */
        if (cave_any_closed_door_bold(y, x))
        {
            int unlock_chance = 0;
            int bash_chance = 0;

            // monsters don't open doors in the tutorial mode
            // if (p_ptr->game_type < 0)
            //{
            //	return (0);
            //}

            // Some monsters can simply pass through doors
            if ((r_ptr->flags2 & (RF2_PASS_DOOR)
                    || (r_ptr->flags2 & (RF2_PASS_WALL)))
                && !cave_glyph(y, x))
            {
                return (move_chance);
            }

            // unwary monsters won't open doors in vaults or interesting rooms
            if ((m_ptr->alertness < ALERTNESS_ALERT)
                && (cave_info[y][x] & (CAVE_ICKY)))
            {
                return (0);
            }

            // no monsters will open secret doors in vaults or interesting rooms
            if ((cave_feat[y][x] == FEAT_SECRET)
                && (cave_info[y][x] & (CAVE_ICKY)))
            {
                return (0);
            }

            /* Monster can open doors */
            if (r_ptr->flags2 & (RF2_OPEN_DOOR))
            {
                if (feat == FEAT_WARDED)
                {
                    // simulated Will check
                    unlock_chance
                        = success_chance(10, monster_skill(m_ptr, S_WIL), 20);
                }
                else if (feat == FEAT_WARDED2)
                {
                    // simulated Will check
                    unlock_chance
                        = success_chance(10, monster_skill(m_ptr, S_WIL), 25);
                }
                else if (feat == FEAT_WARDED3)
                {
                    // simulated Will check
                    unlock_chance
                        = success_chance(10, monster_skill(m_ptr, S_WIL), 30);
                }
                /* Closed doors and secret doors */
                else if ((feat == FEAT_DOOR_HEAD) || (feat == FEAT_SECRET))
                {
                    /*
                     * Note:  This section will have to be rewritten if
                     * secret doors can be jammed or locked as well.
                     */

                    /*
                     * It usually takes two turns to open a door
                     * and move into the doorway.
                     */
                    return (move_chance);
                }

                /*
                 * Locked doors (not jammed).  Monsters know how hard
                 * doors in their neighborhood are to unlock.
                 */
                else if ((r_ptr->flags2 & (RF2_UNLOCK_DOOR))
                    && (feat < FEAT_DOOR_HEAD + 0x08))
                {
                    int difficulty, skill;

                    /* Lock difficulty (power + 5) */
                    difficulty = (feat - FEAT_DOOR_HEAD) + 5;

                    /* Unlocking skill equals monster perception */
                    skill = monster_skill(m_ptr, S_PER);

                    /*
                     * we ignore the fact that it takes extra time to
                     * open the door and walk into the entranceway.
                     */
                    unlock_chance = success_chance(10, skill, difficulty);
                }
            }

            /* Monster can bash doors */
            if (r_ptr->flags2 & (RF2_BASH_DOOR))
            {
                int difficulty, skill;

                if (feat == FEAT_WARDED)
                {
                    // simulated Will check
                    bash_chance
                        = success_chance(10, monster_skill(m_ptr, S_WIL), 20);
                }
                else if (feat == FEAT_WARDED2)
                {
                    // simulated Will check
                    bash_chance
                        = success_chance(10, monster_skill(m_ptr, S_WIL), 25);
                }
                else if (feat == FEAT_WARDED3)
                {
                    // simulated Will check
                    unlock_chance
                        = success_chance(10, monster_skill(m_ptr, S_WIL), 30);
                }
                else
                {
                    /* Door difficulty (power + 2) */
                    /*
                     * XXX - just because a door is difficult to unlock
                     * shouldn't mean that it's hard to bash.  Until the
                     * character door bashing code is changed, however,
                     * we'll stick with this.
                     */
                    difficulty = ((feat - FEAT_DOOR_HEAD) % 8);

                    /*
                     * Calculate bashing ability (ie effective strength)
                     */
                    skill = monster_stat(m_ptr, A_STR) * 2;

                    /*
                     * Note that
                     * monsters "fall" into the entranceway in the same
                     * turn that they bash the door down.
                     */
                    bash_chance = success_chance(10, skill, difficulty);
                }
            }

            /*
             * A monster cannot both bash and unlock a door in the same
             * turn.  It needs to pick one of the two methods to use.
             */

            if ((unlock_chance > bash_chance) || (bash_chance == 0))
                *bash = false;
            else
                *bash = true;

            return (MIN(move_chance, (MAX(unlock_chance, bash_chance))));
        }

        /* Any wall grid that isn't explicitly made passable is impassable. */
        return (0);
    }
}

bool attacker_at(int y, int x)
{
    if (cave_m_idx[y][x] <= 0)
    {
        return false;
    }

    monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    return !(r_ptr->flags1 & (RF1_PEACEFUL));
}

/*
 * Counts the number of monsters adjacent to a given square
 */
int adj_mon_count(int y, int x)
{
    int xx, yy;
    int count = 0;

    for (yy = -1; yy <= +1; yy++)
    {
        for (xx = -1; xx <= +1; xx++)
        {
            if (!((xx == 0) && (yy == 0)))
            {
                if (attacker_at(y + yy, x + xx) > 0)
                {
                    count++;
                }
            }
        }
    }

    return (count);
}

void tell_allies(int y, int x, u32b flag)
{
    monster_type* m_ptr;
    monster_race* r_ptr;

    int i;

    bool warned = false;

    // paranoia
    if (cave_m_idx[y][x] <= 0)
        return;

    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        /* Access the monster */
        monster_type* n_ptr = &mon_list[i];
        monster_race* nr_ptr = &r_info[n_ptr->r_idx];

        int dist;

        // Access the monster
        n_ptr = &mon_list[i];
        nr_ptr = &r_info[n_ptr->r_idx];

        // Ignore dead monsters
        if (!n_ptr->r_idx)
            continue;

        // Ignore monsters with the wrong symbol
        if (r_ptr->d_char != nr_ptr->d_char)
            continue;

        // Ignore monsters that already know
        if ((n_ptr->alertness >= ALERTNESS_ALERT) && (n_ptr->mflag & (flag)))
            continue;

        // determine the distance between the monsters
        dist = distance(m_ptr->fy, m_ptr->fx, n_ptr->fy, n_ptr->fx);

        // penalize this for not being in line of sight
        if (!los(y, x, n_ptr->fy, n_ptr->fx))
            dist *= 2;

        // Ignore monsters that are too far away
        if (dist > 15)
            continue;

        // When the first monter in need of warning is found, make the warning
        // shout
        if (!warned)
        {
            warning_message(m_ptr);
            warned = true;
        }

        // If an eligible monster is now alert, then set the flag
        if (n_ptr->alertness >= ALERTNESS_ALERT)
        {
            // Set the flag
            n_ptr->mflag |= (MFLAG_ACTV | flag);
        }
    }
}
