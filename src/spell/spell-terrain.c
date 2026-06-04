/* File: spell/spell-terrain.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

/*
 * Magically close/lock/restore a door at a particular grid
 */
bool lock_door(int y, int x, int power)
{
    int lock_level;
    int obvious = false;

    // ignore warded doors
    if (cave_glyph(y, x))
        return false;

    if (cave_feat[y][x] == FEAT_BROKEN)
        power -= 10;

    if ((power > 0) && (cave_m_idx[y][x] == 0))
    {
        if (cave_known_closed_door_bold(y, x) || (cave_feat[y][x] == FEAT_OPEN)
            || (cave_feat[y][x] == FEAT_BROKEN))
        {
            if ((cave_feat[y][x] == FEAT_OPEN)
                || (cave_feat[y][x] == FEAT_BROKEN))
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door slams shut.");
                }
                else
                {
                    msg_print("You hear a door slam shut.");
                }
            }

            // lock the door more firmly than it was before
            lock_level = cave_feat[y][x] - FEAT_DOOR_HEAD + power / 2;
            if (lock_level > 7)
            {
                lock_level = 7;
            }

            if (cave_feat[y][x] != FEAT_DOOR_HEAD + lock_level)
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD + lock_level);

                msg_print("You hear a 'click'.");
            }

            /* Update the flow code and visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
        }
    }

    return (obvious);
}

bool lock_doors_radius(int y0, int x0, int radius, int power)
{
    bool obvious = false;

    if (radius < 0)
        return false;

    for (int y = y0 - radius; y <= y0 + radius; y++)
    {
        for (int x = x0 - radius; x <= x0 + radius; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (distance(y0, x0, y, x) > radius)
                continue;

            if (lock_door(y, x, power))
                obvious = true;
        }
    }

    return obvious;
}

/*
 * Create stairs at the player location
 */
void stair_creation(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    /* XXX XXX XXX */
    if (!cave_valid_bold(py, px))
    {
        msg_print("The object resists the spell.");
        return;
    }

    /* XXX XXX XXX */
    delete_object(py, px);

    place_random_stairs(py, px);
}

void clear_temp_array(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);
    }

    /* None left */
    temp_n = 0;
}

/*
 * Aux function -- see below
 */
void cave_temp_mark(int y, int x, bool room)
{
    /* Avoid infinite recursion */
    if (cave_info[y][x] & (CAVE_TEMP))
        return;

    /* Option -- do not leave the current room */
    if ((room) && (!(cave_info[y][x] & (CAVE_ROOM))))
        return;

    /* Verify space */
    if (temp_n == TEMP_MAX)
        return;

    /* Mark the grid */
    cave_info[y][x] |= (CAVE_TEMP);

    /* Add it to the marked set */
    temp_y[temp_n] = y;
    temp_x[temp_n] = x;
    temp_n++;
}

/*
 * Mark the nearby area with CAVE_TEMP flags.  Allow limited range.
 */
void spread_cave_temp(int y1, int x1, int range, bool room)
{
    int i, y, x;

    /* Add the initial grid */
    cave_temp_mark(y1, x1, room);

    /* While grids are in the queue, add their neighbors */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get marked, but stop further spread */
        if (!cave_floor_bold(y, x))
            continue;

        /* Note limited range (note:  we spread out one grid further) */
        if ((range) && (distance(y1, x1, y, x) >= range))
            continue;

        /* Spread adjacent */
        cave_temp_mark(y + 1, x, room);
        cave_temp_mark(y - 1, x, room);
        cave_temp_mark(y, x + 1, room);
        cave_temp_mark(y, x - 1, room);

        /* Spread diagonal */
        cave_temp_mark(y + 1, x + 1, room);
        cave_temp_mark(y - 1, x - 1, room);
        cave_temp_mark(y - 1, x + 1, room);
        cave_temp_mark(y + 1, x - 1, room);
    }
}

/*
 * Destroy traps
 */
bool destroy_traps(int power)
{
    return (project_los_grids(GF_KILL_TRAP, 0, 0, power));
}

/*
 * Open doors
 */
bool open_doors(int power)
{
    return (project_los_grids(GF_KILL_DOOR, 0, 0, power));
}

/*
 * Close and lock doors
 */
bool lock_doors(int power)
{
    return (project_los_grids(GF_LOCK_DOOR, 0, 0, power));
}

void destroy_area(int y1, int x1, int r, bool full)
{
    int y, x, k, t;

    bool flag = false;

    /* Unused parameter */
    (void)full;

    /* No effect on the surface */
    if (!p_ptr->depth)
    {
        msg_print("The ground shakes for a moment.");
        return;
    }

    /* Big area of affect */
    for (y = (y1 - r); y <= (y1 + r); y++)
    {
        for (x = (x1 - r); x <= (x1 + r); x++)
        {
            /* Skip illegal grids */
            if (!in_bounds_fully(y, x))
                continue;

            /* Extract the distance */
            k = distance(y1, x1, y, x);

            /* Stay in the circle of death */
            if (k > r)
                continue;

            /* Lose room and vault */
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);

            /* Lose light and knowledge */
            cave_info[y][x] &= ~(CAVE_GLOW | CAVE_MARK);

            /* Hack -- Notice player affect */
            if (cave_m_idx[y][x] < 0)
            {
                /* Hurt the player later */
                flag = true;

                /* Do not hurt this grid */
                continue;
            }

            /* Hack -- Skip the epicenter */
            if ((y == y1) && (x == x1))
                continue;

            /* Delete the monster (if any) */
            delete_monster(y, x);

            /* Destroy "valid" grids */
            if (cave_valid_bold(y, x))
            {
                int feat = FEAT_FLOOR;

                /* Delete objects */
                delete_object(y, x);

                /* Wall (or floor) type */
                t = rand_int(200);

                /* Granite */
                if (t < 60)
                {
                    /* Create granite wall */
                    feat = FEAT_WALL_EXTRA;
                }

                /* Quartz */
                else if (t < 100)
                {
                    /* Create quartz vein */
                    feat = FEAT_QUARTZ;
                }

                /* Change the feature */
                cave_set_feat(y, x, feat);
            }
        }
    }

    /* Hack -- Affect player */
    if (flag)
    {
        /* Message */
        msg_print("There is a searing blast of light!");

        /* Blind the player */
        if (allow_player_blind(NULL))
        {
            /* Become blind */
            (void)set_blind(p_ptr->blind + damroll(4, 4));
        }
    }

    /* Make a lot of noise */
    monster_perception(true, false, -30);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);
}

/*
 * Creates an earthquake effect centered around (cy,cx) with radius r.
 *
 * Does rd8 damage at the centre, and one less die each square out
 * from there. If a square doesn't have a monster in it after the damage
 * it might be transformed to a different terrain (eg floor to rubble,
 * rubble to wall, wall to rubble), with a damage% chance. Note that
 * no damage is done to the square at the epicentre.
 *
 * If 'pit_y' and 'pit_x' are not zero, then a pit will be created at the
 * specified location. If the player is in this location, they get a chance to
 * move to another square. If there are no squares to jump to, they fall into
 * the pit and take some more damage.
 *
 * Sil-y: Theoretically the non-pit stuff could be moved to the project
 * functions assuming it can pass through walls properly and that it can deal
 * with creating terrain in a square with a monster iff it first kills the
 * monster and it can decay appropriately with distance (this last might be
 * hardest).
 */
void earthquake(int cy, int cx, int pit_y, int pit_x, int r, int who)
{
    int i, t;
    int y, x, dy, dx, yy, xx;

    int dd, ds, damage, net_dam, prt;

    int dist;

    int sn = 0, sy = 0, sx = 0;
    int feat;

    monster_type* creator_m_ptr;

    bool creator_vis = false;
    bool fall_into_pit = false;
    bool already_in_pit = false;

    /* No effect on the surface */
    if (!p_ptr->depth)
    {
        msg_print("The ground shakes for a moment.");
        return;
    }

    // Set the earthquake creator
    if (who < 0)
    {
        creator_m_ptr = PLAYER;
        creator_vis = true;
    }
    else
    {
        creator_m_ptr = &mon_list[who];
        creator_vis = creator_m_ptr->ml;
    }

    /* Paranoia -- Enforce maximum range */
    if (r > 6)
        r = 6;

    // Step 1:
    // deal with pit creation (if a valid location was passed to this function)
    if (in_bounds_fully(pit_y, pit_x))
    {
        // can't dodge out of a pit
        if (cave_pit_bold(p_ptr->py, p_ptr->px))
        {
            already_in_pit = true;
        }

        // deal with the possibility that the player is there
        if ((p_ptr->py == pit_y) && (p_ptr->px == pit_x))
        {
            if (!already_in_pit)
            {
                /* Check around the player for safe locations to dodge to */
                for (i = 0; i < 8; i++)
                {
                    /* Get the location */
                    y = p_ptr->py + ddy_ddd[i];
                    x = p_ptr->px + ddx_ddd[i];

                    /* Skip non-empty grids */
                    if (!cave_empty_bold(y, x))
                        continue;

                    /* Count "safe" grids, apply the randomizer */
                    if ((++sn > 1) && (rand_int(sn) != 0))
                        continue;

                    /* Save the safe location */
                    sy = y;
                    sx = x;
                }
            }

            if (sn > 0)
            {
                monster_swap(p_ptr->py, p_ptr->px, sy, sx);
            }

            else
            {
                // remember to make the player fall into the pit later

                fall_into_pit = true;
            }
        }

        if (cave_valid_bold(pit_y, pit_x))
        {
            /* Delete objects */
            delete_object(pit_y, pit_x);

            /* Change the feature */
            cave_set_feat(pit_y, pit_x, FEAT_TRAP_PIT);
        }
    }

    // Step 2:
    // Earthquake damage

    // flash the area (using project)
    project_ball(-1, r, p_ptr->py, p_ptr->px, p_ptr->py, p_ptr->px, 0, 0, -1,
        GF_EARTHQUAKE, PROJECT_PASS, false);

    for (dy = -r; dy <= r; dy++)
    {
        for (dx = -r; dx <= r; dx++)
        {
            /* Extract the location */
            y = cy + dy;
            x = cx + dx;

            /* Skip illegal grids */
            if (!in_bounds_fully(y, x))
                continue;

            dist = distance(cy, cx, y, x);

            /* Skip distant grids */
            if (dist > r)
                continue;

            // Sil-y: previously lost knowledge of the squares
            // cave_info[y][x] &= ~(CAVE_MARK);

            /* Skip the epicentre */
            if ((y == cy) && (x == cx))
                continue;

            // Roll the damage for this square
            dd = r + 1 - dist;
            ds = 8;
            damage = damroll(dd, ds);

            // If the player is on the square...
            if (cave_m_idx[y][x] < 0)
            {
                // appropriate message
                msg_print("You are pummeled with debris!");

                // apply protection
                prt = protection_roll(GF_HURT, false);
                net_dam = damage - prt;

                // take the damage
                if (net_dam > 0) {
                    killer_mark_other(SCORE_KILLER_OTHER);
                    take_hit(net_dam, "an earthquake");
                }

                // do stunning
                if (allow_player_stun(NULL))
                {
                    set_stun(p_ptr->stun + net_dam * 4);
                }

                // update the combat rolls to display this later
                update_combat_rolls1b(creator_m_ptr, PLAYER, creator_vis);
                update_combat_rolls2(
                    dd, ds, damage, -1, -1, prt, 100, GF_HURT, false);
            }

            // If a monster is on the square...
            else if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                monster_race* r_ptr = &r_info[m_ptr->r_idx];

                char m_name[80];

                /* Describe the monster */
                monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                /* Apply armor dice/sides curses/blessings */
                int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
                if (armor_dice_base < 0)
                    armor_dice_base = 0;
                int armor_dice = armor_dice_base + curse_flag_delta_cur(CUR_MON_ARM_DICE);
                int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_delta_cur(CUR_MON_ARM_SIDE);
                if (armor_dice < 0) armor_dice = 0;
                if (armor_sides < 1) armor_sides = 1;
                prt = damroll(armor_dice, armor_sides);
                net_dam = damage - prt;

                // apply damage after protection
                if (net_dam > 0)
                {
                    bool killed = false;

                    // message for each visible monster
                    if (m_ptr->ml)
                        msg_format("%^s is hit by falling debris.", m_name);

                    // if visible and caused by the player, update the combat
                    // rolls to display this later Sil-y: does seem to work so
                    // turned off temporarily
                    if (m_ptr->ml)
                    {
                        update_combat_rolls1b(
                            creator_m_ptr, m_ptr, creator_vis);
                        update_combat_rolls2(dd, ds, damage, armor_dice,
                            armor_sides, prt, 100, GF_HURT, false);
                    }

                    // do the damage and check for death
                    killed = mon_take_hit(cave_m_idx[y][x], net_dam, NULL, who);

                    // special effects for survivors
                    if (!killed)
                    {
                        /*some creatures are resistant to stunning*/
                        if (r_ptr->flags3 & RF3_NO_STUN)
                        {
                            monster_lore* l_ptr = &l_list[m_ptr->r_idx];

                            /*mark the lore*/
                            if (m_ptr->ml)
                                l_ptr->flags3 |= (RF3_NO_STUN);
                        }

                        else
                        {
                            stun_monster(m_ptr, net_dam * 4);
                        }

                        // Alert it
                        set_alertness(m_ptr,
                            MAX(m_ptr->alertness + 10, ALERTNESS_VERY_ALERT));

                        // message for non-visible monsters
                        if (!m_ptr->ml)
                            message_pain(cave_m_idx[y][x], damage);
                    }
                }
            }

            // squares without monsters/player will sometimes get transformed
            // (note that the monster may have been there but got killed by now)
            if ((cave_m_idx[y][x] == 0) && percent_chance(damage)
                && !((y == pit_y) && (x == pit_x)))
            {
                /* Destroy location (if valid) */
                if (cave_valid_bold(y, x))
                {
                    int adj_chasms = 0;

                    /* Delete objects */
                    delete_object(y, x);

                    // count adjacent chasm squares
                    for (i = 0; i < 8; i++)
                    {
                        /* Get the location */
                        yy = y + ddy_ddd[i];
                        xx = x + ddx_ddd[i];

                        // count the chasms
                        if (cave_feat[yy][xx] == FEAT_CHASM)
                            adj_chasms++;
                    }

                    /* Wall (or floor) type */
                    t = rand_int(100);

                    // if we started with a chasm
                    if (cave_feat[y][x] == FEAT_CHASM)
                    {
                        // mostly leave it unchanged
                        if (one_in_(10))
                        {
                            if (t < 10)
                                feat = FEAT_RUBBLE;
                            else if (t < 70)
                                feat = FEAT_WALL_EXTRA;
                            else
                                feat = FEAT_QUARTZ;
                        }
                        else
                        {
                            feat = FEAT_CHASM;
                        }
                    }

                    // if we started with open floor
                    else if (cave_floor_bold(y, x))
                    {
                        if (dieroll(8) <= adj_chasms + 1)
                            feat = FEAT_CHASM;

                        else if (t < 40)
                            feat = FEAT_RUBBLE;
                        else if (t < 80)
                            feat = FEAT_WALL_EXTRA;
                        else
                            feat = FEAT_QUARTZ;
                    }

                    // if we started with rubble
                    else if (cave_feat[y][x] == FEAT_RUBBLE)
                    {
                        if (dieroll(32) <= adj_chasms)
                            feat = FEAT_CHASM;

                        else if (t < 40)
                            feat = FEAT_FLOOR;
                        else if (t < 70)
                            feat = FEAT_WALL_EXTRA;
                        else
                            feat = FEAT_QUARTZ;
                    }

                    // if we started with a wall of some sort
                    else
                    {
                        if (dieroll(32) <= adj_chasms)
                            feat = FEAT_CHASM;

                        if (t < 80)
                            feat = FEAT_RUBBLE;
                        else
                            feat = FEAT_FLOOR;
                    }

                    // change the feature (unless it would be making a chasm at
                    // 1000 ft)
                    if (!((feat == FEAT_CHASM)
                            && (p_ptr->depth >= MORGOTH_DEPTH)))
                    {
                        cave_info[y][x] &= ~(CAVE_MARK);

                        cave_set_feat(y, x, feat);
                    }
                }
            }
        }
    }

    // Step 3:
    // Miscellaneous stuff

    // Fall into the pit if there were no safe squares to jump to
    if (fall_into_pit && cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        // Store information for the combat rolls window
        combat_roll_special_char = (&f_info[FEAT_TRAP_PIT])->d_char;
        combat_roll_special_attr = (&f_info[FEAT_TRAP_PIT])->d_attr;

        msg_print("You fall back into the newly made pit!");

        /* Falling damage */
        damage = damroll(2, 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(2, 4, damage, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(damage, "falling into a pit");
    }

    /* Make a lot of noise */
    monster_perception(true, false, -30);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Update the health bar */
    p_ptr->redraw |= (PR_HEALTHBAR);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);
}

/*
 * Attempt to close a single square of chasm.
 * Used by the function below (for Staff of Freedom) and by the Song of Freedom.
 */
bool close_chasm(int y, int x, int power)
{
    int adj_chasms = 0;
    int yy, xx;
    bool effect = false;

    for (yy = y - 1; yy <= y + 1; yy++)
    {
        for (xx = x - 1; xx <= x + 1; xx++)
        {
            if (!((yy == y) && (xx == x)) && in_bounds(yy, xx)
                && (cave_feat[yy][xx] == FEAT_CHASM))
                adj_chasms++;
        }
    }

    // cannot close chasms that are completely surrounded
    if (adj_chasms < 8)
    {
        if (skill_check(PLAYER, power, 20 + adj_chasms, NULL) > 0)
        {
            cave_info[y][x] |= (CAVE_TEMP);
            effect = true;
        }
    }

    return (effect);
}

/*
 * Attempt to close chasms.
 * Can't be done with project as it would depend on the order the grids are
 * processed.
 */
bool close_chasms(int power)
{
    int y, x;
    bool effect = false;

    // first find all chasms and mark those that are being closed
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if ((cave_feat[y][x] == FEAT_CHASM)
                && (cave_info[y][x] & (CAVE_VIEW)))
            {
                effect |= close_chasm(y, x, power);
            }
        }
    }

    // then, if any were marked, do the closing
    if (effect)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_CHASM)
                    && (cave_info[y][x] & (CAVE_TEMP)))
                {
                    // remove the temporary marking
                    cave_info[y][x] &= ~(CAVE_TEMP);

                    // close the chasm
                    cave_set_feat(y, x, FEAT_FLOOR);

                    // update the visuals
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }

    return (effect);
}

/*
 * This routine clears the entire "temp" set.
 *
 * This routine will Perma-Lite all "temp" grids.
 *
 * This routine is used (only) by "light_room()"
 *
 * Dark grids are illuminated.
 *
 * Also, process all affected monsters.
 *
 * SMART monsters always wake up when illuminated
 * NORMAL monsters wake up 1/4 the time when illuminated
 * MINDLESS monsters wake up 1/10 the time when illuminated
 */
static void cave_temp_room_light(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);

        /* Perma-Lite */
        cave_info[y][x] |= (CAVE_GLOW);
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Update stuff */
    update_stuff();

    /* Process the grids */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* Redraw the grid */
        lite_spot(y, x);

        /* Process affected monsters */
        if (cave_m_idx[y][x] > 0)
        {
            int alerting_power = damroll(2, 10);

            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Mindless monsters rarely wake up */
            if (r_ptr->flags2 & (RF2_MINDLESS))
                alerting_power /= 2;

            /* Smart monsters mostly wake up */
            if (r_ptr->flags2 & (RF2_SMART))
                alerting_power *= 2;

            /* Alert unwary/sleeping monsters to a degree */
            if (m_ptr->alertness < ALERTNESS_UNWARY)
            {
                set_alertness(m_ptr,
                    MIN(m_ptr->alertness + alerting_power, ALERTNESS_ALERT));

                /*possibly update the monster health bar*/
                if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
                    p_ptr->redraw |= (PR_HEALTHBAR);
            }
        }
    }

    /* None left */
    temp_n = 0;
}

/*
 * This routine clears the entire "temp" set.
 *
 * This routine will "darken" all "temp" grids.
 *
 * In addition, some of these grids will be "unmarked".
 *
 * This routine is used (only) by "darken_room()"
 */
static void cave_temp_room_darken(void)
{
    int i;

    /* Apply flag changes */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* No longer in the array */
        cave_info[y][x] &= ~(CAVE_TEMP);

        /* Darken the grid */
        cave_info[y][x] &= ~(CAVE_GLOW);

        /* Hack -- Forget "boring" grids */
        if (cave_floorlike_bold(y, x))
        {
            /* Forget the grid */
            cave_info[y][x] &= ~(CAVE_MARK);
        }
    }

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Update stuff */
    update_stuff();

    /* Process the grids */
    for (i = 0; i < temp_n; i++)
    {
        int y = temp_y[i];
        int x = temp_x[i];

        /* Redraw the grid */
        lite_spot(y, x);
    }

    /* None left */
    temp_n = 0;
}

/*
 * Aux function -- see below
 */
static void cave_temp_room_aux(int y, int x)
{
    /* Avoid infinite recursion */
    if (cave_info[y][x] & (CAVE_TEMP))
        return;

    /* Do not "leave" the current room */
    if (!(cave_info[y][x] & (CAVE_ROOM)))
        return;

    /* Paranoia -- verify space */
    if (temp_n == TEMP_MAX)
        return;

    /* Mark the grid as "seen" */
    cave_info[y][x] |= (CAVE_TEMP);

    /* Add it to the "seen" set */
    temp_y[temp_n] = y;
    temp_x[temp_n] = x;
    temp_n++;
}

/*
 * Illuminate any room containing the given location.
 */
void light_room(int y1, int x1)
{
    int i, x, y;

    /* Add the initial grid */
    cave_temp_room_aux(y1, x1);

    /* While grids are in the queue, add their neighbors */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get lit, but stop light */
        if (!cave_floor_bold(y, x))
            continue;

        /* Spread adjacent */
        cave_temp_room_aux(y + 1, x);
        cave_temp_room_aux(y - 1, x);
        cave_temp_room_aux(y, x + 1);
        cave_temp_room_aux(y, x - 1);

        /* Spread diagonal */
        cave_temp_room_aux(y + 1, x + 1);
        cave_temp_room_aux(y - 1, x - 1);
        cave_temp_room_aux(y - 1, x + 1);
        cave_temp_room_aux(y + 1, x - 1);
    }

    /* Now, lite them all up at once */
    cave_temp_room_light();
}

/*
 * Darken all rooms containing the given location
 */
void darken_room(int y1, int x1)
{
    int i, x, y;

    /* Add the initial grid */
    cave_temp_room_aux(y1, x1);

    /* Spread, breadth first */
    for (i = 0; i < temp_n; i++)
    {
        x = temp_x[i], y = temp_y[i];

        /* Walls get dark, but stop darkness */
        if (!cave_floor_bold(y, x))
            continue;

        /* Spread adjacent */
        cave_temp_room_aux(y + 1, x);
        cave_temp_room_aux(y - 1, x);
        cave_temp_room_aux(y, x + 1);
        cave_temp_room_aux(y, x - 1);

        /* Spread diagonal */
        cave_temp_room_aux(y + 1, x + 1);
        cave_temp_room_aux(y - 1, x - 1);
        cave_temp_room_aux(y - 1, x + 1);
        cave_temp_room_aux(y + 1, x - 1);
    }

    /* Now, darken them all at once */
    cave_temp_room_darken();
}

/*
 * Hack -- call light around the player
 * Affect all monsters in the projection radius
 */
bool light_area(int dd, int ds, int rad)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_KILL | PROJECT_PASS;

    /* Hack -- Message */
    if (!p_ptr->blind)
    {
        msg_print("You are surrounded by a white light.");
    }

    /* Hook into the "project()" function */
    (void)project(-1, rad, py, px, py, px, dd, ds, -1, GF_LIGHT, flg, 0, false);

    /* Assume seen */
    return (true);
}

/*
 * Hack -- call darkness around the player
 * Affect all monsters in the projection radius
 */
bool darken_area(int dd, int ds, int rad)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_KILL;

    /* Hack -- Message */
    if (!p_ptr->blind)
    {
        msg_print("Darkness surrounds you.");
    }

    /* Hook into the "project()" function */
    (void)project(
        -1, rad, py, px, py, px, dd, ds, -1, GF_DARK_WEAK, flg, 0, false);

    /* Darken the room */
    darken_room(py, px);

    /* Assume seen */
    return (true);
}

/*
 * Some of the old functions
 */

bool light_line(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID;
    return (fire_bolt_beam_special(GF_LIGHT, dir, 6, 4, -1, MAX_RANGE, flg));
}

bool destroy_door(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
    return (
        fire_bolt_beam_special(GF_KILL_DOOR, dir, 0, 0, -1, MAX_RANGE, flg));
}

bool disarm_trap(int dir)
{
    u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
    return (
        fire_bolt_beam_special(GF_KILL_TRAP, dir, 0, 0, -1, MAX_RANGE, flg));
}
