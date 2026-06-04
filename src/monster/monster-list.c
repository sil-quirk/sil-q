/* File: monster-list.c */

#include "monster-internal.h"

/*
 * Return another race for a monster to polymorph into.  -LM-
 *
 * Perform a modified version of "get_mon_num()", with exact minimum and
 * maximum depths and preferred monster types.
 */
s16b poly_r_idx(const monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    s16b base_idx = m_ptr->r_idx;

    alloc_entry* table = alloc_race_table;

    int i, min_lev, max_lev, r_idx;
    long total, value;

    /* Source monster's level and symbol */
    int r_lev = r_ptr->level;
    char d_char = r_ptr->d_char;

    /* Hack -- Uniques never polymorph */
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        return (base_idx);
    }

    /* Allowable level of new monster */
    min_lev = (MAX(1, r_lev - 1 - r_lev / 5));
    max_lev = (MIN(MAX_DEPTH, r_lev + 1 + r_lev / 5));

    /* Reset sum */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_race_size; i++)
    {
        /* Assume no probability */
        table[i].prob3 = 0;

        /* Ignore illegal monsters - only those that don't get generated. */
        if (!table[i].prob1)
            continue;

        /* Not below the minimum base depth */
        if (table[i].level < min_lev)
            continue;

        /* Not above the maximum base depth */
        if (table[i].level > max_lev)
            continue;

        /* Get the monster index */
        r_idx = table[i].index;

        /* We're polymorphing -- we don't want the same monster */
        if (r_idx == base_idx)
            continue;

        /* Get the actual race */
        r_ptr = &r_info[r_idx];

        /* Hack -- No uniques */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Accept */
        table[i].prob3 = table[i].prob2;

        /* Bias against monsters far from initial monster's depth */
        if (table[i].level < (min_lev + r_lev) / 2)
            table[i].prob3 /= 4;
        if (table[i].level > (max_lev + r_lev) / 2)
            table[i].prob3 /= 4;

        /* Bias against monsters not of the same symbol */
        if (r_ptr->d_char != d_char)
            table[i].prob3 /= 4;

        /* Sum up probabilities */
        total += table[i].prob3;
    }

    /* No legal monsters */
    if (total == 0)
    {
        return (base_idx);
    }

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

/*
 * Delete a monster by index.
 *
 * When a monster is deleted, all of its objects are deleted.
 */
void delete_monster_idx(int i)
{
    int x, y;

    monster_type* m_ptr = &mon_list[i];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    s16b this_o_idx, next_o_idx = 0;

    /* Get location */
    y = m_ptr->fy;
    x = m_ptr->fx;

    /* Hack -- Reduce the racial counter */
    r_ptr->cur_num--;

    /* Hack -- count the number of "reproducers" */
    if (r_ptr->flags2 & (RF2_MULTIPLY))
        num_repro--;

    /* Hack -- remove target monster */
    if (p_ptr->target_who == i)
        target_set_monster(0);

    /* Hack -- remove tracked monster */
    if (p_ptr->health_who == i)
        health_track(0);

    /* Monster is gone */
    cave_m_idx[y][x] = 0;
    song_disguise_handle_monster_removed(i);
    song_duels_handle_monster_removed(i);

    /* Delete objects */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Hack -- efficiency */
        o_ptr->held_m_idx = 0;

        /* Delete the object */
        delete_object_idx(this_o_idx);
    }

    /* Wipe the Monster */
    memset(m_ptr, 0, sizeof(monster_type));

    /* Count monsters */
    mon_cnt--;

    /* Visual update */
    lite_spot(y, x);
}

/*
 * Return the monster's base protection sides after permanent reductions.
 */
int monster_base_armour_sides(const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int base = r_ptr->ps;

    if (base <= 0)
        return 0;

    if (m_ptr->armor_ps_reduction >= base)
        return 0;

    return base - m_ptr->armor_ps_reduction;
}

int monster_song_hp_loss(const monster_type* m_ptr)
{
    return (int)m_ptr->song_hp_loss_lo
        | ((int)m_ptr->song_hp_loss_hi << 8);
}

void monster_add_song_hp_loss(monster_type* m_ptr, int amount)
{
    if (amount <= 0)
        return;

    int total = monster_song_hp_loss(m_ptr) + amount;
    if (total > 0xFFFF)
        total = 0xFFFF;

    m_ptr->song_hp_loss_lo = (byte)(total & 0xFF);
    m_ptr->song_hp_loss_hi = (byte)((total >> 8) & 0xFF);
}

/*
 * Delete the monster, if any, at a given location
 */
void delete_monster(int y, int x)
{
    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Delete the monster (if any) */
    if (cave_m_idx[y][x] > 0)
        delete_monster_idx(cave_m_idx[y][x]);
}

/*
 * Move a monster from index i1 to index i2 in the monster list
 */
static void compact_monsters_aux(int i1, int i2)
{
    int y, x;

    monster_type* m_ptr;

    s16b this_o_idx, next_o_idx = 0;

    /* Do nothing */
    if (i1 == i2)
        return;

    /* Old monster */
    m_ptr = &mon_list[i1];

    /* Location */
    y = m_ptr->fy;
    x = m_ptr->fx;

    /* Update the cave */
    cave_m_idx[y][x] = i2;

    /* Repair objects being carried by monster */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Reset monster pointer */
        o_ptr->held_m_idx = i2;
    }

    /* Hack -- Update the target */
    if (p_ptr->target_who == i1)
        p_ptr->target_who = i2;

    /* Hack -- Update the health bar */
    if (p_ptr->health_who == i1)
        p_ptr->health_who = i2;

    /* Hack -- move monster */
    memcpy(&mon_list[i2], &mon_list[i1], sizeof(monster_type));

    /* Hack -- wipe hole */
    memset(&mon_list[i1], 0, sizeof(monster_type));
}

/*
 * Compact and Reorder the monster list
 *
 * This function can be very dangerous, use with caution!
 *
 * When compacting monsters, we first delete far away monsters without
 * objects, starting with those of lowest level.  Then nearby monsters and
 * monsters with objects get compacted, then unique monsters. -LM-
 *
 * After "compacting" (if needed), we "reorder" the monsters into a more
 * compact order, and we reset the allocation info, and the "live" array.
 */

void compact_monsters(int size)
{
    int i, j, cnt;

    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Paranoia -- refuse to wipe too many monsters at one time */
    if (size > MAX_MONSTERS / 2)
        size = MAX_MONSTERS / 2;

    /* Compact */
    if (size)
    {
        s16b* mon_lev;
        s16b* mon_index;

        /* Allocate the "mon_lev and mon_index" arrays */
        mon_lev = mem_alloc_array(mon_max, s16b);
        mon_index = mem_alloc_array(mon_max, s16b);

        /* Message */
        msg_print("Compacting monsters...");

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        /* Scan the monster list */
        for (i = 1; i < mon_max; i++)
        {
            m_ptr = &mon_list[i];
            r_ptr = &r_info[m_ptr->r_idx];

            /* Dead monsters have minimal level (but are counted!) */
            if (!m_ptr->r_idx)
                mon_lev[i] = -1L;

            /* Get the monster level */
            else
            {
                mon_lev[i] = r_ptr->level;

                /* Uniques are protected */
                if (r_ptr->flags1 & (RF1_UNIQUE))
                    mon_lev[i] += MAX_DEPTH * 2;

                /* Nearby monsters are protected */
                else if ((character_dungeon) && (m_ptr->cdis < MAX_SIGHT))
                    mon_lev[i] += MAX_DEPTH;

                /* Monsters with objects are protected */
                else if (m_ptr->hold_o_idx)
                    mon_lev[i] += MAX_DEPTH;
            }

            /* Save this monster index */
            mon_index[i] = i;
        }

        /* Sort all the monsters by (adjusted) level */
        for (i = 0; i < mon_max - 1; i++)
        {
            for (j = 0; j < mon_max - 1; j++)
            {
                int j1 = j;
                int j2 = j + 1;

                /* Bubble sort - ascending values */
                if (mon_lev[j1] > mon_lev[j2])
                {
                    s16b tmp_lev = mon_lev[j1];
                    u16b tmp_index = mon_index[j1];

                    mon_lev[j1] = mon_lev[j2];
                    mon_index[j1] = mon_index[j2];

                    mon_lev[j2] = tmp_lev;
                    mon_index[j2] = tmp_index;
                }
            }
        }

        /* Delete monsters until we've reached our quota */
        for (cnt = 0, i = 0; i < mon_max; i++)
        {
            /* We've deleted enough monsters */
            if (cnt >= size)
                break;

            /* Get this monster, using our saved index */
            m_ptr = &mon_list[mon_index[i]];

            /* "And another one bites the dust" */
            cnt++;

            /* No need to delete dead monsters again */
            if (!m_ptr->r_idx)
                continue;

            /* Delete the monster */
            delete_monster_idx(mon_index[i]);
        }

        /* Free the "mon_lev and mon_index" arrays */
        mem_free_null(mon_lev);
        mem_free_null(mon_index);
    }

    /* Excise dead monsters (backwards!) */
    for (i = mon_max - 1; i >= 1; i--)
    {
        /* Get the i'th monster */
        monster_type* m_ptr = &mon_list[i];

        /* Skip real monsters */
        if (m_ptr->r_idx)
            continue;

        /* Move last monster into open hole */
        compact_monsters_aux(mon_max - 1, i);

        /* Compress "mon_max" */
        mon_max--;
    }
}

/*
 * Delete/Remove all the monsters when the player leaves the level
 *
 * This is an efficient method of simulating multiple calls to the
 * "delete_monster()" function, with no visual effects.
 */
void wipe_mon_list(void)
{
    int i;

    /* Delete all the monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Reduce the racial counter */
        r_ptr->cur_num--;

        /* Monster is gone */
        cave_m_idx[m_ptr->fy][m_ptr->fx] = 0;

        /* Wipe the Monster */
        memset(m_ptr, 0, sizeof(monster_type));
    }

    /* Reset "mon_max" */
    mon_max = 1;

    /* Reset "mon_cnt" */
    mon_cnt = 0;

    /* Hack -- reset "reproducer" count */
    num_repro = 0;

    /* Hack -- no more target */
    target_set_monster(0);

    /* Hack -- no more tracking */
    health_track(0);

    /* Hack -- make sure there is no player ghost */
    bones_selector = 0;
}

/*
 * Get and return the index of a "free" monster.
 *
 * This routine should almost never fail, but it *can* happen.
 */
s16b mon_pop(void)
{
    int i;

    /* Normal allocation */
    if (mon_max < MAX_MONSTERS)
    {
        /* Get the next hole */
        i = mon_max;

        /* Expand the array */
        mon_max++;

        /* Count monsters */
        mon_cnt++;

        /* Return the index */
        return (i);
    }

    /* Recycle dead monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr;

        /* Get the monster */
        m_ptr = &mon_list[i];

        /* Skip live monsters */
        if (m_ptr->r_idx)
            continue;

        /* Count monsters */
        mon_cnt++;

        /* Use this monster */
        return (i);
    }

    /* Warn the player (except during dungeon creation) */
    if (character_dungeon)
        msg_print("Too many monsters!");

    /* Try not to crash */
    return (0);
}

/*
 * Set Hallucinatory monster race
 */
int random_r_idx(void)
{
    monster_race* r_ptr;
    int race_idx;

    while (1)
    {
        race_idx = rand_int(z_info->r_max);
        r_ptr = &r_info[race_idx];
        if ((r_ptr->rarity != 0) && one_in_(r_ptr->rarity))
            return (race_idx);
    }
}

s16b monster_lookup_guid(u64b guid)
{
    if (!guid)
        return 0;

    for (s16b i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        if (r_ptr->guid == guid)
            return i;
    }

    return 0;
}

s16b monster_lookup_guid_text(const char* text)
{
    if (!text)
        return 0;

    u64b guid = 0;
    if (!parse_u64b_hex(text, &guid))
        return 0;

    return monster_lookup_guid(guid);
}

