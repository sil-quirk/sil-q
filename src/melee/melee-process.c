#include "angband.h"
#include "externs.h"
#include "melee/melee-attack.h"
#include "melee/melee-movement.h"
#include "melee/melee-process.h"
#include "melee/melee-util.h"

int challenge_check(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int challenge = ability_bonus(S_SNG, SNG_CHALLENGE);
    int resistance = monster_skill(m_ptr, S_WIL);

    if (!singing(SNG_CHALLENGE) || m_ptr->stance != STANCE_AGGRESSIVE
        || (r_ptr->flags3 & (RF3_NO_CONF)) || m_ptr->r_idx == R_IDX_MORGOTH)
        return 0;

    // Adjust to work best against lower-will monsters.
    resistance = (resistance * resistance) / 10;

    // adjust difficulty by the distance to the monster
    return skill_check(PLAYER, challenge,
        resistance + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx), m_ptr);
}

/*
 * Calculate minimum and desired combat ranges.  -BR-
 */
void find_range(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* All "afraid" monsters will run away */
    if (m_ptr->stance == STANCE_FLEEING)
        m_ptr->min_range = FLEE_RANGE;

    /* Some monsters run when low on mana */
    else if ((r_ptr->flags2 & (RF2_LOW_MANA_RUN))
        && (m_ptr->mana < MON_MANA_MAX / 5))
        m_ptr->min_range = FLEE_RANGE;

    /*mindless monsters always charge*/
    else if (r_ptr->flags2 & (RF2_MINDLESS))
        m_ptr->min_range = 1;

    /* Other monsters default to range 1 */
    else
        m_ptr->min_range = 1;

    if (m_ptr->min_range < FLEE_RANGE)
    {
        /* Creatures that don't move never like to get too close */
        if (r_ptr->flags1 & (RF1_NEVER_MOVE))
            m_ptr->min_range += 3;

        /* Spellcasters that don't strike never like to get too close */
        if (r_ptr->flags1 & (RF1_NEVER_BLOW))
            m_ptr->min_range += 6;

        // Spies have a high minimum range - and get harder to track down
        // at greater depths
        if ((r_ptr->flags2 & (RF2_SMART)) && (r_ptr->flags4 & (RF4_SHRIEK))
            && (m_ptr->stance != STANCE_AGGRESSIVE))
        {
            m_ptr->min_range = 5 + p_ptr->depth / 2;
        }
    }

    /* Handle range greater than FLEE Range (but without an if statement
     * for efficiency)
     */
    else
        m_ptr->min_range = FLEE_RANGE;

    /* Nearby monsters that cannot run away will stand and fight */
    if ((m_ptr->cdis < TURN_RANGE) && (m_ptr->mspeed < p_ptr->pspeed))
        m_ptr->min_range = 1;

    /* Now find preferred range */
    m_ptr->best_range = m_ptr->min_range;

    if (challenge_check(m_ptr) > 0)
    {
        char m_name[80];
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
        if (m_ptr->ml)
        {
            msg_format("%^s is agitated by your song.", m_name);
        }

        m_ptr->mana = 0;
        m_ptr->best_range = 0;
        m_ptr->min_range = 0;
    }
    else if ((r_ptr->freq_ranged > 15) && (m_ptr->r_idx != R_IDX_MORGOTH))
    {
        /* Breathers like range 2  */
        if ((r_ptr->flags4 & (RF4_BREATH_MASK)) && (m_ptr->best_range < 6))
        {
            m_ptr->best_range = 2;
        }

        /* Specialized ranged attackers will sit back */
        else if (m_ptr->mana >= MON_MANA_MAX / 5)
        {
            m_ptr->best_range += (r_ptr->freq_ranged - 15) / 5;
            if (m_ptr->best_range > 8)
                m_ptr->best_range = 8;
            m_ptr->min_range = m_ptr->best_range - 1;
        }
    }

    // Deal with the 'truce' on Morgoth's level (overrides everything else)
    if (p_ptr->truce && (m_ptr->min_range < 25))
    {
        m_ptr->min_range = 25;
        m_ptr->best_range = 25;
    }
}

static void remove_expensive_spells(int m_idx, u32b* f4p)
{
    monster_type* m_ptr = &mon_list[m_idx];

    int i;

    u32b f4 = (*f4p);

    /* check innate spells for mana available */
    for (i = 0; i < 32; i++)
    {
        if (spell_info_RF4[i][COL_SPELL_MANA_COST] > m_ptr->mana)
            f4 &= ~(0x00000001 << i);
    }

    /* Modify the spell list. */
    (*f4p) = f4;
}

/*
 * Intelligent monsters use this function to filter away spells
 * which have no benefit.
 */
static void remove_invalid_spells(int m_idx, u32b* f4p)
{
    monster_type* m_ptr = &mon_list[m_idx];

    u32b f4 = (*f4p);

    int dy, dx;
    int dist = distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);

    // Screech only works at very close range
    if (m_ptr->cdis > 2)
    {
        f4 &= ~(RF4_SCREECH);
    }

    // make sure that missile attacks are never done at melee range or when
    // afraid
    if ((dist == 1) || (m_ptr->stance == STANCE_FLEEING) || p_ptr->truce)
    {
        f4 &= ~(RF4_ARROW1);
        f4 &= ~(RF4_ARROW2);
        f4 &= ~(RF4_BOULDER);
        f4 &= ~(RF4_EARTHQUAKE);
    }

    // make sure that breath attacks are never used when the monster is fleeing
    if (m_ptr->stance == STANCE_FLEEING)
    {
        f4 &= ~(RF4_BREATH_MASK);
    }

    // no songs during the truce
    if (p_ptr->truce)
    {
        f4 &= ~(RF4_SNG_MASK);
    }

    // no songs by Morgoth until uncrowned
    if ((m_ptr->r_idx == R_IDX_MORGOTH)
        && !p_ptr->on_the_run
        && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
    {
        f4 &= ~(RF4_SNG_MASK);
    }

    // In his throne hall, Morgoth should not waste turns on the door-closing
    // part of Song of Binding before the pursuit begins.
    if ((m_ptr->r_idx == R_IDX_MORGOTH)
        && (p_ptr->depth == MORGOTH_DEPTH)
        && p_ptr->morgoth_hall_entered
        && !p_ptr->on_the_run)
    {
        f4 &= ~(RF4_SNG_BINDING);
    }

    // projectiles have limited range
    if (dist > 5)
        f4 &= ~(RF4_BOULDER);
    if (dist > 10)
        f4 &= ~(RF4_ARROW1);
    if (dist > 16)
        f4 &= ~(RF4_ARROW2);

    // Earthquake is only useful at close range and if there is no monster in
    // the smashed square
    dy = (m_ptr->fy > p_ptr->py) ? -1 : ((m_ptr->fy < p_ptr->py) ? 1 : 0);
    dx = (m_ptr->fx > p_ptr->px) ? -1 : ((m_ptr->fx < p_ptr->px) ? 1 : 0);
    if ((m_ptr->cdis > 3) || (cave_m_idx[m_ptr->fy + dy][m_ptr->fx + dx] > 0))
    {
        f4 &= ~(RF4_EARTHQUAKE);
    }

    /* Darkness is only useful if the player's square is lit */
    if (!(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)))
        f4 &= ~(RF4_DARKNESS);

    /* Modify the spell list. */
    (*f4p) = f4;
}

/*
 * Count the number of castable spells.
 *
 * If exactly 1 spell is available cast it.  If more than more is
 * available, and the random bit is set, pick one.
 *
 * Used as a short cut in 'choose_attack_spell' to circumvent AI
 * when there is only 1 choice. (random=false)
 *
 * Also used in 'choose_attack_spell' to circumvent AI when
 * casting randomly (random=true), as with dumb monsters.
 */
static int choose_attack_spell_fast(u32b* f4p, bool do_random)
{
    int i, num = 0;
    byte spells[128];

    u32b f4 = (*f4p);

    /* Extract the 'spells' */
    for (i = 0; i < 32; i++)
    {
        if (f4 & (1L << i))
            spells[num++] = i + 32 * 3;
    }

    /* Paranoia */
    if (num == 0)
        return (0);

    /* Go quick if possible */
    if (num == 1)
    {
        /* Cast the one spell */
        return (spells[0]);
    }

    /*
     * If we aren't allowed to choose at random
     * and we have multiple spells left, give up on quick
     * selection
     */
    if (!(do_random))
        return (0);

    /* Pick at random */
    return (spells[rand_int(num)]);
}

/*
 * Have a monster choose a spell.
 *
 * Monster at m_idx uses this function to select a legal attack spell.
 * Spell casting AI is based here.
 *
 * First the code will try to save time by seeing if
 * choose_attack_spell_fast is helpful.  Otherwise, various AI
 * parameters are used to calculate a 'desirability' for each spell.
 * There is some randomness.  The most desirable spell is cast.
 *
 * Returns the spell number, of '0' if no spell is selected.
 *
 *-BR-
 */
static int choose_ranged_attack(int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    u32b f4;

    byte spell_range;

    bool do_random = false;

    int i;
    int path;

    int cur_range = 0;

    int best_spell = 0, best_spell_rating = 0;
    int cur_spell_rating;

    /* Extract the racial spell flags */
    f4 = r_ptr->flags4;

    /* Check what kinds of spells can hit player */
    path
        = projectable(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK);

    /* do we have the player in sight at all? */
    if (path == PROJECT_NO)
    {
        return (0);
    }

    /* remove boulders and archery */
    else if (path == PROJECT_NOT_CLEAR)
    {
        f4 &= ~(RF4_ARCHERY_MASK);
    }

    /* No spells left */
    if (!f4)
        return (0);

    /* Spells we can not afford */
    remove_expensive_spells(m_idx, &f4);

    /* No spells left */
    if (!f4)
        return (0);

    /* Mindless monsters choose at random. */
    if (r_ptr->flags2 & (RF2_MINDLESS))
        return (choose_attack_spell_fast(&f4, true));

    /* Remove spells that have unfulfilled conditions */
    remove_invalid_spells(m_idx, &f4);

    /* No spells left */
    if (!f4)
        return (0);

    /* Sometimes non-dumb monsters cast randomly (though from the
     * restricted list)
     */
    if ((!(r_ptr->flags2 & (RF2_SMART))) && (one_in_(5)))
        do_random = true;

    /* Try 'fast' selection first.
     * If there is only one spell, choose that spell.
     * If there are multiple spells, choose one randomly if the 'random' flag is
     * set. Otherwise fail, and let the AI choose.
     */
    best_spell = choose_attack_spell_fast(&f4, do_random);
    if (best_spell)
        return (best_spell);

    /* Check if no spells left */
    if (!f4)
        return (0);

    /* The conditionals are written for speed rather than readability
     * They should probably stay that way. */
    for (i = 0; i < 32; i++)
    {
        /* Do we even have this spell? */
        if (!(f4 & (1L << i)))
            continue;
        spell_range = spell_info_RF4[i][COL_SPELL_BEST_RANGE];

        /* Base Desirability*/
        cur_spell_rating = spell_desire_RF4[i][D_BASE];

        /* Penalty for range if attack drops off in power */
        if (spell_range)
        {
            cur_range = m_ptr->cdis;
            while (cur_range-- > spell_range)
                cur_spell_rating
                    = (cur_spell_rating * spell_desire_RF4[i][D_RANGE]) / 100;
        }

        /* Random factor; less random for smart monsters */
        if (r_ptr->flags2 & (RF2_SMART))
            cur_spell_rating += rand_int(10);
        else
            cur_spell_rating += rand_int(50);

        /* Is this the best spell yet?, or alternate between equal spells*/
        if ((cur_spell_rating > best_spell_rating)
            || ((cur_spell_rating == best_spell_rating) && one_in_(2)))
        {
            best_spell_rating = cur_spell_rating;
            best_spell = i + 96;
        }
    }

    if (p_ptr->wizard)
    {
        msg_format("Spell rating: %i.", best_spell_rating);
    }

    // Abort if there are no good spells
    if (best_spell_rating == 0)
        return (0);

    /* Return Best Spell */
    return (best_spell);
}

static bool has_sleeping_kin(monster_type* m_ptr)
{
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    int i;
    bool has_kin = false;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        /* Access the monster */
        monster_type* n_ptr = &mon_list[i];
        monster_race* nr_ptr = &r_info[n_ptr->r_idx];

        /* Access the monster */
        n_ptr = &mon_list[i];
        nr_ptr = &r_info[n_ptr->r_idx];

        /* Ignore dead monsters */
        if (!n_ptr->r_idx)
            continue;

        /* Ignore monsters with the wrong symbol */
        if (r_ptr->d_char != nr_ptr->d_char)
            continue;

        // determine the distance between the monsters
        if (!los(fy, fx, n_ptr->fy, n_ptr->fx))
            continue;

        // Ignore monsters that are awake
        if (n_ptr->alertness >= ALERTNESS_ALERT)
            continue;

        /* Activate all other monsters and communicate to them */
        has_kin = true;
    }

    return (has_kin);
}

void wander(monster_type* m_ptr)
{
    int ty, tx;
    bool fear = false;
    bool bash = false;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    // begin a song of piercing if possible
    // note that Morgoth must be uncrowned
    bool allow_piercing = true;
    if (m_ptr->r_idx == R_IDX_MORGOTH)
        allow_piercing = p_ptr->on_the_run
            || ((&a_info[ART_MORGOTH_3])->cur_num == 1);

    if (allow_piercing
        && (r_ptr->flags4 & (RF4_SNG_PIERCING))
        && (m_ptr->song != SNG_PIERCING)
        && (m_ptr->alertness < ALERTNESS_ALERT)
        && (m_ptr->mana >= MON_MANA_COST))
    {
        make_attack_ranged(m_ptr, 96 + 19);
    }

    // occasionally update the flow (keeping the centre the same)
    // to take account of changes in the dungeon (new glyphs of warding, doors
    // closed etc)
    if (one_in_(10))
    {
        // only do this if they have a real wandering index (not a zero due to
        // too many wandering monster groups)
        if (m_ptr->wandering_idx >= FLOW_WANDERING_HEAD)
        {
            update_flow(flow_center_y[m_ptr->wandering_idx],
                flow_center_x[m_ptr->wandering_idx], m_ptr->wandering_idx);
        }
    }

    /* Choose a pair of target grids, or cancel the move. */
    if (!get_move_wander(m_ptr, &ty, &tx))
    {
        return;
    }

    /* Calculate the actual move.  Cancel move on failure to enter grid. */
    if (!make_move(m_ptr, &ty, &tx, fear, &bash))
        return;

    /* Change terrain, move the monster, handle secondary effects. */
    process_move(m_ptr, ty, tx, bash);
}

static bool morgoth_has_player_track(const monster_type* m_ptr)
{
    if (los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
        return true;
    if (m_ptr->target_y && m_ptr->target_x)
        return true;
    /* Mirror the "can hear the player" activity threshold. */
    return (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) < 20);
}

static void maybe_start_morgoth_piercing(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (m_ptr->r_idx != R_IDX_MORGOTH || !p_ptr->on_the_run)
        return;
    if (!(r_ptr->flags4 & (RF4_SNG_PIERCING)))
        return;
    if (m_ptr->song != SNG_NOTHING)
        return;
    if (m_ptr->mana < MON_MANA_COST)
        return;
    if (morgoth_has_player_track(m_ptr))
        return;

    make_attack_ranged(m_ptr, 96 + 19);
}

int get_chance_of_ranged_attack(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Extract the ranged attack probability. */
    int chance = r_ptr->freq_ranged;

    /* Certain conditions always cause a monster to always cast */
    if (m_ptr->mflag & (MFLAG_ALWAYS_CAST))
        chance = 100;

    /* Cannot use ranged attacks when confused. */
    if (m_ptr->confused)
        chance = 0;

    /* Cannot use ranged attacks during the truce. */
    if (p_ptr->truce)
        chance = 0;

    /* Stunned monsters use ranged attacks half as often. */
    if ((chance) && (m_ptr->stunned))
        chance /= 2;

    /* Successfully challenged monsters get no ranged attacks. */
    if ((singing(SNG_CHALLENGE)) && (m_ptr->stance == STANCE_AGGRESSIVE))
        chance = 0;

    return chance;
}

/*
 * Monster takes its turn.
 */
static void process_monster(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];
    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    int i, k, y, x;
    int ty, tx;
    int chance = 0;
    int choice = 0;

    bool fear = false;

    bool bash = false;

    /* Assume the monster doesn't have a target */
    bool must_use_target = false;

    /* Will the monster move randomly? */
    bool random_move = false;

    // Morgoth is always active during the escape
    // Sil-y: but this might be irrelevant as he can be unwary...
    if ((m_ptr->r_idx == R_IDX_MORGOTH) && p_ptr->on_the_run)
        m_ptr->mflag |= (MFLAG_ACTV);

    // do this before Mastery and Lórien effects kick in...
    maybe_update_morgoth_state_from_hp(m_ptr);

    // assume we are not under the influence of the Song of Mastery
    m_ptr->skip_this_turn = false;

    // first work out if the song of mastery stops the monster's turn
    if (singing(SNG_MASTERY))
    {
        int player_skill = damroll(2, 8) + ability_bonus(S_SNG, SNG_MASTERY);
        int enemy_skill = damroll(2, 10) + monster_skill(m_ptr, S_WIL)
                    + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);

        if (skill_check(PLAYER, player_skill, enemy_skill, m_ptr) > 0)
        {
            // make sure the monster doesn't do any free attacks before its next
            // turn
            m_ptr->skip_this_turn = true;

            // end the monster's turn
            return;
        }
    }

    // deal with monster songs
    if (m_ptr->song != SNG_NOTHING)
    {
        int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
        char m_name[80];

        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

        bool end_song = (m_ptr->mana == 0);
        if (!end_song && (m_ptr->song == SNG_PIERCING))
        {
            if (m_ptr->r_idx == R_IDX_MORGOTH && p_ptr->on_the_run)
            {
                if (morgoth_has_player_track(m_ptr))
                    end_song = true;
            }
            else if (m_ptr->alertness >= ALERTNESS_ALERT)
            {
                end_song = true;
            }
        }

        if (end_song)
        {
            if (m_ptr->ml)
                msg_format("%^s ends his song.", m_name);
            else if (dist <= 30)
                msg_print("The song ends.");
            m_ptr->song = SNG_NOTHING;
        }

        else
        {
            m_ptr->mana--;

            switch (m_ptr->song)
            {
            case (SNG_BINDING):
                song_of_binding(m_ptr);
                break;
            case (SNG_PIERCING):
                song_of_piercing(m_ptr);
                break;
            case (SNG_OATHS):
                song_of_oaths(m_ptr);
                break;
            }
        }
    }

    maybe_start_morgoth_piercing(m_ptr);

    // need to update view if the monster affects light and is close enough
    if ((r_ptr->light != 0) && (m_ptr->cdis < MAX_SIGHT + ABS(r_ptr->light)))
    {
        p_ptr->update |= (PU_UPDATE_VIEW);
    }

    // shuffle along the array of previous actions
    for (i = ACTION_MAX - 1; i > 0; i--)
    {
        m_ptr->previous_action[i] = m_ptr->previous_action[i - 1];
    }
    // put in a default for this turn
    m_ptr->previous_action[0] = ACTION_MISC;

    // unwary but awake monsters can wander around the dungeon
    if (m_ptr->alertness < ALERTNESS_ALERT)
    {
        wander(m_ptr);
        return;
    }

    if (song_disguise_monster_is_fooled(m_ptr))
        return;

    // Update monster flow information
    update_flow(p_ptr->py, p_ptr->px, m_idx);

    /* Calculate the monster's preferred combat range when needed */
    if (m_ptr->min_range == 0)
        find_range(m_ptr);

    // determine if the monster should be active:

    // first, reset the active flag
    m_ptr->mflag &= ~(MFLAG_ACTV);

    // monsters with targets are all active
    if ((m_ptr->target_y) && (m_ptr->target_x))
        m_ptr->mflag |= (MFLAG_ACTV);

    // monsters that are fleeing are active, otherwise they can't get far enough
    // away
    if (m_ptr->stance == STANCE_FLEEING)
        m_ptr->mflag |= (MFLAG_ACTV);

    // Pursuing creatures are always active at the Gates
    if ((r_ptr->level > 17) && (p_ptr->depth == 0))
        m_ptr->mflag |= (MFLAG_ACTV);

    // 'short sighted' monsters are active when the player is *very* close
    if (r_ptr->flags2 & (RF2_SHORT_SIGHTED))
    {
        if (m_ptr->cdis <= 2)
            m_ptr->mflag |= (MFLAG_ACTV);
    }

    // other monsters
    else
    {
        // monsters that can see the player are active
        if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
            m_ptr->mflag |= (MFLAG_ACTV);

        // monsters that can hear the player are active (Sil-y: note this is a
        // rather arbitrary calculation)
        if (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) < 20)
            m_ptr->mflag |= (MFLAG_ACTV);
        // if (flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx) < 20 +
        // monster_skill(m_ptr, S_PER) - stealth_score) m_ptr->mflag |=
        // (MFLAG_ACTV);

        // monsters that can smell the player are active (Sil-y: I don't think
        // this ever happens)
        if (monster_can_smell(m_ptr))
            m_ptr->mflag |= (MFLAG_ACTV);
    }

    /*
     * Special handling if the first turn a monster has after
     * being attacked by the player, but the player is out of sight
     */
    if (m_ptr->mflag & (MFLAG_HIT_BY_RANGED))
    {
        // Monster will be very upset if it can't see the player
        // or if it is in a corridor and can't fire back
        if (((m_ptr->best_range == 1)
                && !(cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_ROOM)))
            || !player_has_los_bold(m_ptr->fy, m_ptr->fx))
        {
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

            /*if smart and has allies, let them know*/
            if ((r_ptr->flags2 & (RF2_SMART))
                && ((r_ptr->flags1 & (RF1_FRIENDS))
                    || (r_ptr->flags1 & (RF1_FRIEND))
                    || (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
                    || (r_ptr->flags1 & (RF1_ESCORT))
                    || (r_ptr->flags1 & (RF1_ESCORTS))
                    || (r_ptr->flags4 & (RF4_SHRIEK))))
            {
                tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);
            }

            /*Monsters with ranged attacks will try to cast a spell*/
            if (r_ptr->freq_ranged)
                m_ptr->mflag |= (MFLAG_ALWAYS_CAST);

            calc_monster_speed(m_ptr->fy, m_ptr->fx);
        }

        /*clear the flag*/
        m_ptr->mflag &= ~(MFLAG_HIT_BY_RANGED);
    }

    /*This if the first turn a monster has after being attacked by the player*/
    if (m_ptr->mflag & (MFLAG_HIT_BY_MELEE))
    {
        /*
         * Monster will be very upset if:
         * (1) if it isn't next to the player on its turn (pillar dance,
         * hack-n-back, etc)
         */
        if (((m_ptr->cdis > 1) && !(m_ptr->mflag & (MFLAG_PUSHED))))
        {
            m_ptr->mflag |= (MFLAG_AGGRESSIVE);

            /*if smart and has allies, let them know*/
            if ((r_ptr->flags2 & (RF2_SMART))
                && ((r_ptr->flags1 & (RF1_FRIENDS))
                    || (r_ptr->flags1 & (RF1_FRIEND))
                    || (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
                    || (r_ptr->flags1 & (RF1_ESCORT))
                    || (r_ptr->flags1 & (RF1_ESCORTS))
                    || (r_ptr->flags4 & (RF4_SHRIEK))))
            {
                tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_AGGRESSIVE);
            }

            /*Monsters with ranged attacks will try to cast a spell*/
            if (r_ptr->freq_ranged)
                m_ptr->mflag |= (MFLAG_ALWAYS_CAST);

            calc_monster_speed(m_ptr->fy, m_ptr->fx);
        }

        /*clear the flags*/
        m_ptr->mflag &= ~(MFLAG_HIT_BY_MELEE);
    }

    // clear CHARGED flag
    if (m_ptr->mflag & (MFLAG_CHARGED))
    {
        m_ptr->mflag &= ~(MFLAG_CHARGED);
    }

    // If a smart monster has sleeping friends and sees player, sometimes shout
    // a warning
    if (one_in_(2) && (r_ptr->flags2 & (RF2_SMART))
        && player_has_los_bold(m_ptr->fy, m_ptr->fx) && has_sleeping_kin(m_ptr))
    {
        /*if part of a pack, let them know*/
        if ((r_ptr->flags1 & (RF1_FRIENDS)) || (r_ptr->flags1 & (RF1_FRIEND))
            || (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
            || (r_ptr->flags1 & (RF1_ESCORT)) || (r_ptr->flags1 & (RF1_ESCORTS))
            || (r_ptr->flags4 & (RF4_SHRIEK)))
        {
            // just make them active, not aggressive
            tell_allies(m_ptr->fy, m_ptr->fx, MFLAG_ACTV);
        }

        calc_monster_speed(m_ptr->fy, m_ptr->fx);
    }

    /*clear the 'pushed' flag*/
    m_ptr->mflag &= ~(MFLAG_PUSHED);

    /* A monster in passive mode will end its turn at this point. */
    if (!(m_ptr->mflag & (MFLAG_ACTV)))
    {
        wander(m_ptr);
        return;
    }

    /* Hack -- Always redraw the current target monster health bar */
    if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
        p_ptr->redraw |= (PR_HEALTHBAR);

    /* Attempt to multiply if able to and allowed */
    if ((r_ptr->flags2 & (RF2_MULTIPLY)) && (mon_cnt < MAX_MONSTERS - 50))
    {
        /* Count the adjacent monsters (including itself) */
        for (k = 0, y = m_ptr->fy - 1; y <= m_ptr->fy + 1; y++)
        {
            for (x = m_ptr->fx - 1; x <= m_ptr->fx + 1; x++)
            {
                /* Check Bounds */
                if (!in_bounds(y, x))
                    continue;

                /* Count monsters */
                if (cave_m_idx[y][x] > 0)
                    k++;
            }
        }

        /* Hack -- multiply slower in crowded areas */
        if ((k <= 3) && (one_in_(k * 8)))
        {
            /* Try to multiply */
            if (reproduce_monster(
                    cave_m_idx[m_ptr->fy][m_ptr->fx], m_ptr->r_idx))
            {
                /* Take note if visible */
                if (m_ptr->ml)
                {
                    l_ptr->flags2 |= (RF2_MULTIPLY);
                }

                /* Multiplying takes energy */
                return;
            }
        }
    }

    /*** Ranged attacks ***/

    /* Monster can cast spells */
    if (r_ptr->freq_ranged)
    {
        chance = get_chance_of_ranged_attack(m_ptr);

        if ((chance) && percent_chance(chance))
        {
            /* Pick a ranged attack */
            choice = choose_ranged_attack(cave_m_idx[m_ptr->fy][m_ptr->fx]);
        }

        /* Selected a ranged attack? */
        if (choice != 0)
        {
            /* Execute said attack */
            make_attack_ranged(m_ptr, choice);

            /* End turn */
            return;
        }
    }

    /*** Movement ***/

    /* Assume no movement */
    ty = 0;
    tx = 0;

    /*
     * Innate semi-random movement.  Monsters adjacent to the character
     * have more chance of just attacking normally.
     */
    if (r_ptr->flags1 & (RF1_RAND_50 | RF1_RAND_25))
    {
        chance = 0;

        /* RAND_25 and RAND_50 are cumulative */
        if (r_ptr->flags1 & (RF1_RAND_25))
        {
            chance += 25;
            if (m_ptr->ml)
                l_ptr->flags1 |= (RF1_RAND_25);
        }
        if (r_ptr->flags1 & (RF1_RAND_50))
        {
            chance += 50;
            if (m_ptr->ml)
                l_ptr->flags1 |= (RF1_RAND_50);
        }

        if (m_ptr->cdis > 1)
        {
            chance /= 2;
        }

        /* Chance of moving randomly */
        if (percent_chance(chance))
            random_move = true;
    }

    /* Monster isn't moving randomly, isn't running away
     * doesn't hear or smell the character
     */
    if (!random_move)
    {
        /*
         * First, monsters who can't cast, are aggressive, and
         * are not afraid just want to charge
         */
        if (m_ptr->stance != STANCE_FLEEING)
        {
            if ((m_ptr->stance == STANCE_AGGRESSIVE) && (!r_ptr->freq_ranged))
            {
                m_ptr->target_y = 0;
                m_ptr->target_x = 0;
            }

            /* Player can see the monster, and it is not afraid */
            if (player_has_los_bold(m_ptr->fy, m_ptr->fx))
            {
                m_ptr->target_y = 0;
                m_ptr->target_x = 0;
            }
        }

        /* Monster has a known target */
        if ((m_ptr->target_y) && (m_ptr->target_x))
            must_use_target = true;
    }

    /*** Find a target to move to ***/

    /* Monster isn't confused, just moving semi-randomly */
    if (random_move)
    {
        int start = rand_int(8);
        bool dummy;

        /* Is the monster scared? */
        if ((!(r_ptr->flags1 & (RF1_NEVER_MOVE)))
            && ((m_ptr->min_range >= FLEE_RANGE)
                || (m_ptr->stance == STANCE_FLEEING)))
        {
            fear = true;
        }

        /* Look at adjacent grids, starting at random. */
        for (i = start; i < 8 + start; i++)
        {
            y = m_ptr->fy + ddy_ddd[i % 8];
            x = m_ptr->fx + ddx_ddd[i % 8];

            /* Accept first passable grid. */
            if (cave_passable_mon(m_ptr, y, x, &dummy) != 0)
            {
                ty = y;
                tx = x;
                break;
            }
        }

        /* No passable grids found */
        if ((ty == 0) && (tx == 0))
            return;

        /* Cannot move, target grid does not contain the character */
        if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && (cave_m_idx[ty][tx] >= 0))
        {
            /* Cannot move */
            return;
        }
    }

    /* Normal movement */
    else
    {
        // *extremely* frightened monsters next to chasms may jump into the void
        if ((m_ptr->stance == STANCE_FLEEING) && (m_ptr->morale < -200)
            && !(r_ptr->flags2 & (RF2_FLYING)) && one_in_(2))
        {
            int chasm_y = 0;
            int chasm_x = 0;

            /* Look at adjacent grids */
            for (i = 0; i < 8; i++)
            {
                int y = m_ptr->fy + ddy_ddd[i];
                int x = m_ptr->fx + ddx_ddd[i];

                /* Check bounds */
                if (!in_bounds(y, x))
                    continue;

                /* Accept a chasm square */
                if (cave_feat[y][x] == FEAT_CHASM)
                {
                    chasm_y = y;
                    chasm_x = x;

                    break;
                }
            }

            if (chasm_y != 0)
            {
                monster_swap(m_ptr->fy, m_ptr->fx, chasm_y, chasm_x);
                return;
            }
        }

        /* Choose a pair of target grids, or cancel the move. */
        if (!get_move(m_ptr, &ty, &tx, &fear, must_use_target))
        {
            return;
        }
    }

    // If the monster thinks its location is optimal...
    if ((ty == m_ptr->fy) && (tx == m_ptr->fx))
    {
        // intelligent monsters that are fleeing can try to use stairs (but not
        // territorial ones)
        if ((r_ptr->flags2 & (RF2_SMART))
            && !(r_ptr->flags2 & (RF2_TERRITORIAL))
            && (m_ptr->stance == STANCE_FLEEING))
        {
            if (cave_stair_bold(m_ptr->fy, m_ptr->fx))
            {
                char m_name[80];

                if (m_ptr->ml)
                {
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
                    if (cave_down_stairs_bold(m_ptr->fy, m_ptr->fx))
                        msg_format("%^s flees down the stairs.", m_name);
                    else
                        msg_format("%^s flees up the stairs.", m_name);
                }

                // if adjacent, you get a chance for an opportunist attack,
                // which might kill them (skip_next_turn is there to stop you
                // getting opportunist attacks afer knocking someone back)
                if (player_active_weapon_is_melee()
                    && p_ptr->active_ability[S_STL][STL_OPPORTUNIST] && m_ptr->ml
                    && !m_ptr->skip_next_turn
                    && (m_ptr->alertness >= ALERTNESS_ALERT) && !p_ptr->truce
                    && !p_ptr->confused && !p_ptr->afraid && !p_ptr->entranced
                    && (p_ptr->stun <= 100))
                {
                    if ((distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px)
                            == 1))
                    {
                        py_attack_aux(m_ptr->fy, m_ptr->fx, ATT_OPPORTUNIST);
                    }
                }

                // removes the monster if it is still alive
                delete_monster(ty, tx);

                return;
            }
        }

        // if the square is non-adjacent to the player, then allow a ranged
        // attack instead of a move
        if ((m_ptr->cdis > 1) && r_ptr->freq_ranged)
        {
            chance = get_chance_of_ranged_attack(m_ptr);

            if ((chance) && percent_chance(chance))
            {
                choice = choose_ranged_attack(cave_m_idx[m_ptr->fy][m_ptr->fx]);
            }

            /* Selected a ranged attack? */
            if (choice != 0)
            {
                /* Execute said attack */
                make_attack_ranged(m_ptr, choice);
            }
        }

        return;
    }

    /* Calculate the actual move.  Cancel move on failure to enter grid. */
    if (!make_move(m_ptr, &ty, &tx, fear, &bash))
        return;

    /* Change terrain, move the monster, handle secondary effects. */
    process_move(m_ptr, ty, tx, bash);

    /* End turn */
    return;
}

/*
 * Produces a cloud if there is one.
 */
void produce_cloud(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    /* Handle any area-effects of the monster - only if active */
    if ((r_ptr->flags2 & (RF2_CLOUD_SURROUND)) && (m_ptr->mflag & (MFLAG_ACTV)))
    {
        /* Assume no affect */
        bool affect = false;

        int typ, dd, ds, rad;

        /* Get information */
        cloud_surround(m_ptr->r_idx, &typ, &dd, &ds, &rad);

        /* Monsters wait for the character to approach and in line of sight */
        if ((m_ptr->cdis <= 5) && (player_can_see_bold(m_ptr->fy, m_ptr->fx)))
            affect = true;

        /* Affect surroundings if appropriate */
        if (affect)
        {
            /* Learn about monster (before visibility changes) */
            if ((m_ptr->ml) && (r_ptr->flags2 & (RF2_CLOUD_SURROUND)))
            {
                l_ptr->flags2 |= (RF2_CLOUD_SURROUND);
            }

            // Sil-y: might want to change difficulty of cloud if I have fear
            // clouds etc
            /* Release of cloud (can affect visibility) */
            if (typ)
                mon_cloud(
                    cave_m_idx[m_ptr->fy][m_ptr->fx], typ, dd, ds, -1, rad);
        }
    }

    return;
}

/*
 *  Calculate the number of monsters of the same type (same letter or RF3 type)
 *  within LOS of a given monster.
 */
int morale_from_friends(monster_type* m_ptr)
{
    int i;
    int fy, fx, y, x;
    int morale_bonus = 0;
    int morale_penalty = 0;

    /* Location of main monster */
    fy = m_ptr->fy;
    fx = m_ptr->fx;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* n_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!n_ptr->r_idx)
            continue;

        /* Location of other monster */
        y = n_ptr->fy;
        x = n_ptr->fx;

        /* Skip self! */
        if ((fy == y) && (fx == x))
            continue;

        // Only consider alert monsters of the same type in line of sight
        if ((n_ptr->alertness >= ALERTNESS_ALERT)
            && similar_monsters(fy, fx, y, x) && los(fy, fx, y, x))
        {
            monster_race* nr_ptr = &r_info[n_ptr->r_idx];
            int multiplier = 1;

            if ((nr_ptr->flags1 & (RF1_ESCORT))
                || (nr_ptr->flags1 & (RF1_ESCORTS)))
                multiplier = 4;

            // add bonus or penalty to morale
            if (n_ptr->stance == STANCE_FLEEING)
                morale_penalty += 10 * multiplier;
            else
                morale_bonus += 10 * multiplier;
        }
    }

    return (morale_bonus - morale_penalty);
}

/*
 * Calculate the morale for a monster.
 */
void calc_morale(monster_type* m_ptr)
{
    int morale;
    int difference;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    s16b this_o_idx, next_o_idx = 0;

    // Starting morale is 60
    morale = 60;

    // Monsters have boosted morale during the endgame
    if (p_ptr->on_the_run)
    {
        morale += 20;
    }

    // Monsters have higher morale if they are usually found deeper than this
    // and vice versa
    else
    {
        morale += (r_ptr->level - p_ptr->depth) * 10;

        // make sure orcs etc in throne room don't have too low morale
        if (p_ptr->depth == MORGOTH_DEPTH)
            morale = MAX(morale, 20);
    }

    // Take player's conditions into account
    if (p_ptr->image)
        morale += 20;
    if (p_ptr->blind)
        morale += 20;
    if (p_ptr->confused)
        morale += 40;
    if (p_ptr->slow)
        morale += 40;
    if (p_ptr->afraid)
        morale += 40;
    if (p_ptr->entranced)
        morale += 80;
    else if (p_ptr->stun > 100)
        morale += 80;
    else if (p_ptr->stun > 50)
        morale += 40;
    else if (p_ptr->stun > 0)
        morale += 20;

    if (!p_ptr->active_ability[S_WIL][WIL_FORMIDABLE])
    {
        // Take player's health into account
        switch (health_level(p_ptr->chp, p_ptr->mhp))
        {
        case HEALTH_WOUNDED:
            morale += 20;
            break; // <= 75% health
        case HEALTH_BADLY_WOUNDED:
            morale += 40;
            break; // <= 50% health
        case HEALTH_ALMOST_DEAD:
            morale += 80;
            break; // <= 25% health
        }
    }

    // Take monster's conditions into account
    if (m_ptr->stunned)
        morale -= 20;
    // skip confusion as it is less good if confused monsters flee
    if (m_ptr->hasted)
        morale += 40;

    // Take monster's health into account
    switch (health_level(m_ptr->hp, m_ptr->maxhp))
    {
    case HEALTH_WOUNDED:
        morale -= 20;
        break; // <= 75% health
    case HEALTH_BADLY_WOUNDED:
        morale -= 40;
        break; // <= 50% health
    case HEALTH_ALMOST_DEAD:
        morale -= 80;
        break; // <= 25% health
    }

    // Extra penalty if <=75% health and already fleeing
    // helps avoid them coming back too quickly
    if ((m_ptr->stance == STANCE_FLEEING)
        && (health_level(m_ptr->hp, m_ptr->maxhp) <= HEALTH_WOUNDED))
    {
        morale -= 20;
    }

    // Get a bonus for non-fleeing friends and a penalty for fleeing ones
    morale += morale_from_friends(m_ptr);

    // Reduce morale for light averse monsters facing a brightly lit player
    if ((r_ptr->flags3 & (RF3_HURT_LITE))
        && (cave_light[p_ptr->py][p_ptr->px] >= 4))
    {
        morale -= (cave_light[p_ptr->py][p_ptr->px] - 3) * 10;
    }

    // Reduce morale for each carried object for non-uniques, so thieves avoid
    // player
    if (!(r_ptr->flags1 & (RF1_UNIQUE)))
    {
        for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
             this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            // Lower morale
            morale -= 20;
        }
    }

    // reduce morale for the Majesty ability
    difference = MAX(p_ptr->skill_use[S_WIL] - monster_skill(m_ptr, S_WIL), 0);
    if (c_info[p_ptr->pcharacter].flags_u & UNQ_WIL_FIN) difference = MAX((3 * p_ptr->skill_use[S_WIL]) / 2 - monster_skill(m_ptr, S_WIL), 0);
    if (p_ptr->active_ability[S_WIL][WIL_MAJESTY])
        morale -= difference / 2 * 10;

    // reduce morale for the Bane ability
    if (p_ptr->active_ability[S_PER][PER_BANE])
        morale -= bane_bonus(m_ptr) * 10;

    // reduce morale for artifact-granted bane
    morale -= artifact_bane_bonus(m_ptr) * 10;

    // increase morale for monster racial bane abilities
    morale += elf_bane_bonus(m_ptr) * 10;
    morale += dwarf_bane_bonus(m_ptr) * 10;
    morale += edain_bane_bonus(m_ptr) * 10;

    // add temporary morale modifiers
    morale += m_ptr->tmp_morale;

    // update the morale
    m_ptr->morale = morale;

    return;
}

/*
 * Calculate the stance for a monster.
 *
 * Based on the monster's morale, type, and other effects.
 *
 * Can be:
 *    STANCE_FLEEING
 *    STANCE_CONFIDENT
 *    STANCE_AGGRESSIVE
 */
void calc_stance(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    int stance;
    int stances[3];

    // set the default stances
    stances[0] = STANCE_FLEEING;
    stances[1] = STANCE_CONFIDENT;
    stances[2] = STANCE_AGGRESSIVE;

    // Some monsters are immune to (non-magical) fear
    if ((r_ptr->flags3 & (RF3_NO_FEAR)) && (m_ptr->tmp_morale >= 0))
    {
        // Wrath means all aggro all the time
        if (p_ptr->aggravate && !(r_ptr->flags2 & (RF2_MINDLESS)))
            stances[0] = STANCE_AGGRESSIVE;
        else
            stances[0] = STANCE_CONFIDENT;
    }

    // Song of Challenge makes monsters attack overconfidently
    if (singing(SNG_CHALLENGE) && m_ptr->morale > 50
        && !(r_ptr->flags3 & (RF3_NO_CONF)))
    {
        stances[1] = STANCE_AGGRESSIVE;
    }

    // Mindless monsters just attack
    if (r_ptr->flags2 & (RF2_MINDLESS))
    {
        stances[0] = STANCE_AGGRESSIVE;
        stances[1] = STANCE_AGGRESSIVE;
    }

    // Trolls are aggressive rather than confident
    if (r_ptr->flags3 & (RF3_TROLL))
    {
        stances[1] = STANCE_AGGRESSIVE;
    }

    // aggravation makes non-mindless things much more hostile
    if (p_ptr->aggravate && !(r_ptr->flags2 & (RF2_MINDLESS)))
    {
        stances[1] = STANCE_AGGRESSIVE;
    }

    // monsters that have been angered have confident turned into aggressive
    if (m_ptr->mflag & (MFLAG_AGGRESSIVE))
    {
        stances[1] = STANCE_AGGRESSIVE;
    }

    // Determine the stance
    if (m_ptr->morale > 200)
        stance = stances[2];
    else if (m_ptr->morale > 0)
        stance = stances[1];
    else
        stance = stances[0];

    // override this for unwary/sleeping monsters
    if (m_ptr->alertness < ALERTNESS_ALERT)
        stance = stances[1];

    // React to changes in stance
    if (stance != m_ptr->stance)
    {
        char m_name[80];
        char buf[160];
        bool message = false;

        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        switch (m_ptr->stance)
        {
        case STANCE_FLEEING:
        {
            // give the monster a temporary 'rally' bonus to its morale
            m_ptr->tmp_morale += 60;
            calc_morale(m_ptr);

            if (!p_ptr->truce)
                sprintf(buf, "turns to fight!");
            else
                sprintf(buf, "recovers its composure.");

            message = true;

            break;
        }
        case STANCE_CONFIDENT:
        {
            if (stance == STANCE_FLEEING)
            {
                // give the monster a temporary 'break' penalty to its morale
                m_ptr->tmp_morale -= 60;
                calc_morale(m_ptr);

                sprintf(buf, "flees in terror!");
                message = true;
            }
            break;
        }
        case STANCE_AGGRESSIVE:
        {
            if (stance == STANCE_FLEEING)
            {
                // give the monster a temporary 'break' penalty to its morale
                m_ptr->tmp_morale -= 60;
                calc_morale(m_ptr);

                sprintf(buf, "flees in terror!");
                message = true;
            }
            break;
        }
        }

        // Inform player of visible changes
        if (message && m_ptr->ml && !(r_ptr->flags1 & (RF1_NEVER_MOVE)))
        {
            msg_format("%^s %s", m_name, buf);
        }

        // force recalculation of range if stance changes
        m_ptr->min_range = 0;
    }

    // update the monster's stance
    m_ptr->stance = stance;
}

/*
 * Monster regeneration of recovery from all temporary
 * conditions.
 *
 * This function is called a lot, and is therefore fairly expensive.
 */
static void recover_monster(monster_type* m_ptr)
{
    bool visible = false;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int i;
    int old_confused = m_ptr->confused;
    int old_stunned = m_ptr->stunned;

    // summoned monsters have a half-life of one turn after the song stops
    if (m_ptr->mflag & (MFLAG_SUMMONED))
    {
        int still_singing = false;

        for (i = 1; i < mon_max; i++)
        {
            monster_type* n_ptr = &mon_list[i];

            /* Skip dead monsters */
            if (!n_ptr->r_idx)
                continue;

            // note if any monster is singing the song of oaths
            if (n_ptr->song == SNG_OATHS)
                still_singing = true;
        }

        if (!still_singing && one_in_(2))
        {
            // removes the monster
            delete_monster(m_ptr->fy, m_ptr->fx);
        }
    }

    /* Visible monsters must be both seen and noticed */
    if (m_ptr->ml)
    {
        visible = true;
    }

    /* produce a cloud if appropriate */
    produce_cloud(m_ptr);

    /* Recover from stuns */
    if (m_ptr->stunned)
    {
        /* Recover somewhat */
        m_ptr->stunned -= 1;

        /* Message if visible */
        if ((m_ptr->stunned == 0) && visible)
        {
            char m_name[80];

            /* Acquire the monster name */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Dump a message */
            msg_format("%^s is no longer stunned.", m_name);
        }
    }

    /* Recover from confusion */
    if (m_ptr->confused)
    {
        /* Recover somewhat */
        m_ptr->confused -= 1;

        /* Message if visible */
        if ((m_ptr->confused == 0) && visible)
        {
            char m_name[80];

            /* Acquire the monster name */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Dump a message */
            msg_format("%^s is no longer confused.", m_name);
        }
    }

    /* Reduce temporary morale modifiers by 10% */
    if (m_ptr->tmp_morale != 0)
    {
        m_ptr->tmp_morale *= 9;
        m_ptr->tmp_morale /= 10;
    }

    /*
     * Handle haste counter
     */
    if (m_ptr->hasted)
    {
        /*efficiency*/
        if (m_ptr->hasted > 1)
            m_ptr->hasted -= 1;

        /*set to 0 and give message*/
        else
            set_monster_haste(cave_m_idx[m_ptr->fy][m_ptr->fx], 0, visible);
    }

    /*
     * Handle slow counter
     */
    if (m_ptr->slowed)
    {
        /*efficiency*/
        if (m_ptr->slowed > 1)
            m_ptr->slowed -= 1;

        /*set to 0 and give message*/
        else
            set_monster_slow(cave_m_idx[m_ptr->fy][m_ptr->fx], 0, visible);
    }

    /* Hack -- Update the health and mana bar (always) */
    if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
        p_ptr->redraw |= (PR_HEALTHBAR);
    if (styled_monster_health_bars && m_ptr->ml
        && (m_ptr->confused != old_confused
            || m_ptr->stunned != old_stunned))
    {
        int current_m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

        p_ptr->window |= PW_MONLIST;
        if (p_ptr->health_who == current_m_idx)
            p_ptr->window |= PW_MONSTER;
        lite_spot(m_ptr->fy, m_ptr->fx);
    }

    // Monsters who are out of sight and fail their perception rolls by 25 or
    // more (15 with Vanish) start to lose track of the player
    if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px)
        && (m_ptr->alertness >= ALERTNESS_ALERT)
        && (m_ptr->stance != STANCE_FLEEING) && (r_ptr->sleep > 0))
    {
        int perception_bonus
            = p_ptr->active_ability[S_STL][STL_VANISH] ? 15 : 25;
        int result
            = skill_check(m_ptr, monster_skill(m_ptr, S_PER) + perception_bonus,
                p_ptr->skill_use[S_STL]
                    + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
                PLAYER);

        if (result < 0)
        {
            set_alertness(
                m_ptr, MAX(m_ptr->alertness + result, ALERTNESS_UNWARY));
        }
    }

    // calculate the monster's morale and stance
    calc_morale(m_ptr);
    calc_stance(m_ptr);
}

/*
 * Process all living monsters, once per game turn.
 *
 * Scan through the list of all living monsters, (backwards, so we can
 * excise any "freshly dead" monsters).
 *
 * Regenerate monsters when it is their turn to move.
 * Allow fully energized monsters to take their turns.*
 * This function and its children are responsible for at least a third of
 * the processor time in normal situations.  If the character is resting,
 * this may rise substantially.
 */
void process_monsters(s16b minimum_energy)
{
    int i;
    monster_type* m_ptr;

    // if time is stopped, no monsters can move
    if (cheat_timestop)
        return;

    /* Process the monsters (backwards) */
    for (i = mon_max - 1; i >= 1; i--)
    {
        /* Player is dead or leaving the current level */
        if (p_ptr->leaving)
            break;

        /* Access the monster */
        m_ptr = &mon_list[i];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Leave monsters without enough energy for later */
        if (m_ptr->energy < minimum_energy)
            continue;

        /* End the turn of monsters without enough energy to move*/
        if (m_ptr->energy < 100)
            continue;

        /* Handle temporary monster attributes */
        recover_monster(m_ptr);

        /* Use up some energy */
        m_ptr->energy -= 100;

        /* Sleeping monsters don't get a move */
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            continue;

        // Monsters who have just noticed you miss their turns (as do those who
        // have been knocked back...)
        if (m_ptr->skip_next_turn)
        {
            // reset its previous movement to stop it charging etc.
            m_ptr->previous_action[0] = ACTION_MISC;

            m_ptr->skip_next_turn = false;
            continue;
        }

        /* Let the monster take its turn */
        process_monster(m_ptr);
    }
}

/*
 * Lets all monsters attempt to notice the player.
 * It can get called multiple times per playerturn.
 * Once each turn is the 'main roll' which is handled differently from the
 * others. The other rolls correspond to noisy events. These events can be
 * caused by the player (in which case 'player_centered' is set to true), or can
 * be caused by a monster, in which case it will be false and FLOW_MONSTER_NOISE
 * will be used instead of the usual FLOW_PLAYER_NOISE.
 */

void monster_perception(bool player_centered, bool main_roll, int difficulty)
{
    int i;
    int m_perception;
    int result;
    int noise_dist;
    monster_type* m_ptr;
    monster_race* r_ptr;
    monster_lore* l_ptr;
    int difficulty_roll;
    int difficulty_roll_alt;

    int combat_noise_bonus = 0;
    int combat_sight_bonus = 0;

    /* Player is dead or leaving the current level */
    if (p_ptr->leaving)
        return;

    // no perception on the first turn of the game
    if (playerturn == 0)
        return;

    // if time is stopped, no monsters can perceive
    if (cheat_timestop)
        return;

    // bonuses for the monster if the player attacked a monster or was attacked
    if (main_roll)
    {
        if (player_attacked)
        {
            combat_noise_bonus += 2;
            combat_sight_bonus += 2;
            player_attacked = false;

            // keep track of this for the ability 'Concentration'
            p_ptr->consecutive_attacks++;
        }
        if (attacked_player)
        {
            combat_noise_bonus += 2;
            combat_sight_bonus += 2;
            attacked_player = false;
        }
    }

    // make the difficulty roll just once per sound source
    // i.e. once per call to this function
    // this is a manual version of a 'skill_check()' and should be treated as
    // such
    difficulty_roll = difficulty + dieroll(10);

    // deal with player curses for skill rolls
    // this is not perfect as some 'player_centered' things are not actually
    // caused by the player
    difficulty_roll_alt = difficulty + dieroll(10);
    if (p_ptr->cursed && player_centered)
        difficulty_roll = MIN(difficulty_roll, difficulty_roll_alt);

    // the song of silence quietens this a bit
    if (singing(SNG_SILENCE))
        difficulty_roll += ability_bonus(S_SNG, SNG_SILENCE);

    /* Process the monsters (backwards) */
    for (i = mon_max - 1; i >= 1; i--)
    {
        /* Access the monster */
        m_ptr = &mon_list[i];

        // Access the race and lore information
        r_ptr = &r_info[m_ptr->r_idx];
        l_ptr = &l_list[m_ptr->r_idx];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* If character is within detection range (unlimited for most monsters,
         * 2 for shortsighted ones) */
        if (!((r_ptr->flags2 & (RF2_SHORT_SIGHTED)) && (m_ptr->cdis > 2)))
        {
            if (player_centered)
            {
                noise_dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
            }
            else
            {
                noise_dist
                    = flow_dist(FLOW_MONSTER_NOISE, m_ptr->fy, m_ptr->fx);
            }

            // start building up the monster's total perception
            m_perception
                = monster_skill(m_ptr, S_PER) - noise_dist + combat_noise_bonus;

            // deal with bane ability (theoretically should modify player roll,
            // but this is equivalent)
            m_perception -= bane_bonus(m_ptr);

            // deal with artifact-granted bane
            m_perception -= artifact_bane_bonus(m_ptr);

            // increase perception for monster racial bane abilities
            m_perception += elf_bane_bonus(m_ptr);
            m_perception += dwarf_bane_bonus(m_ptr);
            m_perception += edain_bane_bonus(m_ptr);

            // monsters are looking more carefully during the escape
            if (p_ptr->on_the_run)
                m_perception += 5;

            // monsters that are already alert get a penalty to the roll to stop
            // them getting *too* alert
            if (m_ptr->alertness >= ALERTNESS_ALERT)
                m_perception -= m_ptr->alertness;

            // aggravation makes non-sleeping monsters much more likely to
            // notice you
            if (p_ptr->aggravate && (m_ptr->alertness >= ALERTNESS_UNWARY)
                && !(r_ptr->flags2 & (RF2_MINDLESS)))
            {
                m_perception += p_ptr->aggravate * 10;
            }

            // awake creatures who have line of sight on player get a bonus
            if (los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px)
                && (m_ptr->alertness >= ALERTNESS_UNWARY))
            {
                bool monster_sees_player = true;

                // Visual recognition check for intelligent monsters
                if (!monster_race_is_vala(m_ptr->r_idx)
                    && visual_recognition && (r_ptr->flags2 & (RF2_SMART)))
                {
                    // Disguise ability reduces monster's effective perception
                    int per_divisor = p_ptr->active_ability[S_STL][STL_DISGUISE] ? 4 : 2;

                    int vision_score = monster_skill(m_ptr, S_PER) / per_divisor
                                     + p_ptr->cur_light
                                     + ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)) ? 2 : 0);

                    monster_sees_player = (vision_score >= m_ptr->cdis);
                }

                if (monster_sees_player)
                {
                    int d, dir, y, x, open_squares = 0;

                    // check adjacent squares for impassable squares
                    for (d = 0; d < 8; d++)
                    {
                        dir = cycle[d];

                        y = p_ptr->py + ddy[dir];
                        x = p_ptr->px + ddx[dir];

                        if (cave_floor_bold(y, x))
                        {
                            open_squares++;
                        }
                    }

                    // Disguise both makes smart monsters less likely to
                    // visually recognize the player and reduces any LOS
                    // sight bonus that still applies.
                    if (p_ptr->active_ability[S_STL][STL_DISGUISE])
                    {
                        m_perception += (open_squares + combat_sight_bonus) / 2;
                    }
                    else
                    {
                        m_perception += open_squares + combat_sight_bonus;
                    }
                }
            }

            // do the 'skill_check()' versus the quietness of the sound...
            result = (m_perception + dieroll(10)) - difficulty_roll;

            /* Debugging message */
            if (cheat_skill_rolls)
            {
                msg_format("{%d+%d v %d+%d = %d}.",
                    result - m_perception + difficulty_roll, m_perception,
                    difficulty_roll - difficulty, difficulty, result);
            }

            if (result > 0)
            {
                // Partly alert monster
                set_alertness(m_ptr, m_ptr->alertness + result);

                /* Still not alert */
                if (m_ptr->alertness < ALERTNESS_ALERT)
                {
                    /* Notice the "not noticing" */
                    if (m_ptr->ml && (l_ptr->ignore < MAX_UCHAR))
                    {
                        l_ptr->ignore++;
                    }
                }

                /* Just became alert */
                else
                {
                    /* Notice the "noticing" */
                    if (m_ptr->ml && (l_ptr->notice < MAX_UCHAR))
                    {
                        l_ptr->notice++;
                    }
                }
            }
        }
    }
}
