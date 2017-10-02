/* File: monster2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"




/*
 * Return another race for a monster to polymorph into.  -LM-
 *
 * Perform a modified version of "get_mon_num()", with exact minimum and
 * maximum depths and preferred monster types.
 */
s16b poly_r_idx(const monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	s16b base_idx = m_ptr->r_idx;

	alloc_entry *table = alloc_race_table;

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
	min_lev = (MAX(        1, r_lev - 1 - r_lev / 5));
	max_lev = (MIN(MAX_DEPTH, r_lev + 1 + r_lev / 5));

	/* Reset sum */
	total = 0L;

	/* Process probabilities */
	for (i = 0; i < alloc_race_size; i++)
	{
		/* Assume no probability */
		table[i].prob3 = 0;

		/* Ignore illegal monsters - only those that don't get generated. */
		if (!table[i].prob1) continue;

		/* Not below the minimum base depth */
		if (table[i].level < min_lev) continue;

		/* Not above the maximum base depth */
		if (table[i].level > max_lev) continue;

		/* Get the monster index */
		r_idx = table[i].index;

		/* We're polymorphing -- we don't want the same monster */
		if (r_idx == base_idx) continue;

		/* Get the actual race */
		r_ptr = &r_info[r_idx];

		/* Hack -- No uniques */
		if (r_ptr->flags1 & (RF1_UNIQUE)) continue;

		/* Accept */
		table[i].prob3 = table[i].prob2;

		/* Bias against monsters far from initial monster's depth */
		if (table[i].level < (min_lev + r_lev) / 2) table[i].prob3 /= 4;
		if (table[i].level > (max_lev + r_lev) / 2) table[i].prob3 /= 4;

		/* Bias against monsters not of the same symbol */
		if (r_ptr->d_char != d_char) table[i].prob3 /= 4;

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
		if (value < table[i].prob3) break;

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

	monster_type *m_ptr = &mon_list[i];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	s16b this_o_idx, next_o_idx = 0;


	/* Get location */
	y = m_ptr->fy;
	x = m_ptr->fx;


	/* Hack -- Reduce the racial counter */
	r_ptr->cur_num--;

	/* Hack -- count the number of "reproducers" */
	if (r_ptr->flags2 & (RF2_MULTIPLY)) num_repro--;

	/* Hack -- remove target monster */
	if (p_ptr->target_who == i) target_set_monster(0);

	/* Hack -- remove tracked monster */
	if (p_ptr->health_who == i) health_track(0);


	/* Monster is gone */
	cave_m_idx[y][x] = 0;

	/* Delete objects */
	for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

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
	(void)WIPE(m_ptr, monster_type);

	/* Count monsters */
	mon_cnt--;


	/* Visual update */
	lite_spot(y, x);
}


/*
 * Delete the monster, if any, at a given location
 */
void delete_monster(int y, int x)
{
	/* Paranoia */
	if (!in_bounds(y, x)) return;

	/* Delete the monster (if any) */
	if (cave_m_idx[y][x] > 0) delete_monster_idx(cave_m_idx[y][x]);
}


/*
 * Move a monster from index i1 to index i2 in the monster list
 */
static void compact_monsters_aux(int i1, int i2)
{
	int y, x;

	monster_type *m_ptr;

	s16b this_o_idx, next_o_idx = 0;


	/* Do nothing */
	if (i1 == i2) return;


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
		object_type *o_ptr;

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Reset monster pointer */
		o_ptr->held_m_idx = i2;
	}

	/* Hack -- Update the target */
	if (p_ptr->target_who == i1) p_ptr->target_who = i2;

	/* Hack -- Update the health bar */
	if (p_ptr->health_who == i1) p_ptr->health_who = i2;

	/* Hack -- move monster */
	COPY(&mon_list[i2], &mon_list[i1], monster_type);

	/* Hack -- wipe hole */
	(void)WIPE(&mon_list[i1], monster_type);
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

	monster_type *m_ptr;
	monster_race *r_ptr;

	/* Paranoia -- refuse to wipe too many monsters at one time */
	if (size > MAX_MONSTERS / 2) size = MAX_MONSTERS / 2;

	/* Compact */
	if (size)
	{
		s16b *mon_lev;
		s16b *mon_index;

		/* Allocate the "mon_lev and mon_index" arrays */
		C_MAKE(mon_lev, mon_max, s16b);
		C_MAKE(mon_index, mon_max, s16b);

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
			if (!m_ptr->r_idx) mon_lev[i] = -1L;

			/* Get the monster level */
			else
			{
				mon_lev[i] = r_ptr->level;

				/* Uniques are protected */
				if (r_ptr->flags1 & (RF1_UNIQUE)) mon_lev[i] += MAX_DEPTH * 2;

				/* Nearby monsters are protected */
				else if ((character_dungeon) && (m_ptr->cdis < MAX_SIGHT))
					mon_lev[i] += MAX_DEPTH;

				/* Monsters with objects are protected */
				else if (m_ptr->hold_o_idx) mon_lev[i] += MAX_DEPTH;
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
			if (cnt >= size) break;
            
			/* Get this monster, using our saved index */
			m_ptr = &mon_list[mon_index[i]];

			/* "And another one bites the dust" */
			cnt++;

			/* No need to delete dead monsters again */
			if (!m_ptr->r_idx) continue;

			/* Delete the monster */
			delete_monster_idx(mon_index[i]);
		}

		/* Free the "mon_lev and mon_index" arrays */
		FREE(mon_lev);
		FREE(mon_index);
	}

	/* Excise dead monsters (backwards!) */
	for (i = mon_max - 1; i >= 1; i--)
	{
		/* Get the i'th monster */
		monster_type *m_ptr = &mon_list[i];

		/* Skip real monsters */
		if (m_ptr->r_idx) continue;

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
		monster_type *m_ptr = &mon_list[i];
		monster_race *r_ptr = &r_info[m_ptr->r_idx];

		/* Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Hack -- Reduce the racial counter */
		r_ptr->cur_num--;

		/* Monster is gone */
		cave_m_idx[m_ptr->fy][m_ptr->fx] = 0;

		/* Wipe the Monster */
		(void)WIPE(m_ptr, monster_type);
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
		monster_type *m_ptr;

		/* Get the monster */
		m_ptr = &mon_list[i];

		/* Skip live monsters */
		if (m_ptr->r_idx) continue;

		/* Count monsters */
		mon_cnt++;

		/* Use this monster */
		return (i);
	}


	/* Warn the player (except during dungeon creation) */
	if (character_dungeon) msg_print("Too many monsters!");

	/* Try not to crash */
	return (0);
}


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
		alloc_entry *entry = &alloc_race_table[i];

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
 * This is typically only allowed on the level generation, not for additional arrivals
 *
 * The 'vault' flag means that it is being generated in a vault or interesting room
 * and that the resulting level shouldn't be modified except for 'Danger' items.
 *
 * Sil-y: note that most of the above is very out of date now
 *
 */
s16b get_mon_num(int level, bool special, bool allow_non_smart, bool vault)
{
	int i;

	int r_idx;

	long value, total;

	monster_race *r_ptr;

	alloc_entry *table = alloc_race_table;

	int generation_level;

	bool pursuing_monster = FALSE;
	
	bool allow24 = FALSE;

	// determine the effective level:
	
	// default
	generation_level = level;
	
	// level 24 monsters can only be generated if especially asked for
	if (level == MORGOTH_DEPTH + 4) allow24 = TRUE;
		
	// if generating escorts or similar, just use the level (which will be the captain's level)
	// this will function as the *maximum* level for generation
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
			// if on the run from Morgoth, then levels 17--23 used for all forced smart monsters and half of others
			if (p_ptr->on_the_run && (one_in_(2) || !allow_non_smart))
			{
				pursuing_monster = TRUE;
				generation_level = rand_range(17,23);
			}
			
			// the surface generates monsters as levels 17--23
			if (level == 0)
			{
				pursuing_monster = TRUE;
				generation_level = rand_range(17,23);
			}
			
            if (pursuing_monster)
            {
				// leave as is
            }
            
			// most of the time use a small distribution
			else if (level == p_ptr->depth)
			{
				// modify the effective level by a small random amount: [1, 4, 6, 4, 1]
				generation_level += damroll(2,2) - damroll(2,2);
			}
			
			// other times use a tiny distribution
			else
			{
				// modify the effective level by a tiny random amount: [1, 2, 1]
				generation_level += damroll(1,2) - damroll(1,2);
			}
		}

	}
	
	// final bounds checking
	if (generation_level < 1) generation_level = 1;
	if (allow24)
	{
		if (generation_level > MORGOTH_DEPTH + 4) generation_level = MORGOTH_DEPTH + 4;
	}
	else
	{
		if (generation_level > MORGOTH_DEPTH + 3) generation_level = MORGOTH_DEPTH + 3;
	}

	/* Reset total */
	total = 0L;

	/* Process probabilities */
	for (i = 0; i < alloc_race_size; i++)
	{
		/* Monsters are sorted by depth */
		if (table[i].level > generation_level) break;

		/* Default */
		table[i].prob3 = 0;

		/* Get the "r_idx" of the chosen monster */
		r_idx = table[i].index;

		/* Get the actual race */
		r_ptr = &r_info[r_idx];

		/* Unless in 'special' generation, ignore monsters before the appropriate level */
		if (!special && (table[i].level < generation_level)) continue;

		/* Even in 'special' generation, ignore monsters before 1/2 the appropriate level */
		if (special && (table[i].level <= generation_level / 2)) continue;

		/* Ignore monsters which are too prolific */
		if (r_ptr->cur_num >= r_ptr->max_num) continue;
		
		/* Forced depth monsters never appear out of depth */
		if ((r_ptr->flags1 & (RF1_FORCE_DEPTH)) && (r_ptr->level > p_ptr->depth))
		{
			continue;
		}

		/* Non-moving monsters can't appear as out-of-depth pursuing monsters */
		if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && pursuing_monster)
		{
			continue;
		}

        /* Territorial monsters can't appear as out-of-depth pursuing monsters */
		if ((r_ptr->flags2 & (RF2_TERRITORIAL)) && pursuing_monster)
		{
			continue;
		}

		// forbid the generation of non-smart monsters except at level-creation or specific summons
		if (!allow_non_smart && !((r_ptr->flags2 & (RF2_SMART)) && !(r_ptr->flags2 & (RF2_TERRITORIAL)))) continue;

		/* Accept */
		table[i].prob3 = table[i].prob2;

		/* Total */
		total += table[i].prob3;
	}

	/* No legal monsters */
	if (total <= 0) return (0);

	/* Pick a monster */
	value = rand_int(total);

	/* Find the monster */
	for (i = 0; i < alloc_race_size; i++)
	{
		/* Found the entry */
		if (value < table[i].prob3) break;

		/* Decrement */
		value = value - table[i].prob3;
	}

	/* Result */
	return (table[i].index);
}



/*
 * Display visible monsters in a window
 */
void display_monlist(void)
{
	int idx, n;
	int line = 0;

	char *m_name;
	char buf[80];

	monster_type *m_ptr;
	monster_race *r_ptr;

	u16b *race_counts;

	if (p_ptr->image)
	{
		/* Erase the rest of the window */
		for (idx = 0; idx < Term->hgt; idx++)
		{
			/* Erase the line */
			Term_erase(0, idx, 255);
		}
		Term_putstr(3, 3, 35, TERM_L_WHITE, "What you see is not to be beleived.");
		
		return;
	}

	/* Allocate the array */
	C_MAKE(race_counts, z_info->r_max, u16b);

	/* Iterate over mon_list */
	for (idx = 1; idx < mon_max; idx++)
	{
		m_ptr = &mon_list[idx];

		/* Only visible monsters */
		if (!m_ptr->ml) continue;

		/* Bump the count for this race */
		race_counts[m_ptr->r_idx]++;
	}


	/* Iterate over mon_list ( again :-/ ) */
	for (idx = 1; idx < mon_max; idx++)
	{
		m_ptr = &mon_list[idx];
		
		n = 0;

		/* Only visible monsters */
		if (!m_ptr->ml) continue;

		/* Do each race only once */
		if (!race_counts[m_ptr->r_idx]) continue;

		/* Get monster race */
		r_ptr = &r_info[m_ptr->r_idx];

		// Start a line
		Term_putstr(0, line, 1, TERM_WHITE, " ");

		/* Append the "standard" attr/char info */
		Term_addch(r_ptr->d_attr, r_ptr->d_char);
		
		n += 2;

		if (use_graphics)
		{
			/* Append the "optional" attr/char info */
			Term_addstr(-1, TERM_WHITE, " / ");

			Term_addch(r_ptr->x_attr, r_ptr->x_char);
			n += 4;

			if (use_bigtile)
			{
				if (r_ptr->x_attr & 0x80)
					Term_addch(255, -1);
				else
					Term_addch(0, ' ');

				n++;
			}
		}
	
		/* Add race count */
		sprintf(buf, "%3d  ", race_counts[m_ptr->r_idx]);
		Term_addstr(strlen(buf), TERM_WHITE, buf);
		n += 5;

		/* Don't do this race again */
		race_counts[m_ptr->r_idx] = 0;

		/* Get the monster name */
		m_name = r_name + r_ptr->name;

		/* Obtain the length of the description */
		n += strlen(m_name);

		/* Display the entry itself */
		Term_addstr(strlen(m_name), TERM_WHITE, m_name);

		/* Erase the rest of the line */
		Term_erase(n, line, 255);

		/* Bump line counter */
		line++;
	}

	/* Free the race counters */
	FREE(race_counts);

	/* Erase the rest of the window */
	for (idx = line; idx < Term->hgt; idx++)
	{
		/* Erase the line */
		Term_erase(0, idx, 255);
	}
}

/*
 * Build a string describing a monster in some way.
 *
 * We can correctly describe monsters based on their visibility.
 * We can force all monsters to be treated as visible or invisible.
 * We can build nominatives, objectives, possessives, or reflexives.
 * We can selectively pronominalize hidden, visible, or all monsters.
 * We can use definite or indefinite descriptions for hidden monsters.
 * We can use definite or indefinite descriptions for visible monsters.
 *
 * Pronominalization involves the gender whenever possible and allowed,
 * so that by cleverly requesting pronominalization / visibility, you
 * can get messages like "You hit someone.  She screams in agony!".
 *
 * Reflexives are acquired by requesting Objective plus Possessive.
 *
 * I am assuming that no monster name is more than 65 characters long,
 * so that "char desc[80];" is sufficiently large for any result, even
 * when the "offscreen" notation is added.
 *
 * Note that the "possessive" for certain unique monsters will look
 * really silly, as in "Morgoth, Lord of Darkness's".  We should
 * perhaps add a flag to "remove" any "descriptives" in the name.
 *
 * Note that "offscreen" monsters will get a special "(offscreen)"
 * notation in their name if they are visible but offscreen.  This
 * may look silly with possessives, as in "the rat's (offscreen)".
 * Perhaps the "offscreen" descriptor should be abbreviated.
 *
 * Mode Flags:
 *   0x01 --> Objective (or Reflexive)
 *   0x02 --> Possessive (or Reflexive)
 *   0x04 --> Use indefinites for hidden monsters ("something")
 *   0x08 --> Use indefinites for visible monsters ("a kobold")
 *   0x10 --> Pronominalize hidden monsters
 *   0x20 --> Pronominalize visible monsters
 *   0x40 --> Assume the monster is hidden
 *   0x80 --> Assume the monster is visible
 *
 * Useful Modes:
 *   0x00 --> Full nominative name ("the kobold") or "it"
 *   0x04 --> Full nominative name ("the kobold") or "something"
 *   0x80 --> Banishment resistance name ("the kobold")
 *   0x88 --> Killing name ("a kobold")
 *   0x22 --> Possessive, genderized if visable ("his") or "its"
 *   0x23 --> Reflexive, genderized if visable ("himself") or "itself"
 */
void monster_desc(char *desc, size_t max, const monster_type *m_ptr, int mode)
{
	cptr res;
	monster_race *r_ptr;
	cptr name;
	bool seen, pron;

	if (p_ptr->image)
	{
		r_ptr = &r_info[m_ptr->image_r_idx];
	}
	else
	{
		r_ptr = &r_info[m_ptr->r_idx];
	}

	name = (r_name + r_ptr->name);

	/* Can we "see" it (forced, or not hidden + visible) */
	seen = ((mode & (0x80)) || (!(mode & (0x40)) && m_ptr->ml));

	/* Sexed Pronouns (seen and forced, or unseen and allowed) */
	pron = ((seen && (mode & (0x20))) || (!seen && (mode & (0x10))));

	/* First, try using pronouns, or describing hidden monsters */
	if (!seen || pron)
	{
		/* an encoding of the monster "sex" */
		int kind = 0x00;

		/* Extract the gender (if applicable) */
		if (r_ptr->flags1 & (RF1_FEMALE)) kind = 0x20;
		else if (r_ptr->flags1 & (RF1_MALE)) kind = 0x10;

		/* Ignore the gender (if desired) */
		if (!m_ptr || !pron) kind = 0x00;

		/* Assume simple result */
		res = "it";

		/* Brute force: split on the possibilities */
		switch (kind + (mode & 0x07))
		{
			/* Neuter, or unknown */
			case 0x00: res = "it"; break;
			case 0x01: res = "it"; break;
			case 0x02: res = "its"; break;
			case 0x03: res = "itself"; break;
			case 0x04: res = "something"; break;
			case 0x05: res = "something"; break;
			case 0x06: res = "something's"; break;
			case 0x07: res = "itself"; break;

			/* Male (assume human if vague) */
			case 0x10: res = "he"; break;
			case 0x11: res = "him"; break;
			case 0x12: res = "his"; break;
			case 0x13: res = "himself"; break;
			case 0x14: res = "someone"; break;
			case 0x15: res = "someone"; break;
			case 0x16: res = "someone's"; break;
			case 0x17: res = "himself"; break;

			/* Female (assume human if vague) */
			case 0x20: res = "she"; break;
			case 0x21: res = "her"; break;
			case 0x22: res = "her"; break;
			case 0x23: res = "herself"; break;
			case 0x24: res = "someone"; break;
			case 0x25: res = "someone"; break;
			case 0x26: res = "someone's"; break;
			case 0x27: res = "herself"; break;
		}

		/* Copy the result */
		my_strcpy(desc, res, max);
	}


	/* Handle visible monsters, "reflexive" request */
	else if ((mode & 0x02) && (mode & 0x01))
	{
		/* The monster is visible, so use its gender */
		if (r_ptr->flags1 & (RF1_FEMALE)) my_strcpy(desc, "herself", max);
		else if (r_ptr->flags1 & (RF1_MALE)) my_strcpy(desc, "himself", max);
		else my_strcpy(desc, "itself", max);
	}


	/* Handle all other visible monster requests */
	else
	{
		/* It could be a Unique */
		if (r_ptr->flags1 & (RF1_UNIQUE))
		{
			/* Start with the name (thus nominative and objective) */
			my_strcpy(desc, name, max);
		}

		/* It could be an indefinite monster */
		else if (mode & 0x08)
		{
			/* XXX Check plurality for "some" */

			/* Indefinite monsters need an indefinite article */
			my_strcpy(desc, is_a_vowel(name[0]) ? "an " : "a ", max);
			my_strcat(desc, name, max);
		}

		/* It could be a normal, definite, monster */
		else
		{
			/* Definite monsters need a definite article */
			my_strcpy(desc, "the ", max);
			my_strcat(desc, name, max);
		}

		/* Handle the Possessive as a special afterthought */
		if (mode & 0x02)
		{
			/* XXX Check for trailing "s" */

			/* Simply append "apostrophe" and "s" */
			my_strcat(desc, "'s", max);
		}

		/* Mention "offscreen" monsters XXX XXX */
		if (!panel_contains(m_ptr->fy, m_ptr->fx))
		{
			/* Append special notation */
			my_strcat(desc, " (offscreen)", max);
		}
	}
}


/*
 * Build a string describing a monster race, currently used for quests.
 *
 * Assumes a singular monster.  This may need to be run through the
 * plural_aux function in the quest.c file.  (Changes "wolf" to
 * wolves, etc.....)
 *
 * I am assuming that no monster name is more than 65 characters long,
 * so that "char desc[80];" is sufficiently large for any result, even
 * when the "offscreen" notation is added.
 *
 */
void monster_desc_race(char *desc, size_t max, int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	cptr name = (r_name + r_ptr->name);

	/* Write the name */
	my_strcpy(desc, name, max);
}


/*
 * Take note that the given monster just dropped some treasure
 *
 * Note that learning the "CHEST/GOOD"/"GREAT" flags gives information
 * about the treasure (even when the monster is killed for the first
 * time, such as uniques, and the treasure has not been examined yet).
 *
 * This "indirect" method is used to prevent the player from learning
 * exactly how much treasure a monster can drop from observing only
 * a single example of a drop.  This method actually observes how many
 * items are dropped, and remembers that information to be
 * described later by the monster recall code.
 */
void lore_treasure(int m_idx, int num_item)
{
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	/* Note the number of things dropped */
	if (num_item > l_ptr->drop_item) l_ptr->drop_item = num_item;

	/* Hack -- memorize the good/great flags */
	if (r_ptr->flags1 & (RF1_DROP_CHEST)) l_ptr->flags1 |= (RF1_DROP_CHEST);
	if (r_ptr->flags1 & (RF1_DROP_GOOD)) l_ptr->flags1 |= (RF1_DROP_GOOD);
	if (r_ptr->flags1 & (RF1_DROP_GREAT)) l_ptr->flags1 |= (RF1_DROP_GREAT);

	/* Update monster recall window */
	if (p_ptr->monster_race_idx == m_ptr->r_idx)
	{
		/* Window stuff */
		p_ptr->window |= (PW_MONSTER);
	}
}


/*
 *  Calculates a skill score for a monster
 */
int monster_skill(monster_type *m_ptr, int skill_type)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	int skill = 0;
	
    switch (skill_type)
    {
        case S_MEL:
            msg_debug("Can't determine the monster's Melee score.");
            break;
        case S_ARC:
            msg_debug("Can't determine the monster's Archery score.");
            break;
        case S_EVN:
            msg_debug("Can't determine the monster's Evasion score.");
            break;
        case S_STL:
            skill = r_ptr->stl;
            break;
        case S_PER:
            skill = r_ptr->per;
            break;
        case S_WIL:
            skill = r_ptr->wil;
            break;
        case S_SMT:
            msg_debug("Can't determine the monster's Smithing score.");
            break;
        case S_SNG:
            msg_debug("Can't determine the monster's Song score.");
            break;
            
        default:
            msg_debug("Asked for an invalid monster skill.");
            break;
    }
    
	// penalise stunning
	if (m_ptr->stunned) skill -= 2;
	
	return (skill);
}



/*
 *  Calculates a Stat score for a monster
 */
int monster_stat(monster_type *m_ptr, int stat_type)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	int stat = 0;
    
	int mhp = m_ptr->maxhp;
	int base = 20;
	
    switch (stat_type) {
        case A_STR:
            stat = (r_ptr->blow[0].dd * 2) + (r_ptr->hdice / 10) - 4;;
            break;
        case A_DEX:
            msg_debug("Can't determine the monster's Dex score.");
            break;
        case A_CON:
            if (mhp < base)
            {
                while (mhp < base)
                {
                    stat--;
                    base = (base * 10) / 12;
                }
            }
            else if (mhp >= base)
            {
                stat--;
                while (mhp >= base)
                {
                    stat++;
                    base = (base * 12) / 10;
                }
            }
            //msg_debug("%d => %d.", m_ptr->maxhp, stat); // Sil-y: this seems slightly erroneous for extreme values
            break;
        case A_GRA:
            msg_debug("Can't determine the monster's Gra score.");
            break;
            
        default:
            msg_debug("Asked for an invalid monster stat.");
            break;
    }
    	
	return (stat);
}


void listen(monster_type *m_ptr)
{
	byte a;
	char c;
	byte k;
	int base;
	
	int result;
	
	int y = m_ptr->fy;
	int x = m_ptr->fx;
	monster_race * r_ptr = &r_info[m_ptr->r_idx];

	int difficulty = flow_dist(FLOW_PLAYER_NOISE, y, x) - m_ptr->noise;

	// reset the monster noise
	m_ptr->noise = 0;
		
	// must have the listen skill
	if (!p_ptr->active_ability[S_PER][PER_LISTEN]) return;
	
	// must not be visible
	if (m_ptr->ml) return;

	// monster must be able to move
	if (r_ptr->flags1 & (RF1_NEVER_MOVE)) return;
	
	// use monster stealth
	difficulty += monster_skill(m_ptr, S_STL);

	// bonus for awake but unwary monsters (to simulate their lack of care)
	if ((m_ptr->alertness >= ALERTNESS_UNWARY) && (m_ptr->alertness < ALERTNESS_ALERT)) difficulty -= 3;
	
	// penalty for song of silence
	if (singing(SNG_SILENCE)) difficulty += ability_bonus(S_SNG, SNG_SILENCE);
	
	// make the check
	result = skill_check(PLAYER, p_ptr->skill_use[S_PER], difficulty, m_ptr);
	
	// give up if it is a failure
	if (result <= 0)
	{
		lite_spot(y,x);
		return;
	}
	
	// make the monster completely visible if a dramatic success
	if (result > 10)
	{
		m_ptr->ml = TRUE;
		lite_spot(y,x);
		return;
	}
	
	if (!(use_graphics && (arg_graphics == GRAPHICS_DAVID_GERVAIS)))
	{
		/* Base graphic '*' */
		base = 0x30;
		
		/* Basic listen color */
		k = TERM_SLATE;
				
		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k];
	}
	else
	{
		int add;
		
    	msg_print("Error: listening doesn't work with tiles.");
		
		// Sil-y: this might look very silly in graphical tiles, but then we don't support them at all
		/* base graphic */
		base = 0x00;
		add = 0;
		
		k = 0;
		
		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k] + add;
	}
	
	/* Display the visual effects */
	print_rel(c, a, y, x);
	move_cursor_relative(y, x);
	Term_fresh();
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
	monster_type *m_ptr = &mon_list[m_idx];

	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	int d;

	/* Current location */
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;

	/* Seen at all */
	bool flag = FALSE;

	/* Seen by vision */
	bool easy = FALSE;
    
    /* Known because immobile */
    bool immobile_seen = FALSE;

    // unmoving mindless monsters (i.e. molds) can be seen once encountered
    if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && (r_ptr->flags2 & (RF2_MINDLESS)) && m_ptr->encountered)
    {
        immobile_seen = TRUE;
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
		d = (dy > dx) ? (dy + (dx>>1)) : (dx + (dy>>1));

		/* Restrict distance */
		if (d > 255) d = 255;

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
	if (m_ptr->mflag & (MFLAG_MARK)) flag = TRUE;

	// debugging option for seeing all monsters
	if (cheat_monsters) flag = TRUE;

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
				flag = TRUE;

				/* Hack -- Memorize mental flags */
				if (r_ptr->flags2 & (RF2_SMART)) l_ptr->flags2 |= (RF2_SMART);
				if (r_ptr->flags2 & (RF2_MINDLESS)) l_ptr->flags2 |= (RF2_MINDLESS);
			}
		}
        
		/* Normal line of sight, and not blind */
		if (player_has_los_bold(fy, fx) && !p_ptr->blind)
		{
			bool do_invisible = FALSE;
			int difficulty = monster_skill(m_ptr, S_WIL) + (2 * distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx));

			/* Use "illumination" */
			if (player_can_see_bold(fy, fx))
			{
				/* Handle "invisible" monsters */
				if (r_ptr->flags2 & (RF2_INVISIBLE))
				{
					/* Take note */
					do_invisible = TRUE;

					/* See invisible makes things much easier */
                    difficulty -= 10 * p_ptr->see_inv;

					/* Keen senses */
					if (p_ptr->active_ability[S_PER][PER_KEEN_SENSES])
					{						
						// makes things a bit easier
						difficulty -= 5;
					}
					
					// Sil-x: calling this here seems to cause randseed issues on reloading games
					//        i.e. saving then loading will 'see' different monsters
					/* See invisible through perception skill */
					if (skill_check(PLAYER, p_ptr->skill_use[S_PER], difficulty, m_ptr) > 0)
					{
						/* Easy to see */
						easy = flag = TRUE;
					}
				}

				/* Handle "normal" monsters */
				else
				{
					/* Easy to see */
					easy = flag = TRUE;
				}
			}
			
			// handle keen senses ability			
			else if (seen_by_keen_senses(fy, fx))
			{
				/* Easy to see */
				easy = flag = TRUE;
			}

			/* Visible */
			if (flag)
			{
				/* Memorize flags */
				if (do_invisible) l_ptr->flags2 |= (RF2_INVISIBLE);
			}
		}
	}


	/* The monster is now visible */
	if (flag || immobile_seen)
	{
        // Untarget if this is an out-of-LOS stationary monster
        if (immobile_seen && !flag)
        {
            if (p_ptr->target_who == m_idx) target_set_monster(0);
            if (p_ptr->health_who == m_idx) health_track(0);
        }
        
		/* It was previously unseen */
		if (!m_ptr->ml)
		{
			/* Mark as visible */
			m_ptr->ml = TRUE;

			/* Draw the monster */
			lite_spot(fy, fx);

			/* Update health bar as needed */
			if (p_ptr->health_who == m_idx) p_ptr->redraw |= (PR_HEALTHBAR);

			/* Disturb on visibility change */
			disturb(0, 0);

			/* Window stuff */
			p_ptr->window |= PW_MONLIST;
			
			// identify see invisible items
			if ((r_ptr->flags2 & (RF2_INVISIBLE)) && (p_ptr->see_inv > 0))	ident_see_invisible(m_ptr);
		}
	}

	/* The monster is not visible */
	else
	{
		/* It was previously seen */
		if (m_ptr->ml)
		{
			/* Mark as not visible */
			m_ptr->ml = FALSE;

			/* Erase the monster */
			lite_spot(fy, fx);

			/* Update health bar as needed */
			if (p_ptr->health_who == m_idx) p_ptr->redraw |= (PR_HEALTHBAR);

			/* Disturb on visibility change */
			//disturb(0, 0);

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
			//disturb(1, 0);

		}
	}
	
	// Sil-x: calling this here seems to cause randseed issues on reloading games
	//        i.e. saving then loading will 'hear' different monsters
	listen(m_ptr);

	// Check ecounters with monsters (must be visible and in line of sight)
	if (m_ptr->ml && !m_ptr->encountered && player_has_los_bold(m_ptr->fy,m_ptr->fx) && (l_ptr->psights < MAX_SHORT)) 
	{
		int new_exp = adjusted_mon_exp(r_ptr, FALSE);
		
		// gain experience for encounter
		gain_exp(new_exp);	
		p_ptr->encounter_exp += new_exp;	
		
		// update stats
		m_ptr->encountered = TRUE;
		l_ptr->psights++;
		if (l_ptr->tsights < MAX_SHORT) l_ptr->tsights++;
		
		// If the player encounters a Unique for the first time, write a note.
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			char note2[120];
			char real_name[120];
			
			/* Get the monster's real name for the notes file */
			monster_desc_race(real_name, sizeof(real_name), m_ptr->r_idx);
			
			/* Write note */
			my_strcpy(note2, format("Encountered %s", real_name), sizeof (note2));
			
			do_cmd_note(note2, p_ptr->depth);
		}
				
		// if it was a wraith, possibly realise you are haunted
		if ((r_ptr->flags3 & (RF3_UNDEAD)) && !(r_ptr->flags2 & (RF2_TERRITORIAL)))
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
		monster_type *m_ptr = &mon_list[i];

		/* Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Update the monster */
		update_mon(i, full);
	}
}



/*
 * Make a monster carry an object
 */
s16b monster_carry(int m_idx, object_type *j_ptr)
{
	s16b o_idx;

	s16b this_o_idx, next_o_idx = 0;

	monster_type *m_ptr = &mon_list[m_idx];

	/* Scan objects already being held for combination */
	for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Check for combination */
		if (object_similar(o_ptr, j_ptr))
		{
			/* Combine the items */
			object_absorb(o_ptr, j_ptr);

			if (j_ptr->number == 0)
			{
				/* Result */
				return (this_o_idx);
			}
		}
	}


	/* Make an object */
	o_idx = o_pop();

	/* Success */
	if (o_idx)
	{
		object_type *o_ptr;

		/* Get new object */
		o_ptr = &o_list[o_idx];

		/* Copy object */
		object_copy(o_ptr, j_ptr);

		/* Forget mark */
		o_ptr->marked = FALSE;

		/* Forget location */
		o_ptr->iy = o_ptr->ix = 0;

		/* Link the object to the monster */
		o_ptr->held_m_idx = m_idx;

		/* Link the object to the pile */
		o_ptr->next_o_idx = m_ptr->hold_o_idx;

		/* Link the monster to the object */
		m_ptr->hold_o_idx = o_idx;
	}

	/* Result */
	return (o_idx);
}

/*
 * Check if the monster in the given location needs to fall down a chasm
 */
void m_fall_in_chasm(int fy, int fx)
{
    monster_type *m_ptr;
    monster_race *r_ptr;
    char m_name[80];
    
    int dice;
    int dam;
    
    // paranoia
    if (cave_m_idx[fy][fx] <= 0) return;

    m_ptr = &mon_list[cave_m_idx[fy][fx]];
    r_ptr = &r_info[m_ptr->r_idx];
    
    if ((cave_feat[fy][fx] == FEAT_CHASM) && !(r_ptr->flags2 & (RF2_FLYING)))
    {        
        // Get the monster name
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        // message for visible monsters
        if (m_ptr->ml)
        {
            // Dump a message
            if (m_ptr->morale < -200)   msg_format("%^s leaps into the abyss!", m_name);
            else                        msg_format("%^s topples into the abyss!", m_name);
        }
        
        // pause so that the monster will be displayed in the chasm before it disappears
        message_flush();

        // determine the falling damage
        if (p_ptr->depth == MORGOTH_DEPTH - 2)  dice = 3; // only fall one floor in this case
        else                                    dice = 6;
        
        // roll the damage dice
        dam = damroll(dice, 4);
        
        // update combat rolls if visible
        if (m_ptr->ml)
        {
            // Store information for the combat rolls window
            combat_roll_special_char = (&f_info[cave_feat[fy][fx]])->d_char;
            combat_roll_special_attr = (&f_info[cave_feat[fy][fx]])->d_attr;

            update_combat_rolls1b(NULL, m_ptr, TRUE);
            update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, FALSE);
        }
        
        // kill monsters which cannot survive the damage
        if (m_ptr->hp <= dam)
        {
            // kill the monster, gain experience etc
            monster_death(cave_m_idx[fy][fx]);

            // delete the monster
            delete_monster_idx(cave_m_idx[fy][fx]);
        }
        
        // otherwise the monster survives! (mainly relevant for uniques)
        else
        {
            // just delete the monster
			delete_monster(m_ptr->fy, m_ptr->fx);
        }
        
    }
}


/*
 * Print a message saying what is underfoot.
 */
void describe_floor_object(void)
{
    object_type *o_ptr;
    char o_name[80];
    
    // generate the object's name
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
    
    // skip 'nothings'
    if (!o_ptr->k_idx)
    {
        // do nothing
    }

    // skip notes
    else if (o_ptr->tval == TV_NOTE)
    {
        // do nothing
    }

    // skip fired/thrown items
    else if (o_ptr->pickup)
    {
        // do nothing
    }
    
    // arms and armour show weight
    else if (((wield_slot(o_ptr) >= INVEN_WIELD) && (wield_slot(o_ptr) <= INVEN_BOW)) ||
             ((wield_slot(o_ptr) >= INVEN_BODY)  && (wield_slot(o_ptr) <= INVEN_FEET)))
    {
        int wgt = o_ptr->weight * o_ptr->number;
        msg_format("You see %s %d.%1d lb.", o_name, wgt / 10, wgt % 10);
        
        /* Disturb */
        disturb(0,0);
    }

    // other things just show description
    else
    {
        msg_format("You see %s.", o_name);

        /* Disturb */
        disturb(0,0);
    }
    
    // special explanation the first time you step over the crown
    if ((o_ptr->name1 == ART_MORGOTH_3) && !(p_ptr->crown_hint))
    {
        msg_print("To attempt to prise a Silmaril from the crown, use the 'destroy' command (which is 'k' by default).");
        p_ptr->crown_hint = TRUE;
    }
}


/*
 * Swap the players/monsters (if any) at two locations XXX XXX XXX
 *
 * Note that this assumes the monster at y1-x1 is actively moving to y2-x2
 */
void monster_swap(int y1, int x1, int y2, int x2)
{
	int m1 = cave_m_idx[y1][x1];
	int m2 = cave_m_idx[y2][x2];
	
    int y, x;
    
	monster_type *m_ptr = NULL; // default to soother compiler warnings
    monster_race *r_ptr = NULL; // default to soother compiler warnings
    monster_lore *l_ptr = NULL; // default to soother compiler warnings
	
	char m_name[80];
	
	bool monster1 = FALSE;

	/* Monster 1 */
	if (m1 > 0)
	{
		monster1 = TRUE;
		m_ptr = &mon_list[m1];
		
		monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        // (skip_next_turn is there to stop you getting opportunist attacks afer knocking someone back)
        if (m_ptr->ml && !m_ptr->skip_next_turn && !p_ptr->truce && !p_ptr->confused && !p_ptr->afraid && !p_ptr->entranced && (p_ptr->stun <= 100))
        {
            if (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT))
            {
                if (p_ptr->active_ability[S_MEL][MEL_ZONE_OF_CONTROL])
                {
                    if ((distance(y1, x1, p_ptr->py, p_ptr->px) == 1) && (distance(y2, x2, p_ptr->py, p_ptr->px) == 1))
                    {
                        msg_format("%^s moves through your zone of control.", m_name);
                        py_attack_aux(y1,x1,ATT_ZONE_OF_CONTROL);
                    }
                }
                if (p_ptr->active_ability[S_STL][STL_OPPORTUNIST])
                {
                    if ((distance(y1, x1, p_ptr->py, p_ptr->px) == 1) && (distance(y2, x2, p_ptr->py, p_ptr->px) > 1))
                    {
                        msg_format("%^s moves away from you.", m_name);
                        py_attack_aux(y1,x1,ATT_OPPORTUNIST);
                    }
                }
            }
        }
		if (m_ptr->hp <= 0) return;
        
		// abort the monster swap if the monster has been moved by the free attack
		if (cave_m_idx[y1][x1] != m1) return;
		
		/* Move monster */
		m_ptr->fy = y2;
		m_ptr->fx = x2;

		// makes noise when moving
		if (m_ptr->noise == 0) m_ptr->noise = 5;

		/* Update monster */
		(void)update_mon(m1, TRUE);
		
	}

	/* Player 1 */
	else if (m1 < 0)
	{
        // deal with monsters with Opportunist or Zone of Control
        for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
        {
            for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
            {
                if (cave_m_idx[y][x] > 0)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    r_ptr = &r_info[m_ptr->r_idx];
                    l_ptr = &l_list[m_ptr->r_idx];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                                        
                    if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused && (m_ptr->stance != STANCE_FLEEING) &&
                        !m_ptr->skip_next_turn && !m_ptr->skip_this_turn)
                    {
                        // Opportunist
                        if ((r_ptr->flags2 & (RF2_OPPORTUNIST)) && (distance(m_ptr->fy, m_ptr->fx, y2, x2) > 1))
                        {
                            msg_format("%^s attacks you as you step away.", m_name);
                            make_attack_normal(m_ptr);
                            
                            // remember that the monster can do this
                            if (m_ptr->ml)  l_ptr->flags2 |= (RF2_OPPORTUNIST);
                        }
                        
                        // Zone of Control
                        if ((r_ptr->flags2 & (RF2_ZONE_OF_CONTROL)) && (distance(m_ptr->fy, m_ptr->fx, y2, x2) == 1))
                        {
                            msg_format("You move through %s's zone of control.", m_name);
                            make_attack_normal(m_ptr);
                            
                            // remember that the monster can do this
                            if (m_ptr->ml)  l_ptr->flags2 |= (RF2_ZONE_OF_CONTROL);
                        }
                     }
                 }
            }
        }
		if (p_ptr->chp <= 0) return;
        
		/* Move player */
		p_ptr->py = y2;
		p_ptr->px = x2;

		/* Update the panel */
		p_ptr->update |= (PU_PANEL);

		/* Update the visuals (and monster distances) */
		p_ptr->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

		/* Window stuff */
		p_ptr->window |= (PW_OVERHEAD);
	}

	/* Monster 2 */
	if (m2 > 0)
	{
		m_ptr = &mon_list[m2];

		/* Move monster */
		m_ptr->fy = y1;
		m_ptr->fx = x1;

		// makes noise when moving
		if (m_ptr->noise == 0) m_ptr->noise = 5;

		/* Update monster */
		(void)update_mon(m2, TRUE);

        // reset its previous movement to stop it charging etc.
        m_ptr->previous_action[0] = ACTION_MISC;
	}

	/* Player 2 */
	else if (m2 < 0)
	{
		/* Move player */
		p_ptr->py = y1;
		p_ptr->px = x1;

		/* Update the panel */
		p_ptr->update |= (PU_PANEL);

		/* Update the visuals (and monster distances) */
		p_ptr->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

		/* Window stuff */
		p_ptr->window |= (PW_OVERHEAD);
	}

	/* Update grids */
	cave_m_idx[y1][x1] = m2;
	cave_m_idx[y2][x2] = m1;

	/* Redraw */
	lite_spot(y1, x1);
	lite_spot(y2, x2);

	// deal with set polearm attacks
	if (p_ptr->active_ability[S_MEL][MEL_POLEARMS] && monster1 && m_ptr->ml)
	{
		object_type *o_ptr = &inventory[INVEN_WIELD];
		u32b f1, f2, f3;
		
		object_flags(o_ptr, &f1, &f2, &f3);
        
		if (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT))
        {
            if ((distance(y1, x1, p_ptr->py, p_ptr->px) > 1) && (distance(y2, x2, p_ptr->py, p_ptr->px) == 1) &&
                 !p_ptr->truce && !p_ptr->confused && !p_ptr->afraid && (f3 & (TR3_POLEARM)) && p_ptr->focused)
            {
                char o_name[80];
                
                /* Get the basic name of the object */
                object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);
                
                msg_format("%^s comes into reach of your %s.", m_name, o_name);
                py_attack_aux(y2,x2,ATT_POLEARM);
            }
        }
            
	}

    // deal with falling down chasms
    if (m1 > 0) m_fall_in_chasm(y2,x2);
    if (m2 > 0) m_fall_in_chasm(y1,x1);

    // describe object you are standing on if any
    if ((m1 < 0) || (m2 < 0))
    {
        describe_floor_object();
    }
}


/*
 * Place the player in the dungeon XXX XXX
 */
s16b player_place(int y, int x)
{

	/* Paranoia XXX XXX */
	if (cave_m_idx[y][x] != 0) return (0);

	/* Save player location */
	p_ptr->py = y;
	p_ptr->px = x;

	/* Mark cave grid */
	cave_m_idx[y][x] = -1;

	/* Success */
	return (-1);
}


/*
 * Place a copy of a monster in the dungeon XXX XXX
 */
s16b monster_place(int y, int x, monster_type *n_ptr)
{
	s16b m_idx;

	monster_type *m_ptr;
	monster_race *r_ptr;

	/* Paranoia XXX XXX */
	if (cave_m_idx[y][x] != 0) return (0);

	/* Get a new record */
	m_idx = mon_pop();

	/* Oops */
	if (m_idx)
	{
		/* Make a new monster */
		cave_m_idx[y][x] = m_idx;

		/* Get the new monster */
		m_ptr = &mon_list[m_idx];

		/* Copy the monster XXX */
		COPY(m_ptr, n_ptr, monster_type);

		/* Location */
		m_ptr->fy = y;
		m_ptr->fx = x;

		/* Update the monster */
		update_mon(m_idx, TRUE);

		/* Get the new race */
		r_ptr = &r_info[m_ptr->r_idx];

		/* Hack -- Notice new multi-hued monsters */
		if (r_ptr->flags1 & (RF1_ATTR_MULTI)) shimmer_monsters = TRUE;

		/* Hack -- Count the number of "reproducers" */
		if (r_ptr->flags2 & (RF2_MULTIPLY)) num_repro++;

		/* Count racial occurances */
		r_ptr->cur_num++;
	}

	/* Result */
	return (m_idx);
}


/*calculate the monster_speed of a monster at a given location*/
void calc_monster_speed(int y, int x)
{
	int speed;

	/*point to the monster at the given location & the monster race*/
	monster_type *m_ptr;
	monster_race *r_ptr;

	/* Paranoia XXX XXX */
	if (cave_m_idx[y][x] <= 0) return;

	m_ptr = &mon_list[cave_m_idx[y][x]];
	r_ptr = &r_info[m_ptr->r_idx];

	/* Get the monster base speed */
	speed = r_ptr->speed;

	/*factor in the hasting and slowing counters*/
	if (m_ptr->hasted) speed += 1;
	if (m_ptr->slowed) speed -= 1;
	
	if (speed < 1) speed = 1;

	/*set the speed and return*/
	m_ptr->mspeed = speed;

	return;
}

void set_monster_haste(s16b m_idx, s16b counter, bool message)
{
	/*get the monster at the given location*/
	monster_type *m_ptr = &mon_list[m_idx];

	bool recalc = FALSE;

	char m_name[80];

	/* Get monster name*/
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/*see if we need to recalculate speed*/
	if (m_ptr->hasted)
	{
		/*monster is no longer hasted and speed needs to be recalculated*/
		if (counter == 0)
		{
			recalc = TRUE;

			/*give a message*/
			if (message) msg_format("%^s slows down.", m_name);
		}
	}
	else
	{
		/*monster is now hasted and speed needs to be recalculated*/
		if (counter > 0)
		{
			recalc = TRUE;

			/*give a message*/
			if (message) msg_format("%^s starts moving faster.", m_name);
		}
	}

	/*update the counter*/
	m_ptr->hasted = counter;

	/*re-calculate speed if necessary*/
	if (recalc) calc_monster_speed(m_ptr->fy, m_ptr->fx);

	return;
}

void set_monster_slow(s16b m_idx, s16b counter, bool message)
{
	/*get the monster at the given location*/
	monster_type *m_ptr = &mon_list[m_idx];

	bool recalc = FALSE;

	char m_name[80];

	/* Get monster name*/
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/*see if we need to recalculate speed*/
	if (m_ptr->slowed)
	{
		/*monster is no longer slowed and speed needs to be recalculated*/
		if (counter == 0)
		{
			recalc = TRUE;

			/*give a message*/
			if (message) msg_format("%^s speeds up.", m_name);
		}
	}
	else
	{
		/*monster is now slowed and speed needs to be recalculated*/
		if (counter > 0)
		{
			recalc = TRUE;

			/*give a message*/
			if (message) msg_format("%^s starts moving slower.", m_name);
		}
	}

	/*update the counter*/
	m_ptr->slowed = counter;

	/*re-calculate speed if necessary*/
	if (recalc) calc_monster_speed(m_ptr->fy, m_ptr->fx);

	return;
}


/*
 * Set Hallucinatory monster race
 */
int random_r_idx(void)
{
	monster_race *r_ptr;
	int race_idx;
	
	while (1)
	{
		race_idx = rand_int(z_info->r_max);
		r_ptr = &r_info[race_idx];
		if ((r_ptr->rarity != 0) && one_in_(r_ptr->rarity)) return(race_idx);
	}
}


/*
 * Attempt to place a monster of the given race at the given location.
 *
 * This routine refuses to place out-of-depth "FORCE_DEPTH" monsters.
 *
 * This is the only function which may place a monster in the dungeon,
 * except for the savefile loading code.
 */
bool place_monster_one(int y, int x, int r_idx, bool slp, bool ignore_depth, monster_type *m_ptr)
{

	monster_race *r_ptr;

	monster_type *n_ptr;
	monster_type monster_type_body;

	cptr name;

	/* Paranoia */
	if (!in_bounds(y, x)) return (FALSE);

	/* Require empty space */
	if (!cave_empty_bold(y, x)) return (FALSE);

	/* Hack -- no creation on glyph of warding */
	if (cave_feat[y][x] == FEAT_GLYPH) return (FALSE);

	/* Handle failure of the "get_mon_num()" function */
	if (!r_idx) return (FALSE);

	if ((feeling >= LEV_THEME_HEAD) && (character_dungeon == TRUE)) return (FALSE);

	/* Race */
	r_ptr = &r_info[r_idx];

	/* The monster must be able to exist in this grid */
	if (!cave_exist_mon(r_ptr, y, x, FALSE, FALSE)) return (FALSE);

	/* Paranoia */
	if (!r_ptr->name) return (FALSE);

	/*limit the population*/
	if (r_ptr->cur_num >= r_ptr->max_num)
	{
		return (FALSE);
	}

	/* Name */
	name = (r_name + r_ptr->name);

	/* Force depth monsters may NOT normally be created out of depth */
	if ((r_ptr->flags1 & (RF1_FORCE_DEPTH)) && (p_ptr->depth < r_ptr->level) && !ignore_depth)
	{
		/* Cannot create */
		return (FALSE);
	}
    
	/* Special generation monsters may NOT normally be created */
	if ((r_ptr->flags1 & (RF1_SPECIAL_GEN)) && !ignore_depth)
	{
		/* Cannot create */
		return (FALSE);
	}

	/* Get local monster */
	n_ptr = &monster_type_body;

	/* Clean out the monster */
	(void)WIPE(n_ptr, monster_type);

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
		
		if (r_ptr->sleep == 0)  amount = 0;
		else					amount = dieroll(r_ptr->sleep);
		
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
				amount = damroll(1,3);
			}
			// and dangerous monsters out of vaults (which are assumed to be in direct pursuit)
			else if ((r_ptr->level > p_ptr->depth + 2) && !(cave_info[y][x] & (CAVE_ICKY)) && (amount > 0))
			{ 
				amount = damroll(1,3);
			}
		}
		
		n_ptr->alertness = ALERTNESS_ALERT - amount;
	}
	else
	{
		if (p_ptr->depth > 0)	n_ptr->alertness = ALERTNESS_ALERT - 1;
		else					n_ptr->alertness = ALERTNESS_ALERT;
	}

	/* Assign average hitpoints */
	if (r_ptr->flags1 & (RF1_UNIQUE))
	{
		n_ptr->maxhp = r_ptr->hdice * (1 + r_ptr->hside) / 2;
	}
	/*assign hitpoints using dice rolls*/
	else
	{
		n_ptr->maxhp = damroll(r_ptr->hdice, r_ptr->hside);
	}

	// marked previously encountered uniques as such
	if (r_ptr->flags1 & (RF1_UNIQUE))
	{
		monster_lore *l_ptr = &l_list[n_ptr->r_idx];
		if (l_ptr->psights > 0)
		n_ptr->encountered = TRUE;
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

	/* Place the monster in the dungeon */
	if (!monster_place(y, x, n_ptr)) return (FALSE);

	// reacquire monster pointer
	n_ptr = &mon_list[cave_m_idx[y][x]];
	
	// give the monster a place to wander towards
	new_wandering_destination(n_ptr, m_ptr);
	
	/*calculate the monster_speed*/
	calc_monster_speed(y, x);

	/* Powerful monster */
	if (r_ptr->level > p_ptr->depth + 2)
	{
		/* Message for cheaters */
		if (cheat_hear) msg_format("(+%d: %s).", r_ptr->level - p_ptr->depth, name);

		/* Boost rating by delta-depth */
		rating += (r_ptr->level - p_ptr->depth);
	}

	/* Note the monster */
	else if (r_ptr->flags1 & (RF1_UNIQUE))
	{
		/* Unique monsters induce message */
		if (cheat_hear) msg_format("Unique (%s).", name);
	}
	
	// Monsters that don't pursue you drop their treasure upon being created
	if (r_ptr->flags2 & (RF2_TERRITORIAL))
	{
		drop_loot(n_ptr);
	}

	/* Success */
	return (TRUE);
}

/*
 * Maximum size of a group of monsters
 */
#define GROUP_MAX	18

/*
 * Attempt to place a group of monsters around the given location.
 *
 * Hack -- A group of monsters counts as a single individual for the
 * level rating.
 */
static bool place_monster_group(int y, int x, int r_idx, bool slp, monster_type *m_ptr, s16b group_size)
{
	int old, n, i;
	int start;

	int hack_n = 0;

 	byte hack_y[GROUP_MAX];
 	byte hack_x[GROUP_MAX];

 	/* Maximum size */
	if (group_size > GROUP_MAX) group_size = GROUP_MAX;

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
			if (place_monster_one(my, mx, r_idx, slp, FALSE, m_ptr))
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

	/* Return true if it places at least one monster (even if fewer than desired) */
	if (hack_n > 1)
		return (TRUE);
	else
		return (FALSE);
}

/*
 * Hack -- help pick an escort type
 */
static int place_monster_idx = 0;

/*
 * Hack -- help pick an escort type
 */
static bool place_monster_okay(int r_idx)
{
	monster_race *r_ptr = &r_info[place_monster_idx];

	monster_race *z_ptr = &r_info[r_idx];

	/* Require similar "race" */
	if (z_ptr->d_char != r_ptr->d_char) return (FALSE);

	/* Skip more advanced monsters */
	if (z_ptr->level > r_ptr->level) return (FALSE);

	/* Skip unique monsters */
	if (z_ptr->flags1 & (RF1_UNIQUE)) return (FALSE);

	/* Paranoia -- Skip identical monsters */
	if (place_monster_idx == r_idx) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Attempt to place a unique's unique ally at a given location
 */
static void place_monster_unique_friend(int y, int x, int leader_idx, bool slp, monster_type *m_ptr)
{
	int i, r;
	
	/* Random direction */
	int start;
	
	monster_race *leader_r_ptr = &r_info[leader_idx];
	
	/* Find the unique friend */
	for (r = 1; r < z_info->r_max; r++)
	{
		monster_race *r_ptr = &r_info[r];

		if ((r_ptr->d_char == leader_r_ptr->d_char) && (r_ptr->flags1 & (RF1_UNIQUE_FRIEND)))
		{
			/* Random direction */
			start = rand_int(8);
			
			/* Check each direction, up to escort_size */
			for (i = start; i < 8 + start; i++)
			{
				int my = y + ddy_ddd[i % 8];
				int mx = x + ddx_ddd[i % 8];
				
				if (!place_monster_one(my, mx, r, slp, TRUE, m_ptr))
				{
					//msg_format("Failed to place %d.", r);
					continue;
				}	
			}
		}
	}
		
}



/*
 * Attempt to place an escort of monsters around the given location
 */
static void place_monster_escort(int y, int x, int leader_idx, bool slp, monster_type *m_ptr)
{
	int escort_size, escort_idx;
	int n, i;

	/* Random direction */
	int start;

	monster_race *r_ptr = &r_info[leader_idx];

	int level = r_ptr->level;

	int hack_n = 0;

	byte hack_y[GROUP_MAX];
	byte hack_x[GROUP_MAX];

	int escort_idxs[GROUP_MAX];
	
	int extras = 0;

	/* Save previous monster restriction value. */
	bool (*get_mon_num_hook_temp)(int r_idx) = get_mon_num_hook;

	/* Calculate the number of escorts we want. */
	if (r_ptr->flags1 & (RF1_ESCORTS)) escort_size = rand_range(8, 16);
	else escort_size = rand_range(4, 7);

	/* Can never have more escorts than maximum group size */
	if (escort_size > GROUP_MAX) escort_size = GROUP_MAX;

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
			escort_idxs[i] = escort_idxs[i-1];
		}
		else
		{
			escort_idxs[i] = get_mon_num(level, TRUE, FALSE, FALSE);
			
			// skip this creature if get_mon_num failed (paranoia)
			if (escort_idxs[i] == 0) continue; 
			
			if (r_info[escort_idxs[i]].flags1 & (RF1_FRIENDS))		extras = rand_range(2,3);
			else if (r_info[escort_idxs[i]].flags1 & (RF1_FRIEND))	extras = rand_range(1,2);
			else													extras = 0;
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

			if (!place_monster_one(my, mx, escort_idx, slp, FALSE, m_ptr))
			{
				//msg_format("Failed to place a %d ().", escort_idx);
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
	(void)get_mon_num(monster_level, FALSE, FALSE, FALSE);
}


/*
 * Attempt to place a monster of the given race at the given location
 *
 * Note that certain monsters are now marked as requiring "friends".
 * These monsters, if successfully placed, and if the "grp" parameter
 * is TRUE, will be surrounded by a "group" of identical monsters.
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
	monster_race *r_ptr = &r_info[r_idx];
	monster_type *m_ptr;
	
	s16b friends_amount;
	s16b friend_amount;
		
	// relative depth  |  number in group  (FRIENDS)
	//             -2  |    2
	//             -1  |  2 / 3
	//              0  |    3
	//             +1  |  3 / 4
	//             +2  |    4
	
    friends_amount = (rand_range(6,7) + (monster_level - r_ptr->level)) / 2;
	if (friends_amount < 2) friends_amount = 2;
	if (friends_amount > 4) friends_amount = 4;

	// relative depth  |  chance of having a companion  (FRIEND)
	//             -2  |    0%
	//             -1  |   25%
	//              0  |   50%
	//             +1  |   75%
	//             +2  |  100%

	friend_amount = 1;
	if (dieroll(4) <= monster_level - r_ptr->level + 2) friend_amount++;

	/* Place one monster, or fail */
	if (!place_monster_one(y, x, r_idx, slp, FALSE, NULL)) return (FALSE);
	
	if (cave_m_idx[y][x] > 0)
	{
		m_ptr = &mon_list[cave_m_idx[y][x]];
	}
	else
	{
		m_ptr = NULL;
	}
	
	/* Require the "group" flag */
	if (!grp) return (TRUE);

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
	return (TRUE);
}


/*
 * Hack -- attempt to place a monster at the given location
 *
 * Attempt to find a monster appropriate to the "monster_level"
 */
bool place_monster(int y, int x, bool slp, bool grp, bool vault)
{
	int r_idx;

	/* Pick a monster */   // Hack - uses the slp flag to determine if non-smart monsters are allowed
	r_idx = get_mon_num(monster_level, FALSE, slp, vault);

	/* Handle failure */
	if (!r_idx) return (FALSE);

	/* Attempt to place the monster */
	if (place_monster_aux(y, x, r_idx, slp, grp)) return (TRUE);

	/* Oops */
	return (FALSE);
}




/*
 * Attempt to allocate a random monster (or group) in the dungeon.
 *
 * It can be forced to be on the stairs and/or forced to be out of sight of the player
 *
 * Returns TRUE if the player sees it happen
 */
bool alloc_monster(bool on_stairs, bool force_undead)
{
	int y, x;
	int sy, sx;
	int	attempts_left = 1000;
	int tries = 0;
	int original_monster_level = monster_level;
	char dir[5];
	char m_name[80];
	char who[80];
	char message[240];
	bool displaced = FALSE;
	bool give_up = FALSE;
	bool placed = FALSE;
	

	// Force some monsters to be generated on the stairs
	if (on_stairs)
	{
		// no monsters come through the stairs on tutorial/challenge levels
		if (p_ptr->game_type != 0)	return (FALSE);
	
		// get a stair location
		if (!random_stair_location(&sy, &sx))	return (FALSE);
				
		// default the new location to this location
		y = sy;
		x = sx;
				
		// if there is something on the stairs, try adjacent squares
		if (cave_m_idx[sy][sx] != 0)
		{
			int d, y1, x1, start;
			bool moveable = TRUE;
			
			// if the monster on the squares cannot move, then simply give up: the stairs are blocked
			if (cave_m_idx[sy][sx] > 0)
			{
				monster_type *n_ptr = &mon_list[cave_m_idx[sy][sx]];
				monster_race *nr_ptr = &r_info[n_ptr->r_idx];
				
				if ((nr_ptr->flags1 & (RF1_NEVER_MOVE)) || (nr_ptr->flags1 & (RF1_HIDDEN_MOVE))) moveable = FALSE;
			}
			
			if (moveable)
			{
				// we will look through the eligible squares and choose an empty one randomly
				start = rand_int(8);
				
				for (d = start; d < 8 + start; d++)
				{
					y1 = sy + ddy_ddd[d % 8];
					x1 = sx + ddx_ddd[d % 8];
					
					/* Check Bounds */
					if (!in_bounds(y1, x1)) continue;
					
					/* Check Empty Square */
					if (!cave_empty_bold(y1, x1)) continue;
					
					if (cave_m_idx[y1][x1] == 0)
					{
						y = y1;
						x = x1;
						displaced = TRUE;
						break;
					}
				}			
			}
			
			if (!displaced) give_up = TRUE;
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
			// Try hard to put a monster on the stairs
			while (!placed && (tries < 50))
			{
				// modify the monster generation level based on the stair type
				monster_level = p_ptr->depth;
				switch (cave_feat[sy][sx])
				{
					case FEAT_LESS_SHAFT:
					{
						monster_level -= 2;
						sprintf(dir,"down");
						break;
					}
					case FEAT_LESS:
					{
						monster_level -= 1;
						sprintf(dir,"down");
						break;
					}
					case FEAT_MORE:
					{
						monster_level += 1;
						sprintf(dir,"up");
						break;
					}
					case FEAT_MORE_SHAFT:
					{
						monster_level += 2;
						sprintf(dir,"up");
						break;
					}
				}
				// correct deviant monster levels
				if (monster_level < 1)	monster_level = 1;	

				// sometimes only wraiths are allowed
				if (force_undead)
				{
					place_monster_by_flag(sy, sx, 3, RF3_UNDEAD, TRUE, MAX(monster_level + 3, 13));
					placed = TRUE;
				}
				
				// but usually allow most monsters
				else
				{
					placed = place_monster(sy, sx, FALSE, TRUE, FALSE);
				}
				
				tries++;
			}
		}
		
		// reset the monster level to the original value
		monster_level = original_monster_level;
		
		
		// print messages etc
		if (placed)
		{
			monster_type *m_ptr = &mon_list[cave_m_idx[sy][sx]];
			monster_race *r_ptr = &r_info[m_ptr->r_idx];
						
			// Display a message if seen
			if (m_ptr->ml)
			{				
				monster_desc(m_name, sizeof(m_name), m_ptr, 0x88);
				
				if (r_ptr->flags1 & (RF1_FRIEND | RF1_FRIENDS | RF1_ESCORT | RF1_ESCORTS))
				{
					my_strcpy(message, format("A group of enemies come %s the stair", dir), 240);
				}
				else
				{
					my_strcpy(message, format("%^s comes %s the stair", m_name, dir), 240);
				}
				
				if (displaced)
				{
					if ((p_ptr->py == y) && (p_ptr->px == x))
					{
						my_strcpy(who, "you", 80);
					}
					else
					{
						monster_desc(who, sizeof(who), &mon_list[cave_m_idx[y][x]], 0x88);
					}
					
					msg_format("%s, forcing %s out of the way!", message, who);
				}
				else
				{
					msg_format("%s!", message);
				}
			}
			
			if (m_ptr->ml)	return (TRUE);
			else			return (FALSE);
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
			if (cave_naked_bold(y, x) && !los(p_ptr->py,p_ptr->px,y,x)) break;
		}
		
		if (!attempts_left)
		{
			if (cheat_xtra || cheat_hear)
			{
				msg_print("Warning! Could not allocate a new monster.");
			}
			
			return (FALSE);
		}
		
		/* Attempt to place the monster, allow groups */
		if (place_monster(y, x, TRUE, TRUE, FALSE)) 
		{
			if ((cave_m_idx[y][x] > 0) && (&mon_list[cave_m_idx[y][x]])->ml)	return (TRUE);
			else																return (FALSE);
		}
	}

	/* Nope */
	return (FALSE);
}




/*
 * Hack -- the "type" of the current "summon specific"
 */
static int summon_specific_type = 0;


/*
 * Hack -- help decide if a monster race is "okay" to summon
 */
static bool summon_specific_okay(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	bool okay = FALSE;

	/* Hack -- no specific type specified */
	if (!summon_specific_type) return (TRUE);


	/* Check our requirements */
	switch (summon_specific_type)
	{

		case SUMMON_ANT:
		{
			okay = FALSE;
 			break;
 		}


		case SUMMON_SPIDER:
		{
			okay = ((r_ptr->d_char == 'M') &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_HOUND:
		{
			okay = ((r_ptr->d_char == 'C') &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_BIRD_BAT:
		{
			okay = ((r_ptr->d_char == 'b') &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_AINU:
		{
			okay = FALSE;
			break;
		}

		case SUMMON_RAUKO:
		{
			okay = ((r_ptr->flags3 & (RF3_RAUKO)) &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_UNDEAD:
		{
			okay = ((r_ptr->flags3 & (RF3_UNDEAD)) &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_DRAGON:
		{
			okay = ((r_ptr->flags3 & (RF3_DRAGON)) &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_HI_DEMON:
		{
			okay = ((r_ptr->flags3 & (RF3_RAUKO)) &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_HI_UNDEAD:
		{
			okay = ((r_ptr->flags3 & (RF3_UNDEAD)) &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_HI_DRAGON:
		{
			okay = (r_ptr->d_char == 'D');
			break;
		}

		case SUMMON_WRAITH:
		{
			okay = ((r_ptr->d_char == 'W') &&
			        (r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_UNIQUE:
		{
			if ((r_ptr->flags1 & (RF1_UNIQUE)) != 0) okay = TRUE;
			break;
		}

		case SUMMON_HI_UNIQUE:
		{
			if (((r_ptr->flags1 & (RF1_UNIQUE)) != 0) &&
				(r_ptr->level > (MORGOTH_DEPTH / 2))) okay = TRUE;
			break;
		}

		case SUMMON_KIN:
		{
			okay = ((r_ptr->d_char == summon_kin_type) &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_ANIMAL:
		{
			okay = FALSE;
			break;
		}

		case SUMMON_BERTBILLTOM:
		{
			okay = FALSE;
			break;
		}


		case SUMMON_THIEF:
		{
			okay = FALSE;
			break;
		}

		default:
		{
			break;
		}

	}

	/* Result */
	return (okay);
}


/*
 * Place a monster (of the specified "type") near the given
 * location.  Return TRUE if a monster was actually summoned.
 *
 * We will attempt to place the monster up to 20 times before giving up.
 *
 * Note: SUMMON_UNIQUE and SUMMON_WRAITH (XXX) will summon Uniques
 * Note: SUMMON_HI_UNDEAD and SUMMON_HI_DRAGON may summon Uniques
 * Note: None of the other summon codes will ever summon Uniques.
 *
 * We usually do not summon monsters greater than the given depth.  -LM-
 *
 * Note that we use the new "monster allocation table" creation code
 * to restrict the "get_mon_num()" function to the set of "legal"
 * monsters, making this function much faster and more reliable.
 *
 * Note that this function may not succeed, though this is very rare.
 */
bool summon_specific(int y1, int x1, int lev, int type)
{
	int i, x, y, r_idx;

	/* Look for a location */
	for (i = 0; i < 20; ++i)
	{
		/* Pick a distance */
		int d = (i / 15) + 1;

		/* Pick a location */
		scatter(&y, &x, y1, x1, d, 0);

		/* Require "empty" floor grid */
		if (!cave_empty_bold(y, x)) continue;

		/* Hack -- no summon on glyph of warding */
		if (cave_feat[y][x] == FEAT_GLYPH) continue;

		/* Okay */
		break;
	}

	/* Failure */
	if (i == 20) return (FALSE);

	/* Save the "summon" type */
	summon_specific_type = type;

	/* Require "okay" monsters */
	get_mon_num_hook = summon_specific_okay;

	/* Prepare allocation table */
	get_mon_num_prep();

	/* Pick a monster, using the given level */
	r_idx = get_mon_num(lev, FALSE, TRUE, FALSE);

	/* Remove restriction */
	get_mon_num_hook = NULL;

	/* Prepare allocation table */
	get_mon_num_prep();

	/* Handle failure */
	if (!r_idx) return (FALSE);

	/* Attempt to place the monster (awake, allow groups) */
	if (!place_monster_aux(y, x, r_idx, FALSE, TRUE)) return (FALSE);

	/* Success */
	return (TRUE);
}



/*
 * Let the given monster attempt to reproduce.
 *
 * Note that "reproduction" REQUIRES empty space.
 */
bool multiply_monster(int m_idx)
{
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

 	int i, y, x;

 	bool result = FALSE;

	u16b grid[8];
	int grids = 0;

	/* Scan the adjacent floor grids */
	for (i = 0; i < 8; i++)
	{
		y = m_ptr->fy + ddy_ddd[i];
		x = m_ptr->fx + ddx_ddd[i];

		/* Must be fully in bounds */
		if (!in_bounds_fully(y, x)) continue;

		/* This grid is OK for this monster (should monsters be able to dig?) */
		if (cave_exist_mon(r_ptr, y, x, FALSE, FALSE))
		{
			/* Save this grid */
			grid[grids++] = GRID(y, x);
		}
	}

	/* No grids available */
	if (!grids) return (FALSE);

	/* Pick a grid at random */
	i = rand_int(grids);

	/* Get the coordinates */
	y = GRID_Y(grid[i]);
	x = GRID_X(grid[i]);

	/* Create a new monster (awake, no groups) */
	result = place_monster_aux(y, x, m_ptr->r_idx, FALSE, FALSE);

 	/* Result */
 	return (result);
}


/*
 * Dump a message describing a monster's reaction to damage.
 *
 * Historically, this function gave a description (visual or auditory) of
 * a monster's reaction in order to give you an idea of their health level.
 *
 * Now it only gives a message if the monster is unseen, and the primary
 * purpose is to show that there is indeed a monster in the dark corridor getting hurt.
 *
 * Note that while the monsters 'cry out', it doesn't wake any monsters or anything,
 * as the idea is that it makes no more noise than regular melee combat. It is just that
 * in melee combat, we wouldn't want to spam up the screen with messages about noises.
 */
void message_pain(int m_idx, int dam)
{
	long oldhp, newhp, tmp;
	int percentage;

	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	char m_name[80];
	
	// Ignore the monster if it is visible
	if (m_ptr->ml) return;
	
	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Note -- subtle fix -CFT */
	newhp = (long)(m_ptr->hp);
	oldhp = newhp + (long)(dam);
	tmp = (newhp * 100L) / oldhp;
	percentage = (int)(tmp);


	/* Wolves */
	if (strchr("C", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear a snarl.");
		else if (percentage > 33)
			msg_print("You hear a yelp.");
		else
			msg_print("You hear a feeble yelp.");
	}

	/* Serpents, Dragons, Centipedes */
	else if (strchr("sScdD", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear a hiss.");
		else if (percentage > 33)
			msg_print("You hear a furious hissing.");
		else
			msg_print("You hear thrashing about.");
	}

	/* Felines */
	else if (strchr("f", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear a feline snarl.");
		else if (percentage > 33)
			msg_print("You hear a mewling sound.");
		else
			msg_print("You hear a pitiful mewling.");
	}

	/* Insects, Spiders */
	else if (strchr("IM", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear an angry droning.");
		else if (percentage > 33)
			msg_print("You hear a scuttling sound.");
		else
			msg_print("You hear a skittering sound.");
	}


	/* Birds, Bats, Vampires */
	else if (strchr("bv", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear a squeal.");
		else if (percentage > 33)
			msg_print("You hear a shrieks.");
		else
			msg_print("You hear erratic fluttering.");
	}

	/* Humanoid monsters */
	else if (strchr("@oTGV", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear a grunt.");
		else if (percentage > 33)
			msg_print("You hear a cry of pain.");
		else
			msg_print("You hear a feeble cry.");
	}

	/* Some other monsters */
	else if (strchr("HRN", r_ptr->d_char))
	{
		if (percentage > 66)
			msg_print("You hear a strange grunt.");
		else if (percentage > 33)
			msg_print("You hear a terrible cry.");
		else
			msg_print("You hear a unnatural cry.");
	}
	
	// m, w, | are silent
}




