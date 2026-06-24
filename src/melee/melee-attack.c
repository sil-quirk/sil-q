#include "angband.h"
#include "externs.h"
#include "melee/melee-attack.h"
#include "player/killer.h"


/*
 * Critical hits by monsters can inflict cuts and stuns.
 *
 * The chance is greater for WOUND and BATTER attacks
 */
static bool monster_cut_or_stun(int crit_bonus_dice, int net_dam, int effect)
{
    if (net_dam <= 0)
        return (false);

    /* Special case -- wounding/battering attack */
    if ((effect == RBE_WOUND) || (effect == RBE_BATTER))
    {
        if (crit_bonus_dice >= dieroll(2))
            return (true);
    }

    /* Standard attack */
    else
    {
        if (one_in_(10))
        {
            if (crit_bonus_dice >= dieroll(2))
                return (true);
        }
    }

    return (false);
}

bool blocking_bonus_active(void)
{
    bool moved_last_turn = (p_ptr->previous_action[0] >= 1)
        && (p_ptr->previous_action[0] <= 9) && (p_ptr->previous_action[0] != 5);

    return !moved_last_turn && p_ptr->active_ability[S_EVN][EVN_BLOCKING];
}

/*
 * Determine whether there is a bonus die for an elemental attack that
 * the player doesn't resist
 */
int elem_bonus(int effect)
{
    int resistance = 1;

    switch (effect)
    {
    case RBE_FIRE:
        resistance = resist_fire();
        break;
    case RBE_COLD:
        resistance = resist_cold();
        break;
    case RBE_POISON:
        resistance = resist_pois();
        break;
    case RBE_DARK:
        resistance = resist_dark();
        break;
    default:
        return (0);
    }

    if (resistance == 1)
        return (1);
    if (resistance < 0)
        return (-resistance);

    return (0);
}

/*
 * Calculate effective protection sides accounting for depth-scaling
 */
static int effective_ps(const object_type* o_ptr)
{
    int ps = o_ptr->ps;
    if (ps <= 0) return ps;

    u32b f1, f2, f3, f4;
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    if (f4 & TR4_DEPTH_SCALE_PS)
    {
        int depth = p_ptr->depth;
        if (depth < 0) depth = 0;
        ps += depth / 5;
    }
    return ps;
}

static u32b protection_flag_for_attack_type(int typ)
{
    switch (typ)
    {
    case GF_FIRE:
        return TR4_PROT_FIRE;
    case GF_COLD:
        return TR4_PROT_COLD;
    case GF_POIS:
        return TR4_PROT_POIS;
    case GF_DARK:
        return TR4_PROT_DARK;
    default:
        return 0L;
    }
}

static bool protection_applies_for_attack(const object_type* o_ptr, int typ)
{
    u32b ignored = 0L, f4 = 0L;
    u32b flag;

    if (typ == GF_HURT)
        return true;

    flag = protection_flag_for_attack_type(typ);
    if (!flag || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &ignored, &ignored, &ignored, &f4);

    return (f4 & flag) != 0;
}

/*
 * Roll the protection dice for all parts of the player's armour
 */
int protection_roll(int typ, bool melee)
{
    (void)melee;
    int i;
    object_type* o_ptr;
    int prt = 0;
    int mult = 1;
    int armour_weight = 0;
    int side_shift = curse_flag_delta_cur(CUR_ARMOR_SIDE_SHIFT);

    // things that always count:

    if (singing(SNG_STAYING))
    {
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) prt += damroll(4, 2);
        else prt += damroll(2, 2);
    }

    // armour:

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // off-hand weapons are not armour, so skip them
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;
        if ((i == INVEN_ARM) && !player_shield_counts_for_active_weapon(o_ptr))
            continue;

        if (i >= INVEN_BODY)
            armour_weight += o_ptr->weight;

        // shields can apply their protection to melee or flagged attack types
        if (i == INVEN_ARM)
        {
            if (protection_applies_for_attack(o_ptr, typ))
            {
                if (blocking_bonus_active())
                {
                    mult = 2;
                }
                if (o_ptr->pd > 0)
                {
                    int sides = effective_ps(o_ptr);
                    if (side_shift && sides > 0) {
                        sides -= side_shift;
                        if (sides < 1) sides = 1;
                    }
                    prt += damroll(o_ptr->pd * mult, sides);
                }
            }
        }

        else if (protection_applies_for_attack(o_ptr, typ))
        {
            if (o_ptr->ps > 0)
            {
                int sides = effective_ps(o_ptr);
                if (side_shift && sides > 0) {
                    sides -= side_shift;
                    if (sides < 1) sides = 1;
                }
                prt += damroll(o_ptr->pd, sides);
            }
        }
    }

    // heavy armour bonus
    if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR] && (typ == GF_HURT))
    {
        /* Use Xd1 (fixed X) instead of 1dX (random 1..X) */
        prt += damroll(armour_weight / 150, 1);
    }

    return prt;
}

/*
 * Roll the protection dice for all parts of the player's armour
 */
int p_min(int typ, bool melee)
{
    (void)melee;
    int i;
    object_type* o_ptr;
    int prt = 0;
    int armour_weight = 0;
    int mult = 1;

    // things that always count:

    if (singing(SNG_STAYING))
    {
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) prt += 4;
        else prt += 2;
    }

    // armour:

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // off-hand weapons are not armour, so skip them
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;
        if ((i == INVEN_ARM) && !player_shield_counts_for_active_weapon(o_ptr))
            continue;

        if (i >= INVEN_BODY)
            armour_weight += o_ptr->weight;

        // shields can apply their protection to melee or flagged attack types
        if (i == INVEN_ARM)
        {
            if (protection_applies_for_attack(o_ptr, typ))
            {
                if (blocking_bonus_active())
                {
                    mult = 2;
                }
                if (o_ptr->pd > 0)
                {
                    prt += o_ptr->pd * mult;
                }
            }
        }

        else if (protection_applies_for_attack(o_ptr, typ))
        {
            if (o_ptr->ps > 0)
            {
                prt += o_ptr->pd;
            }
        }
    }

    // heavy armour bonus
    if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR] && (typ == GF_HURT))
    {
        /* With Xd1 the minimum equals X, so add the full value */
        prt += armour_weight / 150;
    }

    return prt;
}

/*
 * Roll the protection dice for all parts of the player's armour
 */
int p_max(int typ, bool melee)
{
    (void)melee;
    int i;
    object_type* o_ptr;
    int prt = 0;
    int armour_weight = 0;
    int mult = 1;
    int side_shift = curse_flag_delta_cur(CUR_ARMOR_SIDE_SHIFT);

    // things that always count:

    if (singing(SNG_STAYING))
    {
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) prt += 8;
        else prt += 4;
    }

    // armour:

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // off-hand weapons are not armour, so skip them
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;
        if ((i == INVEN_ARM) && !player_shield_counts_for_active_weapon(o_ptr))
            continue;

        if (i >= INVEN_BODY)
            armour_weight += o_ptr->weight;

        // shields can apply their protection to melee or flagged attack types
        if (i == INVEN_ARM)
        {
            if (protection_applies_for_attack(o_ptr, typ))
            {
                if (blocking_bonus_active())
                {
                    mult = 2;
                }
                if (o_ptr->pd > 0)
                {
                    int sides = effective_ps(o_ptr);
                    if (side_shift && sides > 0) {
                        sides -= side_shift;
                        if (sides < 1) sides = 1;
                    }
                    prt += o_ptr->pd * mult * sides;
                }
            }
        }

        else if (protection_applies_for_attack(o_ptr, typ))
        {
            if (o_ptr->ps > 0)
            {
                int sides = effective_ps(o_ptr);
                if (side_shift && sides > 0) {
                    sides -= side_shift;
                    if (sides < 1) sides = 1;
                }
                prt += o_ptr->pd * sides;
            }
        }
    }

    // heavy armour bonus
    if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR] && (typ == GF_HURT))
    {
        prt += armour_weight / 150;
    }

    return prt;
}

/*
 * determines the size of the evasion bonus due to dodging (if any)
 */

int dodging_bonus(void)
{
    if (p_ptr->active_ability[S_EVN][EVN_DODGING]
        && wearing_only_light_armour()
        && (p_ptr->previous_action[0] >= 1) && (p_ptr->previous_action[0] <= 9)
        && (p_ptr->previous_action[0] != 5))
    {
        return 3;
    }
    else
    {
        return 0;
    }
}

/*
 * Determine whether a monster is making a valid charge attack
 */
bool monster_charge(monster_type* m_ptr)
{
    int d, i;

    int speed;

    int deltay = p_ptr->py - m_ptr->fy;
    int deltax = p_ptr->px - m_ptr->fx;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    // paranoia
    if (distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) > 1)
        return (false);

    // determine the monster speed
    speed = r_ptr->speed;
    if (m_ptr->slowed)
        speed--;

    // if it has the ability and isn't slow
    if ((r_ptr->flags2 & (RF2_CHARGE)) && (speed >= 2))
    {
        // try all three directions
        for (i = -1; i <= 1; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];

            if (m_ptr->previous_action[1] == d)
            {
                return (true);
            }
        }
    }

    return (false);
}

/*
 * Returns true if an item is a "traitor" item.
 */
bool is_traitor_item(int item_slot)
{
    u32b f1, f2, f3;
    if (item_slot >= INVEN_WIELD && item_slot < INVEN_TOTAL)
    {
        object_type* o_ptr = &inventory[item_slot];
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & TR2_TRAITOR)
            return true;
    }

    return false;
}

void do_betrayal_ring_amulet(void)
{
    object_type* o_ptr = NULL;
    object_type object_type_body;

    int item = 0;

    if (is_traitor_item(INVEN_LEFT))
        item = INVEN_LEFT;
    if (is_traitor_item(INVEN_RIGHT))
        item = INVEN_RIGHT;
    if (is_traitor_item(INVEN_NECK))
        item = INVEN_NECK;

    if (item == 0)
        return;

    get_sorted_target_list(TARGET_KILL, 4);

    if (temp_n > 4 && one_in_(100 / temp_n) && !p_ptr->truce)
    {
        int i;
        bool fell_in_chasm = false;
        char o_name[120];
        object_type* i_ptr;
        int near_y = p_ptr->py;
        int near_x = p_ptr->px;

        item = inven_takeoff(item, 1);
        if (item == -1)
            return;

        if (item >= 0)
            o_ptr = &inventory[item];
        else
            o_ptr = &o_list[0 - item];

        /* Describe */
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

        if (item == INVEN_NECK)
            msg_format("Your %s comes loose from its chain and falls!", o_name);
        else
            msg_format(
                "Your %s slips from your finger and rolls away!", o_name);

        ident_f2(TR2_TRAITOR, o_ptr);

        for (i = 0; i < temp_n; ++i)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
            monster_type* m_ptr = &mon_list[m_idx];

            set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
        }

        for (i = 0; i < 8; i++)
        {
            /* Get the location */
            int yy = p_ptr->py + ddy_ddd[i];
            int xx = p_ptr->px + ddx_ddd[i];

            // count the chasms
            if (cave_feat[yy][xx] == FEAT_CHASM)
            {
                msg_format("Your %s falls into a chasm!", o_name);
                fell_in_chasm = true;
                break;
            }
        }

        if (!fell_in_chasm)
        {
            /* Get local object */
            i_ptr = &object_type_body;

            /* Obtain local object */
            object_copy(i_ptr, o_ptr);
            i_ptr->number = 1;

            for (i = 0; i < 1000; i++)
            {
                near_y = p_ptr->py + rand_range(-3, 3);
                near_x = p_ptr->px + rand_range(-3, 3);

                if (cave_floor_bold(near_y, near_x))
                    break;
            }

            drop_near(i_ptr, 0, near_y, near_x);
        }

        if (item >= 0)
        {
            inven_item_increase(item, -1);
            inven_item_optimize(item);
        }
        else
        {
            floor_item_increase(0 - item, -1);
            floor_item_optimize(0 - item);
        }

        handle_stuff();
        inven_enforce_current_pack_limits();
    }
}

void do_betrayal_helm_crown(void)
{
    if (is_traitor_item(INVEN_HEAD) && one_in_(20)
        && health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_BADLY_WOUNDED)
    {
        object_type* o_ptr = &inventory[INVEN_HEAD];
        char o_name[120];

        /* Describe */
        object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

        set_blind(p_ptr->blind + damroll(3, 2));
        msg_format("Your %s twists to cover your eyes!", o_name);
        ident_f2(TR2_TRAITOR, o_ptr);
    }
}

/*
 * Attack the player via physical attacks.
 */
bool make_attack_normal(monster_type* m_ptr)
{
    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    int i, k, heal;
    int do_cut, do_stun;

    int b, blows;

    bool alive = true;

    object_type* o_ptr;

    char o_name[120];

    char m_name[80];

    char ddesc[80];

    bool blinked;

    int prt_percent = 100; // a default value to soothe compilation warnings

    int dam_type;

    killer_mark_monster(m_ptr);

    /* Not allowed to attack */
    if (r_ptr->flags1 & (RF1_NEVER_BLOW))
        return (false);

    monster_set_visual_facing_target_immediate(m_ptr, p_ptr->py, p_ptr->px);

    if (m_idx > 0)
        song_disguise_note_monster_attack(m_idx);

    /* Get the monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Get the "died from" information (i.e. "a white worm mass") */
    monster_desc(ddesc, sizeof(ddesc), m_ptr, 0x88);

    /* Assume no blink */
    blinked = false;

    /* Calculate the number of blows this monster gets */
    for (b = 0; b < MONSTER_BLOW_MAX; b++)
    {
        if (!r_ptr->blow[b].method)
            break;
    }
    blows = b;

    /* Monsters might notice */
    attacked_player = true;

    // use the alternate attack one in three times
    if ((blows > 1) && one_in_(3))
        b = 1;
    else
        b = 0;

    // introduce a new code block to all us to declare all these variables
    if (true)
    {
        bool betrayal_wield = false;
        bool betrayal_arm = false;
        bool visible = false;
        bool obvious = false;

        int total_attack_mod = 0;
        int total_evasion_mod = 0;
        int hit_result = 0;

        bool no_crit = false;
        int crit_bonus_dice = 0;
        int elem_bonus_dice = 0;
        int total_damage_dice = 0;

        int dam = 0, prt = 0;
        int net_dam = 0;

        cptr act = NULL;
        char msg[80];

        /* Extract the attack information */
        int effect = r_ptr->blow[b].effect;
        int method = r_ptr->blow[b].method;
        int att = r_ptr->blow[b].att;
        int dd = r_ptr->blow[b].dd;
        int ds = r_ptr->blow[b].ds;
        int dd_reduction = m_ptr->blow_dd_reduction[b];
        int ds_reduction = m_ptr->blow_ds_reduction[b];
        if (dd > 0 && dd_reduction > 0)
        {
            if (dd_reduction >= dd)
                dd = 1;
            else
                dd = MAX(1, dd - dd_reduction);
        }
        if (ds > 0 && ds_reduction > 0)
        {
            if (ds_reduction >= ds)
                ds = 1;
            else
                ds = MAX(1, ds - ds_reduction);
        }

        /* Hack -- no more attacks */
        // if (!method) break;  // Sil-y: not needed as this is no longer a loop

        /* Handle "leaving" */
        // if (p_ptr->leaving) break;   // Sil-y: not needed as this is no
        // longer a loop

        /* Extract visibility (before blink) */
        if (m_ptr->ml)
            visible = true;

        /* Assume no cut, stun, or touch */
        do_cut = do_stun = 0;

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, att);

        if (monster_charge(m_ptr))
        {
            total_attack_mod += 3;
            ds += 3;
        }

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        /* Check if the player was hit */
        // spores always hit (and never critical)
        if (method == RBM_SPORE)
        {
            hit_result = 1;
            update_combat_rolls1b(m_ptr, PLAYER, m_ptr->ml);
        }
        else
        {
            hit_result = hit_roll(
                total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);
        }

        sound(MSG_MONSTER_ATTACK);

        /* Monster hits player */
        if (!effect || (hit_result > 0))
        {
            /* Always disturbing */
            disturb(1, 0);

            /* Describe the attack method, apply special hit chance mods. */
            switch (method)
            {
            case RBM_HIT:
            {
                /* Handle special effect types */
                if (effect == RBE_BATTER)
                {
                    act = "batters you";
                }
                else
                {
                    act = "hits you";
                }

                do_cut = do_stun = 1;

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_TOUCH:
            {
                act = "touches you";

                // ignores armor
                prt_percent = 0;

                // can't do criticals
                no_crit = true;

                break;
            }
            case RBM_CLAW:
            {
                act = "claws you";
                do_cut = 1;

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_BITE:
            {
                act = "bites you";
                do_cut = 1;

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_PECK:
            {
                act = "pecks you";
                do_cut = 1;

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_STING:
            {
                act = "stings you";

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_CRUSH:
            {
                if (dam >= 10)
                    act = "crushes you";
                do_stun = 1;

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_ENGULF:
            {
                act = "engulfs you";

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_CRAWL:
            {
                act = "crawls on you";

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_THORN:
            {
                act = "tears at you";

                // stopped by armor
                prt_percent = 100;

                break;
            }
            case RBM_SPORE:
            {
                act = "releases a cloud of spores";

                // ignores armor
                prt_percent = 0;

                // can't do criticals
                no_crit = true;

                break;
            }
            case RBM_WHIP:
            {
                act = "whips you";

                // stopped by armor
                prt_percent = 100;

                break;
            }
            }

            /* Determine critical-hit bonus dice (if any) */
            // treats attack a weapon weighing 2 pounds per damage die
            crit_bonus_dice = crit_bonus(
                hit_result, 20 * dd, &r_info[0], S_MEL, false, m_ptr, NULL);

            /* Determine elemental attack bonus dice (if any)  */
            elem_bonus_dice = elem_bonus(effect);

            /* certain attacks can't do criticals */
            if (no_crit)
                crit_bonus_dice = 0;

            total_damage_dice = dd + crit_bonus_dice + elem_bonus_dice;

            /* Roll out the damage */
            dam = damroll(total_damage_dice, ds);

            /* Determine the armour based damage-reduction for the player */
            /* Note that some attack types should ignore this             */
            prt = protection_roll(GF_HURT, true);

            // now calculate net_dam, taking (modified) protection into account
            prt = (prt * prt_percent) / 100;
            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            // Traitor items may expose their users to big (non-lethal) hits
            betrayal_wield = is_traitor_item(INVEN_WIELD);
            betrayal_arm = is_traitor_item(INVEN_ARM);

            if ((betrayal_wield || betrayal_arm)
                && (health_level(p_ptr->chp, p_ptr->mhp) > HEALTH_ALMOST_DEAD)
                && one_in_(20))
            {
                int max_dam = total_damage_dice * ds;
                int min_prt = p_min(GF_HURT, true);
                min_prt = (min_prt * prt_percent) / 100;

                int net_max_dam = max_dam - min_prt;

                if (net_max_dam > (p_ptr->chp - 3))
                {
                    /* Select the weapon or shield */
                    o_ptr = betrayal_wield ? &inventory[INVEN_WIELD]
                                           : &inventory[INVEN_ARM];

                    /* Describe */
                    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

                    prt = min_prt;
                    net_dam = p_ptr->chp - dieroll(4);
                    dam = net_dam + prt;

                    msg_format("Your %s feels suddenly heavy! You fail to %s "
                               "the blow!",
                        o_name, betrayal_wield ? "parry" : "block");
                    ident_f2(TR2_TRAITOR, o_ptr);
                }
            }

            /* Message */
            if (act)
            {
                char punctuation[20];

                // determine the punctuation for the attack ("...", ".", "!"
                // etc)
                attack_punctuation(punctuation, net_dam, crit_bonus_dice);

                if (monster_charge(m_ptr))
                {
                    // remember that the monster can do this
                    if (m_ptr->ml)
                        l_ptr->flags2 |= (RF2_CHARGE);

                    act = "charges you";
                }

                /* Message */
                if (act)
                    msg_format("%^s %s%s", m_name, act, punctuation);
            }

            /* Hack -- assume all attacks are obvious */
            obvious = true;

            // default damage type:
            dam_type = GF_HURT;

            /* Apply appropriate damage */
            switch (effect)
            {
            /* No effect */
            case 0:
            {
                /* Hack -- Assume obvious */
                obvious = true;

                /* Hack -- No damage */
                net_dam = 0;

                break;
            }

            /* Ordinary hit */
            case RBE_HURT:
            {
                /* Obvious */
                obvious = true;

                /* Take damage */
                take_hit(net_dam, ddesc);

                break;
            }

            /* Hit with increased chance to wound */
            case RBE_WOUND:
            {
                /* Obvious */
                obvious = true;

                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Usually don't stun */
                if ((do_stun) && (!one_in_(5)))
                    do_stun = false;

                /* Always give a chance to inflict cuts */
                do_cut = true;

                break;
            }

            /* Hit with increased chance to stun */
            case RBE_BATTER:
            {
                /* Obvious */
                obvious = true;

                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Usually don't cut */
                if ((do_cut) && (!one_in_(5)))
                    do_cut = false;

                /* Always give a chance to inflict stuns */
                do_stun = true;

                break;
            }

            /* Hit to cause earthquakes */
            case RBE_SHATTER:
            {
                /* Obvious */
                obvious = true;

                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Usually don't cut */
                if ((do_cut) && (!one_in_(5)))
                    do_cut = false;

                /* Always give a chance to inflict stuns */
                do_stun = true;

                break;
            }

            /* Hit to disenchant */
            case RBE_UN_BONUS:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Apply disenchantment */
                if (apply_disenchant(0))
                    obvious = true;

                break;
            }

            /* Hit to reduce charges of magical items */
            case RBE_UN_POWER:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Find an item */
                for (k = 0; k < 20; k++)
                {
                    /* Blindly hunt ten times for an item. */
                    i = rand_int(INVEN_PACK);

                    /* Obtain the item */
                    o_ptr = &inventory[i];

                    /* Skip non-objects */
                    if (!o_ptr->k_idx)
                        continue;

                    /* Drain charged staffs */
                    if (o_ptr->tval == TV_STAFF)
                    {
                        if (o_ptr->pval)
                        {
                            int counter;

                            heal = r_ptr->level;

                            /* Message */
                            msg_print("Energy drains from your pack!");

                            /* Obvious */
                            obvious = true;

                            /*get the number of rods/wands/staffs to be
                             * drained*/
                            if (o_ptr->tval == TV_STAFF)
                            {
                                counter = o_ptr->pval;

                                /*get the number of charges to be drained*/
                                while ((counter > 1) && (!one_in_(counter)))
                                {
                                    /*reduce by one*/
                                    counter--;
                                }

                                /*drain the wands/staffs*/
                                o_ptr->pval -= counter;

                                /*factor healing times the difference*/
                                heal *= counter;
                            }

                            /* Message */
                            if ((m_ptr->hp < m_ptr->maxhp) && (heal))
                            {
                                if (m_ptr->ml)
                                    msg_format("%^s looks healthier.", m_name);
                                else
                                    msg_format("%^s sounds healthier.", m_name);
                            }

                            /*heal is greater than monster wounds, restore mana
                             * too*/
                            if (heal > (m_ptr->maxhp - m_ptr->hp))
                            {
                                /*leave some left over for mana*/
                                heal -= (m_ptr->maxhp - m_ptr->hp);

                                /*fully heal the monster*/
                                m_ptr->hp = m_ptr->maxhp;

                                /*mana is more powerful than HP*/
                                heal /= 10;

                                /* if heal was less than 10, make it 1*/
                                if (heal < 1)
                                    heal = 1;

                                /*give message if anything left over*/
                                if (m_ptr->mana < MON_MANA_MAX)
                                {
                                    if (m_ptr->ml)
                                        msg_format(
                                            "%^s looks refreshed.", m_name);
                                    else
                                        msg_format(
                                            "%^s sounds refreshed.", m_name);
                                }

                                /*add mana*/
                                m_ptr->mana += heal;

                                if (m_ptr->mana > MON_MANA_MAX)
                                    m_ptr->mana = MON_MANA_MAX;
                            }

                            /* Simple Heal */
                            else
                                m_ptr->hp += heal;

                            /* Redraw (later) if needed */
                            if (p_ptr->health_who == m_idx)
                                p_ptr->redraw |= (PR_HEALTHBAR);

                            /* Combine / Reorder the pack */
                            p_ptr->notice |= (PN_COMBINE | PN_REORDER);

                            /* Window stuff */
                            p_ptr->window |= (PW_INVEN);

                            /* not more than one inventory
                             * slot effected. */
                            break;
                        }
                    }
                }

                break;
            }

            /* Hit to reduce mana */
            case RBE_LOSE_MANA:
            {
                int drain;

                char msg_tmp[80];
                SDL_strlcpy(msg_tmp, msg, sizeof(msg_tmp));

                /* Obvious */
                obvious = true;

                /* Damage (mana) */
                if (net_dam > 0 || dam == 0)
                {
                    if (saving_throw(m_ptr, 0))
                    {
                        SDL_strlcat(msg_tmp, "  You resist the effects.",
                            sizeof(msg_tmp));
                    }
                    else
                    {
                        if (p_ptr->csp)
                        {
                            /* Drain depends on maximum mana */
                            drain = 2 + rand_int(p_ptr->msp / 10);

                            /* Drain the mana */
                            if (drain > p_ptr->csp)
                            {
                                p_ptr->csp = 0;
                                p_ptr->csp_frac = 0;

                                SDL_strlcat(msg_tmp, "  Your voice fails you!",
                                    sizeof(msg_tmp));
                            }
                            else
                            {
                                p_ptr->csp -= drain;
                                SDL_strlcat(msg_tmp, "  Your voice wavers.",
                                    sizeof(msg_tmp));
                            }

                            /* Redraw mana */
                            p_ptr->redraw |= (PR_VOICE);

                            /* Window stuff */
                            p_ptr->window |= (PW_PLAYER_0);
                        }
                    }
                }
                /* Damage (physical) */
                take_hit(net_dam, ddesc);

                break;
            }

            /* Hit to slow */
            case RBE_SLOW:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Increase "slow" */
                if (net_dam > 0 || dam == 0)
                {
                    if (!allow_player_slow(m_ptr))
                    {
                        msg_print("You resist the effects!");
                        obvious = true;
                    }
                    else if (set_slow(p_ptr->slow + damroll(2, 4)))
                    {
                        obvious = true;
                    }
                }

                break;
            }

            /* Hit to steal objects from the pack */
            case RBE_EAT_ITEM:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Blindly scrabble in the backpack ten times */
                for (k = 0; k < 10; k++)
                {
                    object_type* i_ptr;
                    object_type object_type_body;

                    /* Pick an item */
                    i = rand_int(INVEN_PACK);

                    /* Obtain the item */
                    o_ptr = &inventory[i];

                    /* Skip non-objects */
                    if (!o_ptr->k_idx)
                        continue;

                    /* Skip artefacts */
                    if (artefact_p(o_ptr))
                        continue;

                    /* Get a description */
                    object_desc(o_name, sizeof(o_name), o_ptr, false, 3);

                    // Sil-y: perhaps need a PER check to notice?

                    /* Message */
                    msg_format("%sour %s (%c) was stolen!",
                        ((o_ptr->number > 1) ? "One of y" : "Y"), o_name,
                        index_to_label(i));

                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Obtain local object */
                    object_copy(i_ptr, o_ptr);

                    /* One item is stolen at a time. */
                    i_ptr->number = 1;

                    /* Carry the object */
                    (void)monster_carry(m_idx, i_ptr);

                    /* Steal the items */
                    inven_item_increase(i, -1);
                    inven_item_optimize(i);

                    /* Obvious */
                    obvious = true;

                    /* Done */
                    break;
                }

                break;
            }

            /* Hit to eat food */
            case RBE_EAT_FOOD:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Steal some food */
                for (k = 0; k < 6; k++)
                {
                    /* Pick an item from the pack */
                    i = rand_int(INVEN_PACK);

                    /* Get the item */
                    o_ptr = &inventory[i];

                    /* Skip non-objects */
                    if (!o_ptr->k_idx)
                        continue;

                    /* Skip non-food objects */
                    if (o_ptr->tval != TV_FOOD)
                        continue;

                    /* Get a description */
                    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

                    /* Message */
                    msg_format("%s %s (%c) was eaten!",
                        ((o_ptr->number > 1) ? "One of your" : "Your last"),
                        o_name, index_to_label(i));

                    /* Steal the items */
                    inven_item_increase(i, -1);
                    inven_item_optimize(i);

                    /* Obvious */
                    obvious = true;

                    /* Done */
                    break;
                }

                break;
            }

            /* Hit to reduce nutrition */
            case RBE_HUNGER:
            {
                int amount = 500;

                obvious = true;

                /* Take damage */
                take_hit(net_dam, ddesc);

                /* We're not dead yet */
                if (!p_ptr->is_dead && (net_dam > 0 || dam == 0))
                {
                    /* Message -- only if appropriate */
                    if (!saving_throw(m_ptr, 0))
                    {
                        msg_print("You feel an unnatural hunger...");

                        // modify the hunger caused by the player's hunger rate
                        // but go up/down by factors of 1.5 rather than 3
                        if (p_ptr->hunger < 0)
                        {
                            amount *= int_exp(2, -(p_ptr->hunger));
                            amount /= int_exp(3, -(p_ptr->hunger));
                        }
                        if (p_ptr->hunger > 0)
                        {
                            amount *= int_exp(3, p_ptr->hunger);
                            amount /= int_exp(2, p_ptr->hunger);
                        }

                        /* Reduce food counter, but not too much. */
                        set_food(p_ptr->food - amount);
                    }
                }

                break;
            }

            /* Hit to inflict acid damage */
            case RBE_ACID:
            {
                /* Obvious */
                obvious = true;

                /* Message */
                msg_print("You are covered in acid!");

                /* Special damage */
                acid_dam(dam, total_damage_dice, total_damage_dice * ds,
                    net_dam, ddesc);

                dam_type = GF_ACID;

                break;
            }

            /* Hit to electrocute */
            case RBE_ELEC:
            {
                /* Obvious */
                obvious = true;

                /* Message */
                if (net_dam > 0)
                    msg_print("You are struck by electricity!");

                /* Take damage (special) */
                elec_dam(dam, total_damage_dice, total_damage_dice * ds,
                    net_dam, ddesc);

                dam_type = GF_ELEC;

                break;
            }

            /* Hit to darken */
            case RBE_DARK:
            {
                /* Obvious */
                obvious = true;

                /* Take damage */
                dark_dam_mixed(net_dam, ddesc);

                dam_type = GF_DARK;

                break;
            }

                /* Hit to poison */
            case RBE_POISON:
            {
                /* Take "poison" effect */
                pois_dam_mixed(net_dam);

                if (net_dam > 0)
                {
                    obvious = true;
                }

                dam_type = GF_POIS;

                break;
            }

            /* Hit to burn */
            case RBE_FIRE:
            {
                /* Obvious */
                obvious = true;

                /* Message */
                if (net_dam > 0)
                    msg_print("You are enveloped in flames!");

                /* Take damage (special) */
                fire_dam_mixed(dam, total_damage_dice, total_damage_dice * ds,
                    net_dam, ddesc);

                dam_type = GF_FIRE;

                break;
            }

            case RBE_COLD:
            {
                /* Obvious */
                obvious = true;

                /* Message */
                if (net_dam > 0)
                    msg_print("You are covered with frost!");

                /* Take damage (special) */
                cold_dam_mixed(dam, total_damage_dice, total_damage_dice * ds,
                    net_dam, ddesc);

                dam_type = GF_COLD;

                break;
            }

            case RBE_BLIND:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Increase blindness */
                if (net_dam > 0 || dam == 0)
                {
                    if (allow_player_blind(m_ptr))
                    {
                        if (set_blind(p_ptr->blind + damroll(5, 4)))
                        {
                            obvious = true;
                        }
                    }
                    else
                    {
                        if (!p_ptr->blind)
                        {
                            obvious = true;
                            msg_print("Your vision quickly clears.");
                        }
                    }
                }
                break;
            }

            /* Hit to confuse */
            case RBE_CONFUSE:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Increase "confused" */
                if (net_dam > 0 || dam == 0)
                {
                    if (!allow_player_confusion(m_ptr))
                    {
                        msg_print("You resist the effects.");
                        obvious = true;
                    }
                    else if (set_confused(p_ptr->confused + damroll(2, 4)))
                    {
                        obvious = true;
                    }
                }
                break;
            }

            /* Hit to frighten */
            case RBE_TERRIFY:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Increase "afraid" */
                if (!allow_player_fear(m_ptr))
                {
                    msg_print("You stand your ground!");
                    obvious = true;
                }
                else if (set_afraid(p_ptr->afraid + damroll(2, 4)))
                {
                    obvious = true;
                }

                break;
            }

            /* Hit to entrance (never cumulative) */
            case RBE_ENTRANCE:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                /* Increase "entranced" */
                if (net_dam > 0 || dam == 0)
                {
                    if (!allow_player_entrancement(m_ptr))
                    {
                        msg_print("You are unaffected!");
                        obvious = true;
                    }
                    else if (!p_ptr->entranced && !p_ptr->was_entranced)
                    {
                        if (set_entranced(damroll(4, 4)))
                        {
                            obvious = true;
                        }
                    }
                }
                break;
            }

            /* Hit to cause disease */
            case RBE_DISEASE:
            {
                int do_disease = net_dam;

                /* Take (adjusted) damage */
                take_hit(net_dam, ddesc);

                /* Inflict disease */
                if (net_dam > 0 || dam == 0)
                {
                    disease(&do_disease);
                }
                break;
            }

            case RBE_LOSE_STR:
            case RBE_LOSE_DEX:
            case RBE_LOSE_CON:
            case RBE_LOSE_GRA:
            case RBE_LOSE_STR_CON:
            case RBE_LOSE_ALL:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                if (net_dam > 0 || dam == 0)
                {
                    /* Reduce strength */
                    if ((effect == RBE_LOSE_STR) || (effect == RBE_LOSE_STR_CON)
                        || (effect == RBE_LOSE_ALL))
                    {
                        if (do_dec_stat(A_STR, m_ptr))
                            obvious = true;
                    }

                    /* Reduce dexterity */
                    if ((effect == RBE_LOSE_DEX) || (effect == RBE_LOSE_ALL))
                    {
                        if (do_dec_stat(A_DEX, m_ptr))
                            obvious = true;
                    }

                    /* Reduce constitution */
                    if ((effect == RBE_LOSE_CON) || (effect == RBE_LOSE_STR_CON)
                        || (effect == RBE_LOSE_ALL))
                    {
                        if (do_dec_stat(A_CON, m_ptr))
                            obvious = true;
                    }

                    /* Reduce grace */
                    if ((effect == RBE_LOSE_GRA) || (effect == RBE_LOSE_ALL))
                    {
                        if (do_dec_stat(A_GRA, m_ptr))
                            obvious = true;
                    }
                }
                break;
            }

            /* Hit to disarm */
            case RBE_DISARM:
            {
                object_type* o_ptr;
                char o_name[120];

                object_type* i_ptr;
                object_type object_type_body;

                int near_y, near_x;

                int item = INVEN_WIELD;

                int difficulty;

                /* Select the melee weapon */
                o_ptr = &inventory[INVEN_WIELD];

                /* Nothing to disamr */
                if (!o_ptr->k_idx)
                    break;

                /* Describe */
                object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

                /* Base difficulty */
                difficulty = 2;

                /* Adjustment for two handed weapons */
                if (two_handed_melee())
                {
                    difficulty -= 4;
                }

                /* Attempt a skill check against strength */
                if (skill_check(
                        m_ptr, difficulty, p_ptr->stat_use[A_STR] * 2, PLAYER)
                    <= 0)
                {
                    msg_format("%^s tries to disarm you, but you keep a grip "
                               "on your weapon.",
                        m_name);
                }

                /* failed check... */
                else
                {
                    /* Oops */
                    msg_format(
                        "%^s disarms you! Your %s falls to the ground nearby.",
                        m_name, o_name);

                    /* Get the original object */
                    o_ptr = &inventory[item];

                    /* Take off equipment */
                    if (item >= INVEN_WIELD)
                    {
                        /* Take off first */
                        item = inven_takeoff(item, 1);

                        if (item == -1)
                            break;

                        /* Get the original object */
                        if (item >= 0)
                            o_ptr = &inventory[item];
                        else
                            o_ptr = &o_list[0 - item];
                    }

                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Obtain local object */
                    object_copy(i_ptr, o_ptr);

                    /* Modify quantity */
                    i_ptr->number = 1;

                    for (i = 0; i < 1000; i++)
                    {
                        near_y = p_ptr->py + rand_range(-1, 1);
                        near_x = p_ptr->px + rand_range(-1, 1);

                        if (cave_floor_bold(near_y, near_x))
                            break;
                    }

                    /* Drop it near the player */
                    drop_near(i_ptr, 0, near_y, near_x);

                    /* Modify, Optimize */
                    if (item >= 0)
                    {
                        inven_item_increase(item, -1);
                        inven_item_optimize(item);
                    }
                    else
                    {
                        floor_item_increase(0 - item, -1);
                        floor_item_optimize(0 - item);
                    }

                    handle_stuff();
                    inven_enforce_current_pack_limits();
                }

                break;
            }

            case RBE_HALLU:
            {
                /* Take damage */
                take_hit(net_dam, ddesc);

                if (net_dam > 0 || dam == 0)
                {
                    /* Increase "image" */
                    if (!allow_player_image(m_ptr))
                    {
                        msg_print("You resist the effects.");
                        obvious = true;
                    }
                    else if (set_image(p_ptr->image + damroll(10, 4)))
                    {
                        obvious = true;
                    }
                }
                break;
            }
            }

            if (net_dam > 0 && p_ptr->active_ability[S_WIL][WIL_VENGEANCE]
                && !p_ptr->is_dead)
            {
                msg_print("You feel vengeful!");
                p_ptr->vengeance = 1;
            }

            update_combat_rolls2(total_damage_dice, ds, dam, -1, -1, prt,
                prt_percent, dam_type, true);

            display_hit(
                p_ptr->py, p_ptr->px, net_dam, dam_type, p_ptr->is_dead);

            /* Handle character death */
            if (p_ptr->is_dead && (l_ptr->deaths < MAX_SHORT))
            {
                l_ptr->deaths++;

                window_stuff();

                /* Leave immediately */
                return (true);
            }

            /* Hack -- only one of cut or stun */
            if (do_cut && do_stun)
            {
                /* Cancel cut */
                if (one_in_(2))
                {
                    do_cut = 0;
                }

                /* Cancel stun */
                else
                {
                    do_stun = 0;
                }
            }

            /* Handle cut */
            if (do_cut)
            {
                /* Critical hit */
                if (monster_cut_or_stun(crit_bonus_dice, net_dam, effect))
                {
                    int bleeding = net_dam / 2;
                    if (p_ptr->resist_bleed)
                    {
                        bleeding = 1;
                        ident_resist(TR2_RES_BLEED);
                    }

                    (void)set_cut(p_ptr->cut + (bleeding));
                }
            }

            /* Handle stun */
            if (do_stun)
            {
                /* Critical hit */
                if (monster_cut_or_stun(crit_bonus_dice, net_dam, effect))
                {
                    if (allow_player_stun(NULL))
                    {
                        (void)set_stun(p_ptr->stun + net_dam);
                    }
                }
            }

            // deal with Cruel Blow
            if ((r_ptr->flags2 & (RF2_CRUEL_BLOW)) && (crit_bonus_dice >= 1)
                && (net_dam > 0))
            {
                // Sil-y: ideally we'd use a call to allow_player_confuse()
                // here, but that doesn't
                //        work as it can't take the level of the critical into
                //        account. Sadly my solution doesn't let you ID
                //        confusion resistance items.
                int difficulty
                    = p_ptr->skill_use[S_WIL] + (p_ptr->resist_confu * 10);

                if (skill_check(m_ptr, crit_bonus_dice * 4, difficulty, PLAYER)
                    > 0)
                {
                    // remember that the monster can do this
                    if (m_ptr->ml)
                        l_ptr->flags2 |= (RF2_CRUEL_BLOW);

                    msg_format("You reel in pain!");

                    // confuse the player
                    set_confused(p_ptr->confused + crit_bonus_dice);
                }
            }

            // deal with Knock Back
            if (r_ptr->flags2 & (RF2_KNOCK_BACK))
            {
                // only happens on the main attack (so that bites don't knock
                // you back)
                if (b == 0)
                {
                    // determine if the player is knocked back
                    if (skill_check(m_ptr, monster_stat(m_ptr, A_STR) * 2,
                            p_ptr->stat_use[A_CON] * 2, PLAYER)
                        > 0)
                    {
                        if (p_ptr->stand_fast)
                        {
                            char m_name[80];
                            monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                            msg_format("%^s attempts to knock you back, but "
                                       "you stand fast.",
                                m_name);

                            ident_f3(TR3_STAND_FAST, NULL);
                        }
                        else
                        {
                            // do the knocking back
                            knock_back(
                                m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
                        }

                        // remember that the monster can do this
                        if (m_ptr->ml)
                            l_ptr->flags2 |= (RF2_KNOCK_BACK);
                    }
                }
            }

            // deal with cowardice
            if ((p_ptr->cowardice > 0) && (net_dam >= 10 / p_ptr->cowardice))
            {
                if (!p_ptr->afraid)
                {
                    if (allow_player_fear(m_ptr))
                    {
                        ident_f2(TR2_FEAR, NULL);

                        set_afraid(p_ptr->afraid + damroll(10, 4));
                        set_fast(p_ptr->fast + damroll(5, 4));
                    }
                }
            }

            do_betrayal_helm_crown();
        }

        /* Monster missed player */
        else
        {
            /* Analyze failed attacks */
            switch (method)
            {
            case RBM_HIT:
            case RBM_TOUCH:
            case RBM_CLAW:
            case RBM_BITE:
            case RBM_PECK:
            case RBM_STING:
            case RBM_WHIP:
            case RBM_CRUSH:
            case RBM_ENGULF:
            case RBM_CRAWL:
            case RBM_THORN:

                /* Visible monsters */
                if (m_ptr->ml && !p_ptr->confused)
                {
                    bool quake_anyway = false;
                    int damage = m_ptr->maxhp - m_ptr->hp;

                    /* Disturbing */
                    disturb(1, 0);

                    // Extra earthquakes as more damaged
                    // (really for Morgoth, who has more
                    // health to lose) - dice
                    // distribution means low probabilty
                    // at first, gradually increasing
                    quake_anyway = damage > damroll(20, 50);

                    // deal with earthquakes if they miss you by 1 or 2 or 3
                    // points
                    if ((effect == RBE_SHATTER)
                        && ((hit_result > -3) || quake_anyway))
                    {
                        /* Message */
                        msg_format("%^s just misses you.", m_name);
                        update_combat_rolls_no_damage();

                        /* Gender based message */
                        // No female earthquake causers
                        if (r_ptr->flags1 & (RF1_FEMALE))
                        {
                            msg_print("Her blow slams into the floor where you "
                                      "stood, and the "
                                      "ground shakes violently!");
                        }

                        // Morgoth
                        else if (r_ptr->flags1 & (RF1_MALE))
                        {
                            msg_print("You leap aside as his great hammer "
                                      "slams into the floor.");
                            msg_print("The ground shakes violently with the "
                                      "force of the blow!");

                            /* Radius 5 earthquake centered on the monster */
                            earthquake(m_ptr->fy, m_ptr->fx, p_ptr->py,
                                p_ptr->px, 5, cave_m_idx[m_ptr->fy][m_ptr->fx]);
                        }

                        // Kemenrauko
                        else
                        {
                            msg_print("You leap aside as its stony fist slams "
                                      "into the floor.");
                            msg_print("The ground shakes violently with the "
                                      "force of the blow!");

                            /* Radius 4 earthquake centered on the monster */
                            earthquake(m_ptr->fy, m_ptr->fx, -1, -1, 4,
                                cave_m_idx[m_ptr->fy][m_ptr->fx]);
                        }
                    }

                    // a normal miss
                    else
                    {
                        /* Message */
                        msg_format("%^s misses you.", m_name);
                        update_combat_rolls_no_damage();

                        // allow for ripostes
                        if (player_active_weapon_is_melee()
                            && p_ptr->active_ability[S_EVN][EVN_RIPOSTE]
                            && (p_ptr->ripostes < 1) && !p_ptr->afraid
                            && !p_ptr->confused && !p_ptr->entranced
                            && (p_ptr->stun <= 100) && m_ptr->ml
                            && (hit_result <= -10
                                    - (((&inventory[INVEN_WIELD])->weight + 9)
                                        / 10)))
                        {
                            if (valorous_oath_auto_attack_safety
                                && chosen_oath(OATH_VALOROUS)
                                && !oath_invalid(OATH_VALOROUS) && m_ptr->ml
                                && (m_ptr->stance == STANCE_FLEEING))
                            {
                                msg_print("You hold back your riposte to avoid striking a fleeing foe.");
                            }
                            else
                            {
                                msg_print("You riposte!");
                                p_ptr->ripostes++;
                                py_attack_aux(m_ptr->fy, m_ptr->fx, ATT_RIPOSTE);
                            }
                        }
                    }
                }

                break;
            }
        }

        /* Analyze "visible" monsters only */
        if (visible)
        {
            /* Count "obvious" attacks (and ones that cause damage) */
            if (obvious || dam || (l_ptr->blows[b] > 10))
            {
                /* Count attacks of this type */
                if (l_ptr->blows[b] < MAX_UCHAR)
                {
                    l_ptr->blows[b]++;
                }
            }
        }

        /*hack - stop attacks if monster and player are no longer next to each
         * other*/
        // if (do_break) break; // Sil-y: not needed as this is no longer a loop
    }

    /* Blink away */
    if ((blinked) && (alive))
    {
        msg_print("There is a puff of smoke!");
        teleport_away(m_idx, MAX_SIGHT * 2 + 5);
    }

    /* Assume we attacked */
    return (true);
}
