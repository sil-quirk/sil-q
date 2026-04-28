/* File: melee1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "sdl-config.h"

/*
 * Main combat rolls startup deferral state
 * original_main_combat_rolls: saved non-zero configured value at game start
 * main_combat_rolls_deferral_active: true while we have temporarily forced 0
 * main_combat_rolls_restored: becomes true once we restore the original value
 */
static int  original_main_combat_rolls = -1;
static bool main_combat_rolls_deferral_active = false;
static bool main_combat_rolls_restored = false;

/* Helper invoked from any combat roll updater to restore deferred value */
static void maybe_restore_main_combat_rolls(void)
{
    if (main_combat_rolls_deferral_active && !main_combat_rolls_restored) {
        if (op_ptr->main_combat_rolls == 0 && original_main_combat_rolls > 0) {
            op_ptr->main_combat_rolls = original_main_combat_rolls;
            main_combat_rolls_restored = true;
            main_combat_rolls_deferral_active = false;
            p_ptr->redraw |= (PR_MAP); /* recompute SCREEN_HGT */
            log_trace("maybe_restore_main_combat_rolls: restored to %d", op_ptr->main_combat_rolls);
        }
    }
}

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

static int ranged_attack_sound(int attack)
{
    switch (attack)
    {
    case 96 + 0:  /* RF4_ARROW1 */
    case 96 + 1:  /* RF4_ARROW2 */
    case 96 + 2:  /* RF4_BOULDER */
    case 96 + 23: /* RF4_THROW_WEB */
        return MSG_MONSTER_ATTACK_RANGED;
    case 96 + 3:  /* RF4_BRTH_FIRE */
    case 96 + 4:  /* RF4_BRTH_COLD */
    case 96 + 5:  /* RF4_BRTH_POIS */
    case 96 + 6:  /* RF4_BRTH_DARK */
        return MSG_MONSTER_ATTACK_BREATH;
    default:
        return -1;
    }
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
extern int protection_roll(int typ, bool melee)
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
extern int p_min(int typ, bool melee)
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
extern int p_max(int typ, bool melee)
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

void do_betrayal_ring_amulet()
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

void do_betrayal_helm_crown()
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

                p_ptr->window |= (PW_COMBAT_ROLLS);
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

                        // allow for ripostes
                        if (p_ptr->active_ability[S_EVN][EVN_RIPOSTE]
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

    p_ptr->window |= (PW_COMBAT_ROLLS);

    /* Assume we attacked */
    return (true);
}

/*********************************************************************/
/*                                                                   */
/*                      Monster Ranged Attacks                       */
/*                                                                   */
/*********************************************************************/

/*
 * Gets the number of sides used in the monster attack
 */
int get_sides(int attack)
{
    int sides;

    if (attack >= 128)
        return (false);
    else if (attack >= 96)
    {
        sides = spell_info_RF4[attack - 96][COL_SPELL_SIDES];
    }
    else
        return (false);

    return (sides);
}

/*
 * Cast a bolt at the player
 * Stop if we hit a monster
 * Affect monsters and the player
 */
static void mon_bolt(int m_idx, int typ, int dd, int ds, int dif)
{
    monster_type* m_ptr = &mon_list[m_idx];
    int py = p_ptr->py;
    int px = p_ptr->px;
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    u32b flg = PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY;

    /* Target the player with a bolt attack */
    (void)project(m_idx, 0, fy, fx, py, px, dd, ds, dif, typ, flg, 0, false);
}

/*
 * Cast a beam at the player, sometimes with limited range.
 * Do not stop if we hit a monster
 * Affect grids, monsters, and the player
 */
/*
static void mon_beam(int m_idx, int typ, int dd, int ds, int dif, int range)
{
        monster_type *m_ptr = &mon_list[m_idx];
        int py = p_ptr->py;
        int px = p_ptr->px;
        int fy = m_ptr->fy;
        int fx = m_ptr->fx;

        u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL |
                                PROJECT_PLAY;

        // Target the player with a beam attack
        (void)project(m_idx, range, fy, fx, py, px, dd, ds, dif, typ, flg, 0,
true);
}
*/

/*
 * Release a cloud, which is a ball centered on the monster that does not
 * affect other monsters (mostly to avoid annoying messages).
 *
 */
void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad)
{
    monster_type* m_ptr = &mon_list[m_idx];
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    // u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_PLAY |
    // PROJECT_HIDE;
    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_PLAY
        | PROJECT_KILL | PROJECT_HIDE;

    /* Surround the monster with a cloud */
    project(m_idx, rad, fy, fx, fy, fx, dd + 2, ds, dif, typ, flg, 0, 0);
}

/*
 * Breathe or cast an arc-shaped spell at the player.
 * Use an arc spell of specified range and width.
 * Optionally, do not harm monsters with the same r_idx.
 * Affect grids, objects, monsters, and (specifically) the player
 *
 * Monster breaths do not lose strength with distance at the same rate
 * that normal arc spells do.  If the monster is "powerful", they lose
 * less strength; otherwise, they lose more.
 */
static void mon_arc(int m_idx, int typ, bool noharm, int dd, int ds, int dif,
    int rad, int degrees_of_arc)
{
    monster_type* m_ptr = &mon_list[m_idx];

    int py = p_ptr->py;
    int px = p_ptr->px;
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    u32b flg = PROJECT_ARC | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
        | PROJECT_KILL | PROJECT_PLAY;

    /*unused variable*/
    (void)noharm;

    /* Radius of zero means no fixed limit. */
    if (rad == 0)
        rad = MAX_SIGHT;

    /* Target the player with an arc-shaped attack. */
    (void)project(m_idx, rad, fy, fx, py, px, dd + 2, ds, dif, typ, flg,
        degrees_of_arc, false);
}

// a monster calls for help

extern void shriek(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    char m_name[80];

    /* Get the monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

    if (m_ptr->ml)
    {
        if (singing(SNG_SILENCE))
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_format("%^s lets out a muffled shout for help.", m_name);
            else
                msg_format("%^s lets out a muffled shriek.", m_name);
        }
        else
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_format("%^s shouts for help.", m_name);
            else
                msg_format("%^s makes a high pitched shriek.", m_name);
        }
    }
    else
    {
        if (singing(SNG_SILENCE))
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_print("You hear a muffled shout for help.");
            else
                msg_print("You hear a muffled shriek.");
        }
        else
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_print("You hear a shout for help.");
            else
                msg_print("You hear a shriek.");
        }
    }

    // disturb the player
    disturb(0, 0);

    /* Make a lot of noise */
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
    monster_perception(false, false, -10);

    // makes monster noise too
    m_ptr->noise += 10;
}

/*
 * Monster attempts to make a ranged (non-melee) attack.
 *
 * Determine if monster can attack at range, then see if it will.  Use
 * the helper function "choose_attack_spell()" to pick a physical ranged
 * attack, magic spell, or summon.  Execute the attack chosen.  Process
 * its effects, and update character knowledge of the monster.
 *
 * Perhaps monsters should breathe at locations *near* the player,
 * since this would allow them to inflict "partial" damage.
 */
bool make_attack_ranged(monster_type* m_ptr, int attack)
{
    int spower, manacost;

    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    char m_name[80];
    char m_poss[80];

    char ddesc[80];

    /* Is the player blind? */
    bool blind = (p_ptr->blind ? true : false);

    /* Can the player see the monster casting the spell? */
    bool seen = (!blind && m_ptr->ml);

    /* Determine mana cost */
    if (attack >= 128)
        return (false);
    else if (attack >= 96)
        manacost = spell_info_RF4[attack - 96][COL_SPELL_MANA_COST];
    else
        return (false);

    /* Spend mana (for non-songs) */
    if (attack < 96 + RF4_SNG_HEAD)
        m_ptr->mana -= manacost; // Sil-x: this is a hack to only have you pay
                                 // mana for things other than songs

    /*** Get some info. ***/

    /* Extract the monster's spell power.  Must be at least 1. */
    spower = MAX(1, r_ptr->spell_power);

    /* Get the monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

    /* Get the monster possessive ("his"/"her"/"its") */
    monster_desc(m_poss, sizeof(m_name), m_ptr, 0x22);

    /* Hack -- Get the "died from" name */
    monster_desc(ddesc, sizeof(m_name), m_ptr, 0x88);

    // Sil-y: no chance of spell failure anymore

    /*Monster has cast a spell*/
    m_ptr->mflag &= ~(MFLAG_ALWAYS_CAST);

    {
        int attack_sound = ranged_attack_sound(attack);
        if (attack_sound >= 0)
            sound(attack_sound);
    }

    /*** Execute the ranged attack chosen. ***/
    switch (attack)
    {
    /* RF4_ARROW1, RF4_ARROW2 */
    case 96 + 0:
    case 96 + 1:
    {
        int dd = (attack == 96 + 0) ? 1 : 2;

        disturb(1, 0);
        if (spower < 2)
        {
            if (blind)
                msg_print("You hear a twang.");
            else
                msg_format("%^s fires an arrow.", m_name);
        }
        else
        {
            if (blind)
                msg_print("You hear a loud thwang.");
            else
                msg_format("%^s fires an arrow.", m_name);
        }

        mon_bolt(m_idx, GF_ARROW, dd, get_sides(attack), -1);

        break;
    }

    /* RF4_BOULDER */
    case 96 + 2:
    {
        disturb(1, 0);
        if (blind)
            msg_print("You hear something grunt with exertion.");
        else if (spower < 8)
            msg_format("%^s hurls a rock at you.", m_name);
        else
            msg_format("%^s hurls a boulder at you.", m_name);

        mon_bolt(m_idx, GF_BOULDER, 6, get_sides(attack), -1);

        break;
    }

    /* RF4_BRTH_FIRE */
    case 96 + 3:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        else
            msg_format("%^s breathes fire.", m_name);
        mon_arc(m_idx, GF_FIRE, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 60);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_BRTH_COLD */
    case 96 + 4:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        else
            msg_format("%^s breathes frost.", m_name);
        mon_arc(m_idx, GF_COLD, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 60);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_BRTH_POIS */
    case 96 + 5:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        else
            msg_format("%^s breathes poisonous gas.", m_name);
        mon_arc(m_idx, GF_POIS, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 90);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_BRTH_DARK */
    case 96 + 6:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        msg_format("%^s breathes darkness.", m_name);
        mon_arc(m_idx, GF_DARK, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 60);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_EARTHQUAKE */
    case 96 + 7:
    {
        int pit_y, pit_x, dy, dx;

        dy = (m_ptr->fy > p_ptr->py) ? -1 : ((m_ptr->fy < p_ptr->py) ? 1 : 0);
        dx = (m_ptr->fx > p_ptr->px) ? -1 : ((m_ptr->fx < p_ptr->px) ? 1 : 0);
        pit_y = m_ptr->fy + dy;
        pit_x = m_ptr->fx + dx;

        msg_format("%^s slams his hammer into the ground.", m_name);

        earthquake(m_ptr->fy, m_ptr->fx, pit_y, pit_x, 5,
            cave_m_idx[m_ptr->fy][m_ptr->fx]);
        break;
    }

    /* RF4_SHRIEK */
    case 96 + 8:
    {
        disturb(0, 0);

        shriek(m_ptr);
        break;
    }

    /* RF4_SCREECH */
    case 96 + 9:
    {
        disturb(1, 0);
        if (p_ptr->stun || !seen)
        {
            if (singing(SNG_SILENCE))
            {
                msg_print("The air is filled with a muffled screeching.");
            }
            else
            {
                msg_print("The air is filled with an unearthly screeching.");
            }
        }
        else
        {
            if (singing(SNG_SILENCE))
            {
                msg_format("%^s fixes its malevolent gaze upon you and lets "
                           "out a muffled "
                           "screech.",
                    m_name);
            }
            else
            {
                msg_format("%^s fixes its malevolent gaze upon you and lets "
                           "out a terrible "
                           "screech.",
                    m_name);
            }
        }

        if (allow_player_stun(m_ptr))
        {
            if (p_ptr->stun < 100)
            {
                msg_print("Your mind reels.");

                set_stun(p_ptr->stun + 20);
            }
        }

        if (allow_player_fear(m_ptr))
        {
            (void)set_afraid(p_ptr->afraid + damroll(2, 4));
        }

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -20);

        break;
    }

    /* RF4_DARKNESS */
    case 96 + 10:
    {
        disturb(0, 0);

        if (blind)
            msg_format("%^s mutters.", m_name);
        else
            msg_format("%^s gestures in shadow.", m_name);

        (void)darken_area(0, 0, 3);
        break;
    }

    /* RF4_FORGET */
    case 96 + 11:
    {
        disturb(0, 0);

        msg_format("%^s tries to blank your mind.", m_name);

        if (saving_throw(m_ptr, 0))
        {
            msg_print("You resist!");
        }
        else
        {
            msg_print("Your memories fade away.");
            wiz_dark();
        }
        break;
    }

    /* RF4_SCARE */
    case 96 + 12:
    {
        disturb(1, 0);
        if (!m_ptr->ml || one_in_(2))
        {
            msg_format("%^s lets out a terrible cry.", m_name);

            /* Make a lot of noise */
            update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
            monster_perception(false, false, -10);
        }
        else
        {
            msg_format("%^s looks into your eyes.", m_name);
        }
        if (!allow_player_fear(m_ptr) && !(p_ptr->afraid))
        {
            msg_print("You are unafraid.");
        }
        else
        {
            (void)set_afraid(p_ptr->afraid + damroll(3, 4));
        }
        break;
    }

    /* RF4_CONF */
    case 96 + 13:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s mutters.", m_name);
        else
            msg_format("%^s glares at you.", m_name);
        if (allow_player_confusion(m_ptr))
        {
            (void)set_confused(p_ptr->confused + damroll(2, 4));
        }
        break;
    }

    /* RF4_HOLD */
    case 96 + 14:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s mutters.", m_name);
        else
            msg_format("%^s stares deep into your eyes.", m_name);

        if (!allow_player_entrancement(m_ptr))
        {
            if (!p_ptr->entranced)
                msg_print("You stare back unafraid!");
        }
        // Must not already be entranced or entranced last round, as chaining
        // entrancement is too nasty
        else if (!p_ptr->entranced && !p_ptr->was_entranced)
        {
            (void)set_entranced(damroll(4, 4));
        }
        break;
    }

        /* RF4_SLOW */
    case 96 + 15:
    {
        disturb(1, 0);
        msg_format("%^s whispers of fading and decay.", m_name);

        if (!allow_player_slow(m_ptr))
        {
            msg_print("You resist.");
        }
        else
        {
            (void)set_slow(p_ptr->slow + damroll(2, 4));
        }
        break;
    }

        /* RF4_HATCH_SPIDER */
    case 96 + 16:
    {
        hatch_spider(m_ptr);

        break;
    }

        /* RF4_DIM */
    case 96 + 17:
    {
        object_type* o_ptr = &inventory[INVEN_LITE];
        int roll = dieroll(4);
        disturb(0, 0);

        switch (roll)
        {
        case 1:
            msg_format("%^s whispers of the cold beneath the earth.", m_name);
            break;
        case 2:
            msg_format("%^s whispers of dusk turning into night.", m_name);
            break;
        case 3:
            msg_format(
                "%^s whispers of flames burning low in a gathering darkness.",
                m_name);
            break;
        default:
            msg_format("%^s whispers of an ancient gloom.", m_name);
        }

        if (o_ptr->tval == TV_LIGHT && player_light_has_fuel(o_ptr))
        {
            if (o_ptr->sval == SV_LIGHT_TORCH
                || o_ptr->sval == SV_LIGHT_MALLORN)
                msg_print("Your torch sputters.");
            else if (o_ptr->sval == SV_LIGHT_LANTERN)
                msg_print("Your lantern sputters.");
            message_flush();

            player_light_add_fuel(o_ptr, -damroll(20, 20));
            if (player_light_fuel(o_ptr) < 1)
                player_light_set_fuel(o_ptr, 1);
        }

        break;
    }

        // Sil-x: only songs after this point as 96+RF4_SNG_HEAD is used in the
        // spell code to distinguish songs from non-songs

        /* RF4_SNG_BINDING */
    case 96 + 18:
    {
        song_of_binding(m_ptr);

        break;
    }

        /* RF4_SNG_PIERCING */
    case 96 + 19:
    {
        song_of_piercing(m_ptr);

        break;
    }

        /* RF4_SNG_OATHS */
    case 96 + 20:
    {
        song_of_oaths(m_ptr);

        break;
    }

    /* RF4_THROW_WEB */
    case 96 + 23:
    {
        if (blind)
            msg_print("You hear a whispering movement.");
        else
            msg_format("%^s tosses strands of sticky web at you.", m_name);

        mon_bolt(m_idx, GF_WEB, 0, 0, -1);

        break;
    }

    /* RF4_RALLY */
    case 96 + 24:
    {
        if (blind)
            msg_print("You hear a rallying cry.");
        else
            msg_format("%^s shouts a rallying cry.", m_name);

        for (int i = mon_max - 1; i >= 1; i--)
        {
            monster_type* target = &mon_list[i];
            monster_race* r_ptr = &r_info[target->r_idx];

            // Rally works on living monsters which are orcs, men, or raukar
            if (!target->r_idx || target == m_ptr
                || (!(r_ptr->flags3 & (RF3_ORC)) && !(r_ptr->flags3 & (RF3_MAN))
                    && !(r_ptr->flags3 & (RF3_RAUKO))))
            {
                continue;
            }

            int d = distance(m_ptr->fx, m_ptr->fy, target->fx, target->fy);
            target->tmp_morale += ((spower * 10 / (d + 4)) * 10);
        }

        break;
    }

        /* Paranoia */
    default:
    {
        msg_print(
            "A monster tried to cast a spell that has not yet been defined.");
    }
    }

    /* Mark minimum desired range for recalculation */
    m_ptr->min_range = 0;

    /* Remember what the monster did to us */
    if (seen)
    {
        /* Innate spell */
        if (attack < 32 * 4)
        {
            l_ptr->flags4 |= (1L << (attack - 32 * 3));
            if (l_ptr->ranged < MAX_UCHAR)
                l_ptr->ranged++;
        }
    }

    // if (seen && p_ptr->wizard)
    //	msg_format("%^s has %i mana remaining.", m_name, m_ptr->mana);

    /* Always take note of monsters that kill you */
    if (p_ptr->is_dead && (l_ptr->deaths < MAX_SHORT))
    {
        l_ptr->deaths++;
    }

    /* A spell was cast */
    return (true);
}

/*
 * Some monsters are surrounded by poison gas, terrible heat, cold, darkness etc
 * Process any such affects.
 */
void cloud_surround(int r_idx, int* typ, int* dd, int* ds, int* rad)
{
    monster_race* r_ptr = &r_info[r_idx];

    *typ = 0;
    *dd = r_ptr->spell_power / 4;
    *ds = 4;
    *rad = 1;

    /*** Determine the kind of cloud we're supposed to be giving off ***/

    /* If breaths and attrs match, the choice is clear. */
    if (r_ptr->flags4)
    {
        /* This is mostly for the serpents */
        if ((r_ptr->flags4 & (RF4_BRTH_FIRE))
            && (r_ptr->flags4 & (RF4_BRTH_POIS))
            && (r_ptr->flags4 & (RF4_BRTH_COLD))
            && (r_ptr->flags4 & (RF4_BRTH_DARK)))
        {
            int rand_num = dieroll(4);

            switch (rand_num)
            {
            case 1:
                *typ = GF_COLD;
                break;
            case 2:
                *typ = GF_FIRE;
                break;
            case 3:
                *typ = GF_POIS;
                break;
            case 4:
                *typ = GF_DARK;
                break;
            }
        }
        else if (r_ptr->flags4 & (RF4_BRTH_POIS))
            *typ = GF_POIS;
        else if (r_ptr->flags4 & (RF4_BRTH_FIRE))
            *typ = GF_FIRE;
        else if (r_ptr->flags4 & (RF4_BRTH_COLD))
            *typ = GF_COLD;
        else if (r_ptr->flags4 & (RF4_BRTH_DARK))
            *typ = GF_DARK;
    }
}

void new_combat_round(void)
{
    int i;

    log_trace("[ROUND] new_combat_round: ENTER turns_since_combat=%d, combat_number=%d, combat_number_old=%d", turns_since_combat, combat_number, combat_number_old);
    if (combat_number != 0)
        combat_number_old = combat_number;
    combat_number = 0;
    turns_since_combat++;

    /* Add the previous round to combat history before we lose it */
    add_combat_round_to_history();

    log_trace("[ROUND] new_combat_round: after inc, turns_since_combat=%d", turns_since_combat);
    if (turns_since_combat == 1)
    {
        // copy previous round's rolls into old round's rolls
        log_trace("[ROUND] copy current->old: combat_number_old(before)=%d", combat_number_old);
        for (i = 0; i < MAX_COMBAT_ROLLS; i++)
        {
            memcpy(&combat_rolls[1][i], &combat_rolls[0][i], sizeof(combat_roll));
            log_trace("[ROUND]   copied i=%d att_type=%d att=%d evn=%d dam=%d prot=%d atk=%c def=%c", i,
                      combat_rolls[1][i].att_type,
                      combat_rolls[1][i].att,
                      combat_rolls[1][i].evn,
                      combat_rolls[1][i].dam,
                      combat_rolls[1][i].prot,
                      combat_rolls[1][i].attacker_char,
                      combat_rolls[1][i].defender_char);
        }
    }
    else if (turns_since_combat == 11)
    {
        // reset old round's rolls
        combat_number_old = 0;
        for (i = 0; i < MAX_COMBAT_ROLLS; i++)
        {
            combat_rolls[1][i].att_type = COMBAT_ROLL_NONE;
        }
    }

    // reset new round's rolls
    for (i = 0; i < MAX_COMBAT_ROLLS; i++)
    {
        combat_rolls[0][i].att_type = COMBAT_ROLL_NONE;
    }
    log_trace("[ROUND] new_combat_round: EXIT turns_since_combat=%d, combat_number=%d, combat_number_old=%d", turns_since_combat, combat_number, combat_number_old);
}

/*
 * Update combat roll table part 1 (the attack rolls)
 */
void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll)
{
    /* Ensure we restore deferred main_combat_rolls before recording first roll */
    maybe_restore_main_combat_rolls();
    monster_race* r_ptr1;
    monster_race* r_ptr2;

    if (m_ptr1 == PLAYER)
    {
        r_ptr1 = &r_info[0];
    }
    else if (m_ptr1 == NULL)
    {
        // hack for traps hitting you
        r_ptr1 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr1 = &r_info[m_ptr1->image_r_idx];
    }
    else
    {
        r_ptr1 = &r_info[m_ptr1->r_idx];
    }

    if (m_ptr2 == PLAYER)
    {
        r_ptr2 = &r_info[0];
    }
    else if (m_ptr2 == NULL)
    {
        // hack for attacking Morgoth's crown
        r_ptr2 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr2 = &r_info[m_ptr2->image_r_idx];
    }
    else
    {
        r_ptr2 = &r_info[m_ptr2->r_idx];
    }

    log_trace("[ROLL1] enter: combat_number=%d old=%d turns_since_combat=%d", combat_number, combat_number_old, turns_since_combat);
    if (combat_number < MAX_COMBAT_ROLLS)
    {
        combat_rolls[0][combat_number].att_type = COMBAT_ROLL_ROLL;

        if (m_ptr1 == NULL)
        {
            combat_rolls[0][combat_number].attacker_char
                = combat_roll_special_char;
            combat_rolls[0][combat_number].attacker_attr
                = combat_roll_special_attr;
            combat_rolls[0][combat_number].is_attacker_player = false;
        }
        else if (vis || (m_ptr1 == PLAYER))
        {
            if (m_ptr1 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_attacker_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
                    combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].attacker_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].attacker_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_attacker_player = false;
                combat_rolls[0][combat_number].attacker_char = graphics_are_ascii() ? r_ptr1->d_char : r_ptr1->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].attacker_attr = graphics_are_ascii() ? r_ptr1->d_attr : r_ptr1->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_attacker_player = false;
            combat_rolls[0][combat_number].attacker_char = '?';
            combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
        }

        // hack for Iron Crown
        if (m_ptr2 == NULL)
        {
            combat_rolls[0][combat_number].defender_char = ']';
            combat_rolls[0][combat_number].defender_attr = TERM_L_DARK;
            combat_rolls[0][combat_number].is_defender_player = false;
        }
        else if (vis || (m_ptr2 == PLAYER))
        {
            if (m_ptr2 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_defender_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
                    combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].defender_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].defender_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_defender_player = false;
                combat_rolls[0][combat_number].defender_char = graphics_are_ascii() ? r_ptr2->d_char : r_ptr2->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].defender_attr = graphics_are_ascii() ? r_ptr2->d_attr : r_ptr2->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_defender_player = false;
            combat_rolls[0][combat_number].defender_char = '?';
            combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
        }

        combat_rolls[0][combat_number].att = att;
        combat_rolls[0][combat_number].att_roll = att_roll;
        combat_rolls[0][combat_number].evn = evn;
        combat_rolls[0][combat_number].evn_roll = evn_roll;

    log_trace("[ROLL1] added at index=%d atk=%c def=%c att=%d ar=%d evn=%d er=%d", combat_number,
          combat_rolls[0][combat_number].attacker_char,
          combat_rolls[0][combat_number].defender_char,
          combat_rolls[0][combat_number].att,
          combat_rolls[0][combat_number].att_roll,
          combat_rolls[0][combat_number].evn,
          combat_rolls[0][combat_number].evn_roll);
    combat_number++;
    turns_since_combat = 0;
    log_trace("[ROLL1] exit: combat_number=%d old=%d", combat_number, combat_number_old);
    }

    /* Window stuff - DO NOT set flag here; wait for update_combat_rolls2() to complete the data */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Update combat roll table part 1b (the attack when there is no roll made -- eg
 * breath attack)
 */
void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis)
{
    /* Ensure we restore deferred main_combat_rolls before recording first roll */
    maybe_restore_main_combat_rolls();
    monster_race* r_ptr1;
    monster_race* r_ptr2;

    if (m_ptr1 == PLAYER)
    {
        r_ptr1 = &r_info[0];
    }
    else if (m_ptr1 == NULL)
    {
        // hack for traps hitting you
        r_ptr1 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr1 = &r_info[m_ptr1->image_r_idx];
    }
    else
    {
        r_ptr1 = &r_info[m_ptr1->r_idx];
    }

    if (m_ptr2 == PLAYER)
    {
        r_ptr2 = &r_info[0];
    }
    else if (p_ptr->image)
    {
        r_ptr2 = &r_info[m_ptr2->image_r_idx];
    }
    else
    {
        r_ptr2 = &r_info[m_ptr2->r_idx];
    }

    log_trace("[ROLL1B] enter: combat_number=%d old=%d turns_since_combat=%d", combat_number, combat_number_old, turns_since_combat);
    if (combat_number < MAX_COMBAT_ROLLS)
    {
        combat_rolls[0][combat_number].att_type = COMBAT_ROLL_AUTO;

        if (m_ptr1 == NULL)
        {
            combat_rolls[0][combat_number].attacker_char
                = combat_roll_special_char;
            combat_rolls[0][combat_number].attacker_attr
                = combat_roll_special_attr;
            combat_rolls[0][combat_number].is_attacker_player = false;
        }
        else if (vis || (m_ptr1 == PLAYER))
        {
            if (m_ptr1 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_attacker_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
                    combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].attacker_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].attacker_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_attacker_player = false;
                combat_rolls[0][combat_number].attacker_char = graphics_are_ascii() ? r_ptr1->d_char : r_ptr1->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].attacker_attr = graphics_are_ascii() ? r_ptr1->d_attr : r_ptr1->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_attacker_player = false;
            combat_rolls[0][combat_number].attacker_char = '?';
            combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
        }

        if (vis || (m_ptr2 == PLAYER))
        {
            if (m_ptr2 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_defender_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
                    combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].defender_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].defender_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_defender_player = false;
                combat_rolls[0][combat_number].defender_char = graphics_are_ascii() ? r_ptr2->d_char : r_ptr2->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].defender_attr = graphics_are_ascii() ? r_ptr2->d_attr : r_ptr2->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_defender_player = false;
            combat_rolls[0][combat_number].defender_char = '?';
            combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
        }

    log_trace("[ROLL1B] added index=%d atk=%c def=%c (AUTO)", combat_number,
          combat_rolls[0][combat_number].attacker_char,
          combat_rolls[0][combat_number].defender_char);
    combat_number++;
    turns_since_combat = 0;
    log_trace("[ROLL1B] exit: combat_number=%d old=%d", combat_number, combat_number_old);
    }

    /* Window stuff - DO NOT set flag here; defer to main loop to avoid mid-combat updates */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Update combat roll table part 2 (the damage rolls)
 */
void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps, int prot,
    int prt_percent, int dam_type, bool melee)
{
    /* Ensure we restore deferred main_combat_rolls before completing roll */
    maybe_restore_main_combat_rolls();
    log_trace("[ROLL2] enter: combat_number=%d old=%d last_index=%d", combat_number, combat_number_old, combat_number - 1);
    if (combat_number - 1 < MAX_COMBAT_ROLLS)
    {
        combat_rolls[0][combat_number - 1].dam_type = dam_type;
        combat_rolls[0][combat_number - 1].dd = dd;
        combat_rolls[0][combat_number - 1].ds = ds;
        combat_rolls[0][combat_number - 1].dam = dam;
        combat_rolls[0][combat_number - 1].pd = pd;
        combat_rolls[0][combat_number - 1].ps = ps;
        combat_rolls[0][combat_number - 1].prot = prot;
        combat_rolls[0][combat_number - 1].prt_percent = prt_percent;
        combat_rolls[0][combat_number - 1].melee = melee;
        log_trace("[ROLL2] filled index=%d dd=%d ds=%d dam=%d pd=%d ps=%d prot=%d prt%%=%d melee=%d", 
                  combat_number - 1, dd, ds, dam, pd, ps, prot, prt_percent, melee);

        // deal with protection for the player
        // this hackishly uses the pd and ps to store the min and max prot for
        // the player
        if (pd == -1)
        {
            // use the protection values for pure elemental types if there was
            // no attack roll
            if (combat_rolls[0][combat_number - 1].att_type == COMBAT_ROLL_AUTO)
            {
                combat_rolls[0][combat_number - 1].pd = p_min(dam_type, melee);
                combat_rolls[0][combat_number - 1].ps = p_max(dam_type, melee);
            }
            // otherwise use the normal protection values
            else
            {
                combat_rolls[0][combat_number - 1].pd = p_min(GF_HURT, melee);
                combat_rolls[0][combat_number - 1].ps = p_max(GF_HURT, melee);
            }
    }
    log_trace("[ROLL2] exit: index=%d done", combat_number - 1);
    }
    
    /* Window stuff - DO NOT set flag here; defer to main loop to avoid mid-combat updates */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Display combat rolls in a window
 */

typedef struct combat_display_entry
{
    int round;
    int index;
} combat_display_entry;

static int collect_combat_display_entries(combat_display_entry* ordered, int max_entries)
{
    int count = 0;

    for (int round = 0; round < 2; round++)
    {
        int combat_num_for_round = (round == 0) ? combat_number : combat_number_old;
        if (combat_num_for_round <= 0)
            continue;

        int player_indices[MAX_COMBAT_ROLLS];
        int monster_indices[MAX_COMBAT_ROLLS];
        int player_count = 0;
        int monster_count = 0;

        for (int idx = combat_num_for_round - 1; idx >= 0; idx--)
        {
            if (combat_rolls[round][idx].att_type == COMBAT_ROLL_NONE)
                continue;

            if (combat_rolls[round][idx].is_attacker_player)
            {
                if (player_count < MAX_COMBAT_ROLLS)
                    player_indices[player_count++] = idx;
            }
            else
            {
                if (monster_count < MAX_COMBAT_ROLLS)
                    monster_indices[monster_count++] = idx;
            }
        }

        for (int i = 0; (i < player_count) && (count < max_entries); i++)
        {
            ordered[count].round = round;
            ordered[count].index = player_indices[i];
            count++;
        }

        for (int i = 0; (i < monster_count) && (count < max_entries); i++)
        {
            ordered[count].round = round;
            ordered[count].index = monster_indices[i];
            count++;
        }
    }

    return count;
}

static void draw_combat_roll_line(int row, int base_col_offset,
    const combat_roll* roll)
{
    char buf[80];
    int net_att = 0;
    int net_dam;
    int a_att;
    int a_evn;
    int a_hit;
    int a_dam_roll;
    int a_prot_roll;
    int a_net_dam = TERM_L_RED;
    int res = 1;

    log_trace("draw_combat_roll_line: row=%d att_type=%d attacker=%c defender=%c",
        row, roll->att_type, roll->attacker_char, roll->defender_char);

    if (roll->is_defender_player)
    {
        switch (roll->dam_type)
        {
        case GF_FIRE:
            res = resist_fire();
            break;
        case GF_COLD:
            res = resist_cold();
            break;
        case GF_POIS:
            res = resist_pois();
            a_net_dam = TERM_GREEN;
            break;
        case GF_DARK:
            res = resist_dark();
            break;
        default:
            res = 1;
            a_net_dam = TERM_L_RED;
            break;
        }
    }

    if (roll->is_attacker_player)
    {
        a_att = TERM_L_BLUE;
        a_evn = TERM_WHITE;
        a_hit = TERM_L_RED;
        a_dam_roll = TERM_L_BLUE;
        if (roll->prt_percent >= 100)
            a_prot_roll = TERM_WHITE;
        else if (roll->prt_percent >= 1)
            a_prot_roll = TERM_SLATE;
        else
            a_prot_roll = TERM_DARK;
    }
    else
    {
        a_att = TERM_WHITE;
        a_evn = TERM_L_BLUE;
        a_hit = TERM_L_RED;
        a_dam_roll = TERM_WHITE;
        if (roll->prt_percent >= 100)
            a_prot_roll = TERM_L_BLUE;
        else if (roll->prt_percent >= 1)
            a_prot_roll = TERM_BLUE;
        else
            a_prot_roll = TERM_DARK;
    }

    Term_putstr(base_col_offset, row, 1, TERM_WHITE, " ");
    Term_queue_char(base_col_offset + 1, row,
        roll->attacker_attr, roll->attacker_char, 0, 0);
    if (use_bigtile && !graphics_are_ascii())
    {
        if ((roll->attacker_attr & 0x80) && ((byte)roll->attacker_char & 0x80))
            Term_queue_char(base_col_offset + 2, row, 255, -1, 0, 0);
        else
            Term_queue_char(base_col_offset + 2, row, TERM_WHITE, ' ', 0, 0);
    }

    int tile_offset = (use_bigtile && !graphics_are_ascii()) ? 1 : 0;
    int base_col = base_col_offset + 2 + tile_offset;

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        int col = base_col;

        if (roll->att < 10)
        {
            strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
        }
        else
        {
            strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
        }
        Term_putstr(col, row, -1, a_att, buf);
        col += 6;

        strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
        Term_putstr(col, row, -1, a_att, buf);
        col += 4;

        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        if (net_att > 0)
        {
            strnfmt(buf, sizeof(buf), "%4d", net_att);
            Term_putstr(col, row, -1, a_hit, buf);
        }
        else
        {
            Term_putstr(col, row, -1, TERM_SLATE, "   -");
        }
        col += 4;

        strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
        Term_putstr(col, row, -1, a_evn, buf);
        col += 4;

        if (roll->evn < 10)
        {
            strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
        }
        Term_putstr(col, row, -1, a_evn, buf);
        col += 7;

        Term_putstr(col, row, 1, TERM_WHITE, " ");
        col += 1;

        Term_queue_char(col, row,
            roll->defender_attr, roll->defender_char, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                Term_queue_char(col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
            col += 2;
        }
        else
        {
            col += 1;
        }

        int damage_col = base_col + 25 + 1;
        if (use_bigtile && !graphics_are_ascii())
            damage_col += 2;
        else
            damage_col += 1;

        if ((net_att > 0) || (roll->att_type == COMBAT_ROLL_AUTO))
        {
            Term_putstr(damage_col, row, -1, TERM_L_DARK, "  ->");
            damage_col += 4;

            if (roll->ds < 10)
            {
                strnfmt(buf, sizeof(buf), "   (%dd%d) ", roll->dd, roll->ds);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
            }
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            damage_col += 9;

            strnfmt(buf, sizeof(buf), "%4d", roll->dam);
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            damage_col += 4;

            net_dam = roll->dam - roll->prot;
            if (net_dam < 0)
                net_dam = 0;

            if (net_dam > 0)
            {
                strnfmt(buf, sizeof(buf), "%4d", net_dam);
                Term_addstr(-1, a_net_dam, buf);
            }
            else
            {
                Term_addstr(-1, TERM_SLATE, "   -");
            }

            strnfmt(buf, sizeof(buf), "%4d", roll->prot);
            Term_addstr(-1, a_prot_roll, buf);

            log_debug("COMBAT_ROLL_ROLL protection: is_defender_player=%d",
                roll->is_defender_player);

            if (roll->is_defender_player)
            {
                strnfmt(buf, sizeof(buf), "  [%d-%d]", (roll->pd * roll->prt_percent) / 100,
                    (roll->ps * roll->prt_percent) / 100);
                Term_addstr(-1, a_prot_roll, buf);
            }
            else
            {
                if ((roll->ps < 1) || (roll->pd < 1))
                {
                    SDL_strlcpy(buf, "        ", sizeof(buf));
                    Term_addstr(-1, a_prot_roll, buf);
                }
                else if (roll->ps < 10)
                {
                    strnfmt(buf, sizeof(buf), "   [%dd%d]", roll->pd, roll->ps);
                    Term_addstr(-1, a_prot_roll, buf);
                }
                else
                {
                    strnfmt(buf, sizeof(buf), "  [%dd%d]", roll->pd, roll->ps);
                    Term_addstr(-1, a_prot_roll, buf);
                }
                if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
                {
                    strnfmt(buf, sizeof(buf), " (%d%%)", roll->prt_percent);
                    Term_addstr(-1, a_prot_roll, buf);
                }
            }
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        int col = base_col;
        Term_putstr(col, row, -1, TERM_L_DARK,
            "                         ");
        col += 25;

        Term_putstr(col, row, 1, TERM_WHITE, " ");
        col += 1;

        Term_queue_char(col, row,
            roll->defender_attr, roll->defender_char, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                Term_queue_char(col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
            col += 2;
        }
        else
        {
            col += 1;
        }

        int damage_col = base_col + 25 + 1;
        if (use_bigtile && !graphics_are_ascii())
            damage_col += 2;
        else
            damage_col += 1;

        int net_auto;
        if (roll->melee)
            net_auto = roll->dam - roll->prot;
        else if (res > 0)
            net_auto = (roll->dam / res) - roll->prot;
        else
            net_auto = (roll->dam * (-res)) - roll->prot;

        Term_putstr(damage_col, row, -1, TERM_L_DARK, "  ->");
        damage_col += 4;

        if (roll->ds < 10)
        {
            strnfmt(buf, sizeof(buf), "   (%dd%d) ", roll->dd, roll->ds);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
        }
        Term_putstr(damage_col, row, -1, a_dam_roll, buf);
        damage_col += 9;

        strnfmt(buf, sizeof(buf), "%4d", roll->dam);
        Term_putstr(damage_col, row, -1, a_dam_roll, buf);
        damage_col += 4;

        if (net_auto > 0)
        {
            strnfmt(buf, sizeof(buf), "%4d", net_auto);
            Term_addstr(-1, a_net_dam, buf);
        }
        else
        {
            Term_addstr(-1, TERM_SLATE, "   -");
        }

        strnfmt(buf, sizeof(buf), "%4d", roll->prot);
        Term_addstr(-1, a_prot_roll, buf);

        log_debug("COMBAT_ROLL_AUTO protection: is_defender_player=%d",
            roll->is_defender_player);

        if (roll->is_defender_player)
        {
            if (!(roll->melee))
            {
                if (res > 1)
                {
                    strnfmt(buf, sizeof(buf), "  1/%d then", res);
                    Term_addstr(-1, TERM_L_BLUE, buf);
                }
                else if (res < 0)
                {
                    strnfmt(buf, sizeof(buf), "  x%d then", -res);
                    Term_addstr(-1, TERM_L_BLUE, buf);
                }
            }

            strnfmt(buf, sizeof(buf), "  [%d-%d]", roll->pd, roll->ps);
            Term_addstr(-1, a_prot_roll, buf);
        }
        else
        {
            if ((roll->ps < 1) || (roll->pd < 1))
            {
                SDL_strlcpy(buf, "        ", sizeof(buf));
                Term_addstr(-1, a_prot_roll, buf);
            }
            else if (roll->ps < 10)
            {
                strnfmt(buf, sizeof(buf), "   [%dd%d]", roll->pd, roll->ps);
                Term_addstr(-1, a_prot_roll, buf);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "  [%dd%d]", roll->pd, roll->ps);
                Term_addstr(-1, a_prot_roll, buf);
            }
            if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
            {
                strnfmt(buf, sizeof(buf), " (%d%%)", roll->prt_percent);
                Term_addstr(-1, a_prot_roll, buf);
            }
        }
    }
}

void display_combat_rolls(void)
{

    int i;

    log_trace("display_combat_rolls: Starting - combat_number=%d, combat_number_old=%d",
        combat_number, combat_number_old);

    for (i = 0; i < Term->hgt; i++)
    {
        Term_erase(0, i, 255);
    }

    combat_display_entry ordered[MAX_COMBAT_ROLLS * 2];
    int total_entries = collect_combat_display_entries(ordered, MAX_COMBAT_ROLLS * 2);
    int entries_to_show = MIN(total_entries, Term->hgt);

    for (int entry_idx = 0; entry_idx < entries_to_show; entry_idx++)
    {
        int round = ordered[entry_idx].round;
        int idx = ordered[entry_idx].index;

        draw_combat_roll_line(entry_idx, 0, &combat_rolls[round][idx]);
    }
}


/*
 * Clear all 4 combat rolls lines in main terminal (used when settings change)
 */
void clear_main_combat_rolls_area(void)
{
    int i;
    const int col_offset = COL_MAP; /* align with display offset */
    /* Clear all 4 possible lines (one row up from bottom to avoid status line) */
    for (i = 0; i < 4; i++)
    {
        Term_putstr(col_offset, Term->hgt - 4 - 1 + i, 65, TERM_WHITE, "                                                                 ");
    }
}

/*
 * Add the current combat round to the history buffer
 */
void add_combat_round_to_history(void)
{
    int i;
    
    /* Only add if there were actual combat rolls this round */
    if (combat_number_old == 0) {
        return;
    }

    /* Get next position in circular buffer */
    combat_history_head = (combat_history_head + 1) % MAX_COMBAT_HISTORY;
    
    /* Update count if we haven't filled the buffer yet */
    if (combat_history_count < MAX_COMBAT_HISTORY) {
        combat_history_count++;
    }
    
    /* Store the combat round data */
    combat_history[combat_history_head].turn_count = turn;
    combat_history[combat_history_head].num_rolls = combat_number_old;
    
    /* Copy the combat rolls from the previous round */
    for (i = 0; i < combat_number_old && i < MAX_COMBAT_ROLLS; i++) {
        memcpy(&combat_history[combat_history_head].rolls[i], &combat_rolls[1][i], sizeof(combat_roll));
    }
}

/*
 * Display combat history menu similar to message log
 */
void do_cmd_combat_history(void)
{
    char ch;
    int i, j, n, q;
    int wid, hgt;
    char finder[80];
    char buf[120];
    cptr prompt =
        "Up/Down line  PgUp/PgDn page  Wheel/drag  / find  Left/Right pan  Esc";
    
    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));
    
    /* Count total combat rolls across all history */
    n = 0;
    for (i = 0; i < combat_history_count; i++) {
        n += combat_history[i].num_rolls;
    }
    
    /* Start on first roll */
    i = 0;
    
    /* Start at leftmost edge */
    q = 0;
    
    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Get size after any hidden-pane layout change */
    Term_get_size(&wid, &hgt);
    
    /* Process requests until done */
    while (1) {
        int body_top;
        int body_bottom;
        int visible_rows;
        int max_i;
        int page_rows;
        int range_first;
        int range_last;
        int old_i;
        int old_q;

        /* Clear screen */
        Term_clear();

        body_top = 2;
        body_bottom = hgt - 3;
        if (body_bottom < body_top)
        {
            body_top = 1;
            body_bottom = hgt - 2;
        }
        if (body_bottom < body_top)
        {
            body_top = 0;
            body_bottom = hgt - 1;
        }

        visible_rows = body_bottom - body_top + 1;
        if (visible_rows < 1)
            visible_rows = 1;

        max_i = (n > visible_rows) ? (n - visible_rows) : 0;
        if (i > max_i)
            i = max_i;
        if (i < 0)
            i = 0;

        page_rows = (visible_rows > 1) ? (visible_rows - 1) : 1;
        ui_scroll_area_begin(body_top, body_bottom,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        
        /* Display combat rolls */
        for (j = 0; (j < visible_rows) && (i + j < n); j++) {
            /* Find which history entry and which roll within that entry */
            int total_rolls = 0;
            int history_idx = -1;
            int roll_idx = -1;
            
            /* Walk through history from newest to oldest to find the roll at index (i + j) */
            for (int h = 0; h < combat_history_count; h++) {
                int hist_idx = (combat_history_head - h + MAX_COMBAT_HISTORY) % MAX_COMBAT_HISTORY;
                
                if (total_rolls + combat_history[hist_idx].num_rolls > i + j) {
                    history_idx = hist_idx;
                    roll_idx = (i + j) - total_rolls;
                    break;
                }
                total_rolls += combat_history[hist_idx].num_rolls;
            }
            
            if (history_idx == -1 || roll_idx == -1) continue;
            
            combat_history_round* round = &combat_history[history_idx];
            combat_roll* roll = &round->rolls[roll_idx];
            
            /* Skip empty rolls */
            if (roll->att_type == COMBAT_ROLL_NONE) continue;
            
            /* Format the combat roll with proper colors and alignment */
            
            /* Set up color scheme like display_combat_rolls */
            int a_att, a_evn, a_hit, a_dam_roll, a_prot_roll, a_net_dam;
            
            /* Determine if player attack or monster attack */
            bool is_player_attack = roll->is_attacker_player;
            
            if (is_player_attack) {
                a_att = TERM_L_BLUE;
                a_evn = TERM_WHITE;
                a_hit = TERM_L_RED;
                a_dam_roll = TERM_L_BLUE;
                a_net_dam = TERM_L_RED;
                if (roll->prt_percent >= 100)
                    a_prot_roll = TERM_WHITE;
                else if (roll->prt_percent >= 1)
                    a_prot_roll = TERM_SLATE;
                else
                    a_prot_roll = TERM_DARK;
            } else {
                a_att = TERM_WHITE;
                a_evn = TERM_L_BLUE;
                a_hit = TERM_L_RED;
                a_dam_roll = TERM_WHITE;
                a_net_dam = TERM_L_RED;
                if (roll->prt_percent >= 100)
                    a_prot_roll = TERM_L_BLUE;
                else if (roll->prt_percent >= 1)
                    a_prot_roll = TERM_BLUE;
                else
                    a_prot_roll = TERM_DARK;
            }
            
            /* Start building the line with proper formatting */
            int line_y = body_bottom - j;
            int col = 0;
            
            /* Apply horizontal scroll to starting column */
            col = -q;
            
            /* Add turn indicator for first roll of each turn */
            if (roll_idx == 0) {
                strnfmt(buf, sizeof(buf), "Turn %d:", round->turn_count);
                if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, buf);
                col += 9; /* "Turn XXXXX:" is about 9-10 chars */
            } else {
                col += 9; /* Same spacing for continuation rolls */
            }
            
            /* Display like combat rolls window - attacker symbol */
            if (col >= 0) Term_putstr(col, line_y, 1, TERM_WHITE, " ");
            col += 1;
            if (col >= 0) {
                Term_queue_char(col, line_y, roll->attacker_attr, roll->attacker_char, 0, 0);
                if (use_bigtile && !graphics_are_ascii())
                {
                    if ((roll->attacker_attr & 0x80) && ((byte)roll->attacker_char & 0x80))
                        Term_queue_char(col + 1, line_y, 255, -1, 0, 0);
                    else
                        Term_queue_char(col + 1, line_y, TERM_WHITE, ' ', 0, 0);
                }
            }
            col += 1;
            if (use_bigtile && !graphics_are_ascii()) col += 1;
            
            /* Attack roll section */
            if (roll->att_type == COMBAT_ROLL_ROLL) {
                /* Attack bonus */
                if (roll->att < 10) {
                    strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
                } else {
                    strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
                }
                if (col >= 0) Term_putstr(col, line_y, -1, a_att, buf);
                col += strlen(buf);
                
                /* Attack total */
                strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
                if (col >= 0) Term_putstr(col, line_y, -1, a_att, buf);
                col += 4;
                
                /* Net attack (hit calculation) */
                int net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
                if (net_att > 0) {
                    strnfmt(buf, sizeof(buf), "%4d", net_att);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_hit, buf);
                } else {
                    if (col >= 0) Term_putstr(col, line_y, -1, TERM_SLATE, "   -");
                }
                col += 4;
                
                /* Evasion total */
                strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
                if (col >= 0) Term_putstr(col, line_y, -1, a_evn, buf);
                col += 4;
                
                /* Evasion bonus */
                if (roll->evn < 10) {
                    strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
                } else {
                    strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
                }
                if (col >= 0) Term_putstr(col, line_y, -1, a_evn, buf);
                col += strlen(buf);
                
                /* Defender symbol */
                if (col >= 0) Term_putstr(col, line_y, 1, TERM_WHITE, " ");
                col += 1;
                if (col >= 0) {
                    Term_queue_char(col, line_y, roll->defender_attr, roll->defender_char, 0, 0);
                    if (use_bigtile && !graphics_are_ascii())
                    {
                        if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                            Term_queue_char(col + 1, line_y, 255, -1, 0, 0);
                        else
                            Term_queue_char(col + 1, line_y, TERM_WHITE, ' ', 0, 0);
                    }
                }
                col += 1;
                if (use_bigtile && !graphics_are_ascii()) col += 1;
                
                /* Damage section (only if hit) */
                if (net_att > 0) {
                    if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, "  ->");
                    col += 4;
                    
                    /* Damage dice */
                    if (roll->ds < 10) {
                        strnfmt(buf, sizeof(buf), "   (%dd%d)", roll->dd, roll->ds);
                    } else {
                        strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
                    }
                    if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                    col += strlen(buf);
                    
                    /* Damage total */
                    strnfmt(buf, sizeof(buf), "%4d", roll->dam);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                    col += 4;
                    
                    /* Net damage */
                    int net_dam = roll->dam - roll->prot;
                    if (net_dam < 0) net_dam = 0;
                    
                    if (net_dam > 0) {
                        strnfmt(buf, sizeof(buf), "%4d", net_dam);
                        if (col >= 0) Term_putstr(col, line_y, -1, a_net_dam, buf);
                    } else {
                        if (col >= 0) Term_putstr(col, line_y, -1, TERM_SLATE, "   -");
                    }
                    col += 4;
                    
                    /* Protection */
                    strnfmt(buf, sizeof(buf), "%4d", roll->prot);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_prot_roll, buf);
                    col += 4;
                }
                
            } else if (roll->att_type == COMBAT_ROLL_AUTO) {
                /* Auto-hit attacks */
                if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, "                         ");
                col += 25;
                
                /* Defender symbol */
                if (col >= 0) Term_putstr(col, line_y, 1, TERM_WHITE, " ");
                col += 1;
                if (col >= 0) {
                    Term_queue_char(col, line_y, roll->defender_attr, roll->defender_char, 0, 0);
                    if (use_bigtile && !graphics_are_ascii())
                    {
                        if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                            Term_queue_char(col + 1, line_y, 255, -1, 0, 0);
                        else
                            Term_queue_char(col + 1, line_y, TERM_WHITE, ' ', 0, 0);
                    }
                }
                col += 1;
                if (use_bigtile && !graphics_are_ascii()) col += 1;
                
                /* Damage section */
                if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, "  ->");
                col += 4;
                
                /* Damage dice */
                if (roll->ds < 10) {
                    strnfmt(buf, sizeof(buf), "   (%dd%d)", roll->dd, roll->ds);
                } else {
                    strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
                }
                if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                col += strlen(buf);
                
                /* Damage total */
                strnfmt(buf, sizeof(buf), "%4d", roll->dam);
                if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                col += 4;
                
                /* Net damage */
                int net_dam = roll->dam - roll->prot;
                if (net_dam < 0) net_dam = 0;
                
                if (net_dam > 0) {
                    strnfmt(buf, sizeof(buf), "%4d", net_dam);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_net_dam, buf);
                } else {
                    if (col >= 0) Term_putstr(col, line_y, -1, TERM_SLATE, "   -");
                }
                col += 4;
                
                /* Protection */
                strnfmt(buf, sizeof(buf), "%4d", roll->prot);
                if (col >= 0) Term_putstr(col, line_y, -1, a_prot_roll, buf);
                col += 4;
            }
        }
        
        range_first = (n > 0) ? (i + 1) : 0;
        range_last = (n > 0) ? (i + j) : 0;

        /* Display header */
        prt(format("Combat Log (%d-%d of %d rolls), Offset %d",
                   range_first, range_last, n, q), 0, 0);
        
        /* Display prompt */
        prt(prompt, hgt - 1, 0);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_add_text_token('8', 0, hgt - 1, prompt, "Up");
        ui_menu_click_add_text_token('2', 0, hgt - 1, prompt, "Down");
        ui_menu_click_add_text_token('9', 0, hgt - 1, prompt, "PgUp");
        ui_menu_click_add_text_token('3', 0, hgt - 1, prompt, "PgDn");
        ui_menu_click_add_text_token('/', 0, hgt - 1, prompt, "/");
        ui_menu_click_add_text_token('/', 0, hgt - 1, prompt, "find");
        ui_menu_click_add_text_token('4', 0, hgt - 1, prompt, "Left");
        ui_menu_click_add_text_token('6', 0, hgt - 1, prompt, "Right");
        ui_menu_click_add_text_token(ESCAPE, 0, hgt - 1, prompt, "Esc");
        
        /* Get a command without showing the terminal cursor */
        (void)Term_set_cursor(false);
        Term_fresh();
        {
            bool saved_hide_cursor = hide_cursor;
            hide_cursor = true;
            ch = inkey();
            hide_cursor = saved_hide_cursor;
        }

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                ch = (char)clicked_choice;
            }
            else if (ch == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }
        }
        
        /* Exit on Escape */
        if (ch == ESCAPE) break;
        
        /* Handle navigation and search same as messages */
        old_i = i;
        old_q = q;
        
        /* Horizontal scroll */
        if (ch == '4') {
            q = (q >= wid / 2) ? (q - wid / 2) : 0;
            continue;
        }
        if (ch == '6') {
            q = q + wid / 2;
            continue;
        }
        
        /* Search functionality */
        if (ch == '/') {
            s16b z;

            ui_menu_click_clear();
            ui_scroll_area_clear();

            prt("Find: ", hgt - 1, 0);
            if (!askfor_aux(finder, sizeof(finder))) continue;
            
            /* Search through combat rolls */
            for (z = i + 1; z < n; z++) {
                /* Find the roll at index z and check if it matches */
                int total_rolls = 0;
                bool found = false;
                
                for (int h = 0; h < combat_history_count; h++) {
                    int hist_idx = (combat_history_head - h + MAX_COMBAT_HISTORY) % MAX_COMBAT_HISTORY;
                    
                    if (total_rolls + combat_history[hist_idx].num_rolls > z) {
                        int r_idx = z - total_rolls;
                        combat_roll* search_roll = &combat_history[hist_idx].rolls[r_idx];
                        
                        /* Create search string for this roll */
                        char search_buf[120];
                        strnfmt(search_buf, sizeof(search_buf), "Turn %d %c (%+d) (%dd%d)",
                                combat_history[hist_idx].turn_count,
                                search_roll->attacker_char,
                                search_roll->att,
                                search_roll->dd, search_roll->ds);
                        
                        if (strstr(search_buf, finder)) {
                            i = z;
                            found = true;
                            break;
                        }
                        break;
                    }
                    total_rolls += combat_history[hist_idx].num_rolls;
                }
                
                if (found) break;
            }
        }
        
        /* Navigation */
        if (ch == '8') {
            if (i < max_i) i += 1;
        }
        if (ch == '2') {
            if (i > 0) i -= 1;
        }
        if (ch == '9') {
            i += page_rows;
            if (i > max_i) i = max_i;
        }
        if (ch == '3') {
            i -= page_rows;
            if (i < 0) i = 0;
        }
        if (ch == '7') {
            i = max_i;
        }
        if (ch == '1') {
            i = 0;
        }
        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' ')) {
            i += page_rows;
            if (i > max_i) i = max_i;
        }
        if (ch == '+') {
            if (i + 10 < max_i) i += 10;
            else i = max_i;
        }
        if ((ch == 'n') || (ch == KTRL('N'))) {
            i = (i >= page_rows) ? (i - page_rows) : 0;
        }
        if (ch == '-') {
            i = (i >= 10) ? (i - 10) : 0;
        }
        
        /* Error if no change */
        if (i == old_i && q == old_q) bell(NULL);
    }
    
    ui_scroll_area_clear();
    ui_menu_click_clear();

    /* Restore screen */
    screen_pop_supporting_panes_hidden();
    screen_load();
}

/*
 * Display detailed combat rolls for a specific round
 */
void display_combat_round_details(combat_history_round* round)
{
    int i;
    char buf[120];
    
    /* Save screen */
    screen_save();
    
    /* Clear screen */
    Term_clear();
    
    /* Display header */
    strnfmt(buf, sizeof(buf), "Combat Details - Turn %d (%d roll%s)", 
            round->turn_count, round->num_rolls, 
            (round->num_rolls == 1) ? "" : "s");
    prt(buf, 0, 0);
    
    /* Display each combat roll using similar logic to display_combat_rolls */
    int line = 2;
    int player_attacks = 0;
    int monster_attacks = 0;
    
    /* Count player attacks first */
    int total_player_attacks = 0;
    for (i = 0; i < round->num_rolls; i++) {
        if (round->rolls[i].is_attacker_player) {
            total_player_attacks++;
        }
    }
    
    /* Display each roll */
    for (i = 0; i < round->num_rolls; i++) {
        combat_roll* roll = &round->rolls[i];
        
        /* Skip empty rolls */
        if (roll->att_type == COMBAT_ROLL_NONE) continue;
        
        /* Determine line position based on attacker */
        if (roll->is_attacker_player) {
            /* Player attack */
            player_attacks++;
            line = 1 + player_attacks;
        } else {
            /* Monster attack */
            monster_attacks++;
            line = 2 + total_player_attacks + monster_attacks;
            if (total_player_attacks == 0) line--;
        }
        
        /* Build and display the combat roll line */
        char roll_line[120];
        roll_line[0] = '\0';
        
        /* Attacker symbol */
        strnfmt(buf, sizeof(buf), " %c", roll->attacker_char);
        SDL_strlcat(roll_line, buf, sizeof(roll_line));
        
        /* Attack roll info */
        if (roll->att_type == COMBAT_ROLL_ROLL) {
            strnfmt(buf, sizeof(buf), " (%+d)%4d %4d %4d [%+d] %c",
                    roll->att, roll->att + roll->att_roll,
                    (roll->att_roll + roll->att - roll->evn_roll - roll->evn > 0) ?
                        roll->att_roll + roll->att - roll->evn_roll - roll->evn : 0,
                    roll->evn + roll->evn_roll, roll->evn, roll->defender_char);
            SDL_strlcat(roll_line, buf, sizeof(roll_line));
            
            /* Damage info */
            if (roll->att_roll + roll->att - roll->evn_roll - roll->evn > 0) {
                int net_dam = roll->dam - roll->prot;
                if (net_dam < 0) net_dam = 0;
                strnfmt(buf, sizeof(buf), " -> (%dd%d) %4d %4d %4d",
                        roll->dd, roll->ds, roll->dam, net_dam, roll->prot);
                SDL_strlcat(roll_line, buf, sizeof(roll_line));
            }
        } else if (roll->att_type == COMBAT_ROLL_AUTO) {
            strnfmt(buf, sizeof(buf), "                         %c -> (%dd%d) %4d",
                    roll->defender_char, roll->dd, roll->ds, roll->dam);
            SDL_strlcat(roll_line, buf, sizeof(roll_line));
            
            int net_dam = roll->dam - roll->prot;
            if (net_dam < 0) net_dam = 0;
            strnfmt(buf, sizeof(buf), " %4d %4d", net_dam, roll->prot);
            SDL_strlcat(roll_line, buf, sizeof(roll_line));
        }
        
        /* Display the line */
        prt(roll_line, line, 0);
    }
    
    /* Display instructions */
    prt("[Press any key to return]", Term->hgt - 1, 0);
    
    /* Wait for input */
    inkey();
    
    /* Restore screen */
    screen_load();
}

/*
 * Display recent combat rolls in the main terminal's bottom rows
 */
void display_main_combat_rolls(void)
{

    int i;
    int num_lines = op_ptr->main_combat_rolls;

    if (original_main_combat_rolls == -1) {
        original_main_combat_rolls = num_lines;
        if (original_main_combat_rolls > 0) {
            op_ptr->main_combat_rolls = 0;
            num_lines = 0;
            main_combat_rolls_deferral_active = true;
            log_trace("display_main_combat_rolls: deferring initial lines (saved %d)", original_main_combat_rolls);
        }
    }

    log_trace("display_main_combat_rolls: Starting - combat_number=%d, combat_number_old=%d, num_lines=%d",
        combat_number, combat_number_old, num_lines);

    const int col_offset = COL_MAP;

    for (i = 0; i < num_lines; i++)
    {
        Term_putstr(col_offset, Term->hgt - num_lines - 1 + i, 65, TERM_WHITE,
            "                                                                 ");
    }

    if (num_lines == 0)
        return;

    if (combat_number == 0 && combat_number_old == 0)
        return;

    int start_row = Term->hgt - num_lines - 1;

    combat_display_entry ordered[MAX_COMBAT_ROLLS * 2];
    int total_entries = collect_combat_display_entries(ordered, MAX_COMBAT_ROLLS * 2);
    int entries_to_show = MIN(num_lines, total_entries);

    for (int entry_idx = 0; entry_idx < entries_to_show; entry_idx++)
    {
        int round = ordered[entry_idx].round;
        int idx = ordered[entry_idx].index;
        int row = start_row + entry_idx;

        draw_combat_roll_line(row, col_offset, &combat_rolls[round][idx]);
    }
}




