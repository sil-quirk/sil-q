#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "support/input.h"
#include "support/movement-input.h"
#include "ui/menu-click.h"
#include "ui/targeting/targeting-internal.h"

static void look_prt(bool use_story_font, cptr text, int row, int col)
{
    /* When story font is enabled, use story_print_text which handles proportional rendering */
    if (use_story_font) {
        log_debug("look_prt: Using story_print_text for: '%.50s'", text);
        story_print_text(row, col, 0, TERM_WHITE, text);
    } else {
        log_debug("look_prt: Using prt (mono) for: '%.50s'", text);
        prt(text, row, col);
    }
}

/*
 * Monster health description.
 */
static void look_mon_desc(char* buf, size_t max, int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];

    // start the string empty
    SDL_strlcpy(buf, "(", max);

    if (p_ptr->wizard)
    {
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            SDL_strlcat(buf, format("asleep (%d), ", m_ptr->alertness), max);
        else if (m_ptr->alertness < ALERTNESS_ALERT)
            SDL_strlcat(buf, format("unwary (%d), ", m_ptr->alertness), max);
        else
            SDL_strlcat(buf, format("alert (%d), ", m_ptr->alertness), max);
    }

    if (m_ptr->confused)
        SDL_strlcat(buf, "confused, ", max);
    if (m_ptr->stunned)
        SDL_strlcat(buf, "stunned, ", max);
    if ((m_ptr->slowed) && (!m_ptr->hasted))
        SDL_strlcat(buf, "slowed, ", max);
    if ((!m_ptr->slowed) && (m_ptr->hasted))
        SDL_strlcat(buf, "hasted, ", max);

    // If nothing is going to be written, wipe the string
    if (strlen(buf) == 1)
    {
        buf[0] = '\0';
    }
    // Otherwise finish it
    else
    {
        // trim the final ", " first
        buf[strlen(buf) - 2] = '\0';
        SDL_strlcat(buf, ") ", max);
    }
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort_aux(void* u, void* v, int p, int q)
{
    int z, a, b;

    /* Done sort */
    if (p >= q)
        return;

    /* Pivot */
    z = p;

    /* Begin */
    a = p;
    b = q;

    /* Partition */
    while (true)
    {
        /* Slide i2 */
        while (!(*ang_sort_comp)(u, v, b, z))
            b--;

        /* Slide i1 */
        while (!(*ang_sort_comp)(u, v, z, a))
            a++;

        /* Done partition */
        if (a >= b)
            break;

        /* Swap */
        (*ang_sort_swap)(u, v, a, b);

        /* Advance */
        a++, b--;
    }

    /* Recurse left side */
    ang_sort_aux(u, v, p, b);

    /* Recurse right side */
    ang_sort_aux(u, v, b + 1, q);
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort(void* u, void* v, int n)
{
    /* Sort the array */
    ang_sort_aux(u, v, 0, n - 1);
}

/*** Targetting Code ***/

/*
 * Given a "source" and "target" location, extract a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 */
int motion_dir(int y1, int x1, int y2, int x2)
{
    /* No movement required */
    if ((y1 == y2) && (x1 == x2))
        return (5);

    /* South or North */
    if (x1 == x2)
        return ((y1 < y2) ? 2 : 8);

    /* East or West */
    if (y1 == y2)
        return ((x1 < x2) ? 6 : 4);

    /* South-east or South-west */
    if (y1 < y2)
        return ((x1 < x2) ? 3 : 1);

    /* North-east or North-west */
    if (y1 > y2)
        return ((x1 < x2) ? 9 : 7);

    /* Paranoia */
    return (5);
}

/*
 * Extract a direction (or zero) from a character
 */
int target_dir(char ch)
{
    int d = 0;

    int mode;

    cptr act;

    cptr s;

    if (ch == UI_MENU_CLICK_WAKE_KEY
        && movement_input_take_legacy_direction(
            MOVEMENT_INPUT_CONTEXT_ANY, &d))
    {
        return d;
    }

    /* Already a direction? */
    if (isdigit((unsigned char)ch))
    {
        d = D2I(ch);
    }
    else
    {
        // allow 'z' to indicate center direction
        if (ch == 'z')
            d = 5;

        else
        {
            // Determine the keyset
            if (!hjkl_movement && !angband_keyset)
                mode = KEYMAP_MODE_SIL;
            else if (hjkl_movement && !angband_keyset)
                mode = KEYMAP_MODE_SIL_HJKL;
            else if (!hjkl_movement && angband_keyset)
                mode = KEYMAP_MODE_ANGBAND;
            else
                mode = KEYMAP_MODE_ANGBAND_HJKL;

            /* Extract the action (if any) */
            act = keymap_act[mode][(byte)(ch)];

            /* Analyze */
            if (act)
            {
                /* Convert to a direction */
                for (s = act; *s; ++s)
                {
                    /* Use any digits in keymap */
                    if (isdigit((unsigned char)*s))
                        d = D2I(*s);
                }
            }
        }
    }

    /* Return direction */
    return (d);
}

/*
 * Determine is a monster makes a reasonable target
 *
 * The concept of "targetting" was stolen from "Morgul" (?)
 *
 * The player can target any location, or any "target-able" monster.
 *
 * Currently, a monster is "target_able" if it is visible, and if
 * the player can hit it with a projection, and the player is not
 * hallucinating.  This allows use of "use closest target" macros.
 */
bool target_able(int m_idx)
{
    monster_type* m_ptr;

    /* No monster */
    if (m_idx <= 0)
        return (false);

    /* Get monster */
    m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Monster must be alive */
    if (!m_ptr->r_idx)
        return (false);

    /* Monster must be visible */
    if (!m_ptr->ml)
        return (false);

    /* Monster must not be peaceful */
    if (r_ptr->flags1 & (RF1_PEACEFUL))
        return (false);

    /* Monster must be projectable */
    if (!player_can_fire_bold(m_ptr->fy, m_ptr->fx))
        return (false);

    /* Hack -- no targeting hallucinations */
    if (p_ptr->image)
        return (false);

    /* Rage and labyrinth partitions both suppress remembered-grid targeting. */
    if (!grid_info_is_available(m_ptr->fy, m_ptr->fx))
        return (false);

    /* Hack -- Never target trappers XXX XXX XXX */
    /* if (CLEAR_ATTR && (CLEAR_CHAR)) return (false); */

    /* Assume okay */
    return (true);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * We return true if the target is "okay" and false otherwise.
 */
bool target_okay(int range)
{
    /* No target */
    if (!p_ptr->target_set)
        return (false);

    /* Accept some "location" targets */
    if (p_ptr->target_who == 0)
    {
        /* Never "target" the player's own grid */
        if ((p_ptr->target_row == p_ptr->py) && (p_ptr->target_col == p_ptr->px))
            return (false);

        // reject things beyond range
        if ((range > 0)
            && (distance(
                    p_ptr->py, p_ptr->px, p_ptr->target_row, p_ptr->target_col)
                > range))
            return (false);

        // accept things in LOF
        if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_FIRE))
            return (true);

        // accept walls (for horn of blasting stuff)
        else if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_WALL))
            return (true);

        // reject others
        else
            return (false);
    }

    /* Check "monster" targets */
    if (p_ptr->target_who > 0)
    {
        int m_idx = p_ptr->target_who;

        /* Accept reasonable targets */
        if (target_able(m_idx))
        {
            monster_type* m_ptr = &mon_list[m_idx];

            /* Get the monster location */
            p_ptr->target_row = m_ptr->fy;
            p_ptr->target_col = m_ptr->fx;

            // reject things beyond range
            if ((range > 0)
                && (distance(p_ptr->py, p_ptr->px, p_ptr->target_row,
                        p_ptr->target_col)
                    > range))
                return (false);

            /* Good target */
            return (true);
        }
    }

    /* Assume no target */
    return (false);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * Very similar to target_okay, but does not require projectibility, just line
 * of sight
 *
 * We return true if the target is "okay" and false otherwise.
 */
bool target_sighted(void)
{
    /* No target */
    if (!p_ptr->target_set)
        return (false);

    /* Accept "location" targets */
    if (p_ptr->target_who == 0)
        return (true);

    /* Check "monster" targets */
    if (p_ptr->target_who > 0)
    {
        int m_idx = p_ptr->target_who;
        monster_type* m_ptr = &mon_list[m_idx];

        /* Accept reasonable targets */
        if (player_can_see_bold(m_ptr->fy, m_ptr->fx) && m_ptr->ml)
        {
            /* Get the monster location */
            p_ptr->target_row = m_ptr->fy;
            p_ptr->target_col = m_ptr->fx;

            /* Good target */
            return (true);
        }
    }

    /* Assume no target */
    return (false);
}

/*
 * Set the target to a monster (or nobody)
 */
void target_set_monster(int m_idx)
{
    /* Acceptable target */
    if ((m_idx > 0) && target_able(m_idx))
    {
        monster_type* m_ptr = &mon_list[m_idx];

        /* Save target info */
        p_ptr->target_set = true;
        p_ptr->target_who = m_idx;
        p_ptr->target_row = m_ptr->fy;
        p_ptr->target_col = m_ptr->fx;
    }

    /* Clear target */
    else
    {
        /* Reset target info */
        p_ptr->target_set = false;
        p_ptr->target_who = 0;
        p_ptr->target_row = 0;
        p_ptr->target_col = 0;
    }
}

/*
 * Set the target to a location
 */
void target_set_location(int y, int x)
{
    /* Legal target */
    if (in_bounds_fully(y, x))
    {
        /* Save target info */
        p_ptr->target_set = true;
        p_ptr->target_who = 0;
        p_ptr->target_row = y;
        p_ptr->target_col = x;
    }

    /* Clear target */
    else
    {
        /* Reset target info */
        p_ptr->target_set = false;
        p_ptr->target_who = 0;
        p_ptr->target_row = 0;
        p_ptr->target_col = 0;
    }
}

/*
 * Sorting hook -- comp function -- by "monster priority"
 *
 * Sorts monsters by: 1) Uniques first, 2) Then by depth (higher depth first), 3) Then by distance
 * We use "u" and "v" to point to arrays of "x" and "y" positions.
 */
static bool ang_sort_comp_monster_priority(const void* u, const void* v, int a, int b)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    int m_idx_a, m_idx_b;
    monster_type* m_ptr_a;
    monster_type* m_ptr_b;
    monster_race* r_ptr_a;
    monster_race* r_ptr_b;

    /* Get monster indices */
    m_idx_a = cave_m_idx[y[a]][x[a]];
    m_idx_b = cave_m_idx[y[b]][x[b]];

    /* Safety check */
    if (!m_idx_a && !m_idx_b) return false;
    if (!m_idx_a) return false; /* b comes first */
    if (!m_idx_b) return true;  /* a comes first */

    /* Get monster pointers */
    m_ptr_a = &mon_list[m_idx_a];
    m_ptr_b = &mon_list[m_idx_b];
    r_ptr_a = &r_info[m_ptr_a->r_idx];
    r_ptr_b = &r_info[m_ptr_b->r_idx];

    /* Check if either is unique */
    bool unique_a = (r_ptr_a->flags1 & RF1_UNIQUE) != 0;
    bool unique_b = (r_ptr_b->flags1 & RF1_UNIQUE) != 0;

    /* Uniques always come first */
    if (unique_a && !unique_b) return true;  /* a comes first */
    if (!unique_a && unique_b) return false; /* b comes first */

    /* Both unique or both non-unique, sort by depth (higher depth first) */
    if (r_ptr_a->level != r_ptr_b->level)
    {
        return (r_ptr_a->level >= r_ptr_b->level);
    }

    /* Same depth, sort by distance (closer first) */
    int da, db, kx, ky;

    /* Absolute distance components for a */
    kx = x[a] - px;
    kx = ABS(kx);
    ky = y[a] - py;
    ky = ABS(ky);
    da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Absolute distance components for b */
    kx = x[b] - px;
    kx = ABS(kx);
    ky = y[b] - py;
    ky = ABS(ky);
    db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Compare the distances */
    return (da <= db);
}

/*
 * Sorting hook -- comp function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by double-distance to the player.
 */
static bool ang_sort_comp_distance(const void* u, const void* v, int a, int b)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    int da, db, kx, ky;

    /* Absolute distance components */
    kx = x[a];
    kx -= px;
    kx = ABS(kx);
    ky = y[a];
    ky -= py;
    ky = ABS(ky);

    /* Approximate Double Distance to the first point */
    da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Absolute distance components */
    kx = x[b];
    kx -= px;
    kx = ABS(kx);
    ky = y[b];
    ky -= py;
    ky = ABS(ky);

    /* Approximate Double Distance to the first point */
    db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Compare the distances */
    return (da <= db);
}

/*
 * Sorting hook -- swap function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by distance to the player.
 */
static void ang_sort_swap_distance(void* u, void* v, int a, int b)
{
    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    byte temp;

    /* Swap "x" */
    temp = x[a];
    x[a] = x[b];
    x[b] = temp;

    /* Swap "y" */
    temp = y[a];
    y[a] = y[b];
    y[b] = temp;
}

/*
 * Hack -- help "select" a location (see below)
 */
s16b target_pick(int y1, int x1, int dy, int dx)
{
    int i, v;

    int x2, y2, x3, y3, x4, y4;

    int b_i = -1, b_v = 9999;

    /* Scan the locations */
    for (i = 0; i < temp_n; i++)
    {
        /* Point 2 */
        x2 = temp_x[i];
        y2 = temp_y[i];

        /* Directed distance */
        x3 = (x2 - x1);
        y3 = (y2 - y1);

        /* Verify quadrant */
        if (dx && (x3 * dx <= 0))
            continue;
        if (dy && (y3 * dy <= 0))
            continue;

        /* Absolute distance */
        x4 = ABS(x3);
        y4 = ABS(y3);

        /* Verify quadrant */
        if (dy && !dx && (x4 > y4))
            continue;
        if (dx && !dy && (y4 > x4))
            continue;

        /* Approximate Double Distance */
        v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));

        /* Penalize location XXX XXX XXX */

        /* Track best */
        if ((b_i >= 0) && (v >= b_v))
            continue;

        /* Track best */
        b_i = i;
        b_v = v;
    }

    /* Result */
    return (b_i);
}

/*
 * Hack -- determine if a given location is "interesting"
 */
static bool determine_location_is_interesting(int y, int x)
{
    object_type* o_ptr;

    /* Player grids are always interesting */
    if (cave_m_idx[y][x] < 0)
        return (true);

    /* Handle hallucination */
    if (p_ptr->image)
        return (false);

    /* Rage and labyrinth partitions both suppress remembered-grid look data. */
    if (!grid_info_is_available(y, x))
        return (false);

    /* Check for objects first (only shown when on floors, not when in rubble) */
    /* This is checked BEFORE monsters to prevent showing unmarked objects under detected monsters */
    if (cave_floorlike_bold(y, x) || (cave_feat[y][x] == FEAT_SUNLIGHT))
    {
        /* Scan all objects in the grid */
        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Memorized object - this makes the location interesting */
            if (o_ptr->marked && !object_is_searched_skeleton(o_ptr))
                return (true);
        }
    }

    /* Visible monsters (checked AFTER objects) */
    /* This ensures that a location with a monster but no marked objects */
    /* is interesting for monster targeting but NOT for object listing */
    if (cave_m_idx[y][x] > 0)
    {
        monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

        /* Visible monsters */
        if (m_ptr->ml)
            return (true);
    }

    /* Interesting memorized features */
    if (cave_info[y][x] & (CAVE_MARK))
    {
        /* Notice chasms */
        if (cave_feat[y][x] == FEAT_CHASM)
            return (true);

        /* Notice glyphs */
        if (cave_glyph(y, x))
            return (true);

        /* Notice forges */
        if (cave_forge_bold(y, x))
            return (true);

        /* Notice doors */
        if (cave_feat[y][x] == FEAT_OPEN)
            return (true);
        if (cave_feat[y][x] == FEAT_BROKEN)
            return (true);

        /* Notice stairs */
        if (cave_stair_bold(y, x))
            return (true);

        /* Notice traps */
        if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
            return (true);

        /* Notice doors */
        if (cave_known_closed_door_bold(y, x))
            return (true);

        /* Notice rubble */
        if (cave_feat[y][x] == FEAT_RUBBLE)
            return (true);
    }

    /* Nope */
    return (false);
}

/*
 * Prepare the "temp" array for "target_interactive_set"
 *
 * Return the number of target_able monsters in the set.
 */
void get_sorted_target_list(int mode, int range)
{
    int y, x;

    /* Reset "temp" array */
    temp_n = 0;

    /* Scan the current panel */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            // Previously required LOS, but this is now ignored...

            /* Require "interesting" contents */
            if (!determine_location_is_interesting(y, x))
                continue;

            /* Special mode */
            if (mode & (TARGET_KILL))
            {
                /* Must contain a monster */
                if (!(cave_m_idx[y][x] > 0))
                    continue;

                /* Must be a targetable monster */
                if (!target_able(cave_m_idx[y][x]))
                    continue;

                // possibly restrict the distance from the player
                if ((range > 0)
                    && (distance(p_ptr->py, p_ptr->px, y, x) > range))
                    continue;
            }
            else if (mode & (TARGET_LIST_MONSTER))
            {
                /* Must contain a monster */
                if (!(cave_m_idx[y][x] > 0))
                    continue;
            }
            else if (mode & (TARGET_LIST_OBJECT))
            {
                if (!(cave_o_idx[y][x] > 0))
                    continue;
            }

            /* Save the location */
            temp_x[temp_n] = x;
            temp_y[temp_n] = y;
            temp_n++;
        }
    }

    /* Set the sort hooks */
    if (mode & (TARGET_LIST_MONSTER))
    {
        /* Use monster priority sorting (uniques first, then by depth, then by distance) */
        ang_sort_comp = ang_sort_comp_monster_priority;
    }
    else
    {
        /* Use distance sorting for objects and other targets */
        ang_sort_comp = ang_sort_comp_distance;
    }
    ang_sort_swap = ang_sort_swap_distance;

    /* Sort the positions */
    ang_sort(temp_x, temp_y, temp_n);
}

/*
 * Examine a grid, return a keypress.
 *
 * The "mode" argument contains the "TARGET_LOOK" bit flag, which
 * indicates that the "space" key should scan through the contents
 * of the grid, instead of simply returning immediately.  This lets
 * the "look" command get complete information, without making the
 * "target" command annoying.
 *
 * The "info" argument contains the "commands" which should be shown
 * inside the "[xxx]" text.  This string must never be empty, or grids
 * containing monsters will be displayed with an extra comma.
 *
 * Note that if a monster is in the grid, we update both the monster
 * recall info and the health bar info to track that monster.
 *
 * This function correctly handles multiple objects per grid, and objects
 * and terrain features in the same grid, though the latter never happens.
 *
 * This function must handle blindness/hallucination.
 */
static int target_set_interactive_aux(int y, int x, int mode, cptr info, bool use_story_font)
{
    s16b this_o_idx, next_o_idx = 0;

    cptr s1, s2, s3;

    bool boring;

    bool floored;

    int feat;

    int query;

    char out_val[256];

    /* Repeat forever */
    while (1)
    {
        char more[8];
        // reset the 'more' buffer
        strnfmt(more, 1, "");

        /* Paranoia */
        query = ' ';

        /* Assume boring */
        boring = true;

        /* Default */
        s1 = "You see ";
        s2 = "";
        s3 = "";

        /* The player */
        if (cave_m_idx[y][x] < 0)
        {
            /* Description */
            s1 = "You are ";

            /* Preposition */
            s2 = "on ";
        }

        /* Hack -- hallucination */
        if (p_ptr->image)
        {
            /* Display a message */
            strnfmt(out_val, sizeof(out_val),
                "What you see is not to be believed.  [%s]", info);

            look_prt(use_story_font, out_val, 0, 0);
            move_cursor_relative(y, x);
            query = inkey_movement_context(MOVEMENT_INPUT_CONTEXT_TARGETING);

            /* Stop on everything but "return" */
            if ((query != '\n') && (query != '\r'))
                break;

            /* Repeat forever */
            continue;
        }

        /* Actual monsters */
        if ((cave_m_idx[y][x] > 0) && grid_info_is_available(y, x))
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Visible */
            if (m_ptr->ml)
            {
                bool recall = false;

                char m_name[80];

                bool show_more = false;

                /* Not boring */
                boring = false;

                if (p_ptr->rage)
                {
                    SDL_strlcpy(m_name, "an enemy", sizeof(m_name));
                }
                else
                {
                    /* Get the monster name ("a kobold") */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
                }

                /* Hack -- track this monster race */
                monster_race_track(m_ptr->r_idx);

                /* Hack -- health bar for this monster */
                health_track(cave_m_idx[y][x]);

                /* Hack -- handle stuff */
                handle_stuff();

                /* Interact */
                while (1)
                {
                    /* Recall, but not when raging */
                    if ((recall) && !p_ptr->rage)
                    {
                        int recall_key;

                        /* Save screen */
                        screen_save();

                        /* Recall on screen */
                        recall_key = screen_roff(m_ptr->r_idx, m_ptr);

                        if (recall_key)
                        {
                            query = (char)recall_key;
                        }
                        else
                        {
                            /* Hack -- Complete the prompt (again) */
                            Term_addstr(-1, TERM_WHITE,
                                format("  [(r)ecall, %s]", info));

                            /* Command */
                            query = inkey_movement_context(
                                MOVEMENT_INPUT_CONTEXT_TARGETING);
                        }

                        /* Load screen */
                        screen_load();
                    }

                    /* Normal */
                    else
                    {
                        /* Describe the monster, unless a mimic */
                        char buf[80];

                        look_mon_desc(buf, sizeof(buf), cave_m_idx[y][x]);

                        // determine if there is more info to display...

                        // visible squares with monsters holding things
                        if ((cave_info[y][x] & (CAVE_SEEN))
                            && m_ptr->hold_o_idx)
                        {
                            show_more = true;
                        }

                        // known objects on the floor
                        else if (grid_info_is_available(y, x)
                            && (cave_floorlike_bold(y, x)
                                || (cave_feat[y][x] == FEAT_SUNLIGHT))
                            && cave_o_idx[y][x]
                            && (&o_list[cave_o_idx[y][x]])->marked)
                        {
                            show_more = true;
                        }

                        // standing in a known unusual terrain such as wall or
                        // door
                        else if (!cave_floorlike_bold(y, x)
                            && (cave_info[y][x] & (CAVE_MARK)))
                        {
                            show_more = true;
                        }

                        if (show_more)
                        {
                            strnfmt(more, 8, "-more- ");
                        }

                        /* Describe, and prompt for recall */
                        if (p_ptr->wizard)
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s] (%d:%d)", s1, s2,
                                s3, m_name, buf, more, info, y, x);
                        }

                        else
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s]", s1, s2, s3,
                                m_name, buf, more, info);
                        }

                        look_prt(use_story_font, out_val, 0, 0);

                        /* Place cursor */
                        move_cursor_relative(y, x);

                        /* Command */
                        query = inkey_movement_context(
                            MOVEMENT_INPUT_CONTEXT_TARGETING);
                    }

                    /* Normal commands */
                    if (query != 'r')
                        break;

                    /* Toggle recall */
                    recall = !recall;
                }

                /* Stop on everything but "return"/"space" */
                if ((query != '\n') && (query != '\r') && (query != ' '))
                    break;

                /* Sometimes stop at "space" key */
                if ((query == ' ') && !(mode & (TARGET_LOOK)))
                    break;

                /* Stop if not asked to continue */
                if (!show_more)
                    break;

                /* Change the intro */
                s1 = "It is ";

                /* Hack -- take account of gender */
                if (r_ptr->flags1 & (RF1_FEMALE))
                    s1 = "She is ";
                else if (r_ptr->flags1 & (RF1_MALE))
                    s1 = "He is ";

                /* Use a preposition */
                s2 = "carrying ";

                /* Scan all objects being carried */
                for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
                     this_o_idx = next_o_idx)
                {
                    char o_name[80];

                    object_type* o_ptr;

                    /* Get the object */
                    o_ptr = &o_list[this_o_idx];

                    /* Get the next object */
                    next_o_idx = o_ptr->next_o_idx;

                    /*Don't let the player see certain objects (used for vault
                     * treasure)*/
                    if ((o_ptr->ident & (IDENT_HIDE_CARRY)) && (!p_ptr->wizard)
                        && (!cheat_peek))
                        continue;

                    /* Obtain an object description */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                    /* Describe the object */
                    if (p_ptr->wizard)
                    {
                        strnfmt(out_val, sizeof(out_val),
                            "%s%s%s%s %s [%s] (%d:%d)", s1, s2, s3, o_name,
                            more, info, y, x);
                    }
                    else
                    {
                        strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]",
                            s1, s2, s3, o_name, more, info);
                    }

                    look_prt(use_story_font, out_val, 0, 0);
                    move_cursor_relative(y, x);
                    query = inkey_movement_context(
                        MOVEMENT_INPUT_CONTEXT_TARGETING);

                    /* Stop on everything but "return"/"space" */
                    if ((query != '\n') && (query != '\r') && (query != ' '))
                        break;

                    /* Sometimes stop at "space" key */
                    if ((query == ' ') && !(mode & (TARGET_LOOK)))
                        break;

                    /* Change the intro */
                    s2 = "also carrying ";
                }

                /* Double break */
                if (this_o_idx)
                    break;

                /* Use a preposition */
                s2 = "on ";
            }
        }
        // if the square doesn't include a monster...
        else
        {
            // cancel health tracking
            health_track(0);

            /* Hack -- handle stuff */
            handle_stuff();
        }

        /* Assume not floored */
        floored = false;

        /* Scan all objects in the grid */
        for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Skip objects if floored */
            if (floored)
                continue;

            /* Objects (only shown when on floors, not when in rubble) */
            if (cave_floorlike_bold(y, x) || (cave_feat[y][x] == FEAT_SUNLIGHT))
            {
                /* Describe it */
                if (o_ptr->marked && grid_info_is_available(y, x))
                {
                    char o_name[80];

                    /* Not boring */
                    boring = false;

                    /* Obtain an object description */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                    /* Describe the object */
                    if (p_ptr->wizard)
                    {
                        strnfmt(out_val, sizeof(out_val),
                            "%s%s%s%s %s [%s] (%d:%d)", s1, s2, s3, o_name,
                            more, info, y, x);
                    }
                    else
                    {
                        strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]",
                            s1, s2, s3, o_name, more, info);
                    }

                    look_prt(use_story_font, out_val, 0, 0);
                    move_cursor_relative(y, x);
                    query = inkey_movement_context(
                        MOVEMENT_INPUT_CONTEXT_TARGETING);

                    /* Stop on everything but "return"/"space" */
                    if ((query != '\n') && (query != '\r') && (query != ' '))
                        break;

                    /* Sometimes stop at "space" key */
                    if ((query == ' ') && !(mode & (TARGET_LOOK)))
                        break;

                    /* Change the intro */
                    s1 = "It is ";

                    /* Plurals */
                    if (o_ptr->number != 1)
                        s1 = "They are ";

                    /* Preposition */
                    s2 = "on ";
                }
            }
        }

        /* Double break */
        if (this_o_idx)
            break;

        /* Feature (apply "mimic") */
        feat = f_info[cave_feat[y][x]].mimic;

        /* Require knowledge about grid, or ability to see grid */
        if ((!grid_info_is_available(y, x)
                || (!(cave_info[y][x] & (CAVE_MARK))
                    && !player_can_see_bold(y, x)))
            && (distance(p_ptr->py, p_ptr->px, y, x) > 0))
        {
            /* Forget feature */
            feat = FEAT_NONE;
        }

        /* Terrain feature if needed */
        if (boring || !cave_floorlike_bold(y, x))
        {
            cptr name = f_name + f_info[feat].name;
            char name_buf[80];

            /* Hack -- handle unknown grids */
            if (feat == FEAT_NONE)
                name = "unknown square";

            /* Note a trap the player has rewired to catch monsters */
            else if (cave_rewired[y][x] && (feat >= FEAT_TRAP_HEAD)
                && (feat <= FEAT_TRAP_TAIL))
            {
                strnfmt(name_buf, sizeof(name_buf), "%s (rewired)", name);
                name = name_buf;
            }

            /* Pick a prefix */
            if (*s2 && (feat >= FEAT_DOOR_HEAD))
                s2 = "in ";

            /* Use the definite article for the unique forge */
            if ((feat >= FEAT_FORGE_UNIQUE_HEAD)
                && (feat <= FEAT_FORGE_UNIQUE_TAIL))
            {
                s3 = "the ";
            }

            /* Pick proper indefinite article */
            else
            {
                s3 = (is_a_vowel(name[0])) ? "an " : "a ";
            }

            /* Display a message */
            if (p_ptr->wizard)
            {
                strnfmt(out_val, sizeof(out_val),
                    "%s%s%s%s (%d) %s [%s] (%d:%d)", s1, s2, s3, name,
                    cave_feat[y][x], more, info, y, x);
            }
            else
            {
                strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]", s1, s2,
                    s3, name, more, info);
            }

            look_prt(use_story_font, out_val, 0, 0);
            move_cursor_relative(y, x);
            query = inkey_movement_context(MOVEMENT_INPUT_CONTEXT_TARGETING);

            /* Stop on everything but "return"/"space" */
            if ((query != '\n') && (query != '\r') && (query != ' '))
                break;
        }

        /* Stop on everything but "return" */
        if ((query != '\n') && (query != '\r'))
            break;
    }

    // make sure the health tracking is sorted out
    if (p_ptr->target_who)
    {
        health_track(p_ptr->target_who);
    }
    else
    {
        health_track(0);
    }

    /* Keep going */
    return (query);
}

/*
 * Draw a visible path over the squares between (x1,y1) and (x2,y2).
 * The path consists of "*", which are white except where there is a
 * monster, object or feature in the grid.
 *
 * This routine has (at least) three weaknesses:
 * - remembered objects/walls which are no longer present are not shown,
 * - squares which (e.g.) the player has walked through in the dark are
 *   treated as unknown space.
 * - walls which appear strange due to hallucination aren't treated correctly.
 *
 * The first two result from information being lost from the dungeon arrays,
 * which requires changes elsewhere
 */
static int draw_path(
    u16b* path, int range, char* c, byte* a, int y1, int x1, int y2, int x2)
{
    int i;
    int max;
    bool on_screen;

    /* Find the path. */
    max = project_path(
        path, range, y1, x1, &y2, &x2, (PROJECT_THRU | PROJECT_INVISIPASS));

    /* No path, so do nothing. */
    if (max < 1)
        return 0;

    /* The starting square is never drawn, but notice if it is being
     * displayed. In theory, it could be the last such square.
     */
    on_screen = panel_contains(y1, x1);

    /* Draw the path. */
    for (i = 0; i < max; i++)
    {
        byte colour;

        /* Find the co-ordinates on the level. */
        int y = GRID_Y(path[i]);
        int x = GRID_X(path[i]);
        /*
         * As path[] is a straight line and the screen is oblong,
         * there is only section of path[] on-screen.
         * If the square being drawn is visible, this is part of it.
         * If none of it has been drawn, continue until some of it
         * is found or the last square is reached.
         * If some of it has been drawn, finish now as there are no
         * more visible squares to draw.
         */

        if (panel_contains(y, x))
            on_screen = true;
        else if (on_screen)
            break;
        else
            continue;

        /* Find the position on-screen */
        move_cursor_relative(y, x);

        /* This square is being overwritten, so save the original. */
        Term_what(Term->scr->cx, Term->scr->cy, a + i, c + i);

        /* Choose a colour. */
        /* Visible monsters are red. */
        if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
        {
            colour = TERM_L_RED;
        }

        /* Known objects are yellow. */
        else if (cave_o_idx[y][x] && o_list[cave_o_idx[y][x]].marked)
        {
            colour = TERM_YELLOW;
        }

        /* Known walls are blue. */
        else if (!cave_floor_bold(y, x)
            && (cave_info[y][x] & (CAVE_MARK) || player_can_see_bold(y, x)))
        {
            colour = TERM_BLUE;
        }
        /* Unknown squares are grey. */
        else if (!(cave_info[y][x] & (CAVE_MARK)) && !player_can_see_bold(y, x))
        {
            colour = TERM_L_DARK;
        }
        /* Unoccupied squares are white. */
        else
        {
            colour = TERM_WHITE;
        }

        /* Draw the path segment */
        (void)Term_addch(colour, '*');
    }
    return i;
}

/*
 * Load the attr/char at each point along "path" which is on screen from
 * "a" and "c". This was saved in draw_path().
 */
static void load_path(int max, u16b* path, char* c, byte* a)
{
    int i;
    for (i = 0; i < max; i++)
    {
        if (!panel_contains(GRID_Y(path[i]), GRID_X(path[i])))
            continue;

        move_cursor_relative(GRID_Y(path[i]), GRID_X(path[i]));

        (void)Term_addch(a[i], c[i]);
    }

    Term_fresh();
}

/*
 * Handle "target" and "look".
 *
 * Note that this code can be called from "get_aim_dir()".
 *
 * Currently, when "flag" is true, that is, when
 * "interesting" grids are being used, and a directional key is used, we
 * only scroll by a single panel, in the direction requested, and check
 * for any interesting grids on that panel.  The "correct" solution would
 * actually involve scanning a larger set of grids, including ones in
 * panels which are adjacent to the one currently scanned, but this is
 * overkill for this function.  XXX XXX
 *
 * Hack -- targetting/observing an "outer border grid" may induce
 * problems, so this is not currently allowed.
 *
 * The player can use the direction keys to move among "interesting"
 * grids in a heuristic manner, or the "+" and "-" keys to
 * move through the "interesting" grids in a sequential manner, or
 * can enter "location" mode, and use the direction keys to move one
 * grid at a time in any direction.  The confirm command will
 * only target a monster (as opposed to a location) if the monster is
 * target_able and the "interesting" mode is being used.
 *
 * The current grid is described using the "look" method above, and
 * a new command may be entered at any time, but note that if the
 * "TARGET_LOOK" bit flag is set (or if we are in "location" mode,
 * where "space" has no obvious meaning) then "space" will scan
 * through the description of the current grid until done, instead
 * of immediately jumping to the next "interesting" grid.  This
 * allows the "target" command to retain its old semantics.
 *
 * The "*", "+", and "-" keys may always be used to jump immediately
 * to the next (or previous) interesting grid, in the proper mode.
 *
 * The "return" key may always be used to scan through a complete
 * grid description (forever).
 *
 * if the range variable is 0, there is no range limit
 *
 * This command will cancel any old target, even if used from
 * inside the "look" command.
 */
void target_prompt_label(int binding, cptr fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (!buf[0] || streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

static void target_mode_prompt(
    char* info, size_t info_len, bool valid_target, bool manual_mode)
{
    cptr toggle_name = manual_mode ? "auto" : "manual";

    if (!info || !info_len)
        return;

    if (steamdeck_controls_active())
    {
        char confirm_label[24];
        char toggle_label[24];

        target_prompt_label(INPUT_BIND_CONFIRM, "A", confirm_label,
            sizeof(confirm_label));
        target_prompt_label('s', "Y", toggle_label, sizeof(toggle_label));

        if (valid_target)
        {
            strnfmt(info, info_len, "%s=target, %s=%s, <dir>",
                confirm_label, toggle_label, toggle_name);
        }
        else
        {
            strnfmt(info, info_len, "%s=%s, <dir>", toggle_label,
                toggle_name);
        }

        return;
    }

    if (valid_target)
    {
        strnfmt(info, info_len, "Space=target, (s)%s, <dir>",
            toggle_name);
    }
    else
    {
        strnfmt(info, info_len, "(s)%s, <dir>", toggle_name);
    }
}

bool target_set_interactive(int mode, int range)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, d, m, t, bd;

    int y = py;
    int x = px;

    int y2; // these dummy variables are needed in path determination stuff
    int x2;

    int adjusted_range;

    bool done = false;

    bool flag = true;

    bool valid_target;

    bool new_target = false;

    int query;

    char info[80];

    bool use_story_look = story_look_enabled() && (mode & TARGET_LOOK);

    /* These are used for displaying the path to the target */
    u16b path[MAX_RANGE];
    char path_char[MAX_RANGE];
    byte path_attr[MAX_RANGE];
    int max;

    bool wiz = mode & (TARGET_WIZ);

    // turn off auto if doing wizard mode dungeon modification
    if (wiz)
        flag = false;

    if (range == 0)
        adjusted_range = MAX_RANGE;
    else
        adjusted_range = range;

    /* Prepare the "temp" array */
    get_sorted_target_list(mode, range);

    /* Start near the player */
    m = 0;

    /* Interact */
    while (!done)
    {
        max = 0;

        /* Interesting grids */
        if (flag && temp_n)
        {
            y = temp_y[m];
            x = temp_x[m];

            y2 = y;
            x2 = x;

            // need to compute 'max' whether or not we are in 'target mode'
            // in order to correctly determine if a square is targetable
            // taking player's limited knowledge into account

            max = project_path(path, adjusted_range, py, px, &y2, &x2,
                (PROJECT_THRU | PROJECT_INVISIPASS));

            /* Draw the path in "target" mode. If there is one */
            if (mode & (TARGET_KILL))
                (void)draw_path(
                    path, adjusted_range, path_char, path_attr, py, px, y, x);

            // Check whether the target location is valid (ie within the path)
            if ((max == 0)
                || ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                        || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
                    && (((GRID_X(path[max - 1]) <= x) && (x <= px))
                        || ((GRID_X(path[max - 1]) >= x) && (x >= px)))))
            {
                valid_target = true;
            }
            else
            {
                valid_target = false;
            }

            /* Prepare the relevant prompt */
            target_mode_prompt(info, sizeof(info), valid_target, false);

            /* Describe and Prompt */
            if (use_story_look)
                sdl_story_font_enable();
            query = target_set_interactive_aux(y, x, mode, info, use_story_look);
            if (use_story_look)
                sdl_story_font_disable();

            /* Remove the path */
            if (mode & (TARGET_KILL))
                load_path(max, path, path_char, path_attr);

            /* Assume no "direction" */
            d = 0;

            /* Analyze */
            switch (query)
            {
            case ESCAPE:
            case 'q':
            {
                done = true;
                break;
            }

            case '*':
            case '+':
            {
                if (++m == temp_n)
                {
                    m = 0;
                }
                break;
            }

            case '-':
            {
                if (m-- == 0)
                {
                    m = temp_n - 1;
                }
                break;
            }

            case 'p':
            {
                /* Recenter around player */
                verify_panel();

                /* Handle stuff */
                handle_stuff();

                y = py;
                x = px;
                __attribute__((fallthrough));
            }

            case 's':
            case 'm':
            {
                flag = false;
                break;
            }

            case ' ':
            case INPUT_BIND_CONFIRM:
            case 't':
            case '5':
            case 'z':
            case '\n':
            case '\r':
            {
                int m_idx = cave_m_idx[y][x];

                if ((p_ptr->py == y) && (p_ptr->px == x))
                {
                    done = true;
                }
                else if ((m_idx > 0) && target_able(m_idx))
                {
                    health_track(m_idx);
                    target_set_monster(m_idx);
                    new_target = true;
                    done = true;
                }
                else if (valid_target)
                {
                    target_set_location(y, x);
                    health_track(0);
                    new_target = true;
                    done = true;
                }
                else
                {
                    bell("Illegal target.");
                }
                break;
            }

            default:
            {
                /* Extract direction */
                d = target_dir((char)query);

                /* Oops */
                if (!d)
                    bell("Illegal command for target mode!");

                break;
            }
            }

            /* Hack -- move around */
            if (d)
            {
                int old_y = temp_y[m];
                int old_x = temp_x[m];

                /* Find a new monster */
                i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                /* Scroll to find interesting grid */
                if (i < 0)
                {
                    int old_wy = p_ptr->wy;
                    int old_wx = p_ptr->wx;

                    /* Change if legal */
                    if (change_panel(d))
                    {
                        /* Recalculate interesting grids */
                        get_sorted_target_list(mode, range);

                        /* Find a new monster */
                        i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                        /* Restore panel if needed */
                        if ((i < 0) && modify_panel(old_wy, old_wx))
                        {
                            /* Recalculate interesting grids */
                            get_sorted_target_list(mode, range);
                        }

                        /* Handle stuff */
                        handle_stuff();
                    }
                }

                /* Use interesting grid if found */
                if (i >= 0)
                    m = i;
            }
        }

        /* Arbitrary grids */
        else if (!wiz)
        {
            y2 = y;
            x2 = x;

            // need to compute 'max' whether or not we are in 'target mode'
            // in order to correctly determine if a square is targetable
            // taking player's limited knowledge into account
            max = project_path(path, adjusted_range, py, px, &y2, &x2,
                (PROJECT_THRU | PROJECT_INVISIPASS));

            /* Draw the path in "target" mode. If there is one */
            if (mode & (TARGET_KILL))
                (void)draw_path(
                    path, adjusted_range, path_char, path_attr, py, px, y, x);

            // Check whether the target location is valid (ie within the path)
            if ((max == 0)
                || ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                        || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
                    && (((GRID_X(path[max - 1]) <= x) && (x <= px))
                        || ((GRID_X(path[max - 1]) >= x) && (x >= px)))))
            {
                valid_target = true;
            }
            else
            {
                valid_target = false;
            }

            /* Prepare the relevant prompt */
            target_mode_prompt(
                info, sizeof(info), valid_target || p_ptr->wizard, true);

            /* Describe and Prompt (enable "TARGET_LOOK") */
            if (use_story_look)
                sdl_story_font_enable();
            query = target_set_interactive_aux(y, x,
                (mode & TARGET_KILL) ? mode : (mode | TARGET_LOOK), info,
                use_story_look);
            if (use_story_look)
                sdl_story_font_disable();

            /* Remove the path */
            if (mode & (TARGET_KILL))
                load_path(max, path, path_char, path_attr);

            /* Assume no direction */
            d = 0;

            /* Analyze the keypress */
            switch (query)
            {
            case ESCAPE:
            case 'q':
            {
                done = true;
                break;
            }

            case 'p':
            {
                /* Recenter around player */
                verify_panel();

                /* Handle stuff */
                handle_stuff();

                y = py;
                x = px;
                __attribute__((fallthrough));
            }

            case 's':
            case 'a':
            {
                flag = true;

                m = 0;
                bd = 999;

                /* Pick a nearby monster */
                for (i = 0; i < temp_n; i++)
                {
                    t = distance(y, x, temp_y[i], temp_x[i]);

                    /* Pick closest */
                    if (t < bd)
                    {
                        m = i;
                        bd = t;
                    }
                }

                /* Nothing interesting */
                if (bd == 999)
                    flag = false;

                break;
            }

            case ' ':
            case INPUT_BIND_CONFIRM:
            case 't':
            case '5':
            case 'z':
            case '\n':
            case '\r':
            {
                if ((p_ptr->py == y) && (p_ptr->px == x))
                {
                    done = true;
                }
                else if (valid_target || p_ptr->wizard)
                {
                    target_set_location(y, x);
                    health_track(0);
                    new_target = true;
                    done = true;
                }
                else
                {
                    bell("Illegal target.");
                }
                break;
            }

            default:
            {
                /* Extract a direction */
                d = target_dir((char)query);

                /* Oops */
                if (!d)
                    bell("Illegal command for target mode!");

                break;
            }
            }

            /* Handle "direction" */
            if (d)
            {
                /* Move */
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    /* Handle stuff */
                    handle_stuff();

                    /* Recalculate interesting grids */
                    get_sorted_target_list(mode, range);
                }
            }
        }

        /* Wizard dungeon modification */
        else
        {
            bool inc_monster = false;
            bool inc_object = false;
            bool inc_terrain = false;
            bool reroll_monster = false;
            bool reroll_object = false;
            bool found = false;

            y2 = y;
            x2 = x;

            // prepare the relevant prompt
            SDL_strlcpy(info, "<space>, <tab>, <dir>", sizeof(info));

            /* Describe and Prompt (enable "TARGET_LOOK") */
            query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info, use_story_look);

            /* Remove the path */
            if (mode & (TARGET_KILL))
                load_path(max, path, path_char, path_attr);

            /* Assume no direction */
            d = 0;

            // space increments (and is handled specially)
            if (query == ' ')
            {
                // increment a monster race
                if (cave_m_idx[y][x])
                    inc_monster = true;
                // increment an object kind
                else if (cave_o_idx[y][x])
                    inc_object = true;
                // increment a terrain type
                else
                    inc_terrain = true;
            }

            // tab rerolls (and is handled specially)
            if (query == '\t')
            {
                // reroll a monster race
                if (cave_m_idx[y][x])
                    reroll_monster = true;
                // reroll an object kind
                else if (cave_o_idx[y][x])
                    reroll_object = true;
            }

            // escape exits
            if (query == ESCAPE)
            {
                done = true;
            }

            // backspace changes the light level (and is handled specially)
            else if (query == '\b')
            {
                // toggle the cave_glow value
                if (cave_info[y][x] & (CAVE_GLOW))
                {
                    cave_info[y][x] &= ~(CAVE_GLOW);
                    if (cave_floorlike_bold(y, x))
                    {
                        cave_info[y][x] &= ~(CAVE_MARK);
                    }
                }
                else
                {
                    cave_info[y][x] |= (CAVE_GLOW);
                }

                update_view();
            }

            // numbers move
            else if (strchr("12346789", query))
            {
                /* Extract a direction */
                d = target_dir((char)query);
            }

            // summon a creature
            else if (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX"
                            "YZ&@",
                         query)
                || inc_monster || reroll_monster)
            {
                monster_race* r_ptr;
                monster_race* old_r_ptr;
                monster_type* m_ptr;

                // recreate a monster of the same type.
                if (reroll_monster)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    i = m_ptr->r_idx;
                    found = true;
                }

                // go through monster race list and find next monster with that
                // symbol.
                else if (inc_monster)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    old_r_ptr = &r_info[m_ptr->r_idx];

                    for (i = 1; i < z_info->r_max; i++)
                    {
                        r_ptr = &r_info[(i + m_ptr->r_idx) % z_info->r_max];

                        // stop when you find one
                        if ((r_ptr->d_char == old_r_ptr->d_char)
                            && (r_ptr->cur_num < r_ptr->max_num)
                            && (r_ptr->level <= 25))
                        {
                            found = true;
                            i = (i + m_ptr->r_idx) % z_info->r_max;
                            break;
                        }
                    }
                }

                // go through monster race list and find first monster with that
                // symbol.
                else
                {
                    for (i = 1; i < z_info->r_max; i++)
                    {
                        r_ptr = &r_info[i];

                        // stop when you find one
                        if (r_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing monster
                    if (cave_m_idx[y][x])
                    {
                        delete_monster_idx(cave_m_idx[y][x]);
                    }
                    // place the new one
                    place_monster_one(y, x, i, true, true, NULL);
                }
            }

            // create an object
            else if (strchr("([)|/\\]}-~*\"=_?!~,", query) || inc_object
                || reroll_object)
            {
                object_kind* old_k_ptr;
                object_type* o_ptr;
                object_kind* k_ptr;
                object_type* i_ptr;
                object_type object_type_body;

                // recreate an object of the same type.
                if (reroll_object)
                {
                    o_ptr = &o_list[cave_o_idx[y][x]];
                    i = o_ptr->k_idx;
                    found = true;
                }

                // go through object kind list and find next object kind with
                // that symbol.
                else if (inc_object)
                {
                    o_ptr = &o_list[cave_o_idx[y][x]];
                    old_k_ptr = &k_info[o_ptr->k_idx];

                    for (i = 1; i < z_info->k_max; i++)
                    {
                        k_ptr = &k_info[(i + o_ptr->k_idx) % z_info->k_max];

                        // stop when you find one
                        if (k_ptr->d_char == old_k_ptr->d_char)
                        {
                            found = true;
                            i = (i + o_ptr->k_idx) % z_info->k_max;
                            break;
                        }
                    }
                }

                // go through object kind list and find first object kind with
                // that symbol.
                else
                {
                    for (i = 1; i < z_info->k_max; i++)
                    {
                        k_ptr = &k_info[i];

                        // stop when you find one
                        if (k_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing item
                    if (cave_o_idx[y][x])
                    {
                        delete_object_idx(cave_o_idx[y][x]);
                    }

                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Create the item */
                    object_prep(i_ptr, i);

                    /* Apply magic (no messages, no artefacts) */
                    apply_magic(
                        i_ptr, p_ptr->depth, false, false, false, false);

                    // apply the autoinscription (if any)
                    apply_autoinscription(i_ptr);

                    if (i_ptr->tval == TV_ARROW)
                        i_ptr->number = 24;

                    /* Drop the object from heaven */
                    drop_near(i_ptr, -1, y, x);
                }
            }

            // change the terrain
            else if (strchr(".;'^+#:%0<>", query) || inc_terrain)
            {
                feature_type* f_ptr;
                feature_type* old_f_ptr;

                // go through terrain list and find next terrain type with that
                // symbol.
                if (inc_terrain)
                {
                    old_f_ptr = &f_info[cave_feat[y][x]];

                    for (i = 1; i < z_info->f_max; i++)
                    {
                        f_ptr = &f_info[(i + cave_feat[y][x]) % z_info->f_max];

                        // stop when you find one
                        if (f_ptr->d_char == old_f_ptr->d_char)
                        {
                            found = true;
                            i = (i + cave_feat[y][x]) % z_info->f_max;
                            break;
                        }
                    }
                }

                // go through terrain list and find first terrain type with that
                // symbol.
                else
                {
                    for (i = 1; i < z_info->f_max; i++)
                    {
                        f_ptr = &f_info[i];

                        // stop when you find one
                        if (f_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing monster
                    if (cave_m_idx[y][x])
                    {
                        delete_monster_idx(cave_m_idx[y][x]);
                    }
                    // delete any existing item
                    if (cave_o_idx[y][x])
                    {
                        delete_object_idx(cave_o_idx[y][x]);
                    }
                    // place the new terrain
                    cave_info[y][x] &= ~(CAVE_MARK);
                    cave_set_feat(y, x, i);
                    update_view();
                }
            }

            // unexpected symbol
            else if ((query != ' ') && (query != '\t'))
            {
                bell("Illegal command for target mode!");
            }

            /* Handle "direction" */
            if (d)
            {
                /* Move */
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    /* Handle stuff */
                    handle_stuff();

                    /* Recalculate interesting grids */
                    get_sorted_target_list(mode, range);
                }
            }
        }
    }

    /* Forget */
    temp_n = 0;

    /* Clear the top line */
    prt("", 0, 0);

    /* Recenter around player */
    verify_panel();

    /* Handle stuff */
    handle_stuff();

    /* Failure to set target */
    if (!new_target)
    {
        // if we did not select a new target and were in targetting mode, then
        // abort target
        if (mode & (TARGET_KILL))
        {
            target_set_monster(0);
            health_track(0);
        }
        return (false);
    }

    /* Success */
    return (true);
}
