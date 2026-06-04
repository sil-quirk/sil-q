/* File: monster-update.c */

#include "monster-internal.h"

/*
 * Shared sound-based detection logic. Returns true when the check succeeds.
 */
static bool listen_visual_effects_suppressed(void)
{
    return !character_generated || character_icky || character_xtra
        || screen_startup_supporting_panes_hidden_active();
}

bool detect_monster_noise(monster_type* m_ptr, int skill)
{
    byte a;
    char c;
    byte k;
    int base;
    bool suppress_visuals;

    int result;

    int y = m_ptr->fy;
    int x = m_ptr->fx;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    int difficulty = flow_dist(FLOW_PLAYER_NOISE, y, x) - m_ptr->noise;

    // reset the monster noise
    m_ptr->noise = 0;

    // must not be visible
    if (m_ptr->ml)
        return false;

    // monster must be able to move
    if (r_ptr->flags1 & (RF1_NEVER_MOVE))
        return false;

    // use monster stealth
    difficulty += monster_skill(m_ptr, S_STL);

    // bonus for awake but unwary monsters (to simulate their lack of care)
    if ((m_ptr->alertness >= ALERTNESS_UNWARY)
        && (m_ptr->alertness < ALERTNESS_ALERT))
        difficulty -= 3;

    // penalty for song of silence
    if (singing(SNG_SILENCE))
        difficulty += ability_bonus(S_SNG, SNG_SILENCE);

    // make the check
    result = skill_check(PLAYER, skill, difficulty, m_ptr);
    suppress_visuals = listen_visual_effects_suppressed();

    // give up if it is a failure
    if (result <= 0)
    {
        if (!suppress_visuals)
            lite_spot(y, x);
        return false;
    }

    // make the monster completely visible if a dramatic success
    if (result > 10)
    {
        m_ptr->ml = true;
        if (!suppress_visuals)
            lite_spot(y, x);
        return true;
    }

    if (suppress_visuals)
        return true;

    if (graphics_are_ascii())
    {
        /* Base graphic '*' */
        base = 0x30;

        /* Basic listen color */
        k = TERM_SLATE;

        /* Obtain attr/char */
        a = misc_to_attr[base + k];
        c = misc_to_char[base + k];
    }
    else
    {
        a = misc_to_attr[ICON_UNKNOWN_ENEMY];
        c = misc_to_char[ICON_UNKNOWN_ENEMY];
    }

    /* Display the visual effects */
    print_rel(c, a, y, x);
    move_cursor_relative(y, x);
    Term_fresh();

    return true;
}

void listen(monster_type* m_ptr)
{
    // must have the listen skill
    if (!p_ptr->active_ability[S_PER][PER_LISTEN])
        return;

    detect_monster_noise(m_ptr, p_ptr->skill_use[S_PER]);
}

/*
 * This function updates the monster record of the given monster
 *
 * This involves extracting the distance to the player (if requested),
 * and then checking for visibility (natural, see-invis,
 * telepathy), updating the monster visibility flag, redrawing (or
 * erasing) the monster when its visibility changes, and taking note
 * of any interesting monster flags (cold-blooded, invisible, etc).
 *
 * Note the new "mflag" field which encodes several monster state flags,
 * including "view" for when the monster is currently in line of sight,
 * and "mark" for when the monster is currently visible via detection.
 *
 * The only monster fields that are changed here are "cdis" (the
 * distance from the player), "ml" (visible to the player), and
 * "mflag" (to maintain the "MFLAG_VIEW" flag).
 *
 * Note the special "update_monsters()" function which can be used to
 * call this function once for every monster.
 *
 * Note the "full" flag which requests that the "cdis" field be updated,
 * this is only needed when the monster (or the player) has moved.
 *
 * Every time a monster moves, we must call this function for that
 * monster, and update the distance, and the visibility.  Every time
 * the player moves, we must call this function for every monster, and
 * update the distance, and the visibility.  Whenever the player "state"
 * changes in certain ways ("blindness", "telepathy",
 * and "see invisible"), we must call this function for every monster,
 * and update the visibility.
 *
 * Routines that change the "illumination" of a grid must also call this
 * function for any monster in that grid, since the "visibility" of some
 * monsters may be based on the illumination of their grid.
 *
 * Note that this function is called once per monster every time the
 * player moves.  When the player is running, this function is one
 * of the primary bottlenecks, along with "update_view()" and the
 * "process_monsters()" code, so efficiency is important.
 *
 * Note the optimized "inline" version of the "distance()" function.
 *
 * A monster is "visible" to the player if (1) it has been detected
 * by the player, (2) it is close to the player and the player has
 * telepathy, or (3) it is close to the player, and in line of sight
 * of the player, and it is "illuminated" by some combination of
 * torch light, or permanent light (invisible monsters
 * are only affected by "light" if the player can see invisible).
 *
 * Monsters which are not on the current panel may be "visible" to
 * the player, and their descriptions will include an "offscreen"
 * reference.  Currently, offscreen monsters cannot be targetted
 * or viewed directly, but old targets will remain set.  XXX XXX
 *
 */
void update_mon(int m_idx, bool full)
{
    monster_type* m_ptr = &mon_list[m_idx];

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    int d;

    /* Current location */
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    if (monster_clear_vala_state(m_ptr))
        calc_monster_speed(fy, fx);

    /* Seen at all */
    bool flag = false;

    /* Seen by vision */
    bool easy = false;

    /* Known because immobile */
    bool immobile_seen = false;

    // unmoving mindless monsters (i.e. molds) can be seen once encountered
    if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && (r_ptr->flags2 & (RF2_MINDLESS))
        && m_ptr->encountered)
    {
        immobile_seen = true;
    }

    /* Compute distance */
    if (full)
    {
        int py = p_ptr->py;
        int px = p_ptr->px;

        /* Distance components */
        int dy = (py > fy) ? (py - fy) : (fy - py);
        int dx = (px > fx) ? (px - fx) : (fx - px);

        /* Approximate distance */
        d = (dy > dx) ? (dy + (dx >> 1)) : (dx + (dy >> 1));

        /* Restrict distance */
        if (d > 255)
            d = 255;

        /* Save the distance */
        m_ptr->cdis = d;
    }

    /* Extract distance */
    else
    {
        /* Extract the distance */
        d = m_ptr->cdis;
    }

    /* Detected */
    if (m_ptr->mflag & (MFLAG_MARK))
        flag = true;

    // debugging option for seeing all monsters
    if (cheat_monsters)
        flag = true;

    /* Nearby */
    if (d <= MAX_SIGHT)
    {
        /* Basic telepathy */
        if (p_ptr->telepathy > 0)
        {
            /* Mindless, no telepathy */
            if (r_ptr->flags2 & (RF2_MINDLESS))
            {
                /* Memorize flags */
                l_ptr->flags2 |= (RF2_MINDLESS);
            }

            /* Normal mind, allow telepathy */
            else
            {
                /* Detectable */
                flag = true;

                /* Hack -- Memorize mental flags */
                if (r_ptr->flags2 & (RF2_SMART))
                    l_ptr->flags2 |= (RF2_SMART);
                if (r_ptr->flags2 & (RF2_MINDLESS))
                    l_ptr->flags2 |= (RF2_MINDLESS);
            }
        }

        /* Normal line of sight, and not blind */
        if (player_has_los_bold(fy, fx) && !p_ptr->blind)
        {
            bool do_invisible = false;
            int difficulty = monster_skill(m_ptr, S_WIL)
                + (2 * distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx));

            /* Use "illumination" */
            if (player_can_see_bold(fy, fx))
            {
                /* Handle "invisible" monsters */
                if (r_ptr->flags2 & (RF2_INVISIBLE))
                {
                    /* Take note */
                    do_invisible = true;

                    /* See invisible makes things much easier */
                    difficulty -= 10 * p_ptr->see_inv;

                    /* Keen senses */
                    if (p_ptr->active_ability[S_PER][PER_KEEN_SENSES])
                    {
                        // makes things a bit easier
                        difficulty -= 5;
                    }

                    // Sil-x: calling this here seems to cause randseed issues
                    // on reloading games
                    //        i.e. saving then loading will 'see' different
                    //        monsters
                    /* See invisible through perception skill */
                    if (skill_check(
                            PLAYER, p_ptr->skill_use[S_PER], difficulty, m_ptr)
                        > 0)
                    {
                        /* Easy to see */
                        easy = flag = true;
                    }
                }

                /* Handle "normal" monsters */
                else
                {
                    /* Easy to see */
                    easy = flag = true;
                }
            }

            // handle keen senses ability
            else if (seen_by_keen_senses(fy, fx))
            {
                /* Easy to see */
                easy = flag = true;
            }

            /* Visible */
            if (flag)
            {
                /* Memorize flags */
                if (do_invisible)
                    l_ptr->flags2 |= (RF2_INVISIBLE);
            }
        }
    }

    /* The monster is now visible */
    if (flag || immobile_seen)
    {
        // Untarget if this is an out-of-LOS stationary monster
        if (immobile_seen && !flag)
        {
            if (p_ptr->target_who == m_idx)
                target_set_monster(0);
            if (p_ptr->health_who == m_idx)
                health_track(0);
        }

        /* It was previously unseen */
        if (!m_ptr->ml)
        {
            /* Mark as visible */
            m_ptr->ml = true;

            /* Track monster visibility for Nienna mercy quest */
            if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE && m_ptr->r_idx != R_IDX_NIENA) {
                p_ptr->niena_monsters_seen++;
                log_trace("Nienna quest: Monster seen (total seen=%d, killed=%d)",
                         p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
            }

            /* Draw the monster */
            lite_spot(fy, fx);

            /* Update health bar as needed */
            if (p_ptr->health_who == m_idx)
                p_ptr->redraw |= (PR_HEALTHBAR);

            /* Disturb on visibility change */
            disturb(0, 0);

            /* Window stuff */
            p_ptr->window |= PW_MONLIST;

            // identify see invisible items
            if ((r_ptr->flags2 & (RF2_INVISIBLE)) && (p_ptr->see_inv > 0))
                ident_see_invisible(m_ptr);
        }
    }

    /* The monster is not visible */
    else
    {
        /* It was previously seen */
        if (m_ptr->ml)
        {
            /* Mark as not visible */
            m_ptr->ml = false;

            /* Erase the monster */
            lite_spot(fy, fx);

            /* Update health bar as needed */
            if (p_ptr->health_who == m_idx)
                p_ptr->redraw |= (PR_HEALTHBAR);

            /* Disturb on visibility change */
            // disturb(0, 0);

            /* Window stuff */
            p_ptr->window |= PW_MONLIST;
        }
    }

    /* The monster is now easily visible */
    if (easy)
    {
        /* Change */
        if (!(m_ptr->mflag & (MFLAG_VIEW)))
        {
            /* Mark as easily visible */
            m_ptr->mflag |= (MFLAG_VIEW);

            /* Disturb on appearance */
            disturb(0, 0);
        }
    }

    /* The monster is not easily visible */
    else
    {
        /* Change */
        if (m_ptr->mflag & (MFLAG_VIEW))
        {
            /* Mark as not easily visible */
            m_ptr->mflag &= ~(MFLAG_VIEW);

            /* Disturb on disappearance */
            // disturb(1, 0);
        }
    }

    // Ensure repeated calls within the same turn remain deterministic by seeding
    // the RNG from the current turn, then restoring the saved state afterwards.
    {
        u64b saved_state = Rand_state_export();
        u64b temp_seed = ((u64b)playerturn + 1) * 15485863ULL;
        Rand_state_import(temp_seed);
        listen(m_ptr);
        Rand_state_import(saved_state);
    }

    // Check ecounters with monsters (must be visible and in line of sight)
    if (m_ptr->ml && !m_ptr->encountered
        && player_has_los_bold(m_ptr->fy, m_ptr->fx)
        && (l_ptr->psights < MAX_SHORT))
    {
        int new_exp = adjusted_mon_exp(r_ptr, false);

        // gain experience for encounter
        gain_exp(new_exp);
        p_ptr->encounter_exp += new_exp;

        // update stats
        m_ptr->encountered = true;
        l_ptr->psights++;
        if (l_ptr->tsights < MAX_SHORT)
            l_ptr->tsights++;

        // If the player encounters a Unique for the first time, write a note.
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            char note2[120];
            char real_name[120];

            /* Get the monster's real name for the notes file */
            monster_desc_race(real_name, sizeof(real_name), m_ptr->r_idx);

            /* Write note */
            SDL_strlcpy(
                note2, format("Encountered %s", real_name), sizeof(note2));

            do_cmd_note(note2, p_ptr->depth);
        }

        // if it was a wraith, possibly realise you are haunted
        if ((r_ptr->flags3 & (RF3_UNDEAD))
            && !(r_ptr->flags2 & (RF2_TERRITORIAL)))
        {
            ident_haunted();
        }
    }
}

/*
 * This function simply updates all the (non-dead) monsters (see above).
 */
void update_monsters(bool full)
{
    int i;

    /* Update each (live) monster */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Update the monster */
        update_mon(i, full);
    }
}

