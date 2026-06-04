/* File: spell/spell-teleport.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

void teleport_away(int m_idx, int dis)
{
    int ny, nx, oy, ox, d, i, min;

    bool look = true;

    monster_type* m_ptr = &mon_list[m_idx];

    /* Paranoia */
    if (!m_ptr->r_idx)
        return;

    /* Save the old location */
    oy = m_ptr->fy;
    ox = m_ptr->fx;

    /* Minimum distance */
    min = dis / 2;

    /* Look until done */
    while (look)
    {
        /* Verify max distance */
        if (dis > 200)
            dis = 200;

        /* Try several locations */
        for (i = 0; i < 500; i++)
        {
            /* Pick a (possibly illegal) location */
            while (1)
            {
                ny = rand_spread(oy, dis);
                nx = rand_spread(ox, dis);
                d = distance(oy, ox, ny, nx);
                if ((d >= min) && (d <= dis))
                    break;
            }

            /* Ignore illegal locations */
            if (!in_bounds_fully(ny, nx))
                continue;

            /* Require "empty" floor space */
            if (!cave_empty_bold(ny, nx))
                continue;

            /* Hack -- no teleport onto glyph of warding */
            if (cave_glyph(ny, nx))
                continue;

            /* No teleporting into vaults and such */
            /* if (cave_info[ny][nx] & (CAVE_ICKY)) continue; */

            /* This grid looks good */
            look = false;

            /* Stop looking */
            break;
        }

        /* Increase the maximum distance */
        dis = dis * 2;

        /* Decrease the minimum distance */
        min = min / 2;
    }

    /* Sound */
    sound(MSG_TPOTHER);

    /*the monster should re-evaluate their target*/
    m_ptr->target_y = 0;
    m_ptr->target_x = 0;

    /* Swap the monsters */
    monster_swap(oy, ox, ny, nx);
}

/*
 * Teleport the player to a location up to "dis" grids away.
 *
 * If no such spaces are readily available, the distance may increase.
 * Try very hard to move the player at least a quarter that distance.
 */
void teleport_player(int dis)
{
    int x_location_tables[20];
    int y_location_tables[20];
    int spot_counter = 0;

    int py = p_ptr->py;
    int px = p_ptr->px;

    int d, i, min, y, x;

    bool look = true;

    /* Minimum distance */
    min = dis / 2;

    /*guage the dungeon size*/
    d = distance(p_ptr->cur_map_hgt, p_ptr->cur_map_wid, 0, 0);

    /*first start with a realistic range*/
    if (dis > d)
        dis = d;

    /*must have a realistic minimum*/
    if (min > (d * 4 / 10))
    {
        min = (d * 4 / 10);
    }

    /* Look until done */
    while (look)
    {
        /*find the allowable range*/
        int min_y = MAX((py - dis), 0);
        int min_x = MAX((px - dis), 0);
        int max_y = MIN((py + dis), (p_ptr->cur_map_hgt - 1));
        int max_x = MIN((px + dis), (p_ptr->cur_map_wid - 1));

        /* Try several locations */
        for (i = 0; i < 10000; i++)
        {
            /* Pick a (possibly illegal) location */
            y = rand_range(min_y, max_y);
            x = rand_range(min_x, max_x);
            d = distance(py, px, y, x);
            if ((d <= min) || (d >= dis))
                continue;

            /*only open floor space*/
            if (!cave_naked_bold(y, x))
                continue;

            /* No teleporting into vaults and such */
            // if (cave_info[y][x] & (CAVE_ICKY)) continue;

            /*don't go over size of array*/
            if (spot_counter < 20)
            {
                x_location_tables[spot_counter] = x;
                y_location_tables[spot_counter] = y;

                /*increase the counter*/
                spot_counter++;
            }

            /*we have enough spots, keep looking*/
            if (spot_counter == 20)
            {
                /* This grid looks good */
                look = false;

                /* Stop looking */
                break;
            }
        }

        /*we have enough random spots*/
        if (spot_counter > 3)
            break;

        /* Increase the maximum distance */
        dis = dis * 2;

        /* Decrease the minimum distance */
        min = min * 6 / 10;
    }

    i = rand_int(spot_counter);

    /* Mark the location */
    x = x_location_tables[i];
    y = y_location_tables[i];

    /* Sound */
    sound(MSG_TELEPORT);

    /* Move player */
    monster_swap(py, px, y, x);

    /* Handle stuff XXX XXX XXX */
    handle_stuff();
}

/*
 * Teleport player to a grid near the given location
 *
 * This function is slightly obsessive about correctness.
 * This function allows teleporting into vaults (!)
 */
void teleport_player_to(int ny, int nx)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    int dis = 0, ctr = 0;

    /* Initialize */
    y = py;
    x = px;

    /* Find a usable location */
    while (1)
    {
        /* Pick a nearby legal location */
        while (1)
        {
            y = rand_spread(ny, dis);
            x = rand_spread(nx, dis);
            if (in_bounds_fully(y, x))
                break;
        }

        /* Accept "naked" floor grids */
        if (cave_naked_bold(y, x))
            break;

        /* Occasionally advance the distance */
        if (++ctr > (4 * dis * dis + 4 * dis + 1))
        {
            ctr = 0;
            dis++;
        }
    }

    /* Sound */
    sound(MSG_TELEPORT);

    /* Move player */
    monster_swap(py, px, y, x);

    /* Handle stuff XXX XXX XXX */
    handle_stuff();
}

/*
 * Teleport monster to a grid near the given location.  This function is
 * used in the monster spell "TELE_SELF_TO", to allow monsters both to
 * suddenly jump near the character, and to make them "dance" around the
 * character.
 *
 * Usually, monster will teleport to a grid that is not more than 4
 * squares away from the given location, and not adjacent to the given
 * location.  These restrictions are relaxed if necessary.
 *
 * This function allows teleporting into vaults.
 */
void teleport_towards(int oy, int ox, int ny, int nx)
{
    int y, x;

    int dist;
    int ctr = 0;
    int min = 2, max = 4;

    /* Find a usable location */
    while (true)
    {
        /* Pick a nearby legal location */
        while (true)
        {
            y = rand_spread(ny, max);
            x = rand_spread(nx, max);
            if (in_bounds_fully(y, x))
                break;
        }

        /* Consider all empty grids */
        if (cave_empty_bold(y, x))
        {
            /*Don't allow monster to teleport onto glyphs*/
            if (cave_glyph(y, x))
                continue;

            /* Calculate distance between target and current grid */
            dist = distance(ny, nx, y, x);

            /* Accept grids that are the right distance away. */
            if ((dist >= min) && (dist <= max))
                break;
        }

        /* Occasionally relax the constraints */
        if (++ctr > 15)
        {
            ctr = 0;

            max++;
            if (max > 5)
                min = 0;
        }
    }

    /* Sound (assumes monster is moving) */
    sound(MSG_TPOTHER);

    /* Move monster */
    monster_swap(oy, ox, y, x);

    /* Handle stuff XXX XXX XXX */
    handle_stuff();
}

/*
 * Teleport the player one level up or down (random when legal)
 */
void teleport_player_level()
{
    bool go_up = false;
    bool go_down = false;

    if (birth_ironman)
    {
        msg_print("Nothing happens.");
        return;
    }

    if (!p_ptr->depth)
        go_down = true;

    /*
     * the bottom of the dungeon.
     */
    if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        go_up = true;
    }

    /*
     * the surface.
     */
    if (p_ptr->depth == 0)
    {
        go_down = true;
    }

    /*We don't have a direction yet, pick one at random*/
    if ((!go_up) && (!go_down))
    {
        if (one_in_(2))
            go_up = true;
        else
            go_down = true;
    }

    /*up*/
    if (go_up == true)
    {
        message(MSG_TPLEVEL, 0, "You rise up through the ceiling.");

        varda_quest_fail_if_bastion_missed();

        // make a note if the player loses a greater vault
        note_lost_greater_vault();

        /* New depth */
        p_ptr->depth--;

        /* Leaving */
        p_ptr->leaving = true;
    }

    else
    {
        message(MSG_TPLEVEL, 0, "You sink through the floor.");

        varda_quest_fail_if_bastion_missed();

        // make a note if the player loses a greater vault
        note_lost_greater_vault();

        /* New depth */
        p_ptr->depth++;

        /* Leaving */
        p_ptr->leaving = true;
    }
}

