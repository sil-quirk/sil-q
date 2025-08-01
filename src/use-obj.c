/* File: use-obj.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

int medicine_bonus(int original)
{
    int bonus = 0;
    object_type* o_ptr;

    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        u32b t1, t2, t3;
        o_ptr = &inventory[i];
        if (!o_ptr->k_idx)
            continue;

        object_flags(o_ptr, &t1, &t2, &t3);
        if (t3 & (TR3_MEDIC))
            bonus++;
    } 

    return (original / 3) * bonus;
}

static bool eat_food(object_type* o_ptr, bool* ident)
{
    // Easter Eggs
    if ((o_ptr->sval < SV_FOOD_MIN_FOOD) && easter_time())
    {
        msg_print("You unwrap the foil and eat the chocolate egg.");
        msg_print("It is delicious!");
    }

    /* Analyze the food */
    switch (o_ptr->sval)
    {
    case SV_FOOD_RAGE:
    {
        if (set_afraid(0))
            *ident = true;
        if (set_rage(p_ptr->rage + damroll(10, 4)))
            *ident = true;
        break;
    }

    case SV_FOOD_SUSTENANCE:
    {
        if (set_food(p_ptr->food + 2000))
            *ident = true;
        break;
    }

    case SV_FOOD_TERROR:
    {
        if (allow_player_fear(NULL))
        {
            if (set_afraid(p_ptr->afraid + damroll(10, 4)))
            {
                *ident = true;
            }
            if (set_fast(p_ptr->fast + damroll(5, 4)))
            {
                *ident = true;
            }
        }
        else
        {
            msg_print("You feel nervous for a moment.");
            *ident = true;
        }

        break;
    }

    case SV_FOOD_HEALING:
    {
        msg_print("It has the bitter taste of medicine.");
        *ident = true;
        set_cut(p_ptr->cut / 2);
        hp_player(50 + medicine_bonus(50), true, true);
        break;
    }

    case SV_FOOD_RESTORATION:
    {
        if (do_res_stat(A_STR, 3))
            *ident = true;
        if (do_res_stat(A_DEX, 3))
            *ident = true;
        if (do_res_stat(A_CON, 3))
            *ident = true;
        if (do_res_stat(A_GRA, 3))
            *ident = true;
        break;
    }

    case SV_FOOD_HUNGER:
    {
        if (set_food(p_ptr->food - 1000))
            *ident = true;
        break;
    }

    case SV_FOOD_VISIONS:
    {
        if (set_blind(0))
            *ident = true;
        if (allow_player_image(NULL))
        {
            if (set_image(p_ptr->image + damroll(80, 4)))
            {
                *ident = true;
            }
        }
        else
        {
            msg_print("You thought you saw something for a moment.");
            *ident = true;
        }
        break;
    }

    case SV_FOOD_ENTRANCEMENT:
    {
        if (allow_player_entrancement(NULL))
        {
            if (set_entranced(damroll(10, 4)))
            {
                *ident = true;
            }
        }
        else
        {
            msg_print("You feel strangely peaceful for a moment.");
            *ident = true;
        }
        break;
    }

    case SV_FOOD_WEAKNESS:
    {
        if (do_dec_stat(A_STR, NULL))
            *ident = true;
        break;
    }

    case SV_FOOD_SICKNESS:
    {
        if (do_dec_stat(A_CON, NULL))
            *ident = true;
        break;
    }

    case SV_FOOD_BREAD:
    {
        msg_print("It is hard and bitter.");
        *ident = true;
        break;
    }

    case SV_FOOD_MEAT:
    {
        msg_print("It tastes foul.");
        *ident = true;
        break;
    }

    case SV_FOOD_LEMBAS:
    {
        msg_print("It fills you with renewed energy and spirit.");
        if (do_res_stat(A_GRA, 1))
            *ident = true;
        *ident = true;
        break;
    }
    }

    /* Food can feed the player */
    (void)set_food(p_ptr->food + o_ptr->pval);

    return (true);
}

static bool quaff_potion(object_type* o_ptr, bool* ident)
{
    /* Analyze the potion */
    switch (o_ptr->sval)
    {
    case SV_POTION_MIRUVOR:
    {
        msg_print("It has a gentle warmth, and a taste as of flowers.");
        *ident = true;
        (void)set_stun(0);
        (void)set_confused(0);
        (void)set_image(0);
        (void)set_poisoned(0);
        (void)set_blind(0);
        (void)set_cut(p_ptr->cut / 2);
        (void)set_afraid(0);
        (void)hp_player(50 + medicine_bonus(50), true, true);
        if (p_ptr->csp < p_ptr->msp)
        {
            p_ptr->csp = p_ptr->msp;
            p_ptr->csp_frac = 0;
            msg_print("You feel your power renew.");
            p_ptr->redraw |= (PR_VOICE);
            p_ptr->window |= (PW_PLAYER_0);
        }
        break;
    }

    case SV_POTION_ORCISH_LIQUOR:
    {
        msg_print("It burns your mouth and throat.");

        if (allow_player_stun(NULL))
        {
            (void)set_stun(p_ptr->stun + damroll(2, 4));
        }

        (void)set_afraid(0);
        hp_player(25 + medicine_bonus(25), true, true);
        *ident = true;
        break;
    }

    case SV_POTION_ESGALDUIN:
    {
        msg_print("It is the cold clear water of mystic Esgalduin.");
        *ident = true;

        set_tmp_per(p_ptr->tmp_per + damroll(20, 4));

        if (p_ptr->csp < p_ptr->msp)
        {
            int added = p_ptr->msp / 4;
            p_ptr->csp = p_ptr->csp + added;

            if (p_ptr->csp > p_ptr->msp)
                p_ptr->csp = p_ptr->msp;

            p_ptr->csp_frac = 0;
            msg_print("You feel your power renew.");
            p_ptr->redraw |= (PR_VOICE);
            p_ptr->window |= (PW_PLAYER_0);
        }
        break;
    }

    case SV_POTION_CLARITY:
    {
        if (set_stun(0))
            *ident = true;
        if (set_confused(0))
            *ident = true;
        if (set_image(0))
            *ident = true;
        if (set_rage(0))
            *ident = true;
        break;
    }

    case SV_POTION_HEALING:
    {
        msg_print("It has the bitter taste of medicine.");
        *ident = true;
        set_cut(p_ptr->cut / 2);
        hp_player(50 + medicine_bonus(50), true, true);
        break;
    }

    case SV_POTION_VOICE:
    {
        if (p_ptr->csp < p_ptr->msp)
        {
            p_ptr->csp = p_ptr->msp;
            p_ptr->csp_frac = 0;
            msg_print("You feel your power renew.");
            p_ptr->redraw |= (PR_VOICE);
            p_ptr->window |= (PW_PLAYER_0);
            *ident = true;
        }
        break;
    }

    case SV_POTION_true_SIGHT:
    {
        if (set_blind(0))
            *ident = true;
        if (set_image(0))
            *ident = true;
        if (set_tim_invis(p_ptr->tim_invis + damroll(10, 4)))
        {
            *ident = true;
        }
        break;
    }

    case SV_POTION_ANTIDOTE:
    {
        if (p_ptr->poisoned)
        {
            set_poisoned(0);
            *ident = true;
        }
        break;
    }

    case SV_POTION_QUICKNESS:
    {
        if (set_fast(p_ptr->fast + damroll(10, 4)))
            *ident = true;
        break;
    }

    case SV_POTION_ELEM_RESISTANCE:
    {
        if (set_oppose_fire(p_ptr->oppose_fire + damroll(20, 4)))
        {
            *ident = true;
        }
        if (set_oppose_cold(p_ptr->oppose_cold + damroll(20, 4)))
        {
            *ident = true;
        }
        break;
    }

    case SV_POTION_STR:
    {
        if (p_ptr->stat_drain[A_STR] <= -3)
        {
            if (do_res_stat(A_STR, 3))
                *ident = true;
        }
        else if (set_tmp_str(p_ptr->tmp_str + damroll(20, 4)))
            *ident = true;
        break;
    }

    case SV_POTION_DEX:
    {
        if (p_ptr->stat_drain[A_DEX] <= -3)
        {
            if (do_res_stat(A_DEX, 3))
                *ident = true;
        }
        else if (set_tmp_dex(p_ptr->tmp_dex + damroll(20, 4)))
            *ident = true;
        break;
    }

    case SV_POTION_CON:
    {
        if (p_ptr->stat_drain[A_CON] <= -3)
        {
            if (do_res_stat(A_CON, 3))
                *ident = true;
        }
        else if (set_tmp_con(p_ptr->tmp_con + damroll(20, 4)))
            *ident = true;
        break;
    }

    case SV_POTION_GRA:
    {
        if (p_ptr->stat_drain[A_GRA] <= -3)
        {
            if (do_res_stat(A_GRA, 3))
                *ident = true;
        }
        else if (set_tmp_gra(p_ptr->tmp_gra + damroll(20, 4)))
            *ident = true;
        break;
    }

    case SV_POTION_SLOWNESS:
    {
        if (allow_player_slow(NULL))
        {
            if (set_slow(p_ptr->slow + damroll(10, 4)))
                *ident = true;
        }
        else
        {
            msg_print("You feel lethargic for a moment.");
            *ident = true;
        }
        break;
    }

    case SV_POTION_POISON:
    {
        update_combat_rolls1b(PLAYER, PLAYER, true);
        pois_dam_pure(5, 4, true);
        *ident = true;
        break;
    }

    case SV_POTION_BLINDNESS:
    {
        if (!p_ptr->blind)
        {
            *ident = true;

            if (allow_player_blind(NULL))
            {
                set_blind(p_ptr->blind + damroll(10, 4));
            }
            else
            {
                msg_print("Your vision clouds for a moment.");
            }
        }
        break;
    }

    case SV_POTION_CONFUSION:
    {
        if (allow_player_confusion(NULL))
        {
            if (set_confused(p_ptr->confused + damroll(5, 4)))
            {
                *ident = true;
            }
        }
        else
        {
            msg_print("You feel dizzy for a moment.");
            *ident = true;
        }
        break;
    }

    case SV_POTION_DEC_DEX:
    {
        if (do_dec_stat(A_DEX, NULL))
            *ident = true;
        break;
    }

    case SV_POTION_DEC_GRA:
    {
        if (do_dec_stat(A_GRA, NULL))
            *ident = true;
        break;
    }
    }

    /* Potions can feed the player */
    (void)set_food(p_ptr->food + o_ptr->pval);

    return (true);
}

static bool use_staff(object_type* o_ptr, bool* ident)
{
    int k;

    bool use_charge = true;

    int will_score = p_ptr->skill_use[S_WIL];

    /* Analyze the staff */
    switch (o_ptr->sval)
    {
    case SV_STAFF_SECRETS:
    {
        if (detect_traps())
            *ident = true;
        if (detect_doors())
            *ident = true;
        break;
    }

    case SV_STAFF_IMPRISONMENT:
    {
        if (lock_doors(will_score))
            *ident = true;
        break;
    }

    case SV_STAFF_FREEDOM:
    {
        if (detect_traps())
            *ident = true;
        if (detect_doors())
            *ident = true;
        if (open_doors(will_score))
            *ident = true;
        if (destroy_traps(will_score))
            *ident = true;
        if (close_chasms(will_score))
            *ident = true;
        break;
    }

    case SV_STAFF_LIGHT:
    {
        if (light_area(4, 4, 7))
            *ident = true;
        break;
    }

    case SV_STAFF_SANCTITY:
    {
        if (remove_curse(false))
        {
            msg_print("You feel sanctified.");
            *ident = true;
        }
        break;
    }

    case SV_STAFF_UNDERSTANDING:
    {
        if (!ident_spell())
            use_charge = false;
        *ident = true;
        break;
    }

    case SV_STAFF_REVELATIONS:
    {
        map_area();
        *ident = true;
        break;
    }

    case SV_STAFF_TREASURES:
    {
        if (detect_objects_normal())
            *ident = true;
        break;
    }

    case SV_STAFF_FOES:
    {
        if (detect_monsters())
            *ident = true;
        break;
    }

    case SV_STAFF_SLUMBER:
    {
        if (sleep_monsters(will_score))
            *ident = true;
        break;
    }

    case SV_STAFF_MAJESTY:
    {
        if (project_los(GF_FEAR, 0, 0, will_score, false))
            *ident = true;
        break;
    }

    case SV_STAFF_SELF_KNOWLEDGE:
    {
        msg_print("Your mind turns inward.");
        self_knowledge();
        *ident = true;
        break;
    }

    case SV_STAFF_WARDING:
    {
        msg_print("The base of the staff glows an intense green...");

        if (cave_clean_bold(p_ptr->py, p_ptr->px))
        {
            msg_print("You trace out a glyph of warding upon the floor.");
            warding_glyph();
        }
        else
        {
            msg_print("but you cannot draw a glyph without a clean expanse of "
                      "floor.");
            use_charge = false;
        }

        *ident = true;
        break;
    }

    case SV_STAFF_DISMAY:
    {
        if (project_los(GF_CONFUSION, 0, 0, 5 + will_score / 2, false))
            *ident = true;
        break;
    }

    case SV_STAFF_RECHARGING:
    {
        if (!recharge(CHANNELING_CHARGE_MULTIPLIER))
            use_charge = false;
        *ident = true;
        break;
    }

    case SV_STAFF_SUMMONING:
    {
        int monsters = damroll(1, 4);

        // summon 1d4 creatures on the stairs
        for (k = 0; k < monsters; k++)
        {
            if (alloc_monster(true, false))
                *ident = true;
            // random_unseen_floor(&ry, &rx);
            //(void) summon_specific(ry, rx, p_ptr->depth, 0);
        }
        break;
    }

    case SV_STAFF_SHADOWS:
    {
        if (darken_area(4, 4, 7))
            *ident = true;
        break;
    }
    }

    /* --------------------------------------------------
     * Meta‐run curse: CUR_HALLU
     *   Each stack gives +20% chance on any quaff
     *   to trigger hallucination (set_image).
     * -------------------------------------------------- */
    {
        int stacks = curse_flag_count(CUR_HALLU);
        if (stacks && use_charge)  /* only when a potion actually took effect */
        {
            int chance = stacks * 20;
            if (chance > 100) chance = 100;
            /* chance%: rand_int(100) gives 0–99, so < chance is true chance times */
            if (rand_int(100) < chance)
            {
                /* give a short hallucination burst */
                int dur = rand_int(10) + 1 + 10; /* 1–10, then +10 */
                (void)set_image(p_ptr->image + dur);
                *ident = true;
                msg_print("You suddenly see impossible colors!");
            }
        }
    }

    return (use_charge);
}

static bool play_instrument(object_type* o_ptr, bool* ident)
{
    int voice_cost = p_ptr->active_ability[S_WIL][WIL_CHANNELING] ? 10 : 20;
    int will_score;
    int dir;

    if (o_ptr->tval == TV_HORN)
    {
        /* Check that you have enough voice */
        if (p_ptr->csp < voice_cost)
        {
            flush();
            msg_print("You are out of breath.");
            return (false);
        }

        /* Get a direction, allow cancel */
        if (!get_aim_dir(&dir, 0))
            return (false);
    }

    /* Base chance of success */
    will_score = p_ptr->skill_use[S_WIL] * ((c_info[p_ptr->phouse].flags_u & UNQ_WIL_TUOR) ? 2 : 1);

    p_ptr->csp -= voice_cost;

    /* Redraw voice */
    p_ptr->redraw |= (PR_VOICE);

    /* Window stuff */
    p_ptr->window |= (PW_PLAYER_0);

    msg_print("You sound a loud note on the horn.");

    /* Analyze the horn */
    switch (o_ptr->sval)
    {
    case SV_HORN_TERROR:
    {
        if (fire_arc(GF_FEAR, dir, 0, 0, will_score, 3, 90))
            *ident = true;

        /* Make a lot of noise */
        monster_perception(true, false, -10);

        break;
    }

    case SV_HORN_THUNDER:
    {
        if (fire_arc(GF_SOUND, dir, 10, 4, will_score, 3, 90))
            *ident = true;

        /* Make a lot of noise */
        monster_perception(true, false, -20);

        break;
    }

    case SV_HORN_FORCE:
    {
        int i, j, k;
        int direction = chome[dir];

        for (i = -1; i < 2; ++i)
        {
            for (j = 3; j > 0; --j)
            {
                int knock_back_dir = cycle[direction + i];

                int y = p_ptr->py + j * ddy[knock_back_dir];
                int x = p_ptr->px + j * ddx[knock_back_dir];

                if (cave_m_idx[y][x] <= 0)
                    continue;

                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                char m_name[80];

                // Get the monster name
                monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                // skill check of Will+10 vs Con*2
                if (skill_check(PLAYER, will_score + 10,
                        monster_stat(m_ptr, A_CON) * 2, m_ptr)
                    > 0)
                {
                    bool knocked_monster_back = false;

                    for (k = 3; k > j; --k)
                    {
                        if (knock_back(
                                p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
                        {
                            knocked_monster_back = true;
                        }
                    }

                    if (knocked_monster_back)
                    {
                        if (m_ptr->ml)
                        {
                            // message for visible pushes
                            msg_format(
                                "%^s is blown backwards by the force.", m_name);

                            // identify horn if a visible enemy is pushed back
                            *ident = true;
                        }

                        m_ptr->energy = 0;
                        stun_monster(m_ptr, will_score / j);

                        // Alert the monster
                        make_alert(m_ptr);
                    }
                    else
                    {
                        if (m_ptr->ml)
                        {
                            // message for visible pushes
                            msg_format(
                                "%^s is staggered by the force.", m_name);

                            // identify horn if a visible enemy is pushed back
                            *ident = true;
                        }

                        m_ptr->energy = 0;
                        stun_monster(m_ptr, (will_score * 2) / j);

                        // Alert the monster
                        make_alert(m_ptr);
                    }
                }
                else
                {
                    if (m_ptr->ml)
                    {
                        // message for visible pushes
                        msg_format(
                            "%^s holds firm against the force of the blast.",
                            m_name);

                        // identify horn if a visible enemy is pushed back
                        *ident = true;
                    }
                }

                if (m_ptr->r_idx == R_IDX_MORGOTH)
                {
                    drop_iron_crown(m_ptr,
                        "Your blast unseats the crown from his head "
                        "and it falls to the floor.");
                }
            }
        }

        // fires a beam so it looks like all other horns
        fire_arc(GF_NOTHING, dir, 0, 0, will_score, 3, 90);

        /* Make a lot of noise */
        monster_perception(true, false, -20);

        break;
    }

    case SV_HORN_BLASTING:
    {
        if (dir == DIRECTION_UP)
        {
            // skill check of Will vs 10
            if (skill_check(PLAYER, will_score, 10, NULL) > 0)
            {
                int dam = damroll(4, 8);
                int prt = protection_roll(GF_HURT, false);
                int net_dam = dam - prt;

                // no negative damage
                if (net_dam < 0)
                    net_dam = 0;

                msg_print("The ceiling cracks and rock rains down upon you!");
                earthquake(p_ptr->py, p_ptr->px, -1, -1, 3, -1);

                update_combat_rolls1b(PLAYER, PLAYER, true);
                update_combat_rolls2(
                    4, 8, dam, -1, -1, prt, 100, GF_HURT, false);

                take_hit(net_dam, "a collapsing ceiling");

                if (allow_player_stun(NULL))
                {
                    set_stun(p_ptr->stun + net_dam * 4);
                }

                *ident = true;
            }
            else
            {
                msg_print("The blast hits the ceiling, but you did not blow "
                          "hard enough to "
                          "bring it down.");
                *ident = true;
            }
        }
        else if (dir == DIRECTION_DOWN)
        {
            // skill check of Will vs 10
            if (skill_check(PLAYER, will_score, 10, NULL) > 0)
            {
                if (p_ptr->depth < (MORGOTH_DEPTH - 1))
                {
                    // Store information for the combat rolls window
                    combat_roll_special_char = object_char(o_ptr);
                    combat_roll_special_attr = object_attr(o_ptr);

                    msg_print("The floor crumbles beneath you!");
                    message_flush();
                    msg_print("You fall through...");
                    message_flush();
                    msg_print(
                        "...and land somewhere deeper in the Iron Hells.");
                    message_flush();

                    // add to the notes file
                    do_cmd_note("Fell through the floor with a horn blast.",
                        p_ptr->depth);

                    // take some damage
                    falling_damage(true);

                    message_flush();

                    // make a note if the player loses a greater vault
                    note_lost_greater_vault();

                    /* New depth */
                    p_ptr->depth++;

                    /* Leaving */
                    p_ptr->leaving = true;

                    /* Generate rubble on arrival */
                    p_ptr->create_rubble = true;

                    *ident = true;
                }
                else
                {
                    msg_print(
                        "Cracks spread across the floor, but it holds firm.");
                    *ident = true;
                }
            }
            else
            {
                msg_print("The blast hits the floor, but you did not blow hard "
                          "enough to "
                          "collapse it.");
                *ident = true;
            }
        }
        else
        {
            if (fire_arc(GF_KILL_WALL, dir, 8, 4, will_score, 3, 90))
                *ident = true;
        }

        /* Make a lot of noise */
        monster_perception(true, false, -30);

        break;
    }

    case SV_HORN_WARNING:
    {
        // fires an inert beam so it looks like all other horns
        fire_arc(GF_NOTHING, dir, 0, 0, will_score, 3, 90);

        /* Make a lot of noise */
        monster_perception(true, false, -40);

        // Makes nearby monsters aggressive
        *ident = make_aggressive();

        break;
    }
    }

    // Break the truce if creatures see
    break_truce(false);

    return (true);
}

/*
 * Activate a wielded object.  Wielded objects never stack.
 * And even if they did, activatable objects never stack.
 *
 * Currently, only (some) artefacts, and Dragon Scale Mail, can be activated.
 * But one could, for example, easily make an activatable "Ring of Plasma".
 *
 * Note that it always takes a turn to activate an artefact, even if
 * the user hits "escape" at the "direction" prompt.
 */
static bool activate_object(object_type* o_ptr)
{
    int k, dir;

    /* Check the recharge */
    if (o_ptr->timeout)
    {
        msg_print("It glows and fades...");
        return false;
    }

    /* Activate the artefact */
    message(MSG_ZAP, 0, "You activate it...");

    /* Artefacts, except for special artefacts with dragon scale mail*/
    if ((o_ptr->name1) && (o_ptr->name1 < z_info->art_norm_max))
    {
        bool did_activation = true;

        artefact_type* a_ptr = &a_info[o_ptr->name1];
        char o_name[80];

        /* Get the basic name of the object */
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

        switch (a_ptr->activation)
        {
        case ACT_ILLUMINATION:
        {
            msg_format("The %s wells with clear light...", o_name);
            light_area(4, 4, 3);
            break;
        }

        case ACT_MAGIC_MAP:
        {
            msg_format("The %s shines brightly...", o_name);
            map_area();
            break;
        }

        case ACT_CLAIRVOYANCE:
        {
            msg_format("The %s glows a deep green...", o_name);
            wiz_light();
            (void)detect_traps();
            (void)detect_doors();
            (void)detect_stairs();
            break;
        }

        case ACT_PROT_EVIL:
        {
            // Removed
            break;
        }

        case ACT_DISP_EVIL:
        {
            // Removed
            break;
        }

        case ACT_HASTE2:
        {
            msg_format("The %s glows brightly...", o_name);
            if (!p_ptr->fast)
            {
                (void)set_fast(dieroll(75) + 75);
            }
            else
            {
                (void)set_fast(p_ptr->fast + 5);
            }
            break;
        }

        case ACT_FIRE3:
        {
            msg_format("The %s glows deep red...", o_name);
            if (!get_aim_dir(&dir, 0))
                return false;
            fire_ball(GF_FIRE, dir, 10, 4, -1, 3);
            break;
        }

        case ACT_FROST5:
        {
            msg_format("The %s glows bright white...", o_name);
            if (!get_aim_dir(&dir, 0))
                return false;
            fire_ball(GF_COLD, dir, 10, 4, -1, 3);
            break;
        }

        case ACT_ELEC2:
        {
            msg_format("The %s glows deep blue...", o_name);
            if (!get_aim_dir(&dir, 0))
                return false;
            fire_ball(GF_ELEC, dir, 10, 4, -1, 3);
            break;
        }

        case ACT_BIZZARE:
        {
            // Removed
            break;
        }

        case ACT_STAR_BALL:
        {
            // Removed
            break;
        }

        case ACT_RAGE_BLESS_RESIST:
        {
            int act_time = dieroll(50) + 50;
            msg_format("Your %s glows many colours...", o_name);
            (void)hp_player(25, true, true);
            (void)set_afraid(0);
            (void)set_rage(p_ptr->rage + damroll(10, 4));
            (void)set_oppose_fire(p_ptr->oppose_fire + act_time);
            (void)set_oppose_cold(p_ptr->oppose_cold + act_time);
            (void)set_oppose_pois(p_ptr->oppose_pois + act_time);
            break;
        }

        case ACT_HEAL2:
        {
            msg_format("Your %s glows a bright white...", o_name);
            msg_print("You are fully healed...");
            (void)hp_player(100, true, true);
            (void)set_cut(0);
            break;
        }

        case ACT_PHASE:
        {
            msg_format("Your %s twists space around you...", o_name);
            teleport_player(10);
            break;
        }

        case ACT_BANISHMENT:
        {
            msg_format("Your %s glows deep blue...", o_name);
            if (!banishment())
                return false;
            break;
        }

        case ACT_TRAP_DOOR_DEST:
        {
            msg_format("Your %s glows bright red...", o_name);
            // destroy doors touch
            break;
        }

        case ACT_DETECT:
        {
            msg_format("Your %s glows bright white...", o_name);
            msg_print("An image forms in your mind...");
            detect_all();
            break;
        }

        case ACT_HEAL1:
        {
            msg_format("Your %s glows deep green...", o_name);
            msg_print("You feel much better.");
            (void)hp_player(50, true, true);
            (void)set_cut(p_ptr->cut / 2);
            break;
        }

        case ACT_RESIST:
        {
            int act_time = dieroll(20) + 20;
            msg_format("Your %s glows many colours...", o_name);
            (void)set_oppose_fire(p_ptr->oppose_fire + act_time);
            (void)set_oppose_cold(p_ptr->oppose_cold + act_time);
            (void)set_oppose_pois(p_ptr->oppose_pois + act_time);
            break;
        }

        case ACT_SLEEP:
        {
            // Removed
            break;
        }

        case ACT_RECHARGE1:
        {
            msg_format("Your %s glows bright yellow...", o_name);
            recharge(CHANNELING_CHARGE_MULTIPLIER);
            break;
        }

        case ACT_TELEPORT:
        {
            msg_format("Your %s twists space around you...", o_name);
            teleport_player(100);
            break;
        }

        case ACT_RESTORE_VOICE:
        {
            msg_format("Your %s swirls around you...", o_name);
            if (p_ptr->csp < p_ptr->msp)
            {
                p_ptr->csp = p_ptr->msp;
                p_ptr->csp_frac = 0;
                msg_print("Your feel your renewed power in your voice.");
                p_ptr->redraw |= (PR_VOICE);
                p_ptr->window |= (PW_PLAYER_0);
            }
            break;
        }

        case ACT_MISSILE:
        {
            // Removed
            break;
        }

        case ACT_FIRE1:
        {
            // Removed
            break;
        }

        case ACT_FROST1:
        {
            // Removed
            break;
        }

        case ACT_LIGHTNING_BOLT:
        {
            msg_format("Your %s is covered in sparks...", o_name);
            if (!get_aim_dir(&dir, 0))
                return false;
            fire_beam(GF_ELEC, dir, 4, 8, -1);
            break;
        }

        case ACT_ACID1:
        {
            // Removed
            break;
        }

        case ACT_ARROW:
        {
            // Removed
            break;
        }

        case ACT_HASTE1:
        {
            msg_format("Your %s glows bright green...", o_name);
            if (!p_ptr->fast)
            {
                (void)set_fast(dieroll(20) + 20);
            }
            else
            {
                (void)set_fast(p_ptr->fast + 5);
            }
            break;
        }

        case ACT_REM_FEAR_POIS:
        {
            msg_format("Your %s glows deep blue...", o_name);
            (void)set_afraid(0);
            (void)set_poisoned(0);
            break;
        }

        case ACT_STINKING_CLOUD:
        {
            // Removed
            break;
        }

        case ACT_FROST2:
        {
            msg_format("Your %s is covered in frost...", o_name);
            if (!get_aim_dir(&dir, 0))
                return false;
            fire_ball(GF_COLD, dir, 5, 4, -1, 2);
            break;
        }

        case ACT_FROST4:
        {
            // Removed
            break;
        }

        case ACT_FROST3:
        {
            // Removed
            break;
        }

        case ACT_FIRE2:
        {
            msg_format("Your %s rages in fire...", o_name);
            if (!get_aim_dir(&dir, 0))
                return false;
            fire_ball(GF_FIRE, dir, 5, 4, -1, 2);
            break;
        }

        case ACT_DRAIN_LIFE2:
        {
            // Removed
            break;
        }

        case ACT_STONE_TO_MUD:
        {
            // Removed
            break;
        }

        case ACT_MASS_BANISHMENT:
        {
            msg_format("Your %s lets out a long, shrill note...", o_name);
            (void)mass_banishment();
            break;
        }

        case ACT_CURE_WOUNDS:
        {
            msg_format("Your %s radiates deep purple...", o_name);
            hp_player(50, true, true);
            (void)set_cut((p_ptr->cut / 2) - 10);
            break;
        }

        case ACT_TELE_AWAY:
        {
            // Removed
            break;
        }

        case ACT_WOR:
        {
            // Removed
            break;
        }

        case ACT_CONFUSE:
        {
            // Removed
            break;
        }

        case ACT_IDENTIFY:
        {
            // Removed
            break;
        }

        case ACT_PROBE:
        {
            // Removed
            break;
        }

        case ACT_DRAIN_LIFE1:
        {
            // Removed
            break;
        }

        case ACT_FIREBRAND:
        {
            // Removed
            break;
        }

        case ACT_STARLIGHT:
        {
            msg_format(
                "Your %s glows with the light of a thousand stars...", o_name);
            for (k = 0; k < 8; k++)
                light_line(ddd[k]);
            break;
        }

        case ACT_MANA_BOLT:
        {
            // Removed
            break;
        }

        case ACT_BERSERKER:
        {
            msg_format("Your %s glows in anger...", o_name);
            set_rage(p_ptr->rage + damroll(10, 4));
            break;
        }
        case ACT_RES_ACID:
        {
            // Removed
            break;
        }
        case ACT_RES_ELEC:
        {
            // Removed
            break;
        }
        case ACT_RES_FIRE:
        {
            msg_format("Your %s glows light red...", o_name);
            (void)set_oppose_fire(p_ptr->oppose_fire + dieroll(20) + 20);
            break;
        }
        case ACT_RES_COLD:
        {
            msg_format("Your %s glows bright white...", o_name);
            (void)set_oppose_cold(p_ptr->oppose_cold + dieroll(20) + 20);
            break;
        }
        case ACT_RES_POIS:
        {
            msg_format("Your %s glows light green...", o_name);
            (void)set_oppose_cold(p_ptr->oppose_pois + dieroll(20) + 20);
            break;
        }

        default:
        {
            did_activation = false;
        }
        }

        if (did_activation)
        {
            /* Set the recharge time */
            if (a_ptr->randtime)
                o_ptr->timeout = a_ptr->time + (byte)dieroll(a_ptr->randtime);
            else
                o_ptr->timeout = a_ptr->time;

            /* Window stuff */
            p_ptr->window |= (PW_INVEN | PW_EQUIP);

            /* Done */
            return false;
        }
    }

    /* Mistake */
    msg_print("Oops.  That object cannot be activated.");

    /* Not used up */
    return (false);
}

bool use_object(object_type* o_ptr, bool* ident)
{
    bool used;

    /* Analyze the object */
    switch (o_ptr->tval)
    {
    case TV_FOOD:
    {
        used = eat_food(o_ptr, ident);
        break;
    }

    case TV_POTION:
    {
        used = quaff_potion(o_ptr, ident);
        break;
    }

    case TV_STAFF:
    {
        used = use_staff(o_ptr, ident);
        break;
    }

    case TV_HORN:
    {
        used = play_instrument(o_ptr, ident);
        break;
    }

    default:
    {
        used = activate_object(o_ptr);
        break;
    }
    }

    return (used);
}

#ifdef MACINTOSH
static int i = 0;
#endif
