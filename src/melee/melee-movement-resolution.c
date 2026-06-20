#include "angband.h"
#include "externs.h"
#include "melee/melee-attack.h"
#include "melee/melee-movement.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-util.h"

void warning_message(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    char c = r_ptr->d_char;
    char m_name[80];

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    if (strchr("o@Gp", c))
    {
        if (singing(SNG_SILENCE))
        {
            if (m_ptr->ml)
                msg_format("%^s shouts a muffled warning.", m_name);
            else
                msg_print("You hear a muffled warning shout.");
        }
        else
        {
            if (m_ptr->ml)
                msg_format("%^s shouts a warning.", m_name);
            else
                msg_print("You hear a warning shout.");
        }
    }
    else if (strchr("dDsS", c))
    {
        if (singing(SNG_SILENCE))
        {
            if (m_ptr->ml)
                msg_format("%^s lets out a muffled roar.", m_name);
            else
                msg_print("You hear a muffled roar.");
        }
        else
        {
            if (m_ptr->ml)
                msg_format("%^s roars in anger.", m_name);
            else
                msg_print("You hear a loud roar.");
        }
    }
    else if (strchr("T", c))
    {
        if (singing(SNG_SILENCE))
        {
            if (m_ptr->ml)
                msg_format("%^s lets out a muffled grunt.", m_name);
            else
                msg_print("You hear a muffled grunt.");
        }
        else
        {
            if (m_ptr->ml)
                msg_format("%^s grunts in anger.", m_name);
            else
                msg_print("You hear a loud grunt.");
        }
    }
    else if (strchr("C", c))
    {
        if (singing(SNG_SILENCE))
        {
            if (m_ptr->ml)
                msg_format("%^s makes a muffled howl.", m_name);
            else
                msg_print("You hear a muffled howl.");
        }
        else
        {
            if (m_ptr->ml)
                msg_format("%^s makes a low howl.", m_name);
            else
                msg_print("You hear a low howl.");
        }
    }
    else
    {
        if (singing(SNG_SILENCE))
        {
            if (m_ptr->ml)
                msg_format("%^s cries out a muffled warning.", m_name);
            else
                msg_print("You hear something cry out a muffled warning.");
        }
        else
        {
            if (m_ptr->ml)
                msg_format("%^s cries out a warning.", m_name);
            else
                msg_print("You hear something cry out a warning.");
        }
    }

    disturb(1, 0);

    /* Hard not to notice */
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
    monster_perception(false, false, -10);

    // makes monster noise too
    m_ptr->noise += 10;
}

static void pursuit_message(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    char c = r_ptr->d_char;
    char m_name[80];
    int dist = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    if (strchr("o@Gp", c))
    {
        if (m_ptr->ml)
            msg_format("%^s shouts excitedly.", m_name);
        else if (dist < 20)
            msg_print("You hear an shout.");
        else
            msg_print("You hear a distant shout.");
    }
    else if (strchr("dDsS", c))
    {
        if (m_ptr->ml)
            msg_format("%^s roars.", m_name);
        else if (dist < 20)
            msg_print("You hear a loud roar.");
        else
            msg_print("You hear a distant roar.");
    }
    else if (strchr("T", c))
    {
        if (m_ptr->ml)
            msg_format("%^s grunts.", m_name);
        else if (dist < 20)
            msg_print("You hear a low grunt.");
        else
            msg_print("You hear a distant grunt.");
    }
    else if (strchr("C", c))
    {
        if (m_ptr->ml)
            msg_format("%^s makes a low howl.", m_name);
        else if (dist < 20)
            msg_print("You hear a low howl.");
        else
            msg_print("You hear a distant howl.");
    }
    else if (strchr("f", c))
    {
        if (m_ptr->ml)
            msg_format("%^s makes a yowling sound.", m_name);
        else if (dist < 20)
            msg_print("You hear yowling nearby.");
        else
            msg_print("You hear distant yowling.");
    }
    else
    {
        if (m_ptr->ml)
            msg_format("%^s roars in anger.", m_name);
        else if (dist < 20)
            msg_print("You hear a loud roar.");
        else
            msg_print("You hear a distant roar.");
    }
}

/*
 * Deal with the monster Ability: exchange places
 */
void monster_exchange_places(monster_type* m_ptr)
{
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];
    char m_name1[80];
    char m_name2[80];
    char m_name3[80];

    int y = m_ptr->fy;
    int x = m_ptr->fx;

    monster_desc(m_name1, sizeof(m_name1), m_ptr, 0);
    monster_desc(m_name2, sizeof(m_name2), m_ptr, 0x21);
    monster_desc(m_name3, sizeof(m_name3), m_ptr, 0x20);

    /* Message */
    msg_format("%^s exchanges places with you.", m_name1);

    // swap positions with the player
    monster_swap(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);

    // update some things
    update_view();

    // attack of opportunity
    if (player_active_weapon_is_melee()
        && !p_ptr->afraid && !p_ptr->entranced && (p_ptr->stun <= 100))
    {
        if (valorous_oath_auto_attack_safety && chosen_oath(OATH_VALOROUS)
            && !oath_invalid(OATH_VALOROUS) && m_ptr->ml
            && (m_ptr->stance == STANCE_FLEEING))
        {
            msg_format("You refuse to strike %s as it flees past.", m_name1);
        }
        else
        {
            // this might be the most complicated auto-grammatical message in the
            // game...
            msg_format("You attack %s as %s slips past.", m_name2, m_name3);
            py_attack_aux(m_ptr->fy, m_ptr->fx, ATT_OPPORTUNITY);
        }
    }

    // remember that the monster can do this
    if (m_ptr->ml)
        l_ptr->flags2 |= (RF2_EXCHANGE_PLACES);

    /* Set off traps */
    if (cave_trap_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
    {
        // If it is hidden
        if (cave_info[y][x] & (CAVE_HIDDEN))
        {
            /* Reveal the trap */
            reveal_trap(y, x);
        }

        /* Hit the trap */
        hit_trap(y, x);
    }
}

/*
 * Process a monster's move.
 *
 * All the plotting and planning has been done, and all this function
 * has to do is move the monster into the chosen grid.
 *
 * This may involve attacking the character, breaking a glyph of
 * warding, bashing down a door, etc..  Once in the grid, monsters may
 * stumble into monster traps, hit a scent trail, pick up or destroy
 * objects, and so forth.
 *
 * A monster's move may disturb the character, depending on which
 * disturbance options are set.
 */
void process_move(monster_type* m_ptr, int ty, int tx, bool bash)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    /* Existing monster location, proposed new location */
    int oy, ox, ny, nx;

    /* Default move, default lack of view */
    bool do_move = true;
    bool do_view = false;

    /* Assume nothing */
    bool did_swap = false;
    bool did_pass_door = false;
    bool did_open_door = false;
    bool did_unlock_door = false;
    bool did_bash_door = false;
    bool did_take_item = false;
    bool did_kill_item = false;
    bool did_kill_body = false;
    bool did_pass_wall = false;
    bool did_kill_wall = false;
    bool did_tunnel_wall = false;

    /* Remember where monster is */
    oy = m_ptr->fy;
    ox = m_ptr->fx;

    /* Get the destination */
    ny = ty;
    nx = tx;

    /* Check Bounds */
    if (!in_bounds(ny, nx))
        return;

    /*
     * Some monsters will not move into sight of the player.
     */
    if ((r_ptr->flags1 & (RF1_HIDDEN_MOVE))
        && ((cave_info[ty][tx] & (CAVE_SEEN)) || seen_by_keen_senses(ty, tx)))
    {
        /* Hack -- memorize lack of moves after a while. */
        if (!(l_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            if (m_ptr->ml && (one_in_(50)))
            {
                l_ptr->flags1 |= (RF1_HIDDEN_MOVE);
            }
        }
        /* If we are in sight, do not move */
        return;
    }

    /* The grid is occupied by the player. */
    if (cave_m_idx[ny][nx] < 0)
    {
        // unalert monsters notice the player instead of attacking
        if (m_ptr->alertness < ALERTNESS_ALERT)
        {
            set_alertness(
                m_ptr, rand_range(ALERTNESS_ALERT, ALERTNESS_ALERT + 5));

            // Hack: we must unset this flag to avoid the monster missing *2*
            // turns
            m_ptr->skip_next_turn = false;
        }

        // Otherwise attack if possible
        else if (!p_ptr->truce && !(r_ptr->flags1 & (RF1_NEVER_BLOW)))
        {
            if (r_ptr->flags2 & (RF2_EXCHANGE_PLACES) && one_in_(4)
                && (adj_mon_count(m_ptr->fy, m_ptr->fx)
                    >= adj_mon_count(p_ptr->py, p_ptr->px)))
            {
                if (p_ptr->stand_fast)
                {
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                    msg_format("%^s attempts to exchange places with you, but "
                               "you stand fast.",
                        m_name);

                    ident_f3(TR3_STAND_FAST, NULL);
                }
                else
                {
                    monster_exchange_places(m_ptr);
                }
            }
            else
            {
                (void)make_attack_normal(m_ptr);
            }
        }

        /* End move */
        do_move = false;
    }

    /* Can still move */
    if (do_move)
    {
        /* Entering a wall */
        if (cave_info[ny][nx] & (CAVE_WALL))
        {
            /* Monster passes through walls (and doors) */
            if (r_ptr->flags2 & (RF2_PASS_WALL))
            {
                /* Monster went through a wall */
                did_pass_wall = true;
            }

            /* Monster destroys walls (and doors) */
            else if (r_ptr->flags2 & (RF2_KILL_WALL))
            {
                bool msg = false;

                /* Noise distance depends on monster "dangerousness"  XXX */
                int noise_dist = 10;

                /* Forget the wall */
                cave_info[ny][nx] &= ~(CAVE_MARK);

                /* Note that the monster killed the wall */
                if (player_can_see_bold(ny, nx))
                {
                    do_view = true;
                    did_kill_wall = true;
                }

                /* Output warning messages if the racket gets too loud */
                else if (m_ptr->cdis <= noise_dist)
                {
                    msg = true;
                }

                /* Grid is currently a door */
                if (cave_any_closed_door_bold(ny, nx))
                {
                    cave_set_feat(ny, nx, FEAT_BROKEN);

                    if (msg)
                    {
                        // disturb the player
                        disturb(0, 0);
                        msg_print("You hear a door being smashed open.");
                    }

                    // monster noise
                    m_ptr->noise += 10;
                }

                /* Grid is anything else */
                else
                {
                    cave_set_feat(ny, nx, FEAT_FLOOR);

                    if (msg)
                    {
                        // disturb the player
                        disturb(0, 0);
                        msg_print("You hear grinding noises.");
                    }

                    // monster noise
                    m_ptr->noise += 15;
                }
            }

            /* Monster tunnels walls */
            else if ((r_ptr->flags2 & (RF2_TUNNEL_WALL))
                && !cave_any_closed_door_bold(ny, nx))
            {
                bool msg = false;

                /* Noise distance depends on monster "dangerousness"  XXX */
                int noise_dist = 10;

                /* Forget the wall */
                cave_info[ny][nx] &= ~(CAVE_MARK);

                /* Do not move */
                do_move = false;

                /* Note that the monster killed the wall */
                if (player_can_see_bold(ny, nx))
                {
                    do_view = true;
                    did_kill_wall = true;
                }

                /* Output warning messages if the racket gets too loud */
                else if (m_ptr->cdis <= noise_dist)
                {
                    msg = true;
                }

                /* Grid is currently rubble */
                if (cave_feat[ny][nx] == FEAT_RUBBLE)
                {
                    cave_set_feat(ny, nx, FEAT_FLOOR);

                    if (msg)
                    {
                        // disturb the player
                        disturb(0, 0);
                        msg_print("You hear grinding noises.");
                    }

                    // monster noise
                    m_ptr->noise += 15;
                }

                /* Grid is granite or quartz */
                else
                {
                    cave_set_feat(ny, nx, FEAT_RUBBLE);

                    if (msg)
                    {
                        // disturb the player
                        disturb(0, 0);
                        msg_print("You hear grinding noises.");
                    }

                    // monster noise
                    m_ptr->noise += 15;
                }
            }

            /* Doors */
            else if (cave_any_closed_door_bold(ny, nx))
            {
                if (cave_glyph(ny, nx))
                {
                    /* Handle doors in sight */
                    if (player_can_see_bold(ny, nx))
                    {
                        msg_print("Your ward on the door is broken!");
                    }
                }

                /* Monster passes through doors */
                if (r_ptr->flags2 & (RF2_PASS_DOOR))
                {
                    /* Monster went through a door */
                    did_pass_door = true;
                }

                /* Monster bashes the door down */
                else if (bash)
                {
                    /* Handle doors in sight */
                    if (player_can_see_bold(ny, nx))
                    {
                        /* Always disturb */
                        disturb(0, 0);

                        msg_print("The door bursts open!");

                        do_view = true;
                    }
                    /* Character is not too far away */
                    else if (m_ptr->cdis < 20)
                    {
                        // disturb the player
                        disturb(0, 0);

                        /* Message */
                        msg_print("You hear a door burst open!");
                    }

                    /* Note that the monster bashed the door (if visible) */
                    did_bash_door = true;

                    // monster noise
                    m_ptr->noise += 10;

                    /* Break down the door */
                    if (one_in_(2))
                        cave_set_feat(ny, nx, FEAT_BROKEN);
                    else
                        cave_set_feat(ny, nx, FEAT_OPEN);
                }

                /* Monster opens the door */
                else
                {
                    /* Locked doors */
                    if (cave_feat[ny][nx] != FEAT_DOOR_HEAD + 0x00)
                    {
                        /* Note that the monster unlocked the door (if visible)
                         */
                        did_unlock_door = true;

                        /* Unlock the door */
                        cave_set_feat(ny, nx, FEAT_DOOR_HEAD + 0x00);

                        /* Do not move */
                        do_move = false;

                        /* Handle doors in sight */
                        if (player_has_los_bold(ny, nx))
                        {
                            msg_print("You hear a 'click'.");
                        }
                    }

                    /* Ordinary doors */
                    else
                    {
                        /* Note that the monster opened the door (if visible) */
                        did_open_door = true;

                        /* Open the door */
                        cave_set_feat(ny, nx, FEAT_OPEN);

                        /* Step into doorway sometimes */
                        if (!one_in_(5))
                            do_move = false;
                    }

                    /* Handle doors in sight */
                    if (player_can_see_bold(ny, nx))
                    {
                        /* Do not disturb automatically */

                        do_view = true;
                    }
                }
            }

            /* Paranoia -- Ignore all features not added to this code */
            else
                return;
        }

        /* Glyphs */
        else if (cave_feat[ny][nx] == FEAT_GLYPH)
        {
            /* Describe observable breakage */
            if (cave_info[ny][nx] & (CAVE_MARK))
            {
                msg_print("The glyph of warding is broken!");
            }

            /* Forget the rune */
            cave_info[ny][nx] &= ~(CAVE_MARK);

            /* Break the rune */
            cave_set_feat(ny, nx, FEAT_FLOOR);
        }
    }

    /* Monster is allowed to move */
    if (do_move)
    {
        /* The grid is occupied by a monster. */
        if (cave_m_idx[ny][nx] > 0)
        {
            monster_type* n_ptr = &mon_list[cave_m_idx[ny][nx]];
            monster_race* nr_ptr = &r_info[n_ptr->r_idx];

            /* XXX - Kill (much) weaker monsters */
            if ((r_ptr->flags2 & (RF2_KILL_BODY))
                && (!(nr_ptr->flags1 & (RF1_UNIQUE)))
                && (r_ptr->level > nr_ptr->level * 2))
            {
                /* Note that the monster killed another monster (if visible) */
                did_kill_body = true;

                /* Kill the monster */
                delete_monster(ny, nx);
            }
            /* Swap with or push aside the other monster */
            else
            {
                did_swap = true;

                /* If other monster can't move, then abort the swap */

                // Sil-y: should no longer need this

                // if (nr_ptr->flags1 & (RF1_NEVER_MOVE))
                //{
                //	/* Cancel move */
                //	do_move = false;
                //}

                /* The other monster cannot switch places */
                if (!cave_exist_mon(nr_ptr, m_ptr->fy, m_ptr->fx, true, true))
                {
                    /* Try to push it aside */
                    if (!push_aside(m_ptr, n_ptr))
                    {
                        /* Cancel move on failure */
                        do_move = false;
                    }
                }
            }
        }
    }

    /* Monster can (still) move */
    if (do_move)
    {
        // deal with possible flanking attack
        if ((r_ptr->flags2 & (RF2_FLANKING))
            && (distance(oy, ox, p_ptr->py, p_ptr->px) == 1)
            && (distance(ny, nx, p_ptr->py, p_ptr->px) == 1)
            && (m_ptr->alertness >= ALERTNESS_ALERT)
            && (m_ptr->stance != STANCE_FLEEING) && !m_ptr->confused
            && !did_swap)
        {
            char m_name[80];

            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            msg_format("%^s attacks you as it moves by.", m_name);
            make_attack_normal(m_ptr);

            // remember that the monster can do this
            if (m_ptr->ml)
                l_ptr->flags2 |= (RF2_FLANKING);
        }

        /* Move the monster */
        monster_swap(oy, ox, ny, nx);

        /* Cancel target when reached */
        if ((m_ptr->target_y == ny) && (m_ptr->target_x == nx))
        {
            m_ptr->target_y = 0;
            m_ptr->target_x = 0;
        }

        /*Did a new monster get pushed into the old space?*/
        if (cave_m_idx[oy][ox] > 0)
        {
            monster_type* n_ptr;
            monster_type monster_type_body;

            /* Get local monster */
            n_ptr = &monster_type_body;

            n_ptr = &mon_list[cave_m_idx[oy][ox]];

            n_ptr->mflag |= (MFLAG_PUSHED);

            // exchanging places doesn't count as movement in a direction for
            // abilities
        }

        // if it moved into a free space
        else
        {
            // record the direction of travel (for charge attacks)
            m_ptr->previous_action[0] = rough_direction(oy, ox, ny, nx);
        }

        /*
         * If a member of a monster group capable of smelling hits a
         * scent trail while out of LOS of the character, it will
         * communicate this to similar monsters.
         */
        if ((!player_has_los_bold(ny, nx)) && (r_ptr->flags1 & (RF1_FRIENDS))
            && (monster_can_smell(m_ptr)) && (get_scent(oy, ox) == -1)
            && (!m_ptr->target_y) && (!m_ptr->target_x))
        {
            int i;
            monster_type* n_ptr;
            monster_race* nr_ptr;
            bool alerted_others = false;

            /* Scan all other monsters */
            for (i = mon_max - 1; i >= 1; i--)
            {
                /* Access the monster */
                n_ptr = &mon_list[i];
                nr_ptr = &r_info[n_ptr->r_idx];

                /* Ignore dead monsters */
                if (!n_ptr->r_idx)
                    continue;

                /* Ignore monsters with the wrong symbol */
                if (r_ptr->d_char != nr_ptr->d_char)
                    continue;

                /* Ignore monsters with specific orders */
                if ((n_ptr->target_x) || (n_ptr->target_y))
                    continue;

                /* Ignore monsters picking up a good scent */
                if (get_scent(n_ptr->fy, n_ptr->fx) < SMELL_STRENGTH - 10)
                    continue;

                /* Ignore monsters not in LOS */
                if (!los(m_ptr->fy, m_ptr->fx, n_ptr->fy, n_ptr->fx))
                    continue;

                /* Activate all other monsters and give directions */
                make_alert(m_ptr);
                n_ptr->mflag |= (MFLAG_ACTV);
                n_ptr->target_y = ny;
                n_ptr->target_x = nx;

                alerted_others = true;
            }

            if (alerted_others)
            {
                pursuit_message(m_ptr);
            }
        }

        /* Monster is visible and not cloaked */
        if (m_ptr->ml)
        {
            // report passing through doors
            if (cave_any_closed_door_bold(m_ptr->fy, m_ptr->fx))
            {
                /* Monster passes through doors */
                if (r_ptr->flags2 & (RF2_PASS_DOOR))
                {
                    char m_name[80];

                    /* Always disturb */
                    disturb(0, 0);

                    /* Get the monster name */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

                    /* Dump a message */
                    msg_format("%^s passes under the door.", m_name);
                }
            }

            /* Player will always be disturbed if monster moves adjacent */
            if (m_ptr->cdis == 1)
            {
                disturb(0, 0);
            }

            /* be disturbed by all other monster movement */
            else
            {
                disturb(0, 0);
            }
        }

        /* Take or kill objects on the floor */
        if ((r_ptr->flags2 & (RF2_TAKE_ITEM))
            || (r_ptr->flags2 & (RF2_KILL_ITEM)))
        {
            u32b f1, f2, f3;

            char m_name[80];
            char o_name[120];

            s16b this_o_idx, next_o_idx = 0;

            /* Scan all objects in the grid */
            for (this_o_idx = cave_o_idx[ny][nx]; this_o_idx;
                 this_o_idx = next_o_idx)
            {
                object_type* o_ptr;

                /* Get the object */
                o_ptr = &o_list[this_o_idx];

                /* Get the next object */
                next_o_idx = o_ptr->next_o_idx;

                /* Extract some flags */
                object_flags(o_ptr, &f1, &f2, &f3);

                /* Don't pick up cursed items */
                if ((r_ptr->flags2 & (RF2_TAKE_ITEM))
                    && (cursed_p(o_ptr) || broken_p(o_ptr)))
                {
                    /* Describe observable situations */
                    if (m_ptr->ml && player_has_los_bold(ny, nx))
                    {
                        /* Get the object name */
                        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                        /* Get the monster name */
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

                        /* Dump a message */
                        msg_format(
                            "%^s looks at %s, but moves on.", m_name, o_name);
                    }
                }

                /* The object cannot be picked up by the monster */
                // else if (artefact_p(o_ptr) || (f3 & flg3))
                //{
                //	/* Only give a message for "take_item" */
                //	if (r_ptr->flags2 & (RF2_TAKE_ITEM))
                //	{
                //		/* Take note */
                //		did_take_item = true;

                //		/* Describe observable situations */
                //		if (m_ptr->ml && player_has_los_bold(ny, nx))
                //		{
                //			/* Get the object name */
                //			object_desc(o_name, sizeof(o_name),
                // o_ptr, true, 3);

                //			/* Get the monster name */
                //			monster_desc(m_name, sizeof(m_name),
                // m_ptr, 0x04);

                //			/* Dump a message */
                //			msg_format("%^s tries to pick up %s, but
                // fails.", 				   m_name, o_name);
                //		}
                //	}
                //}

                /* Pick up the item */
                else if (r_ptr->flags2 & (RF2_TAKE_ITEM))
                {
                    object_type* i_ptr;
                    object_type object_type_body;

                    /* Take note */
                    did_take_item = true;

                    /* Describe observable situations */
                    if (player_has_los_bold(ny, nx) && m_ptr->ml)
                    {
                        /* Get the object name */
                        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                        /* Get the monster name */
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

                        /* Dump a message */
                        msg_format("%^s picks up %s.", m_name, o_name);
                    }

                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Obtain local object */
                    object_copy(i_ptr, o_ptr);

                    /* Delete the object */
                    delete_object_idx(this_o_idx);

                    /* Carry the object */
                    (void)monster_carry(
                        cave_m_idx[m_ptr->fy][m_ptr->fx], i_ptr);
                }

                /* Destroy the item */
                else
                {
                    /* Take note */
                    did_kill_item = true;

                    /* Describe observable situations */
                    if (player_has_los_bold(ny, nx))
                    {
                        /* Get the object name */
                        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                        /* Get the monster name */
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

                        /* Dump a message */
                        msg_format("%^s crushes %s.", m_name, o_name);
                    }

                    /* Delete the object */
                    delete_object_idx(this_o_idx);
                }
            }
        }
    } /* End of monster's move */

    /* Notice changes in view */
    if (do_view)
    {
        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
    }

    /* Learn things from observable monster */
    if (m_ptr->ml)
    {
        /* Monster passed through a door */
        if (did_pass_door)
            l_ptr->flags2 |= (RF2_PASS_DOOR);

        /* Monster opened a door */
        if (did_open_door)
            l_ptr->flags2 |= (RF2_OPEN_DOOR);

        /* Monster unlocked a door */
        if (did_unlock_door)
            l_ptr->flags2 |= (RF2_UNLOCK_DOOR);

        /* Monster bashed a door */
        if (did_bash_door)
            l_ptr->flags2 |= (RF2_BASH_DOOR);

        /* Monster tried to pick something up */
        if (did_take_item)
            l_ptr->flags2 |= (RF2_TAKE_ITEM);

        /* Monster tried to crush something */
        if (did_kill_item)
            l_ptr->flags2 |= (RF2_KILL_ITEM);

        /* Monster ate another monster */
        if (did_kill_body)
            l_ptr->flags2 |= (RF2_KILL_BODY);

        /* Monster passed through a wall */
        if (did_pass_wall)
            l_ptr->flags2 |= (RF2_PASS_WALL);

        /* Monster killed a wall */
        if (did_kill_wall)
            l_ptr->flags2 |= (RF2_KILL_WALL);

        /* Monster tunneled through a wall */
        if (did_tunnel_wall)
            l_ptr->flags2 |= (RF2_TUNNEL_WALL);
    }
}
