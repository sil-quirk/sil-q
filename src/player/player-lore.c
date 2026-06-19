#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"

/*
 * Handle "p_ptr->notice"
 */
void notice_stuff(void)
{
    /* Notice stuff */
    if (!p_ptr->notice)
        return;

    /* Combine the pack */
    if (p_ptr->notice & (PN_COMBINE))
    {
        p_ptr->notice &= ~(PN_COMBINE);
        combine_pack();
    }

    /* Reorder the pack */
    if (p_ptr->notice & (PN_REORDER))
    {
        p_ptr->notice &= ~(PN_REORDER);
        reorder_pack(true);
    }

    if (p_ptr->notice & PN_AUTOINSCRIBE)
    {
        p_ptr->notice &= ~(PN_AUTOINSCRIBE);
        autoinscribe_pack();
        autoinscribe_ground();
    }
}

bool player_auto_identifies_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    /*
     * Smithing-difficulty items use the new identification rules and are never
     * auto-identified by category abilities (Enchantment/Jeweller/etc.).
     */
    if (object_uses_smithing_difficulty(o_ptr))
        return false;

    bool alchemy = p_ptr->active_ability[S_PER][PER_ALCHEMY]
        || p_ptr->have_ability[S_PER][PER_ALCHEMY];
    bool channeling = p_ptr->active_ability[S_WIL][WIL_CHANNELING]
        || p_ptr->have_ability[S_WIL][WIL_CHANNELING];
    bool jeweller = p_ptr->active_ability[S_SMT][SMT_JEWELLER]
        || p_ptr->have_ability[S_SMT][SMT_JEWELLER];
    bool enchantment = p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]
        || p_ptr->have_ability[S_SMT][SMT_ENCHANTMENT];

    bool is_potion = (o_ptr->tval == TV_POTION);
    bool is_herb = (o_ptr->tval == TV_FOOD) && (o_ptr->sval <= SV_FOOD_SICKNESS);
    bool is_gem = (o_ptr->tval == TV_GEM);
    bool is_staff = (o_ptr->tval == TV_STAFF);
    bool is_horn = (o_ptr->tval == TV_HORN);
    bool is_jewellery = (o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET)
        || (o_ptr->tval == TV_LIGHT);

    if (alchemy && (is_potion || is_herb || is_gem))
        return true;

    if (channeling && (is_staff || is_horn))
        return true;

    if (jeweller && is_jewellery)
        return true;

    if (enchantment && !(is_potion || is_herb || is_gem))
        return true;

    return false;
}

static bool player_has_ability_bonus(int skilltype, int abilitynum)
{
    if (skilltype < 0 || skilltype >= S_MAX)
        return false;
    if (abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        return false;

    return p_ptr->active_ability[skilltype][abilitynum]
        || p_ptr->have_ability[skilltype][abilitynum];
}

typedef enum
{
    SMITH_ID_CAT_WEAPON = 0,
    SMITH_ID_CAT_ARMOUR = 1,
    SMITH_ID_CAT_JEWELLERY = 2,
    SMITH_ID_CAT_OTHER = 3
} smith_id_category;

static smith_id_category smith_id_category_for_object(const object_type* o_ptr)
{
    if (!o_ptr)
        return SMITH_ID_CAT_OTHER;

    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
        return SMITH_ID_CAT_WEAPON;

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
        return SMITH_ID_CAT_ARMOUR;

    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
        return SMITH_ID_CAT_JEWELLERY;

    default:
        return SMITH_ID_CAT_OTHER;
    }
}

static int smithing_ident_distance_penalty(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;

    int dist = distance(p_ptr->py, p_ptr->px, o_ptr->iy, o_ptr->ix);
    int penalty = dist / 2;
    if (penalty > 10)
        penalty = 10;
    if (penalty < 0)
        penalty = 0;

    log_trace(
        "smithing-ident: distance penalty dist=%d penalty=%d player=(%d,%d) obj=(%d,%d)",
        dist, penalty, p_ptr->py, p_ptr->px, o_ptr->iy, o_ptr->ix);

    return penalty;
}

static bool song_revealing_ident_bonus_applies(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (!singing(SNG_REVEALING))
        return false;

    if ((o_ptr >= inventory) && (o_ptr < inventory + INVEN_TOTAL))
        return true;

    if ((o_ptr >= o_list) && (o_ptr < o_list + o_max))
    {
        if (!o_ptr->marked)
            return false;
        if (o_ptr->held_m_idx)
            return false;

        int song_score = ability_bonus(S_SNG, SNG_REVEALING);
        int range = (song_score / 2) + 8;
        int dist = flow_dist(FLOW_PLAYER_NOISE, o_ptr->iy, o_ptr->ix);

        return (song_score > 0) && (dist < FLOW_MAX_DIST) && (dist <= range);
    }

    return false;
}

static int player_smithing_identify_skill(const object_type* o_ptr,
    bool is_equipped, bool apply_distance_penalty, bool ignore_distance_penalty,
    int bonus)
{
    int grace_bonus = p_ptr->stat_use[A_GRA];
    int base_per = p_ptr->skill_use[S_PER] - p_ptr->skill_stat_mod[S_PER];

    /* Resonance doubles the Perception portion only; Grace is added once below. */
    if (player_has_ability_bonus(S_PER, PER_LISTEN))
    {
        base_per *= 2;
    }

    int base_smt = p_ptr->skill_use[S_SMT] - p_ptr->skill_stat_mod[S_SMT];
    /* Basis for identification skill checks: start at -3 */
    int basis = -3;
    int skill = base_per + base_smt + grace_bonus + basis;

    int bonus_enchantment = player_has_ability_bonus(S_SMT, SMT_ENCHANTMENT) ? 5 : 0;
    int bonus_artifice = player_has_ability_bonus(S_SMT, SMT_ARTEFACT) ? 7 : 0;
    int bonus_curse_breaking = player_has_ability_bonus(S_WIL, WIL_CURSE_BREAKING) ? 7 : 0;
    int bonus_quick_study = player_has_ability_bonus(S_PER, PER_QUICK_STUDY) ? 5 : 0;

    int category_bonus = 0;
    smith_id_category cat = smith_id_category_for_object(o_ptr);
    if (cat == SMITH_ID_CAT_WEAPON && player_has_ability_bonus(S_SMT, SMT_WEAPONSMITH))
        category_bonus = 5;
    if (cat == SMITH_ID_CAT_ARMOUR && player_has_ability_bonus(S_SMT, SMT_ARMOURSMITH))
        category_bonus = 5;
    if (cat == SMITH_ID_CAT_JEWELLERY && player_has_ability_bonus(S_SMT, SMT_JEWELLER))
        category_bonus = 5;

    int bonus_equipped = is_equipped ? 3 : 0;
    int bonus_experienced = (o_ptr && (o_ptr->ident & IDENT_EXPERIENCED)) ? 5 : 0;
    int bonus_known_ego = 0;
    int bonus_revealing = song_revealing_ident_bonus_applies(o_ptr)
        ? damroll(1, 5)
        : 0;
    if (o_ptr)
    {
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);
        if (ego_pfx && !e_info[ego_pfx].aware)
            bonus_known_ego -= 5;
        if (ego_sfx && !e_info[ego_sfx].aware)
            bonus_known_ego -= 5;
    }
    int distance_penalty = 0;

    /* EASY_ID/DIF_ID flags affect identification skill */
    int bonus_easy_id = 0;
    if (o_ptr)
    {
        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & TR3_EASY_ID)
            bonus_easy_id = 7;
        else if (f3 & TR3_DIF_ID)
            bonus_easy_id = -7;
    }

    /* CUR_IDENT_DIFF: curse increases identification difficulty, blessing decreases it */
    int ident_diff_delta = curse_flag_delta_cur(CUR_IDENT_DIFF);
    int curse_ident_diff_penalty = ident_diff_delta * -7;

    /* Cursed items impose an identification penalty unless the player has Curse Breaking */
    int curse_penalty = 0;
    bool has_curse_breaking = player_has_ability_bonus(S_WIL, WIL_CURSE_BREAKING) ? true : false;
    if (o_ptr && cursed_p(o_ptr) && !has_curse_breaking)
    {
        curse_penalty = -5;
        skill += curse_penalty;
    }

    /* Ability bonuses */
    skill += bonus_enchantment;
    skill += bonus_artifice;
    skill += bonus_curse_breaking;
    skill += bonus_quick_study;
    if (current_character_profile && (current_character_profile->flags & RHF_KHELED_ZARAM))
        skill += 30;

    /* Category bonuses */
    skill += category_bonus;

    /* Context bonuses */
    skill += bonus_equipped;
    skill += bonus_experienced;
    skill += bonus_known_ego;
    skill += bonus_revealing;

    /* Item identification flags */
    skill += bonus_easy_id;

    /* Curse-based identification difficulty shift */
    skill += curse_ident_diff_penalty;

    skill += bonus;

    if (apply_distance_penalty)
    {
        distance_penalty = smithing_ident_distance_penalty(o_ptr);
        if (!ignore_distance_penalty)
            skill -= distance_penalty;
    }

    log_trace(
        "smithing-ident: skill calc k_idx=%d tval=%d sval=%d name1=%d ego_pfx=%d ego_sfx=%d ident=0x%08X base(per_no_gra=%d smt_no_gra=%d gra=%d) abil(enchant=%d artifice=%d cursebreak=%d quick=%d) cat=%d cat_bonus=%d ctx(equip=%d exp=%d ego=%d revealing=%d) bonus=%d dist(apply=%d ignore=%d pen=%d curse_penalty=%d ident_diff=%d) => skill=%d",
        o_ptr ? o_ptr->k_idx : 0,
        o_ptr ? o_ptr->tval : 0,
        o_ptr ? o_ptr->sval : 0,
        o_ptr ? o_ptr->name1 : 0,
        o_ptr ? object_ego_prefix(o_ptr) : 0,
        o_ptr ? object_ego_suffix(o_ptr) : 0,
        (unsigned)(o_ptr ? o_ptr->ident : 0),
        base_per, base_smt, grace_bonus,
        bonus_enchantment, bonus_artifice, bonus_curse_breaking, bonus_quick_study,
        (int)cat, category_bonus,
        bonus_equipped, bonus_experienced, bonus_known_ego,
        bonus_revealing,
        bonus,
        apply_distance_penalty ? 1 : 0, ignore_distance_penalty ? 1 : 0, distance_penalty, curse_penalty,
        curse_ident_diff_penalty,
        skill);

    return skill;
}

void player_mark_object_experienced(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return;

    if (o_ptr->ident & IDENT_EXPERIENCED)
    {
        /* Ensure legacy/edge cases still keep floor combat stats visible. */
        o_ptr->ident |= IDENT_HANDLED;
        return;
    }

    log_trace(
        "smithing-ident: mark experienced k_idx=%d tval=%d sval=%d name1=%d ego_pfx=%d ego_sfx=%d ident=0x%08X",
        o_ptr->k_idx, o_ptr->tval, o_ptr->sval, o_ptr->name1,
        object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
        (unsigned)o_ptr->ident);

    o_ptr->ident |= IDENT_HANDLED;
    o_ptr->ident |= IDENT_EXPERIENCED;
}

bool player_try_identify_smithing_object(
    object_type* o_ptr, bool is_equipped, int bonus)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (!object_uses_smithing_difficulty(o_ptr))
        return false;
    if (object_known_p(o_ptr))
        return false;

    int skill = player_smithing_identify_skill(
        o_ptr, is_equipped, false, false, bonus);
    int difficulty = object_smithing_difficulty(o_ptr);
    bool success = (skill >= difficulty);

    log_trace(
        "smithing-ident: fixed check k_idx=%d tval=%d sval=%d name1=%d ego_pfx=%d ego_sfx=%d is_equipped=%d bonus=%d skill=%d difficulty=%d success=%d",
        o_ptr->k_idx, o_ptr->tval, o_ptr->sval, o_ptr->name1,
        object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
        is_equipped ? 1 : 0, bonus, skill, difficulty, success ? 1 : 0);

    if (success)
    {
        ident(o_ptr);
        {
            char o_name[80];
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            msg_format("You identify %s.", o_name);
        }
        return true;
    }

    return false;
}

bool player_try_identify_smithing_object_on_examine(
    object_type* o_ptr, bool is_equipped)
{
    if (p_ptr && p_ptr->image)
        return false;
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (!object_uses_smithing_difficulty(o_ptr))
        return false;
    if (object_known_p(o_ptr))
        return false;

    bool is_floor_object = false;
    for (int i = 1; i < o_max; i++)
    {
        if (o_ptr == &o_list[i])
        {
            is_floor_object = true;
            break;
        }
    }

    if (is_floor_object && !is_equipped)
        return player_auto_identify_smithing_object(o_ptr, false);

    return player_try_identify_smithing_object(o_ptr, is_equipped, 0);
}

bool player_auto_identify_smithing_object(
    object_type* o_ptr, bool ignore_distance_penalty)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (!object_uses_smithing_difficulty(o_ptr))
        return false;
    if (object_known_p(o_ptr))
        return false;

    int skill = player_smithing_identify_skill(
        o_ptr, false, true, ignore_distance_penalty, 0);
    int difficulty = object_smithing_difficulty(o_ptr);
    int dist = distance(p_ptr->py, p_ptr->px, o_ptr->iy, o_ptr->ix);
    /* Reduce the auto-identify distant margin from 10 to 5 */
    int margin = (ignore_distance_penalty || (dist == 0)) ? 0 : 5;

    log_trace(
        "smithing-ident: auto check k_idx=%d tval=%d sval=%d name1=%d ego_pfx=%d ego_sfx=%d skill=%d difficulty=%d margin=%d threshold=%d ignore_dist=%d obj=(%d,%d) player=(%d,%d)",
        o_ptr->k_idx, o_ptr->tval, o_ptr->sval, o_ptr->name1,
        object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
        skill, difficulty, margin, difficulty + margin, ignore_distance_penalty ? 1 : 0,
        o_ptr->iy, o_ptr->ix, p_ptr->py, p_ptr->px);

    if (skill >= difficulty + margin)
    {
        ident(o_ptr);
        {
            char o_name[80];
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            if (dist > 1)
                msg_format("You identify %s from afar.", o_name);
            else
                msg_format("You identify %s.", o_name);
        }
        return true;
    }

    return false;
}

/*
 * Helper function for update_lore()
 */
void update_lore_aux(object_type* o_ptr)
{
    bool is_floor_object = (o_ptr >= o_list) && (o_ptr < (o_list + o_max));

    metarun_try_identify_remembered_artefact(o_ptr);

    /* Auto-identify easy smithing items when seen (distance penalty applies). */
    if (is_floor_object)
        player_auto_identify_smithing_object(o_ptr, false);

    // Identify items the player can auto-identify, even if only awareness is missing.
    if (player_auto_identifies_object(o_ptr)
        && (!object_known_p(o_ptr) || !object_aware_p(o_ptr)))
    {
        ident(o_ptr);
    }

    // Mark new identified artefacts/specials and gain experience for them
    if (object_known_p(o_ptr) && !p_ptr->leaving)
    {
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];
            char note[150];
            char shorter_desc[120];
            int new_exp;

            if (a_ptr->found_num == 0)
            {
                if (a_ptr->flags3 & TR3_EASY_ID)
                    metarun_record_artefact_identification(o_ptr->name1);

                // mark
                a_ptr->found_num = 1;

                // gain experience for identification
                new_exp = 100;
                gain_exp(new_exp);
                p_ptr->ident_exp += new_exp;
                object_desc(shorter_desc, sizeof(shorter_desc), o_ptr, true, 0);
                msg_format("The hidden tale of %s rises before your thought, and 100 experience is won.",
                    shorter_desc);

                // display a note for new artefacts
                if ((o_ptr->name1 != ART_MORGOTH_2)
                    && (o_ptr->name1 != ART_MORGOTH_1)
                    && (o_ptr->name1 != ART_MORGOTH_0))
                {
                    /* Build note and write */
                    if (o_ptr->xtra1 == p_ptr->depth)
                    {
                        sprintf(note, "Found %s", shorter_desc);
                    }
                    else
                    {
                        sprintf(note, "Found %s (from %d ft)", shorter_desc,
                            o_ptr->xtra1 * 50);
                    }

                    /* Record the depth where the artefact was identified */
                    do_cmd_note(note, p_ptr->depth);
                }
            }
        }

        else if (object_has_ego(o_ptr))
        {
            int new_exp = 0;
            byte ego_pfx = object_ego_prefix(o_ptr);
            byte ego_sfx = object_ego_suffix(o_ptr);

            if (ego_pfx)
            {
                e_info[ego_pfx].everseen = true;
                if (!e_info[ego_pfx].aware)
                {
                    cptr ego_name = e_name + e_info[ego_pfx].name;
                    e_info[ego_pfx].aware = true;
                    new_exp += 75;
                    if (ego_name_is_prefix(ego_name))
                    {
                        msg_format("The fore-name %s is made plain to you, and 75 experience is won.",
                            ego_name);
                    }
                    else
                    {
                        msg_format("The after-name %s is made plain to you, and 75 experience is won.",
                            ego_name);
                    }
                }
            }

            if (ego_sfx && ego_sfx != ego_pfx)
            {
                e_info[ego_sfx].everseen = true;
                if (!e_info[ego_sfx].aware)
                {
                    cptr ego_name = e_name + e_info[ego_sfx].name;
                    e_info[ego_sfx].aware = true;
                    new_exp += 75;
                    if (ego_name_is_prefix(ego_name))
                    {
                        msg_format("The fore-name %s is made plain to you, and 75 experience is won.",
                            ego_name);
                    }
                    else
                    {
                        msg_format("The after-name %s is made plain to you, and 75 experience is won.",
                            ego_name);
                    }
                }
            }

            if (new_exp > 0)
            {
                gain_exp(new_exp);
                p_ptr->ident_exp += new_exp;
            }
        }
    }
}

/*
 * This function does a few book keeping things for item identification.
 *
 * It identifies visible objects for the Lore-Keeper ability,
 * marks artefacts/specials as seen and grants experience for the first
 * sighting.
 */
void update_lore(u32b update_flags)
{
    int i;
    object_type* o_ptr;
    static s32b last_floor_turn = -1;
    static int last_floor_y = -1;
    static int last_floor_x = -1;
    static int last_perception = -9999;
    static int last_smithing = -9999;
    static int last_grace = -9999;
    bool identification_skill_changed;
    bool scan_floor;

    identification_skill_changed =
        last_perception != p_ptr->skill_use[S_PER]
        || last_smithing != p_ptr->skill_use[S_SMT]
        || last_grace != p_ptr->stat_use[A_GRA];
    scan_floor = (update_flags & PU_UPDATE_VIEW)
        || last_floor_turn != playerturn
        || last_floor_y != p_ptr->py
        || last_floor_x != p_ptr->px
        || identification_skill_changed;

    /*
     * The floor scan is O(o_max) and smithing identification is non-trivial.
     * Several update phases can run at the same position during one action;
     * only rescan when the view, turn, position, or base identify skill changed.
     */
    if (scan_floor)
    {
        last_floor_turn = playerturn;
        last_floor_y = p_ptr->py;
        last_floor_x = p_ptr->px;
        last_perception = p_ptr->skill_use[S_PER];
        last_smithing = p_ptr->skill_use[S_SMT];
        last_grace = p_ptr->stat_use[A_GRA];

        // Scan all dungeon objects that are 'seen' (in LOS and lit)
        for (i = 1; i < o_max; i++)
        {
            /* Get the next object from the dungeon */
            o_ptr = &o_list[i];

            /* Skip dead objects */
            if (!o_ptr->k_idx)
                continue;

            /* Skip held objects */
            if (o_ptr->held_m_idx)
                continue;

            /* If the object is in sight, or under the player... */
            if ((cave_info[o_ptr->iy][o_ptr->ix] & (CAVE_SEEN))
                || ((p_ptr->py == o_ptr->iy) && (p_ptr->px == o_ptr->ix)))
            {
                update_lore_aux(o_ptr);
            }
        }
    }

    // Scan the inventory / equipment
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the next object from the inventory/equipment */
        o_ptr = &inventory[i];

        /* Skip empty objects */
        if (!o_ptr->k_idx)
            continue;

        update_lore_aux(o_ptr);
    }

    int supply_count = supplies_entry_count();
    for (int s_idx = 0; s_idx < supply_count; s_idx++)
    {
        object_type* supply_obj = supplies_entry_at(s_idx);
        if (!supply_obj || !supply_obj->k_idx)
            continue;

        update_lore_aux(supply_obj);
    }
}
