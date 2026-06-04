/* File: spell/spell-detection.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

void detect_all_doors_traps()
{
    int y, x;

    /* Scan the visible area */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            /* Detect invisible traps */
            if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                /* Reveal the trap */
                reveal_trap(y, x);
            }

            /* Detect secret doors */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Pick a door */
                place_closed_door(y, x);

                /* Hack -- Memorize */
                cave_info[y][x] |= (CAVE_MARK);

                /* Redraw */
                lite_spot(y, x);
            }
        }
    }
}

/*
 * Detect all traps in sight
 */
bool detect_traps(void)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_can_see_bold(y, x))
                continue;

            /* Detect invisible traps */
            if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                /* Reveal the trap */
                reveal_trap(y, x);

                /* Obvious */
                detect = true;
            }
        }
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of traps!");
    }

    /* Result */
    return (detect);
}

/*
 * Detect all doors in sight
 */
bool detect_doors(void)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = p_ptr->py - MAX_SIGHT; y < p_ptr->py + MAX_SIGHT; y++)
    {
        for (x = p_ptr->px - MAX_SIGHT; x < p_ptr->px + MAX_SIGHT; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!player_can_see_bold(y, x))
                continue;

            /* Detect secret doors */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Pick a door */
                place_closed_door(y, x);

                /* Hack -- Memorize */
                cave_info[y][x] |= (CAVE_MARK);

                /* Redraw */
                lite_spot(y, x);

                detect = true;
            }
        }
    }

    /* Result */
    return (detect);
}

/*
 * Detect all stairs within 20 squares
 */
bool detect_stairs(void)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = p_ptr->py - 20; y < p_ptr->py + 20; y++)
    {
        for (x = p_ptr->px - 20; x < p_ptr->px + 20; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            /* Detect stairs */
            if (cave_stair_bold(y, x))
            {
                /* Hack -- Memorize */
                cave_info[y][x] |= (CAVE_MARK);

                /* Redraw */
                lite_spot(y, x);

                /* Obvious */
                detect = true;
            }
        }
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of stairs!");
    }

    /* Result */
    return (detect);
}

/*
 * Detect all "normal" objects on the current panel
 */
bool detect_objects_normal(int radius)
{
    int i, y, x;

    bool detect = false;

    /* Scan objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        // Skip staffs of treasures (cute, and helps prevent run-away detection
        // spiral)
        if ((o_ptr->tval == TV_STAFF) && (o_ptr->sval == SV_STAFF_TREASURES))
            continue;

        /* Location */
        y = o_ptr->iy;
        x = o_ptr->ix;

        /* Only detect nearby objects (within radius if specified) */
        if (radius > 0)
        {
            int dist = distance(p_ptr->py, p_ptr->px, y, x);
            if (dist > radius)
                continue;
        }

        /* Hack -- memorize it */
        o_ptr->marked = true;

        if (o_ptr->name1)
        {
            a_info[o_ptr->name1].seen |= ART_SEEN_PHYSICAL;
            o_ptr->ident |= IDENT_ARTIFACT_SEEN;
        }

        /* Detection reveals easy smithing items (no distance penalty). */
        (void)player_auto_identify_smithing_object(o_ptr, true);

        /* Redraw */
        lite_spot(y, x);

        /* Detect */
        detect = true;
    }

    /* Scan monsters, looking for object-like ones */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* we want to detect object-like monsters */
        if (!(strchr("|!?-_=~", r_ptr->d_char)))
            continue;

        /* Location */
        y = m_ptr->fy;
        x = m_ptr->fx;

        /*mark them*/
        m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

        /* Optimize -- Repair flags */
        repair_mflag_mark = true;
        repair_mflag_show = true;

        /* Update the monster */
        update_mon(i, false);

        /* Detect */
        detect = true;
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of objects!");
    }

    /* Result */
    return (detect);
}

/*
 * Detect all "magic" objects on the current panel.
 *
 * This will light up all spaces with "magic" items, including artefacts,
 * special items, potions, staves, amulets, rings.
 */
bool detect_objects_magic(void)
{
    int i, y, x, tv;

    bool detect = false;

    /* Scan all objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Location */
        y = o_ptr->iy;
        x = o_ptr->ix;

        /* Only detect nearby objects */
        if (!panel_contains(y, x))
            continue;

        /* Examine the tval */
        tv = o_ptr->tval;

        /* Artefacts, misc magic items, or ego items */
        if (artefact_p(o_ptr) || ego_item_p(o_ptr) || (tv == TV_AMULET)
            || (tv == TV_RING) || (tv == TV_STAFF) || (tv == TV_HORN)
            || (tv == TV_POTION))
        {
            /* Memorize the item */
            o_ptr->marked = true;

            /* Redraw */
            lite_spot(y, x);

            /* Detect */
            detect = true;
        }
    }

    /* Describe */
    if (detect)
    {
        msg_print("You sense the presence of enchantments!");
    }

    /* Return result */
    return (detect);
}

/*
 * Detect all "normal" monsters on the current panel
 */
bool detect_monsters(int radius)
{
    int i;

    bool flag = false;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only detect monsters within radius if specified */
        if (radius > 0)
        {
            int dist = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
            if (dist > radius)
                continue;
        }

        /* Optimize -- Repair flags */
        repair_mflag_mark = true;
        repair_mflag_show = true;

        /* Hack -- Detect the monster */
        m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

        /* Update the monster */
        update_mon(i, false);

        /* Detect */
        flag = true;
    }

    /* Describe */
    if (flag)
    {
        /* Describe result */
        msg_print("You sense the presence of your enemies!");
    }

    /* Result */
    return (flag);
}

/*
 * Detect all "invisible" monsters in sight
 */
bool detect_monsters_invis(void)
{
    int i;

    bool flag = false;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        monster_lore* l_ptr = &l_list[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Detect invisible monsters */
        if (r_ptr->flags2 & (RF2_INVISIBLE))
        {
            /* Take note that they are invisible */
            l_ptr->flags2 |= (RF2_INVISIBLE);

            /* Update monster recall window */
            if (p_ptr->monster_race_idx == m_ptr->r_idx)
            {
                /* Window stuff */
                p_ptr->window |= (PW_MONSTER);
            }

            /* Optimize -- Repair flags */
            repair_mflag_mark = true;
            repair_mflag_show = true;

            /* Hack -- Detect the monster */
            m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

            /* Update the monster */
            update_mon(i, false);

            /* Detect */
            flag = true;
        }
    }

    /* Describe */
    if (flag)
    {
        /* Describe result */
        msg_print("You sense the presence of invisible creatures!");
    }

    /* Result */
    return (flag);
}

/*
 * Detect everything
 */
bool detect_all(void)
{
    bool detect = false;

    /* Detect everything */
    if (detect_traps())
        detect = true;
    if (detect_doors())
        detect = true;
    if (detect_stairs())
        detect = true;
    if (detect_objects_normal(0))
        detect = true;
    if (detect_monsters_invis())
        detect = true;
    if (detect_monsters(0))
        detect = true;

    /* Result */
    return (detect);
}
