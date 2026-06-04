/* File: player/player-song-disguise.c */

#include "angband.h"
#include "externs.h"
#include "player/player-song-internal.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

static bool song_disguise_active = false;
static byte* song_disguise_seen = NULL;
static byte* song_disguise_pacified = NULL;
static byte* song_disguise_attacked = NULL;
static int song_disguise_seen_count = 0;
static int song_disguise_attackers_current_turn = 0;
static int song_disguise_attackers_last_turn = 0;

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

    song_revealing_handle_monster_removed(m_idx);
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

void sing_song_of_disguise(int score)
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

bool song_disguise_is_active(void)
{
    return song_disguise_active;
}

bool song_disguise_any_monster_observes_player(void)
{
    return any_monster_observes_player();
}

void song_disguise_start(void)
{
    song_disguise_on_start();
}

void song_disguise_stop(void)
{
    song_disguise_on_stop();
}

/*
 * Teleport a monster, normally up to "dis" grids away.
 *
 * Attempt to move the monster at least "dis/2" grids away.
 *
 * But allow variation to prevent infinite loops.
 */
