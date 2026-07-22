#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "player/player-upkeep-internal.h"

static bool heavy_armour_evasion_bonus_applies(const object_type* o_ptr)
{
    return (o_ptr->tval == TV_MAIL)
        && ((o_ptr->sval == SV_MAIL_CORSLET)
            || (o_ptr->sval == SV_LONG_CORSLET));
}

/*
 * Calculate the player's current "state", taking into account
 * not only race/character intrinsics, but also objects being worn
 * and temporary spell effects.
 *
 * See also calc_voice() and calc_hitpoints().
 *
 * The "weapon" and "bow" do *not* add to the bonuses to hit or to
 * damage, since that would affect non-combat things.  These values
 * are actually added in later, at the appropriate place.
 *
 * This function induces various "status" messages.
 */
void calc_bonuses(void)
{
    int i, j;

    int old_speed;

    int old_telepathy;
    int old_see_inv;

    int old_mdd = p_ptr->mdd;
    int old_mds = p_ptr->mds;

    int old_mdd2 = p_ptr->mdd2;
    int old_mds2 = p_ptr->mds2;

    int old_add = p_ptr->add;
    int old_ads = p_ptr->ads;

    int new_p_min = p_min(GF_HURT, true);
    int new_p_max = p_max(GF_HURT, true);

    int old_stat_use[A_MAX];
    int old_stat_tmp_mod[A_MAX];

    int old_skill_use[S_MAX];

    object_type* o_ptr;

    u32b f1, f2, f3;

    int armour_weight = 0;

    // Remove off-hand weapons if you cannot wield them
    if (!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON])
    {
        o_ptr = &inventory[INVEN_ARM];

        if ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM)
            || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING))
        {
            char o_name[80];

            /* Full object description */
            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

            /* Print the messages */
            msg_print("You can no longer wield both weapons.");

            // take it off
            do_cmd_takeoff(o_ptr, INVEN_ARM);
        }
    }

    /*** Memorize ***/

    /* Save the old speed */
    old_speed = p_ptr->pspeed;

    /* Save the old vision stuff */
    old_telepathy = p_ptr->telepathy;
    old_see_inv = p_ptr->see_inv;

    /* Save the old stats */
    for (i = 0; i < A_MAX; i++)
    {
        old_stat_use[i] = p_ptr->stat_use[i];
        old_stat_tmp_mod[i] = p_ptr->stat_drain[i];
    }

    /* Save the old skills */
    for (i = 0; i < S_MAX; i++)
    {
        old_skill_use[i] = p_ptr->skill_use[i];
    }

    /*** Reset ***/

    /* Reset player speed */
    p_ptr->pspeed = 2;

    /* Reset "fire" info */
    p_ptr->add = 0;
    p_ptr->ads = 0;
    p_ptr->ammo_tval = 0;

    /* Clear the equipment stat modifiers */
    for (i = 0; i < A_MAX; i++)
        p_ptr->stat_equip_mod[i] = 0;

    /* Clear the misc stat modifiers */
    for (i = 0; i < A_MAX; i++)
        p_ptr->stat_misc_mod[i] = 0;

    /* Clear the total values of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_use[i] = 0;

    /* Clear the stat modifiers of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_stat_mod[i] = 0;

    /* Clear the equipment modifiers of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_equip_mod[i] = 0;

    /* Clear the misc modifiers of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_misc_mod[i] = 0;

    /* Clear other bonuses */
    p_ptr->to_mdd = 0;
    p_ptr->to_mds = 0;
    p_ptr->mdd = 0;
    p_ptr->mds = 0;
    p_ptr->mdd2 = 0;
    p_ptr->mds2 = 0;
    p_ptr->offhand_mel_mod = 0;
    p_ptr->to_ads = 0;

    /* Clear all the flags */
    p_ptr->hunger = 0;
    p_ptr->danger = 0;
    p_ptr->aggravate = 0;
    p_ptr->cowardice = 0;
    p_ptr->haunted = 0;
    p_ptr->see_inv = 0;
    p_ptr->free_act = 0;
    p_ptr->stand_fast = 0;
    p_ptr->avoid_traps = 0;
    p_ptr->regenerate = 0;
    p_ptr->telepathy = 0;
    p_ptr->sustain_str = 0;
    p_ptr->sustain_con = 0;
    p_ptr->sustain_dex = 0;
    p_ptr->sustain_gra = 0;
    p_ptr->resist_fire = 1;
    p_ptr->resist_cold = 1;
    p_ptr->resist_pois = 1;
    p_ptr->resist_bleed = 0;
    p_ptr->resist_fear = 0;
    p_ptr->resist_blind = 0;
    p_ptr->resist_confu = 0;
    p_ptr->resist_stun = 0;
    p_ptr->resist_hallu = 0;

    /* Clear the item granted abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            /* For Special abilities skill, preserve quest-granted abilities */
            if (i == S_SPC) {
                /* Don't reset special abilities - they're not item-granted */
                continue;
            }
            p_ptr->have_ability[i][j] = p_ptr->innate_ability[i][j];
        }
    }

    /*** Extract race/character info ***/

    // Recalculate total weight
    p_ptr->total_weight = 0;
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        p_ptr->total_weight += o_ptr->number * o_ptr->weight;

        // *all* carried objects still cause danger
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & (TR2_DANGER))
            p_ptr->danger += 1;
    }
    p_ptr->total_weight += supplies_total_weight();
    p_ptr->total_weight += player_lamp_oil_weight();

    /*** Analyze equipment ***/

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        bool is_quiver1 = (i == INVEN_QUIVER1);
        bool is_quiver2 = (i == INVEN_QUIVER2);
        bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, f3);
        bool is_arrow = (o_ptr->tval == TV_ARROW);

        bool throwing_quiver = is_quiver2 && is_throwing_item;

        if (is_quiver1)
            continue;
        if (is_quiver2 && !is_throwing_item && !is_arrow)
            continue;

        bool combat_bonuses_active =
            player_weapon_slot_combat_bonuses_active(i, o_ptr);

        /* Affect stats */
        if (f1 & (TR1_STR | TR1_NEG_STR))
            p_ptr->stat_equip_mod[A_STR] += o_ptr->stat_bonus[A_STR];
        if (f1 & (TR1_DEX | TR1_NEG_DEX))
            p_ptr->stat_equip_mod[A_DEX] += o_ptr->stat_bonus[A_DEX];
        if (f1 & (TR1_CON | TR1_NEG_CON))
            p_ptr->stat_equip_mod[A_CON] += o_ptr->stat_bonus[A_CON];
        if (f1 & (TR1_GRA | TR1_NEG_GRA))
            p_ptr->stat_equip_mod[A_GRA] += o_ptr->stat_bonus[A_GRA];

        /* Affect skills */
        if ((f1 & (TR1_MEL)) && combat_bonuses_active)
            p_ptr->skill_equip_mod[S_MEL] += o_ptr->skill_bonus[S_MEL];
        if ((f1 & (TR1_ARC)) && combat_bonuses_active)
            p_ptr->skill_equip_mod[S_ARC] += o_ptr->skill_bonus[S_ARC];
        if (f1 & (TR1_STL))
            p_ptr->skill_equip_mod[S_STL] += o_ptr->skill_bonus[S_STL];
        if (f1 & (TR1_PER))
            p_ptr->skill_equip_mod[S_PER] += o_ptr->skill_bonus[S_PER];
        if (f1 & (TR1_WIL))
            p_ptr->skill_equip_mod[S_WIL] += o_ptr->skill_bonus[S_WIL];
        if (f1 & (TR1_SMT))
            p_ptr->skill_equip_mod[S_SMT] += o_ptr->skill_bonus[S_SMT];
        if (f1 & (TR1_SNG))
            p_ptr->skill_equip_mod[S_SNG] += o_ptr->skill_bonus[S_SNG];

        /* Affect Damage Sides */
        if ((f1 & (TR1_DAMAGE_SIDES)) && combat_bonuses_active)
        {
            p_ptr->to_mds += o_ptr->pval;
            p_ptr->to_ads += o_ptr->pval;
        }

        /* Good flags */
        if (f2 & (TR2_SLOW_DIGEST))
            p_ptr->hunger -= 1;
        if (f2 & (TR2_REGEN))
            p_ptr->regenerate += 1;

        if (f2 & (TR2_SEE_INVIS))
        {
            (void)set_blind(0);
            p_ptr->see_inv += 1;
        }
        if (f2 & (TR2_FREE_ACT))
            p_ptr->free_act += 1;
        if (f2 & (TR2_SPEED))
        {
            p_ptr->pspeed += 1;
        }

        if (f3 & (TR3_STAND_FAST))
            p_ptr->stand_fast += 1;

        if (f3 & (TR3_AVOID_TRAPS))
            p_ptr->avoid_traps += 1;

        /* Bad flags */
        if (f2 & (TR2_HUNGER))
            p_ptr->hunger += 1;
        if (f2 & (TR2_SLOWNESS))
            p_ptr->pspeed -= 1;
        if (f2 & (TR2_AGGRAVATE))
            p_ptr->aggravate += 1;
        if (f2 & (TR2_FEAR))
            p_ptr->cowardice += 1;
        if (f2 & (TR2_HAUNTED))
            p_ptr->haunted += 1;

        // danger has already been handled in the general inventory
        // if (f2 & (TR2_DANGER)) p_ptr->danger += 1;

        // darkness and light are handled later...

        /* Resistance flags */
        if (f2 & (TR2_RES_COLD))
            p_ptr->resist_cold += 1;
        if (f2 & (TR2_RES_FIRE))
            p_ptr->resist_fire += 1;
        if (f2 & (TR2_RES_POIS))
            p_ptr->resist_pois += 1;

        if (f2 & (TR2_VUL_COLD))
            p_ptr->resist_cold -= 1;
        if (f2 & (TR2_VUL_FIRE))
            p_ptr->resist_fire -= 1;
        if (f2 & (TR2_VUL_POIS))
            p_ptr->resist_pois -= 1;

        if (f2 & (TR2_RES_BLEED))
            p_ptr->resist_bleed += 1;

        if (f2 & (TR2_RES_FEAR))
            p_ptr->resist_fear += 1;
        if (f2 & (TR2_RES_BLIND))
            p_ptr->resist_blind += 1;
        if (f2 & (TR2_RES_CONFU))
            p_ptr->resist_confu += 1;
        if (f2 & (TR2_RES_STUN))
            p_ptr->resist_stun += 1;
        if (f2 & (TR2_RES_HALLU))
            p_ptr->resist_hallu += 1;

        /* Sustain flags */
        if (f2 & (TR2_SUST_STR))
            p_ptr->sustain_str += 1;
        if (f2 & (TR2_SUST_DEX))
            p_ptr->sustain_dex += 1;
        if (f2 & (TR2_SUST_CON))
            p_ptr->sustain_con += 1;
        if (f2 & (TR2_SUST_GRA))
            p_ptr->sustain_gra += 1;

        // Parrying grants extra bonus for weapon evasion:
        if (player_active_weapon_is_melee()
            && p_ptr->active_ability[S_EVN][EVN_PARRY] && (i == INVEN_WIELD))
        {
            p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;
        }

        /* Add up the armour weight */
        if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            armour_weight += o_ptr->weight;

        // add the abilities
        int ability_count = o_ptr->abilities;
        for (j = 0; j < ability_count; j++)
        {
            p_ptr->have_ability[o_ptr->skilltype[j]][o_ptr->abilitynum[j]]
                = true;
        }

        /* Hack -- do not apply "melee" to-hit bonuses yet */
        if (i == INVEN_WIELD)
            continue;

        /* Hack -- do not apply "melee" to-hit bonuses yet */
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;

        /* Hack -- do not apply "bow" to-hit bonuses yet */
        if (i == INVEN_BOW)
            continue;

        /* Hack -- do not apply "arrow" to-hit bonuses at all */
        if (i == INVEN_QUIVER1)
            continue;
        if ((i == INVEN_QUIVER2) && !throwing_quiver)
            continue;

        if (!combat_bonuses_active)
            continue;

        /* Apply the bonus to hit */
        p_ptr->skill_equip_mod[S_MEL] += o_ptr->att;
        p_ptr->skill_equip_mod[S_ARC] += o_ptr->att;

        /* Apply the evasion bonus */
        p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;

        if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR]
            && heavy_armour_evasion_bonus_applies(o_ptr))
        {
            p_ptr->skill_equip_mod[S_EVN] += 1;
        }
    }

    /* Clear the old item granted abilities */
    for (i = 0; i < S_MAX; i++)
    {
        /* Skip special abilities - they persist once granted */
        if (i == S_SPC) continue;

        for (j = 0; j < ABILITIES_MAX; j++)
        {
            if (!p_ptr->have_ability[i][j])
            {
                p_ptr->active_ability[i][j] = false;
            }
        }
    }

    /*** Most abilities ***/

    if (p_ptr->active_ability[S_MEL][MEL_STR])
        p_ptr->stat_misc_mod[A_STR]++;
    if (p_ptr->active_ability[S_ARC][ARC_DEX])
        p_ptr->stat_misc_mod[A_DEX]++;
    if (p_ptr->active_ability[S_EVN][EVN_DEX])
        p_ptr->stat_misc_mod[A_DEX]++;
    if (p_ptr->active_ability[S_STL][STL_DEX])
        p_ptr->stat_misc_mod[A_DEX]++;
    if (p_ptr->active_ability[S_PER][PER_GRA])
        p_ptr->stat_misc_mod[A_GRA]++;
    if (p_ptr->active_ability[S_WIL][WIL_CON])
        p_ptr->stat_misc_mod[A_CON]++;
    if (p_ptr->active_ability[S_SMT][SMT_GRA])
        p_ptr->stat_misc_mod[A_GRA]++;
    if (p_ptr->active_ability[S_SNG][SNG_GRA])
        p_ptr->stat_misc_mod[A_GRA]++;

    if (singing(SNG_ELVENESS))
        p_ptr->stat_misc_mod[A_GRA]++;

    if (p_ptr->active_ability[S_WIL][WIL_STRENGTH_IN_ADVERSITY])
    {
        // if <= 50% health, give a bonus to strength and grace
        if (health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_BADLY_WOUNDED)
        {
            p_ptr->stat_misc_mod[A_STR]++;
            p_ptr->stat_misc_mod[A_DEX]++;
            p_ptr->stat_misc_mod[A_GRA]++;
        }

        // if <= 25% health, give an extra bonus
        if (health_level(p_ptr->chp, p_ptr->mhp) <= HEALTH_ALMOST_DEAD)
        {
            p_ptr->stat_misc_mod[A_STR] += 2;
            p_ptr->stat_misc_mod[A_DEX] += 2;
            p_ptr->stat_misc_mod[A_GRA] += 2;
        }
    }

    /* Oath of Light: wearing light-dimming gear immediately breaks the vow */
    if (p_ptr->oath_type == OATH_LIGHT && !oath_invalid(OATH_LIGHT))
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            object_type* o_ptr = &inventory[i];
            if (!o_ptr->k_idx) continue;

            u32b f1, f2, f3, f4;
            object_flags4(o_ptr, &f1, &f2, &f3, &f4);
            if ((f2 & TR2_DARKNESS) || (f4 & TR4_UNLIGHT))
            {
                p_ptr->oaths_broken |= OATH_LIGHT_FLAG;
                p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
                apply_oath_breaking_curse(OATH_LIGHT);
                break;
            }
        }
    }

    /* Oath bonuses (granted by special oath abilities, disabled if oath is broken) */
    /* Apply dynamic oath bonuses based on oath.txt data */
    const bool has_oath_negate = player_has_inventory_flag3(TR3_OATH_NEGATE);
    const bool has_oath_boost = player_has_equipped_flag3(TR3_OATH_BOOST);

    /* Only apply oath bonuses if not negated */
    if (!has_oath_negate)
    {
        for (int oath_idx = 0; oath_idx < z_info->oath_max; oath_idx++)
        {
            oath_type *oath_ptr = &oath_info[oath_idx];

            /* Check if player has this oath and it's not broken */
            if (oath_ptr->oath_num >= OATH_MERCY && oath_ptr->oath_num <= OATH_LIGHT)
            {
                int special_ability = oath_special_ability_from_oath_num(oath_ptr->oath_num);

                /* Apply bonuses if player has oath and it's not broken */
                if (special_ability >= 0 &&
                    p_ptr->active_ability[S_SPC][special_ability] &&
                    !oath_invalid(oath_ptr->oath_num))
                {
                    int bonus_mult = (has_oath_boost && oath_ptr->oath_num == p_ptr->oath_type) ? 2 : 1;

                    /* Apply stat bonuses */
                    p_ptr->stat_misc_mod[A_STR] += oath_ptr->stat_bonuses[0] * bonus_mult;
                    p_ptr->stat_misc_mod[A_DEX] += oath_ptr->stat_bonuses[1] * bonus_mult;
                    p_ptr->stat_misc_mod[A_CON] += oath_ptr->stat_bonuses[2] * bonus_mult;
                    p_ptr->stat_misc_mod[A_GRA] += oath_ptr->stat_bonuses[3] * bonus_mult;

                    /* Apply skill bonuses */
                    if (oath_ptr->skill_type > 0 && oath_ptr->skill_type < S_MAX)
                    {
                        p_ptr->skill_misc_mod[oath_ptr->skill_type] += oath_ptr->skill_bonus * bonus_mult;
                    }
                }
            }
        }
    }

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        p_ptr->skill_misc_mod[S_MEL] -= 3;
    }

    if (p_ptr->active_ability[S_WIL][WIL_POISON_RESISTANCE])
    {
        p_ptr->resist_pois += 1;
    }

    /*** Temporary flags ***/

    /* Apply temporary "stun" */
    if (p_ptr->stun >= 50)
    {
        for (i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_misc_mod[i] -= 4;
        }
    }
    else if (p_ptr->stun)
    {
        for (i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_misc_mod[i] -= 2;
        }
    }

    /* Temporary "Rage" */
    if (p_ptr->rage)
    {
        p_ptr->stat_misc_mod[A_STR] += 1;
        p_ptr->stat_misc_mod[A_DEX] -= 1;
        p_ptr->stat_misc_mod[A_CON] += 1;
        p_ptr->stat_misc_mod[A_GRA] -= 1;
    }

    /* Temporary Strength */
    if (p_ptr->tmp_str)
    {
        p_ptr->stat_misc_mod[A_STR] += 3;
        p_ptr->sustain_str += 1;
    }

    /* Temporary Dexterity */
    if (p_ptr->tmp_dex)
    {
        p_ptr->stat_misc_mod[A_DEX] += 3;
        p_ptr->sustain_dex += 1;
    }

    /* Temporary Constitution */
    if (p_ptr->tmp_con)
    {
        p_ptr->stat_misc_mod[A_CON] += 3;
        p_ptr->sustain_con += 1;
    }

    /* Temporary Grace */
    if (p_ptr->tmp_gra)
    {
        p_ptr->stat_misc_mod[A_GRA] += 3;
        p_ptr->sustain_gra += 1;
    }

    /* Temporary "fast" */
    if (p_ptr->fast)
    {
        p_ptr->pspeed += 1;
    }

    /* Temporary "slow" */
    if (p_ptr->slow)
    {
        p_ptr->pspeed -= 1;
    }

    /* Temporary see invisible, resist blindness, and resist hallucination */
    if (p_ptr->tim_invis)
    {
        /* Hack */
        p_ptr->see_inv += 1;

        /* Hack */
        p_ptr->resist_blind += 1;

        /* Hack */
        p_ptr->resist_hallu += 1;
    }

    /* Weak with hunger */
    if (p_ptr->food < PY_FOOD_WEAK)
    {
        p_ptr->stat_misc_mod[A_STR] -= 1;
    }

    // 'Indomitable' ability provides resist_fear, resist_confusion,
    // resist_stunning and resist_hallucination as well as slowing hunger
    if (p_ptr->active_ability[S_WIL][WIL_INDOMITABLE])
    {
        p_ptr->resist_confu += 1;
        p_ptr->resist_fear += 1;
        p_ptr->resist_stun += 1;
        p_ptr->resist_hallu += 1;
        p_ptr->hunger -= 1;
    }

    /* Meta-run curses/blessings adjusting resistances */
    {
        int shift;

        shift = curse_flag_delta_cur(CUR_RES_FEAR_SHIFT);
        if (shift) p_ptr->resist_fear -= shift;

        shift = curse_flag_delta_cur(CUR_RES_STUN_SHIFT);
        if (shift) p_ptr->resist_stun -= shift;

        shift = curse_flag_delta_cur(CUR_RES_CONFU_SHIFT);
        if (shift) p_ptr->resist_confu -= shift;

        shift = curse_flag_delta_cur(CUR_RES_HALLU_SHIFT);
        if (shift) p_ptr->resist_hallu -= shift;

        shift = curse_flag_delta_cur(CUR_RES_POIS_SHIFT);
        if (shift) p_ptr->resist_pois -= shift;

        shift = curse_flag_delta_cur(CUR_RES_FIRE_SHIFT);
        if (shift) p_ptr->resist_fire -= shift;

        shift = curse_flag_delta_cur(CUR_RES_COLD_SHIFT);
        if (shift) p_ptr->resist_cold -= shift;
    }

    /* CUR_HUNGER curse/blessing: curse increases hunger, blessing decreases it */
    {
        int h = curse_flag_delta_cur(CUR_HUNGER);
        if (h != 0) p_ptr->hunger += h;
    }

    // Mandos' Doom special ability grants immunity to fear, hallucination,
    // entrancement, rage, stun and confusion (implemented as high resistance + clear)
    if (p_ptr->have_ability[S_SPC][SPC_MANDOS]) {
        p_ptr->resist_fear += 100; // effectively immune
        p_ptr->resist_hallu += 100;
        p_ptr->resist_stun += 100;
        p_ptr->resist_confu += 100; // added confusion immunity
        log_trace("ABILITY DEBUG: Mandos' Doom active - granting mental immunities (fear+100, hallu+100, stun+100, confu+100). Total resist_confu: %d", p_ptr->resist_confu);
        // Clear timed effects each turn
        if (p_ptr->afraid) {
            (void)set_afraid(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared fear effect");
        }
        if (p_ptr->image) {
            p_ptr->image = 0;  // No set_image function found
            p_ptr->redraw |= (PR_MAP);  // Manually trigger redraw for hallucination
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared hallucination effect");
        }
        if (p_ptr->entranced) {
            (void)set_entranced(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared entrancement effect");
        }
        if (p_ptr->rage) {
            (void)set_rage(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared rage effect");
        }
        if (p_ptr->stun) {
            (void)set_stun(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared stun effect");
        }
        if (p_ptr->confused) {
            (void)set_confused(0);
            log_trace("ABILITY DEBUG: Mandos' Doom - cleared confusion effect");
        }
    } else {
        log_trace("ABILITY DEBUG: Mandos' Doom NOT active - have_ability[S_SPC][SPC_MANDOS] = %d", p_ptr->have_ability[S_SPC][SPC_MANDOS]);
    }

    /* Big cave environmental penalties: reduce key resistances while inside. */
    {
        big_cave_type_t cave_type = level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px);
        bool suppressed = (cave_info[p_ptr->py][p_ptr->px]
            & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
        bool should_log = (cave_type != BIG_CAVE_NONE) || suppressed;

        if (should_log)
        {
            log_partition_debug_for_point("calc_bonuses.big_cave", p_ptr->py,
                p_ptr->px);
            log_debug(
                "calc_bonuses.big_cave pre: fire=%d cold=%d pois=%d fear=%d stun=%d oppose_fire=%d oppose_cold=%d oppose_pois=%d",
                p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
                p_ptr->resist_fear, p_ptr->resist_stun, p_ptr->oppose_fire,
                p_ptr->oppose_cold, p_ptr->oppose_pois);
        }

        if (cave_type != BIG_CAVE_NONE)
        {
            p_ptr->resist_fear -= 1;
            p_ptr->resist_stun -= 1;
            if (cave_type == BIG_CAVE_FIRE)
                p_ptr->resist_fire -= 1;
            else if (cave_type == BIG_CAVE_ICE)
                p_ptr->resist_cold -= 1;
            else if (cave_type == BIG_CAVE_POIS)
                p_ptr->resist_pois -= 1;
        }

        if (should_log)
        {
            log_debug(
                "calc_bonuses.big_cave post: fire=%d cold=%d pois=%d fear=%d stun=%d effective_fire=%d effective_cold=%d effective_pois=%d",
                p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
                p_ptr->resist_fear, p_ptr->resist_stun, resist_fire(),
                resist_cold(), resist_pois());
        }
    }

    // Helper function to calculate total monsters seen across all races
    int total_monsters_seen = 0;
    int total_monsters_killed = 0;
    int race_idx;
    for (race_idx = 0; race_idx < z_info->r_max; race_idx++) {
        monster_race *r_ptr = &r_info[race_idx];
        monster_lore *l_ptr = &l_list[race_idx];

        /* Skip non-monsters and unique monsters for mercy calculation */
        if (!r_ptr->name) continue;
        if (r_ptr->flags1 & RF1_UNIQUE) continue;

        total_monsters_seen += l_ptr->psights;
        total_monsters_killed += l_ptr->pkills;
    }

    // Nienna's Gift of Mercy special ability grants enhanced stealth proportional to mercy shown
    if (p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY]) {
        if (total_monsters_seen > 0) {
            /* Calculate stealth bonus: 10*(seen-killed)/seen, rounded up */
            int mercy_ratio_times_10 = (10 * (total_monsters_seen - total_monsters_killed));
            int stealth_bonus = (mercy_ratio_times_10 + total_monsters_seen - 1) / total_monsters_seen; /* Ceiling division */

            if (stealth_bonus > 0) {
                p_ptr->skill_misc_mod[S_STL] += stealth_bonus;
                log_trace("ABILITY DEBUG: Nienna's Gift of Mercy active - granting +%d stealth (global: seen=%d, killed=%d, ratio=%.2f)",
                         stealth_bonus, total_monsters_seen, total_monsters_killed,
                         (float)(total_monsters_seen - total_monsters_killed) / total_monsters_seen);
            } else {
                log_trace("ABILITY DEBUG: Nienna's Gift of Mercy active but no bonus (global: seen=%d, killed=%d)",
                         total_monsters_seen, total_monsters_killed);
            }
        }
    }

    /* Apply full-set bonuses from equipped item sets. */
    item_sets_apply_player_bonuses();

    /*** Handle stats ***/
    calc_stats();

    /*** Analyze weight ***/

    /* Extract the current weight (in tenth pounds) */
    j = p_ptr->total_weight;

    /* Extract the "weight limit" (in tenth pounds) */
    i = weight_limit();

    /* Apply "encumbrance" from weight */
    if (j > i)
        p_ptr->pspeed -= 1;

    /* Stealth slows the player down (unless they are passing) */
    if (p_ptr->stealth_mode)
    {
        if (p_ptr->previous_action[0] != 5)
            p_ptr->pspeed -= 1;
        p_ptr->skill_misc_mod[S_STL] += STEALTH_MODE_BONUS;
    }

    if (p_ptr->rage)
    {
        p_ptr->skill_misc_mod[S_STL] -= 3;
    }

    /* Speed must lie between 1 and 4 */
    if (p_ptr->pspeed < 1)
        p_ptr->pspeed = 1;
    else if (p_ptr->pspeed > 4)
        p_ptr->pspeed = 4;

    /* Sprinting bonus: only applies if speed < 3, so it caps at 3 */
    if (sprinting())
    {
        if (p_ptr->pspeed < 3)
        {
            p_ptr->pspeed += 1;
        }
    }

    // Increase food consumption if actively regenerating
    if (p_ptr->regenerate
        && (p_ptr->chp < p_ptr->mhp || p_ptr->csp < p_ptr->msp))
    {
        p_ptr->hunger += 1;
    }

    /* armour weight (not inventory weight reduces stealth */
    /* by 1 point per 10 pounds (rounding down) */
    p_ptr->skill_equip_mod[S_STL] -= armour_weight / 100;

    // Penalise stealth based on song(s) being sung
    if (p_ptr->song1 != SNG_NOTHING)
    {
        int song_noise = 0;
        int song;

        for (i = 0; i < 2; i++)
        {
            if (i == 0)
                song = p_ptr->song1;
            else
                song = p_ptr->song2;

            switch (song)
            {
            case SNG_NOTHING:
                song_noise += 0;
                break;
            case SNG_ELBERETH:
                song_noise += 8;
                break;
            case SNG_CHALLENGE:
                song_noise += 12;
                break;
            case SNG_DELVINGS:
                song_noise += 4;
                break;
            case SNG_FREEDOM:
                song_noise += 4;
                break;
            case SNG_SILENCE:
                song_noise += 0;
                break;
            case SNG_STAUNCHING:
                song_noise += 4;
                break;
            case SNG_TREES:
                song_noise += 4;
                break;
            case SNG_ELVENESS:
                song_noise += 6;
                break;
            case SNG_DISGUISE:
                song_noise += 6;
                break;
            case SNG_THRESHOLDS:
                song_noise += 4;
                break;
            case SNG_STAYING:
                song_noise += 8;
                break;
            case SNG_SLAYING:
                song_noise += 8;
                break;
            case SNG_LORIEN:
                song_noise += 4;
                break;
            case SNG_MASTERY:
                song_noise += 8;
                break;
            }
        }

        // average the noise if there are two songs
        if (p_ptr->song2 != SNG_NOTHING)
            song_noise /= 2;

        p_ptr->skill_misc_mod[S_STL] -= song_noise;
    }

    /* Race/Character skill flags */
    p_ptr->skill_misc_mod[S_MEL] += affinity_level(S_MEL);
    p_ptr->skill_misc_mod[S_ARC] += affinity_level(S_ARC);
    p_ptr->skill_misc_mod[S_EVN] += affinity_level(S_EVN);
    p_ptr->skill_misc_mod[S_STL] += affinity_level(S_STL);
    p_ptr->skill_misc_mod[S_PER] += affinity_level(S_PER);
    p_ptr->skill_misc_mod[S_WIL] += affinity_level(S_WIL);
    p_ptr->skill_misc_mod[S_SMT] += affinity_level(S_SMT);
    p_ptr->skill_misc_mod[S_SNG] += affinity_level(S_SNG);

    /*** Modify skills by ability scores ***/

    /* Affect Skill -- melee (DEX) */
    p_ptr->skill_stat_mod[S_MEL] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- archery (DEX) */
    p_ptr->skill_stat_mod[S_ARC] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- evasion (DEX) */
    p_ptr->skill_stat_mod[S_EVN] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- stealth (DEX) */
    p_ptr->skill_stat_mod[S_STL] = p_ptr->stat_use[A_DEX];

    /* Affect Skill -- perception (GRA) */
    p_ptr->skill_stat_mod[S_PER] = p_ptr->stat_use[A_GRA];

    /* Affect Skill -- will (GRA) */
    p_ptr->skill_stat_mod[S_WIL] = p_ptr->stat_use[A_GRA];

    /* Affect Skill -- smithing (GRA) */
    p_ptr->skill_stat_mod[S_SMT] = p_ptr->stat_use[A_GRA];

    /* Affect Skill -- song (GRA) */
    p_ptr->skill_stat_mod[S_SNG] = p_ptr->stat_use[A_GRA];

    // Finalise song first as it modifies some other skills...
    p_ptr->skill_use[S_SNG] = p_ptr->skill_base[S_SNG]
        + p_ptr->skill_equip_mod[S_SNG] + p_ptr->skill_stat_mod[S_SNG]
        + p_ptr->skill_misc_mod[S_SNG];

    // Apply song effects that modify skills
    if (singing(SNG_ELVENESS))
        p_ptr->skill_misc_mod[S_EVN] += ability_bonus(S_SNG, SNG_ELVENESS);
    if (singing(SNG_STAYING))
    {
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) p_ptr->skill_misc_mod[S_WIL] += ability_bonus(S_SNG, SNG_STAYING);
        else p_ptr->skill_misc_mod[S_WIL] += ability_bonus(S_SNG, SNG_STAYING) / 2;
    }
    if (singing(SNG_FREEDOM))
    {
        p_ptr->free_act += 1;
    }

    if (p_ptr->tmp_per)
    {
        p_ptr->skill_misc_mod[S_PER] += 10;
    }

    /*** Finalise all skills other than combat skills  (as bows/weapons must be
     * analysed first) ***/

    p_ptr->skill_use[S_STL] = p_ptr->skill_base[S_STL]
        + p_ptr->skill_equip_mod[S_STL] + p_ptr->skill_stat_mod[S_STL]
        + p_ptr->skill_misc_mod[S_STL];
    p_ptr->skill_use[S_PER] = p_ptr->skill_base[S_PER]
        + p_ptr->skill_equip_mod[S_PER] + p_ptr->skill_stat_mod[S_PER]
        + p_ptr->skill_misc_mod[S_PER];
    p_ptr->skill_use[S_WIL] = p_ptr->skill_base[S_WIL]
        + p_ptr->skill_equip_mod[S_WIL] + p_ptr->skill_stat_mod[S_WIL]
        + p_ptr->skill_misc_mod[S_WIL];
    p_ptr->skill_use[S_SMT] = p_ptr->skill_base[S_SMT]
        + p_ptr->skill_equip_mod[S_SMT] + p_ptr->skill_stat_mod[S_SMT]
        + p_ptr->skill_misc_mod[S_SMT];

    /*** Analyze current bow ***/

    /* Examine the "current bow" */
    o_ptr = &inventory[INVEN_BOW];

    if (player_active_weapon_is_ranged())
    {
        p_ptr->skill_equip_mod[S_ARC] += o_ptr->att;

        /* Analyze launcher */
        // attack bonuses for those with bow proficiency
        p_ptr->skill_misc_mod[S_ARC] += bow_bonus(&inventory[INVEN_BOW]);

        /* Warden is the melee mirror of Versatility. */
        p_ptr->skill_misc_mod[S_ARC] +=
            ability_current_skill_bonus(S_MEL, MEL_WARDEN);

        if (o_ptr->k_idx)
        {
            p_ptr->ammo_tval = TV_ARROW;

            p_ptr->add = o_ptr->dd;
            p_ptr->ads = total_ads(o_ptr);

            /* set the archery skill (if using a bow) -- it gets set again later,
             * anyway
             */
            p_ptr->skill_use[S_ARC] = p_ptr->skill_base[S_ARC]
                + p_ptr->skill_equip_mod[S_ARC] + p_ptr->skill_stat_mod[S_ARC]
                + p_ptr->skill_misc_mod[S_ARC];
        }
    }

    /*** Analyze melee weapon ***/

    /* Examine the "current melee weapon" */
    o_ptr = &inventory[INVEN_WIELD];

    if (player_active_weapon_is_melee())
    {
        // add the weapon's attack mod
        p_ptr->skill_equip_mod[S_MEL] += o_ptr->att;

        // add the weapon's evasion bonus (Parry ability grants this as extra bonus earlier)
        p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;

        // attack bonuses for matched weapon types
        p_ptr->skill_misc_mod[S_MEL] += axe_bonus(o_ptr) + polearm_bonus(o_ptr);

        p_ptr->skill_misc_mod[S_MEL] +=
            ability_current_skill_bonus(S_ARC, ARC_VERSATILITY);

        /* generate the melee dice/sides from weapon, to_mdd, to_mds and strength */
        p_ptr->mdd = total_mdd(o_ptr);
        p_ptr->mds = total_mds(
            o_ptr, p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] ? -3 : 0);

        // determine the off-hand melee score, damage and sides
        // Check if we have paired weapons (e.g., Glamdring + Orcrist)
        bool paired_offhand = false;
        if (inventory[INVEN_WIELD].name1 && inventory[INVEN_ARM].name1)
        {
            int paired_idx = get_paired_artefact(inventory[INVEN_WIELD].name1);
            if (paired_idx == inventory[INVEN_ARM].name1)
            {
                paired_offhand = true;
            }
        }

        if (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]
            && (((&inventory[INVEN_ARM])->tval != TV_SHIELD)
                && ((&inventory[INVEN_ARM])->tval != 0)))
        {
            // remove main-hand specific bonuses
            p_ptr->offhand_mel_mod
                -= o_ptr->att + axe_bonus(o_ptr) + polearm_bonus(o_ptr);
            if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
                p_ptr->offhand_mel_mod += 3;

            // add off-hand specific bonuses
            o_ptr = &inventory[INVEN_ARM];
            // Paired weapons have no off-hand attack penalty
            int offhand_penalty = paired_offhand ? 0 : 3;
            p_ptr->offhand_mel_mod
                += o_ptr->att + axe_bonus(o_ptr) + polearm_bonus(o_ptr) - offhand_penalty;

            // add off-hand weapon's evasion bonus
            p_ptr->skill_equip_mod[S_EVN] += o_ptr->evn;

            p_ptr->mdd2 = total_mdd(o_ptr);
            // Paired weapons have no strength adjustment penalty
            p_ptr->mds2 = total_mds(o_ptr, paired_offhand ? 0 : -3);
        }
    }

    /* Meta-run curse adjusting melee damage sides */
    {
        int shift = curse_flag_delta_cur(CUR_MDS_SHIFT);
        if (shift != 0) {
            if (p_ptr->mds > 0) {
                int adjusted = p_ptr->mds - shift;
                if (adjusted < 1) adjusted = 1;
                p_ptr->mds = adjusted;
            }
            if (p_ptr->mds2 > 0) {
                int adjusted2 = p_ptr->mds2 - shift;
                if (adjusted2 < 1) adjusted2 = 1;
                p_ptr->mds2 = adjusted2;
            }
        }
    }

    /* Entrancement or being knocked out sets total evasion score to -5 */
    if (p_ptr->entranced || (p_ptr->stun > 100))
    {
        p_ptr->skill_misc_mod[S_EVN] = -5
            - (p_ptr->skill_base[S_EVN] + p_ptr->skill_equip_mod[S_EVN]
                + p_ptr->skill_stat_mod[S_EVN]);
    }

    /* finalise the combat and evasion skills */

    p_ptr->skill_use[S_MEL] = p_ptr->skill_base[S_MEL]
        + p_ptr->skill_equip_mod[S_MEL] + p_ptr->skill_stat_mod[S_MEL]
        + p_ptr->skill_misc_mod[S_MEL];
    p_ptr->skill_use[S_ARC] = p_ptr->skill_base[S_ARC]
        + p_ptr->skill_equip_mod[S_ARC] + p_ptr->skill_stat_mod[S_ARC]
        + p_ptr->skill_misc_mod[S_ARC];
    p_ptr->skill_use[S_EVN] = p_ptr->skill_base[S_EVN]
        + p_ptr->skill_equip_mod[S_EVN] + p_ptr->skill_stat_mod[S_EVN]
        + p_ptr->skill_misc_mod[S_EVN];

    /* Blows (melee attacks per round) and digging power */
    if (o_ptr->k_idx)
    {
        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);
    }

    /*** Notice changes ***/

    /* Analyze stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Notice changes */
        if (p_ptr->stat_drain[i] != old_stat_tmp_mod[i])
        {
            /* Redisplay the stats later */
            p_ptr->redraw |= (PR_STATS);

            /* Window stuff */
            p_ptr->window |= (PW_PLAYER_0);
        }

        /* Notice changes */
        if (p_ptr->stat_use[i] != old_stat_use[i])
        {
            /* Redisplay the stats later */
            p_ptr->redraw |= (PR_STATS);

            /* Window stuff */
            p_ptr->window |= (PW_PLAYER_0);
            /* Change in CON affects Hitpoints */
            if (i == A_CON)
            {
                p_ptr->update |= (PU_HP);
            }
        }
    }

    /* Recalculate voice needed */
    if (p_ptr->stat_use[A_GRA] != old_stat_use[A_GRA])
    {
        p_ptr->update |= (PU_MANA);
    }

    /* Hack -- Telepathy Change */
    if (p_ptr->telepathy != old_telepathy)
    {
        /* Update monster visibility */
        p_ptr->update |= (PU_MONSTERS);
    }

    /* Hack -- See Invis Change */
    if (p_ptr->see_inv != old_see_inv)
    {
        /* Update monster visibility */
        p_ptr->update |= (PU_MONSTERS);
    }

    /* Redraw speed (if needed) */
    if (p_ptr->pspeed != old_speed)
    {
        /* Redraw speed */
        p_ptr->redraw |= (PR_SPEED);
    }

    /* Always redraw terrain */
    p_ptr->redraw |= (PR_TERRAIN);

    /* Redraw melee (if needed) */
    if ((p_ptr->skill_use[S_MEL] != old_skill_use[S_MEL])
        || (p_ptr->mdd != old_mdd) || (p_ptr->mds != old_mds)
        || (p_ptr->mdd2 != old_mdd2) || (p_ptr->mds2 != old_mds2))
    {
        /* Redraw */
        p_ptr->redraw |= (PR_MEL);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Redraw archery (if needed) */
    if ((p_ptr->skill_use[S_ARC] != old_skill_use[S_ARC])
        || (p_ptr->add != old_add) || (p_ptr->ads != old_ads))
    {
        /* Redraw */
        p_ptr->redraw |= (PR_ARC);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Redraw armor */
    if ((p_ptr->skill_use[S_EVN] != old_skill_use[S_EVN])
        || (p_ptr->old_p_min != new_p_min) || (p_ptr->old_p_max != new_p_max))
    {
        /* Redraw */
        p_ptr->redraw |= (PR_ARMOR);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);

        p_ptr->old_p_min = new_p_min;
        p_ptr->old_p_max = new_p_max;
    }

    if (c_info[p_ptr->pcharacter].flags & RHF_MOR_CURSE) p_ptr->danger += 1;

    /* Hack -- handle "xtra" mode */
    if (character_xtra)
        return;

    // identify {special} items when the type has been seen before
    id_known_specials();
    reorder_pack(false);
}
