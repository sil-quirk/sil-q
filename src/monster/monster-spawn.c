/* File: monster-spawn.c */

#include "monster-internal.h"

/*
 * Attempt to place a monster of the given race at the given location.
 *
 * This routine refuses to place out-of-depth "FORCE_DEPTH" monsters.
 *
 * This is the only function which may place a monster in the dungeon,
 * except for the savefile loading code.
 */
bool place_monster_one(
    int y, int x, int r_idx, bool slp, bool ignore_depth, monster_type* m_ptr)
{
    monster_race* r_ptr;

    monster_type* n_ptr;
    monster_type monster_type_body;

    cptr name;

    /* Paranoia */
    if (!in_bounds(y, x))
        return (false);

    /* Require empty space */
    if (!cave_empty_bold(y, x))
        return (false);

    /* Hack -- no creation on glyph of warding */
    if (cave_glyph(y, x))
        return (false);

    /* Handle failure of the "get_mon_num()" function */
    if (!r_idx)
        return (false);

    if ((feeling >= LEV_THEME_HEAD) && (character_dungeon == true))
        return (false);

    /* Race */
    r_ptr = &r_info[r_idx];

    /* The monster must be able to exist in this grid */
    if (!cave_exist_mon(r_ptr, y, x, false, false))
        return (false);

    /* Paranoia */
    if (!r_ptr->name)
        return (false);

    /*limit the population*/
    if (r_ptr->cur_num >= r_ptr->max_num)
    {
        return (false);
    }

    /* Name */
    name = (r_name + r_ptr->name);

    /* Force depth monsters may NOT normally be created out of depth */
    if ((r_ptr->flags1 & (RF1_FORCE_DEPTH)) && (p_ptr->depth < r_ptr->level)
        && !ignore_depth)
    {
        /* Cannot create */
        return (false);
    }

    /* Special generation monsters may NOT normally be created */
    if ((r_ptr->flags1 & (RF1_SPECIAL_GEN)) && !ignore_depth)
    {
        /* Cannot create */
        return (false);
    }

    if (r_ptr->flags3 & (RF3_SPECIAL_VAULT_ONLY))
    {
        int build_vault_type = 0;
        bool exact_token = false;
        bool allowed = monster_special_vault_only_allowed_at(y, x);
        monster_special_vault_debug_context(&build_vault_type, &exact_token);
        log_trace(
            "SPECIAL_VAULT_ONLY place-check: monster='%s' r_idx=%d depth=%d at=(%d,%d) ignore_depth=%s build_vault_type=%d exact_token=%s cave_g_vault=%d cave_icky=%d cave_morgoth_tunnel=%d allowed=%s",
            name, r_idx, p_ptr->depth, y, x, ignore_depth ? "yes" : "no",
            build_vault_type, exact_token ? "yes" : "no",
            (cave_info[y][x] & CAVE_G_VAULT) ? 1 : 0,
            (cave_info[y][x] & CAVE_ICKY) ? 1 : 0,
            (cave_info[y][x] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
            allowed ? "yes" : "no");
        if (!allowed)
        {
            return (false);
        }
    }

    /* Check quest monster spawning restrictions when not ignoring depth */
    if (!ignore_depth && get_mon_num_hook && !(*get_mon_num_hook)(r_idx))
    {
        /* Cannot create */
        return (false);
    }

    /* Get local monster */
    n_ptr = &monster_type_body;

    /* Clean out the monster */
    memset(n_ptr, 0, sizeof(monster_type));

    /* Save the race */
    n_ptr->r_idx = r_idx;

    /* Save the hallucinatory race */
    if (r_idx == R_IDX_MORGOTH)
    {
        n_ptr->image_r_idx = R_IDX_MORGOTH_HALLU;
    }
    else if (m_ptr != NULL)
    {
        n_ptr->image_r_idx = m_ptr->image_r_idx;
    }
    else
    {
        n_ptr->image_r_idx = random_r_idx();
    }

    /* Enforce sleeping if needed */
    if (slp)
    {
        int amount;

        if (r_ptr->sleep == 0)
            amount = 0;
        else
            amount = dieroll(r_ptr->sleep);

        // if there is a lead monster, copy its value
        if (m_ptr != NULL)
        {
            amount = ALERTNESS_ALERT - m_ptr->alertness;
        }

        // many monsters are more alert during the player's escape
        else if (p_ptr->on_the_run)
        {
            // including all monsters on the Gates level
            if ((p_ptr->depth == 0) && (amount > 0))
            {
                amount = damroll(1, 3);
            }
            // and dangerous monsters out of vaults (which are assumed to be in
            // direct pursuit)
            else if ((r_ptr->level > p_ptr->depth + 2)
                && !(cave_info[y][x] & (CAVE_ICKY)) && (amount > 0))
            {
                amount = damroll(1, 3);
            }
        }

        n_ptr->alertness = ALERTNESS_ALERT - amount;
    }
    else
    {
        if (p_ptr->depth > 0)
            n_ptr->alertness = ALERTNESS_ALERT - 1;
        else
            n_ptr->alertness = ALERTNESS_ALERT;
    }

    /* Assign average hitpoints */
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        n_ptr->maxhp = r_ptr->hdice * (1 + r_ptr->hside) / 2;

        /* Apply unique-HP curses/blessings: +20% curse, -10% blessing per stack */
        {
            int stacks = curse_flag_delta_cur(CUR_U_MON_HP);
            if (stacks > 0) {
                /* Curse: +20% per stack */
                n_ptr->maxhp = (n_ptr->maxhp * (100 + 20 * stacks)) / 100;
            } else if (stacks < 0) {
                /* Blessing: -10% per stack */
                n_ptr->maxhp = (n_ptr->maxhp * (100 + 10 * stacks)) / 100;
            }
        }
    }
    /*assign hitpoints using dice rolls*/
    else
    {
        n_ptr->maxhp = damroll(r_ptr->hdice, r_ptr->hside);

        /* Apply normal-HP curses/blessings: +20% curse, -10% blessing per stack */
        {
            int stacks = curse_flag_delta_cur(CUR_MON_HP);
            if (stacks > 0) {
                /* Curse: +20% per stack */
                n_ptr->maxhp = (n_ptr->maxhp * (100 + 20 * stacks)) / 100;
            } else if (stacks < 0) {
                /* Blessing: -10% per stack */
                n_ptr->maxhp = (n_ptr->maxhp * (100 + 10 * stacks)) / 100;
            }
        }
    }

    // marked previously encountered uniques as such
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        monster_lore* l_ptr = &l_list[n_ptr->r_idx];
        if (l_ptr->psights > 0)
            n_ptr->encountered = true;
    }

    /* Initialize mana */
    n_ptr->mana = MON_MANA_MAX;

    /* Initialize song */
    n_ptr->song = SNG_NOTHING;

    /* And start out fully healthy */
    n_ptr->hp = n_ptr->maxhp;

    /* Mark minimum range for recalculation */
    n_ptr->min_range = 0;

    /* Give almost no starting energy (avoids clumped movement) */
    // Same as old FORCE_SLEEP flag, which is now the default behaviour
    n_ptr->energy = (byte)rand_int(10);

    /* Initialize stance to STANCE_CONFIDENT as default */
    n_ptr->stance = STANCE_CONFIDENT;

    /* Place the monster in the dungeon */
    if (!monster_place(y, x, n_ptr))
        return (false);

    // reacquire monster pointer
    n_ptr = &mon_list[cave_m_idx[y][x]];

    if (r_ptr->flags3 & (RF3_SPECIAL_VAULT_ONLY))
    {
        int build_vault_type = 0;
        bool exact_token = false;
        monster_special_vault_debug_context(&build_vault_type, &exact_token);
        log_trace(
            "SPECIAL_VAULT_ONLY placed: monster='%s' r_idx=%d m_idx=%d depth=%d at=(%d,%d) build_vault_type=%d exact_token=%s cave_g_vault=%d cave_icky=%d cave_morgoth_tunnel=%d",
            name, r_idx, cave_m_idx[y][x], p_ptr->depth, y, x,
            build_vault_type, exact_token ? "yes" : "no",
            (cave_info[y][x] & CAVE_G_VAULT) ? 1 : 0,
            (cave_info[y][x] & CAVE_ICKY) ? 1 : 0,
            (cave_info[y][x] & CAVE_MORGOTH_TUNNEL) ? 1 : 0);
    }

    // give the monster a place to wander towards
    new_wandering_destination(n_ptr, m_ptr);

    /*calculate the monster_speed*/
    calc_monster_speed(y, x);

    /* Powerful monster */
    if (r_ptr->level > p_ptr->depth + 2)
    {
        /* Message for cheaters */
        if (cheat_hear)
            msg_format("(+%d: %s).", r_ptr->level - p_ptr->depth, name);

        /* Boost rating by delta-depth */
        rating += (r_ptr->level - p_ptr->depth);
    }

    /* Note the monster */
    else if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        /* Unique monsters induce message */
        if (cheat_hear)
            msg_format("Unique (%s).", name);
    }

    // Monsters that don't pursue you drop their treasure upon being created
    if (r_ptr->flags2 & (RF2_TERRITORIAL))
    {
        drop_loot(n_ptr);
    }

    /* Success */
    return (true);
}

void log_live_special_vault_only_monsters(const char* reason)
{
    int count = 0;
    static const int tracked_r_idx[] = {
        R_IDX_GOTHMOG,
        R_IDX_UNGOLIANT,
        R_IDX_GLAURUNG,
        R_IDX_GORTHAUR,
    };

    for (size_t ti = 0; ti < N_ELEMENTS(tracked_r_idx); ti++)
    {
        int r_idx = tracked_r_idx[ti];
        monster_race* r_ptr = &r_info[r_idx];

        log_trace(
            "SPECIAL_VAULT_ONLY race: reason='%s' monster='%s' r_idx=%d flags3=0x%08lx has_special=%d cur_num=%d max_num=%d",
            reason ? reason : "<none>", r_name + r_ptr->name, r_idx,
            (unsigned long)r_ptr->flags3,
            (r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY) ? 1 : 0,
            r_ptr->cur_num, r_ptr->max_num);
    }

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr;

        if (!m_ptr->r_idx)
            continue;

        r_ptr = &r_info[m_ptr->r_idx];
        if (!(r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY))
            continue;

        count++;
        log_trace(
            "SPECIAL_VAULT_ONLY live: reason='%s' monster='%s' r_idx=%d m_idx=%d depth=%d at=(%d,%d) cave_g_vault=%d cave_icky=%d cave_morgoth_tunnel=%d alertness=%d energy=%d",
            reason ? reason : "<none>", r_name + r_ptr->name, m_ptr->r_idx, i,
            p_ptr->depth, m_ptr->fy, m_ptr->fx,
            (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_G_VAULT) ? 1 : 0,
            (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_ICKY) ? 1 : 0,
            (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
            m_ptr->alertness, m_ptr->energy);
    }

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr;
        bool tracked = false;

        if (!m_ptr->r_idx)
            continue;

        for (size_t ti = 0; ti < N_ELEMENTS(tracked_r_idx); ti++)
        {
            if (m_ptr->r_idx == tracked_r_idx[ti])
            {
                tracked = true;
                break;
            }
        }

        if (!tracked)
            continue;

        r_ptr = &r_info[m_ptr->r_idx];
        log_trace(
            "SPECIAL_VAULT_ONLY tracked-live: reason='%s' monster='%s' r_idx=%d m_idx=%d depth=%d at=(%d,%d) has_special=%d cave_g_vault=%d cave_icky=%d cave_morgoth_tunnel=%d alertness=%d energy=%d",
            reason ? reason : "<none>", r_name + r_ptr->name, m_ptr->r_idx, i,
            p_ptr->depth, m_ptr->fy, m_ptr->fx,
            (r_ptr->flags3 & RF3_SPECIAL_VAULT_ONLY) ? 1 : 0,
            (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_G_VAULT) ? 1 : 0,
            (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_ICKY) ? 1 : 0,
            (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
            m_ptr->alertness, m_ptr->energy);
    }

    if (count == 0)
    {
        log_trace(
            "SPECIAL_VAULT_ONLY live: reason='%s' none depth=%d mon_max=%d",
            reason ? reason : "<none>", p_ptr->depth, mon_max);
    }
}

bool place_monster_by_guid(
    int y, int x, u64b guid, bool slp, bool ignore_depth, monster_type* summoner)
{
    s16b r_idx = monster_lookup_guid(guid);
    if (!r_idx)
    {
        log_warn("place_monster_by_guid: no monster with GUID 0x%08lx%08lx",
            (unsigned long)(guid >> 32), (unsigned long)(guid & 0xFFFFFFFFUL));
        return false;
    }

    return place_monster_one(y, x, r_idx, slp, ignore_depth, summoner);
}

/*
 * Maximum size of a group of monsters
 */
#define GROUP_MAX 18

/*
 * Attempt to place a group of monsters around the given location.
 *
 * Hack -- A group of monsters counts as a single individual for the
 * level rating.
 */
static bool place_monster_group(
    int y, int x, int r_idx, bool slp, monster_type* m_ptr, s16b group_size)
{
    int old, n, i;
    int start;

    int hack_n = 0;

    byte hack_y[GROUP_MAX];
    byte hack_x[GROUP_MAX];

    /* Maximum size */
    if (group_size > GROUP_MAX)
        group_size = GROUP_MAX;

    /* Save the rating */
    old = rating;

    /* Start on the monster */
    hack_n = 1;
    hack_x[0] = x;
    hack_y[0] = y;

    /* Puddle monsters, breadth first, up to group_size */
    for (n = 0; (n < hack_n) && (hack_n < group_size); n++)
    {
        /* Grab the location */
        int hx = hack_x[n];
        int hy = hack_y[n];

        /* Random direction */
        start = rand_int(8);

        /* Check each direction, up to group_size */
        for (i = start; (i < 8 + start) && (hack_n < group_size); i++)
        {
            int mx = hx + ddx_ddd[i % 8];
            int my = hy + ddy_ddd[i % 8];

            /* Attempt to place another monster */
            if (place_monster_one(my, mx, r_idx, slp, false, m_ptr))
            {
                /* Add it to the "hack" set */
                hack_y[hack_n] = my;
                hack_x[hack_n] = mx;
                hack_n++;
            }
        }
    }

    /* Hack -- restore the rating */
    rating = old;

    /* Return true if it places at least one monster (even if fewer than
     * desired)
     */
    if (hack_n > 1)
        return (true);
    else
        return (false);
}

/*
 * Hack -- help pick an escort type
 */
static int place_monster_idx = 0;

/*
 * Check if a quest monster should be allowed to spawn normally
 * Prevents quest monsters from spawning outside their quest contexts
 */
bool quest_monster_spawn_okay(int r_idx)
{
    /* Prevent quest monsters from spawning outside their quest contexts */
    switch (r_idx) {
        case R_IDX_TULKAS:
            /* Tulkas only spawns through quest logic */
            return false;
        case R_IDX_NIENA:
            /* Nienna only spawns through quest logic at depth 14+ */
            return false;
        case R_IDX_AULE:
            /* Aulë only spawns in special vaults/quest contexts */
            return false;
        case R_IDX_MANDOS:
            /* Mandos only spawns in special vaults/quest contexts */
            return false;
        default:
            /* All other monsters can spawn normally */
            return true;
    }
}

/*
 * Hack -- help pick an escort type
 */
static bool place_monster_okay(int r_idx)
{
    monster_race* r_ptr = &r_info[place_monster_idx];

    monster_race* z_ptr = &r_info[r_idx];

    /* Require similar "race" */
    if (z_ptr->d_char != r_ptr->d_char)
        return (false);

    /* Skip more advanced monsters */
    if (z_ptr->level > r_ptr->level)
        return (false);

    /* Skip unique monsters */
    if (z_ptr->flags1 & (RF1_UNIQUE))
        return (false);

    /* Paranoia -- Skip identical monsters */
    if (place_monster_idx == r_idx)
        return (false);

    /* Okay */
    return (true);
}

/*
 * Attempt to place a unique's unique ally at a given location
 */
static void place_monster_unique_friend(
    int y, int x, int leader_idx, bool slp, monster_type* m_ptr)
{
    int i, r;

    /* Random direction */
    int start;

    monster_race* leader_r_ptr = &r_info[leader_idx];

    /* Find the unique friend */
    for (r = 1; r < z_info->r_max; r++)
    {
        monster_race* r_ptr = &r_info[r];

        if ((r_ptr->d_char == leader_r_ptr->d_char)
            && (r_ptr->flags1 & (RF1_UNIQUE_FRIEND)))
        {
            /* Random direction */
            start = rand_int(8);

            /* Check each direction, up to escort_size */
            for (i = start; i < 8 + start; i++)
            {
                int my = y + ddy_ddd[i % 8];
                int mx = x + ddx_ddd[i % 8];

                if (!place_monster_one(my, mx, r, slp, true, m_ptr))
                {
                    // msg_format("Failed to place %d.", r);
                    continue;
                }
            }
        }
    }
}

/*
 * Attempt to place an escort of monsters around the given location
 */
static void place_monster_escort(
    int y, int x, int leader_idx, bool slp, monster_type* m_ptr)
{
    int escort_size, escort_idx;
    int n, i;

    /* Random direction */
    int start;

    monster_race* r_ptr = &r_info[leader_idx];

    int level = r_ptr->level;

    int hack_n = 0;

    byte hack_y[GROUP_MAX];
    byte hack_x[GROUP_MAX];

    int escort_idxs[GROUP_MAX];

    int extras = 0;

    /* Save previous monster restriction value. */
    bool (*get_mon_num_hook_temp)(int r_idx) = get_mon_num_hook;

    /* Calculate the number of escorts we want. */
    if (r_ptr->flags1 & (RF1_ESCORTS))
        escort_size = rand_range(8, 16);
    else
        escort_size = rand_range(4, 7);

    /* Can never have more escorts than maximum group size */
    if (escort_size > GROUP_MAX)
        escort_size = GROUP_MAX;

    /* Use the leader's monster type to restrict the escorts. */
    place_monster_idx = leader_idx;

    /* Set the escort hook */
    get_mon_num_hook = place_monster_okay;

    /* Prepare allocation table */
    get_mon_num_prep();

    /* Build monster table, get indices of all escorts */
    for (i = 0; i < escort_size; i++)
    {
        if (extras > 0)
        {
            escort_idxs[i] = escort_idxs[i - 1];
        }
        else
        {
            escort_idxs[i] = get_mon_num(level, true, false, false);

            // skip this creature if get_mon_num failed (paranoia)
            if (escort_idxs[i] == 0)
                continue;

            if (r_info[escort_idxs[i]].flags1 & (RF1_FRIENDS))
                extras = rand_range(2, 3);
            else if (r_info[escort_idxs[i]].flags1 & (RF1_FRIEND))
                extras = rand_range(1, 2);
            else
                extras = 0;
        }
    }

    escort_idx = escort_idxs[0];

    /* Start on the monster */
    hack_n = 1;
    hack_x[0] = x;
    hack_y[0] = y;

    /* Puddle monsters, breadth first, up to escort_size */
    for (n = 0; (n < hack_n) && (hack_n <= escort_size); n++)
    {
        /* Grab the location */
        int hx = hack_x[n];
        int hy = hack_y[n];

        /* Random direction */
        start = rand_int(8);

        /* Check each direction, up to escort_size */
        for (i = start; (i < 8 + start) && (hack_n <= escort_size); i++)
        {
            int mx = hx + ddx_ddd[i % 8];
            int my = hy + ddy_ddd[i % 8];

            if (!place_monster_one(my, mx, escort_idx, slp, false, m_ptr))
            {
                // msg_format("Failed to place a %d ().", escort_idx);
                continue;
            }

            /* Get index of the next escort */
            escort_idx = escort_idxs[hack_n];

            /* Add grid to the "hack" set */
            hack_y[hack_n] = my;
            hack_x[hack_n] = mx;
            hack_n++;
        }
    }

    /* Return to previous monster restrictions (usually none) */
    get_mon_num_hook = get_mon_num_hook_temp;

    /* Prepare allocation table */
    get_mon_num_prep();

    /* XXX - rebuild monster table */
    (void)get_mon_num(monster_level, false, false, false);
}

/*
 * Attempt to place a monster of the given race at the given location
 *
 * Note that certain monsters are now marked as requiring "friends".
 * These monsters, if successfully placed, and if the "grp" parameter
 * is true, will be surrounded by a "group" of identical monsters.
 *
 * Note that certain monsters are now marked as requiring an "escort",
 * which is a collection of monsters with similar "race" but lower level.
 *
 * Some monsters induce a fake "group" flag on their escorts.
 *
 * Note the "bizarre" use of non-recursion to prevent annoying output
 * when running a code profiler.
 *
 * Note the use of the new "monster allocation table" code to restrict
 * the "get_mon_num()" function to "legal" escort types.
 */
bool place_monster_aux(int y, int x, int r_idx, bool slp, bool grp)
{
    monster_race* r_ptr = &r_info[r_idx];
    monster_type* m_ptr;

    s16b friends_amount;
    s16b friend_amount;

    // relative depth  |  number in group  (FRIENDS)
    //             -2  |    2
    //             -1  |  2 / 3
    //              0  |    3
    //             +1  |  3 / 4
    //             +2  |    4

    friends_amount = (rand_range(6, 7) + (monster_level - r_ptr->level)) / 2;
    if (friends_amount < 2)
        friends_amount = 2;
    if (friends_amount > 4)
        friends_amount = 4;

    // relative depth  |  chance of having a companion  (FRIEND)
    //             -2  |    0%
    //             -1  |   25%
    //              0  |   50%
    //             +1  |   75%
    //             +2  |  100%

    friend_amount = 1;
    if (dieroll(4) <= monster_level - r_ptr->level + 2)
        friend_amount++;

    /* Place one monster, or fail */
    if (!place_monster_one(y, x, r_idx, slp, false, NULL))
        return (false);

    if (cave_m_idx[y][x] > 0)
    {
        m_ptr = &mon_list[cave_m_idx[y][x]];
    }
    else
    {
        m_ptr = NULL;
    }

    /* Require the "group" flag */
    if (!grp)
        return (true);

    /* Escorts for certain monsters */
    if (r_ptr->flags1 & (RF1_UNIQUE_FRIEND))
    {
        (void)place_monster_unique_friend(y, x, r_idx, slp, m_ptr);
    }

    /* Friends for certain monsters */
    if (r_ptr->flags1 & (RF1_FRIENDS))
    {
        (void)place_monster_group(y, x, r_idx, slp, m_ptr, friends_amount);
    }

    else if (r_ptr->flags1 & (RF1_FRIEND))
    {
        /* Attempt to place a small group */
        (void)place_monster_group(y, x, r_idx, slp, m_ptr, friend_amount);
    }

    /* Escorts for certain monsters */
    if ((r_ptr->flags1 & (RF1_ESCORT)) || (r_ptr->flags1 & (RF1_ESCORTS)))
    {
        place_monster_escort(y, x, r_idx, slp, m_ptr);
    }

    /* Success */
    return (true);
}

/*
 * Hack -- attempt to place a monster at the given location
 *
 * Attempt to find a monster appropriate to the "monster_level"
 */
bool place_monster(int y, int x, bool slp, bool grp, bool vault)
{
    int r_idx;

    /* Pick a monster */ // Hack - uses the slp flag to determine if non-smart
                         // monsters are allowed
    r_idx = get_mon_num(monster_level, false, slp, vault);

    /* Handle failure */
    if (!r_idx)
        return (false);

    /* Attempt to place the monster */
    if (place_monster_aux(y, x, r_idx, slp, grp))
        return (true);

    /* Oops */
    return (false);
}

/*
 * Roomy partitions can tolerate monster groups; non-roomy partitions should
 * stay single-file so they do not flood one origin point with dense packs.
 */
static bool monster_groups_allowed_at(int y, int x)
{
    return level_partition_kind_for_point(y, x) == LEVEL_PART_ROOMY;
}

/*
 * Attempt to allocate a random monster (or group) in the dungeon.
 *
 * It can be forced to be on the stairs and/or forced to be out of sight of the
 * player
 *
 * Returns true if the player sees it happen
 */
bool alloc_monster(bool on_stairs, bool force_undead)
{
    int y, x;
    int sy, sx;
    int attempts_left = 1000;
    int tries = 0;
    int original_monster_level = monster_level;
    char dir[5];
    char m_name[80];
    char who[80];
    char message[240];
    bool displaced = false;
    bool give_up = false;
    bool placed = false;

    // Force some monsters to be generated on the stairs
    if (on_stairs)
    {
        // no monsters come through the stairs on tutorial/challenge levels
        if (p_ptr->game_type != 0)
            return (false);

        // get a stair location
        if (!random_stair_location(&sy, &sx))
            return (false);

        // default the new location to this location
        y = sy;
        x = sx;

        // if there is something on the stairs, try adjacent squares
        if (cave_m_idx[sy][sx] != 0)
        {
            int d, y1, x1, start;
            bool moveable = true;

            // if the monster on the squares cannot move, then simply give up:
            // the stairs are blocked
            if (cave_m_idx[sy][sx] > 0)
            {
                monster_type* n_ptr = &mon_list[cave_m_idx[sy][sx]];
                monster_race* nr_ptr = &r_info[n_ptr->r_idx];

                if ((nr_ptr->flags1 & (RF1_NEVER_MOVE))
                    || (nr_ptr->flags1 & (RF1_HIDDEN_MOVE)))
                    moveable = false;
            }

            if (moveable)
            {
                // we will look through the eligible squares and choose an empty
                // one randomly
                start = rand_int(8);

                for (d = start; d < 8 + start; d++)
                {
                    y1 = sy + ddy_ddd[d % 8];
                    x1 = sx + ddx_ddd[d % 8];

                    /* Check Bounds */
                    if (!in_bounds(y1, x1))
                        continue;

                    /* Check Empty Square */
                    if (!cave_empty_bold(y1, x1))
                        continue;

                    if (cave_m_idx[y1][x1] == 0)
                    {
                        y = y1;
                        x = x1;
                        displaced = true;
                        break;
                    }
                }
            }

            if (!displaced)
                give_up = true;
        }

        // First, displace the existing monster to the safe square
        if (displaced)
        {
            monster_swap(sy, sx, y, x);

            // need to update the player's field of view if she is moved
            if ((p_ptr->py == y) && (p_ptr->px == x))
            {
                update_view();
            }
        }

        if (!give_up)
        {
            bool suppress_grp = !monster_groups_allowed_at(sy, sx);

            // Try hard to put a monster on the stairs
            while (!placed && (tries < 50))
            {
                // modify the monster generation level based on the stair type
                monster_level = player_generation_depth();
                switch (cave_feat[sy][sx])
                {
                case FEAT_LESS_SHAFT:
                {
                    monster_level -= 2;
                    sprintf(dir, "down");
                    break;
                }
                case FEAT_LESS:
                {
                    monster_level -= 1;
                    sprintf(dir, "down");
                    break;
                }
                case FEAT_MORE:
                {
                    monster_level += 1;
                    sprintf(dir, "up");
                    break;
                }
                case FEAT_MORE_SHAFT:
                {
                    monster_level += 2;
                    sprintf(dir, "up");
                    break;
                }
                }
                // correct deviant monster levels
                if (monster_level < 1)
                    monster_level = 1;

                // sometimes only wraiths are allowed
                if (force_undead)
                {
                    place_monster_by_flag(sy, sx, 3, RF3_UNDEAD, true,
                        MAX(monster_level + 3, 13));
                    placed = true;
                }

                // but usually allow most monsters
                else
                {
                    placed = place_monster(sy, sx, false, !suppress_grp, false);
                }

                tries++;
            }
        }

        // reset the monster level to the original value
        monster_level = original_monster_level;

        // print messages etc
        if (placed)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[sy][sx]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            // Display a message if seen
            if (m_ptr->ml)
            {
                monster_desc(m_name, sizeof(m_name), m_ptr, 0x88);

                if (r_ptr->flags1
                    & (RF1_FRIEND | RF1_FRIENDS | RF1_ESCORT | RF1_ESCORTS))
                {
                    SDL_strlcpy(message,
                        format("A group of enemies come %s the stair", dir),
                        240);
                }
                else
                {
                    SDL_strlcpy(message,
                        format("%^s comes %s the stair", m_name, dir), 240);
                }

                if (displaced)
                {
                    if ((p_ptr->py == y) && (p_ptr->px == x))
                    {
                        SDL_strlcpy(who, "you", 80);
                    }
                    else
                    {
                        monster_desc(who, sizeof(who),
                            &mon_list[cave_m_idx[y][x]], 0x88);
                    }

                    msg_format("%s, forcing %s out of the way!", message, who);
                }
                else
                {
                    msg_format("%s!", message);
                }
            }

            if (m_ptr->ml)
                return (true);
            else
                return (false);
        }
    }

    // Other monsters can be generated anywhere
    else
    {
        /* Find a legal, distant, unoccupied, space */
        while (attempts_left)
        {
            --attempts_left;

            /* Pick a location */
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            /* Require a grid that all monsters can exist in. */
            if (cave_naked_bold(y, x) && !los(p_ptr->py, p_ptr->px, y, x))
                break;
        }

        if (!attempts_left)
        {
            if (cheat_xtra || cheat_hear)
            {
                msg_print("Warning! Could not allocate a new monster.");
            }

            return (false);
        }

        /* In any non-ROOMY partition (big cave, chasm, cavey, ruined, labyrinth),
         * suppress group spawning. BFS floods up to 18 monsters from one origin point;
         * in open cave/chasm/blob floors this creates dense clusters. Each non-ROOMY
         * partition compensates with a higher alloc_monster loop count instead. */
        if (place_monster(y, x, true, monster_groups_allowed_at(y, x), false))
        {
            if ((cave_m_idx[y][x] > 0) && (&mon_list[cave_m_idx[y][x]])->ml)
                return (true);
            else
                return (false);
        }
    }

    /* Nope */
    return (false);
}

