#include "angband.h"
#include "externs.h"
#include "melee/melee-attack.h"
#include "melee/melee-movement.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-process.h"
#include "melee/melee-util.h"

/*
 * Given a central direction at position [dir #][0], return a series
 * of directions radiating out on both sides from the central direction
 * all the way back to its rear.
 *
 * Side directions come in pairs; for example, directions '1' and '3'
 * flank direction '2'.  The code should know which side to consider
 * first.  If the left, it must add 10 to the central direction to
 * access the second part of the table.
 */
static byte side_dirs[20][8] = { { 0, 0, 0, 0, 0, 0, 0, 0 }, /* bias right */
    { 1, 4, 2, 7, 3, 8, 6, 9 }, { 2, 1, 3, 4, 6, 7, 9, 8 },
    { 3, 2, 6, 1, 9, 4, 8, 7 }, { 4, 7, 1, 8, 2, 9, 3, 6 },
    { 5, 5, 5, 5, 5, 5, 5, 5 }, { 6, 3, 9, 2, 8, 1, 7, 4 },
    { 7, 8, 4, 9, 1, 6, 2, 3 }, { 8, 9, 7, 6, 4, 3, 1, 2 },
    { 9, 6, 8, 3, 7, 2, 4, 1 },

    { 0, 0, 0, 0, 0, 0, 0, 0 }, /* bias left */
    { 1, 2, 4, 3, 7, 6, 8, 9 }, { 2, 3, 1, 6, 4, 9, 7, 8 },
    { 3, 6, 2, 9, 1, 8, 4, 7 }, { 4, 1, 7, 2, 8, 3, 9, 6 },
    { 5, 5, 5, 5, 5, 5, 5, 5 }, { 6, 9, 3, 8, 2, 7, 1, 4 },
    { 7, 4, 8, 1, 9, 2, 6, 3 }, { 8, 7, 9, 4, 6, 1, 3, 2 },
    { 9, 8, 6, 7, 3, 4, 2, 1 } };

/*
 * Choose the probable best direction for a monster to move in.  This
 * is done by choosing a target grid and then finding the direction that
 * best approaches it.
 *
 * Monsters that cannot move always attack if possible.
 * Frightened monsters retreat.
 * Monsters adjacent to the character attack if possible.
 *
 * Monster packs lure the character into open ground and then leap
 * upon him.  Monster groups try to surround the character.  -KJ-
 *
 * Monsters not in LOS always advance (this avoids player frustration).
 * Monsters in LOS will advance to the character, up to their standard
 * combat range, to a grid that allows them to target the character, or
 * just stay still if they are happy where they are, depending on the
 * tactical situation and the monster's preferred and minimum combat
 * ranges.
 * NOTE:  Here is an area that would benefit from more development work.
 *
 * Non-trivial movement calculations are performed by the helper
 * functions "get_move_advance" and "get_move_retreat", which keeps
 * this function relatively simple.
 *
 * The variable "must_use_target" is used for monsters that can't
 * currently perceive the character, but have a known target to move
 * towards.  With a bit more work, this will lead to semi-realistic
 * "hunting" behavior.
 *
 * Return false if monster doesn't want to move or can't.
 */
bool get_move(
    monster_type* m_ptr, int* ty, int* tx, bool* fear, bool must_use_target)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    int i, start;
    int y, x;

    int py = p_ptr->py;
    int px = p_ptr->px;

    /* Assume no movement */
    *ty = m_ptr->fy;
    *tx = m_ptr->fx;

    /*
     * Some monsters will not move into sight of the player.
     */
    if ((r_ptr->flags1 & (RF1_HIDDEN_MOVE))
        && ((cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_SEEN))
            || seen_by_keen_senses(m_ptr->fy, m_ptr->fx)))
    {
        /* Memorize lack of moves after a while. */
        if (!(l_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            if (m_ptr->ml && (one_in_(50)))
            {
                l_ptr->flags1 |= (RF1_HIDDEN_MOVE);
            }
        }
        /* If we are in sight, do not move */
        return (false);
    }

    // Morgoth will not move during the 'truce'
    if ((m_ptr->r_idx == R_IDX_MORGOTH) && p_ptr->truce)
    {
        return (false);
    }

    // worm masses, nameless things and the like won't deliberately move towards
    // the player if she is too far away
    if ((r_ptr->flags2 & (RF2_MINDLESS)) && (r_ptr->flags2 & (RF2_TERRITORIAL))
        && (m_ptr->cdis > 5))
    {
        return (false);
    }

    /*
     * Monsters that cannot move will attack the character if he is
     * adjacent.  Otherwise, they cannot move.
     */
    if (r_ptr->flags1 & (RF1_NEVER_MOVE))
    {
        /* Hack -- memorize lack of moves after a while. */
        if (!(l_ptr->flags1 & (RF1_NEVER_MOVE)))
        {
            if (m_ptr->ml && (one_in_(20)))
                l_ptr->flags1 |= (RF1_NEVER_MOVE);
        }

        /* Is character in range? */
        if (m_ptr->cdis <= 1)
        {
            /* Monster can't melee either (pathetic little creature) */
            if (r_ptr->flags1 & (RF1_NEVER_BLOW))
            {
                /* Hack -- memorize lack of attacks after a while */
                if (!(l_ptr->flags1 & (RF1_NEVER_BLOW)))
                {
                    if (m_ptr->ml && (one_in_(10)))
                        l_ptr->flags1 |= (RF1_NEVER_BLOW);
                }
            }

            /* Can attack */
            else
            {
                /* Kill. */
                *fear = false;
                *ty = py;
                *tx = px;
                return (true);
            }
        }

        /* If we can't hit anything, do not move */
        return (false);
    }

    /*
     * Monster is only allowed to use targeting information.
     */
    if (must_use_target)
    {
        *ty = m_ptr->target_y;
        *tx = m_ptr->target_x;
        return (true);
    }

    /*** Handle monster fear -- only for monsters that can move ***/

    /* Is the monster scared? */
    if ((m_ptr->min_range >= FLEE_RANGE) || (m_ptr->stance == STANCE_FLEEING))
        *fear = true;
    else
        *fear = false;

    /* Monster is frightened or terrified. */
    if (*fear)
    {
        /* The character is too close to avoid, and faster than we are */
        if ((m_ptr->stance != STANCE_FLEEING) && (m_ptr->cdis < TURN_RANGE)
            && (p_ptr->pspeed > m_ptr->mspeed))
        {
            /* Recalculate range */
            find_range(m_ptr);

            /* Note changes in monster attitude */
            if (m_ptr->min_range < m_ptr->cdis)
            {
                /* Cancel fear */
                *fear = false;

                /* No message -- too annoying */

                /* Charge! */
                *ty = py;
                *tx = px;

                return (true);
            }
        }

        /* The monster is within 25 grids of the character */
        else if (m_ptr->cdis < FLEE_RANGE)
        {
            /* Find and move towards a hidey-hole */
            get_move_retreat(m_ptr, ty, tx);
            return (true);
        }

        /* Monster is well away from danger */
        else
        {
            /* No need to move */
            return (false);
        }
    }

    // if far too close, step back towards the monster's minimum range
    if ((!*fear) && (m_ptr->cdis < m_ptr->min_range - 2))
    {
        if (get_move_retreat(m_ptr, ty, tx))
        {
            *fear = true;
            return (true);
        }
        else
        {
            /* No safe spot -- charge */
            *ty = py;
            *tx = px;
        }
    }

    /* If the character is adjacent, back off, surround the player, or attack.
     */
    if ((!*fear) && (m_ptr->cdis <= 1))
    {
        /* Monsters that cannot attack back off. */
        if (r_ptr->flags1 & (RF1_NEVER_BLOW))
        {
            /* Hack -- memorize lack of attacks after a while */
            if (!(l_ptr->flags1 & (RF1_NEVER_BLOW)))
            {
                if (m_ptr->ml && (one_in_(10)))
                    l_ptr->flags1 |= (RF1_NEVER_BLOW);
            }

            /* Back away */
            *fear = true;
        }

        else
        {
            // Smart monsters try harder to surround the player
            if (r_ptr->flags2 & (RF2_SMART))
            {
                int fy = m_ptr->fy;
                int fx = m_ptr->fx;
                int count = adj_mon_count(fy, fx);
                int dy = py - fy;
                int dx = px - fx;

                start = rand_int(8);

                /* Maybe move to a less crowded square near the player if
                 * possible */
                for (i = start; i < 8 + start; i++)
                {
                    /* Pick squares near player */
                    y = py + ddy_ddd[i % 8];
                    x = px + ddx_ddd[i % 8];

                    // if also adjacent to monster
                    if ((ABS(fy - y) <= 1) && (ABS(fx - x) <= 1)
                        && !((fy == y) && (fx == x)))
                    {
                        // if it is free...
                        if (cave_floor_bold(y, x) && (cave_m_idx[y][x] <= 0))
                        {
                            // and has a lower count...
                            if ((adj_mon_count(y, x) <= count)
                                && ((r_ptr->flags2 & (RF2_FLANKING))
                                    || one_in_(2)))
                            {
                                // then maybe set it as a new target
                                *ty = y;
                                *tx = x;
                                return (true);
                            }
                        }
                    }
                }

                /* If the monster didn't do that, then check for end-corridor
                 * cases */

                // if player is in an orthogonal direction, eg:
                //
                //  X#A
                //  Xo@
                //  X#B
                //
                if (dy * dx == 0)
                {
                    // if walls on either side of monster ('#')
                    if (cave_wall_bold(fy + dx, fx + dy)
                        && cave_wall_bold(fy - dx, fx - dy))
                    {
                        // if there is a monster in one of the three squares
                        // behind ('X')
                        if ((cave_m_idx[fy + dx - dy][fx + dy - dx] > 0)
                            || (cave_m_idx[fy - dy][fx - dx] > 0)
                            || (cave_m_idx[fy - dx - dy][fx - dy - dx] > 0))
                        {
                            // if 'A' and 'B' are free, go to one at random
                            if ((cave_m_idx[fy + dx + dy][fx + dy + dx] <= 0)
                                && (cave_m_idx[fy - dx + dy][fx - dy + dx]
                                    <= 0))
                            {
                                if (one_in_(2))
                                {
                                    *ty = fy + dx + dy;
                                    *tx = fx + dy + dx;
                                }
                                else
                                {
                                    *ty = fy - dx + dy;
                                    *tx = fx - dy + dx;
                                }
                                return (true);
                            }
                            // if 'A' is free, go there
                            else if (cave_m_idx[fy + dx + dy][fx + dy + dx]
                                <= 0)
                            {
                                *ty = fy + dx + dy;
                                *tx = fx + dy + dx;
                                return (true);
                            }
                            // if 'B' is free, go there
                            else if (cave_m_idx[fy - dx + dy][fx - dy + dx]
                                <= 0)
                            {
                                *ty = fy - dx + dy;
                                *tx = fx - dy + dx;
                                return (true);
                            }
                        }
                    }
                }
                // if player is in a diagonal direction, eg:
                //
                //  X#       XXX
                //  XoA  or  #o#
                //  X#@       A@
                //
                else
                {
                    // if walls north and south of monster ('#')
                    if (cave_wall_bold(fy + 1, fx)
                        && cave_wall_bold(fy - 1, fx))
                    {
                        // if there is a monster in one of the three squares
                        // behind ('X')
                        if ((cave_m_idx[fy - 1][fx - dx] > 0)
                            || (cave_m_idx[fy][fx - dx] > 0)
                            || (cave_m_idx[fy + 1][fx - dx] > 0))
                        {
                            // if 'A' is free, go there
                            if (cave_m_idx[fy][fx + dx] <= 0)
                            {
                                *ty = fy;
                                *tx = fx + dx;
                                return (true);
                            }
                        }
                    }
                    // if walls east and west of monster ('#')
                    else if (cave_wall_bold(fy, fx - 1)
                        && cave_wall_bold(fy, fx + 1))
                    {
                        // if there is a monster in one of the three squares
                        // behind ('X')
                        if ((cave_m_idx[fy - dy][fx - 1] > 0)
                            || (cave_m_idx[fy - dy][fx] > 0)
                            || (cave_m_idx[fy - dy][fx + 1] > 0))
                        {
                            // if 'A' is free, go there
                            if (cave_m_idx[fy + dy][fx] <= 0)
                            {
                                *ty = fy + dy;
                                *tx = fx;
                                return (true);
                            }
                        }
                    }
                }
            }

            /* All other monsters attack. */
            *ty = py;
            *tx = px;
            return (true);
        }
    }

    // Smart monsters try to lure the character into the open.
    if ((!*fear) && (r_ptr->flags2 & (RF2_SMART))
        && !(r_ptr->flags2 & (RF2_PASS_WALL | RF2_KILL_WALL))
        && (m_ptr->stance == STANCE_CONFIDENT))
    {
        // determine how vulnerable the player is
        int vulnerability = calc_vulnerability(m_ptr->fy, m_ptr->fx);

        // determine how hesitant the monster is
        int hesitance = calc_hesitance(m_ptr);

        // Character is insufficiently vulnerable
        if (vulnerability < hesitance)
        {
            /* Monster has to be willing to melee */
            if (m_ptr->min_range == 1)
            {
                /* If we're in sight, find a hiding place */
                if (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_FIRE | CAVE_SEEN))
                {
                    /* Find a safe spot to lurk in */
                    if (get_move_retreat(m_ptr, ty, tx))
                    {
                        *fear = true;
                    }
                    else
                    {
                        /* No safe spot -- charge */
                        *ty = py;
                        *tx = px;
                    }
                }

                /* Otherwise, we advance cautiously */
                else
                {
                    /* Advance, ... */
                    get_move_advance(m_ptr, ty, tx);

                    /* ... but make sure we stay hidden. */
                    if (m_ptr->cdis > 1)
                        *fear = true;
                }

                /* done */
                return (true);
            }
            else
            {
                /* If we're in sight, find a hiding place */
                if (cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_FIRE | CAVE_SEEN))
                {
                    /* Find a safe spot to lurk in */
                    if (get_move_retreat(m_ptr, ty, tx))
                    {
                        *fear = true;
                    }
                    else
                    {
                        /* No safe spot -- charge */
                        *ty = py;
                        *tx = px;
                    }
                }
            }
        }
    }

    /* Monster groups try to surround the character */
    if ((!*fear)
        && ((r_ptr->flags1 & (RF1_FRIENDS)) || (r_ptr->flags1 & (RF1_FRIEND)))
        && (m_ptr->cdis <= 3) && (player_has_los_bold(m_ptr->fy, m_ptr->fx)))
    {
        /*Only if we do not have a clean path to player*/
        if (projectable(
                m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK)
            != PROJECT_CLEAR)
        {
            start = rand_int(8);

            /* Find a random empty square next to the player to head for */
            for (i = start; i < 8 + start; i++)
            {
                /* Pick squares near player */
                y = py + ddy_ddd[i % 8];
                x = px + ddx_ddd[i % 8];

                /* Check Bounds */
                if (!in_bounds(y, x))
                    continue;

                /* Ignore occupied grids */
                if (cave_m_idx[y][x] != 0)
                    continue;

                /* Ignore grids that monster can't enter immediately */
                if (!cave_exist_mon(r_ptr, y, x, false, true))
                    continue;

                /* Accept */
                *ty = y;
                *tx = x;
                return (true);
            }
        }
    }

    /* No special moves made -- use standard movement */

    /* Not frightened */
    if (!*fear)
    {
        /*
         * XXX XXX -- The monster cannot see the character.  Make it
         * advance, so the player can have fun ambushing it.
         */
        if (!player_has_los_bold(m_ptr->fy, m_ptr->fx))
        {
            /* Advance */
            get_move_advance(m_ptr, ty, tx);
        }

        /* Monster can see the character */
        else
        {
            /* Always reset the monster's target */
            m_ptr->target_y = py;
            m_ptr->target_x = px;

            /* Monsters too far away will advance. */
            if (m_ptr->cdis > m_ptr->best_range)
            {
                *ty = py;
                *tx = px;
            }

            /* Monsters not too close will often advance */
            else if ((m_ptr->cdis > m_ptr->min_range) && (one_in_(2)))
            {
                *ty = py;
                *tx = px;
            }

            /* Monsters that can't target the character will advance. */
            else if (!player_can_fire_bold(m_ptr->fy, m_ptr->fx))
            {
                *ty = py;
                *tx = px;
            }

            /* Otherwise they will stay still or move randomly. */
            else
            {
                /*
                 * It would be odd if monsters that move randomly
                 * were to stay still.
                 */
                if (r_ptr->flags1 & (RF1_RAND_50 | RF1_RAND_25))
                {
                    /* Pick a random grid next to the monster */
                    i = rand_int(8);

                    *ty = m_ptr->fy + ddy_ddd[i];
                    *tx = m_ptr->fx + ddx_ddd[i];
                }

                /* Monsters could look for better terrain... */
            }

            // in most cases where the monster is targetting the player, use the
            // clever pathfinding instead
            if ((*ty == py) && (*tx == px))
            {
                m_ptr->target_y = 0;
                m_ptr->target_x = 0;

                /* Advance */
                get_move_advance(m_ptr, ty, tx);
            }
        }
    }

    /* Monster is frightened */
    else
    {
        /* Back away -- try to be smart about it */
        get_move_retreat(m_ptr, ty, tx);
    }

    /* We do not want to move */
    if ((*ty == m_ptr->fy) && (*tx == m_ptr->fx))
        return (false);

    /* We want to move */
    return (true);
}

/*
 * Confused monsters bang into walls and doors, and wander into lava or
 * water.  This function assumes that the monster does not belong in this
 * grid, and therefore should suffer for trying to enter it.
 */
static void make_confused_move(monster_type* m_ptr, int y, int x)
{
    char m_name[80];

    int feat;

    monster_race* r_ptr;

    bool seen = false;

    bool confused = m_ptr->confused;

    r_ptr = &r_info[m_ptr->r_idx];

    /* Check Bounds (fully) */
    if (!in_bounds_fully(y, x))
        return;

    /* Check location */
    feat = cave_feat[y][x];

    /* Check visibility */
    if ((m_ptr->ml) && (cave_info[y][x] & (CAVE_SEEN)))
        seen = true;

    /* Get the monster name/poss */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    // Feature is a chasm
    if (cave_feat[y][x] == FEAT_CHASM)
    {
        // The creature can't fly and the grid is empty
        if (!(r_ptr->flags2 & (RF2_FLYING)) && (cave_m_idx[y][x] < 0))
        {
            monster_swap(m_ptr->fy, m_ptr->fx, y, x);
        }
    }

    /* Feature is a wall */
    else if (cave_info[y][x] & (CAVE_WALL))
    {
        /* Feature is a (known) door */
        if (cave_known_closed_door_bold(y, x))
        {
            if (seen && confused)
                msg_format("%^s staggers into a door.", m_name);
        }

        /* Rubble */
        else if (feat == FEAT_RUBBLE)
        {
            if (seen && confused)
                msg_format("%^s staggers into some rubble.", m_name);
        }

        /* Otherwise, we assume that the feature is a "wall".  XXX  */
        else
        {
            if (seen && confused)
                msg_format("%^s bashes into a wall.", m_name);
        }

        /*possibly update the monster health bar*/
        if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
            p_ptr->redraw |= (PR_HEALTHBAR);
    }

    /* Feature is not a wall */
    else
    {
        /* No changes */
    }
}

/*
 * Given a target grid, calculate the grid the monster will actually
 * attempt to move into.
 *
 * The simplest case is when the target grid is adjacent to us and
 * able to be entered easily.  Usually, however, one or both of these
 * conditions don't hold, and we must pick an initial direction, than
 * look at several directions to find that most likely to be the best
 * choice.  If so, the monster needs to know the order in which to try
 * other directions on either side.  If there is no good logical reason
 * to prioritize one side over the other, the monster will act on the
 * "spur of the moment", using current turn as a randomizer.
 *
 * The monster then attempts to move into the grid.  If it fails, this
 * function returns false and the monster ends its turn.
 *
 * The variable "fear" is used to invoke any special rules for monsters
 * wanting to retreat rather than advance.  For example, such monsters
 * will not leave an non-viewable grid for a viewable one and will try
 * to avoid the character.
 *
 * The variable "bash" remembers whether a monster had to bash a door
 * or not.  This has to be remembered because the choice to bash is
 * made in a different function than the actual bash move.  XXX XXX  If
 * the number of such variables becomes greater, a structure to hold them
 * would look better than passing them around from function to function.
 */
bool make_move(
    monster_type* m_ptr, int* ty, int* tx, bool fear, bool* bash)
{
    int i, j;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Start direction, current direction */
    int dir0, dir;

    /* Deltas, absolute axis distances from monster to target grid */
    int dy, ay, dx, ax;

    /* Existing monster location, proposed new location */
    int oy, ox, ny, nx;

    bool avoid = false;
    bool passable = false;
    bool look_again = false;

    int chance;

    /* Remember where monster is */
    oy = m_ptr->fy;
    ox = m_ptr->fx;

    /* Get the change in position needed to get to the target */
    dy = *ty - oy;
    dx = *tx - ox;

    /* Calculate vertical and horizontal distances */
    ay = ABS(dy);
    ax = ABS(dx);

    /* We mostly want to move vertically */
    if (ay > (ax * 2))
    {
        /* Choose between directions '8' and '2' */
        if (dy < 0)
        {
            /* We're heading up */
            dir0 = 8;
            if ((dx < 0) || (dx == 0 && (turn % 2 == 0)))
                dir0 += 10;
        }
        else
        {
            /* We're heading down */
            dir0 = 2;
            if ((dx > 0) || (dx == 0 && (turn % 2 == 0)))
                dir0 += 10;
        }
    }

    /* We mostly want to move horizontally */
    else if (ax > (ay * 2))
    {
        /* Choose between directions '4' and '6' */
        if (dx < 0)
        {
            /* We're heading left */
            dir0 = 4;
            if ((dy > 0) || (dy == 0 && (turn % 2 == 0)))
                dir0 += 10;
        }
        else
        {
            /* We're heading right */
            dir0 = 6;
            if ((dy < 0) || (dy == 0 && (turn % 2 == 0)))
                dir0 += 10;
        }
    }

    /* We want to move up and sideways */
    else if (dy < 0)
    {
        /* Choose between directions '7' and '9' */
        if (dx < 0)
        {
            /* We're heading up and left */
            dir0 = 7;
            if ((ay < ax) || (ay == ax && (turn % 2 == 0)))
                dir0 += 10;
        }
        else
        {
            /* We're heading up and right */
            dir0 = 9;
            if ((ay > ax) || (ay == ax && (turn % 2 == 0)))
                dir0 += 10;
        }
    }

    /* We want to move down and sideways */
    else
    {
        /* Choose between directions '1' and '3' */
        if (dx < 0)
        {
            /* We're heading down and left */
            dir0 = 1;
            if ((ay > ax) || (ay == ax && (turn % 2 == 0)))
                dir0 += 10;
        }
        else
        {
            /* We're heading down and right */
            dir0 = 3;
            if ((ay < ax) || (ay == ax && (turn % 2 == 0)))
                dir0 += 10;
        }
    }

    // Sil-y: not sure why this bit is needed, but it seemed to be, so I added
    // it

    // If the monster wants to stay still...
    if ((*ty == m_ptr->fy) && (*tx == m_ptr->fx))
    {
        // if it is adjacent to the player, and can attack, then just try that.
        if ((m_ptr->cdis == 1) && !(r_ptr->flags1 & (RF1_NEVER_BLOW)))
        {
            *ty = p_ptr->py;
            *tx = p_ptr->px;
        }

        // otherwise just do nothing
        else
        {
            return (false);
        }
    }

    /* Apply monster confusion */
    if ((m_ptr->confused) && (!(r_ptr->flags1 & (RF1_NEVER_MOVE))))
    {
        // undo +10 modifiers
        if (dir0 > 10)
            dir0 -= 10;

        // gives 3 chances to be turned left and 3 chances to be turned right
        // leads to a binomial distribution of direction around the intended
        // one:
        //
        // 15 20 15
        //  6     6   (chances are all out of 64)
        //  1  0  1

        i = damroll(3, 2) - damroll(3, 2);
        dir0 = cycle[chome[dir0] + i];
    }

    /* Is the target grid adjacent to the current monster's position? */
    if ((dy >= -1) && (dy <= 1) && (dx >= -1) && (dx <= 1) && !m_ptr->confused)
    {
        /* If it is, try the shortcut of simply moving into the grid */

        /* Get the probability of entering this grid */
        chance = cave_passable_mon(m_ptr, *ty, *tx, bash);

        /* Grid must be pretty easy to enter */
        if (chance >= 50)
        {
            /* We can enter this grid */
            if ((chance >= 100) || percent_chance(chance))
            {
                return (true);
            }

            /* Failure to enter grid.  Cancel move */
            else
            {
                return (false);
            }
        }
    }

    /*
     * Now that we have an initial direction, we must determine which
     * grid to actually move into.
     */
    if (true)
    {
        /* Build a structure to hold movement data */
        typedef struct move_data move_data;
        struct move_data
        {
            int move_chance;
            bool move_bash;
        };
        move_data moves_data[8];

        /*
         * Scan each of the eight possible directions, in the order of
         * priority given by the table "side_dirs", choosing the one that
         * looks like it will get the monster to the character - or away
         * from him - most effectively.
         */
        for (i = 0; i <= 8; i++)
        {
            /* Out of options */
            if (i == 8)
                break;

            /* Get the actual direction */
            dir = side_dirs[dir0][i];

            /* Get the grid in our chosen direction */
            ny = oy + ddy[dir];
            nx = ox + ddx[dir];

            /* Check Bounds */
            if (!in_bounds(ny, nx))
                continue;

            /* Store this grid's movement data. */
            moves_data[i].move_chance = cave_passable_mon(m_ptr, ny, nx, bash);
            moves_data[i].move_bash = *bash;

            /* Confused monsters must choose the first grid */
            if (m_ptr->confused)
                break;

            /* If this grid is totally impassable, skip it */
            if (moves_data[i].move_chance == 0)
                continue;

            /* Frightened monsters work hard not to be seen. */
            if (fear)
            {
                /* Monster is having trouble navigating to its target. */
                if ((m_ptr->target_y) && (m_ptr->target_x) && (i >= 2)
                    && (distance(m_ptr->fy, m_ptr->fx, m_ptr->target_y,
                            m_ptr->target_x)
                        > 1))
                {
                    /* Look for an adjacent grid leading to the target */
                    if (get_route_to_target(m_ptr, ty, tx))
                    {
                        /* Calculate the chance to enter the grid */
                        chance = cave_passable_mon(m_ptr, *ty, *tx, bash);

                        /* Try to move into the grid */
                        if (!percent_chance(chance))
                        {
                            /* Can't move */
                            return (false);
                        }

                        /* Can move */
                        return (true);
                    }

                    /* No good route found */
                    else if (i >= 3)
                    {
                        /*
                         * We can't get to our hiding place.  We're in line of
                         * fire. The only thing left to do is go down fighting.
                         * XXX XXX
                         */
                        if ((m_ptr->ml) && (player_can_fire_bold(oy, ox))
                            && !p_ptr->truce && (r_ptr->freq_ranged < 50))
                        {
                            /* Message if visible */
                            if (m_ptr->ml)
                            {
                                char m_name[80];

                                /* Get the monster name */
                                monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                                /* Dump a message */
                                msg_format("%^s panics.", m_name);
                            }

                            // boost morale and make the monster aggressive
                            m_ptr->tmp_morale = MAX(m_ptr->tmp_morale + 60, 60);
                            calc_morale(m_ptr);
                            calc_stance(m_ptr);
                            m_ptr->mflag |= (MFLAG_AGGRESSIVE);
                        }
                    }
                }

                /* Attacking the character as a first choice? */
                if ((i == 0) && (ny == p_ptr->py) && (nx == p_ptr->px))
                {
                    /* Need to rethink some plans XXX XXX XXX */
                    m_ptr->target_y = 0;
                    m_ptr->target_x = 0;
                }

                /* Monster is visible */
                if (m_ptr->ml)
                {
                    /* And is in LOS */
                    if (player_has_los_bold(oy, ox))
                    {
                        /* Accept any easily passable grid out of LOS */
                        if ((!player_has_los_bold(ny, nx))
                            && (moves_data[i].move_chance > 40))
                        {
                            break;
                        }
                    }
                    else
                    {
                        /* Do not enter a grid in LOS */
                        if (player_has_los_bold(ny, nx))
                        {
                            moves_data[i].move_chance = 0;
                            continue;
                        }
                    }
                }

                /* Monster can't be seen, and is not in a "seen" grid. */
                if ((!m_ptr->ml) && (!player_can_see_bold(oy, ox)))
                {
                    /* Do not enter a "seen" grid */
                    if (player_can_see_bold(ny, nx))
                    {
                        moves_data[i].move_chance = 0;
                        continue;
                    }
                }
            }

            /* XXX XXX -- Sometimes attempt to break glyphs. */
            if (cave_glyph(ny, nx) && (!fear) && (one_in_(5)))
            {
                break;
            }

            /* Initial direction is almost certainly the best one */
            if ((i == 0) && (moves_data[i].move_chance >= 80))
            {
                /*
                 * If backing away and close, try not to walk next
                 * to the character, or get stuck fighting him.
                 */
                if ((fear) && (m_ptr->cdis <= 2)
                    && (distance(p_ptr->py, p_ptr->px, ny, nx) <= 1))
                {
                    avoid = true;
                }

                else
                    break;
            }

            /* Either of the first two side directions looks good */
            else if (((i == 1) || (i == 2))
                && (moves_data[i].move_chance >= 50))
            {
                /* Accept the central direction if at least as good */
                if ((moves_data[0].move_chance >= moves_data[i].move_chance))
                {
                    if (avoid)
                    {
                        /* Frightened monsters try to avoid the character */
                        if (distance(p_ptr->py, p_ptr->px, ny, nx) == 0)
                        {
                            i = 0;
                        }
                    }
                    else
                    {
                        i = 0;
                    }
                }

                /* Accept this direction */
                break;
            }

            /* This is the first passable direction */
            if (!passable)
            {
                /* Note passable */
                passable = true;

                /* All the best directions are blocked. */
                if (i >= 3)
                {
                    /* Settle for "good enough" */
                    break;
                }
            }

            /* We haven't made a decision yet; look again. */
            if (i == 7)
                look_again = true;
        }

        /* We've exhausted all the easy answers. */
        if (look_again)
        {
            /* There are no passable directions. */
            if (!passable)
            {
                return (false);
            }

            /* We can move. */
            for (j = 0; j < 8; j++)
            {
                /* Accept the first option, however poor.  XXX */
                if (moves_data[j].move_chance)
                {
                    i = j;
                    break;
                }
            }
        }

        /* If no direction was acceptable, end turn */
        if (i >= 8)
        {
            return (false);
        }

        /* Get movement information (again) */
        dir = side_dirs[dir0][i];
        *bash = moves_data[i].move_bash;

        /* No good moves, so we just sit still and wait. */
        if ((dir == 5) || (dir == 0))
        {
            return (false);
        }

        /* Get grid to move into */
        *ty = oy + ddy[dir];
        *tx = ox + ddx[dir];

        /*
         * Amusing messages and effects for confused monsters trying
         * to enter terrain forbidden to them.
         */
        if ((m_ptr->confused) && (moves_data[i].move_chance <= 25))
        {
            /* Sometimes hurt the poor little critter */
            make_confused_move(m_ptr, *ty, *tx);

            /* Do not actually move */
            if (!moves_data[i].move_chance)
                return (false);
        }

        /* Try to move in the chosen direction.  If we fail, end turn. */
        if ((moves_data[i].move_chance < 100)
            && !percent_chance(moves_data[i].move_chance))
        {
            return (false);
        }
    }

    /* Monster is frightened, and is obliged to fight. */
    if ((fear) && (cave_m_idx[*ty][*tx] < 0) && !p_ptr->truce)
    {
        /* Message if visible */
        if (m_ptr->ml)
        {
            char m_name[80];

            /* Get the monster name */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Dump a message */
            msg_format("%^s panics.", m_name);
        }

        // boost morale and make the monster aggressive
        m_ptr->tmp_morale = MAX(m_ptr->tmp_morale + 60, 60);
        calc_morale(m_ptr);
        calc_stance(m_ptr);
        m_ptr->mflag |= (MFLAG_AGGRESSIVE);
    }

    /* We can move. */
    return (true);
}

/*
 * If one monster moves into another monster's grid, they will
 * normally swap places.  If the second monster cannot exist in the
 * grid the first monster left, this can't happen.  In such cases,
 * the first monster tries to push the second out of the way.
 */
bool push_aside(monster_type* m_ptr, monster_type* n_ptr)
{
    /* Get racial information about the second monster */
    monster_race* nr_ptr = &r_info[n_ptr->r_idx];

    int y, x, i;
    int dir = 0;

    /*
     * Translate the difference between the locations of the two
     * monsters into a direction of travel.
     */
    for (i = 0; i < 10; i++)
    {
        /* Require correct difference along the y-axis */
        if ((n_ptr->fy - m_ptr->fy) != ddy[i])
            continue;

        /* Require correct difference along the x-axis */
        if ((n_ptr->fx - m_ptr->fx) != ddx[i])
            continue;

        /* Found the direction */
        dir = i;
        break;
    }

    /* Favor either the left or right side on the "spur of the moment". */
    if (one_in_(2))
        dir += 10;

    /* Check all directions radiating out from the initial direction. */
    for (i = 0; i < 7; i++)
    {
        int side_dir = side_dirs[dir][i];

        y = n_ptr->fy + ddy[side_dir];
        x = n_ptr->fx + ddx[side_dir];

        /* Illegal grid */
        if (!in_bounds_fully(y, x))
            continue;

        /* Grid is not occupied, and the 2nd monster can exist in it. */
        if (cave_exist_mon(nr_ptr, y, x, false, true))
        {
            /* Push the 2nd monster into the empty grid. */
            monster_swap(n_ptr->fy, n_ptr->fx, y, x);
            return (true);
        }
    }

    /* We didn't find any empty, legal grids */
    return (false);
}
