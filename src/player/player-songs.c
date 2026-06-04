/* File: player/player-songs.c */

#include "angband.h"
#include "externs.h"
#include "player/player-song-internal.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

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
        if (song_disguise_is_active())
            song_disguise_stop();

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
            if (song_disguise_any_monster_observes_player())
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

    if ((song_to_change == 1) && new_song_is_duel && (song != SNG_NOTHING))
    {
        monster_type* m_ptr = song_duel_get_target(song);
        if (m_ptr)
        {
            song_duel_learn_target_stats(m_ptr, song);
            song_duel_reveal_target_stats(m_ptr, song);
        }
    }

    // Display synergy message if a woven theme pair is detected
    if (song != SNG_NOTHING && song_to_change == 2)
    {
        display_synergy_message(p_ptr->song1, p_ptr->song2);
    }

    if (!singing(SNG_DISGUISE) && song_disguise_is_active())
        song_disguise_stop();

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

cptr song_voice_cost_desc(int song)
{
    switch (song)
    {
    case SNG_CHALLENGE:
    case SNG_FREEDOM:
    case SNG_SILENCE:
    case SNG_THRESHOLDS:
    case SNG_DELVINGS:
    case SNG_REVEALING:
    case SNG_TREES:
        return "1 Voice per 3 turns";

    case SNG_ELBERETH:
    case SNG_STAUNCHING:
    case SNG_ELVENESS:
    case SNG_STAYING:
    case SNG_SLAYING:
    case SNG_LORIEN:
        return "1 Voice per turn";

    case SNG_MASTERY:
    case SNG_SHATTERING:
        return "2 Voice per turn";

    case SNG_DISGUISE:
        return "3 Voice per turn";

    case SNG_CONTEST:
    case SNG_LAMENT:
        return "7 Voice per turn";

    default:
        return NULL;
    }
}

static int song_voice_cost_for_turn(int song, int theme_slot, int song_duration)
{
    switch (song)
    {
    case SNG_CHALLENGE:
    case SNG_FREEDOM:
    case SNG_SILENCE:
    case SNG_THRESHOLDS:
    case SNG_DELVINGS:
    case SNG_REVEALING:
    case SNG_TREES:
        return ((song_duration % 3) == theme_slot - 1) ? 1 : 0;

    case SNG_ELBERETH:
    case SNG_STAUNCHING:
    case SNG_ELVENESS:
    case SNG_STAYING:
    case SNG_SLAYING:
    case SNG_LORIEN:
        return 1;

    case SNG_MASTERY:
    case SNG_SHATTERING:
        return 2;

    case SNG_DISGUISE:
        return 3;

    case SNG_CONTEST:
    case SNG_LAMENT:
        return (theme_slot == 1) ? 7 : 0;

    default:
        return 0;
    }
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
        song_revealing_clear();

        if (song_disguise_is_active())
            song_disguise_stop();
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
        if (song_disguise_is_active())
            song_disguise_stop();
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
        if (!song_disguise_is_active())
            song_disguise_start();
    }
    else if (song_disguise_is_active())
    {
        song_disguise_stop();
    }

    for (type = 1; type <= 2; type++)
    {
        if (type == 1)
            song = p_ptr->song1;
        if (type == 2)
            song = p_ptr->song2;

        score = ability_bonus(S_SNG, song);
        cost += song_voice_cost_for_turn(song, type, p_ptr->song_duration);

        switch (song)
        {
        case SNG_ELBERETH:
        {
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
            sing_song_of_freedom(score);
            break;
        }
        case SNG_STAUNCHING:
        {
            int cycle = p_ptr->song_duration % 12;
            int song_frac = score % 12;
            int bonus_hp = 0;

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
            break;
        }
        case SNG_THRESHOLDS:
        {
            break;
        }
        case SNG_DELVINGS:
        {
            sing_song_of_delvings(score);

            break;
        }
        case SNG_REVEALING:
        {
            sing_song_of_revealing(score);

            break;
        }
        case SNG_TREES:
        {
            sing_song_of_trees(song_effective_skill(song));
            
            break;
        }
        case SNG_ELVENESS:
        {
            break;
        }
        case SNG_STAYING:
        {
            break;
        }
        case SNG_DISGUISE:
        {
            sing_song_of_disguise(score);
            break;
        }
        case SNG_SLAYING:
        {
            break;
        }
        case SNG_LORIEN:
        {
            sing_song_of_lorien(score);

            break;
        }
        case SNG_CONTEST:
        {
            if (type == 1)
            {
                if (!song_duel_process_contest(score))
                    abort_song = true;
            }
            break;
        }
        case SNG_LAMENT:
        {
            if (type == 1)
            {
                if (!song_duel_process_lament(score))
                    abort_song = true;
            }
            break;
        }
        case SNG_MASTERY:
        {
            break;
        }
        case SNG_SHATTERING:
        {
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
