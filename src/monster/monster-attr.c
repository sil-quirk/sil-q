/* File: monster-attr.c */

#include "monster-internal.h"

/*
 *  Calculates a skill score for a monster
 */
int monster_skill(monster_type* m_ptr, int skill_type)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int skill = 0;

    switch (skill_type)
    {
    case S_MEL:
        msg_debug("Can't determine the monster's Melee score.");
        break;
    case S_ARC:
        msg_debug("Can't determine the monster's Archery score.");
        break;
    case S_EVN:
        msg_debug("Can't determine the monster's Evasion score.");
        break;
    case S_STL:
        skill = r_ptr->stl;
        skill -= m_ptr->song_stealth_penalty;
        skill += 2 * curse_flag_delta_cur(CUR_MON_STL);   /* +/-2 Stl per stack */
        break;
    case S_PER:
        skill = r_ptr->per;
        skill += 2 * curse_flag_delta_cur(CUR_MON_PER);   /* +/-2 Per per stack */
        break;
    case S_WIL:
        skill = r_ptr->wil;
        skill -= m_ptr->song_will_penalty;
        skill += 2 * curse_flag_delta_cur(CUR_MON_WIL);   /* +/-2 Wil per stack */
        break;
    case S_SMT:
        msg_debug("Can't determine the monster's Smithing score.");
        break;
    case S_SNG:
        msg_debug("Can't determine the monster's Song score.");
        break;

    default:
        msg_debug("Asked for an invalid monster skill.");
        break;
    }

    // penalise stunning
    if (m_ptr->stunned)
        skill -= 2;

    // Song of Challenge debuff - applies while singing or for some time after
    // NOTE: Challenge now reduces monster Will (S_WIL) in addition to Stealth
    if (p_ptr->song_challenge_effect > 0 && (skill_type == S_STL || skill_type == S_WIL))
    {
        // Calculate the full penalty and max duration based on current song skill
        int song_skill = ability_bonus(S_SNG, SNG_CHALLENGE);
    int full_penalty = song_skill / 5;
    if (full_penalty < 1) full_penalty = 1;

        // Calculate max duration: 15 turns at skill 20, formula: (skill * 3) / 4
        int max_duration = (song_skill * 3) / 4;
        if (max_duration < 3) max_duration = 3;

        // Scale the penalty based on remaining duration
        int penalty = (full_penalty * p_ptr->song_challenge_effect) / max_duration;
        if (penalty < 1 && p_ptr->song_challenge_effect > 0) penalty = 1;

        if (penalty > 0)
        {
            int before = skill;
            skill -= penalty;
            log_debug(
                "Song of Challenge penalty applied (r_idx=%d skill=%d -> %d, "
                "delta=%d, effect=%d/%d)",
                (int)m_ptr->r_idx, before, skill, penalty,
                p_ptr->song_challenge_effect, max_duration);
        }
    }

    // Song of Elbereth debuff - applies while singing or for some time after
    if (p_ptr->song_elbereth_effect > 0 && skill_type == S_WIL)
    {
        // Calculate the full penalty and max duration based on current song skill
        int song_skill = ability_bonus(S_SNG, SNG_ELBERETH);
    int full_penalty = song_skill / 5;
    if (full_penalty < 1) full_penalty = 1;

        // Calculate max duration: 15 turns at skill 20, formula: (skill * 3) / 4
        int max_duration = (song_skill * 3) / 4;
        if (max_duration < 3) max_duration = 3;

        // Scale the penalty based on remaining duration
        int penalty = (full_penalty * p_ptr->song_elbereth_effect) / max_duration;
        if (penalty < 1 && p_ptr->song_elbereth_effect > 0) penalty = 1;

        if (penalty > 0)
        {
            int before = skill;
            skill -= penalty;
            log_debug(
                "Song of Elbereth penalty applied (r_idx=%d skill=%d -> %d, "
                "delta=%d, effect=%d/%d)",
                (int)m_ptr->r_idx, before, skill, penalty,
                p_ptr->song_elbereth_effect, max_duration);
        }
    }

    return (skill);
}

/*
 *  Calculates a Stat score for a monster
 */
int monster_stat(monster_type* m_ptr, int stat_type)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int stat = 0;

    int mhp = m_ptr->maxhp;
    int base = 20;

    switch (stat_type)
    {
    case A_STR:
        stat = (r_ptr->blow[0].dd * 2) + (r_ptr->hdice / 10) - 4;
        ;
        break;
    case A_DEX:
        msg_debug("Can't determine the monster's Dex score.");
        break;
    case A_CON:
        if (mhp < base)
        {
            while (mhp < base)
            {
                stat--;
                base = (base * 10) / 12;
            }
        }
        else if (mhp >= base)
        {
            stat--;
            while (mhp >= base)
            {
                stat++;
                base = (base * 12) / 10;
            }
        }
        // msg_debug("%d => %d.", m_ptr->maxhp, stat); // Sil-y: this seems
        // slightly erroneous for extreme values
        break;
    case A_GRA:
        msg_debug("Can't determine the monster's Gra score.");
        break;

    default:
        msg_debug("Asked for an invalid monster stat.");
        break;
    }

    return (stat);
}

bool monster_race_is_vala(int r_idx)
{
    monster_race* r_ptr;

    if (!z_info || !r_info)
        return false;
    if ((r_idx <= 0) || (r_idx >= z_info->r_max))
        return false;

    /* Morgoth has bespoke alertness/sleep mechanics tied to the Iron Crown. */
    if (r_idx == R_IDX_MORGOTH)
        return false;

    r_ptr = &r_info[r_idx];
    if (!r_ptr->name)
        return false;

    return (r_ptr->d_char == 'V');
}

bool monster_clear_vala_state(monster_type* m_ptr)
{
    bool speed_changed;

    if (!m_ptr || !monster_race_is_vala(m_ptr->r_idx))
        return false;

    speed_changed = (m_ptr->hasted != 0) || (m_ptr->slowed != 0);

    m_ptr->alertness = ALERTNESS_ALERT;
    m_ptr->stunned = 0;
    m_ptr->confused = 0;
    m_ptr->slowed = 0;
    m_ptr->hasted = 0;
    m_ptr->skip_next_turn = false;
    m_ptr->skip_this_turn = false;
    m_ptr->stance = STANCE_CONFIDENT;

    return speed_changed;
}

void set_monster_haste(s16b m_idx, s16b counter, bool message)
{
    /*get the monster at the given location*/
    monster_type* m_ptr = &mon_list[m_idx];

    bool recalc = false;

    char m_name[80];

    if (monster_race_is_vala(m_ptr->r_idx))
    {
        if (monster_clear_vala_state(m_ptr))
            calc_monster_speed(m_ptr->fy, m_ptr->fx);
        return;
    }

    /* Get monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /*see if we need to recalculate speed*/
    if (m_ptr->hasted)
    {
        /*monster is no longer hasted and speed needs to be recalculated*/
        if (counter == 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s slows down.", m_name);
        }
    }
    else
    {
        /*monster is now hasted and speed needs to be recalculated*/
        if (counter > 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s starts moving faster.", m_name);
        }
    }

    /*update the counter*/
    m_ptr->hasted = counter;

    /*re-calculate speed if necessary*/
    if (recalc)
        calc_monster_speed(m_ptr->fy, m_ptr->fx);

    return;
}

void set_monster_slow(s16b m_idx, s16b counter, bool message)
{
    /*get the monster at the given location*/
    monster_type* m_ptr = &mon_list[m_idx];

    bool recalc = false;

    char m_name[80];

    if (monster_race_is_vala(m_ptr->r_idx))
    {
        if (monster_clear_vala_state(m_ptr))
            calc_monster_speed(m_ptr->fy, m_ptr->fx);
        return;
    }

    /* Get monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /*see if we need to recalculate speed*/
    if (m_ptr->slowed)
    {
        /*monster is no longer slowed and speed needs to be recalculated*/
        if (counter == 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s speeds up.", m_name);
        }
    }
    else
    {
        /*monster is now slowed and speed needs to be recalculated*/
        if (counter > 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s starts moving slower.", m_name);
        }
    }

    /*update the counter*/
    m_ptr->slowed = counter;

    /*re-calculate speed if necessary*/
    if (recalc)
        calc_monster_speed(m_ptr->fy, m_ptr->fx);

    return;
}

