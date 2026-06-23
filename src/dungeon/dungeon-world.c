/* File: dungeon/dungeon-world.c */

#include "angband.h"
#include "dungeon-internal.h"

/*
 *  Determines how many points of health/song is regenerated next round
 *  assuming it increases by 'max' points every 'regen_period'.
 *  Note that players use 'playerturn' and monsters use 'turn'.
 *  This lets hasted players regenerate at the right speed.
 */

int regen_amount(int turn_number, int max, int regen_period)
{
    int regen_so_far, regen_next;

    if (turn_number == 0)
    {
        /* do nothing on the first turn of the game */
        return (0);
    }
    if ((turn_number % regen_period) > 0)
    {
        regen_so_far
            = (max * ((turn_number - 1) % regen_period)) / regen_period;
        regen_next = (max * ((turn_number) % regen_period)) / regen_period;
    }
    else
    {
        regen_so_far
            = (max * ((turn_number - 1) % regen_period)) / regen_period;
        regen_next = (max * (regen_period)) / regen_period;
    }

    return (regen_next - regen_so_far);
}

/*
 * Regenerate hit points
 */
void regenhp(int regen_multiplier)
{
    int old_chp;

    // exit immediately if the multiplier is zero (avoids div by zero error)
    if (regen_multiplier == 0)
        return;

    /* Save the old hitpoints */
    old_chp = p_ptr->chp;

    /* Work out how much increase is due */
    /* where the player should get completely healed every PY_REGEN_HP_PERIOD
     * player turns */

    p_ptr->chp += regen_amount(
        playerturn, p_ptr->mhp, PY_REGEN_HP_PERIOD / regen_multiplier);

    /* Fully healed */
    if (p_ptr->chp >= p_ptr->mhp)
    {
        p_ptr->chp = p_ptr->mhp;
    }

    /* Notice changes */
    if (old_chp != p_ptr->chp)
    {
        /* Redraw */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}

/*
 * Regenerate mana points
 */
void regenmana(int regen_multiplier)
{
    int old_csp;

    // exit immediately if the multiplier is zero (avoids div by zero error)
    if (regen_multiplier == 0)
        return;

    // don't regenerate voice if singing
    if (!singing(SNG_NOTHING))
        return;

    /* Save the old hitpoints */
    old_csp = p_ptr->csp;

    /* Work out how much increase is due */
    /* where the player should get completely recovered every PY_REGEN_SP_PERIOD
     * player turns */

    p_ptr->csp += regen_amount(
        playerturn, p_ptr->msp, PY_REGEN_SP_PERIOD / regen_multiplier);

    /* Fully recovered */
    if (p_ptr->csp >= p_ptr->msp)
    {
        p_ptr->csp = p_ptr->msp;
    }

    /* Redraw mana */
    if (old_csp != p_ptr->csp)
    {
        /* Redraw */
        p_ptr->redraw |= (PR_VOICE);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}

/*
 * Regenerate the monsters (once per 100 game turns)
 */

static void regen_monsters(void)
{
    int i;
    int regen_period;

    /* Regenerate everyone */
    for (i = 1; i < mon_max; i++)
    {
        /* Check the i'th monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Allow hp regeneration, if needed. */
        if (m_ptr->hp != m_ptr->maxhp)
        {
            /* Some monsters regenerate quickly */
            if (r_ptr->flags2 & (RF2_REGENERATE))
            {
                regen_period = MON_REGEN_HP_PERIOD / 5;
            }
            else
            {
                regen_period = MON_REGEN_HP_PERIOD;
            }

            m_ptr->hp += regen_amount(turn / 10, m_ptr->maxhp, regen_period);

            /* Do not over-regenerate */
            if (m_ptr->hp > m_ptr->maxhp)
                m_ptr->hp = m_ptr->maxhp;

            /* Fully healed -> flag minimum range for recalculation */
            if (m_ptr->hp == m_ptr->maxhp)
                m_ptr->min_range = 0;
        }

        /* Allow mana regeneration, if needed. */
        if (m_ptr->mana != MON_MANA_MAX)
        {
            // can only regenerate mana if not singing
            if (m_ptr->song == SNG_NOTHING)
            {
                m_ptr->mana += regen_amount(
                    turn / 10, MON_MANA_MAX, MON_REGEN_SP_PERIOD);

                /* Do not over-regenerate */
                if (m_ptr->mana > MON_MANA_MAX)
                    m_ptr->mana = MON_MANA_MAX;

                /* Fully healed -> flag minimum range for recalculation */
                if (m_ptr->mana == MON_MANA_MAX)
                    m_ptr->min_range = 0;
            }
        }
    }
}


/*
 * Scan for artifacts within 22 tiles of player and mark them as seen.
 * This allows players to skip full exploration while still tracking artifacts.
 * Only scans the area that changed (player moved or objects shifted).
 */
void scan_artifacts_near_player(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int radius = 22;
    
    /* Scan 44x44 area centered on player */
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            int y = py + dy;
            int x = px + dx;
            
            /* Skip out of bounds */
            if (!in_bounds(y, x))
                continue;

            /* Only consider grids the player can actually see */
            if (!player_can_see_bold(y, x))
                continue;
            
            /* Check for objects at this location */
            s16b this_o_idx = cave_o_idx[y][x];
            
            while (this_o_idx)
            {
                object_type* o_ptr = &o_list[this_o_idx];
                
                /* If this is an artifact that hasn't been marked seen yet */
                if (o_ptr->name1
                    && !(a_info[o_ptr->name1].seen & ART_SEEN_PHYSICAL))
                {
                    a_info[o_ptr->name1].seen |= ART_SEEN_PHYSICAL;
                    
                    /* Optional: log for debugging */
                    if (cheat_peek)
                    {
                        char o_name[80];
                        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                        msg_format("Artifact marked as seen: %s", o_name);
                    }
                }
                
                /* Next object in this square */
                this_o_idx = o_ptr->next_o_idx;
            }
        }
    }
}

/*
 * Handle certain things once every 10 game turns
 */
void process_world(void)
{
    int i, j;

    object_type* o_ptr;

    bool was_ghost = false;

    /* Check for Tulkas quest interaction every turn */
    check_tulkas_quest_interaction();

    /* Check for Aulë quest interaction every turn */
    check_aule_quest_interaction();

    /* Check for Nienna quest interaction every turn */
    check_niena_quest_interaction();

    /* Check for Oromë quest interaction every turn */
    check_orome_quest_interaction();

    /* Check for Varda quest interaction every turn */
    check_varda_quest_interaction();

    /* Stop now unless the turn count is divisible by 10 */
    if (turn % 10)
        return;

    /*** Check the Time and Load ***/
    if (!(turn % 1000))
    {
        /* Check time and load */
        if (0 != check_time())
        {
            /* Warning */
            if (closing_flag <= 2)
            {
                /* Disturb */
                disturb(0, 0);

                /* Count warnings */
                closing_flag++;

                /* Message */
                msg_print("The gates to ANGBAND are closing...");
                msg_print("Please finish up and/or save your game.");
            }

            /* Slam the gate */
            else
            {
                /* Message */
                msg_print("The gates to ANGBAND are now closed.");

                /* Stop playing */
                p_ptr->playing = false;

                /* Leaving */
                p_ptr->leaving = true;
            }
        }
    }

    /*** Handle the "surface" ***/

    /* While on the surface */
    if (p_ptr->depth == 0)
    {
        if (percent_chance(10))
        {
            /* Make a new monster */
            (void)alloc_monster(true, false);
        }
    }

    /*** Process the monsters ***/

    /* Hack - see if there is already a player ghost on the level */
    if (bones_selector)
        was_ghost = true;

    /* Vastly more wandering monsters during the endgame when you have 2 or 3
     * Silmarils */
    if (silmarils_possessed() >= 2)
    {
        int percent = (p_ptr->cur_map_hgt * p_ptr->cur_map_wid)
            / (PANEL_HGT * PANEL_WID_FIXED);

        if (percent_chance(percent))
        {
            /* Make a new monster */
            (void)alloc_monster(true, false);
        }
    }

    /* Check for normal wandering monster generation */
    else if (one_in_(MAX_M_ALLOC_CHANCE))
    {
        /* Make a new monster */
        (void)alloc_monster(true, false);
    }

    // Players with the haunted curse attract wraiths
    if (percent_chance(p_ptr->haunted))
    {
        /* Make a new wraith */
        (void)alloc_monster(true, true);
    }

    /* Hack - if there is a ghost now, and there was not before,
     * give a challenge */
    if ((bones_selector) && (!(was_ghost)))
        ghost_challenge();

    /* Regenerate creatures */
    regen_monsters();

    /*** Process Light ***/

    /* Check for light being wielded */
    o_ptr = &inventory[INVEN_LITE];

    /* Burn some fuel in the current lite */
    if (o_ptr->tval == TV_LIGHT)
    {
        /* Hack -- Use some fuel */
        if (player_light_has_fuel(o_ptr)
            && !((o_ptr->sval == SV_LIGHT_LANTERN)
                && (object_ego_prefix(o_ptr) == EGO_BROKEN_BRASS_LANTERN)))
        {
            /* Decrease life-span */
            int fuel = 1;
            if (fuelable_light_p(o_ptr)
                && (level_partition_kind_for_point(p_ptr->py, p_ptr->px) == LEVEL_PART_CAVEY))
            {
                /*
                 * Small caves: double fuel drain only while standing in the actual
                 * CA-blob cave area (not merely anywhere in the partition).
                 *
                 * CA blobs are generated as (dark) "room" grids; corridors/links are not.
                 */
                if ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_ROOM)) &&
                    !(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)))
                {
                    fuel = 2;
                }
            }

            player_light_add_fuel(o_ptr, -fuel);
            p_ptr->redraw |= (PR_LIGHT);

            /* Hack -- notice interesting fuel steps */
            if ((player_light_fuel(o_ptr) <= player_light_sputter_threshold(o_ptr))
                || (!(player_light_fuel(o_ptr) % 100)))
            {
                /* Window stuff */
                p_ptr->window |= (PW_EQUIP);
            }

            /* Hack -- Special treatment when blind */
            if (p_ptr->blind)
            {
                /* Hack -- save some light for later */
                if (player_light_fuel(o_ptr) == 0)
                    player_light_set_fuel(o_ptr, 1);
            }

            /* The light is now out */
            else if (player_light_fuel(o_ptr) == 0)
            {
                disturb(0, 0);
                msg_print("Your light has gone out!");
            }

            /* The light is getting dim */
            else if ((player_light_fuel(o_ptr)
                    <= player_light_sputter_threshold(o_ptr))
                && (!(player_light_fuel(o_ptr)
                    % MIN(MAX(player_light_sputter_threshold(o_ptr), 1), 20))))
            {
                // disturb the first time
                if (player_light_fuel(o_ptr) == player_light_sputter_threshold(o_ptr))
                    disturb(0, 0);

                msg_print("Your light is growing faint.");
            }
        }
    }

    /*** Process Inventory ***/

    /* Process equipment */
    for (j = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Recharge activatable objects */
        if (o_ptr->timeout > 0 && !fuelable_light_p(o_ptr))
        {
            /* Recharge */
            o_ptr->timeout--;

            /* Notice changes */
            if (!(o_ptr->timeout))
            {
                /* Update window */
                j++;
            }
        }
    }

    /* Notice changes */
    if (j)
    {
        /* Window stuff */
        p_ptr->window |= (PW_EQUIP);
    }

    /* Notice changes */
    if (j)
    {
        /* Combine pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /*** Process Objects ***/

    /* Process objects */
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;
    }
}
