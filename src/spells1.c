/* File: spells1.c */

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
#include <math.h>

/*
 * Mega-Hack -- count number of monsters killed out of sight
 */
static int death_count;

static bool song_disguise_active = false;
static byte* song_disguise_seen = NULL;
static byte* song_disguise_pacified = NULL;
static byte* song_disguise_attacked = NULL;
static int song_disguise_seen_count = 0;
static int song_disguise_attackers_current_turn = 0;
static int song_disguise_attackers_last_turn = 0;

static byte song_revealing_hint[MAX_MONSTERS];  // Stores detection result quality
static bool song_revealing_has_data = false;

#define SONG_REVEALING_HINT_TTL 3
#define SONG_REVEALING_FULL_VISIBILITY 10  // Threshold for full visibility

static void ensure_song_disguise_buffers(void)
{
    if (!song_disguise_seen)
    {
        song_disguise_seen = mem_alloc_array(MAX_MONSTERS, byte);
        song_disguise_pacified = mem_alloc_array(MAX_MONSTERS, byte);
        song_disguise_attacked = mem_alloc_array(MAX_MONSTERS, byte);
    }
}

static void song_disguise_clear_pacified(void)
{
    if (!song_disguise_pacified)
        return;

    memset(song_disguise_pacified, 0, MAX_MONSTERS * sizeof(byte));
}

static void song_disguise_on_start(void)
{
    ensure_song_disguise_buffers();
    song_disguise_clear_pacified();
    song_disguise_active = true;
}

static void song_disguise_on_stop(void)
{
    song_disguise_active = false;
    song_disguise_clear_pacified();
    song_disguise_attackers_current_turn = 0;
}

static bool monster_currently_sees_player(const monster_type* m_ptr)
{
    if (m_ptr->alertness < ALERTNESS_ALERT)
        return false;

    if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        return false;

    const monster_race* r_ptr = &r_info[m_ptr->r_idx];
    if (r_ptr->flags1 & RF1_PEACEFUL)
        return false;

    return true;
}

static bool any_monster_observes_player(void)
{
    for (int i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];

        if (!m_ptr->r_idx)
            continue;

        if (monster_currently_sees_player(m_ptr))
            return true;
    }

    return false;
}

static int count_monsters_observing_player(void)
{
    int count = 0;

    for (int i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];

        if (!m_ptr->r_idx)
            continue;

        if (monster_currently_sees_player(m_ptr))
            count++;
    }

    return count;
}

void song_disguise_new_player_turn(void)
{
    ensure_song_disguise_buffers();

    song_disguise_attackers_last_turn = song_disguise_attackers_current_turn;
    song_disguise_attackers_current_turn = 0;

    if (song_disguise_attacked)
        memset(song_disguise_attacked, 0, MAX_MONSTERS * sizeof(byte));
}

void song_disguise_handle_monster_removed(int m_idx)
{
    if (!song_disguise_seen || m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return;

    if (song_disguise_seen[m_idx])
    {
        song_disguise_seen[m_idx] = 0;
        if (song_disguise_seen_count > 0)
            song_disguise_seen_count--;
    }

    if (song_disguise_pacified)
        song_disguise_pacified[m_idx] = 0;
    if (song_disguise_attacked)
        song_disguise_attacked[m_idx] = 0;

    if (m_idx > 0 && m_idx < MAX_MONSTERS)
    {
        song_revealing_hint[m_idx] = 0;

        if (song_revealing_has_data)
        {
            bool any_hint = false;
            for (int i = 1; i < MAX_MONSTERS; i++)
            {
                if (song_revealing_hint[i])
                {
                    any_hint = true;
                    break;
                }
            }

            if (!any_hint)
                song_revealing_has_data = false;
        }
    }
}

void song_disguise_note_monster_attack(int m_idx)
{
    if (m_idx <= 0)
        return;

    ensure_song_disguise_buffers();

    if (!song_disguise_attacked[m_idx])
    {
        song_disguise_attacked[m_idx] = 1;
        song_disguise_attackers_current_turn++;
    }
}

void song_disguise_note_player_attack(int m_idx)
{
    (void)m_idx;

    if (!singing(SNG_DISGUISE))
        return;

    if (p_ptr->song1 == SNG_DISGUISE)
    {
        if (p_ptr->song2 != SNG_NOTHING)
        {
            p_ptr->song1 = p_ptr->song2;
            p_ptr->song2 = SNG_NOTHING;
            msg_print("Your attack breaks your song of disguise.");
        }
        else
        {
            p_ptr->song1 = SNG_NOTHING;
            msg_print("Your attack ends your song of disguise.");
        }
    }
    else if (p_ptr->song2 == SNG_DISGUISE)
    {
        p_ptr->song2 = SNG_NOTHING;
        msg_print("Your attack ends your minor theme of disguise.");
    }

    song_disguise_on_stop();

    p_ptr->redraw |= (PR_SONG);
    p_ptr->update |= (PU_BONUS);
}

static void song_revealing_decay(void)
{
    bool any = false;

    for (int i = 1; i < MAX_MONSTERS; i++)
    {
        if (song_revealing_hint[i] > 0)
        {
            // Decay the detection quality each turn (reduce by ~3-4 points per turn)
            if (song_revealing_hint[i] > 3)
                song_revealing_hint[i] -= 3;
            else
                song_revealing_hint[i] = 0;
        }

        if (song_revealing_hint[i] > 0)
            any = true;
    }

    song_revealing_has_data = any;
}

bool song_revealing_overlay(int m_idx, byte* a, char* c)
{
    if (!song_revealing_has_data)
        return false;

    if (m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return false;

    if (!song_revealing_hint[m_idx])
        return false;

    monster_type* m_ptr = &mon_list[m_idx];

    if (!m_ptr->r_idx || m_ptr->ml)
        return false;

    // If detection quality is high enough, make the monster fully visible
    if (song_revealing_hint[m_idx] > SONG_REVEALING_FULL_VISIBILITY)
    {
        m_ptr->ml = true;
        return false;  // Let normal rendering handle it
    }

    // Otherwise show as a hint marker
    if (graphics_are_ascii())
    {
        int base = 0x30;
        int k = TERM_SLATE;
        byte idx = (byte)(base + k);
        *a = misc_to_attr[idx];
        *c = misc_to_char[idx];
    }
    else
    {
        *a = misc_to_attr[ICON_UNKNOWN_ENEMY];
        *c = misc_to_char[ICON_UNKNOWN_ENEMY];
    }

    return true;
}

#define SONG_DUEL_STACK_LIMIT 3
#define SONG_DUEL_LOCKOUT_TURNS 10

static bool song_is_duel(int song)
{
    return (song == SNG_CONTEST || song == SNG_LAMENT);
}

static void display_synergy_message(int song1, int song2)
{
    // Check if we have a valid synergy pair
    if (song1 == SNG_NOTHING || song2 == SNG_NOTHING)
        return;

    // Define synergy pairs and their messages
    struct {
        int song_a;
        int song_b;
        const char* message;
    } synergies[] = {
        { SNG_ELBERETH, SNG_TREES,
          "The starlight and the Two Trees harmonize in glorious unity!" },
        { SNG_ELBERETH, SNG_STAUNCHING,
          "Starlight and healing blend into a restorative radiance!" },
        { SNG_CHALLENGE, SNG_SLAYING,
          "Your fury and mockery blend into a devastating war-song!" },
        { SNG_DELVINGS, SNG_REVEALING,
          "Stone and secrets resonate together, unveiling all that is hidden!" },
        { SNG_FREEDOM, SNG_ELVENESS,
          "Grace and liberty intertwine in an uplifting melody!" },
        { SNG_STAYING, SNG_CONTEST,
          "Your courage strengthens your voice in the duel of songs!" },
        { SNG_STAYING, SNG_LAMENT,
          "Courage and sorrow unite in a song of enduring strength!" },
        { SNG_SILENCE, SNG_DISGUISE,
          "Quietness and guile weave a cloak of perfect concealment!" },
        { SNG_SILENCE, SNG_LORIEN,
          "Silence and rest deepen into profound tranquility!" },
        { SNG_SHATTERING, SNG_MASTERY,
          "Destruction and dominion unite in overwhelming force!" }
    };

    for (size_t i = 0; i < sizeof(synergies) / sizeof(synergies[0]); i++)
    {
        if ((song1 == synergies[i].song_a && song2 == synergies[i].song_b)
            || (song1 == synergies[i].song_b && song2 == synergies[i].song_a))
        {
            msg_print(synergies[i].message);
            return;
        }
    }
}

static void song_duel_clear_player_target(void)
{
    p_ptr->song_target_idx = 0;
    p_ptr->song_target_song = SNG_NOTHING;
}

static monster_type* song_duel_get_target(int song)
{
    if (!song_is_duel(song))
        return NULL;

    if (p_ptr->song_target_song != song)
        return NULL;

    int m_idx = p_ptr->song_target_idx;
    if (m_idx <= 0 || m_idx >= mon_max)
        return NULL;

    monster_type* m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx)
        return NULL;

    return m_ptr;
}

static void song_duel_reset_player_stack(void)
{
    p_ptr->song_contest_player_stacks = 0;
    p_ptr->song_contest_last_turn = 0;
}

static void song_duel_reset_monster_stack(monster_type* m_ptr, int song)
{
    if (song == SNG_CONTEST)
    {
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_contest_last_turn = 0;
    }
    else if (song == SNG_LAMENT)
    {
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lament_last_turn = 0;
    }
}

static bool song_duel_select_target(int song)
{
    const char* prompt = (song == SNG_CONTEST)
        ? "Choose a foe to challenge with your contest."
        : "Choose a foe to bear the weight of your lament.";

    msg_print(prompt);

    if (!target_set_interactive(TARGET_KILL, 0))
    {
        msg_print("You let the song fade before it finds a target.");
        return false;
    }

    if (!p_ptr->target_set || p_ptr->target_who <= 0)
    {
        msg_print("You let the song fade before it finds a target.");
        return false;
    }

    monster_type* m_ptr = &mon_list[p_ptr->target_who];
    if (!m_ptr->r_idx)
    {
        msg_print("No suitable foe answers your song.");
        return false;
    }

    // Check if this monster has already completed a duel of this type
    if (song == SNG_CONTEST && m_ptr->song_contest_completed)
    {
        msg_print("You have already completed a contest with this foe.");
        return false;
    }
    else if (song == SNG_LAMENT && m_ptr->song_lament_completed)
    {
        msg_print("You have already sung your lament against this foe.");
        return false;
    }

    song_duel_clear_player_target();
    song_duel_reset_player_stack();

    p_ptr->song_target_idx = p_ptr->target_who;
    p_ptr->song_target_song = song;

    if (song == SNG_CONTEST)
    {
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_contest_last_turn = playerturn;
    }
    else
    {
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lament_last_turn = playerturn;
    }

    // Wake up and alert the monster - it notices the song directed at it
    set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
    update_mon(p_ptr->target_who, false);

    return true;
}

static void song_duel_reduce_monster_hp(monster_type* m_ptr, int steps)
{
    if (steps <= 0)
        return;

    int old_maxhp = m_ptr->maxhp;
    if (old_maxhp <= 0)
        old_maxhp = 1;

    int new_maxhp = old_maxhp;

    for (int i = 0; i < steps; i++)
    {
        new_maxhp = (new_maxhp * 10 + 11) / 12;
        if (new_maxhp < 1)
        {
            new_maxhp = 1;
            break;
        }
    }

    if (new_maxhp < 1)
        new_maxhp = 1;

    if (new_maxhp < m_ptr->maxhp)
    {
        long scaled = (long)m_ptr->hp * new_maxhp;
        m_ptr->hp = (int)(scaled / old_maxhp);
        if (m_ptr->hp < 1)
            m_ptr->hp = 1;
        if (m_ptr->hp > new_maxhp)
            m_ptr->hp = new_maxhp;

        int hp_loss = m_ptr->maxhp - new_maxhp;
        if (hp_loss > 0)
            monster_add_song_hp_loss(m_ptr, hp_loss);

        m_ptr->maxhp = new_maxhp;

        /* Morgoth's anger state depends on current HP% (and maxHP can change here). */
        maybe_update_morgoth_state_from_hp(m_ptr);
    }
}

static void song_duel_reduce_monster_damage_dice(monster_type* m_ptr, int penalty)
{
    if (penalty <= 0)
        return;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    for (int b = 0; b < MONSTER_BLOW_MAX; b++)
    {
        if (!r_ptr->blow[b].method)
            continue;

        int max_reduction = (r_ptr->blow[b].dd > 1) ? (r_ptr->blow[b].dd - 1) : 0;
        if (max_reduction <= 0)
            continue;

        int total = m_ptr->blow_dd_reduction[b] + penalty;
        if (total > max_reduction)
            total = max_reduction;
        m_ptr->blow_dd_reduction[b] = (byte)total;
    }
}

static void song_duel_apply_lament_penalties(monster_type* m_ptr, int song_skill)
{
    int will_penalty = MAX(1, song_skill / 2);
    int con_penalty = MAX(1, song_skill / 12);

    m_ptr->song_will_penalty += will_penalty;

    song_duel_reduce_monster_hp(m_ptr, con_penalty);
    song_duel_reduce_monster_damage_dice(m_ptr, con_penalty);
}

static void song_duel_apply_contest_penalties(monster_type* m_ptr, int song_skill)
{
    int will_penalty = MAX(1, song_skill / 3);
    int stealth_penalty = MAX(1, song_skill / 2);
    int evasion_penalty = MAX(1, song_skill / 5);
    int armor_penalty = MAX(1, song_skill / 12);

    m_ptr->song_will_penalty += will_penalty;
    m_ptr->song_stealth_penalty += stealth_penalty;
    m_ptr->song_evasion_penalty += evasion_penalty;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int max_penalty = r_ptr->pd;

    if (armor_penalty > 0)
    {
        int total = m_ptr->song_armor_dice_penalty + armor_penalty;
        if (total > max_penalty)
            total = max_penalty;
        m_ptr->song_armor_dice_penalty = (byte)total;
    }
}

static void song_duel_finish_monster_loss(monster_type* m_ptr, int song, int song_skill)
{
    char m_name[80];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    if (song == SNG_CONTEST)
        msg_format("Your contest overwhelms %s!", m_name);
    else
        msg_format("%s succumbs to your lament!", m_name);

    if (song == SNG_CONTEST)
        song_duel_apply_contest_penalties(m_ptr, song_skill);
    else
    {
        song_duel_apply_lament_penalties(m_ptr, song_skill);
        // Song of Lament always drains Grace - no resistance
        if (dec_stat(A_GRA, 1, false))
            msg_print("You feel drained.");
    }

    m_ptr->song = SNG_NOTHING;
    m_ptr->song_lockout_timer = SONG_DUEL_LOCKOUT_TURNS;

    // Mark this duel as completed so it can't be re-targeted
    if (song == SNG_CONTEST)
        m_ptr->song_contest_completed = 1;
    else
        m_ptr->song_lament_completed = 1;

    song_duel_reset_monster_stack(m_ptr, SNG_CONTEST);
    song_duel_reset_monster_stack(m_ptr, SNG_LAMENT);

    song_duel_clear_player_target();
    song_duel_reset_player_stack();

    p_ptr->song_lockout_timer = SONG_DUEL_LOCKOUT_TURNS;

    change_song(SNG_NOTHING);
}

static void song_duel_finish_player_loss(int song, monster_type* m_ptr)
{
    msg_print("You can no longer sustain the song.");

    // Mark this duel as completed so it can't be re-targeted
    if (m_ptr)
    {
        if (song == SNG_CONTEST)
            m_ptr->song_contest_completed = 1;
        else if (song == SNG_LAMENT)
            m_ptr->song_lament_completed = 1;
    }

    song_duel_clear_player_target();
    song_duel_reset_player_stack();

    p_ptr->song_lockout_timer = SONG_DUEL_LOCKOUT_TURNS;

    if (song == SNG_CONTEST)
    {
        // Song of Contest always drains a random stat - no resistance
        int stat = rand_int(A_MAX);
        static cptr desc_stat_neg[] = { "weak", "awkward", "sickly", "drained" };
        if (dec_stat(stat, 1, false))
            msg_format("You feel %s.", desc_stat_neg[stat]);
    }

    change_song(SNG_NOTHING);
}

static bool song_duel_process_contest(int song_skill)
{
    monster_type* m_ptr = song_duel_get_target(SNG_CONTEST);
    if (!m_ptr)
    {
        msg_print("Your contest has no opponent.");
        change_song(SNG_NOTHING);
        return false;
    }

    char m_name[80];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    int player_skill = song_skill + (p_ptr->skill_use[S_WIL] / 2);
    int monster_will = monster_skill(m_ptr, S_WIL);

    int result = skill_check(PLAYER, player_skill, monster_will, m_ptr);

    if (result > 0)
    {
        p_ptr->song_contest_player_stacks = 0;
        p_ptr->song_contest_last_turn = playerturn;

        if (m_ptr->song_contest_stacks < SONG_DUEL_STACK_LIMIT)
            m_ptr->song_contest_stacks++;
        m_ptr->song_contest_last_turn = playerturn;

        if (m_ptr->ml)
            msg_format("%s falters under your contest. (%d/%d)", m_name,
                m_ptr->song_contest_stacks, SONG_DUEL_STACK_LIMIT);
        else
            msg_print("You press your advantage in the contest.");

        if (m_ptr->song_contest_stacks >= SONG_DUEL_STACK_LIMIT)
        {
            song_duel_finish_monster_loss(m_ptr, SNG_CONTEST, song_skill);
            return false;
        }
    }
    else if (result < 0)
    {
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_contest_last_turn = playerturn;

        if (p_ptr->song_contest_player_stacks < SONG_DUEL_STACK_LIMIT)
            p_ptr->song_contest_player_stacks++;
        p_ptr->song_contest_last_turn = playerturn;

        msg_print("Your foe pushes back against your song.");

        if (p_ptr->song_contest_player_stacks >= SONG_DUEL_STACK_LIMIT)
        {
            song_duel_finish_player_loss(SNG_CONTEST, m_ptr);
            return false;
        }
    }
    else
    {
        msg_print("The contest hangs in the balance.");
    }

    return true;
}

static bool song_duel_process_lament(int song_skill)
{
    monster_type* m_ptr = song_duel_get_target(SNG_LAMENT);
    if (!m_ptr)
    {
        msg_print("Your lament has no audience.");
        change_song(SNG_NOTHING);
        return false;
    }

    char m_name[80];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    int player_skill = song_skill + (p_ptr->skill_use[S_WIL] / 2);
    int monster_will = monster_skill(m_ptr, S_WIL);

    int result = skill_check(PLAYER, player_skill, monster_will, m_ptr);

    if (result > 0)
    {
        if (m_ptr->song_lament_stacks < SONG_DUEL_STACK_LIMIT)
            m_ptr->song_lament_stacks++;
        m_ptr->song_lament_last_turn = playerturn;

        if (m_ptr->ml)
            msg_format("%s reels beneath your lament. (%d/%d)", m_name,
                m_ptr->song_lament_stacks, SONG_DUEL_STACK_LIMIT);
        else
            msg_print("Your lament burdens an unseen foe.");

        if (m_ptr->song_lament_stacks >= SONG_DUEL_STACK_LIMIT)
        {
            song_duel_finish_monster_loss(m_ptr, SNG_LAMENT, song_skill);
            return false;
        }
    }
    else if (result < 0)
    {
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lament_last_turn = playerturn;
        msg_print("Your lament fails to take hold.");
    }
    else
    {
        msg_print("Your lament and the foe's will are evenly matched.");
    }

    return true;
}

void song_duels_new_player_turn(void)
{
    if (p_ptr->song_lockout_timer > 0)
        p_ptr->song_lockout_timer--;

    if (p_ptr->song_contest_player_stacks > 0
        && p_ptr->song_contest_last_turn > 0
        && (playerturn - p_ptr->song_contest_last_turn) >= SONG_DUEL_LOCKOUT_TURNS)
    {
        song_duel_reset_player_stack();
    }

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;

        if (m_ptr->song_lockout_timer > 0)
            m_ptr->song_lockout_timer--;

        if (m_ptr->song_contest_stacks > 0
            && m_ptr->song_contest_last_turn > 0
            && (playerturn - m_ptr->song_contest_last_turn) >= SONG_DUEL_LOCKOUT_TURNS)
        {
            m_ptr->song_contest_stacks = 0;
            m_ptr->song_contest_last_turn = 0;
        }

        if (m_ptr->song_lament_stacks > 0
            && m_ptr->song_lament_last_turn > 0
            && (playerturn - m_ptr->song_lament_last_turn) >= SONG_DUEL_LOCKOUT_TURNS)
        {
            m_ptr->song_lament_stacks = 0;
            m_ptr->song_lament_last_turn = 0;
        }
    }
}

void song_duels_handle_monster_removed(int m_idx)
{
    if (p_ptr->song_target_idx == m_idx)
    {
        song_duel_clear_player_target();
        song_duel_reset_player_stack();
    }
}

bool song_disguise_monster_is_fooled(const monster_type* m_ptr)
{
    if (!song_disguise_active)
        return false;

    if (!song_disguise_pacified)
        return false;

    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    if (m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return false;

    if (!song_disguise_pacified[m_idx])
        return false;

    if (!monster_currently_sees_player(m_ptr))
        return false;

    return true;
}

static void sing_song_of_disguise(int score)
{
    ensure_song_disguise_buffers();

    song_disguise_clear_pacified();

    int player_skill = score + p_ptr->skill_use[S_WIL];
    int observer_penalty = count_monsters_observing_player();

    // Turgon's unique: Shadow Walker - add Perception to the check
    if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_TURGON)
    {
        player_skill += p_ptr->skill_use[S_PER];
    }

    for (int i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];
        char m_name[80];

        if (!m_ptr->r_idx)
            continue;

        if (!monster_currently_sees_player(m_ptr))
            continue;

        int difficulty = monster_skill(m_ptr, S_WIL)
            + monster_skill(m_ptr, S_PER);

        difficulty += observer_penalty;

        if (m_ptr->cdis > 1)
            difficulty -= (m_ptr->cdis - 1);

        if (difficulty < 0)
            difficulty = 0;

        int m_idx = i;

        int other_watchers = song_disguise_seen_count;
        if (song_disguise_seen[m_idx])
            other_watchers--;
        if (other_watchers > 0)
            difficulty += other_watchers * 5;

        if (song_disguise_attackers_last_turn > 0)
            difficulty += song_disguise_attackers_last_turn * 5;

        if (song_disguise_seen[m_idx])
            difficulty += 10;

        int result = skill_check(
            PLAYER, player_skill, difficulty, m_ptr);

        if (result > 0)
        {
            song_disguise_pacified[m_idx] = 1;
            if (song_disguise_seen[m_idx])
            {
                song_disguise_seen[m_idx] = 0;
                if (song_disguise_seen_count > 0)
                    song_disguise_seen_count--;
            }
        }
        else
        {
            if (!song_disguise_seen[m_idx])
            {
                monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                msg_format("%^s sees through your disguise.", m_name);
                song_disguise_seen[m_idx] = 1;
                song_disguise_seen_count++;
            }
        }
    }
}

/*
 * Teleport a monster, normally up to "dis" grids away.
 *
 * Attempt to move the monster at least "dis/2" grids away.
 *
 * But allow variation to prevent infinite loops.
 */
void teleport_away(int m_idx, int dis)
{
    int ny, nx, oy, ox, d, i, min;

    bool look = true;

    monster_type* m_ptr = &mon_list[m_idx];

    /* Paranoia */
    if (!m_ptr->r_idx)
        return;

    /* Save the old location */
    oy = m_ptr->fy;
    ox = m_ptr->fx;

    /* Minimum distance */
    min = dis / 2;

    /* Look until done */
    while (look)
    {
        /* Verify max distance */
        if (dis > 200)
            dis = 200;

        /* Try several locations */
        for (i = 0; i < 500; i++)
        {
            /* Pick a (possibly illegal) location */
            while (1)
            {
                ny = rand_spread(oy, dis);
                nx = rand_spread(ox, dis);
                d = distance(oy, ox, ny, nx);
                if ((d >= min) && (d <= dis))
                    break;
            }

            /* Ignore illegal locations */
            if (!in_bounds_fully(ny, nx))
                continue;

            /* Require "empty" floor space */
            if (!cave_empty_bold(ny, nx))
                continue;

            /* Hack -- no teleport onto glyph of warding */
            if (cave_glyph(ny, nx))
                continue;

            /* No teleporting into vaults and such */
            /* if (cave_info[ny][nx] & (CAVE_ICKY)) continue; */

            /* This grid looks good */
            look = false;

            /* Stop looking */
            break;
        }

        /* Increase the maximum distance */
        dis = dis * 2;

        /* Decrease the minimum distance */
        min = min / 2;
    }

    /* Sound */
    sound(MSG_TPOTHER);

    /*the monster should re-evaluate their target*/
    m_ptr->target_y = 0;
    m_ptr->target_x = 0;

    /* Swap the monsters */
    monster_swap(oy, ox, ny, nx);
}

/*
 * Teleport the player to a location up to "dis" grids away.
 *
 * If no such spaces are readily available, the distance may increase.
 * Try very hard to move the player at least a quarter that distance.
 */
void teleport_player(int dis)
{
    int x_location_tables[20];
    int y_location_tables[20];
    int spot_counter = 0;

    int py = p_ptr->py;
    int px = p_ptr->px;

    int d, i, min, y, x;

    bool look = true;

    /* Minimum distance */
    min = dis / 2;

    /*guage the dungeon size*/
    d = distance(p_ptr->cur_map_hgt, p_ptr->cur_map_wid, 0, 0);

    /*first start with a realistic range*/
    if (dis > d)
        dis = d;

    /*must have a realistic minimum*/
    if (min > (d * 4 / 10))
    {
        min = (d * 4 / 10);
    }

    /* Look until done */
    while (look)
    {
        /*find the allowable range*/
        int min_y = MAX((py - dis), 0);
        int min_x = MAX((px - dis), 0);
        int max_y = MIN((py + dis), (p_ptr->cur_map_hgt - 1));
        int max_x = MIN((px + dis), (p_ptr->cur_map_wid - 1));

        /* Try several locations */
        for (i = 0; i < 10000; i++)
        {
            /* Pick a (possibly illegal) location */
            y = rand_range(min_y, max_y);
            x = rand_range(min_x, max_x);
            d = distance(py, px, y, x);
            if ((d <= min) || (d >= dis))
                continue;

            /*only open floor space*/
            if (!cave_naked_bold(y, x))
                continue;

            /* No teleporting into vaults and such */
            // if (cave_info[y][x] & (CAVE_ICKY)) continue;

            /*don't go over size of array*/
            if (spot_counter < 20)
            {
                x_location_tables[spot_counter] = x;
                y_location_tables[spot_counter] = y;

                /*increase the counter*/
                spot_counter++;
            }

            /*we have enough spots, keep looking*/
            if (spot_counter == 20)
            {
                /* This grid looks good */
                look = false;

                /* Stop looking */
                break;
            }
        }

        /*we have enough random spots*/
        if (spot_counter > 3)
            break;

        /* Increase the maximum distance */
        dis = dis * 2;

        /* Decrease the minimum distance */
        min = min * 6 / 10;
    }

    i = rand_int(spot_counter);

    /* Mark the location */
    x = x_location_tables[i];
    y = y_location_tables[i];

    /* Sound */
    sound(MSG_TELEPORT);

    /* Move player */
    monster_swap(py, px, y, x);

    /* Handle stuff XXX XXX XXX */
    handle_stuff();
}

/*
 * Teleport player to a grid near the given location
 *
 * This function is slightly obsessive about correctness.
 * This function allows teleporting into vaults (!)
 */
void teleport_player_to(int ny, int nx)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    int dis = 0, ctr = 0;

    /* Initialize */
    y = py;
    x = px;

    /* Find a usable location */
    while (1)
    {
        /* Pick a nearby legal location */
        while (1)
        {
            y = rand_spread(ny, dis);
            x = rand_spread(nx, dis);
            if (in_bounds_fully(y, x))
                break;
        }

        /* Accept "naked" floor grids */
        if (cave_naked_bold(y, x))
            break;

        /* Occasionally advance the distance */
        if (++ctr > (4 * dis * dis + 4 * dis + 1))
        {
            ctr = 0;
            dis++;
        }
    }

    /* Sound */
    sound(MSG_TELEPORT);

    /* Move player */
    monster_swap(py, px, y, x);

    /* Handle stuff XXX XXX XXX */
    handle_stuff();
}

/*
 * Teleport monster to a grid near the given location.  This function is
 * used in the monster spell "TELE_SELF_TO", to allow monsters both to
 * suddenly jump near the character, and to make them "dance" around the
 * character.
 *
 * Usually, monster will teleport to a grid that is not more than 4
 * squares away from the given location, and not adjacent to the given
 * location.  These restrictions are relaxed if necessary.
 *
 * This function allows teleporting into vaults.
 */
void teleport_towards(int oy, int ox, int ny, int nx)
{
    int y, x;

    int dist;
    int ctr = 0;
    int min = 2, max = 4;

    /* Find a usable location */
    while (true)
    {
        /* Pick a nearby legal location */
        while (true)
        {
            y = rand_spread(ny, max);
            x = rand_spread(nx, max);
            if (in_bounds_fully(y, x))
                break;
        }

        /* Consider all empty grids */
        if (cave_empty_bold(y, x))
        {
            /*Don't allow monster to teleport onto glyphs*/
            if (cave_glyph(y, x))
                continue;

            /* Calculate distance between target and current grid */
            dist = distance(ny, nx, y, x);

            /* Accept grids that are the right distance away. */
            if ((dist >= min) && (dist <= max))
                break;
        }

        /* Occasionally relax the constraints */
        if (++ctr > 15)
        {
            ctr = 0;

            max++;
            if (max > 5)
                min = 0;
        }
    }

    /* Sound (assumes monster is moving) */
    sound(MSG_TPOTHER);

    /* Move monster */
    monster_swap(oy, ox, y, x);

    /* Handle stuff XXX XXX XXX */
    handle_stuff();
}

/*
 * Teleport the player one level up or down (random when legal)
 */
void teleport_player_level()
{
    bool go_up = false;
    bool go_down = false;

    if (birth_ironman)
    {
        msg_print("Nothing happens.");
        return;
    }

    if (!p_ptr->depth)
        go_down = true;

    /*
     * the bottom of the dungeon.
     */
    if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        go_up = true;
    }

    /*
     * the surface.
     */
    if (p_ptr->depth == 0)
    {
        go_down = true;
    }

    /*We don't have a direction yet, pick one at random*/
    if ((!go_up) && (!go_down))
    {
        if (one_in_(2))
            go_up = true;
        else
            go_down = true;
    }

    /*up*/
    if (go_up == true)
    {
        message(MSG_TPLEVEL, 0, "You rise up through the ceiling.");

        varda_quest_fail_if_bastion_missed();

        // make a note if the player loses a greater vault
        note_lost_greater_vault();

        /* New depth */
        p_ptr->depth--;

        /* Leaving */
        p_ptr->leaving = true;
    }

    else
    {
        message(MSG_TPLEVEL, 0, "You sink through the floor.");

        varda_quest_fail_if_bastion_missed();

        // make a note if the player loses a greater vault
        note_lost_greater_vault();

        /* New depth */
        p_ptr->depth++;

        /* Leaving */
        p_ptr->leaving = true;
    }
}

/*
 * Stuns a monster, making sure not to overflow the stun counter.
 */
void stun_monster(monster_type* m_ptr, int stun)
{
    int new_stun = m_ptr->stunned + stun;
    m_ptr->stunned = MIN(new_stun, 255);
}

/*
 * Return a color to use for the bolt/ball spells
 */
static byte spell_color(int type)
{
    /* Analyze */
    switch (type)
    {
    case GF_ARROW:
        return (TERM_L_UMBER);
    case GF_BOULDER:
        return (TERM_SLATE);
    case GF_ACID:
        return (TERM_SLATE);
    case GF_ELEC:
        return (TERM_BLUE);
    case GF_FIRE:
        return (TERM_RED);
    case GF_COLD:
        return (TERM_WHITE);
    case GF_POIS:
        return (TERM_GREEN);
    case GF_CONFUSION:
        return (TERM_L_UMBER);
    case GF_SOUND:
        return (TERM_L_WHITE);
    case GF_LIGHT:
        return (TERM_WHITE);
    case GF_DARK_WEAK:
        return (TERM_L_DARK);
    case GF_DARK:
        return (TERM_L_DARK);
    case GF_IDENTIFY:
        return (TERM_WHITE);
    case GF_EARTHQUAKE:
        return (TERM_SLATE);
    case GF_WEB:
        return (TERM_L_UMBER);
    }

    /* Standard "color" */
    return (TERM_L_WHITE);
}

/*
 * Find the attr/char pair to use for a spell effect
 *
 * It is moving (or has moved) from (x,y) to (nx,ny).
 *
 * If the distance is not "one", we (may) return "*".
 */
u16b bolt_pict(int y, int x, int ny, int nx, int typ)
{
    int base;

    byte k;

    byte a;
    char c;

    /* No motion (*) */
    if ((ny == y) && (nx == x))
        base = 0x30;

    /* Vertical (|) */
    else if (nx == x)
        base = 0x40;

    /* Horizontal (-) */
    else if (ny == y)
        base = 0x50;

    /* Diagonal (/) */
    else if ((ny - y) == (x - nx))
        base = 0x60;

    /* Diagonal (\) */
    else if ((ny - y) == (nx - x))
        base = 0x70;

    /* Weird (*) */
    else
        base = 0x30;

    if (typ == GF_LIGHT && use_graphics == GRAPHICS_MICROCHASM)
    {
        a = misc_to_attr[ICON_GLOW];
        c = misc_to_char[ICON_GLOW];
    }
    else
    {
        /* Basic spell color */
        k = spell_color(typ);

        /* Obtain attr/char */
        a = misc_to_attr[base + k];
        c = misc_to_char[base + k];
    }

    /* Create pict */
    return (PICT(a, c));
}

/*
 * Allows items that have the CHEAT_DEATH flag to save the player
 */
void attempt_to_cheat_death(void)
{
    char o_name[80];

    /* Scan the equipment */
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        u32b f1, f2, f3;

        object_type* o_ptr = &inventory[i];
        object_flags(o_ptr, &f1, &f2, &f3);

        /* If player is dead, save them at the cost of the item */
        if (f3 & TR3_CHEAT_DEATH && p_ptr->chp <= 0)
        {
            p_ptr->chp = 1;
            p_ptr->energy += 100;
            set_blind(0);
            set_confused(0);
            set_poisoned(0);
            set_afraid(0);
            set_entranced(0);
            set_image(0);
            set_stun(0);
            set_cut(0);
            set_slow(0);

            /* Get a description */
            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

            msg_format("Your %s breaks into two pieces!", o_name);
            ident_f3(TR3_CHEAT_DEATH, o_ptr);

            inven_item_increase(i, -1);
            inven_item_optimize(i);
        }
    }
}

/*
 * Decreases players hit points and sets death flag if necessary
 *
 * Invulnerability needs to be changed into a "shield" XXX XXX XXX
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
void take_hit(int dam, cptr kb_str)
{
    int old_chp = p_ptr->chp;

    int warning = (p_ptr->mhp * op_ptr->hitpoint_warn / 10);

    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    /* Paranoia */
    if (p_ptr->is_dead)
        return;

    /* Disturb */
    disturb(1, 0);

    /* Hurt the player */
    p_ptr->chp -= dam;

    attempt_to_cheat_death();

    /* Display the hitpoints */
    p_ptr->redraw |= (PR_HP);

    /* Window stuff */
    p_ptr->window |= (PW_PLAYER_0);

    if (p_ptr->chp <= 0)
    {
        /* Hack -- Note death */
        message(MSG_DEATH, 0, "You die.");
        message_flush();

        /* Note cause of death */
        if (p_ptr->image == 0)
        {
            SDL_strlcpy(p_ptr->died_from, kb_str, sizeof(p_ptr->died_from));
        }
        else
        {
            strnfmt(p_ptr->died_from, sizeof(p_ptr->died_from),
                "%s (while hallucinating)", kb_str);
        }

        killer_commit(kb_str);

        /* Note death */
        p_ptr->is_dead = true;

        /* Leaving */
        p_ptr->leaving = true;

        /* Write a note */

        /* Get time */
        (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

        /* Add note */
        SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

        /*killed by */
        sprintf(buf, "Slain by %s.", p_ptr->died_from);

        /* Write message */
        do_cmd_note(buf, p_ptr->depth);

        /* date and time*/
        sprintf(buf, "Died on %s.", long_day);

        /* Write message */
        do_cmd_note(buf, p_ptr->depth);

        SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

        /* Dead */
        return;
    }

    /* Hitpoint warning */
    if (p_ptr->chp < warning)
    {
        /* Hack -- bell on first notice */
        if (old_chp > warning)
        {
            bell("Low hitpoint warning!");
        }

        /* Message */
        message(MSG_HITPOINT_WARN, 0, "*** LOW HITPOINT WARNING! ***");
        message_flush();
    }

    // Cancel entrancement
    set_entranced(0);
}

/*
 * Does a given class of objects (usually) hate acid?
 * Note that acid can either melt or corrode something.
 */
bool hates_acid(const object_type* o_ptr)
{
    /* Analyze the type */
    switch (o_ptr->tval)
    {
    /* Wearable items */
    case TV_ARROW:
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
    }

    /* Staffs are wood */
    case TV_STAFF:
    {
        return (true);
    }

    /* Ouch */
    case TV_CHEST:
    {
        return (true);
    }

    /* Skeleton */
    case TV_SKELETON:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate electricity?
 */
bool hates_elec(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_RING:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate fire?
 * Hafted/Polearm weapons have wooden shafts.
 * Arrows/Bows are mostly wooden.
 */
bool hates_fire(const object_type* o_ptr)
{
    /* Analyze the type */
    switch (o_ptr->tval)
    {
    /* Wearable */
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    {
        return (true);
    }

    /* Chests */
    case TV_CHEST:
    {
        return (true);
    }

    /* Torches */
    case TV_LIGHT:
    {
        if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
            return (true);
        else
            return (false);
    }

    /* Notes burn */
    case TV_NOTE:
    {
        return (true);
    }

    /* Staffs burn */
    case TV_STAFF:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate cold?
 */
bool hates_cold(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_POTION:
    case TV_GEM:
    case TV_FLASK:
    {
        return (true);
    }
    case TV_LIGHT:
    {
        return (o_ptr->sval == SV_LIGHT_LANTERN)
            || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL);
    }
    }

    return (false);
}

/*
 * Melt something
 */
static int set_acid_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_acid(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_ACID))
        return (false);
    return (true);
}

/*
 * Electrical damage
 */
static int set_elec_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_elec(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_ELEC))
        return (false);
    return (true);
}

/*
 * Burn something
 */
static int set_fire_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_fire(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_FIRE))
        return (false);
    return (true);
}

/*
 * Freeze things
 */
static int set_cold_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_cold(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_COLD))
        return (false);
    return (true);
}

/*
 * Loud concussive force shatters fragile carried items like cold does.
 */
static int set_sound_destroy(const object_type* o_ptr)
{
    return hates_cold(o_ptr);
}

/*
 * This seems like a pretty standard "typedef"
 */
typedef int (*inven_func)(const object_type*);

typedef enum elemental_item_candidate_location
{
    ELEMENTAL_CANDIDATE_INVENTORY = 0,
    ELEMENTAL_CANDIDATE_SUPPLY = 1
} elemental_item_candidate_location;

typedef struct elemental_item_candidate
{
    elemental_item_candidate_location location;
    int index;
    object_type* o_ptr;
    long weight;
    int units;
    int unit_size;
    int quantity_per_unit;
} elemental_item_candidate;

typedef struct elemental_item_debug_info
{
    bool enabled;
    int attack_type;
    int raw_dam;
    int min_raw;
    int max_raw;
    int hp_dam;
    double roll_cdf;
    double q_factor;
    double hurt_factor;
    int threshold;
    bool gate_roll_made;
    int gate_roll;
    int candidate_count;
    long total_weight;
    bool candidate_selected;
    int selection_roll;
    elemental_item_candidate_location selected_location;
    int selected_index;
    long selected_weight;
    double slot_factor;
    double material_factor;
    double stack_factor;
    bool acid_roll_made;
    int acid_roll;
    char selected_name[80];
    cptr outcome;
} elemental_item_debug_info;

static cptr elemental_attack_name(int attack_type)
{
    switch (attack_type)
    {
    case GF_ACID:
        return "acid";
    case GF_ELEC:
        return "elec";
    case GF_FIRE:
        return "fire";
    case GF_COLD:
        return "cold";
    case GF_SOUND:
        return "sound";
    default:
        return "unknown";
    }
}

static void elemental_debug_init(elemental_item_debug_info* debug,
    int attack_type, int raw_dam, int min_raw, int max_raw, int hp_dam)
{
    if (!debug)
        return;

    memset(debug, 0, sizeof(*debug));
    debug->enabled = show_elemental_item_rolls;
    debug->attack_type = attack_type;
    debug->raw_dam = raw_dam;
    debug->min_raw = min_raw;
    debug->max_raw = max_raw;
    debug->hp_dam = hp_dam;
}

static void elemental_debug_slot_desc(
    elemental_item_candidate_location location, int index, char* buf,
    size_t buf_size)
{
    if (!buf || (buf_size == 0))
        return;

    if (location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        strnfmt(buf, buf_size, "supplies[%d]", index);
        return;
    }

    if (index < INVEN_PACK)
    {
        strnfmt(buf, buf_size, "pack(%c)", index_to_label(index));
        return;
    }

    switch (index)
    {
    case INVEN_WIELD:
        strnfmt(buf, buf_size, "wield");
        return;
    case INVEN_BOW:
        strnfmt(buf, buf_size, "bow");
        return;
    case INVEN_STAFF:
        strnfmt(buf, buf_size, "staff");
        return;
    case INVEN_LEFT:
        strnfmt(buf, buf_size, "left");
        return;
    case INVEN_RIGHT:
        strnfmt(buf, buf_size, "right");
        return;
    case INVEN_NECK:
        strnfmt(buf, buf_size, "neck");
        return;
    case INVEN_LITE:
        strnfmt(buf, buf_size, "light");
        return;
    case INVEN_BODY:
        strnfmt(buf, buf_size, "body");
        return;
    case INVEN_OUTER:
        strnfmt(buf, buf_size, "outer");
        return;
    case INVEN_ARM:
        strnfmt(buf, buf_size, "arm");
        return;
    case INVEN_HEAD:
        strnfmt(buf, buf_size, "head");
        return;
    case INVEN_HANDS:
        strnfmt(buf, buf_size, "hands");
        return;
    case INVEN_FEET:
        strnfmt(buf, buf_size, "feet");
        return;
    case INVEN_QUIVER1:
        strnfmt(buf, buf_size, "quiver1");
        return;
    case INVEN_QUIVER2:
        strnfmt(buf, buf_size, "quiver2");
        return;
    case INVEN_HORN:
        strnfmt(buf, buf_size, "horn");
        return;
    default:
        strnfmt(buf, buf_size, "slot[%d]", index);
        return;
    }
}

static bool elemental_slot_uses_pack_like_factor(int slot,
    elemental_item_candidate_location location);
static double elemental_item_slot_factor(int slot,
    elemental_item_candidate_location location);
static double elemental_item_material_factor(int attack_type,
    const object_type* o_ptr);
static void elemental_mark_inventory_item_changed(void);

static void elemental_debug_record_candidate(
    elemental_item_debug_info* debug, int attack_type,
    const elemental_item_candidate* candidate, int selection_roll,
    int candidate_count, long total_weight)
{
    double stack_factor = 1.0;

    if (!debug || !candidate || !candidate->o_ptr)
        return;

    debug->candidate_count = candidate_count;
    debug->total_weight = total_weight;
    debug->candidate_selected = true;
    debug->selection_roll = selection_roll;
    debug->selected_location = candidate->location;
    debug->selected_index = candidate->index;
    debug->selected_weight = candidate->weight;
    debug->slot_factor = elemental_item_slot_factor(candidate->index,
        candidate->location);
    debug->material_factor = elemental_item_material_factor(attack_type,
        candidate->o_ptr);

    if (elemental_slot_uses_pack_like_factor(candidate->index,
        candidate->location))
    {
        stack_factor = sqrt((double)MAX(candidate->o_ptr->number, 1));
    }

    debug->stack_factor = stack_factor;
    object_desc(debug->selected_name, sizeof(debug->selected_name),
        candidate->o_ptr, false, 3);
}

static void elemental_debug_emit(const elemental_item_debug_info* debug)
{
    char gate_buf[64];
    char target_buf[256];
    char slot_buf[32];
    char acid_buf[32];
    char buf[768];
    double threshold_pct;
    double candidate_pct = 0.0;

    if (!debug || !debug->enabled || !debug->outcome)
        return;

    if (!debug->gate_roll_made)
    {
        strnfmt(gate_buf, sizeof(gate_buf), "gate=skip");
    }
    else
    {
        strnfmt(gate_buf, sizeof(gate_buf), "gate=%d%s%d",
            debug->gate_roll, (debug->gate_roll < debug->threshold) ? "<" : ">=",
            debug->threshold);
    }

    target_buf[0] = '\0';
    if (debug->candidate_selected && (debug->total_weight > 0))
    {
        elemental_debug_slot_desc(debug->selected_location, debug->selected_index,
            slot_buf, sizeof(slot_buf));
        candidate_pct = ((double)debug->selected_weight * 100.0)
            / (double)debug->total_weight;
        strnfmt(target_buf, sizeof(target_buf),
            " pick=%d/%ld target=%s@%s w=%ld(%.2f%%) sf=%.2f mf=%.2f st=%.2f",
            debug->selection_roll, debug->total_weight, debug->selected_name,
            slot_buf, debug->selected_weight, candidate_pct,
            debug->slot_factor, debug->material_factor, debug->stack_factor);
    }

    acid_buf[0] = '\0';
    if (debug->acid_roll_made)
    {
        strnfmt(acid_buf, sizeof(acid_buf), " acid=%d/2->%s",
            debug->acid_roll + 1,
            (debug->acid_roll == 0) ? "corrode" : "destroy");
    }

    threshold_pct = (double)debug->threshold / 10000.0;
    strnfmt(buf, sizeof(buf),
        "[Elem %s] raw=%d/%d..%d hp=%d cdf=%.1f%% q=%.3f hurt=%.3f chance=%.4f%% %s cand=%d%s%s -> %s",
        elemental_attack_name(debug->attack_type), debug->raw_dam, debug->min_raw,
        debug->max_raw, debug->hp_dam, debug->roll_cdf * 100.0,
        debug->q_factor, debug->hurt_factor,
        threshold_pct, gate_buf, debug->candidate_count, target_buf, acid_buf,
        debug->outcome);
    msg_print(buf);
}

bool elemental_attack_destroys_object(int attack_type, const object_type* o_ptr)
{
    inven_func typ = NULL;

    switch (attack_type)
    {
    case GF_ACID:
        typ = set_acid_destroy;
        break;
    case GF_ELEC:
        typ = set_elec_destroy;
        break;
    case GF_FIRE:
        typ = set_fire_destroy;
        break;
    case GF_COLD:
        typ = set_cold_destroy;
        break;
    case GF_SOUND:
        typ = set_sound_destroy;
        break;
    }

    if (!typ || !o_ptr)
        return false;

    return (*typ)(o_ptr) ? true : false;
}

static bool elemental_attack_can_target_object(int attack_type,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (artefact_p(o_ptr))
        return false;

    if (!elemental_attack_destroys_object(attack_type, o_ptr))
        return false;

    if ((attack_type == GF_FIRE) && object_is_fire_broken(o_ptr))
        return false;

    if (((attack_type == GF_COLD) || (attack_type == GF_SOUND))
        && (o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
        && (object_ego_prefix(o_ptr) == EGO_BROKEN_BRASS_LANTERN))
    {
        return false;
    }

    return true;
}

static double elemental_clamp01(double value)
{
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

static double elemental_linear_damage_percentile(int raw_dam, int min_raw,
    int max_raw)
{
    double percentile;

    if (max_raw < min_raw)
    {
        int tmp = max_raw;
        max_raw = min_raw;
        min_raw = tmp;
    }

    if (max_raw == min_raw)
        return (raw_dam >= max_raw) ? 1.0 : 0.0;

    percentile = ((double)raw_dam - (double)min_raw)
        / ((double)max_raw - (double)min_raw);
    return elemental_clamp01(percentile);
}

static bool elemental_damage_roll_shape(int min_raw, int max_raw, int* dice,
    int* sides)
{
    if (!dice || !sides)
        return false;

    if ((min_raw <= 0) || (max_raw < min_raw))
        return false;

    if ((max_raw % min_raw) != 0)
        return false;

    *dice = min_raw;
    *sides = max_raw / min_raw;
    return (*sides >= 1);
}

/*
 * Use the actual NdS distribution when min/max describe one; otherwise,
 * fall back to the previous linear percentile.
 */
static double elemental_damage_cdf_percentile(int raw_dam, int min_raw,
    int max_raw)
{
    int dice;
    int sides;
    int max_sum;
    int capped_raw;
    int prev_max = 0;
    double cdf = 0.0;
    double* prev;
    double* next;

    if (max_raw < min_raw)
    {
        int tmp = max_raw;
        max_raw = min_raw;
        min_raw = tmp;
    }

    if (raw_dam < min_raw)
        return 0.0;

    if (raw_dam >= max_raw)
        return 1.0;

    if (!elemental_damage_roll_shape(min_raw, max_raw, &dice, &sides))
    {
        return elemental_linear_damage_percentile(raw_dam, min_raw, max_raw);
    }

    if (sides == 1)
        return (raw_dam >= dice) ? 1.0 : 0.0;

    max_sum = dice * sides;
    prev = mem_alloc_array(max_sum + 1, double);
    next = mem_alloc_array(max_sum + 1, double);
    if (!prev || !next)
    {
        prev = mem_free(prev);
        next = mem_free(next);
        return elemental_linear_damage_percentile(raw_dam, min_raw, max_raw);
    }

    prev[0] = 1.0;

    for (int die = 0; die < dice; die++)
    {
        memset(next, 0, (size_t)(max_sum + 1) * sizeof(*next));

        for (int sum = 0; sum <= prev_max; sum++)
        {
            double probability = prev[sum];

            if (probability <= 0.0)
                continue;

            probability /= (double)sides;
            for (int face = 1; face <= sides; face++)
            {
                next[sum + face] += probability;
            }
        }

        {
            double* tmp = prev;
            prev = next;
            next = tmp;
        }

        prev_max += sides;
    }

    capped_raw = MIN(raw_dam, max_sum);
    for (int sum = min_raw; sum <= capped_raw; sum++)
    {
        cdf += prev[sum];
    }

    prev = mem_free(prev);
    next = mem_free(next);
    return elemental_clamp01(cdf);
}

static int elemental_attack_probability_per_million(int raw_dam, int min_raw,
    int max_raw, int hp_dam, elemental_item_debug_info* debug)
{
    double percentile;
    double q;
    double hp = (double)hp_dam;
    const double hurt_scale = 80.0 / 3.0;
    double hurt;
    double chance;
    int threshold;

    if (debug)
    {
        debug->roll_cdf = 0.0;
        debug->q_factor = 0.0;
        debug->hurt_factor = 0.0;
        debug->threshold = 0;
    }

    if (hp_dam <= 0)
        return 0;

    percentile = elemental_damage_cdf_percentile(raw_dam, min_raw, max_raw);
    q = elemental_clamp01((percentile - 0.50) / 0.50);
    hurt = (hp * hp) / ((hp * hp) + (hurt_scale * hurt_scale));
    chance = q * q * hurt;
    threshold = (int)(chance * 1000000.0 + 0.5);

    if (threshold < 0)
        threshold = 0;
    if (threshold > 1000000)
        threshold = 1000000;

    if (debug)
    {
        debug->roll_cdf = percentile;
        debug->q_factor = q;
        debug->hurt_factor = hurt;
        debug->threshold = threshold;
    }

    return threshold;
}

static void elemental_debug_emit_size_summary(int attack_type, int raw_dam,
    int min_raw, int max_raw, int hp_dam, double cdf, double q_squared,
    double hurt, double chance, int total, int remaining, cptr outcome)
{
    char buf1[192];
    char buf2[192];

    if (!show_elemental_item_rolls || !outcome)
        return;

    strnfmt(buf1, sizeof(buf1),
        "[Elem %s] raw=%d/%d..%d hp=%d cdf=%.1f%% q2=%.3f hurt=%.3f chance=%.4f%%",
        elemental_attack_name(attack_type), raw_dam, min_raw, max_raw, hp_dam,
        cdf * 100.0, q_squared, hurt, chance * 100.0);
    strnfmt(buf2, sizeof(buf2), "[Elem %s] total=%d remaining=%d -> %s",
        elemental_attack_name(attack_type), total, remaining, outcome);
    msg_print(buf1);
    msg_print(buf2);
}

static bool elemental_attack_matches_object_material(int attack_type,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (attack_type)
    {
    case GF_ACID:
        return hates_acid(o_ptr);
    case GF_ELEC:
        return hates_elec(o_ptr);
    case GF_FIRE:
        return hates_fire(o_ptr);
    case GF_COLD:
        return hates_cold(o_ptr);
    default:
        return false;
    }
}

static bool elemental_attack_allows_size_location(int attack_type,
    elemental_item_candidate_location location, int index,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (attack_type)
    {
    case GF_ACID:
    case GF_ELEC:
    case GF_FIRE:
        return (location == ELEMENTAL_CANDIDATE_INVENTORY)
            && (index >= INVEN_WIELD) && (index < INVEN_TOTAL);

    case GF_COLD:
        if (location == ELEMENTAL_CANDIDATE_SUPPLY)
            return o_ptr->tval != TV_LIGHT;

        return (location == ELEMENTAL_CANDIDATE_INVENTORY)
            && (index == INVEN_LITE);

    default:
        return false;
    }
}

static bool elemental_object_has_attack_resistance(const object_type* o_ptr,
    int attack_type)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f4;

    switch (attack_type)
    {
    case GF_ACID:
        return (f3 & TR3_IGNORE_ACID) ? true : false;
    case GF_ELEC:
        return ((f2 & TR2_RES_ELEC) || (f3 & TR3_IGNORE_ELEC)) ? true : false;
    case GF_FIRE:
        return ((f2 & TR2_RES_FIRE) || (f3 & TR3_IGNORE_FIRE)) ? true : false;
    case GF_COLD:
        return ((f2 & TR2_RES_COLD) || (f3 & TR3_IGNORE_COLD)) ? true : false;
    default:
        return false;
    }
}

static bool elemental_shield_has_attack_protection(const object_type* o_ptr,
    int attack_type)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;

    switch (attack_type)
    {
    case GF_ACID:
        return (f3 & TR3_IGNORE_ACID) ? true : false;
    case GF_ELEC:
        return (f3 & TR3_IGNORE_ELEC) ? true : false;
    case GF_FIRE:
        return ((f4 & TR4_PROT_FIRE) || (f3 & TR3_IGNORE_FIRE)) ? true : false;
    case GF_COLD:
        return ((f4 & TR4_PROT_COLD) || (f3 & TR3_IGNORE_COLD)) ? true : false;
    default:
        return false;
    }
}

static int elemental_shield_block_base(const object_type* o_ptr)
{
    int chance = 0;
    byte ego_idx;

    if (!o_ptr || !o_ptr->k_idx || (o_ptr->tval != TV_SHIELD))
        return 0;

    chance += k_info[o_ptr->k_idx].elemental_block;

    if (o_ptr->name1)
        chance += a_info[o_ptr->name1].elemental_block;

    ego_idx = object_ego_prefix(o_ptr);
    if (ego_idx)
        chance += e_info[ego_idx].elemental_block;

    ego_idx = object_ego_suffix(o_ptr);
    if (ego_idx)
        chance += e_info[ego_idx].elemental_block;

    return chance;
}

static int elemental_shield_block_chance(const object_type* o_ptr,
    int attack_type)
{
    int chance = elemental_shield_block_base(o_ptr);

    if (!o_ptr || !o_ptr->k_idx || (o_ptr->tval != TV_SHIELD))
        return 0;

    if (elemental_object_has_attack_resistance(o_ptr, attack_type))
        chance += 25;

    if (blocking_bonus_active())
        chance += 25;

    if (chance < 0)
        chance = 0;
    if (chance > 100)
        chance = 100;

    return chance;
}

static object_type* elemental_equipped_shield(void)
{
    object_type* o_ptr = &inventory[INVEN_ARM];

    if (!o_ptr->k_idx || (o_ptr->tval != TV_SHIELD))
        return NULL;

    return o_ptr;
}

static bool elemental_damage_blocking_shield(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || (o_ptr->ps <= 0))
        return false;

    o_ptr->ps--;
    elemental_mark_inventory_item_changed();
    return true;
}

static int elemental_item_unit_size(const object_type* o_ptr,
    elemental_item_candidate_location location, int index)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f4;

    if ((location == ELEMENTAL_CANDIDATE_INVENTORY)
        && (index >= INVEN_WIELD) && (index < INVEN_TOTAL)
        && ((f3 & TR3_THROWING) || (index == INVEN_QUIVER1)
            || (index == INVEN_QUIVER2)))
    {
        return 1;
    }

    switch (o_ptr->tval)
    {
    case TV_ARROW:
        return 1;
    case TV_SHIELD:
        return 2;
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
        return 2;
    case TV_BOW:
        return 2;
    case TV_STAFF:
        return 1;
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
        return 1;
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return 2;
    case TV_POTION:
    case TV_GEM:
        return 1;
    case TV_LIGHT:
        return ((o_ptr->sval == SV_LIGHT_TORCH)
            || (o_ptr->sval == SV_LIGHT_MALLORN))
            ? 1
            : 2;
    default:
        return 1;
    }
}

static int elemental_item_unit_count(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || (o_ptr->number <= 0))
        return 0;

    if (o_ptr->tval == TV_ARROW)
        return (o_ptr->number + 11) / 12;

    return o_ptr->number;
}

static int elemental_item_quantity_per_unit(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || (o_ptr->number <= 0))
        return 0;

    if (o_ptr->tval == TV_ARROW)
        return MIN(o_ptr->number, 12);

    return 1;
}

static void elemental_describe_quantity(char* buf, size_t buf_size,
    const elemental_item_candidate* candidate, int amount)
{
    object_type desc_obj;

    if (!buf || (buf_size == 0) || !candidate || !candidate->o_ptr)
        return;

    object_copy(&desc_obj, candidate->o_ptr);
    desc_obj.number = (byte)amount;
    object_desc(buf, buf_size, &desc_obj, false, 3);
}

static void elemental_message_amount(const elemental_item_candidate* candidate,
    int original_number, int amount, cptr o_name, cptr singular_action,
    cptr plural_action)
{
    cptr owner;
    cptr action = (amount > 1) ? plural_action : singular_action;

    if (original_number > 1)
    {
        if (amount >= original_number)
            owner = "All of your";
        else if (amount > 1)
            owner = "Some of your";
        else
            owner = "One of your";
    }
    else
    {
        owner = "Your";
    }

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        msg_format("%s %s in your supplies %s", owner, o_name, action);
    }
    else if (candidate->index < INVEN_PACK)
    {
        msg_format("%s %s (%c) %s", owner, o_name,
            index_to_label(candidate->index), action);
    }
    else
    {
        msg_format("%s %s %s", owner, o_name, action);
    }
}

static void elemental_remove_quantity_from_candidate(
    const elemental_item_candidate* candidate, int amount)
{
    object_type* o_ptr;
    int total_before;
    int charges_lost;

    if (!candidate || !candidate->o_ptr || (amount <= 0))
        return;

    o_ptr = candidate->o_ptr;
    amount = MIN(amount, o_ptr->number);

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        (void)supplies_consume_quantity(candidate->index, amount);
        return;
    }

    if (((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN))
        && (o_ptr->number > amount))
    {
        total_before = o_ptr->number;
        charges_lost = (o_ptr->pval * amount) / total_before;
        if ((charges_lost <= 0) && (o_ptr->pval > 0))
            charges_lost = 1;
        if (charges_lost > o_ptr->pval)
            charges_lost = o_ptr->pval;
        o_ptr->pval -= charges_lost;
    }

    inven_item_increase(candidate->index, -amount);
    inven_item_optimize(candidate->index);
}

static void elemental_destroy_candidate_quantity(
    const elemental_item_candidate* candidate, int attack_type, int amount)
{
    char o_name[80];
    int original_number;

    if (!candidate || !candidate->o_ptr || (amount <= 0))
        return;

    original_number = candidate->o_ptr->number;
    amount = MIN(amount, original_number);
    elemental_describe_quantity(o_name, sizeof(o_name), candidate, amount);

    if ((candidate->o_ptr->tval == TV_CHEST) && (amount > 0))
    {
        chest_release_contents(candidate->o_ptr, p_ptr->py, p_ptr->px,
            attack_type);
    }

    elemental_remove_quantity_from_candidate(candidate, amount);
    elemental_message_amount(candidate, original_number, amount, o_name,
        "was destroyed!", "were destroyed!");
}

static int elemental_damage_quota_divisor(int attack_type)
{
    switch (attack_type)
    {
    case GF_ACID:
        return 20;
    case GF_ELEC:
        return 10;
    case GF_FIRE:
        return 10;
    case GF_COLD:
        return 5;
    default:
        return 0;
    }
}

static int elemental_damage_total(int attack_type, int hp_dam)
{
    int divisor = elemental_damage_quota_divisor(attack_type);
    int groups;
    int total = 1;

    if ((hp_dam <= 0) || (divisor <= 0))
        return 0;

    groups = hp_dam / divisor;
    if (groups < 1)
        groups = 1;

    for (int i = 2; i <= groups; i++)
    {
        if (one_in_(i))
            total++;
    }

    return total;
}

static bool elemental_select_size_candidate(int attack_type, int total,
    elemental_item_candidate* out, int* candidate_count, long* total_units,
    int* selection_roll)
{
    int supply_count = supplies_entry_count();
    int capacity = INVEN_TOTAL + supply_count;
    elemental_item_candidate* candidates;
    int count = 0;
    long available_units = 0;
    int pick;
    int allowed_size = total;

retry_with_size:

    if (!out || (capacity <= 0) || (total <= 0))
        return false;

    candidates = mem_alloc_array(capacity, elemental_item_candidate);
    if (!candidates)
        return false;

    for (int slot = 0; slot < INVEN_TOTAL; slot++)
    {
        object_type* o_ptr = &inventory[slot];
        int unit_size;
        int units;

        if (!o_ptr->k_idx)
            continue;

        if (!elemental_attack_allows_size_location(attack_type,
                ELEMENTAL_CANDIDATE_INVENTORY, slot, o_ptr))
            continue;

        if (!elemental_attack_matches_object_material(attack_type, o_ptr))
            continue;

        unit_size = elemental_item_unit_size(o_ptr,
            ELEMENTAL_CANDIDATE_INVENTORY, slot);
        if ((unit_size <= 0) || (unit_size > allowed_size))
            continue;

        units = elemental_item_unit_count(o_ptr);
        if (units <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_INVENTORY;
        candidates[count].index = slot;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = units;
        candidates[count].units = units;
        candidates[count].unit_size = unit_size;
        candidates[count].quantity_per_unit = elemental_item_quantity_per_unit(o_ptr);
        available_units += units;
        count++;
    }

    for (int idx = 0; idx < supply_count; idx++)
    {
        object_type* o_ptr = supplies_entry_at(idx);
        int unit_size;
        int units;

        if (!o_ptr || !o_ptr->k_idx)
            continue;

        if (!elemental_attack_allows_size_location(attack_type,
                ELEMENTAL_CANDIDATE_SUPPLY, idx, o_ptr))
            continue;

        if (!elemental_attack_matches_object_material(attack_type, o_ptr))
            continue;

        unit_size = elemental_item_unit_size(o_ptr,
            ELEMENTAL_CANDIDATE_SUPPLY, idx);
        if ((unit_size <= 0) || (unit_size > allowed_size))
            continue;

        units = elemental_item_unit_count(o_ptr);
        if (units <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_SUPPLY;
        candidates[count].index = idx;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = units;
        candidates[count].units = units;
        candidates[count].unit_size = unit_size;
        candidates[count].quantity_per_unit = elemental_item_quantity_per_unit(o_ptr);
        available_units += units;
        count++;
    }

    if (candidate_count)
        *candidate_count = count;
    if (total_units)
        *total_units = available_units;

    if ((count <= 0) || (available_units <= 0))
    {
        if ((allowed_size == total) && (total == 1))
        {
            allowed_size = total + 1;
            goto retry_with_size;
        }

        mem_free(candidates);
        return false;
    }

    pick = rand_int((int)available_units);
    if (selection_roll)
        *selection_roll = pick;

    for (int i = 0; i < count; i++)
    {
        if (pick < candidates[i].units)
        {
            *out = candidates[i];
            mem_free(candidates);
            return true;
        }

        pick -= candidates[i].units;
    }

    *out = candidates[count - 1];
    mem_free(candidates);
    return true;
}

static void elemental_attack_affect_multiple_items(int attack_type,
    int raw_dam, int min_raw, int max_raw, int hp_dam)
{
    double cdf;
    double q;
    double q_squared;
    double hp = (double)hp_dam;
    const double hurt_scale = 80.0 / 3.0;
    double hurt;
    double chance;
    int threshold;
    int total;
    int total_budget;
    int destroyed = 0;
    int destroyed_size = 0;
    int resisted = 0;
    int resisted_size = 0;
    char outcome[80];

    if (hp_dam <= 0)
        return;

    cdf = elemental_damage_cdf_percentile(raw_dam, min_raw, max_raw);
    if (cdf <= 0.50)
    {
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, 0.0, 0.0, 0.0, 0, 0, "cdf<=50%");
        return;
    }

    q = elemental_clamp01((cdf - 0.50) / 0.50);
    q_squared = q * q;

    if (q_squared <= 0.50)
        msg_print("The elemental assault was furious.");
    else
        msg_print("The elemental assault was devastating.");

    hurt = (hp * hp) / ((hp * hp) + (hurt_scale * hurt_scale));
    chance = q_squared * hurt;
    threshold = (int)(chance * 1000000.0 + 0.5);

    if (threshold <= 0)
    {
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, q_squared, hurt, chance, 0, 0,
            "no trigger");
        return;
    }

    if (rand_int(1000000) >= threshold)
    {
        msg_print("Luckily, nothing was damaged.");
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, q_squared, hurt, chance, 0, 0,
            "hurt roll failed");
        return;
    }

    {
        object_type* shield = elemental_equipped_shield();

        if (shield)
        {
            int block_chance = elemental_shield_block_chance(shield,
                attack_type);

            if (block_chance > 0)
            {
                if (rand_int(100) < block_chance)
                {
                    msg_print("Your shield blocked the attack.");
                    if (!elemental_shield_has_attack_protection(shield,
                            attack_type)
                        && elemental_damage_blocking_shield(shield))
                    {
                        msg_print("Your shield lost one side of protection.");
                    }

                    elemental_debug_emit_size_summary(attack_type, raw_dam,
                        min_raw, max_raw, hp_dam, cdf, q_squared, hurt, chance,
                        0, 0, "shield blocked");
                    return;
                }

                msg_print("Your shield could not block the attack.");
            }
        }
    }

    total = elemental_damage_total(attack_type, hp_dam);
    total_budget = total;
    if (total <= 0)
    {
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, q_squared, hurt, chance, 0, 0, "no total");
        return;
    }

    while (total > 0)
    {
        elemental_item_candidate candidate;
        int amount;
        char o_name[80];

        if (!elemental_select_size_candidate(attack_type, total, &candidate,
                NULL, NULL, NULL))
        {
            break;
        }

        total -= candidate.unit_size;
        amount = candidate.quantity_per_unit;
        if (candidate.o_ptr && (candidate.o_ptr->number > 0))
            amount = MIN(amount, candidate.o_ptr->number);

        elemental_describe_quantity(o_name, sizeof(o_name), &candidate, amount);

        if (elemental_object_has_attack_resistance(candidate.o_ptr, attack_type))
        {
            elemental_message_amount(&candidate, candidate.o_ptr->number, amount,
                o_name, "resisted the attack.", "resisted the attack.");
            resisted++;
            resisted_size += candidate.unit_size;
            continue;
        }

        elemental_destroy_candidate_quantity(&candidate, attack_type, amount);
        destroyed++;
        destroyed_size += candidate.unit_size;
    }

    strnfmt(outcome, sizeof(outcome),
        "destroyed=%d(size=%d) resisted=%d(size=%d)",
        destroyed, destroyed_size, resisted, resisted_size);
    elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw, max_raw,
        hp_dam, cdf, q_squared, hurt, chance, total_budget, total, outcome);
}

static bool elemental_slot_uses_pack_like_factor(int slot,
    elemental_item_candidate_location location)
{
    if (location == ELEMENTAL_CANDIDATE_SUPPLY)
        return true;

    return (slot < INVEN_PACK) || (slot == INVEN_LITE)
        || (slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2);
}

static double elemental_item_slot_factor(int slot,
    elemental_item_candidate_location location)
{
    if (location == ELEMENTAL_CANDIDATE_SUPPLY)
        return 0.70;

    if (slot < INVEN_PACK)
        return 0.70;

    switch (slot)
    {
    case INVEN_WIELD:
        return 1.40;
    case INVEN_BOW:
        return 1.20;
    case INVEN_ARM:
        return 1.10;
    case INVEN_BODY:
        return 1.00;
    case INVEN_OUTER:
        return 1.15;
    case INVEN_HANDS:
        return 0.90;
    case INVEN_FEET:
        return 0.85;
    case INVEN_HEAD:
        return 0.85;
    case INVEN_LITE:
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
        return 0.70;
    default:
        return 1.00;
    }
}

static double elemental_item_material_factor(int attack_type,
    const object_type* o_ptr)
{
    switch (attack_type)
    {
    case GF_FIRE:
        if ((o_ptr->tval == TV_ARROW) || (o_ptr->tval == TV_HAFTED)
            || (o_ptr->tval == TV_POLEARM))
        {
            return 1.20;
        }

        if ((o_ptr->tval == TV_BOW) || (o_ptr->tval == TV_STAFF))
            return 1.60;

        if ((o_ptr->tval == TV_CLOAK) || (o_ptr->tval == TV_BOOTS)
            || (o_ptr->tval == TV_GLOVES)
            || ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_ROBE)))
        {
            return 1.40;
        }

        return 1.00;

    case GF_COLD:
    case GF_SOUND:
        if ((o_ptr->tval == TV_POTION) || (o_ptr->tval == TV_GEM)
            || (o_ptr->tval == TV_FLASK))
        {
            return 1.50;
        }

        if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN))
            return 1.20;

        return 1.00;

    case GF_ACID:
        if ((o_ptr->tval == TV_MAIL) || (o_ptr->tval == TV_SHIELD)
            || (o_ptr->tval == TV_HELM) || (o_ptr->tval == TV_CROWN))
        {
            return 1.40;
        }

        if ((o_ptr->tval == TV_ARROW) || (o_ptr->tval == TV_BOW)
            || (o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_HAFTED)
            || (o_ptr->tval == TV_POLEARM))
        {
            return 1.10;
        }

        return 1.00;

    default:
        return 1.00;
    }
}

static bool elemental_attack_allows_candidate_location(int attack_type,
    elemental_item_candidate_location location, int index)
{
    switch (attack_type)
    {
    case GF_FIRE:
    case GF_ACID:
    case GF_ELEC:
        return (location == ELEMENTAL_CANDIDATE_INVENTORY)
            && (index >= INVEN_WIELD) && (index < INVEN_TOTAL);

    case GF_COLD:
        return location == ELEMENTAL_CANDIDATE_SUPPLY;

    case GF_SOUND:
        return true;

    default:
        return false;
    }
}

static long elemental_item_weight(int attack_type,
    elemental_item_candidate_location location, int index,
    const object_type* o_ptr)
{
    double slot_factor;
    double material_factor;
    double stack_factor = 1.0;
    long scaled;

    if (!elemental_attack_can_target_object(attack_type, o_ptr))
        return 0;

    if (!elemental_attack_allows_candidate_location(attack_type, location, index))
        return 0;

    slot_factor = elemental_item_slot_factor(index, location);
    material_factor = elemental_item_material_factor(attack_type, o_ptr);

    if (elemental_slot_uses_pack_like_factor(index, location))
    {
        stack_factor = sqrt((double)MAX(o_ptr->number, 1));
    }

    scaled = (long)((slot_factor * material_factor * stack_factor * 1000.0)
        + 0.5);

    if (scaled < 1)
        scaled = 1;

    return scaled;
}

static bool elemental_select_candidate(int attack_type,
    elemental_item_candidate* out, elemental_item_debug_info* debug)
{
    int supply_count = supplies_entry_count();
    int capacity = INVEN_TOTAL + supply_count;
    elemental_item_candidate* candidates;
    int count = 0;
    long total_weight = 0;
    int pick;
    int selection_roll;

    if (!out || (capacity <= 0))
        return false;

    candidates = mem_alloc_array(capacity, elemental_item_candidate);

    for (int slot = 0; slot < INVEN_TOTAL; slot++)
    {
        object_type* o_ptr = &inventory[slot];
        long weight;

        if (!o_ptr->k_idx)
            continue;

        weight = elemental_item_weight(attack_type,
            ELEMENTAL_CANDIDATE_INVENTORY, slot, o_ptr);
        if (weight <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_INVENTORY;
        candidates[count].index = slot;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = weight;
        total_weight += weight;
        count++;
    }

    for (int idx = 0; idx < supply_count; idx++)
    {
        object_type* o_ptr = supplies_entry_at(idx);
        long weight;

        if (!o_ptr || !o_ptr->k_idx)
            continue;

        weight = elemental_item_weight(attack_type,
            ELEMENTAL_CANDIDATE_SUPPLY, idx, o_ptr);
        if (weight <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_SUPPLY;
        candidates[count].index = idx;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = weight;
        total_weight += weight;
        count++;
    }

    if ((count <= 0) || (total_weight <= 0))
    {
        if (debug)
        {
            debug->candidate_count = count;
            debug->total_weight = total_weight;
        }
        mem_free(candidates);
        return false;
    }

    pick = rand_int((int)total_weight);
    selection_roll = pick;
    for (int i = 0; i < count; i++)
    {
        if (pick < candidates[i].weight)
        {
            *out = candidates[i];
            elemental_debug_record_candidate(debug, attack_type, out,
                selection_roll, count, total_weight);
            mem_free(candidates);
            return true;
        }

        pick -= (int)candidates[i].weight;
    }

    *out = candidates[count - 1];
    elemental_debug_record_candidate(debug, attack_type, out, selection_roll,
        count, total_weight);
    mem_free(candidates);
    return true;
}

static void elemental_message(const elemental_item_candidate* candidate,
    int original_number, cptr o_name, cptr action)
{
    cptr owner = (original_number > 1) ? "One of your" : "Your";

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        msg_format("%s %s in your supplies %s", owner, o_name, action);
    }
    else if (candidate->index < INVEN_PACK)
    {
        msg_format("%s %s (%c) %s", owner, o_name,
            index_to_label(candidate->index), action);
    }
    else
    {
        msg_format("%s %s %s", owner, o_name, action);
    }
}

static void elemental_mark_inventory_item_changed(void)
{
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

static void elemental_prepare_split_item(const object_type* src,
    object_type* split)
{
    object_copy(split, src);
    split->number = 1;
    split->pickup = false;
    split->pickup_slot = -1;
}

static void elemental_remove_one_from_candidate(
    const elemental_item_candidate* candidate)
{
    object_type* o_ptr = candidate->o_ptr;

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        (void)supplies_consume_quantity(candidate->index, 1);
        return;
    }

    if (((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN))
        && (o_ptr->number > 1))
    {
        o_ptr->pval -= o_ptr->pval / o_ptr->number;
    }

    inven_item_increase(candidate->index, -1);
    inven_item_optimize(candidate->index);
}

static void elemental_reinsert_split_item(
    const elemental_item_candidate* candidate, object_type* split)
{
    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        if (!supplies_absorb_object(split))
            drop_near(split, 0, p_ptr->py, p_ptr->px);
        return;
    }

    if (inven_carry(split, false) < 0)
        drop_near(split, 0, p_ptr->py, p_ptr->px);
}

static bool acid_can_corrode_object(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return true;

    default:
        return false;
    }
}

static bool acid_corrode_object(object_type* o_ptr)
{
    if ((o_ptr->ps <= 0) && (o_ptr->evn <= 0))
        return true;

    if (o_ptr->evn >= 0)
        o_ptr->evn--;
    else
        o_ptr->ps--;

    return false;
}

static cptr elemental_corrode_candidate(const elemental_item_candidate* candidate,
    elemental_item_debug_info* debug)
{
    object_type* source = candidate->o_ptr;
    object_type split;
    int original_number = source->number;
    char o_name[80];

    object_desc(o_name, sizeof(o_name), source, false, 3);

    if (original_number > 1)
    {
        elemental_prepare_split_item(source, &split);
        elemental_remove_one_from_candidate(candidate);

        if (acid_corrode_object(&split))
        {
            if (debug)
            {
                debug->outcome = "was destroyed!";
                elemental_debug_emit(debug);
            }
            elemental_message(candidate, original_number, o_name,
                "was destroyed!");
            return "was destroyed!";
        }

        elemental_reinsert_split_item(candidate, &split);
        if (debug)
        {
            debug->outcome = "was damaged!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was damaged!");
        return "was damaged!";
    }

    if (acid_corrode_object(source))
    {
        elemental_remove_one_from_candidate(candidate);
        if (debug)
        {
            debug->outcome = "was destroyed!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was destroyed!");
        return "was destroyed!";
    }

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
        supplies_refresh_entry(candidate->index);
    else
        elemental_mark_inventory_item_changed();

    if (debug)
    {
        debug->outcome = "was damaged!";
        elemental_debug_emit(debug);
    }
    elemental_message(candidate, original_number, o_name, "was damaged!");
    return "was damaged!";
}

static cptr elemental_destroy_candidate(
    const elemental_item_candidate* candidate, int attack_type,
    elemental_item_debug_info* debug)
{
    object_type* source = candidate->o_ptr;
    object_type split;
    object_type* target = source;
    int original_number = source->number;
    char o_name[80];

    object_desc(o_name, sizeof(o_name), source, false, 3);

    if (original_number > 1)
    {
        elemental_prepare_split_item(source, &split);
        target = &split;
    }

    if ((target->tval == TV_CHEST) && (attack_type != GF_SOUND))
    {
        chest_release_contents(target, p_ptr->py, p_ptr->px, attack_type);
        elemental_remove_one_from_candidate(candidate);
        if (debug)
        {
            debug->outcome = "was destroyed!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name,
            "was destroyed!");
        return "was destroyed!";
    }

    if ((attack_type == GF_FIRE) && object_break_shafted_weapon_by_fire(target))
    {
        if (original_number > 1)
        {
            elemental_remove_one_from_candidate(candidate);
            elemental_reinsert_split_item(candidate, target);
        }
        else if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
        {
            supplies_refresh_entry(candidate->index);
        }
        else
        {
            elemental_mark_inventory_item_changed();
        }

        if (debug)
        {
            debug->outcome = "was broken!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was broken!");
        return "was broken!";
    }

    if (((attack_type == GF_COLD) || (attack_type == GF_SOUND))
        && (target->tval == TV_LIGHT) && (target->sval == SV_LIGHT_LANTERN)
        && object_break_brass_lantern(target))
    {
        if (original_number > 1)
        {
            elemental_remove_one_from_candidate(candidate);
            elemental_reinsert_split_item(candidate, target);
        }
        else if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
        {
            supplies_refresh_entry(candidate->index);
        }
        else
        {
            elemental_mark_inventory_item_changed();
        }

        if (debug)
        {
            debug->outcome = "was broken!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was broken!");
        return "was broken!";
    }

    elemental_remove_one_from_candidate(candidate);
    if (debug)
    {
        debug->outcome = "was destroyed!";
        elemental_debug_emit(debug);
    }
    elemental_message(candidate, original_number, o_name, "was destroyed!");
    return "was destroyed!";
}

static void elemental_attack_affect_one_item(int attack_type, int raw_dam,
    int min_raw, int max_raw, int hp_dam)
{
    elemental_item_candidate candidate;
    elemental_item_debug_info debug;
    int gate_roll;

    elemental_debug_init(&debug, attack_type, raw_dam, min_raw, max_raw, hp_dam);
    int threshold = elemental_attack_probability_per_million(raw_dam, min_raw,
        max_raw, hp_dam, debug.enabled ? &debug : NULL);

    if (threshold <= 0)
    {
        if (debug.enabled)
        {
            debug.outcome = "no trigger chance";
            elemental_debug_emit(&debug);
        }
        return;
    }

    gate_roll = rand_int(1000000);
    if (debug.enabled)
    {
        debug.gate_roll_made = true;
        debug.gate_roll = gate_roll;
    }

    if (gate_roll >= threshold)
    {
        if (debug.enabled)
        {
            debug.outcome = "no item";
            elemental_debug_emit(&debug);
        }
        return;
    }

    if (!elemental_select_candidate(attack_type, &candidate,
        debug.enabled ? &debug : NULL))
    {
        if (debug.enabled)
        {
            debug.outcome = "no eligible target";
            elemental_debug_emit(&debug);
        }
        return;
    }

    if ((attack_type == GF_ACID) && acid_can_corrode_object(candidate.o_ptr)
        )
    {
        int acid_roll = rand_int(2);

        if (debug.enabled)
        {
            debug.acid_roll_made = true;
            debug.acid_roll = acid_roll;
        }

        if (acid_roll == 0)
        {
            (void)elemental_corrode_candidate(&candidate,
                debug.enabled ? &debug : NULL);
            return;
        }
    }

    (void)elemental_destroy_candidate(&candidate, attack_type,
        debug.enabled ? &debug : NULL);
}

static void sound_dam(int raw_dam, int min_raw, int max_raw, int hp_dam)
{
    if (raw_dam <= 0)
        return;

    elemental_attack_affect_one_item(GF_SOUND, raw_dam, min_raw, max_raw,
        hp_dam);
}

/*
 * Hurt the player with Acid
 */
void acid_dam(int raw_dam, int min_raw, int max_raw, int hp_dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_ACID, raw_dam, min_raw, max_raw,
        hp_dam);
}

/*
 * Hurt the player with electricity
 */
void elec_dam(int raw_dam, int min_raw, int max_raw, int hp_dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_ELEC, raw_dam, min_raw, max_raw,
        hp_dam);
}

/*
 * The player's fire resistance depends on equipment and temporary effects
 */
extern int resist_fire(void)
{
    int res = p_ptr->resist_fire;

    if (p_ptr->oppose_fire)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's cold resistance depends on equipment and temporary effects
 */
extern int resist_cold(void)
{
    int res = p_ptr->resist_cold;

    if (p_ptr->oppose_cold)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's poison resistance depends on equipment and temporary effects
 */
extern int resist_pois(void)
{
    int res = p_ptr->resist_pois;

    if (p_ptr->oppose_pois)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's dark resistance is strictly dependent
 * on the brightness of their square
 */
extern int resist_dark(void)
{
    int res = cave_light[p_ptr->py][p_ptr->px];

    if (res < 1)
        res = 1;

    return (res);
}

static void log_elemental_damage_context(const char* tag, cptr kb_str, int dam,
    int prt, int resistance, int net_dam)
{
    bool should_log = level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px)
        != BIG_CAVE_NONE;

    if (!should_log)
    {
        should_log = (cave_info[p_ptr->py][p_ptr->px]
            & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
    }

    if (!should_log)
        return;

    log_partition_debug_for_point(tag, p_ptr->py, p_ptr->px);
    log_debug(
        "%s: killer=%s raw=%d prt=%d net=%d base_fire=%d base_cold=%d base_pois=%d oppose_fire=%d oppose_cold=%d oppose_pois=%d effective_resistance=%d",
        tag, kb_str ? kb_str : "(none)", dam, prt, net_dam,
        p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
        p_ptr->oppose_fire, p_ptr->oppose_cold, p_ptr->oppose_pois,
        resistance);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_mixed(int raw_dam, int min_raw, int max_raw, int hp_dam,
    cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_FIRE, raw_dam, min_raw, max_raw,
        hp_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_FIRE, false);
    int resistance = resist_fire();

    if (resistance > 0)
        net_dam = dam / resistance;
    else
        net_dam = dam * (-resistance);

    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_FIRE, false);
    }

    log_elemental_damage_context("fire_dam_pure", kb_str, dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_FIRE, dam, dd, dd * ds,
        net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_mixed(int raw_dam, int min_raw, int max_raw, int hp_dam,
    cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_COLD, raw_dam, min_raw, max_raw,
        hp_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_COLD, false);
    int resistance = resist_cold();

    if (resistance > 0)
        net_dam = dam / resistance;
    else
        net_dam = dam * (-resistance);

    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_COLD, false);
    }

    log_elemental_damage_context("cold_dam_pure", kb_str, dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_COLD, dam, dd, dd * ds,
        net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Darkness from melee
 */
void dark_dam_mixed(int dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Take damage */
    take_hit(dam, kb_str);
}

/*
 * Hurt the player with Darkness from breaths
 */
void dark_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_DARK, false);
    int resistance = resist_dark();

    net_dam = dam / resistance;
    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_DARK, false);
    }

    // 'pure' darkness attacks can also blind
    if (one_in_(resistance) && allow_player_blind(NULL))
    {
        (void)set_blind(p_ptr->blind + damroll(2, 4));
    }

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);
}

/*
 * Poison the player from melee
 */
void pois_dam_mixed(int dam)
{
    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Set poison counter */
    set_poisoned(p_ptr->poisoned + dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_POIS);
}

/*
 * Poison the player from breaths etc
 */
void pois_dam_pure(int dd, int ds, bool update_rolls)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_POIS, false);
    int resistance = resist_pois();

    if (resistance > 0)
        net_dam = dam / resistance;
    else
        net_dam = dam * (-resistance);

    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_POIS, false);
    }

    log_elemental_damage_context("pois_dam_pure", "poison", dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Set poison counter */
    set_poisoned(p_ptr->poisoned + net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_POIS);
}

/*
 * Increase a stat by one randomized level
 *
 * Most code will "restore" a stat before calling this function,
 * in particular, stat potions will always restore the stat and
 * then increase the fully restored value.
 */
bool inc_stat(int stat)
{
    /* Cannot go above BASE_STAT_MAX */
    if (p_ptr->stat_base[stat] < BASE_STAT_MAX)
    {
        p_ptr->stat_base[stat]++;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);

        /* Success */
        return (true);
    }

    /* Nothing to gain */
    return (false);
}

/*
 * Decreases a stat by a number of points.
 *
 * Note that "permanent" means that the *given* amount is permanent,
 * not that the new value becomes permanent.
 */
bool dec_stat(int stat, int amount, bool permanent)
{
    int result = false;

    /* Temporary damage */
    if (!permanent)
    {
        p_ptr->stat_drain[stat] -= amount;
        result = true;
    }

    /* Permanent damage */
    if (permanent && (p_ptr->stat_base[stat] > 0))
    {
        if (amount > p_ptr->stat_base[stat])
            p_ptr->stat_base[stat] = 0;
        else
            p_ptr->stat_base[stat] -= amount;

        result = true;
    }

    /* Apply changes */
    if (result)
    {
        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);
    }

    /* Done */
    return (result);
}

/*
 * Restore a stat by the number of points.
 * Return true only if this actually makes a difference.
 */
bool res_stat(int stat, int points)
{
    /* Restore if needed */
    if (p_ptr->stat_drain[stat] < 0)
    {
        /* Restore */
        p_ptr->stat_drain[stat] += points;

        if (p_ptr->stat_drain[stat] > 0)
            p_ptr->stat_drain[stat] = 0;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);

        /* Success */
        return (true);
    }

    /* Nothing to restore */
    return (false);
}

/*
 * Inflict disease on the character.
 */
void disease(int* damage)
{
    int con, attempts;
    int i;

    /* Get current constitution */
    con = p_ptr->stat_use[A_CON];

    /* Adjust damage and choose message based on constitution */
    if (con < -2)
    {
        msg_print("You feel deathly ill.");
        *damage *= 2;
    }

    else if (con < 0)
    {
        msg_print("You feel seriously ill.");
    }

    else if (con < 2)
    {
        msg_print("You feel quite ill.");
        *damage = *damage * 2 / 3;
    }

    else if (con < 5)
    {
        msg_print("You feel ill.");
        *damage /= 2;
    }

    else if (con < 7)
    {
        msg_print("You feel sick.");
        *damage /= 3;
    }

    else
    {
        msg_print("You feel a bit sick.");
        *damage /= 4;
    }

    /* Infect the character (fully cumulative) */
    set_poisoned(p_ptr->poisoned + *damage + 1);

    /* Determine # of stat-reduction attempts */
    attempts = (5 + *damage) / 5;

    /* Attack stats */
    for (i = 0; i < attempts; i++)
    {
        /* Each attempt has a 10% chance of success */
        if (one_in_(10))
        {
            /* Damage a random stat */
            (void)do_dec_stat(rand_int(A_MAX), NULL);
        }
    }
}

/*
 * Apply disenchantment to the player's stuff
 *
 * This function is also called from the "melee" code.
 *
 * The "mode" is currently unused.
 *
 * Return "true" if the player notices anything.
 *
 * Sil-y: this presently brings att, evn, dd, ds, pd, ds down towards their base
 * values by one point each
 */
bool apply_disenchant(int mode)
{
    int t = 0;

    object_type* o_ptr;

    object_kind* k_ptr;

    char o_name[80];

    /* Unused parameter */
    (void)mode;

    /* Pick a random slot */
    switch (dieroll(8))
    {
    case 1:
        t = INVEN_WIELD;
        break;
    case 2:
        t = INVEN_BOW;
        break;
    case 3:
        t = INVEN_BODY;
        break;
    case 4:
        t = INVEN_OUTER;
        break;
    case 5:
        t = INVEN_ARM;
        break;
    case 6:
        t = INVEN_HEAD;
        break;
    case 7:
        t = INVEN_HANDS;
        break;
    case 8:
        t = INVEN_FEET;
        break;
    }

    /* Get the item */
    o_ptr = &inventory[t];

    k_ptr = &k_info[o_ptr->k_idx];

    /* No item, nothing happens */
    if (!o_ptr->k_idx)
        return (false);

    /* Check to see if it is disenchantable */

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Artefacts have 60% chance to resist */
    if (artefact_p(o_ptr) && percent_chance(60))
    {
        /* Message */
        msg_format("Your %s (%c) resist%s disenchantment!", o_name,
            index_to_label(t), ((o_ptr->number != 1) ? "" : "s"));

        /* Notice */
        return (true);
    }

    /* Do the disenchanting */
    if (o_ptr->att > k_ptr->att)
        o_ptr->att--;
    if (o_ptr->evn > k_ptr->evn)
        o_ptr->evn--;
    if (o_ptr->ds > k_ptr->ds)
        o_ptr->ds--;
    if (o_ptr->dd > k_ptr->dd)
        o_ptr->dd--;
    if (o_ptr->ps > k_ptr->ps)
        o_ptr->ps--;
    if (o_ptr->pd > k_ptr->pd)
        o_ptr->pd--;

    msg_format("Your %s (%c) %s disenchanted!", o_name, index_to_label(t),
        ((o_ptr->number != 1) ? "were" : "was"));

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

    /* Notice */
    return (true);
}

/*
 * Mega-Hack -- track "affected" monsters (see "project()" comments)
 */
static int project_m_n;
static int project_m_x;
static int project_m_y;

/*
 * Magically close/lock/restore a door at a particular grid
 */
bool lock_door(int y, int x, int power)
{
    int lock_level;
    int obvious = false;

    // ignore warded doors
    if (cave_glyph(y, x))
        return false;

    if (cave_feat[y][x] == FEAT_BROKEN)
        power -= 10;

    if ((power > 0) && (cave_m_idx[y][x] == 0))
    {
        if (cave_known_closed_door_bold(y, x) || (cave_feat[y][x] == FEAT_OPEN)
            || (cave_feat[y][x] == FEAT_BROKEN))
        {
            if ((cave_feat[y][x] == FEAT_OPEN)
                || (cave_feat[y][x] == FEAT_BROKEN))
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door slams shut.");
                }
                else
                {
                    msg_print("You hear a door slam shut.");
                }
            }

            // lock the door more firmly than it was before
            lock_level = cave_feat[y][x] - FEAT_DOOR_HEAD + power / 2;
            if (lock_level > 7)
            {
                lock_level = 7;
            }

            if (cave_feat[y][x] != FEAT_DOOR_HEAD + lock_level)
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD + lock_level);

                msg_print("You hear a 'click'.");
            }

            /* Update the flow code and visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
        }
    }

    return (obvious);
}

bool lock_doors_radius(int y0, int x0, int radius, int power)
{
    bool obvious = false;

    if (radius < 0)
        return false;

    for (int y = y0 - radius; y <= y0 + radius; y++)
    {
        for (int x = x0 - radius; x <= x0 + radius; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (distance(y0, x0, y, x) > radius)
                continue;

            if (lock_door(y, x, power))
                obvious = true;
        }
    }

    return obvious;
}

/*
 * We are called from "project()" to "damage" terrain features
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * We return "true" if the effect of the projection is "obvious".
 *
 * Hack -- We also "see" grids which are "memorized".
 *
 * Perhaps we should affect doors and/or walls.
 */
static bool project_f(
    int who, int y, int x, int dist, int dd, int ds, int dif, int typ)
{
    bool obvious = false;
    monster_type* who_ptr = (who == -1) ? PLAYER : &mon_list[who]; // Sil-y

    /* Unused parameters */
    (void)dist;
    (void)dd;
    (void)ds;

    /* Analyze the type */
    switch (typ)
    {
    /* Ignore most effects */

    /* Destroy Traps */
    case GF_KILL_TRAP:
    {
        /* Destroy traps */
        if (cave_trap_bold(y, x))
        {
            /* Check line of sight */
            if (player_has_los_bold(y, x) && !cave_floorlike_bold(y, x))
            {
                obvious = true;
            }

            /* Forget the trap */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the trap */
            cave_set_feat(y, x, FEAT_FLOOR);
        }

        break;
    }

    /* unlock/open/break Doors */
    case GF_KILL_DOOR:
    {
        if (cave_known_closed_door_bold(y, x) && !cave_glyph(y, x))
        {
            int result = skill_check(who_ptr, dif, 0, NULL);

            if (result <= 0)
            {
                /* Do nothing */
            }
            else if (result <= 5)
            {
                /* Unlock the door */
                cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);

                msg_print("You hear a 'click'.");
            }
            else if (result <= 10)
            {
                /* Forget the door */
                // cave_info[y][x] &= ~(CAVE_MARK);

                /* Open the door */
                cave_set_feat(y, x, FEAT_OPEN);

                /* Update the flow code */
                p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door flies open.");
                }
                else
                {
                    msg_print("You hear a door burst open.");
                }
            }
            else
            {
                /* Break the door */
                cave_set_feat(y, x, FEAT_BROKEN);

                /* Update the flow code */
                p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

                obvious = true;

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The door is ripped from its hinges.");
                }
                else
                {
                    msg_print("You hear a door burst open.");
                }
            }
        }

        if (cave_feat[y][x] == FEAT_RUBBLE)
        {
            int result = skill_check(who_ptr, dif, 0, NULL);

            if (result <= 0)
            {
                /* Do nothing */
            }

            else
            {
                /* Disperse the rubble */
                cave_set_feat(y, x, FEAT_FLOOR);

                obvious = true;

                /* Update the flow code */
                p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

                if (cave_info[y][x] & (CAVE_SEEN))
                {
                    msg_print("The rubble is scattered across the floor.");
                }
                else
                {
                    msg_print("You hear a loud rumbling.");
                }
            }
        }

        break;
    }

    /* Destroy walls (and doors) */
    case GF_KILL_WALL:
    {
        /* Non-walls (etc) */
        if (cave_floor_bold(y, x))
            break;

        /* Permanent walls */
        if (cave_feat[y][x] == FEAT_WALL_PERM)
            break;

        /* Granite */
        if (cave_feat[y][x] >= FEAT_WALL_EXTRA
            && skill_check(PLAYER, dif, 14, NULL) > 0)
        {
            /* Message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The wall shatters!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the wall */
            cave_set_feat(y, x, FEAT_RUBBLE);
        }
        /* Quartz */
        else if (cave_feat[y][x] >= FEAT_QUARTZ
            && skill_check(PLAYER, dif, 12, NULL) > 0)
        {
            /* Message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The vein shatters!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the wall */
            cave_set_feat(y, x, FEAT_RUBBLE);
        }
        /* Rubble */
        else if (cave_feat[y][x] == FEAT_RUBBLE
            && skill_check(PLAYER, dif, 10, NULL) > 0)
        {
            /* Message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The rubble is blown away!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the rubble */
            cave_set_feat(y, x, FEAT_FLOOR);
        }

        /* Destroy doors (and secret doors) */
        else if (cave_any_closed_door_bold(y, x)
            && skill_check(PLAYER, dif, 8, NULL) > 0)
        {
            /* Hack -- special message */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                msg_print("The door is blown from its hinges!");
                obvious = true;
            }

            /* Forget the wall */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Destroy the feature */
            cave_set_feat(y, x, FEAT_BROKEN);
        }

        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        break;
    }

    /* Lock Doors */
    case GF_LOCK_DOOR:
    {
        obvious = lock_door(y, x, skill_check(who_ptr, dif, 0, NULL));

        break;
    }

    /* Lite up the grid */
    case GF_LIGHT:
    {
        // Must make sure it is viewable (passwall was only used to guarantee
        // wall lighting)
        if (cave_info[y][x] & (CAVE_VIEW))
        {
            /* Turn on the light */
            cave_info[y][x] |= (CAVE_GLOW);
        }

        /* Grid is in line of sight */
        if (player_has_los_bold(y, x))
        {
            if (!p_ptr->blind)
            {
                /* Observe */
                obvious = true;
            }

            /* Fully update the visuals */
            p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
        }

        break;
    }

    /* Darken the grid */
    case GF_DARK_WEAK:
    case GF_DARK:
    {
        if (cave_info[y][x] & (CAVE_GLOW))
        {
            /* Turn off the light */
            cave_info[y][x] &= ~(CAVE_GLOW);

            /* Hack -- Forget "boring" grids */
            if (cave_floorlike_bold(y, x))
            {
                /* Forget */
                cave_info[y][x] &= ~(CAVE_MARK);
            }
            /* Grid is in line of sight */
            if (player_has_los_bold(y, x))
            {
                /* Observe */
                obvious = true;

                /* Fully update the visuals */
                p_ptr->update
                    |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
            }
        }

        /* All done */
        break;
    }
    }

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * We are called from "project()" to "damage" objects
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * Perhaps we should only SOMETIMES damage things on the ground.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- We also "see" objects which are "memorized".
 *
 * We return "true" if the effect of the projection is "obvious".
 */
static bool project_o(int who, int y, int x, int dd, int ds, int dif, int typ)
{
    s16b this_o_idx, next_o_idx = 0;

    bool obvious = false;

    u32b f1, f2, f3;

    char o_name[80];

    /* Unused parameters */
    (void)who;
    (void)dif;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        bool is_art = false;
        bool ignore = false;
        bool plural = false;
        bool do_kill = false;

        cptr note_kill = NULL;

        // Sil-y: previously used damage to see if items were broken, now just
        // ignoring damage
        // int dam = damroll(dd, ds);
        (void)dd; // cast to soothe compiler warnings
        (void)ds; // cast to soothe compiler warnings

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        /* Get the "plural"-ness */
        if (o_ptr->number > 1)
            plural = true;

        /* Check for artefact */
        if (artefact_p(o_ptr))
            is_art = true;

        /* Analyze the type */
        switch (typ)
        {
        /* Acid -- Lots of things */
        case GF_ACID:
        {
            if (hates_acid(o_ptr))
            {
                do_kill = true;
                note_kill = (plural ? " melt!" : " melts!");
                if (f3 & (TR3_IGNORE_ACID))
                    ignore = true;
            }
            break;
        }

        /* Elec -- Rings */
        case GF_ELEC:
        {
            if (hates_elec(o_ptr))
            {
                do_kill = true;
                note_kill = (plural ? " are destroyed!" : " is destroyed!");
                if (f3 & (TR3_IGNORE_ELEC))
                    ignore = true;
            }
            break;
        }

        /* Fire -- Flammable objects */
        case GF_FIRE:
        {
            if (hates_fire(o_ptr))
            {
                do_kill = true;
                note_kill = (plural ? " burn up!" : " burns up!");
                if (f3 & (TR3_IGNORE_FIRE))
                    ignore = true;
            }
            break;
        }

        /* Cold -- potions and flasks */
        case GF_COLD:
        {
            if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
                && !(f3 & TR3_IGNORE_COLD))
            {
                if (o_ptr->marked)
                {
                    obvious = true;
                    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
                }

                if (object_break_brass_lantern(o_ptr))
                {
                    if (o_ptr->marked)
                        msg_format("The %s %s broken!", o_name,
                            (plural ? "are" : "is"));
                    lite_spot(y, x);
                    break;
                }
            }

            if (hates_cold(o_ptr))
            {
                note_kill = (plural ? " shatter!" : " shatters!");
                do_kill = true;
                if (f3 & (TR3_IGNORE_COLD))
                    ignore = true;
            }
            break;
        }

        /* Hack -- break potions and such */
        case GF_SOUND:
        case GF_EARTHQUAKE:
        {
            if (hates_cold(o_ptr))
            {
                note_kill = (plural ? " shatter!" : " shatters!");
                do_kill = true;
            }
            break;
        }

        /* Unlock chests */
        case GF_KILL_TRAP:
        case GF_KILL_DOOR:
        {
            /* Chests are noticed only if trapped or locked */
            if (o_ptr->tval == TV_CHEST)
            {
                /* Disarm/Unlock traps */
                if (o_ptr->pval > 0)
                {
                    /* Disarm or Unlock */
                    o_ptr->pval = (0 - o_ptr->pval);

                    /* Identify */
                    object_known(o_ptr);
                }
            }

            break;
        }

        /* Mass-identify */
        case GF_IDENTIFY:
        {
            int squelch;

            /* Ignore hidden objects */
            if (!o_ptr->marked)
                continue;

            /* Ignore known objects */
            if (object_known_p(o_ptr))
                continue;

            /* Identify object and get squelch setting */
            /* Note the first argument */
            squelch = do_ident_item(-1, o_ptr);

            /* Redraw purple dots */
            lite_spot(y, x);

            /* Squelch? */
            if (squelch == SQUELCH_YES)
                do_kill = true;

            break;
        }
        }

        /* Attempt to destroy the object */
        if (do_kill)
        {
            /* Effect "observed" */
            if (o_ptr->marked)
            {
                obvious = true;
                object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
            }

            /* Artefacts, and other objects, get to resist */
            if (is_art || ignore)
            {
                /* Observe the resist */
                if (o_ptr->marked)
                {
                    msg_format("The %s %s unaffected!", o_name,
                        (plural ? "are" : "is"));
                }
            }

            /* Kill it */
            else
            {
                /* Describe if needed */
                if (o_ptr->marked && note_kill)
                {
                    msg_format("The %s%s", o_name, note_kill);
                }

                if ((o_ptr->tval == TV_CHEST) && (typ != GF_SOUND)
                    && (typ != GF_EARTHQUAKE))
                {
                    chest_release_contents(o_ptr, y, x, typ);
                }

                /* Delete the object */
                delete_object_idx(this_o_idx);

                /* Redraw */
                lite_spot(y, x);
            }
        }
    }

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball/arc causing damage to a monster.
 *
 * This routine takes a "source monster" (by index) which is mostly used to
 * determine if the player is causing the damage, and a "radius" (see below),
 * which is used to decrease the power of explosions with distance, and a
 * location, via integers which are modified by certain types of attacks
 * (polymorph and teleport being the obvious ones), a default damage, which
 * is modified as needed based on various properties, and finally a "damage
 * type" (see below).
 *
 * Note that this routine can handle "no damage" attacks (like teleport) by
 * taking a "zero" damage, and can even take "parameters" to attacks (like
 * confuse) by accepting a "damage", using it to calculate the effect, and
 * then setting the damage to zero.  Note that the "damage" parameter is
 * lessened by two dice for each square of distance from the center.
 *
 * Note that "polymorph" is dangerous, since a failure in "place_monster()"'
 * may result in a dereference of an invalid pointer.  XXX XXX XXX
 *
 * In this function, "result" messages are postponed until the end, where
 * the "note" string is appended to the monster name, if not NULL.  So,
 * to make a spell have "no effect" just set "note" to NULL.  You should
 * also set "notice" to false, or the player will learn what the spell does.
 *
 * We attempt to return "true" if the player saw anything "useful" happen.
 */
static bool project_m(
    int who, int y, int x, int dd, int ds, int dif, int typ, u32b flg)
{
    int tmp;
    bool suppress_message = !!(flg & PROJECT_SILENT);

    monster_type* m_ptr;
    monster_race* r_ptr;
    monster_lore* l_ptr;

    monster_type* who_ptr = (who == -1) ? PLAYER : &mon_list[who]; // Sil-y
    bool who_vis = (who == -1) ? true : who_ptr->ml;

    int dam = damroll(dd, ds);

    // Monster's skill modifier
    int resistance;

    // Result of opposed check
    int result;

    /* Is the monster "seen"? */
    bool seen = false;

    /* Were the effects "obvious" (if seen)? */
    bool obvious = false;

    /* Were the effects "irrelevant"? */
    bool skipped = false;

    /* Does it alert the monster */
    bool alerting = true;

    /* Polymorph setting (true or false) */
    int do_poly = 0;

    /* Teleport setting (max distance) */
    int do_dist = 0;

    /* Confusion setting (amount to confuse) */
    int do_conf = 0;

    /* Stunning setting (amount to stun) */
    int do_stun = 0;

    /* Slow setting (amount to haste) */
    int do_slow = 0;

    /* Haste setting (amount to haste) */
    int do_haste = 0;

    /* Sleep amount (amount to sleep) */
    int do_sleep = 0;

    /* Fear amount (amount to fear) */
    int do_fear = 0;

    /* Hold the monster name */
    char m_name[80];

    /* Assume no note */
    cptr note = NULL;

    /* Assume a default death */
    cptr note_dies = " dies.";

    /* Unused parameter*/
    (void)flg;

    /* Walls protect monsters */
    if (!cave_floor_bold(y, x))
        return (false);

    /* No monster here */
    if (!(cave_m_idx[y][x] > 0))
        return (false);

    /* Never affect projector */
    if (cave_m_idx[y][x] == who)
        return (false);

    /* Obtain monster info */
    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];
    l_ptr = &l_list[m_ptr->r_idx];
    if (m_ptr->ml)
        seen = true;

    /* Get the monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Some monsters get "destroyed" */
    if (monster_nonliving(r_ptr))
    {
        /* Special note at death */
        note_dies = " is destroyed.";
    }

    /* Monster goes active */
    m_ptr->mflag |= (MFLAG_ACTV);

    /*Mark the monster as attacked by the player*/
    if (who < 0)
        m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

    /* Analyze the damage type */
    switch (typ)
    {
    /* Acid */
    case GF_ACID:
    {
        if (seen)
            obvious = true;
        break;
    }

    /* Electricity */
    case GF_ELEC:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_ELEC))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_ELEC);
        }
        break;
    }

    /* Fire damage */
    case GF_FIRE:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_FIRE))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_FIRE);
        }
        if (r_ptr->flags3 & (RF3_HURT_FIRE))
        {
            note = " is badly hurt.";
            dam *= 2;
            if (seen)
                l_ptr->flags3 |= (RF3_HURT_FIRE);
        }
        break;
    }

    /* Cold */
    case GF_COLD:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_COLD))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_COLD);
        }
        if (r_ptr->flags3 & (RF3_HURT_COLD))
        {
            note = " is badly hurt.";
            dam *= 2;
            if (seen)
                l_ptr->flags3 |= (RF3_HURT_COLD);
        }
        break;
    }

    /* Poison */
    case GF_POIS:
    {
        if (seen)
            obvious = true;
        if (r_ptr->flags3 & (RF3_RES_POIS))
        {
            note = " resists.";
            dam = 0;
            if (seen)
                l_ptr->flags3 |= (RF3_RES_POIS);
        }
        break;
    }

    /* Sound (use "dam" as amount of stunning) */
    case GF_SOUND:
    {
        obvious = true;

        do_stun = dam;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Heal Monster (use "dam" as amount of healing) */
    case GF_HEAL:
    {
        bool healed = true;

        /*does monster need healing?*/
        if (m_ptr->hp == m_ptr->maxhp)
            healed = false;

        if (seen)
            obvious = true;

        /* Monster goes active */
        m_ptr->mflag |= (MFLAG_ACTV);

        /* Heal */
        m_ptr->hp += dam;

        /* No overflow */
        if (m_ptr->hp > m_ptr->maxhp)
            m_ptr->hp = m_ptr->maxhp;

        /* Redraw (later) if needed */
        if (p_ptr->health_who == cave_m_idx[y][x])
            p_ptr->redraw |= (PR_HEALTHBAR);

        /*monster was at full hp to begin*/
        if (!healed)
        {
            obvious = false;
        }

        /* Message */
        else
            note = " looks healthier.";

        // doesn't alert sleeping monsters
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            alerting = false;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Speed Monster */
    case GF_SPEED:
    {
        if (seen)
            obvious = true;

        /* Speed up */
        do_haste = dam;

        // doesn't alert sleeping monsters
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            alerting = false;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Slow Monster (Use "dif" as difficulty and for duration) */
    case GF_SLOW:
    {
        if (seen)
            obvious = true;

        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_SLOW))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        /* If successful, slow the monster */
        if (result > 0)
        {
            do_slow = result + 10;
        }
        else
        {
            note = " is unaffected!";
            obvious = false;
            if ((seen) && (r_ptr->flags3 & (RF3_NO_SLOW)))
                l_ptr->flags3 |= (RF3_NO_SLOW);
        }

        // doesn't alert sleeping or unaffected monsters
        if ((m_ptr->alertness < ALERTNESS_UNWARY) || (do_slow == 0))
            alerting = false;

        /* No "real" damage */
        dam = 0;

        break;
    }

    /* Sleep (Use "dif" as difficulty and for strength) */
    case GF_SLEEP:
    {
        if (seen)
            obvious = true;

        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_SLEEP))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif + 10 - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        /* If successful, (partially) put the monster to sleep */
        if (result > 0)
        {
            do_sleep = result + 5;
        }
        else
        {
            note = " is unaffected!";
            obvious = false;
            if ((seen) && (r_ptr->flags3 & (RF3_NO_SLEEP)))
                l_ptr->flags3 |= (RF3_NO_SLEEP);
        }

        // doesn't alert monsters
        alerting = false;

        /* No "real" damage */
        dam = 0;

        break;
    }

    /* Confusion (Use "dif" as difficulty and for duration) */
    case GF_CONFUSION:
    {
        if (seen)
            obvious = true;

        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_CONF))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        /* If successful, slow the monster */
        if (result > 0)
        {
            do_conf = result + 10;
        }
        else
        {
            note = " is unaffected!";
            obvious = false;
            if ((seen) && (r_ptr->flags3 & (RF3_NO_CONF)))
                l_ptr->flags3 |= (RF3_NO_CONF);
        }

        // doesn't alert monsters (they are either unaffected or too confused)
        alerting = false;

        /* No "real" damage */
        dam = 0;

        break;
    }

    /* Lite, but only hurts susceptible creatures */
    case GF_LIGHT:
    {
        /* Default: no damage (GF_LIGHT only hurts specific monsters) */
        dam = 0;

        // Must make sure it is viewable (passwall was only used to guarantee
        // wall lighting)
        if (cave_info[y][x] & (CAVE_VIEW))
        {
            int light_level = cave_light[y][x];
            
            /* Hurt by light - ONLY affects HURT_LITE monsters */
            if (r_ptr->flags3 & (RF3_HURT_LITE))
            {
                /* Memorize the effects */
                if (seen)
                    l_ptr->flags3 |= (RF3_HURT_LITE);

                /* Stun and damage work when light level > 2 and player-caused */
                if ((who < 0) && (light_level > 2))
                {
                    int resistance;
                    int result;
                    int actual_dam;
                    int stun_amount;
                    int skill_to_use;
                    
                    /* Determine skill to use for resistance check */
                    /* If dif >= 0, this is Song of Trees (dif contains song score), otherwise use Will */
                    if (dif >= 0)
                        skill_to_use = dif;
                    else
                        skill_to_use = p_ptr->skill_use[S_WIL];
                    
                    /* Get monster's Will resistance */
                    resistance = monster_skill(m_ptr, S_WIL);
                    
                    /* Adjust difficulty by the distance to the player */
                    result = skill_check(PLAYER, skill_to_use, 
                        resistance + 5 + distance(p_ptr->py, p_ptr->px, y, x),
                        m_ptr);
                    
                    /* Stun is applied when monster FAILS Will save (result > 0 means player wins) */
                    /* Stun amount scales with light level */
                    if (result > 0)
                    {
                        stun_amount = damroll(dd, light_level);
                        
                        /* Apply stun */
                        if (stun_amount > 0)
                        {
                            stun_monster(m_ptr, stun_amount);
                            
                            /*possibly update the monster health bar*/
                            if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
                                p_ptr->redraw |= (PR_HEALTHBAR);
                        }
                    }
                    else
                    {
                        /* Monster resisted - no stun */
                        stun_amount = 0;
                    }
                    
                    /* Damage only happens on STRONG Will failure (result >= 10) */
                    /* This represents intense light overwhelming the monster */
                    if (result >= 10)
                    {
                        /* Use light level as dice sides, dd from the attack */
                        actual_dam = damroll(dd, light_level);
                        
                        /* Reduce damage based on how much the monster failed */
                        int raw_dam = actual_dam;
                        actual_dam = (actual_dam * result) / (result + 5);
                        
                        /* Debug logging */
                        if (seen)
                        {
                            log_debug("GF_LIGHT: dd=%d light=%d raw=%d result=%d final=%d stun=%d", 
                                dd, light_level, raw_dam, result, actual_dam, stun_amount);
                        }
                        
                        if (actual_dam > 0)
                        {
                            /* Override dam with actual calculated damage */
                            dam = actual_dam;
                            
                            /* Obvious effect */
                            if (seen)
                                obvious = true;
                            
                            /* Message for visible monsters */
                            if (seen)
                                note = " is seared by radiant light!";
                        }
                        else
                        {
                            dam = 0;
                            
                            /* Stunned but no damage */
                            if (seen)
                                note = " cringes from the light!";
                        }
                    }
                    else if (result > 0)
                    {
                        /* Stunned but not enough to damage */
                        dam = 0;
                        
                        if (seen)
                            note = " cringes from the light!";
                    }
                    else
                    {
                        /* Monster resisted - no stun, no damage */
                        dam = 0;
                        
                        if (seen)
                            note = " resists the light!";
                    }
                }
                else
                {
                    /* Light level too low or not player-caused - no damage or stun */
                    dam = 0;
                }
            }
            else
            {
                /* Not hurt by light - no damage */
                dam = 0;
            }
        }

        // Doesn't alert monsters (there is a seperate function to do this for
        // light)
        alerting = false;

        break;
    }

    /* Dark */
    case GF_DARK:
    {
        if (seen)
            obvious = true;
        if ((r_ptr->flags4 & (RF4_BRTH_DARK)) || (r_ptr->flags3 & (RF3_UNDEAD))
            || (r_ptr->light < 0))
        {
            note = " resists.";
            dam = 0;
        }
        break;
    }

    /* Blasting */
    case GF_KILL_WALL:
    {
        /* Hurt by rock remover */
        if (r_ptr->flags3 & (RF3_STONE))
        {
            /* Notice effect */
            if (seen)
                obvious = true;

            /* Memorize the effects */
            if (seen)
                l_ptr->flags3 |= (RF3_STONE);

            // skill check of Will vs Con * 2
            if (skill_check(PLAYER, dif, monster_stat(m_ptr, A_CON) * 2, m_ptr)
                > 0)
            {
                /* Cute little message */
                note = " partly shatters!";
                note_dies = " shatters!";
            }

            // Will check fails
            else
            {
                note = " resists!";

                /* No damage */
                dam = 0;
            }
        }

        /* Usually, ignore the effects */
        else
        {
            // doesn't alert unaffected monsters
            alerting = false;

            /* No damage */
            dam = 0;
        }

        break;
    }

    /* Teleport monster (Use "dam" as "power") */
    case GF_AWAY_ALL:
    {
        /* Obvious */
        if (seen)
            obvious = true;

        /* Prepare to teleport */
        do_dist = dam;

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* Fear (Use "dif" as difficulty and for duration) */
    case GF_FEAR:
    {
        resistance = monster_skill(m_ptr, S_WIL);
        if (r_ptr->flags3 & (RF3_NO_FEAR))
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(who_ptr,
            dif + 10 - distance(p_ptr->py, p_ptr->px, y, x), resistance, m_ptr);

        if (result > 0)
        {
            /* Obvious */
            if (seen)
                obvious = true;

            /* Apply some fear */
            do_fear = result * 20;
        }
        else
        {
            // Doesn't alert unaffected monsters
            alerting = false;

            /* No obvious effect */
            note = " is unaffected!";
            obvious = false;

            if ((seen) && (r_ptr->flags3 & (RF3_NO_FEAR)))
                l_ptr->flags3 |= (RF3_NO_FEAR);
        }

        /* No "real" damage */
        dam = 0;
        break;
    }

    /* No effect */
    case GF_NOTHING:
    {
        break;
    }

    /* Default */
    default:
    {
        /* Irrelevant */
        skipped = true;

        /* No damage */
        dam = 0;

        break;
    }
    }

    /* Absolutely no effect */
    if (skipped)
        return (false);

    /* "Unique" monsters cannot be polymorphed */
    if (r_ptr->flags1 & (RF1_UNIQUE))
        do_poly = false;

    /* "Unique" monsters can only be "killed" by the player */
    // if (r_ptr->flags1 & (RF1_UNIQUE))
    //{
    //	/* Uniques may only be killed by the player */
    //	if ((who > 0) && (dam > m_ptr->hp)) dam = m_ptr->hp;
    //}

    /* Check for death */
    if (dam > m_ptr->hp)
    {
        /* Extract method of death */
        note = note_dies;
    }

    /* Mega-Hack -- Handle "polymorph" -- monsters get a saving throw */
    else if (do_poly && (dieroll(90) > r_ptr->level))
    {
        /* Default -- assume no polymorph */
        note = " is unaffected!";

        /* Pick a "new" monster race */
        tmp = poly_r_idx(m_ptr);

        /* Handle polymorph */
        if (tmp != m_ptr->r_idx)
        {
            /* Obvious */
            if (seen)
                obvious = true;

            /* Monster polymorphs */
            note = " changes!";

            /* Turn off the damage */
            dam = 0;

            /* "Kill" the "old" monster */
            delete_monster_idx(cave_m_idx[y][x]);

            /* Create a new monster (no groups) */
            (void)place_monster_aux(y, x, tmp, false, false);

            /* Hack -- Assume success XXX XXX XXX */

            /* Hack -- Get new monster */
            m_ptr = &mon_list[cave_m_idx[y][x]];

            /* Hack -- Get new race */
            r_ptr = &r_info[m_ptr->r_idx];
        }
    }

    /* Handle "teleport" */
    else if (do_dist)
    {
        /* no teleporting on certain levels */
        if ((p_ptr->depth != 0) && (p_ptr->depth != MORGOTH_DEPTH))
        {
            /* Obvious */
            if (seen)
                obvious = true;

            /* Message */
            note = " disappears!";

            /* Teleport */
            teleport_away(cave_m_idx[y][x], do_dist);

            /* Hack -- get new location */
            y = m_ptr->fy;
            x = m_ptr->fx;
        }
    }

    /* Stunning */
    else if (do_stun)
    {
        /* Obvious */
        if (seen)
            obvious = true;

        /* Get confused */
        if (m_ptr->stunned)
            note = " is more dazed.";
        else
            note = " is dazed.";

        /*some creatures are resistant to stunning*/
        if (r_ptr->flags3 & RF3_NO_STUN)
        {
            /*mark the lore*/
            if (seen)
                l_ptr->flags3 |= (RF3_NO_STUN);

            note = " is unaffected!";
        }

        /* Apply stun */
        else
            stun_monster(m_ptr, do_stun);

        /*possibly update the monster health bar*/
        if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
            p_ptr->redraw |= (PR_HEALTHBAR);
    }

    /* Confusion  */
    else if (do_conf)
    {
        /* Obvious */
        if (seen)
            obvious = true;

        /* Generate message */
        if (m_ptr->confused)
            note = " looks more confused.";
        else
            note = " looks confused.";

        tmp = m_ptr->confused + do_conf;

        /* Apply confusion */
        m_ptr->confused += (tmp < 200) ? tmp : 200;

        if (p_ptr->health_who == cave_m_idx[m_ptr->fy][m_ptr->fx])
            p_ptr->redraw |= (PR_HEALTHBAR);
    }

    /*Slowing*/
    else if (do_slow)
    {
        /* Increase slowing */
        tmp = m_ptr->slowed + do_slow;

        /* set or add to slow counter */
        set_monster_slow(cave_m_idx[m_ptr->fy][m_ptr->fx], tmp, seen);
    }

    /* Hasting */
    else if (do_haste)
    {
        /* Increase haste */
        tmp = m_ptr->hasted + do_haste;

        /* set or add to slow counter */
        set_monster_haste(cave_m_idx[m_ptr->fy][m_ptr->fx], tmp, seen);
    }

    /* Fear */
    if (do_fear)
    {
        /* Decrease temporary morale */
        m_ptr->tmp_morale -= do_fear;
    }

    // update combat info
    if ((dam > 0) && m_ptr->ml)
    {
        int combat_dd = dd;
        int combat_ds = ds;

        if (typ == GF_LIGHT)
            combat_ds = cave_light[y][x];

        update_combat_rolls1b(who_ptr, m_ptr, who_vis);
        update_combat_rolls2(combat_dd, combat_ds, dam, -1, -1, 0, 0, typ, false);
    }

    /* If another monster did the damage, hurt the monster by hand */
    if (who > 0)
    {
        /* Redraw (later) if needed */
        if (p_ptr->health_who == cave_m_idx[y][x])
            p_ptr->redraw |= (PR_HEALTHBAR);

        /* Monster goes active */
        m_ptr->mflag |= (MFLAG_ACTV);

        /* Hurt the monster */
        m_ptr->hp -= dam;

        if (dam > 0)
            maybe_update_morgoth_state_from_hp(m_ptr);

        /* Dead monster */
        if (m_ptr->hp <= 0)
        {
            /* Song of Trees: trolls slain by radiant light crumble into rubble (Kemenrauko-style). */
            if ((typ == GF_LIGHT) && (who < 0) && (dif >= 0)
                && (r_ptr->flags3 & RF3_TROLL) && !cave_stair_bold(y, x))
            {
                cave_set_feat(y, x, FEAT_RUBBLE);
            }

            /* Generate treasure, etc */
            monster_death(cave_m_idx[y][x]);

            /* Delete the monster */
            delete_monster_idx(cave_m_idx[y][x]);

            /* Give detailed messages if destroyed */
            if ((note) && (seen))
            {
                /* dump the note*/
                if (!suppress_message)
                    msg_format("%^s%s", m_name, note);
            }

            else
                death_count++;
        }

        /* Damaged monster */
        else
        {
            // Alert it
            make_alert(m_ptr);

            /* Give detailed messages if visible or destroyed */
            if (note && seen)
            {
                /* dump the note*/
                if (!suppress_message)
                    msg_format("%^s%s", m_name, note);
            }

            /* Hack -- Pain message */
            else if (dam > 0)
                message_pain(cave_m_idx[y][x], dam);

            /* Hack -- handle sleep */
            if (do_sleep)
            {
                set_alertness(m_ptr, m_ptr->alertness - do_sleep);
            }
        }
    }

    /* If the player did it, give him experience, check fear */
    else
    {
        /*hack - only give message if seen*/
        if (!seen)
            note_dies = "";

        /* Check for oath breaking before applying damage */
        if (who < 0 && dam > 0) // Player-caused damage
        {
            /* All player-caused attacks break Valor on hit */
            if (m_ptr->ml && cowardly_attack(m_ptr))
            {
                do_cmd_note("Broke your oath", p_ptr->depth);
                apply_oath_breaking_curse(OATH_VALOROUS);
                p_ptr->oaths_broken |= OATH_VALOROUS_FLAG;
            }

            break_mercy_oath(m_ptr, dam);
        }

        /* Hurt the monster, check for death */
        if (mon_take_hit(cave_m_idx[y][x], dam, note_dies, who))
        {
            /* Note death */
            if (!seen)
                death_count++;
        }

        /* Damaged monster */
        else
        {
            // Alert it, if there has been no damage to alert it so far
            if (alerting && (dam == 0))
                make_alert(m_ptr);

            /* Give detailed messages if visible or destroyed */
            if (note && seen)
            {
                if (!suppress_message)
                    msg_format("%^s%s", m_name, note);
            }

            /* Hack -- Pain message */
            else if (dam > 0)
                message_pain(cave_m_idx[y][x], dam);

            /* Take note */
            if ((do_fear) && (m_ptr->ml) && (!suppress_message))
            {
                /* Message */
                message_format(MSG_FLEE, m_ptr->r_idx, "%^s cowers.", m_name);
            }

            /* Hack -- handle sleep */
            if (do_sleep)
            {
                set_alertness(m_ptr, m_ptr->alertness - do_sleep);
            }
        }
    }

    /* Verify this code XXX XXX XXX */

    /* Update the monster */
    update_mon(cave_m_idx[y][x], false);

    /* Redraw the monster grid */
    lite_spot(y, x);

    /* Update monster recall window */
    if (p_ptr->monster_race_idx == m_ptr->r_idx)
    {
        /* Window stuff */
        p_ptr->window |= (PW_MONSTER);
    }

    /* Track it */
    project_m_n++;
    project_m_x = x;
    project_m_y = y;

    /*
     * If this is the first monster hit, the spell was capable
     * of causing damage, and the player was the source of the spell,
     * make noise. -LM-
     */
    if ((project_m_n == 1) && (who <= 0) && (dam))
    {
        stealth_score -= 0;
    }

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball causing damage to the player.
 *
 * This routine takes a "source monster" (by index), a "distance", a default
 * "damage", and a "damage type".  See "project_m()" above.
 *
 * If "rad" is non-zero, then the blast was centered elsewhere, and the damage
 * is reduced (see "project_m()" above).  This can happen if a monster breathes
 * at the player and hits a wall instead.
 *
 * We return "true" if any "obvious" effects were observed.
 *
 * Actually, for historical reasons, we just assume that the effects were
 * obvious.  XXX XXX XXX
 */
static bool project_p(int who, int y, int x, int dd, int ds, int dif, int typ)
{
    /* Hack -- assume obvious */
    bool obvious = true;

    /* Player blind-ness */
    bool blind = (p_ptr->blind ? true : false);

    /* Source monster */
    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Monster name (for attacks) */
    char m_name[80];

    /* Monster name (for damage) */
    char killer[80];

    int dam;

    bool do_disturb = true;

    // Sil-y: unusued parameter, casting it to soothe compilation warnings
    (void)dif;

    /* No player here */
    if (!(cave_m_idx[y][x] < 0))
        return (false);

    /* Never affect projector */
    if (cave_m_idx[y][x] == who)
        return (false);

    /* Get the source monster */
    m_ptr = &mon_list[who];

    /* Get the monster race. */
    r_ptr = &r_info[m_ptr->r_idx];

    if (who > 0 && who < mon_max) {
        killer_mark_monster(m_ptr);
    } else {
        killer_mark_other(SCORE_KILLER_OTHER);
    }

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Get the monster's real name */
    monster_desc(killer, sizeof(killer), m_ptr, 0x88);

    dam = damroll(dd, ds);

    // generate the display messages for undodgable attacks
    if ((dam > 0) && (typ != GF_ARROW) && (typ != GF_BOULDER)
        && (typ != GF_WEB))
    {
        update_combat_rolls1b(m_ptr, PLAYER, m_ptr->ml);

        if ((typ != GF_FIRE) && (typ != GF_COLD) && (typ != GF_POIS)
            && (typ != GF_DARK))
        {
            update_combat_rolls2(dd, ds, dam, -1, -1, 0, 0, typ, false);
        }
    }

    /* Analyze the damage */
    switch (typ)
    {
    /* Standard damage -- can damage carried items too */
    case GF_ACID:
    {
        if (blind)
            msg_print("You are hit by acid!");
        acid_dam(dam, dd, dd * ds, dam, killer);
        break;
    }

    /* Standard damage -- can damage carried items too */
    case GF_ELEC:
    {
        if (blind)
            msg_print("You are hit by lightning!");
        elec_dam(dam, dd, dd * ds, dam, killer);
        break;
    }

    /* Standard damage -- can damage carried items too */
    case GF_FIRE:
    {
        if (blind)
            msg_print("You are hit by fire!");
        fire_dam_pure(dd, ds, true, killer);
        break;
    }

    /* Standard damage -- can damage carried items too */
    case GF_COLD:
    {
        if (blind)
            msg_print("You are hit by cold!");
        cold_dam_pure(dd, ds, true, killer);
        break;
    }

    /* Dark  */
    case GF_DARK:
    {
        if (blind)
            msg_print("You are hit by something!");
        dark_dam_pure(dd, ds, true, killer);
        break;
    }

    /* Weak Dark -- nothing! */
    case GF_DARK_WEAK:
    {
        do_disturb = false;
        obvious = false;
        break;
    }

    /* Posion */
    case GF_POIS:
    {
        if (blind)
            msg_print("You are hit by poison!");
        (void)pois_dam_pure(dd, ds, true);
        break;
    }

    /* Arrow */
    case GF_ARROW:
    {
        int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
        int total_dd, total_ds;
        int prt, net_dam, weight;

        // attacks with GF_ARROW will require an attack roll

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        // target only gets half the evasion modifier against archery
        total_evasion_mod /= 2;

        // simulate weights of longbows and shortbows
        if (ds >= 11)
            weight = 30;
        else
            weight = 20;

        // perform the hit roll
        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);

        if (hit_result > 0)
        {
            crit_bonus_dice = crit_bonus(
                hit_result, weight, &r_info[0], S_ARC, false, m_ptr, NULL);
            total_dd = dd + crit_bonus_dice;
            total_ds = ds;

            dam = damroll(total_dd, total_ds);

            // armour is effective against GF_ARROW
            prt = protection_roll(GF_HURT, false);
            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            if (blind)
            {
                msg_print("You are hit by something sharp.");
            }
            else
            {
                if (net_dam > 0)
                {
                    if (crit_bonus_dice == 0)
                    {
                        msg_print("It hits you.");
                    }
                    else
                    {
                        msg_print("It hits!");
                    }
                }
            }

            update_combat_rolls2(
                total_dd, total_ds, dam, -1, -1, prt, 100, GF_HURT, false);
            display_hit(p_ptr->py, p_ptr->px, net_dam, GF_HURT, p_ptr->is_dead);

            if (net_dam)
            {
                take_hit(net_dam, killer);

                // deal with crippling shot ability
                if ((r_ptr->flags2 & (RF2_CRIPPLING)) && (crit_bonus_dice >= 1)
                    && (net_dam > 0))
                {
                    // Sil-y: ideally we'd use a call to allow_player_slow()
                    // here, but that doesn't
                    //        work as it can't take the level of the critical
                    //        into account. Sadly my solution doesn't let you ID
                    //        free action items.
                    int difficulty
                        = p_ptr->skill_use[S_WIL] + (p_ptr->free_act * 10);

                    if (skill_check(
                            m_ptr, crit_bonus_dice * 4, difficulty, PLAYER)
                        > 0)
                    {
                        monster_lore* l_ptr = &l_list[m_ptr->r_idx];

                        // remember that the monster can do this
                        if (m_ptr->ml)
                            l_ptr->flags2 |= (RF2_CRIPPLING);

                        msg_format("The shot tears into your thigh!");

                        // slow the player
                        set_slow(p_ptr->slow + crit_bonus_dice);
                    }
                }
            }

            /* Make some noise */
            monster_perception(true, false, -5);
        }

        break;
    }

    /* Boulder */
    /* mostly the same as GF_ARROW, but doing 6d4 damage instead*/
    case GF_BOULDER:
    {
        int total_attack_mod, total_evasion_mod, crit_bonus_dice, hit_result;
        int total_dd, total_ds;
        int prt, net_dam;

        // attacks with GF_BOULDER will require an attack roll

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        // perform the hit roll
        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);

        if (hit_result > 0)
        {
            crit_bonus_dice
                = crit_bonus(hit_result, 100, &r_info[0], S_ARC, true, m_ptr, NULL);
            total_dd = dd + crit_bonus_dice;
            total_ds = ds;

            dam = damroll(total_dd, total_ds);

            // armour is effective against GF_BOULDER
            prt = protection_roll(GF_HURT, false);
            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            if (blind)
            {
                msg_print("You are hit by something very heavy.");
            }
            else
            {
                if (net_dam > 0)
                {
                    if (crit_bonus_dice == 0)
                    {
                        msg_print("It hits you.");
                    }
                    else
                    {
                        msg_print("It hits!");
                    }
                }
            }

            update_combat_rolls2(
                total_dd, total_ds, dam, -1, -1, prt, 100, GF_HURT, false);
            display_hit(p_ptr->py, p_ptr->px, net_dam, GF_HURT, p_ptr->is_dead);

            if (net_dam)
            {
                take_hit(net_dam, killer);
            }

            /* Make some noise */
            monster_perception(true, false, -10);
        }

        break;
    }

    case GF_WEB:
    {
        int total_attack_mod, total_evasion_mod, hit_result;
        // attacks with GF_WEB will require an attack roll

        // determine the monster's attack score
        total_attack_mod = total_monster_attack(m_ptr, r_ptr->spell_power);

        // determine the player's evasion score
        total_evasion_mod = total_player_evasion(m_ptr, false);

        // perform the hit roll
        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, m_ptr, PLAYER, true);

        if (hit_result > 0)
        {
            int feat = cave_feat[p_ptr->py][p_ptr->px];
            bool can_web = (feat == FEAT_FLOOR || feat == FEAT_TRAP_WEB);

            if (can_web)
            {
                if (blind)
                {
                    msg_print("Something sticky falls over you.");
                }
                else
                {
                    msg_print("You are enveloped in a thick web.");
                }

                cave_set_feat(p_ptr->py, p_ptr->px, FEAT_TRAP_WEB);
            }
            else
            {
                if (blind)
                {
                    msg_print("Something sticky splatters nearby.");
                }
                else
                {
                    msg_print("The web cannot take hold here.");
                }
            }
        }

        break;
    }

    /* Sound (use "dam" as stunning) */
    case GF_SOUND:
    {
        if (blind)
            msg_print("You are hit by a cacophony of sound!");
        if (allow_player_stun(m_ptr))
        {
            (void)set_stun(p_ptr->stun + dam);
        }
        else
        {
            msg_print("You are unfazed.");
        }
        /* Sound uses stun instead of HP, so use the inflicted sound magnitude
         * as the hurt gate for carried-item shattering. */
        sound_dam(dam, dd, dd * ds, dam);
        break;
    }

    /* Does nothing */
    case GF_NOTHING:
    {
        do_disturb = false;
        obvious = false;
        break;
    }

    /* Default */
    default:
    {
        /* No damage */
        dam = 0;

        break;
    }
    }

    /* Disturb */
    if (do_disturb)
        disturb(1, 0);

    p_ptr->window |= (PW_COMBAT_ROLLS);

    /* Return "Anything seen?" */
    return (obvious);
}

/*
 * Calculate and store the arcs used to make starbursts.
 */
static void calc_starburst(
    int height, int width, byte* arc_first, byte* arc_dist, int* arc_num)
{
    int i;
    int size, dist, vert_factor;
    int degree_first, center_of_arc;

    /* Note the "size" */
    size = 2 + div_round(width + height, 22);

    /* Ask for a reasonable number of arcs. */
    *arc_num = 8 + (height * width / 80);
    *arc_num = rand_spread(*arc_num, 3);
    if (*arc_num < 8)
        *arc_num = 8;
    if (*arc_num > 45)
        *arc_num = 45;

    /* Determine the start degrees and expansion distance for each arc. */
    for (degree_first = 0, i = 0; i < *arc_num; i++)
    {
        /* Get the first degree for this arc (using 180-degree circles). */
        arc_first[i] = degree_first;

        /* Get a slightly randomized start degree for the next arc. */
        degree_first += div_round(180, *arc_num);

        /* Do not entirely leave the usual range */
        if (degree_first < 180 * (i + 1) / *arc_num)
            degree_first = 180 * (i + 1) / *arc_num;
        if (degree_first > (180 + *arc_num) * (i + 1) / *arc_num)
            degree_first = (180 + *arc_num) * (i + 1) / *arc_num;

        /* Get the center of the arc (convert from 180 to 360 circle). */
        center_of_arc = degree_first + arc_first[i];

        /* Get arc distance from the horizontal (0 and 180 degrees) */
        if (center_of_arc <= 90)
            vert_factor = center_of_arc;
        else if (center_of_arc >= 270)
            vert_factor = ABS(center_of_arc - 360);
        else
            vert_factor = ABS(center_of_arc - 180);

        /*
         * Usual case -- Calculate distance to expand outwards.  Pay more
         * attention to width near the horizontal, more attention to height
         * near the vertical.
         */
        dist = ((height * vert_factor) + (width * (90 - vert_factor))) / 90;

        /* Randomize distance (should never be greater than radius) */
        arc_dist[i] = rand_range(dist / 4, dist / 2);

        /* Keep variability under control (except in special cases). */
        if ((dist != 0) && (i != 0))
        {
            int diff = arc_dist[i] - arc_dist[i - 1];

            if (ABS(diff) > size)
            {
                if (diff > 0)
                    arc_dist[i] = arc_dist[i - 1] + size;
                else
                    arc_dist[i] = arc_dist[i - 1] - size;
            }
        }
    }

    /* Neaten up final arc of circle by comparing it to the first. */
    if (true)
    {
        int diff = arc_dist[*arc_num - 1] - arc_dist[0];

        if (ABS(diff) > size)
        {
            if (diff > 0)
                arc_dist[*arc_num - 1] = arc_dist[0] + size;
            else
                arc_dist[*arc_num - 1] = arc_dist[0] - size;
        }
    }
}

/*
 * Generic "beam"/"bolt"/"ball" projection routine.
 *
 * Input:
 *   who: Index of "source" monster (negative for "player")
 *   rad: Radius of explosion (0 = beam/bolt, 1 to 9 = ball)
 *   y,x: Target location (or location to travel "towards")
 *   dam: Base damage roll to apply to affected monsters (or player)
 *   typ: Type of damage to apply to monsters (and objects)
 *   flg: Extra bit flags (see PROJECT_xxxx in "defines.h")
 *   degrees: How wide an arc spell is (in degrees).
 *   uniform: uniform means no damage reduction with range, otherwise it is one
 * die per square.
 *
 * Return:
 *   true if any "effects" of the projection were observed, else false
 *
 * At present, there are five major types of projections:
 *
 * Point-effect projection:  (no PROJECT_BEAM flag, radius of zero, and either
 *   jumps directly to target or has a single source and target grid)
 * A point-effect projection has no line of projection, and only affects one
 *   grid.  It is used for most area-effect spells (like dispel evil) and
 *   pinpoint strikes.
 *
 * Bolt:  (no PROJECT_BEAM flag, radius of zero, has to travel from source to
 *   target)
 * A bolt travels from source to target and affects only the final grid in its
 *   projection path.  If given the PROJECT_STOP flag, it is stopped by any
 *   monster or character in its path (at present, all bolts use this flag).
 *
 * Beam:  (PROJECT_BEAM)
 * A beam travels from source to target, affecting all grids passed through
 *   with full damage.  It is never stopped by monsters in its path.  Beams
 *   may never be combined with any other projection type.
 *
 * Ball:  (positive radius, unless the PROJECT_ARC flag is set)
 * A ball travels from source towards the target, and always explodes.  Unless
 *   specified, it does not affect wall grids, but otherwise affects any grids
 *   in LOS from the center of the explosion.
 * If used with a direction, a ball will explode on the first occupied grid in
 *   its path.  If given a target, it will explode on that target.  If a
 *   wall is in the way, it will explode against the wall.  If a ball reaches
 *   MAX_RANGE without hitting anything or reaching its target, it will
 *   explode at that point.
 *
 * Arc:  (positive radius, with the PROJECT_ARC flag set)
 * An arc is a portion of a source-centered ball that explodes outwards
 *   towards the target grid.  Like a ball, it affects all non-wall grids in
 *   LOS of the source in the explosion area.  The width of arc spells is con-
 *   trolled by degrees.
 * An arc is created by rejecting all grids that form the endpoints of lines
 *   whose angular difference (in degrees) from the centerline of the arc is
 *   greater than one-half the input "degrees".  See the table "get_
 *   angle_to_grid" in "util.c" for more information.
 * Note:  An arc with a value for degrees of zero is actually a beam of
 *   defined length.
 *
 * Projections that affect all monsters in LOS are handled through the use
 *   of "project_los()", which applies a single-grid projection to individual
 *   monsters.  Projections that light up rooms or affect all monsters on the
 *   level are more efficiently handled through special functions.
 *
 *
 * Variations:
 *
 * PROJECT_STOP forces a path of projection to stop at the first occupied
 *   grid it hits.  This is used with bolts, and also by ball spells
 *   travelling in a specific direction rather than towards a target.
 *
 * PROJECT_THRU allows a path of projection towards a target to continue
 *   past that target.
 *
 * PROJECT_JUMP allows a projection to immediately set the source of the pro-
 *   jection to the target.  This is used for all area effect spells (like
 *   dispel evil), and can also be used for bombardments.
 *
 * PROJECT_WALL allows a projection, not just to affect one layer of any
 *   passable wall (rubble, trees), but to affect the surface of any wall.
 *   Certain projection types always have this flag.
 *
 * PROJECT_PASS allows projections to ignore walls completely.
 *   Certain projection types always have this flag.
 *
 * PROJECT_HIDE erases all graphical effects, making the projection
 *   invisible.
 *
 * PROJECT_GRID allows projections to affect terrain features.
 *
 * PROJECT_ITEM allows projections to affect objects on the ground.
 *
 * PROJECT_KILL allows projections to affect monsters.
 *
 * PROJECT_PLAY allows projections to affect the player.
 *
 * degrees controls the width of arc spells.  With a value for
 *   degrees of zero, arcs act like beams of defined length.
 *
 * Implementation notes:
 *
 * If the source grid is not the same as the target, we project along the path
 *   between them.  Bolts stop if they hit anything, beams stop if they hit a
 *   wall, and balls and arcs may exhibit either behavior.  When they reach
 *   the final grid in the path, balls and arcs explode.  We do not allow beams
 *   to be combined with explosions.
 * Balls affect all floor grids in LOS (optionally, also wall grids adjacent
 *   to a grid in LOS) within their radius.  Arcs do the same, but only within
 *   their cone of projection.
 * Because affected grids are only scanned once, and it is really helpful to
 *   have explosions that travel outwards from the source, they are sorted by
 *   distance.  For each distance, an adjusted damage is calculated.
 * In successive passes, the code then displays explosion graphics, erases
 *   these graphics, marks terrain for possible later changes, affects
 *   objects, monsters, the character, and finally changes features and
 *   teleports monsters and characters in marked grids.
 *
 *
 * Usage and graphics notes:
 *
 * If the option "fresh_before" is on, or the delay factor is anything other
 * than zero, bolt and explosion pictures will be momentarily shown on screen.
 *
 * Only 256 grids can be affected per projection, limiting the effective
 * radius of standard ball attacks to nine units (diameter nineteen).  Arcs
 * can have larger radii; an arc capable of going out to range 20 should not
 * be wider than 70 degrees.
 *
 * Balls must explode BEFORE hitting walls, or they would affect monsters on
 * both sides of a wall.
 *
 * Note that for consistency, we pretend that the bolt actually takes time
 * to move from point A to point B, even if the player cannot see part of the
 * projection path.  Note that in general, the player will *always* see part
 * of the path, since it either starts at the player or ends on the player.
 *
 * Hack -- we assume that every "projection" is "self-illuminating".
 *
 * Hack -- when only a single monster is affected, we automatically track
 * (and recall) that monster, unless "PROJECT_JUMP" is used.
 *
 * Note that we must call "handle_stuff()" after affecting terrain features
 * in the blast radius, in case the illumination of the grid was changed,
 * and "update_view()" and "update_monsters()" need to be called.
 */
bool project(int who, int rad, int y0, int x0, int y1, int x1, int dd, int ds,
    int dif, int typ, u32b flg, int degrees, bool uniform)
{
    int i, j, k;
    int dist = 0;

    u32b dam_temp;
    int centerline = 0;

    int y = y0;
    int x = x0;
    int n1y = 0;
    int n1x = 0;
    int y2, x2;

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    /* Assume the player sees nothing */
    bool notice = false;

    /* Assume the player has seen nothing */
    bool visual = false;

    /* Assume the player has seen no blast grids */
    bool drawn = false;

    /* Is the player blind? */
    bool blind = (p_ptr->blind ? true : false);

    /* Number of grids in the "path" */
    int path_n = 0;

    /* Actual grids in the "path" */
    u16b path_g[512];

    /* Number of grids in the "blast area" (including the "beam" path) */
    int grids = 0;

    /* Coordinates of the affected grids */
    byte gx[256], gy[256];

    /* Distance to each of the affected grids. */
    byte gd[256];

    /* Precalculated damage values for each distance. */
    int dam_at_dist[MAX_RANGE + 1];

    /*
     * Starburst projections only --
     * Holds first degree of arc, maximum effect distance in arc.
     */
    byte arc_first[45];
    byte arc_dist[45];

    /* Number (max 45) of arcs. */
    int arc_num = 0;

    int degree, max_dist;

    /* Hack -- Flush any pending output */
    handle_stuff();

    /* Make certain that the radius is not too large */
    if (rad > MAX_SIGHT)
        rad = MAX_SIGHT;

    /* Some projection types always PROJECT_WALL. */
    if ((typ == GF_KILL_WALL) || (typ == GF_KILL_DOOR))
    {
        flg |= (PROJECT_WALL);
    }

    /* Hack -- Jump to target, but require a valid target */
    if ((flg & (PROJECT_JUMP)) && (y1) && (x1))
    {
        y0 = y1;
        x0 = x1;

        /* Clear the flag */
        flg &= ~(PROJECT_JUMP);
    }

    /* If a single grid is both source and destination, store it. */
    if ((x1 == x0) && (y1 == y0))
    {
        gy[grids] = y0;
        gx[grids] = x0;
        gd[grids++] = 0;
    }

    /* Otherwise, unless an arc or a star, travel along the projection path. */
    else if (!(flg & (PROJECT_ARC | PROJECT_STAR)))
    {
        /* Determine maximum length of projection path */
        if (flg & (PROJECT_BOOM))
            dist = MAX_RANGE;
        else if (rad <= 0)
            dist = MAX_RANGE;
        else
            dist = rad;

        /* Calculate the projection path */
        path_n = project_path(path_g, dist, y0, x0, &y1, &x1, flg);

        /* Project along the path */
        for (i = 0; i < path_n; ++i)
        {
            int oy = y;
            int ox = x;

            int ny = GRID_Y(path_g[i]);
            int nx = GRID_X(path_g[i]);

            /* Hack -- Balls explode before reaching walls. */
            if ((flg & (PROJECT_BOOM)) && (!cave_floor_bold(ny, nx)))
            {
                break;
            }

            /* Advance */
            y = ny;
            x = nx;

            /* If a beam, collect all grids in the path. */
            if (flg & (PROJECT_BEAM))
            {
                gy[grids] = y;
                gx[grids] = x;
                gd[grids++] = 0;
            }

            /* Otherwise, collect only the final grid in the path. */
            else if (i == path_n - 1)
            {
                gy[grids] = y;
                gx[grids] = x;
                gd[grids++] = 0;
            }

            /* Only do visuals if requested */
            if (!blind && !(flg & (PROJECT_HIDE)))
            {
                /* Only do visuals if the player can "see" the projection */
                if (panel_contains(y, x) && player_has_los_bold(y, x))
                {
                    u16b p;

                    byte a;
                    char c;

                    /* Obtain the bolt pict */
                    p = bolt_pict(oy, ox, y, x, typ);

                    /* Extract attr/char */
                    a = PICT_A(p);
                    c = PICT_C(p);

                    /* Display the visual effects */
                    print_rel(c, a, y, x);
                    move_cursor_relative(y, x);
                    if (op_ptr->delay_factor)
                        Term_fresh();

                    /* Delay */
                    Term_xtra(TERM_XTRA_DELAY, msec);

                    /* Erase the visual effects */
                    lite_spot(y, x);
                    if (op_ptr->delay_factor)
                        Term_fresh();

                    /* Re-display the beam  XXX */
                    if (flg & (PROJECT_BEAM))
                    {
                        /* Obtain the explosion pict */
                        p = bolt_pict(y, x, y, x, typ);

                        /* Extract attr/char */
                        a = PICT_A(p);
                        c = PICT_C(p);

                        /* Visual effects */
                        print_rel(c, a, y, x);
                    }

                    /* Hack -- Activate delay */
                    visual = true;
                }

                /* Hack -- Always delay for consistency */
                else if (visual)
                {
                    /* Delay for consistency */
                    Term_xtra(TERM_XTRA_DELAY, msec);
                }
            }
        }
    }

    /* Save the "blast epicenter" */
    y2 = y;
    x2 = x;

    /* Beams have already stored all the grids they will affect. */
    if (flg & (PROJECT_BEAM))
    {
        /* No special actions */
    }

    /* Handle explosions */
    else if (flg & (PROJECT_BOOM))
    {
        /* Some projection types always PROJECT_WALL. */
        if (typ == GF_ACID)
        {
            /* Note that acid only affects monsters if it melts the wall. */
            flg |= (PROJECT_WALL);
        }

        /* Pre-calculate some things for starbursts. */
        if (flg & (PROJECT_STAR))
        {
            calc_starburst(
                1 + rad * 2, 1 + rad * 2, arc_first, arc_dist, &arc_num);

            /* Mark the area nearby -- limit range, ignore rooms */
            spread_cave_temp(y0, x0, rad, false);
        }

        /* Pre-calculate some things for arcs. */
        if (flg & (PROJECT_ARC))
        {
            /* The radius of arcs cannot be more than 20 */
            if (rad > 20)
                rad = 20;

            /* Reorient the grid forming the end of the arc's centerline. */
            n1y = y1 - y0 + 20;
            n1x = x1 - x0 + 20;

            /* Correct overly large or small values */
            if (n1y > 40)
                n1y = 40;
            if (n1x > 40)
                n1x = 40;
            if (n1y < 0)
                n1y = 0;
            if (n1x < 0)
                n1x = 0;

            /* Get the angle of the arc's centerline */
            centerline = 90 - get_angle_to_grid[n1y][n1x];
        }

        /*
         * If the center of the explosion hasn't been
         * saved already, save it now.
         */
        if (grids == 0)
        {
            gy[grids] = y2;
            gx[grids] = x2;
            gd[grids++] = 0;
        }

        /*
         * Scan every grid that might possibly
         * be in the blast radius.
         */
        for (y = y2 - rad; y <= y2 + rad; y++)
        {
            for (x = x2 - rad; x <= x2 + rad; x++)
            {
                /* Center grid has already been stored. */
                if ((y == y2) && (x == x2))
                    continue;

                /* Precaution: Stay within area limit. */
                if (grids >= 255)
                    break;

                /* Ignore "illegal" locations */
                if (!in_bounds(y, x))
                    continue;

                /* This is a wall grid (whether passable or not). */
                if (!cave_floor_bold(y, x))
                {
                    /* Spell with PROJECT_PASS ignore walls */
                    if (!(flg & (PROJECT_PASS)))
                    {
                        /* This grid is passable, or PROJECT_WALL is active */
                        if ((flg & (PROJECT_WALL)) || (cave_floor_bold(y, x)))
                        {
                            /* Allow grids next to grids in LOS of explosion
                             * center */
                            for (i = 0, k = 0; i < 8; i++)
                            {
                                int yy = y + ddy_ddd[i];
                                int xx = x + ddx_ddd[i];

                                /* Stay within dungeon */
                                if (!in_bounds(yy, xx))
                                    continue;

                                if (los(y2, x2, yy, xx))
                                {
                                    k++;
                                    break;
                                }
                            }

                            /* Require at least one adjacent grid in LOS */
                            if (!k)
                                continue;
                        }

                        /* We can't affect this non-passable wall */
                        else
                            continue;
                    }
                }

                /* Must be within maximum distance. */
                dist = (distance(y2, x2, y, x));
                if (dist > rad)
                    continue;

                /* Projection is a starburst */
                if (flg & (PROJECT_STAR))
                {
                    /* Grid is within effect range */
                    if (cave_info[y][x] & (CAVE_TEMP))
                    {
                        /* Reorient current grid for table access. */
                        int ny = y - y2 + 20;
                        int nx = x - x2 + 20;

                        /* Illegal table access is bad. */
                        if ((ny < 0) || (ny > 40) || (nx < 0) || (nx > 40))
                            continue;

                        /* Get angle to current grid. */
                        degree = get_angle_to_grid[ny][nx];

                        /* Scan arcs to find the one that applies here. */
                        for (i = arc_num - 1; i >= 0; i--)
                        {
                            if (arc_first[i] <= degree)
                            {
                                max_dist = arc_dist[i];

                                /* Must be within effect range. */
                                if (max_dist >= dist)
                                {
                                    gy[grids] = y;
                                    gx[grids] = x;
                                    gd[grids] = 0;
                                    grids++;
                                }

                                /* Arc found.  End search */
                                break;
                            }
                        }
                    }
                }

                /* Use angle comparison to delineate an arc. */
                else if (flg & (PROJECT_ARC))
                {
                    int n2y, n2x, tmp, diff;

                    /* Reorient current grid for table access. */
                    n2y = y - y2 + 20;
                    n2x = x - x2 + 20;

                    /*
                     * Find the angular difference (/2) between
                     * the lines to the end of the arc's center-
                     * line and to the current grid.
                     */
                    tmp = ABS(get_angle_to_grid[n2y][n2x] + centerline) % 180;
                    diff = ABS(90 - tmp);

                    /*
                     * If difference is not greater then that
                     * allowed, and the grid is in LOS, accept it.
                     */
                    if (diff < (degrees + 6) / 4)
                    {
                        if (los(y2, x2, y, x))
                        {
                            gy[grids] = y;
                            gx[grids] = x;
                            gd[grids] = dist;
                            grids++;
                        }
                    }
                }

                /* Standard ball spell -- accept all grids in LOS. */
                else
                {
                    if (flg & (PROJECT_PASS) || los(y2, x2, y, x))
                    {
                        gy[grids] = y;
                        gx[grids] = x;
                        gd[grids] = dist;
                        grids++;
                    }
                }
            }
        }
    }

    /* Clear the "temp" array  XXX */
    clear_temp_array();

    /* Calculate and store the actual damage at each distance. */
    for (i = 0; i <= MAX_RANGE; i++)
    {
        /* No damage outside the radius. */
        if (i > rad)
            dam_temp = 0;

        /* No damage reduction with range if uniform. */
        else if (uniform)
        {
            dam_temp = dd;
        }

        /* Otherwise, lose two dice per square. */
        else
        {
            if (dd > 2 * i)
                dam_temp = dd - 2 * i;
            else
                dam_temp = 0;
        }

        /* Store it. */
        dam_at_dist[i] = dam_temp;
    }

    /* Sort the blast grids by distance, starting at the origin. */
    for (i = 0, k = 0; i < rad; i++)
    {
        int tmp_y, tmp_x, tmp_d;

        /* Collect all the grids of a given distance together. */
        for (j = k; j < grids; j++)
        {
            if (gd[j] == i)
            {
                tmp_y = gy[k];
                tmp_x = gx[k];
                tmp_d = gd[k];

                gy[k] = gy[j];
                gx[k] = gx[j];
                gd[k] = gd[j];

                gy[j] = tmp_y;
                gx[j] = tmp_x;
                gd[j] = tmp_d;

                /* Write to next slot */
                k++;
            }
        }
    }

    /* Display the "blast area" if allowed */
    if (!blind && !(flg & (PROJECT_HIDE)))
    {
        /* Do the blast from inside out */
        for (i = 0; i < grids; i++)
        {
            /* Extract the location */
            y = gy[i];
            x = gx[i];

            /* Only do visuals if the player can "see" the blast */
            if (panel_contains(y, x) && player_has_los_bold(y, x))
            {
                u16b p;

                byte a;
                char c;

                drawn = true;

                /* Obtain the explosion pict */
                p = bolt_pict(y, x, y, x, typ);

                /* Extract attr/char */
                a = PICT_A(p);
                c = PICT_C(p);

                /* Visual effects -- Display */
                print_rel(c, a, y, x);
            }

            /* Hack -- center the cursor */
            move_cursor_relative(y2, x2);

            /* New radius is about to be drawn */
            if ((i == grids - 1) || ((i < grids - 1) && (gd[i + 1] > gd[i])))
            {
                /* Flush each radius separately */
                if (op_ptr->delay_factor)
                    Term_fresh();

                /* Delay (efficiently) */
                if (visual || drawn)
                {
                    Term_xtra(TERM_XTRA_DELAY, msec);
                }
            }
        }

        /* Delay for a while if there are pretty graphics to show */
        if ((grids > 1) && (visual || drawn))
        {
            if (!op_ptr->delay_factor)
                Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, 50 + msec);
        }

        /* Flush the erasing -- except if we specify lingering graphics */
        if ((drawn) && (!(flg & (PROJECT_NO_REDRAW))))
        {
            /* Erase the explosion drawn above */
            for (i = 0; i < grids; i++)
            {
                /* Extract the location */
                y = gy[i];
                x = gx[i];

                /* Hack -- Erase if needed */
                if (panel_contains(y, x) && player_has_los_bold(y, x))
                {
                    lite_spot(y, x);
                }
            }

            /* Hack -- center the cursor */
            move_cursor_relative(y2, x2);

            /* Flush the explosion */
            if (op_ptr->delay_factor)
                Term_fresh();
        }
    }

    /* Check features */
    if (flg & (PROJECT_GRID))
    {
        /* Scan for features */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the feature in that grid */
            if (project_f(who, y, x, gd[i], dam_at_dist[gd[i]], ds, dif, typ))
                notice = true;
        }
    }

    /* Check objects */
    if (flg & (PROJECT_ITEM))
    {
        /* Scan for objects */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the object in the grid */
            if (project_o(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
                notice = true;
        }
    }

    /* Check monsters */
    if (flg & (PROJECT_KILL))
    {
        /* Mega-Hack */
        project_m_n = 0;
        project_m_x = 0;
        project_m_y = 0;
        death_count = 0;

        /* Scan for monsters */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Affect the monster in the grid */
            if (project_m(who, y, x, dam_at_dist[gd[i]], ds, dif, typ, flg))
                notice = true;
        }

        /* Player affected one monster (without "jumping") */
        if ((who < 0) && (project_m_n == 1) && !(flg & (PROJECT_JUMP)))
        {
            /* Location */
            x = project_m_x;
            y = project_m_y;

            /* Track if possible */
            if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

                /* Hack -- auto-recall */
                if (m_ptr->ml)
                    monster_race_track(m_ptr->r_idx);

                /* Hack - auto-track */
                // Sil-y: turned this off experimentally
                // if (m_ptr->ml) health_track(cave_m_idx[y][x]);
            }
        }

        /* Hack -- Moria-style death messages for non-visible monsters */
        if (death_count)
        {
            /* One monster */
            if (death_count == 1)
            {
                msg_print("You hear a scream of agony!");
            }

            /* Several monsters */
            else
            {
                msg_print("You hear several screams of agony!");
            }

            /* Reset */
            death_count = 0;
        }
    }

    /* Check player */
    if (flg & (PROJECT_PLAY))
    {
        /* Scan for player */
        for (i = 0; i < grids; i++)
        {
            /* Get the grid location */
            y = gy[i];
            x = gx[i];

            /* Player is in this grid */
            if (cave_m_idx[y][x] < 0)
            {
                /* Affect the player */
                if (project_p(who, y, x, dam_at_dist[gd[i]], ds, dif, typ))
                {
                    notice = true;

                    /* Only affect the player once */
                    break;
                }
            }
        }
    }

    /* Clear the "temp" array  (paranoia is good) */
    clear_temp_array();

    /* Update stuff if needed */
    if (p_ptr->update)
        update_stuff();

    /* Return "something was noticed" */
    return (notice);
}

/*
 *  Do the effects of Song of Freedom
 */
void sing_song_of_freedom(int score)
{
    int y, x;
    int base_difficulty, difficulty;
    int result;
    int new_feat;
    object_type* o_ptr;
    bool closed_chasm = false;

    // set the base difficulty
    if (p_ptr->depth > 0)
    {
        base_difficulty = p_ptr->depth / 2;
    }
    else
    {
        base_difficulty = 10;
    }

    /* Scan the map */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            // get the object present (if any)
            o_ptr = &o_list[cave_o_idx[y][x]];

            /* Locked/trapped chest */
            if (o_ptr->tval == TV_CHEST)
            {
                /* Disarm/Unlock traps */
                if (o_ptr->pval > 0)
                {
                    difficulty = base_difficulty + 5
                        + flow_dist(FLOW_PLAYER_NOISE, y, x);
                    if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                    {
                        /* Disarm or Unlock */
                        o_ptr->pval = (0 - o_ptr->pval);

                        /* Identify */
                        object_known(o_ptr);
                    }
                }
            }

            // Chasm
            else if (cave_feat[y][x] == FEAT_CHASM)
            {
                closed_chasm |= close_chasm(
                    y, x, score - flow_dist(FLOW_PLAYER_NOISE, y, x) - 5);
            }

            /* Invisible trap */
            else if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                difficulty
                    = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Remove the trap */
                    cave_feat[y][x] = FEAT_FLOOR;
                }
            }

            /* Visible trap */
            else if (cave_trap_bold(y, x))
            {
                difficulty
                    = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Remove the trap */
                    cave_feat[y][x] = FEAT_FLOOR;

                    if (cave_info[y][x] & (CAVE_SEEN))
                    {
                        lite_spot(y, x);
                    }
                }
            }

            /* Secret door */
            else if (cave_feat[y][x] == FEAT_SECRET)
            {
                difficulty
                    = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Pick a door */
                    place_closed_door(y, x);

                    if (cave_info[y][x] & (CAVE_SEEN))
                    {
                        /* Message */
                        msg_print("You have found a secret door.");

                        /* Disturb */
                        disturb(0, 0);
                    }
                }
            }

            /* Stuck door */
            else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
                && (cave_feat[y][x] <= FEAT_DOOR_TAIL))
            {
                difficulty
                    = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    new_feat = cave_feat[y][x] - result;

                    if (new_feat <= FEAT_DOOR_HEAD + 0x08)
                        new_feat = FEAT_DOOR_HEAD;

                    cave_feat[y][x] = new_feat;
                }
            }

            /* Locked door */
            else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
                && (cave_feat[y][x] <= FEAT_DOOR_HEAD + 0x07))
            {
                difficulty
                    = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    new_feat = cave_feat[y][x] - result;

                    if (new_feat < FEAT_DOOR_HEAD)
                        new_feat = FEAT_DOOR_HEAD;

                    cave_feat[y][x] = new_feat;
                }
            }

            /* Rubble */
            else if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                int noise_dist = 100;
                int d, dir;

                // check adjacent squares for valid noise distances, since
                // rubble is impervious to sound
                for (d = 0; d < 8; d++)
                {
                    dir = cycle[d];
                    noise_dist = MIN(noise_dist,
                        flow_dist(
                            FLOW_PLAYER_NOISE, y + ddy[dir], x + ddx[dir]));
                }
                noise_dist++;

                difficulty = base_difficulty + 5 + noise_dist;
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    /* Disperse the rubble */
                    cave_set_feat(y, x, FEAT_FLOOR);

                    /* Update the flow code */
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }

    // then, if any chasms were marked to be closed, do the closing
    if (closed_chasm)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_CHASM)
                    && (cave_info[y][x] & (CAVE_TEMP)))
                {
                    // remove the temporary marking
                    cave_info[y][x] &= ~(CAVE_TEMP);

                    // close the chasm
                    cave_set_feat(y, x, FEAT_FLOOR);

                    // update the visuals
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }
}

/*
 *  Do the effects of (the monster song) Song of Binding
 */
void song_of_binding(monster_type* m_ptr)
{
    int y, x;
    int resistance;
    int result;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = r_info[m_ptr->r_idx].wil; // Use Will as song skill

    if (m_ptr->song_lockout_timer > 0 && m_ptr->song != SNG_BINDING)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if (m_ptr->song != SNG_BINDING)
    {
        msg_format("%^s begins a song of binding.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_BINDING;

        // disturb if message printed
        disturb(1, 0);
    }

    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
        case 1:
            description = "durance";
            break;
        case 2:
            description = "chains";
            break;
        case 3:
            description = "thralls";
            break;
        case 4:
            description = "prison walls";
            break;
        case 5:
            description = "locks without keys";
            break;
        default:
            description = "binding";
        }

        if (m_ptr->ml)
            msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)
            msg_format("You hear a song of %s.", description);
        else if (dist <= 30)
            msg_print("You hear singing in the distance.");

        // disturb if message printed
        if (m_ptr->ml || (dist <= 30))
            disturb(1, 0);
    }

    // use the monster noise flow to represent the song levels at each square
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);

    // Morgoth's throne-room fight should not revolve around him repeatedly
    // re-closing the hall's doors, but keep the song's slowing effect.
    if (!((m_ptr->r_idx == R_IDX_MORGOTH)
            && (p_ptr->depth == MORGOTH_DEPTH)
            && p_ptr->morgoth_hall_entered
            && !p_ptr->on_the_run))
    {
        // scan the map, closing doors
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if (!in_bounds_fully(y, x))
                    continue;

                // if there is no player/monster in the square
                if (cave_m_idx[y][x] == 0)
                {
                    // if it is a door
                    if ((cave_feat[y][x] == FEAT_OPEN)
                        || (cave_feat[y][x] == FEAT_BROKEN)
                        || cave_known_closed_door_bold(y, x))
                    {
                        // if the door isn't between the monster and the player
                        if (!(ORDERED(m_ptr->fy, y, p_ptr->py)
                                && ORDERED(m_ptr->fx, x, p_ptr->px)))
                        {
                            result = skill_check(m_ptr, song_skill,
                                15 + flow_dist(FLOW_MONSTER_NOISE, y, x), NULL);

                            (void)lock_door(y, x, result);
                        }
                    }
                }
            }
        }
    }

    /*
    // scan the map, slowing monsters
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x)) continue;

            // if there is a monster in the square
            if ((cave_m_idx[y][x] > 0) && !((y == m_ptr->fy) && (x ==
    m_ptr->fx)))
            {
                monster_type *n_ptr = &mon_list[cave_m_idx[y][x]];

                resistance = monster_skill(n_ptr, S_WIL) + 5 +
    flow_dist(FLOW_MONSTER_NOISE, y, x);

                result = skill_check(m_ptr, song_skill, resistance, n_ptr);

                // if the check succeeds, the monster is slowed for at least 2
    rounds if (result > 0)
                {
                    set_monster_slow(cave_m_idx[y][x], MAX(m_ptr->slowed, 2),
    mon_list[cave_m_idx[y][x]].ml);
                }
            }
        }
    }
    */

    // if the player is singing the song of silence, then the  monster suffers a
    // penalty
    if (singing(SNG_SILENCE))
        song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;

    // determine the player's resistance
    // Sil-y: might want to add in the same +5 bonus as against Mastery and
    // Lorien
    resistance = p_ptr->skill_use[S_WIL] + (p_ptr->free_act * 10)
        + flow_dist(FLOW_MONSTER_NOISE, p_ptr->py, p_ptr->px);

    // Sil-y: ideally we'd use a call to allow_player_slow() here, but that
    // doesn't
    //        work as it can't take the noise distance into account.
    //        Sadly my solution doesn't let you ID free action items.
    result = skill_check(m_ptr, song_skill, resistance, PLAYER);

    // if the check succeeds, the player is slowed for at least 2 rounds
    // note that only the first of these affects you as you aren't slow on the
    // round it wears off
    if (result > 0)
    {
        set_slow(MAX(p_ptr->slow, 2));
    }
}

/*
 *  Do the effects of (the monster song) Song of Piercing
 */
void song_of_piercing(monster_type* m_ptr)
{
    int resistance;
    int result;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = r_info[m_ptr->r_idx].wil; // Use Will as song skill

    if (m_ptr->song_lockout_timer > 0 && m_ptr->song != SNG_PIERCING)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if ((m_ptr->song != SNG_PIERCING) && m_ptr->ml)
    {
        msg_format("%^s begins a song of piercing.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_PIERCING;

        // disturb if message printed
        disturb(1, 0);
    }

    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
        case 1:
            description = "opening";
            break;
        case 2:
            description = "treachery";
            break;
        case 3:
            description = "revealing";
            break;
        case 4:
            description = "uncovering";
            break;
        case 5:
            description = "betraying";
            break;
        default:
            description = "piercing";
        }

        if (m_ptr->ml)
            msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)
            msg_format("You hear a song of %s.", description);
        else if (dist <= 30)
            msg_print("You hear singing in the distance.");

        // disturb if message printed
        if (m_ptr->ml || (dist <= 30))
            disturb(1, 0);
    }

    // if the player is singing the song of silence, then the  monster suffers a
    // penalty
    if (singing(SNG_SILENCE))
        song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;

    // determine the player's resistance
    resistance = p_ptr->skill_use[S_WIL] + dist + 5;

    // perform the skill check
    result = skill_check(m_ptr, song_skill, resistance, PLAYER);

    // if the check succeeds, Morgoth knows the player's location
    if (result > 0)
    {
        msg_print("You feel your mind laid bare before Morgoth's will.");
        set_alertness(m_ptr, MIN(result, ALERTNESS_VERY_ALERT));
        /* Give Morgoth a fresh pursuit target after a successful piercing. */
        m_ptr->target_y = p_ptr->py;
        m_ptr->target_x = p_ptr->px;
    }

    else if (result > -5)
    {
        msg_print(
            "You feel the force of Morgoth's will searching for the intruder.");
    }
}

/*
 *  Do the effects of (the monster song) Song of Oaths
 */
void song_of_oaths(monster_type* m_ptr)
{
    int y, x;
    int result;
    int range;
    int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
    char m_name[80];
    cptr description;

    int song_skill = 21; // Gorthaur's song skill. If more monsters get songs
                         // I'll put this in monster.txt

    if (m_ptr->song_lockout_timer > 0 && m_ptr->song != SNG_OATHS)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    // messages for beginning a new song
    if (m_ptr->song != SNG_OATHS)
    {
        msg_format("%^s begins a song of oaths.", m_name);

        // and remember the monsters is now singing this song
        m_ptr->song = SNG_OATHS;

        // disturb if message printed
        disturb(1, 0);
    }

    // messages for continuing a song
    else
    {
        switch (dieroll(8))
        {
        case 1:
            description = "vows broken";
            break;
        case 2:
            description = "promises";
            break;
        case 3:
            description = "duty";
            break;
        case 4:
            description = "tasks forgotten";
            break;
        case 5:
            description = "redemption";
            break;
        default:
            description = "oaths";
        }

        if (m_ptr->ml)
            msg_format("%^s sings of %s.", m_name, description);
        else if (dist <= 20)
            msg_format("You hear a song of %s.", description);
        else if (dist <= 30)
            msg_print("You hear singing in the distance.");

        // Disturb if message printed
        if (m_ptr->ml || (dist <= 30))
            disturb(1, 0);
    }

    // use the monster noise flow to represent the song levels at each square
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);

    // if the player is singing the song of silence, then the  monster suffers a
    // penalty
    if (singing(SNG_SILENCE))
        song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;

    // perform the skill check
    result = skill_check(m_ptr, song_skill, 15, PLAYER);

    // if the check was successful, summon an oathwraith to a nearby square
    if (result > 0)
    {
        int attempts = 10;

        // the greatest distance away the wraith can be summoned -- smaller is
        // typically better
        range = MAX(15 - result, 3);

        while (attempts--)
        {
            // choose a random square
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            if (!in_bounds(y, x))
                continue;

            // check the square is empty and close enough
            if (cave_empty_bold(y, x)
                && flow_dist(FLOW_MONSTER_NOISE, y, x) <= range)
            {
                monster_type* n_ptr;

                // place it
                place_monster_one(y, x, R_IDX_OATHWRAITH, true, false, NULL);

                n_ptr = &mon_list[cave_m_idx[y][x]];

                // message if visible
                if (n_ptr->ml)
                    msg_print("An Oathwraith appears.");

                // mark the wraith as having been summoned
                n_ptr->mflag |= (MFLAG_SUMMONED);

                // let it know where the player is
                set_alertness(n_ptr, ALERTNESS_QUITE_ALERT);

                break;
            }
        }
    }
}

void hatch_spider(monster_type* m_ptr)
{
    char m_name[80];

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);

    if (m_ptr->ml)
        msg_format("An egg on %s's back hatches.", m_name);
    reproduce_monster(cave_m_idx[m_ptr->fy][m_ptr->fx], R_IDX_SPIDER_HATCHLING);

    // Monster still gets to attack next turn
    m_ptr->energy += 50;
}

/*
 *  Allows you to change the song you are singing to a new one.
 *  If you have the ability 'woven themes' and try to sing a different song,
 *  it will add it as a theme or change the current theme.
 *  Choosing the current main song again stops singing. Choosing the current
 *  minor theme again cancels that minor theme. Starting a new song (or
 *  changing songs) takes a turn, but ending a song/theme does not.
 */

void change_song(int song)
{
    int song_to_change;
    int old_song;
    bool new_song_is_duel;
    bool old_song_is_duel;

    if (p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]
        && (p_ptr->song1 != SNG_NOTHING) && (song != SNG_NOTHING))
    {
        song_to_change = 2;
        old_song = p_ptr->song2;
    }
    else
    {
        song_to_change = 1;
        old_song = p_ptr->song1;
    }

    // attempting to change to the main song again stops singing
    if (p_ptr->song1 == song)
    {
        song_to_change = 1;
        old_song = p_ptr->song1;
        song = SNG_NOTHING;
    }

    // attempting to change minor theme to itself cancels the minor theme
    else if ((song_to_change == 2) && (p_ptr->song2 == song))
    {
        song = SNG_NOTHING;
    }

    new_song_is_duel = song_is_duel(song);
    old_song_is_duel = song_is_duel(old_song);

    if ((song_to_change == 2) && new_song_is_duel)
    {
        msg_print("That song cannot be woven as a minor theme.");
        return;
    }

    if ((song_to_change == 1) && new_song_is_duel)
    {
        if (p_ptr->song_lockout_timer > 0)
        {
            msg_print("Your voice has not yet recovered for such a song.");
            return;
        }
        if (!song_duel_select_target(song))
            return;
    }

    if ((song_to_change == 1) && old_song_is_duel && !new_song_is_duel)
    {
        song_duel_clear_player_target();
        song_duel_reset_player_stack();
    }

    // Recalculate various bonuses
    p_ptr->redraw |= (PR_SONG);
    p_ptr->update |= (PU_BONUS);

    // swap the minor and major themes
    if (song == SNG_EXCHANGE_THEMES)
    {
        p_ptr->song2 = p_ptr->song1;
        p_ptr->song1 = old_song;

        msg_print("You change the order of your themes.");

        /* Take time */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        return;
    }

    // Reset the song duration counter if changing major theme
    if (song_to_change == 1)
    {
        p_ptr->song_duration = 0;
    }

    switch (song)
    {
    case SNG_NOTHING:
    {
        if (song_disguise_active)
            song_disguise_on_stop();

        if ((song_to_change == 1) && (p_ptr->song1 != SNG_NOTHING))
        {
            msg_print("You end your song.");
        }
        else if ((song_to_change == 2) && (p_ptr->song2 != SNG_NOTHING))
        {
            msg_print("You end your minor theme.");
        }
        break;
    }
    case SNG_ELBERETH:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song to the Queen of the Stars.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about the Queen of the Stars.");
        }
        else
        {
            msg_print("You change your minor theme to one about the Queen of "
                      "the Stars.");
        }
        break;
    }
    case SNG_CHALLENGE:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a strident song of mockery and scorn.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of mockery and scorn.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one of mockery and scorn.");
        }
        break;
    }
    case SNG_FREEDOM:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of freedom and safe passage.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of freedom and safe passage.");
        }
        else
        {
            msg_print("You change your minor theme to one of freedom and safe "
                      "passage.");
        }
        break;
    }
    case SNG_STAUNCHING:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a murmuring song of soft and soothing words.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of soft and soothing words.");
        }
        else
        {
            msg_print("You change your minor theme to one of soft and soothing "
                      "words.");
        }
        msg_print("You feel your wounds close and your body heal.");
        break;
    }
    case SNG_SILENCE:
    {
        if (song_to_change == 1)
        {
            msg_print("You whisper a song of silence.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of silence.");
        }
        else
        {
            msg_print("You change your minor theme to one of silence.");
        }
        break;
    }
    case SNG_DELVINGS:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song about the rocky bones of the earth.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print(
                "You add a minor theme about the rocky bones of the earth.");
        }
        else
        {
            msg_print("You change your minor theme to one about the rocky "
                      "bones of the "
                      "earth.");
        }
        break;
    }
    case SNG_REVEALING:
    {
        if (song_to_change == 1)
        {
            msg_print("You weave a song to unveil hidden life and treasure.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme that seeks what lies concealed.");
        }
        else
        {
            msg_print("You shift your minor theme toward revealing secrets.");
        }
        break;
    }
    case SNG_TREES:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song about the Two Trees of Valinor.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about the Two Trees of Valinor.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one about the Two Trees of "
                "Valinor.");
        }
        msg_print("A memory of their light wells up around you.");
        break;
    }
    case SNG_ELVENESS:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a lilting song celebrating the grace of the Eldar.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme celebrating the grace of the Eldar.");
        }
        else
        {
            msg_print("You change your minor theme to honor the grace of the Eldar.");
        }
        break;
    }
    case SNG_DISGUISE:
    {
        if (old_song != SNG_DISGUISE)
        {
            if (any_monster_observes_player())
            {
                msg_print("You cannot begin the Song of Disguise while observed.");
                return;
            }
        }

        if (song_to_change == 1)
        {
            msg_print("You begin a soft song of misdirection and guile.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme weaving subtle disguises.");
        }
        else
        {
            msg_print("You change your minor theme to one of misdirection and guile.");
        }
        break;
    }
    case SNG_STAYING:
    {
        if (song_to_change == 1)
        {
            msg_print(
                "You begin a song about the courage of great heroes past.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about the courage of great heroes "
                      "past.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one about the courage of great "
                "heroes past.");
        }
        break;
    }
    case SNG_SLAYING:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of fury and dread.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of fury and dread.");
        }
        else
        {
            msg_print("You change your minor theme to one of fury and dread.");
        }
        break;
    }
    case SNG_LORIEN:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a soothing song about weariness and rest.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about weariness and rest.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one about weariness and rest.");
        }
        break;
    }
    case SNG_THRESHOLDS:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of ways guarded and impassable.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of ways guarded and impassable.");
        }
        else
        {
            msg_print("You change your minor theme to one of ways guarded and "
                      "impassable.");
        }
        break;
    }
    case SNG_MASTERY:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of mastery and command.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of mastery and command.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one of mastery and command.");
        }
        break;
    }
    case SNG_SHATTERING:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a fell song of breaking and sundering.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of breaking and sundering.");
        }
        else
        {
            msg_print("You change your minor theme to one of breaking and sundering.");
        }
        break;
    }
    case SNG_CONTEST:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a piercing song of contest and rivalry.");
        }
        break;
    }
    case SNG_LAMENT:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a sorrowful song of loss and lament.");
        }
        break;
    }
    }

    // Actually set the song
    if (song_to_change == 1)
    {
        p_ptr->song1 = song;
    }
    if ((song_to_change == 2) || (song == SNG_NOTHING))
    {
        p_ptr->song2 = song;
    }

    // Display synergy message if a woven theme pair is detected
    if (song != SNG_NOTHING && song_to_change == 2)
    {
        display_synergy_message(p_ptr->song1, p_ptr->song2);
    }

    if (!singing(SNG_DISGUISE) && song_disguise_active)
        song_disguise_on_stop();

    // beginning/changing songs takes time
    if (song != SNG_NOTHING)
    {
        /* Take time */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;
    }
}

bool singing(int song)
{
    if (song == SNG_NOTHING)
    {
        if (p_ptr->song1 == song)
            return (true);
    }
    else
    {
        if (p_ptr->song1 == song)
            return (true);
        if (p_ptr->song2 == song)
            return (true);
    }

    return (false);
}

bool known_to_delvings(int y, int x)
{
    if (!in_bounds(y, x))
        return false;
    return ((cave_info[y][x] & CAVE_MARK) || (cave_info[y][x] & CAVE_SEEN));
}

void sing_song_of_challenge(int score)
{
    int i;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        int resistance;
        int result;

        /* Access the monster */
        monster_type* m_ptr = &mon_list[i];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        resistance = monster_skill(m_ptr, S_WIL);

        // Adjust to work best against lower-will monsters.
        resistance = (resistance * resistance) / 10;

        // adjust difficulty by the distance to the monster
        result = skill_check(PLAYER, score,
            resistance + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);

        /* If successful, alert the monster and make it more aggressive */
        if (result > 0)
        {
            set_alertness(m_ptr, m_ptr->alertness + result);
            // boost morale and check for the monster turning aggressive
            m_ptr->tmp_morale = MAX(m_ptr->tmp_morale, 30);
            calc_morale(m_ptr);
            calc_stance(m_ptr);
        }
    }
}

void sing_song_of_delvings(int score)
{
    int y, x, yy, xx;
    int min_x, max_x, min_y, max_y, y_range, x_range;

    int px = p_ptr->px;
    int py = p_ptr->py;

    int range = score + 8;

    min_y = MAX(1, py - range);
    max_y = MIN(MAX_DUNGEON_HGT, py + range + 1);
    min_x = MAX(1, px - range);
    max_x = MIN(MAX_DUNGEON_WID, px + range + 1);
    y_range = max_y - min_y;
    x_range = max_x - min_x;

    char* delvings;
    delvings = mem_alloc_array(y_range * x_range * 4, char);

    for (y = min_y; y < max_y; ++y)
    {
        for (x = min_x; x < max_x; ++x)
        {
            bool neighbour_known = false;
            int distance_from_player = (distance(py, px, y, x));
            int adjusted_score = score - distance_from_player;

            for (yy = y - 1; yy <= y + 1; ++yy)
            {
                for (xx = x - 1; xx <= x + 1; ++xx)
                {
                    int chance = damroll(1, 6);
                    if (known_to_delvings(yy, xx) && chance < adjusted_score)
                        neighbour_known = true;
                }
            }

            if (neighbour_known)
            {
                int dy = y - min_y;
                int dx = x - min_x;

                delvings[(dy * x_range) + dx] = true;
            }
        }
    }

    for (y = min_y; y < max_y; ++y)
    {
        for (x = min_x; x < max_x; ++x)
        {
            int dy = y - min_y;
            int dx = x - min_x;

            if (delvings[(dy * x_range) + dx] == true)
            {
                map_feature(y, x);
                if (cave_feat[y][x] == FEAT_SECRET && known_to_delvings(y, x))
                {
                    place_closed_door(y, x);
                }
            }
            if (cave_stair_bold(y, x) || cave_forge_bold(y, x)
                || cave_trap_bold(y, x))
            {
                // Special case for stairs and forges - if we know a square
                // within a distance of 5 along an axis, we spot them.
                int i, j;
                int start_y = MAX(min_y, y - 5);
                int end_y = MIN(max_y, y + 5);
                int start_x = MAX(min_x, x - 5);
                int end_x = MIN(max_x, x + 5);

                for (j = start_y; j < end_y; ++j)
                {
                    if (delvings[(j * x_range) + dx] == true)
                    {
                        if (cave_trap_bold(y, x))
                            reveal_trap(y, x);
                        else
                            map_feature(y, x);
                    }
                }

                for (i = start_x; i < end_x; ++i)
                {
                    if (delvings[(dy * x_range) + i] == true)
                    {
                        if (cave_trap_bold(y, x))
                            reveal_trap(y, x);
                        else
                            map_feature(y, x);
                    }
                }
            }
        }
    }

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);

    mem_free_null(delvings);
}

static bool object_is_monster_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
        return true;
    default:
        return false;
    }
}

static bool object_is_monster_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return true;
    default:
        return false;
    }
}

static void find_monster_equipment(
    monster_type* m_ptr, object_type** weapon, object_type** armour)
{
    s16b this_o_idx;

    *weapon = NULL;
    *armour = NULL;

    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
         this_o_idx = o_list[this_o_idx].next_o_idx)
    {
        object_type* o_ptr = &o_list[this_o_idx];

        if (!*weapon && object_is_monster_weapon(o_ptr))
            *weapon = o_ptr;

        if (!*armour && object_is_monster_armour(o_ptr))
            *armour = o_ptr;

        if (*weapon && *armour)
            break;
    }
}

static bool object_is_indestructible(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (artefact_p(o_ptr))
        return true;

    if (o_ptr->discount == INSCRIP_INDESTRUCTIBLE)
        return true;

    return false;
}

static bool object_is_weapon(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;
    return object_is_monster_weapon(o_ptr);
}

static bool object_is_armour(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;
    return object_is_monster_armour(o_ptr);
}

static bool object_can_be_shattered(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (object_is_indestructible(o_ptr))
        return false;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    if (k_ptr->flags3 & TR3_IGNORE_ALL)
        return false;

    return true;
}

static int base_weapon_ds(const object_type* o_ptr)
{
    int base = k_info[o_ptr->k_idx].ds;

    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];
        if (a_ptr->ds > 0)
            base = a_ptr->ds;
    }

    return base;
}

static int base_armour_ps(const object_type* o_ptr)
{
    int base = k_info[o_ptr->k_idx].ps;

    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];
        if (a_ptr->ps > 0)
            base = a_ptr->ps;
    }

    return base;
}

static bool shatter_weapon_object(object_type* o_ptr, int amount)
{
    if (!object_is_weapon(o_ptr))
        return false;

    if (!object_can_be_shattered(o_ptr))
        return false;

    int base = base_weapon_ds(o_ptr);

    if (o_ptr->ds <= base)
        return false;

    int new_ds = MAX(base, o_ptr->ds - amount);

    if (new_ds < o_ptr->ds)
    {
        o_ptr->ds = (byte)new_ds;
        return true;
    }

    return false;
}

static bool shatter_armour_object(object_type* o_ptr, int amount)
{
    if (!object_is_armour(o_ptr))
        return false;

    if (!object_can_be_shattered(o_ptr))
        return false;

    int base = base_armour_ps(o_ptr);

    if (o_ptr->ps <= base)
        return false;

    int new_ps = MAX(base, o_ptr->ps - amount);

    if (new_ps < o_ptr->ps)
    {
        o_ptr->ps = (byte)new_ps;
        return true;
    }

    return false;
}

static void shatter_floor_items(int score);

void sing_song_of_elbereth(int score)
{
    int i;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        int resistance;
        int result;

        /* Access the monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        resistance = monster_skill(m_ptr, S_WIL);

        // only intelligent monsters are affected
        if (!(r_ptr->flags2 & (RF2_SMART)))
            resistance += 100;

        // Morgoth is not affected
        if (m_ptr->r_idx == R_IDX_MORGOTH)
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(PLAYER, score,
            resistance + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);

        /* If successful, cause fear in the monster */
        if (result > 0)
        {
            /* Decrease temporary morale */
            m_ptr->tmp_morale -= result * 10;
        }
    }
}

void sing_song_of_trees(int score)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int rad = ability_bonus(S_SNG, SNG_TREES); // Stepped light radius bonus
    int dd = 1; // Always 1 die
    int ds = score;            // Not used for GF_LIGHT damage; kept for debugging
    int dif = score;           // Song score for GF_LIGHT resistance checks
    
    log_debug("sing_song_of_trees: score=%d rad=%d dd=%d ds=%d", score, rad, dd, ds);
    
    /* Song of Trees damages/stuns light-sensitive monsters without visual flash */
    /* Uses PROJECT_KILL to affect monsters, but NOT PROJECT_GRID (no visual light squares effect) */
    /* PROJECT_HIDE prevents the projectile animation */
    /* Light radius increase is handled separately in xtra1.c calc_light() */
    /* Shows damage messages to provide feedback when monsters are affected */
    /* IMPORTANT: Use uniform=true so dd doesn't decay with distance (damage is based on light_level at monster's position) */
    u32b flg = PROJECT_BOOM | PROJECT_KILL | PROJECT_PASS | PROJECT_HIDE;
    
    (void)project(-1, rad, py, px, py, px, dd, ds, dif, GF_LIGHT, flg, 0, true);
}

void sing_song_of_lorien(int score)
{
    int i;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        int resistance;
        int result;

        /* Access the monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        monster_lore* l_ptr = &l_list[m_ptr->r_idx];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        resistance = monster_skill(m_ptr, S_WIL);

        // Deal with sleep resistance
        if (r_ptr->flags3 & (RF3_NO_SLEEP))
        {
            resistance += 100;
            if (m_ptr->ml)
                l_ptr->flags3 |= (RF3_NO_SLEEP);
        }

        // adjust difficulty by the distance to the monster
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_LUT) result = skill_check(PLAYER, (3*score)/2,
            resistance + 5 + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);
        else result = skill_check(PLAYER, score,
            resistance + 5 + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);

        /* If successful, (partially) put the monster to sleep */
        if (result > 0)
        {
            set_alertness(m_ptr, m_ptr->alertness - result);
        }
    }
}

/*
 * Apply shattering effect to monsters in an arc (like Horn of Blasting)
 * This affects monsters within a 90-degree arc, radius 3
 */
void shatter_in_arc(int dir, int score)
{
    int i, j;
    int direction;
    extern const byte cycle[];
    extern const byte chome[];

    /* Handle special directions */
    if (dir == DIRECTION_UP || dir == DIRECTION_DOWN || dir == 5)
        return;

    direction = chome[dir];

    /* Scan arc: 3 forward, in 3 directions (left, center, right) */
    for (i = -1; i < 2; ++i)
    {
        for (j = 1; j <= 3; ++j)
        {
            int arc_dir = cycle[direction + i];
            int y = p_ptr->py + j * ddy[arc_dir];
            int x = p_ptr->px + j * ddx[arc_dir];

            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            /* Check for monster */
            if (cave_m_idx[y][x] <= 0)
                continue;

            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            object_type* weapon;
            object_type* armour;
            int resistance;
            int result;
            bool weapon_possible = false;
            bool armour_possible = false;
            int weapon_blow = -1;
            int best_ds = 0;

            /* Ignore dead monsters */
            if (!m_ptr->r_idx)
                continue;

            bool has_weapon_flag = (r_ptr->flags3 & RF3_HAS_WEAPON) != 0;
            bool has_armour_flag = (r_ptr->flags3 & RF3_HAS_ARMOUR) != 0;

            if (!has_weapon_flag && !has_armour_flag)
                continue;

            /* Identify items carried by the monster */
            find_monster_equipment(m_ptr, &weapon, &armour);

            /* Determine resistance (no distance scaling for arc effect) */
            resistance = monster_skill(m_ptr, S_WIL);

            result = skill_check(PLAYER, score, resistance, m_ptr);

            if (result <= 0)
                continue;

            /* Check for weapon possibility */
            if (has_weapon_flag)
            {
                for (int b = 0; b < MONSTER_BLOW_MAX; b++)
                {
                    if (!r_ptr->blow[b].method)
                        break;

                    int ds = r_ptr->blow[b].ds;
                    if (ds <= 1)
                        continue;

                    int max_reduction = ds - 1;
                    int current = m_ptr->blow_ds_reduction[b];

                    if (current >= max_reduction)
                        continue;

                    if (ds > best_ds)
                    {
                        best_ds = ds;
                        weapon_blow = b;
                    }
                }

                weapon_possible = (weapon_blow != -1);
            }

            /* Check for armour possibility */
            if (has_armour_flag && r_ptr->ps > 0)
            {
                if (m_ptr->armor_ps_reduction < r_ptr->ps)
                    armour_possible = true;
            }

            if (!weapon_possible && !armour_possible)
                continue;

            /* 50/50 chance to target weapon or armour */
            bool target_weapon = weapon_possible
                && (!armour_possible || one_in_(2));

            if (target_weapon && weapon_possible)
            {
                /* Probability to weaken: score/3 percent */
                int weaken_chance = score / 3;
                
                if (percent_chance(weaken_chance))
                {
                    /* Reduce by exactly 1 */
                    m_ptr->blow_ds_reduction[weapon_blow] += 1;

                    if (weapon)
                        shatter_weapon_object(weapon, 1);

                    if (m_ptr->ml)
                    {
                        char m_name[80];
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                        msg_format("The blast splinters %s's weapon.", m_name);
                    }
                }
            }
            else if (!target_weapon && armour_possible)
            {
                /* Probability to weaken: score/3 percent */
                int weaken_chance = score / 3;
                
                if (percent_chance(weaken_chance))
                {
                    /* Reduce by exactly 1 */
                    m_ptr->armor_ps_reduction += 1;

                    if (armour)
                        shatter_armour_object(armour, 1);

                    if (m_ptr->ml)
                    {
                        char m_name[80];
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                        msg_format("The blast warps %s's armour.", m_name);
                    }
                }
            }
        }
    }
}

void sing_song_of_shattering(int score)
{
    int i;
    int monsters_checked = 0;
    int monsters_with_flags = 0;
    int skill_check_passed = 0;

    log_debug("Song of Shattering: starting with score=%d", score);

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        object_type* weapon;
        object_type* armour;
        int resistance;
        int result;
        bool weapon_possible = false;
        bool armour_possible = false;
        int weapon_blow = -1;
        int best_ds = 0;

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        monsters_checked++;

        bool has_weapon_flag = (r_ptr->flags3 & RF3_HAS_WEAPON) != 0;
        bool has_armour_flag = (r_ptr->flags3 & RF3_HAS_ARMOUR) != 0;
        bool has_stone_body = (r_ptr->flags3 & RF3_STONE) != 0;

        if (!has_weapon_flag && !has_armour_flag && !has_stone_body)
            continue;

        monsters_with_flags++;
        log_debug("Song of Shattering: Monster %s has flags (weapon=%d, armour=%d, stone=%d)", 
                  r_name + r_ptr->name, has_weapon_flag, has_armour_flag,
                  has_stone_body);

        /* Identify items carried by the monster (for secondary effects) */
        find_monster_equipment(m_ptr, &weapon, &armour);

        /* Determine resistance, scaling with distance */
        resistance = monster_skill(m_ptr, S_WIL);
        resistance += flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);

        result = skill_check(PLAYER, score, resistance, m_ptr);

        log_debug("Song of Shattering: skill_check result=%d (score=%d, resistance=%d)", 
                  result, score, resistance);

        if (result <= 0)
            continue;

        skill_check_passed++;

        /* Check for weapon possibility */
        if (has_weapon_flag)
        {
            for (int b = 0; b < MONSTER_BLOW_MAX; b++)
            {
                if (!r_ptr->blow[b].method)
                    break;

                int ds = r_ptr->blow[b].ds;
                if (ds <= 1)
                    continue;

                int max_reduction = ds - 1;
                int current = m_ptr->blow_ds_reduction[b];

                if (current >= max_reduction)
                    continue;

                if (ds > best_ds)
                {
                    best_ds = ds;
                    weapon_blow = b;
                }
            }

            weapon_possible = (weapon_blow != -1);
        }

        /* Check for armour possibility */
        if ((has_armour_flag || has_stone_body) && r_ptr->ps > 0)
        {
            if (m_ptr->armor_ps_reduction < r_ptr->ps)
                armour_possible = true;
        }

        if (!weapon_possible && !armour_possible)
        {
            log_debug("Song of Shattering: No valid targets for this monster");
            continue;
        }

        /* 50/50 chance to target weapon or armour (no fallthrough) */
        bool target_weapon = weapon_possible
            && (!armour_possible || one_in_(2));

        log_debug("Song of Shattering: target_weapon=%d, weapon_possible=%d, armour_possible=%d", 
                  target_weapon, weapon_possible, armour_possible);

        if (target_weapon && weapon_possible)
        {
            /* Probability to weaken: score/3 percent (6.7% at score 20) */
            int weaken_chance = score / 3;
            
            log_debug("Song of Shattering: Attempting weapon damage, weaken_chance=%d%%", weaken_chance);
            
            if (percent_chance(weaken_chance))
            {
                /* Reduce by exactly 1 */
                m_ptr->blow_ds_reduction[weapon_blow] += 1;

                if (weapon)
                    shatter_weapon_object(weapon, 1);

                if (m_ptr->ml)
                {
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                    msg_format("Your song of sundering rends %s's weapon.", m_name);
                }
                
                log_debug("Song of Shattering: Weapon damage SUCCESS");
            }
            else
            {
                log_debug("Song of Shattering: Weapon damage FAILED probability check");
            }
        }
        else if (!target_weapon && armour_possible)
        {
            /* Probability to weaken: score/3 percent (6.7% at score 20) */
            int weaken_chance = score / 3;
            
            log_debug("Song of Shattering: Attempting armour damage, weaken_chance=%d%%", weaken_chance);
            
            if (percent_chance(weaken_chance))
            {
                /* Reduce by exactly 1 */
                m_ptr->armor_ps_reduction += 1;

                if (armour)
                    shatter_armour_object(armour, 1);

                if (m_ptr->ml)
                {
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                    msg_format("Your song of sundering mars %s's armour.", m_name);
                }
                
                log_debug("Song of Shattering: Armour damage SUCCESS");
            }
            else
            {
                log_debug("Song of Shattering: Armour damage FAILED probability check");
            }
        }
    }

    log_debug("Song of Shattering: Summary - checked=%d, with_flags=%d, passed_skill_check=%d", 
              monsters_checked, monsters_with_flags, skill_check_passed);

    shatter_floor_items(score);
}

static void shatter_floor_items(int score)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!cave_o_idx[y][x])
                continue;

            int dist = flow_dist(FLOW_PLAYER_NOISE, y, x);
            if (dist >= FLOW_MAX_DIST)
                continue;

            int difficulty = 10 + dist;
            int result = skill_check(PLAYER, score, difficulty, NULL);
            if (result <= 0)
                continue;

            s16b this_o_idx = cave_o_idx[y][x];

            while (this_o_idx)
            {
                object_type* o_ptr = &o_list[this_o_idx];
                bool changed = false;

                /* Probability to weaken: score/3 percent (6.7% at score 20) */
                int weaken_chance = score / 3;

                if (percent_chance(weaken_chance))
                {
                    if (object_is_weapon(o_ptr))
                    {
                        changed = shatter_weapon_object(o_ptr, 1);
                    }
                    else if (object_is_armour(o_ptr))
                    {
                        changed = shatter_armour_object(o_ptr, 1);
                    }

                    if (changed)
                    {
                        if (panel_contains(y, x) && player_can_see_bold(y, x))
                        {
                            char o_name[80];
                            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
                            msg_format("%s answers your song with a bitter crack.", o_name);
                        }

                        lite_spot(y, x);
                    }
                }

                this_o_idx = o_ptr->next_o_idx;
            }
        }
    }
}

static void song_reveal_items(int range)
{
    bool marked_anything = false;

    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        if (!o_ptr->k_idx)
            continue;

        if (o_ptr->held_m_idx)
            continue;

        int y = o_ptr->iy;
        int x = o_ptr->ix;

        if (distance(p_ptr->py, p_ptr->px, y, x) > range)
            continue;

        if (!o_ptr->marked)
        {
            o_ptr->marked = true;
            marked_anything = true;
        }

        if (o_ptr->name1)
        {
            a_info[o_ptr->name1].seen |= ART_SEEN_PHYSICAL;
            o_ptr->ident |= IDENT_ARTIFACT_SEEN;
        }

        /* Revelation reveals easy smithing items (no distance penalty). */
        (void)player_auto_identify_smithing_object(o_ptr, true);

        lite_spot(y, x);
    }

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        if (!m_ptr->r_idx)
            continue;

        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if (!(strchr("|!?-_=~", r_ptr->d_char)))
            continue;

        if (distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) > range)
            continue;

        m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);
        marked_anything = true;

        repair_mflag_mark = true;
        repair_mflag_show = true;

        update_mon(i, false);
    }

    if (marked_anything)
        p_ptr->redraw |= (PR_MAP);
}

void sing_song_of_revealing(int score, bool primary_song)
{
    int effective_skill = p_ptr->skill_use[S_SNG];
    if (!primary_song)
        effective_skill /= 2;

    if (effective_skill <= 0)
        return;

    int range = (score / 2) + 8;
    if (range < 0)
        range = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        if (!m_ptr->r_idx)
            continue;

        // Skip already visible monsters
        if (m_ptr->ml)
            continue;

        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        // Monster must be able to move
        if (r_ptr->flags1 & (RF1_NEVER_MOVE))
            continue;

        int dist = flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);
        if (dist >= FLOW_MAX_DIST)
            continue;

        if (dist > range)
            continue;

        // Calculate detection difficulty (similar to detect_monster_noise)
        int difficulty = dist - m_ptr->noise;
        
        // Use monster stealth
        difficulty += monster_skill(m_ptr, S_STL);

        // Bonus for awake but unwary monsters
        if ((m_ptr->alertness >= ALERTNESS_UNWARY)
            && (m_ptr->alertness < ALERTNESS_ALERT))
            difficulty -= 3;

        // Penalty for song of silence
        if (singing(SNG_SILENCE))
            difficulty += ability_bonus(S_SNG, SNG_SILENCE);

        // Make the skill check
        int result = skill_check(PLAYER, effective_skill, difficulty, m_ptr);

        // If detection succeeds, store the detection quality
        // This will determine visualization (full visibility vs hint marker)
        if (result > 0)
        {
            // Store the detection result (capped at reasonable maximum)
            song_revealing_hint[i] = (byte)MIN(result, 30);
            song_revealing_has_data = true;
            
            // If detection is strong enough, make fully visible immediately
            if (result > SONG_REVEALING_FULL_VISIBILITY)
            {
                m_ptr->ml = true;
            }
            
            lite_spot(m_ptr->fy, m_ptr->fx);
        }
    }

    song_reveal_items(range);
}

void sing(void)
{
    int type;
    int song = p_ptr->song1; // a default to soothe compilation warnings
    int score = 0;
    int cost = 0;
    bool abort_song = false;

    song_revealing_decay();

    if (p_ptr->song1 == SNG_NOTHING)
    {
        if (song_revealing_has_data)
        {
            memset(song_revealing_hint, 0, MAX_MONSTERS * sizeof(byte));
            song_revealing_has_data = false;
        }

        if (song_disguise_active)
            song_disguise_on_stop();
        return;
    }

    // abort song if out of voice, lost the ability to weave themes, or lost
    // either song ability
    if ((p_ptr->csp < 1)
        || ((p_ptr->song2 != SNG_NOTHING)
            && !p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES])
        || (!p_ptr->active_ability[S_SNG][p_ptr->song1])
        || ((p_ptr->song2 != SNG_NOTHING)
            && !p_ptr->active_ability[S_SNG][p_ptr->song2]))
    {
        /* Stop singing */
        if (song_disguise_active)
            song_disguise_on_stop();
        change_song(SNG_NOTHING);

        /* Disturb */
        disturb(0, 0);
        return;
    }
    else
    {
        p_ptr->song_duration++;
    }

    if (singing(SNG_DISGUISE))
    {
        if (!song_disguise_active)
            song_disguise_on_start();
    }
    else if (song_disguise_active)
    {
        song_disguise_on_stop();
    }

    for (type = 1; type <= 2; type++)
    {
        if (type == 1)
            song = p_ptr->song1;
        if (type == 2)
            song = p_ptr->song2;

        score = ability_bonus(S_SNG, song);

        switch (song)
        {
        case SNG_ELBERETH:
        {
            cost += 1;

            sing_song_of_elbereth(score);

            // Maintain the lingering effect counter while singing
            // Duration scales with song skill: 15 turns at skill 20
            // Formula: (skill * 3) / 4
            int duration = (score * 3) / 4;
            if (duration < 3) duration = 3; // Minimum 3 turns
            p_ptr->song_elbereth_effect = duration;

            break;
        }
        case SNG_CHALLENGE:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;

            sing_song_of_challenge(score);

            // Maintain the lingering effect counter while singing
            // Duration scales with song skill: 15 turns at skill 20
            // Formula: (skill * 3) / 4
            int duration = (score * 3) / 4;
            if (duration < 3) duration = 3; // Minimum 3 turns
            p_ptr->song_challenge_effect = duration;

            break;
        }
        case SNG_FREEDOM:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;
            sing_song_of_freedom(score);
            break;
        }
        case SNG_STAUNCHING:
        {
            int cycle = p_ptr->song_duration % 12;
            int song_frac = score % 12;
            int bonus_hp = 0;

            cost += 1;
            set_cut(0);

            if ((cycle * song_frac) % 12 < song_frac)
                bonus_hp = 1;

            bonus_hp += (score / 12);

            p_ptr->chp += bonus_hp;

            if (p_ptr->chp > p_ptr->mhp)
                p_ptr->chp = p_ptr->mhp;

            break;
        }
        case SNG_SILENCE:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;
            break;
        }
        case SNG_THRESHOLDS:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;

            break;
        }
        case SNG_DELVINGS:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;

            sing_song_of_delvings(score);

            break;
        }
        case SNG_REVEALING:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;

            sing_song_of_revealing(score, song == p_ptr->song1);

            break;
        }
        case SNG_TREES:
        {
            if ((p_ptr->song_duration % 3) == type - 1)
                cost += 1;
            
            sing_song_of_trees(song_effective_skill(song));
            
            break;
        }
        case SNG_ELVENESS:
        {
            cost += 1;
            break;
        }
        case SNG_STAYING:
        {
            cost += 1;
            break;
        }
        case SNG_DISGUISE:
        {
            cost += 3;
            sing_song_of_disguise(score);
            break;
        }
        case SNG_SLAYING:
        {
            cost += 1;
            break;
        }
        case SNG_LORIEN:
        {
            cost += 1;

            sing_song_of_lorien(score);

            break;
        }
        case SNG_CONTEST:
        {
            if (type == 1)
            {
                cost += 7;
                if (!song_duel_process_contest(score))
                    abort_song = true;
            }
            break;
        }
        case SNG_LAMENT:
        {
            if (type == 1)
            {
                cost += 7;
                if (!song_duel_process_lament(score))
                    abort_song = true;
            }
            break;
        }
        case SNG_MASTERY:
        {
            cost += 2;
            break;
        }
        case SNG_SHATTERING:
        {
            cost += 2;

            sing_song_of_shattering(score);
            break;
        }
        }

        if (abort_song)
            break;
    }

    // pay the price of the singing
    if (p_ptr->csp >= cost)
        p_ptr->csp -= cost;
    else
        p_ptr->csp = 0;

    p_ptr->redraw |= (PR_VOICE);
    p_ptr->redraw |= (PR_HP);
}




