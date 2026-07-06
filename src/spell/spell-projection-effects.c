/* File: spell/spell-projection-effects.c */

#include "angband.h"
#include "externs.h"
#include "spell/spell-projection-internal.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

/*
 * Mega-Hack -- track "affected" monsters (see "project()" comments)
 */
int project_m_n;
int project_m_x;
int project_m_y;
int death_count;

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
bool project_f(
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
bool project_o(int who, int y, int x, int dd, int ds, int dif, int typ)
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
            /* Ignore hidden objects */
            if (!o_ptr->marked)
                continue;

            /* Ignore known objects */
            if (object_known_p(o_ptr))
                continue;

            /* Identify object (note the first argument) */
            do_ident_item(-1, o_ptr);

            /* Redraw purple dots */
            lite_spot(y, x);

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
bool project_m(
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
        if (healed && m_ptr->ml
            && (styled_monster_health_bars || styled_monster_tile_health_bars))
        {
            if (styled_monster_health_bars)
            {
                p_ptr->window |= PW_MONLIST;
                if (p_ptr->health_who == cave_m_idx[y][x])
                    p_ptr->window |= PW_MONSTER;
            }
            if (styled_monster_tile_health_bars)
                lite_spot(y, x);
        }

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
        if (monster_race_is_vala(m_ptr->r_idx)
            || (r_ptr->flags3 & (RF3_NO_SLOW)))
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
        if (monster_race_is_vala(m_ptr->r_idx)
            || (r_ptr->flags3 & (RF3_NO_SLEEP)))
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
        if (monster_race_is_vala(m_ptr->r_idx)
            || (r_ptr->flags3 & (RF3_NO_CONF)))
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
        if (monster_race_is_vala(m_ptr->r_idx) || (r_ptr->flags3 & RF3_NO_STUN))
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

        if (monster_race_is_vala(m_ptr->r_idx))
        {
            monster_clear_vala_state(m_ptr);
            note = " is unaffected!";
        }
        else
        {
            /* Generate message */
            if (m_ptr->confused)
                note = " looks more confused.";
            else
                note = " looks confused.";

            tmp = m_ptr->confused + do_conf;

            /* Apply confusion */
            m_ptr->confused += (tmp < 200) ? tmp : 200;
        }

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

        if (m_ptr->ml
            && (styled_monster_health_bars || styled_monster_tile_health_bars))
        {
            if (styled_monster_health_bars)
            {
                p_ptr->window |= PW_MONLIST;
                if (p_ptr->health_who == cave_m_idx[y][x])
                    p_ptr->window |= PW_MONSTER;
            }
            if (styled_monster_tile_health_bars)
                lite_spot(y, x);
        }

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
    if (styled_monster_health_bars && m_ptr->ml)
        p_ptr->window |= PW_MONLIST;

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
bool project_p(int who, int y, int x, int dd, int ds, int dif, int typ)
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

            update_combat_rolls_no_damage();
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

