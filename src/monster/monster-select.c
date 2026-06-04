/* File: monster-select.c */

#include "monster-internal.h"

/*
 * Apply a "monster restriction function" to the "monster allocation table"
 */
errr get_mon_num_prep(void)
{
    int i;

    /* Scan the allocation table */
    for (i = 0; i < alloc_race_size; i++)
    {
        /* Get the entry */
        alloc_entry* entry = &alloc_race_table[i];

        /* Accept monsters which pass the restriction, if any */
        if (!get_mon_num_hook || (*get_mon_num_hook)(entry->index))
        {
            /* Accept this monster */
            entry->prob2 = entry->prob1;
        }

        /* Do not use this monster */
        else
        {
            /* Decline this monster */
            entry->prob2 = 0;
        }
    }

    /* Success */
    return (0);
}

/*
 * Choose a monster race that seems "appropriate" to the given level
 *
 * This function uses the "prob2" field of the "monster allocation table",
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an "appropriate" monster, in
 * a relatively efficient manner.
 *
 * There is a small chance (1/50) of "boosting" the given depth by
 * a small amount (up to four levels), and
 * a minimum depth enforcer for creature (unless specific monsters
 * are being called)
 *
 * It is (slightly) more likely to acquire a monster of the given level
 * than one of a lower level.  This is done by choosing several monsters
 * appropriate to the given level and keeping the "hardest" one.
 *
 * Note that if no monsters are "appropriate", then this function will
 * fail, and return zero, but this should *almost* never happen.
 *
 * The 'special' flag indicates special generation, such as for escorts
 * and this allows for a greater range of levels to be used, so as to have
 * more chance of finding a suitable monster.
 *
 * The 'allow_mindless' flag means that mindless monsters can be generated
 * This is typically only allowed on the level generation, not for additional
 * arrivals
 *
 * The 'vault' flag means that it is being generated in a vault or interesting
 * room and that the resulting level shouldn't be modified except for 'Danger'
 * items.
 *
 * Sil-y: note that most of the above is very out of date now
 *
 */
s16b get_mon_num(int level, bool special, bool allow_non_smart, bool vault)
{
    int i;

    int r_idx;

    long value, total;

    monster_race* r_ptr;

    alloc_entry* table = alloc_race_table;

    int generation_level;

    bool pursuing_monster = false;

    bool allow24 = false;
    int build_vault_type = 0;
    bool exact_token = false;
    int current_generation_depth = player_generation_depth();

    // determine the effective level:

    level = generation_depth_for_level(level);

    // default
    generation_level = level;

    // level 24 monsters can only be generated if especially asked for
    if (level == MORGOTH_DEPTH + 4)
        allow24 = true;

    // if generating escorts or similar, just use the level (which will be the
    // captain's level) this will function as the *maximum* level for generation
    if (special)
    {
        generation_level = level;
    }
    else
    {
        // deal with 'danger' items
        generation_level += p_ptr->danger;

        // various additional modifications when not created as part of a vault
        if (!vault)
        {
            // if on the run from Morgoth, then levels 17--23 used for all
            // forced smart monsters and half of others
            if (p_ptr->on_the_run && (one_in_(2) || !allow_non_smart))
            {
                pursuing_monster = true;
                generation_level = rand_range(17, 23);
            }

            if (pursuing_monster)
            {
                // leave as is
            }

            // most of the time use a small distribution
            else if (level == current_generation_depth)
            {
                // modify the effective level by a small random amount: [1, 4,
                // 6, 4, 1]
                generation_level += damroll(2, 2) - damroll(2, 2);
            }

            // other times use a tiny distribution
            else
            {
                // modify the effective level by a tiny random amount: [1, 2, 1]
                generation_level += damroll(1, 2) - damroll(1, 2);
            }
        }
    }

    // final bounds checking
    if (generation_level < 1)
        generation_level = 1;
    if (allow24)
    {
        if (generation_level > MORGOTH_DEPTH + 4)
            generation_level = MORGOTH_DEPTH + 4;
    }
    else
    {
        if (generation_level > MORGOTH_DEPTH + 3)
            generation_level = MORGOTH_DEPTH + 3;
    }

    /* Reset total */
    total = 0L;
    monster_special_vault_debug_context(&build_vault_type, &exact_token);

    /* Process probabilities */
    for (i = 0; i < alloc_race_size; i++)
    {
        /* Monsters are sorted by depth */
        if (table[i].level > generation_level)
            break;

        /* Default */
        table[i].prob3 = 0;

        /* Get the "r_idx" of the chosen monster */
        r_idx = table[i].index;

        /* Get the actual race */
        r_ptr = &r_info[r_idx];

        /* Unless in 'special' generation, ignore monsters before the
         * appropriate level */
        if (!special && (table[i].level < generation_level))
            continue;

        /* Even in 'special' generation, ignore monsters before 1/2 the
         * appropriate level */
        if (special && (table[i].level <= generation_level / 2))
            continue;

        /* Ignore monsters which are too prolific */
        if (r_ptr->cur_num >= r_ptr->max_num)
            continue;

        /* Forced depth monsters never appear out of depth */
        if ((r_ptr->flags1 & (RF1_FORCE_DEPTH))
            && (r_ptr->level > p_ptr->depth))
        {
            continue;
        }

        /* Special-vault-only monsters must not enter generic selection outside
         * their explicit vault-token or throne-room build contexts. */
        if (r_ptr->flags3 & (RF3_SPECIAL_VAULT_ONLY))
        {
            bool allowed = monster_special_vault_selection_allowed();
            log_trace(
                "SPECIAL_VAULT_ONLY select: monster='%s' r_idx=%d requested_level=%d generation_level=%d depth=%d special=%s vault=%s build_vault_type=%d exact_token=%s allowed=%s",
                r_name + r_ptr->name, r_idx, level, generation_level,
                p_ptr->depth, special ? "yes" : "no", vault ? "yes" : "no",
                build_vault_type, exact_token ? "yes" : "no",
                allowed ? "yes" : "no");
            if (!allowed)
            {
                continue;
            }
        }

        /* Non-moving monsters can't appear as out-of-depth pursuing monsters */
        if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && pursuing_monster)
        {
            continue;
        }

        /* Territorial monsters can't appear as out-of-depth pursuing monsters
         */
        if ((r_ptr->flags2 & (RF2_TERRITORIAL)) && pursuing_monster)
        {
            continue;
        }

        // forbid the generation of non-smart monsters except at level-creation
        // or specific summons
        if (!allow_non_smart
            && !((r_ptr->flags2 & (RF2_SMART))
                && !(r_ptr->flags2 & (RF2_TERRITORIAL))))
            continue;

        /* Accept */
        table[i].prob3 = table[i].prob2;

        /* Total */
        total += table[i].prob3;
    }

    /* No legal monsters */
    if (total <= 0)
        return (0);

    /* Pick a monster */
    value = rand_int(total);

    /* Find the monster */
    for (i = 0; i < alloc_race_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* Result */
    return (table[i].index);
}

