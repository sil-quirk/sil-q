/* File: player/player-song-revealing.c */

#include "angband.h"
#include "externs.h"
#include "player/player-song-internal.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

static byte song_revealing_hint[MAX_MONSTERS];  // Stores detection result quality
static bool song_revealing_has_data = false;

#define SONG_REVEALING_HINT_TTL 3
#define SONG_REVEALING_FULL_VISIBILITY 10  // Threshold for full visibility

void song_revealing_decay(void)
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

static void song_reveal_items(int score, int range)
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

        int dist = flow_dist(FLOW_PLAYER_NOISE, y, x);
        if (dist >= FLOW_MAX_DIST)
            continue;

        if (dist > range)
            continue;

        if (!o_ptr->marked)
        {
            if (skill_check(PLAYER, score, dist, NULL) <= 0)
                continue;

            o_ptr->marked = true;
            marked_anything = true;

            if (o_ptr->name1)
            {
                a_info[o_ptr->name1].seen |= ART_SEEN_PHYSICAL;
                o_ptr->ident |= IDENT_ARTIFACT_SEEN;
            }

            lite_spot(y, x);
        }

        /* Revelation helps identify revealed smithing items while in range. */
        (void)player_auto_identify_smithing_object(o_ptr, true);
    }

    if (marked_anything)
        p_ptr->redraw |= (PR_MAP);
}

static void song_reveal_carried_items(void)
{
    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        (void)player_try_identify_smithing_object(
            o_ptr, i >= INVEN_WIELD, 0);
    }
}

void sing_song_of_revealing(int score)
{
    if (score <= 0)
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
        int result = skill_check(PLAYER, score, difficulty, m_ptr);

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

    song_reveal_items(score, range);
    song_reveal_carried_items();
}

void song_revealing_clear(void)
{
    if (song_revealing_has_data)
    {
        memset(song_revealing_hint, 0, MAX_MONSTERS * sizeof(byte));
        song_revealing_has_data = false;
    }
}

void song_revealing_handle_monster_removed(int m_idx)
{
    if (m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return;

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
