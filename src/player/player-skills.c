#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"

int affinity_level(int skilltype)
{
    int  level         = 0;
    u32b affinity_flag = 0L;
    u32b penalty_flag  = 0L;

    /* map skill -> (affinity, penalty) pair */
    switch (skilltype)
    {
        case S_MEL: affinity_flag = RHF_MEL_AFFINITY; penalty_flag = RHF_MEL_PENALTY; break;
        case S_ARC: affinity_flag = RHF_ARC_AFFINITY; penalty_flag = RHF_ARC_PENALTY; break;
        case S_EVN: affinity_flag = RHF_EVN_AFFINITY; penalty_flag = RHF_EVN_PENALTY; break;
        case S_STL: affinity_flag = RHF_STL_AFFINITY; penalty_flag = RHF_STL_PENALTY; break;
        case S_PER: affinity_flag = RHF_PER_AFFINITY; penalty_flag = RHF_PER_PENALTY; break;
        case S_WIL: affinity_flag = RHF_WIL_AFFINITY; penalty_flag = RHF_WIL_PENALTY; break;
        case S_SMT: affinity_flag = RHF_SMT_AFFINITY; penalty_flag = RHF_SMT_PENALTY; break;
        case S_SNG: affinity_flag = RHF_SNG_AFFINITY; penalty_flag = RHF_SNG_PENALTY; break;
        default:    return 0;
    }

    /* race + character */
    if (rp_ptr->flags & affinity_flag) level++;
    if (current_character_profile->flags & affinity_flag) level++;
    if (rp_ptr->flags & penalty_flag)  level--;
    if (current_character_profile->flags & penalty_flag)  level--;

    /* every copy of the same curse flag */
    level += curse_flag_count_rhf(affinity_flag);
    level -= curse_flag_count_rhf(penalty_flag);

    /* keep inside the allowed range */
    if (level >  2) level =  2;
    if (level < -2) level = -2;

    if ((skilltype == S_WIL) && (current_character_profile->flags_u & UNQ_EARENDIL)) level = 3;

    return level;
}

/*
 * Calculate the minstrel bonus for song abilities.
 * Unlike affinity_level, this is uncapped and only affects ability costs.
 * It does not provide skill increases.
 */
int minstrel_level(void)
{
    int level = 0;

    /* Check for MINSTREL unique flag */
    if (current_character_profile->flags_u & UNQ_MINSTREL) level++;

    /* Include curse flags (similar to affinity) */
    level += curse_flag_count_rhf(RHF_SNG_AFFINITY);
    level -= curse_flag_count_rhf(RHF_SNG_PENALTY);

    /* No cap - can go beyond 2 */
    return level;
}

static bool songs_are_synergy_pair(byte song_a, byte song_b)
{
    static const byte synergy_pairs[][2] = {
        { SNG_ELBERETH,  SNG_TREES },
        { SNG_ELBERETH,  SNG_STAUNCHING },
        { SNG_CHALLENGE, SNG_SLAYING },
        { SNG_DELVINGS,  SNG_REVEALING },
        { SNG_FREEDOM,   SNG_ELVENESS },
        { SNG_STAYING,   SNG_CONTEST },
        { SNG_STAYING,   SNG_LAMENT },
        { SNG_SILENCE,   SNG_DISGUISE },
        { SNG_SILENCE,   SNG_LORIEN },
        { SNG_SHATTERING, SNG_MASTERY },
    };

    if ((song_a == SNG_NOTHING) || (song_b == SNG_NOTHING))
        return false;

    for (size_t i = 0; i < N_ELEMENTS(synergy_pairs); i++)
    {
        if ((song_a == synergy_pairs[i][0] && song_b == synergy_pairs[i][1])
            || (song_a == synergy_pairs[i][1] && song_b == synergy_pairs[i][0]))
        {
            return true;
        }
    }

    return false;
}

static int song_synergy_bonus(byte abilitynum, int full_skill)
{
    int synergy = 0;
    byte partner = SNG_NOTHING;

    if (full_skill <= 0)
        return 0;

    if (p_ptr->song1 == abilitynum)
        partner = p_ptr->song2;
    else if (p_ptr->song2 == abilitynum)
        partner = p_ptr->song1;
    else
        return 0;

    if (!songs_are_synergy_pair(abilitynum, partner))
        return 0;

    /* 10% of base song skill (integer math, rounded). */
    synergy = (full_skill + 5) / 10;

    return synergy;
}

int song_effective_skill(int abilitynum)
{
    int skill = p_ptr->skill_use[S_SNG];
    const int full_skill = skill;

    // penalize minor themes - check if this ability is the minor theme
    // UNLESS the character has the WOVEN_MASTER flag (Daeron)
    if ((p_ptr->song2 == abilitynum) && (p_ptr->song1 != abilitynum))
    {
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_WOVEN_MASTER))
            skill /= 2;
    }

    // Song of Silence dampens other songs when woven together
    // EXCEPT for Disguise and Lórien (its synergy pairs)
    // This dampening is applied BEFORE synergy bonus
    if (singing(SNG_SILENCE) && (abilitynum != SNG_SILENCE)
        && (abilitynum != SNG_DISGUISE) && (abilitynum != SNG_LORIEN))
    {
        // Calculate Silence bonus directly to avoid recursion
        int silence_skill = p_ptr->skill_use[S_SNG] / 2;
        int silence_penalty = silence_skill / 2;
        skill -= silence_penalty;
        if (skill < 0)
            skill = 0;
    }

    // woven theme synergy pairs grant an extra 20% of base song skill
    skill += song_synergy_bonus(abilitynum, full_skill);

    // effective skill is never negative
    if (skill < 0)
        skill = 0;

    return skill;
}

/*
 * Return a stepped bonus that starts at 1 and grows after widening ranges.
 * Example: first_threshold=5, next_gap=6 => 1 at 0-5, 2 at 6-11, 3 at 12-18, ...
 */
static int stepped_song_bonus(int skill, int first_threshold, int next_gap)
{
    int bonus = 1;
    int threshold = first_threshold;
    int gap = next_gap;

    if (skill < 0)
        skill = 0;

    while (skill > threshold)
    {
        bonus++;
        threshold += gap;
        gap++;
    }

    return bonus;
}

static bool ability_weapon_skill_partner_active(int skilltype, int abilitynum)
{
    if (skilltype == S_MEL && abilitynum == MEL_WARDEN)
        return p_ptr->active_ability[S_ARC][ARC_VERSATILITY];

    if (skilltype == S_ARC && abilitynum == ARC_VERSATILITY)
        return p_ptr->active_ability[S_MEL][MEL_WARDEN];

    return false;
}

static int ability_weapon_skill_combo_bonus(int base_skill)
{
    return (MAX(0, base_skill) + 2) / 3;
}

static int ability_weapon_skill_bonus(int skilltype, int abilitynum,
    bool target_active, bool partner_active, bool weapon_active)
{
    if (skilltype == S_MEL && abilitynum == MEL_WARDEN)
    {
        if (!target_active || !weapon_active)
            return 0;

        if (partner_active)
            return ability_weapon_skill_combo_bonus(p_ptr->skill_base[S_MEL]);

        if (p_ptr->skill_base[S_MEL] > p_ptr->skill_base[S_ARC])
        {
            return (p_ptr->skill_base[S_MEL] - p_ptr->skill_base[S_ARC]) / 2;
        }
    }
    else if (skilltype == S_ARC && abilitynum == ARC_VERSATILITY)
    {
        if (!target_active || !weapon_active)
            return 0;

        if (partner_active)
            return ability_weapon_skill_combo_bonus(p_ptr->skill_base[S_ARC]);

        if (p_ptr->skill_base[S_ARC] > p_ptr->skill_base[S_MEL])
        {
            return (p_ptr->skill_base[S_ARC] - p_ptr->skill_base[S_MEL]) / 2;
        }
    }

    return 0;
}

/* Return the current weapon-dependent skill addition supplied by Warden or
 * Versatility.  Keep this calculation shared by the bonus pass and the
 * ability browser so the displayed value follows the actual combat value. */
int ability_current_skill_bonus(int skilltype, int abilitynum)
{
    bool weapon_active;

    weapon_active = (skilltype == S_MEL)
        ? player_active_weapon_is_ranged()
        : player_active_weapon_is_melee();

    return ability_weapon_skill_bonus(skilltype, abilitynum,
        p_ptr->active_ability[skilltype][abilitynum],
        ability_weapon_skill_partner_active(skilltype, abilitynum),
        weapon_active);
}

/* Return the value the ability would add if acquired and enabled now.  The
 * matching weapon check is intentionally omitted: this is a preview of the
 * ability's effect, not the player's current loadout contribution. */
int ability_potential_skill_bonus(int skilltype, int abilitynum)
{
    return ability_weapon_skill_bonus(skilltype, abilitynum, true,
        ability_weapon_skill_partner_active(skilltype, abilitynum), true);
}

/* Return the value the ability would add when it and its partner are both
 * acquired and enabled.  This is used to explain the reciprocal increase in
 * the Warden/Versatility ability previews. */
int ability_potential_skill_bonus_with_partner(int skilltype, int abilitynum)
{
    return ability_weapon_skill_bonus(skilltype, abilitynum, true, true, true);
}

int ability_bonus(int skilltype, int abilitynum)
{
    int bonus = 0;
    int skill = p_ptr->skill_use[skilltype];

    if (skilltype == S_SNG)
    {
        skill = song_effective_skill(abilitynum);

        switch (abilitynum)
        {
        case SNG_ELBERETH:
        {
            bonus = skill;
            break;
        }
        case SNG_CHALLENGE:
        {
            bonus = skill;
            break;
        }
        case SNG_FREEDOM:
        {
            bonus = skill;
            break;
        }
        case SNG_STAUNCHING:
        {
            bonus = skill;
            break;
        }
        case SNG_SILENCE:
        {
            bonus = skill / 2;
            break;
        }
        case SNG_DELVINGS:
        {
            bonus = skill;
            break;
        }
        case SNG_REVEALING:
        {
            bonus = skill;
            break;
        }
        case SNG_THRESHOLDS:
        {
            bonus = skill;
            break;
        }
        case SNG_TREES:
        {
            bonus = stepped_song_bonus(skill, 5, 6);
            break;
        }
        case SNG_ELVENESS:
        {
            bonus = stepped_song_bonus(skill, 7, 8);
            break;
        }
        case SNG_DISGUISE:
        {
            bonus = skill + 5;
            break;
        }
        case SNG_STAYING:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN) ? 2 : 1) * skill;
            break;
        }
        case SNG_SLAYING:
        {
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN) ? 2 : 1) * skill * 2;
            break;
        }
        case SNG_LORIEN:
        {
            bonus = skill;
            break;
        }
        case SNG_MASTERY:
        {
            /* Thingol: Song of Mastery is 1.75x effective (7/4 as integer math) */
            bonus = ((c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL) ? (7 * skill) / 4 : skill);
            break;
        }
        case SNG_SHATTERING:
        {
            bonus = skill;
            break;
        }
        case SNG_CONTEST:
        {
            bonus = skill;
            break;
        }
        case SNG_LAMENT:
        {
            bonus = skill;
            break;
        }
        }

        // these bonuses are never negative
        if (bonus < 0)
            bonus = 0;
    }

    return (bonus);
}

/*
 * Computes current weight limit in tenths of pounds.
 *
 * 100 + a compounding 20% bonus per point of str
 */
int weight_limit(void)
{
    int i;
    int limit;

    limit = 1000;
    if (p_ptr->stat_use[A_STR] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_STR]; i++)
        {
            limit = limit * 12 / 10;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_STR]); i++)
        {
            limit = limit * 10 / 12;
        }
    }

    /* CUR_WEAK: curse reduces weight limit by 20% per stack; blessing increases by 20% per stack */
    int weak_delta = curse_flag_delta_cur(CUR_WEAK);
    if (weak_delta > 0) {
        for (i = 0; i < weak_delta; i++) limit = limit * 8 / 10;
    } else if (weak_delta < 0) {
        for (i = 0; i < -weak_delta; i++) limit = limit * 12 / 10;
    }

    /* Return the result */
    return (limit);
}

bool sprinting(void)
{
    int i;
    int turns = 1;

    if (p_ptr->active_ability[S_EVN][EVN_SPRINTING])
    {
        /* Count up to 5 squares so the heavier-armour threshold can be met */
        for (i = 1; i < 5; i++)
        {
            if ((p_ptr->previous_action[i] >= 1)
                && (p_ptr->previous_action[i] <= 9)
                && (p_ptr->previous_action[i] != 5))
            {
                if ((p_ptr->previous_action[i + 1] >= 1)
                    && (p_ptr->previous_action[i + 1] <= 9)
                    && (p_ptr->previous_action[i + 1] != 5))
                {
                    if (p_ptr->previous_action[i]
                        == p_ptr->previous_action[i + 1])
                    {
                        turns++;
                    }
                    else if (p_ptr->previous_action[i]
                        == cycle[chome[p_ptr->previous_action[i + 1]] - 1])
                    {
                        turns++;
                    }
                    else if (p_ptr->previous_action[i]
                        == cycle[chome[p_ptr->previous_action[i + 1]] + 1])
                    {
                        turns++;
                    }
                }
            }
        }
    }

    /* Light armour lets you reach top speed a square sooner */
    return (turns >= (wearing_only_light_armour() ? 4 : 5));
}

/* Calculate stats */
void calc_stats(void)
{
    for (int i = 0; i < A_MAX; i++)
    {
        /* Extract the new "stat_use" value for the stat */
        p_ptr->stat_use[i] = p_ptr->stat_base[i] + p_ptr->stat_equip_mod[i]
            + p_ptr->stat_drain[i] + p_ptr->stat_misc_mod[i];

        /* cap to -9 and 20 */
        if (p_ptr->stat_use[i] < BASE_STAT_MIN)
            p_ptr->stat_use[i] = BASE_STAT_MIN;
        else if (p_ptr->stat_use[i] > BASE_STAT_MAX)
            p_ptr->stat_use[i] = BASE_STAT_MAX;
    }
}
