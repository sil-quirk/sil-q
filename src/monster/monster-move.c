/* File: monster-move.c */

#include "monster-internal.h"

/*
 * Make a monster carry an object
 */
s16b monster_carry(int m_idx, object_type* j_ptr)
{
    s16b o_idx;

    s16b this_o_idx, next_o_idx = 0;

    monster_type* m_ptr = &mon_list[m_idx];

    /* Scan objects already being held for combination */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Check for combination */
        if (object_similar(o_ptr, j_ptr))
        {
            /* Combine the items */
            object_absorb(o_ptr, j_ptr);

            if (j_ptr->number == 0)
            {
                /* Result */
                return (this_o_idx);
            }
        }
    }

    /* Make an object */
    o_idx = o_pop();

    /* Success */
    if (o_idx)
    {
        object_type* o_ptr;

        /* Get new object */
        o_ptr = &o_list[o_idx];

        /* Copy object */
        object_copy(o_ptr, j_ptr);

        /* Forget mark */
        o_ptr->marked = false;

        /* Forget location */
        o_ptr->iy = o_ptr->ix = 0;

        /* Link the object to the monster */
        o_ptr->held_m_idx = m_idx;

        /* Link the object to the pile */
        o_ptr->next_o_idx = m_ptr->hold_o_idx;

        /* Link the monster to the object */
        m_ptr->hold_o_idx = o_idx;
    }

    /* Result */
    return (o_idx);
}

/*
 * Check if the monster in the given location needs to fall down a chasm
 */
void m_fall_in_chasm(int fy, int fx)
{
    monster_type* m_ptr;
    monster_race* r_ptr;
    char m_name[80];

    int dice;
    int dam;

    // paranoia
    if (cave_m_idx[fy][fx] <= 0)
        return;

    m_ptr = &mon_list[cave_m_idx[fy][fx]];
    r_ptr = &r_info[m_ptr->r_idx];

    if ((cave_feat[fy][fx] == FEAT_CHASM) && !(r_ptr->flags2 & (RF2_FLYING)))
    {
        // Get the monster name
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        // message for visible monsters
        if (m_ptr->ml)
        {
            // Dump a message
            if (m_ptr->morale < -200)
                msg_format("%^s leaps into the abyss!", m_name);
            else
                msg_format("%^s topples into the abyss!", m_name);
        }

        // pause so that the monster will be displayed in the chasm before it
        // disappears
        message_flush();

        // determine the falling damage
        if (p_ptr->depth >= MORGOTH_DEPTH - 1)
            dice = 3; // only fall one floor in this case
        else
            dice = 6;

        // roll the damage dice
        dam = damroll(dice, 4);

        // update combat rolls if visible
        if (m_ptr->ml)
        {
            // Store information for the combat rolls window
            combat_roll_special_char = (&f_info[cave_feat[fy][fx]])->d_char;
            combat_roll_special_attr = (&f_info[cave_feat[fy][fx]])->d_attr;

            update_combat_rolls1b(NULL, m_ptr, true);
            update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, false);
        }

        // kill monsters which cannot survive the damage
        if (m_ptr->hp <= dam)
        {
            // kill the monster, gain experience etc
            monster_death(cave_m_idx[fy][fx]);

            // delete the monster
            delete_monster_idx(cave_m_idx[fy][fx]);
        }

        // otherwise the monster survives! (mainly relevant for uniques)
        else
        {
            // just delete the monster
            delete_monster(m_ptr->fy, m_ptr->fx);
        }
    }
}

/*
 * Apply a rewired trap's effect to the monster standing on (fy, fx).
 *
 * Mirrors the offensive subset of hit_trap(), but the victim is a monster.
 * Damage is dealt through mon_take_hit() with who == -1 so the player is
 * credited with the kill; status traps reuse the same machinery the player
 * versions use.  Only the traps listed in trap_is_rewireable() reach here.
 */
static void mon_trigger_rewired_trap(int m_idx, int fy, int fx)
{
    monster_type* m_ptr = &mon_list[m_idx];
    int feat = cave_feat[fy][fx];
    char m_name[80];
    int dd = 0, ds = 0, dam;
    bool died;
    bool to_rubble = false;

    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Store the trap glyph for the combat-rolls window */
    combat_roll_special_char = (&f_info[feat])->d_char;
    combat_roll_special_attr = (&f_info[feat])->d_attr;

    switch (feat)
    {
    case FEAT_TRAP_DART:
        if (m_ptr->ml)
            msg_format("A dart shoots out and strikes %s.", m_name);
        dd = 1;
        ds = 15;
        break;

    case FEAT_TRAP_CALTROPS:
        if (m_ptr->ml)
            msg_format("%^s blunders into a field of caltrops.", m_name);
        dd = 1;
        ds = 4;
        break;

    case FEAT_TRAP_ACID:
        if (m_ptr->ml)
            msg_format("%^s is splashed with acid.", m_name);
        dd = 4;
        ds = 4;
        break;

    case FEAT_TRAP_DEADFALL:
        if (m_ptr->ml)
            msg_format("The ceiling collapses on %s!", m_name);
        dd = 6;
        ds = 8;
        to_rubble = true;
        break;

    case FEAT_TRAP_GAS_CONF:
        if (m_ptr->ml)
            msg_format("Confusing vapours billow around %s.", m_name);
        /* Reuse the same radius effect the player gas trap uses; it confuses
         * the stepping monster (and any neighbours) and resolves its own
         * death/messages, so there is nothing more to do here. */
        explosion(-1, 1, fy, fx, 3, 4, 10, GF_CONFUSION);
        return;

    default:
        return;
    }

    dam = damroll(dd, ds);

    if (m_ptr->ml)
    {
        update_combat_rolls1b(NULL, m_ptr, true);
        update_combat_rolls2(dd, ds, dam, -1, -1, 0, 0, GF_HURT, false);
    }

    /* Hurt the monster (who == -1 so the player gains the experience) */
    died = mon_take_hit(m_idx, dam, NULL, -1);

    /* Caltrops also slow a survivor */
    if (!died && (feat == FEAT_TRAP_CALTROPS))
        set_monster_slow(m_idx, m_ptr->slowed + damroll(4, 4), m_ptr->ml);

    /* A deadfall buries its grid in rubble, consuming the trap */
    if (to_rubble)
    {
        cave_info[fy][fx] &= ~(CAVE_MARK);
        cave_rewired[fy][fx] = 0;
        cave_set_feat(fy, fx, FEAT_RUBBLE);
    }
}

/*
 * A monster has stepped onto (fy, fx).  If the player has rewired a trap there,
 * the monster engages it.
 *
 * The creature still knows a trap sits here -- but rewiring silently changed
 * how the mechanism works.  A mindless creature blunders straight in, trusting
 * habit.  An intelligent one may notice the device has been tampered with (a
 * Perception roll vs the rewire quality); if it does, it stops to disarm the
 * altered trap, either neutralising it or fumbling and setting it off on
 * itself.  The rolls are shown to the player exactly like their own disarming
 * whenever the monster is in view.
 */
void m_engage_rewired_trap(int fy, int fx)
{
    int m_idx = cave_m_idx[fy][fx];
    monster_type* m_ptr;
    monster_race* r_ptr;
    int difficulty, detect, disarm;
    char m_name[80];
    bool seen;

    /* Only monsters, and only a live, rewired trap */
    if (m_idx <= 0)
        return;
    if (!cave_rewired[fy][fx] || !cave_trap_bold(fy, fx))
        return;

    m_ptr = &mon_list[m_idx];
    r_ptr = &r_info[m_ptr->r_idx];
    seen = m_ptr->ml;
    difficulty = cave_rewired[fy][fx];

    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Mindless creatures never suspect a thing */
    if (r_ptr->flags2 & (RF2_MINDLESS))
    {
        mon_trigger_rewired_trap(m_idx, fy, fx);
        return;
    }

    /* Does it notice the mechanism has been altered this time? */
    if (seen)
        detect = show_interaction_skill_roll_animation_actor(m_ptr,
            "Noticing tampering", "Studying the trap", fy, fx,
            monster_skill(m_ptr, S_PER), difficulty, NULL);
    else
        detect
            = skill_check(m_ptr, monster_skill(m_ptr, S_PER), difficulty, NULL);

    /* Fails to notice -- trusts its stale familiarity and walks right in */
    if (detect <= 0)
    {
        mon_trigger_rewired_trap(m_idx, fy, fx);
        return;
    }

    /* It noticed -- it stops to disarm the altered trap */
    if (seen)
        msg_format("%^s notices the trap has been tampered with.", m_name);

    if (seen)
        disarm = show_interaction_skill_roll_animation_actor(m_ptr,
            "Disarming trap", "Testing the mechanism", fy, fx,
            monster_skill(m_ptr, S_PER), difficulty, NULL);
    else
        disarm
            = skill_check(m_ptr, monster_skill(m_ptr, S_PER), difficulty, NULL);

    if (disarm > 0)
    {
        /* Neutralises your handiwork */
        if (seen)
            msg_format("%^s dismantles the trap.", m_name);

        cave_info[fy][fx] &= ~(CAVE_MARK);
        cave_rewired[fy][fx] = 0;
        cave_set_feat(fy, fx, FEAT_FLOOR);
    }
    else
    {
        /* Fumbles and sets it off on itself */
        mon_trigger_rewired_trap(m_idx, fy, fx);
    }
}

/*
 * Print a message saying what is underfoot.
 */
void describe_floor_object(void)
{
    object_type* o_ptr;
    char o_name[80];
    char smith_buf[20];
    bool disturb_for_underfoot_notice = !p_ptr->resting;

    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

    // skip 'nothings'
    if (!o_ptr->k_idx)
    {
        return;
    }

    // skip notes
    else if (o_ptr->tval == TV_NOTE)
    {
        return;
    }

    // skip fired/thrown items
    else if (o_ptr->pickup)
    {
        return;
    }

    if (p_ptr->image)
    {
        if (!p_ptr->blind)
        {
            msg_print("Your vision is too distorted to tell what lies here.");
            window_stuff();
            Term_fresh();
        }

        if (disturb_for_underfoot_notice)
            disturb(0, 0);

        return;
    }

    // generate the object's name
    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

    smith_buf[0] = '\0';
    if (op_ptr->opt[OPT_show_smithing_difficulty_look]
        && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);
        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
    }

    // arms and armour show weight
    if (((wield_slot(o_ptr) >= INVEN_WIELD)
                 && (wield_slot(o_ptr) <= INVEN_STAFF))
        || (wield_slot(o_ptr) == INVEN_HORN)
        || ((wield_slot(o_ptr) >= INVEN_BODY)
            && (wield_slot(o_ptr) <= INVEN_FEET)))
    {
        int wgt = o_ptr->weight * o_ptr->number;
        if (!p_ptr->blind)
            msg_format("You see %s %d.%1d lb%s.", o_name, wgt / 10, wgt % 10,
                smith_buf);
        else
            msg_format("Your feet strike against %s.", o_name);

        /* Underfoot reminders should not cancel an active rest. */
        if (disturb_for_underfoot_notice)
            disturb(0, 0);
    }

    // other things just show description
    else
    {
        if (!p_ptr->blind)
            msg_format("You see %s%s.", o_name, smith_buf);
        else
            msg_format("Your feet strike against %s.", o_name);

        /* Underfoot reminders should not cancel an active rest. */
        if (disturb_for_underfoot_notice)
            disturb(0, 0);
    }

    // special explanation the first time you step over the crown
    if ((o_ptr->name1 == ART_MORGOTH_3) && !(p_ptr->crown_hint))
    {
        if (hjkl_movement)
        {
            msg_print("To attempt to prise a Silmaril from the crown, use the "
                      "'destroy' "
                      "command ('Ctrl-k').");
        }
        else
        {
            msg_print("To attempt to prise a Silmaril from the crown, use the "
                      "'destroy' "
                      "command (which is 'k' by default).");
        }
        p_ptr->crown_hint = true;
    }
}

/*
 * Swap the players/monsters (if any) at two locations XXX XXX XXX
 *
 * Note that this assumes the monster at y1-x1 is actively moving to y2-x2
 */
static bool player_environment_bonus_state_changed(int old_y, int old_x,
    int new_y, int new_x)
{
    return level_partition_big_cave_type_for_point(old_y, old_x)
        != level_partition_big_cave_type_for_point(new_y, new_x);
}

void monster_swap(int y1, int x1, int y2, int x2)
{
    int m1 = cave_m_idx[y1][x1];
    int m2 = cave_m_idx[y2][x2];

    int y, x;

    monster_type* m_ptr = NULL; // default to soother compiler warnings
    monster_race* r_ptr = NULL; // default to soother compiler warnings
    monster_lore* l_ptr = NULL; // default to soother compiler warnings

    char m_name[80];

    bool monster1 = false;

    /* Monster 1 */
    if (m1 > 0)
    {
        monster1 = true;
        m_ptr = &mon_list[m1];

        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        // (skip_next_turn is there to stop you getting opportunist attacks afer
        // knocking someone back)
        if (player_active_weapon_is_melee()
            && !singing(SNG_DISGUISE) && m_ptr->ml && !m_ptr->skip_next_turn
            && !p_ptr->truce
            && !p_ptr->confused && !p_ptr->afraid && !p_ptr->entranced
            && (p_ptr->stun <= 100))
        {
            if (!forgo_attacking_unwary
                || (m_ptr->alertness >= ALERTNESS_ALERT))
            {
                bool moved_last_turn
                    = ((p_ptr->previous_action[1] >= 1)
                          && (p_ptr->previous_action[1] <= 9)
                          && (p_ptr->previous_action[1] != 5))
                    || (p_ptr->previous_action[1] == ACTION_BASH);

                if (p_ptr->active_ability[S_MEL][MEL_ZONE_OF_CONTROL]
                    && !moved_last_turn)
                {
                    if ((distance(y1, x1, p_ptr->py, p_ptr->px) == 1)
                        && (distance(y2, x2, p_ptr->py, p_ptr->px) == 1))
                    {
                        if (valorous_oath_auto_attack_safety
                            && chosen_oath(OATH_VALOROUS)
                            && !oath_invalid(OATH_VALOROUS) && m_ptr->ml
                            && (m_ptr->stance == STANCE_FLEEING))
                        {
                            msg_format("%^s moves through your zone of control, but you hold back.", m_name);
                        }
                        else
                        {
                            msg_format("%^s moves through your zone of control.", m_name);
                            py_attack_aux(y1, x1, ATT_ZONE_OF_CONTROL);
                        }
                    }
                }
                if (p_ptr->active_ability[S_STL][STL_OPPORTUNIST])
                {
                    if ((distance(y1, x1, p_ptr->py, p_ptr->px) == 1)
                        && (distance(y2, x2, p_ptr->py, p_ptr->px) > 1))
                    {
                        if (valorous_oath_auto_attack_safety
                            && chosen_oath(OATH_VALOROUS)
                            && !oath_invalid(OATH_VALOROUS) && m_ptr->ml
                            && (m_ptr->stance == STANCE_FLEEING))
                        {
                            msg_format("%^s moves away from you, but you hold back.", m_name);
                        }
                        else
                        {
                            msg_format("%^s moves away from you.", m_name);
                            py_attack_aux(y1, x1, ATT_OPPORTUNIST);
                        }
                    }
                }
            }
        }
        if (m_ptr->hp <= 0)
            return;

        // abort the monster swap if the monster has been moved by the free
        // attack
        if (cave_m_idx[y1][x1] != m1)
            return;

        /* Move monster */
        m_ptr->fy = y2;
        m_ptr->fx = x2;
        m_ptr->visual_facing_dir = (byte)rough_direction(y1, x1, y2, x2);

        if ((r_info[m_ptr->r_idx].flags3 & RF3_SPECIAL_VAULT_ONLY)
            && (((cave_info[y1][x1] & CAVE_G_VAULT) == 0)
                || ((cave_info[y2][x2] & CAVE_G_VAULT) == 0)))
        {
            log_trace(
                "SPECIAL_VAULT_ONLY move: monster='%s' r_idx=%d m_idx=%d depth=%d from=(%d,%d) to=(%d,%d) from_g_vault=%d to_g_vault=%d from_icky=%d to_icky=%d from_morgoth_tunnel=%d to_morgoth_tunnel=%d",
                m_name, m_ptr->r_idx, m1, p_ptr->depth, y1, x1, y2, x2,
                (cave_info[y1][x1] & CAVE_G_VAULT) ? 1 : 0,
                (cave_info[y2][x2] & CAVE_G_VAULT) ? 1 : 0,
                (cave_info[y1][x1] & CAVE_ICKY) ? 1 : 0,
                (cave_info[y2][x2] & CAVE_ICKY) ? 1 : 0,
                (cave_info[y1][x1] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
                (cave_info[y2][x2] & CAVE_MORGOTH_TUNNEL) ? 1 : 0);
        }

        // makes noise when moving
        if (m_ptr->noise == 0)
            m_ptr->noise = 5;

        /* Update monster */
        (void)update_mon(m1, true);
    }

    /* Player 1 */
    else if (m1 < 0)
    {
        bool bonus_state_changed =
            player_environment_bonus_state_changed(y1, x1, y2, x2);
        bool should_log_environment = bonus_state_changed
            || level_partition_big_cave_type_for_point(y1, x1) != BIG_CAVE_NONE
            || level_partition_big_cave_type_for_point(y2, x2) != BIG_CAVE_NONE
            || ((cave_info[y1][x1] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0)
            || ((cave_info[y2][x2] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0);

        if (should_log_environment)
            log_partition_debug_for_point("monster_swap.old", y1, x1);

        // deal with monsters with Opportunist or Zone of Control
        for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
        {
            for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
            {
                if (cave_m_idx[y][x] > 0)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    r_ptr = &r_info[m_ptr->r_idx];
                    l_ptr = &l_list[m_ptr->r_idx];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                    if (!singing(SNG_DISGUISE)
                        && (m_ptr->alertness >= ALERTNESS_ALERT)
                        && !m_ptr->confused && (m_ptr->stance != STANCE_FLEEING)
                        && !m_ptr->skip_next_turn && !m_ptr->skip_this_turn)
                    {
                        // Opportunist
                        if ((r_ptr->flags2 & (RF2_OPPORTUNIST))
                            && (distance(m_ptr->fy, m_ptr->fx, y2, x2) > 1))
                        {
                            msg_format(
                                "%^s attacks you as you step away.", m_name);
                            make_attack_normal(m_ptr);

                            // remember that the monster can do this
                            if (m_ptr->ml)
                                l_ptr->flags2 |= (RF2_OPPORTUNIST);
                        }

                        // Zone of Control
                        if ((r_ptr->flags2 & (RF2_ZONE_OF_CONTROL))
                            && (distance(m_ptr->fy, m_ptr->fx, y2, x2) == 1))
                        {
                            msg_format("You move through %s's zone of control.",
                                m_name);
                            make_attack_normal(m_ptr);

                            // remember that the monster can do this
                            if (m_ptr->ml)
                                l_ptr->flags2 |= (RF2_ZONE_OF_CONTROL);
                        }
                    }
                }
            }
        }
        if (p_ptr->chp <= 0)
            return;

        /* Move player */
        p_ptr->py = y2;
        p_ptr->px = x2;

        /* Update the panel */
        p_ptr->update |= (PU_PANEL);

        /* Update the visuals (and monster distances) */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        if (should_log_environment)
            log_partition_debug_for_point("monster_swap.new", y2, x2);

        if (bonus_state_changed)
        {
            p_ptr->update |= (PU_BONUS);
            update_stuff();
        }
    }

    /* Monster 2 */
    if (m2 > 0)
    {
        m_ptr = &mon_list[m2];

        /* Move monster */
        m_ptr->fy = y1;
        m_ptr->fx = x1;
        m_ptr->visual_facing_dir = (byte)rough_direction(y2, x2, y1, x1);

        // makes noise when moving
        if (m_ptr->noise == 0)
            m_ptr->noise = 5;

        /* Update monster */
        (void)update_mon(m2, true);

        // reset its previous movement to stop it charging etc.
        m_ptr->previous_action[0] = ACTION_MISC;
    }

    /* Player 2 */
    else if (m2 < 0)
    {
        /* Move player */
        p_ptr->py = y1;
        p_ptr->px = x1;

        /* Update the panel */
        p_ptr->update |= (PU_PANEL);

        /* Update the visuals (and monster distances) */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);
    }

    /* Update grids */
    cave_m_idx[y1][x1] = m2;
    cave_m_idx[y2][x2] = m1;

    /* Redraw */
    lite_spot(y1, x1);
    lite_spot(y2, x2);

    // deal with set polearm attacks
    if (player_active_weapon_is_melee()
        && p_ptr->active_ability[S_MEL][MEL_POLEARMS] && monster1 && m_ptr->ml)
    {
        object_type* o_ptr = &inventory[INVEN_WIELD];
        u32b f1, f2, f3;

        object_flags(o_ptr, &f1, &f2, &f3);

        if (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT))
        {
            if ((distance(y1, x1, p_ptr->py, p_ptr->px) > 1)
                && (distance(y2, x2, p_ptr->py, p_ptr->px) == 1)
                && !p_ptr->truce && !p_ptr->confused && !p_ptr->afraid
                && (f3 & (TR3_POLEARM)) && p_ptr->focused)
            {
                char o_name[80];

                /* Get the basic name of the object */
                object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

                if (valorous_oath_auto_attack_safety && chosen_oath(OATH_VALOROUS)
                    && !oath_invalid(OATH_VALOROUS)
                    && (m_ptr->stance == STANCE_FLEEING))
                {
                    msg_format("%^s comes into reach of your %s, but you hold back.", m_name, o_name);
                }
                else
                {
                    msg_format("%^s comes into reach of your %s.", m_name, o_name);
                    py_attack_aux(y2, x2, ATT_POLEARM);
                }
            }
        }
    }

    // deal with falling down chasms
    if (m1 > 0)
        m_fall_in_chasm(y2, x2);
    if (m2 > 0)
        m_fall_in_chasm(y1, x1);

    // a monster that moved onto a rewired trap may set it off
    if (m1 > 0)
        m_engage_rewired_trap(y2, x2);
    if (m2 > 0)
        m_engage_rewired_trap(y1, x1);

    // describe object you are standing on if any
    if ((m1 < 0) || (m2 < 0))
    {
        describe_floor_object();
    }
}

/*
 * Place the player in the dungeon XXX XXX
 */
s16b player_place(int y, int x)
{
    /* Paranoia XXX XXX */
    if (cave_m_idx[y][x] != 0)
        return (0);

    /* Save player location */
    p_ptr->py = y;
    p_ptr->px = x;

    /* Mark cave grid */
    cave_m_idx[y][x] = -1;
    if (cave_feat[y][x] == FEAT_RUBBLE)
        cave_feat[y][x] = FEAT_FLOOR;

    /* Success */
    return (-1);
}

/*
 * Place a copy of a monster in the dungeon XXX XXX
 */
static byte monster_initial_random_tile_facing(int m_idx, int r_idx)
{
    u32b hash = (u32b)m_idx;

    /*
     * Keep cosmetic facing independent of the gameplay RNG stream and the
     * monster's position, so it remains unchanged after movement or reload.
     */
    hash ^= (u32b)r_idx * 0x9e3779b9U;
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;

    return (hash & 1U) ? MONSTER_TILE_FACING_LEFT
                       : MONSTER_TILE_FACING_RIGHT;
}

s16b monster_place(int y, int x, monster_type* n_ptr)
{
    s16b m_idx;

    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Paranoia XXX XXX */
    if (cave_m_idx[y][x] != 0)
        return (0);

    /* Get a new record */
    m_idx = mon_pop();

    /* Oops */
    if (m_idx)
    {
        /* Make a new monster */
        cave_m_idx[y][x] = m_idx;

        /* Get the new monster */
        m_ptr = &mon_list[m_idx];

        /* Copy the monster XXX */
        memcpy(m_ptr, n_ptr, sizeof(monster_type));

        /* Location */
        m_ptr->fy = y;
        m_ptr->fx = x;

        /* Set once at placement so random-facing races never flicker. */
        m_ptr->visual_random_facing = monster_initial_random_tile_facing(
            m_idx, m_ptr->r_idx);

        /* Update the monster */
        update_mon(m_idx, true);

        /* Get the new race */
        r_ptr = &r_info[m_ptr->r_idx];

        /* Hack -- Notice new multi-hued monsters */
        if (r_ptr->flags1 & (RF1_ATTR_MULTI))
            shimmer_monsters = true;

        /* Hack -- Count the number of "reproducers" */
        if (r_ptr->flags2 & (RF2_MULTIPLY))
            num_repro++;

        /* Count racial occurances */
        r_ptr->cur_num++;
    }

    /* Result */
    return (m_idx);
}

/*calculate the monster_speed of a monster at a given location*/
void calc_monster_speed(int y, int x)
{
    int speed;

    /*point to the monster at the given location & the monster race*/
    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Paranoia XXX XXX */
    if (cave_m_idx[y][x] <= 0)
        return;

    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];

    /* Get the monster base speed */
    speed = r_ptr->speed;

    /*factor in the hasting and slowing counters*/
    if (m_ptr->hasted)
        speed += 1;
    if (m_ptr->slowed)
        speed -= 1;

    if (speed < 1)
        speed = 1;

    /*set the speed and return*/
    m_ptr->mspeed = speed;

    return;
}

