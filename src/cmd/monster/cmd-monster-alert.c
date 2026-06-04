#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include <math.h>

void new_wandering_flow(monster_type* m_ptr, int ty, int tx)
{
    int y, x, i;
    int wandering_idx = m_ptr->wandering_idx;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (wandering_idx < FLOW_WANDERING_HEAD)
    {
        return;
    }

    // territorial monsters target their creation location
    // same with the tutorial
    if ((r_ptr->flags2 & (RF2_TERRITORIAL)) || (p_ptr->game_type < 0))
    {
        // they only pick a new location on creation
        // Sil-y: Hack: using the fact that speed hasn't been determined yet on
        // creation
        if (m_ptr->mspeed == 0)
        {
            // update the flow
            update_flow(m_ptr->fy, m_ptr->fx, wandering_idx);
        }
    }

    // if a location was requested, use that
    else if (in_bounds_fully(ty, tx))
    {
        y = ty;
        x = tx;

        // update the flow
        update_flow(y, x, wandering_idx);
    }

    // otherwise choose a location
    else
    {
        // sometimes intelligent monsters want to pick a staircase and leave the
        // level
        if ((r_ptr->flags2 & (RF2_SMART))
            && !(r_ptr->flags2 & (RF2_TERRITORIAL))
            && (p_ptr->depth != MORGOTH_DEPTH) && one_in_(5)
            && random_stair_location(&y, &x) && (cave_m_idx[y][x] >= 0)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            // update the flow
            update_flow(y, x, wandering_idx);
        }

        // otherwise pick a random location (on a floor, in a room, and not in a
        // vault)
        else
        {
            // give up after 100 tries
            for (i = 0; i < 100; i++)
            {
                y = rand_int(p_ptr->cur_map_hgt);
                x = rand_int(p_ptr->cur_map_wid);
                if (in_bounds_fully(y, x) && (cave_feat[y][x] == FEAT_FLOOR)
                    && (cave_info[y][x] & (CAVE_ROOM))
                    && !(cave_info[y][x] & (CAVE_ICKY)))
                {
                    // update the flow
                    update_flow(y, x, wandering_idx);
                    break;
                }
            }
        }
    }

    // reset the pause (if any)
    wandering_pause[wandering_idx] = 0;
}

/*
 * Determines a wandering-destination for a monster.
 * default_idx_ptr is the wandering index to use by default, and gets updated by
 * this function.
 */
void new_wandering_destination(monster_type* m_ptr, monster_type* leader_ptr)
{
    int i;
    bool wandering_indices[FLOW_WANDERING_TAIL + 1];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    // many monsters don't get wandering destinations:
    if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
        || (r_ptr->flags1 & (RF1_HIDDEN_MOVE))
        || !((r_ptr->flags2 & (RF2_SMART)) || (r_ptr->flags4 & (RF4_SHRIEK))))
    {
        return;
    }

    // there is a special way of finding indices at the Gates level
    // as otherwise we run out too quickly
    if (p_ptr->depth == 0)
    {
        // mark the used indices
        for (i = 1; i < mon_max; i++)
        {
            monster_type* n_ptr = &mon_list[i];

            /* Skip dead monsters */
            if (!n_ptr->r_idx)
                continue;

            if ((n_ptr->r_idx == m_ptr->r_idx) && one_in_(2))
                leader_ptr = n_ptr;
        }
    }

    // find a new index if one is not specified
    if (leader_ptr != NULL)
    {
        i = leader_ptr->wandering_idx;
    }
    else
    {
        // clear the index array
        for (i = 0; i <= FLOW_WANDERING_TAIL; i++)
        {
            wandering_indices[i] = false;
        }

        // mark the used indices
        for (i = 1; i < mon_max; i++)
        {
            monster_type* n_ptr = &mon_list[i];

            /* Skip dead monsters */
            if (!n_ptr->r_idx)
                continue;

            wandering_indices[n_ptr->wandering_idx] = true;
        }

        // find the smallest unused index
        for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
        {
            if (!wandering_indices[i])
                break;
        }
    }

    // if we have a valid index, then find a location and build the noise flow
    if (i <= FLOW_WANDERING_TAIL)
    {
        m_ptr->wandering_idx = i;
        m_ptr->wandering_dist = MON_WANDER_RANGE;
        new_wandering_flow(m_ptr, 0, 0);
    }

    // if we can't store any more indices, then just set it to zero, which means
    // that the monster will just move randomly and won't wander properly
    // this is very rare, but does occasionally happen (1 in 100 deep levels?)
    else
    {
        // msg_debug("Out of wandering monster indices.");
        m_ptr->wandering_idx = 0;
        m_ptr->wandering_dist = MON_WANDER_RANGE;
    }
}

/*
 * Makes Morgoth drop his Iron Crown with an appropriate message.
 */

void drop_iron_crown(monster_type* m_ptr, const char* msg)
{
    int i, near_y, near_x;

    log_debug("drop_iron_crown: called, ART_MORGOTH_3 cur_num=%d", 
              (&a_info[ART_MORGOTH_3])->cur_num);

    if ((&a_info[ART_MORGOTH_3])->cur_num == 0)
    {
        log_debug("drop_iron_crown: crown not yet dropped, dropping now");
        msg_print(msg);

        // choose a nearby location, but not his own square
        for (i = 0; i < 1000; i++)
        {
            near_y = m_ptr->fy + rand_range(-1, 1);
            near_x = m_ptr->fx + rand_range(-1, 1);

            if (((near_y != m_ptr->fy) || (near_x != m_ptr->fx))
                && cave_floor_bold(near_y, near_x))
                break;
        }

        log_debug("drop_iron_crown: dropping crown at (%d, %d)", near_y, near_x);
        
        // drop it there
        create_chosen_artefact(ART_MORGOTH_3, near_y, near_x, true);

        log_debug("drop_iron_crown: calling anger_morgoth(1) - crown lost");
        // lower Morgoth's protection, remove his light source, increase his
        // will and perception and evasion
        anger_morgoth(1);
    }
    else
    {
        log_debug("drop_iron_crown: crown already dropped, skipping");
    }
}

void make_alert(monster_type* m_ptr)
{
    int random_level = rand_range(ALERTNESS_ALERT, ALERTNESS_QUITE_ALERT);
    set_alertness(m_ptr, MAX(m_ptr->alertness, random_level));
}

/*
 * Changes a monster's alertness value and displays any appropriate messages
 */
void set_alertness(monster_type* m_ptr, int alertness)
{
    char m_name[80];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool redisplay = false;
    bool is_non_alert_thrall =
        m_ptr->r_idx == R_IDX_HUMAN_THRALL || m_ptr->r_idx == R_IDX_ELF_THRALL;

    // cap the alertness value
    if (alertness < ALERTNESS_MIN)
        alertness = ALERTNESS_MIN;
    if (alertness > ALERTNESS_MAX)
        alertness = ALERTNESS_MAX;

    if (monster_race_is_vala(m_ptr->r_idx))
    {
        alertness = ALERTNESS_ALERT;
        if (monster_clear_vala_state(m_ptr))
            calc_monster_speed(m_ptr->fy, m_ptr->fx);
    }

    // Nothing to be done...
    if (m_ptr->alertness == alertness)
        return;

    // Can't alert non-alert thralls so cap alertness lower for them
    if (is_non_alert_thrall && alertness >= ALERTNESS_UNWARY)
    {
        alertness = ALERTNESS_UNWARY;
    }

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    // First deal with cases where the monster becomes more alert
    if (m_ptr->alertness < alertness)
    {
        if ((m_ptr->alertness < ALERTNESS_UNWARY)
            && (alertness >= ALERTNESS_ALERT))
        {
            // Monster must spend its next turn noticing you
            m_ptr->skip_next_turn = true;

            // Notice the "waking up and noticing"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s wakes up and notices you.", m_name);

                // disturb the player
                disturb(1, 0);

                // redisplay the monster
                redisplay = true;
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_UNWARY)
            && (alertness >= ALERTNESS_UNWARY))
        {
            // Notice the "waking up"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s wakes up.", m_name);

                // disturb the player
                disturb(1, 0);

                // redisplay the monster
                redisplay = true;
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_ALERT)
            && (alertness >= ALERTNESS_ALERT))
        {
            // Monster must spend its next turn noticing you
            m_ptr->skip_next_turn = true;

            // Notice the "noticing" (!)
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s notices you.", m_name);

                // disturb the player
                disturb(1, 0);

                // redisplay the monster
                redisplay = true;
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_UNWARY)
            && (alertness < ALERTNESS_UNWARY)
            && (alertness >= ALERTNESS_UNWARY - 2))
        {
            // Notice the "stirring"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s stirs.", m_name);
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_ALERT)
            && (alertness < ALERTNESS_ALERT)
            && (alertness >= ALERTNESS_ALERT - 2))
        {
            // Notice the "looking around"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s looks around.", m_name);
            }
        }
    }
    // First deal with cases where the monster becomes less alert
    else
    {
        if ((m_ptr->alertness >= ALERTNESS_UNWARY)
            && (alertness < ALERTNESS_UNWARY))
        {
            // Notice the falling asleep
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s falls asleep.", m_name);

                // redisplay the monster
                redisplay = true;
            }
            if (m_ptr->r_idx == R_IDX_MORGOTH)
            {
                // Dump a message
                msg_format("%^s falls asleep.", m_name);

                // redisplay the monster
                redisplay = true;

                drop_iron_crown(m_ptr,
                    "His crown slips from off his brow and falls to the "
                    "ground nearby.");
            }
        }
        else if ((m_ptr->alertness >= ALERTNESS_ALERT)
            && (alertness < ALERTNESS_ALERT))
        {
            // Notice the becoming unwary
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s becomes unwary.", m_name);

                // redisplay the monster
                redisplay = true;

                // give the monster a new place to wander towards
                if (!(r_ptr->flags2 & (RF2_TERRITORIAL)))
                    new_wandering_flow(m_ptr, p_ptr->py, p_ptr->px);
            }
        }
        else if (alertness < ALERTNESS_UNWARY)
        {
            // Notice the deepening sleep
            if (m_ptr->ml)
            {
                // Dump a message
                // msg_format("%^s's sleep deepens.", m_name);
            }
        }
        else if (alertness < ALERTNESS_ALERT)
        {
            // Notice the increasing unwariness
            if (m_ptr->ml)
            {
                // Dump a message
                // msg_format("%^s becomes more unwary.", m_name);
            }
        }
        else
        {
            // Notice the decreasing alertness
            if (m_ptr->ml)
            {
                // Dump a message
                // msg_format("%^s looks less alert.", m_name);
            }
        }
    }

    // do the actual alerting
    m_ptr->alertness = alertness;

    // redisplay the monster
    if (redisplay)
        lite_spot(m_ptr->fy, m_ptr->fx);
}
