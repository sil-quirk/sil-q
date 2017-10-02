/* File: object2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * Excise a dungeon object from any stacks
 */
void excise_object_idx(int o_idx)
{
	object_type *j_ptr;

	s16b this_o_idx, next_o_idx = 0;

	s16b prev_o_idx = 0;


	/* Object */
	j_ptr = &o_list[o_idx];

	/* Monster */
	if (j_ptr->held_m_idx)
	{
		monster_type *m_ptr;

		/* Monster */
		m_ptr = &mon_list[j_ptr->held_m_idx];

		/* Scan all objects in the grid */
		for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
		{
			object_type *o_ptr;

			/* Get the object */
			o_ptr = &o_list[this_o_idx];

			/* Get the next object */
			next_o_idx = o_ptr->next_o_idx;

			/* Done */
			if (this_o_idx == o_idx)
			{
				/* No previous */
				if (prev_o_idx == 0)
				{
					/* Remove from list */
					m_ptr->hold_o_idx = next_o_idx;
				}

				/* Real previous */
				else
				{
					object_type *i_ptr;

					/* Previous object */
					i_ptr = &o_list[prev_o_idx];

					/* Remove from list */
					i_ptr->next_o_idx = next_o_idx;
				}

				/* Forget next pointer */
				o_ptr->next_o_idx = 0;

				/* Done */
				break;
			}

			/* Save prev_o_idx */
			prev_o_idx = this_o_idx;
		}
	}

	/* Dungeon */
	else
	{
		int y = j_ptr->iy;
		int x = j_ptr->ix;

		/* Scan all objects in the grid */
		for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
		{
			object_type *o_ptr;

			/* Get the object */
			o_ptr = &o_list[this_o_idx];

			/* Get the next object */
			next_o_idx = o_ptr->next_o_idx;

			/* Done */
			if (this_o_idx == o_idx)
			{
				/* No previous */
				if (prev_o_idx == 0)
				{
					/* Remove from list */
					cave_o_idx[y][x] = next_o_idx;
				}

				/* Real previous */
				else
				{
					object_type *i_ptr;

					/* Previous object */
					i_ptr = &o_list[prev_o_idx];

					/* Remove from list */
					i_ptr->next_o_idx = next_o_idx;
				}

				/* Forget next pointer */
				o_ptr->next_o_idx = 0;

				/* Done */
				break;
			}

			/* Save prev_o_idx */
			prev_o_idx = this_o_idx;
		}
	}
}


/*
 * Delete a dungeon object
 *
 * Handle "stacks" of objects correctly.
 */
void delete_object_idx(int o_idx)
{
	object_type *j_ptr;

	/* Excise */
	excise_object_idx(o_idx);

	/* Object */
	j_ptr = &o_list[o_idx];

	/* Dungeon floor */
	if (!(j_ptr->held_m_idx))
	{
		int y, x;

		/* Location */
		y = j_ptr->iy;
		x = j_ptr->ix;

		/* Visual update */
		lite_spot(y, x);
	}

	/* Wipe the object */
	object_wipe(j_ptr);

	/* Count objects */
	o_cnt--;
}


/*
 * Deletes all objects at given location
 */
void delete_object(int y, int x)
{
	s16b this_o_idx, next_o_idx = 0;


	/* Paranoia */
	if (!in_bounds(y, x)) return;

	/* Scan all objects in the grid */
	for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Wipe the object */
		object_wipe(o_ptr);

		/* Count objects */
		o_cnt--;
	}

	/* Objects are gone */
	cave_o_idx[y][x] = 0;

	/* Visual update */
	lite_spot(y, x);
}



/*
 * Move an object from index i1 to index i2 in the object list
 */
static void compact_objects_aux(int i1, int i2)
{
	int i;

	object_type *o_ptr;


	/* Do nothing */
	if (i1 == i2) return;


	/* Repair objects */
	for (i = 1; i < o_max; i++)
	{
		/* Get the object */
		o_ptr = &o_list[i];

		/* Skip "dead" objects */
		if (!o_ptr->k_idx) continue;

		/* Repair "next" pointers */
		if (o_ptr->next_o_idx == i1)
		{
			/* Repair */
			o_ptr->next_o_idx = i2;
		}
	}


	/* Get the object */
	o_ptr = &o_list[i1];


	/* Monster */
	if (o_ptr->held_m_idx)
	{
		monster_type *m_ptr;

		/* Get the monster */
		m_ptr = &mon_list[o_ptr->held_m_idx];

		/* Repair monster */
		if (m_ptr->hold_o_idx == i1)
		{
			/* Repair */
			m_ptr->hold_o_idx = i2;
		}
	}

	/* Dungeon */
	else
	{
		int y, x;

		/* Get location */
		y = o_ptr->iy;
		x = o_ptr->ix;

		/* Repair grid */
		if (cave_o_idx[y][x] == i1)
		{
			/* Repair */
			cave_o_idx[y][x] = i2;
		}
	}


	/* Hack -- move object */
	COPY(&o_list[i2], &o_list[i1], object_type);

	/* Hack -- wipe hole */
	object_wipe(o_ptr);
}


/*
 * Compact and Reorder the object list
 *
 * This function can be very dangerous, use with caution!
 *
 * When actually "compacting" objects, we base the saving throw on a
 * combination of object level, distance from player, and current
 * "desperation".
 *
 * After "compacting" (if needed), we "reorder" the objects into a more
 * compact order, and we reset the allocation info, and the "live" array.
 */
void compact_objects(int size)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int i, y, x, num, cnt;

	int cur_lev, cur_dis, chance;


	/* Compact */
	if (size)
	{
		/* Message */
		msg_print("Compacting objects...");

		/* Redraw map */
		p_ptr->redraw |= (PR_MAP);

		/* Window stuff */
		p_ptr->window |= (PW_OVERHEAD);
	}


	/* Compact at least 'size' objects */
	for (num = 0, cnt = 1; num < size; cnt++)
	{
		/* Get more vicious each iteration */
		cur_lev = 5 * cnt;

		/* Get closer each iteration */
		cur_dis = 5 * (20 - cnt);

		/* Examine the objects */
		for (i = 1; i < o_max; i++)
		{
			object_type *o_ptr = &o_list[i];

			object_kind *k_ptr = &k_info[o_ptr->k_idx];

			/* Skip dead objects */
			if (!o_ptr->k_idx) continue;

			/* Hack -- High level objects start out "immune" */
			if ((k_ptr->level > cur_lev) && (k_ptr->squelch != SQUELCH_ALWAYS)) continue;

			/* Monster */
			if (o_ptr->held_m_idx)
			{
				monster_type *m_ptr;

				/* Get the monster */
				m_ptr = &mon_list[o_ptr->held_m_idx];

				/* Get the location */
				y = m_ptr->fy;
				x = m_ptr->fx;

				/* Monsters protect their objects */
				if (percent_chance(90) && (k_ptr->squelch != SQUELCH_ALWAYS)) continue;
			}

			/* Dungeon */
			else
			{
				/* Get the location */
				y = o_ptr->iy;
				x = o_ptr->ix;
			}

			/* Nearby objects start out "immune" */
			if ((cur_dis > 0) && (distance(py, px, y, x) < cur_dis) &&
				(k_ptr->squelch != SQUELCH_ALWAYS)) continue;

			/* Saving throw */
			chance = 90;

			/* Squelched items get compacted */
			if ((k_ptr->aware) && (k_ptr->squelch == SQUELCH_ALWAYS)) chance = 0;

 			/* Hack -- only compact artefacts in emergencies */
			if (artefact_p(o_ptr) && (cnt < 1000)) chance = 100;

			/* Apply the saving throw */
			if (percent_chance(chance)) continue;

			/* Delete the object */
			delete_object_idx(i);

			/* Count it */
			num++;
		}
	}


	/* Excise dead objects (backwards!) */
	for (i = o_max - 1; i >= 1; i--)
	{
		object_type *o_ptr = &o_list[i];

		/* Skip real objects */
		if (o_ptr->k_idx) continue;

		/* Move last object into open hole */
		compact_objects_aux(o_max - 1, i);

		/* Compress "o_max" */
		o_max--;
	}
}




/*
 * Delete all the items when player leaves the level
 *
 * Note -- we do NOT visually reflect these (irrelevant) changes
 *
 * Hack -- we clear the "cave_o_idx[y][x]" field for every grid,
 * and the "m_ptr->next_o_idx" field for every monster, since
 * we know we are clearing every object.  Technically, we only
 * clear those fields for grids/monsters containing objects,
 * and we clear it once for every such object.
 */
void wipe_o_list(void)
{
	int i;

	/* Delete the existing objects */
	for (i = 1; i < o_max; i++)
	{
		object_type *o_ptr = &o_list[i];

		/* Skip dead objects */
		if (!o_ptr->k_idx) continue;

		/* Mega-Hack -- preserve artefacts */
		// Sil-y: no longer preserving artefacts
		//if (!character_dungeon || (PRESERVE_MODE))
		//{
		//	/* Hack -- Preserve unknown artefacts */
		//	if (artefact_p(o_ptr) && !object_known_p(o_ptr))
		//	{
		//		/* Mega-Hack -- Preserve the artefact */
		//		a_info[o_ptr->name1].cur_num = 0;
		//	}
		//}

		/* Monster */
		if (o_ptr->held_m_idx)
		{
			monster_type *m_ptr;

			/* Monster */
			m_ptr = &mon_list[o_ptr->held_m_idx];

			/* Hack -- see above */
			m_ptr->hold_o_idx = 0;
		}

		/* Dungeon */
		else
		{
			/* Get the location */
			int y = o_ptr->iy;
			int x = o_ptr->ix;

			/* Hack -- see above */
			cave_o_idx[y][x] = 0;
		}

		/*Wipe the randart if necessary*/
		if (o_ptr->name1) artefact_wipe(o_ptr->name1);

		/* Wipe the object */
		(void)WIPE(o_ptr, object_type);
	}

	/* Reset "o_max" */
	o_max = 1;

	/* Reset "o_cnt" */
	o_cnt = 0;
}


/*
 * Get and return the index of a "free" object.
 *
 * This routine should almost never fail, but in case it does,
 * we must be sure to handle "failure" of this routine.
 */
s16b o_pop(void)
{
	int i;


	/* Initial allocation */
	if (o_max < z_info->o_max)
	{
		/* Get next space */
		i = o_max;

		/* Expand object array */
		o_max++;

		/* Count objects */
		o_cnt++;

		/* Use this object */
		return (i);
	}


	/* Recycle dead objects */
	for (i = 1; i < o_max; i++)
	{
		object_type *o_ptr;

		/* Get the object */
		o_ptr = &o_list[i];

		/* Skip live objects */
		if (o_ptr->k_idx) continue;

		/* Count objects */
		o_cnt++;

		/* Use this object */
		return (i);
	}


	/* Warn the player (except during dungeon creation) */
	if (character_dungeon) msg_print("Too many objects!");

	/* Oops */
	return (0);
}


/*
 * Get the first object at a dungeon location
 * or NULL if there isn't one.
 */
object_type* get_first_object(int y, int x)
{
	s16b o_idx = cave_o_idx[y][x];

	if (o_idx) return (&o_list[o_idx]);

	/* No object */
	return (NULL);
}


/*
 * Get the next object in a stack or
 * NULL if there isn't one.
 */
object_type* get_next_object(const object_type *o_ptr)
{
	if (o_ptr->next_o_idx) return (&o_list[o_ptr->next_o_idx]);

	/* No more objects */
	return (NULL);
}


/*
 * Apply a "object restriction function" to the "object allocation table"
 */
errr get_obj_num_prep(void)
{
	int i;

	/* Get the entry */
	alloc_entry *table = alloc_kind_table;

	/* Scan the allocation table */
	for (i = 0; i < alloc_kind_size; i++)
	{
		/* Accept objects which pass the restriction, if any */
		if (!get_obj_num_hook || (*get_obj_num_hook)(table[i].index))
		{
			/* Accept this object */
			table[i].prob2 = table[i].prob1;
		}

		/* Do not use this object */
		else
		{
			/* Decline this object */
			table[i].prob2 = 0;
		}
	}

	/* Success */
	return (0);
}



/*
 * Choose an object kind that seems "appropriate" to the given level
 *
 * This function uses the "prob2" field of the "object allocation table",
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an "appropriate" object, in
 * a relatively efficient manner.
 *
 * It is (slightly) more likely to acquire an object of the given level
 * than one of a lower level.  This is done by choosing several objects
 * appropriate to the given level and keeping the "hardest" one.
 *
 * Note that if no objects are "appropriate", then this function will
 * fail, and return zero, but this should *almost* never happen.
 * (but it does happen with certain themed items occasionally). -JG
 */
s16b get_obj_num(int level)
{
	int i, j, p;

	int k_idx;

	long value, total;

	object_kind *k_ptr;

	alloc_entry *table = alloc_kind_table;

	/* Boost level */
	if (level > 0)
	{
		/* Occasional "boost" */
		if (one_in_(GREAT_OBJ))
		{
			// most of the time, choose a new deeper depth, weighted towards the current depth
			if (level < MORGOTH_DEPTH)
			{
				int x = rand_range(level+1, MORGOTH_DEPTH);
				int y = rand_range(level+1, MORGOTH_DEPTH);
				
				level = MIN(x,y);
			}
			
			// but if it was already very deep, just increment it
			else
			{
				level++;
			}
		}
	}

	/* Reset total */
	total = 0L;

	/* Process probabilities */
	for (i = 0; i < alloc_kind_size; i++)
	{
		/* Objects are sorted by depth */
		if (table[i].level > level) break;

		/* Default */
		table[i].prob3 = 0;

		/* Get the index */
		k_idx = table[i].index;

		/* Get the actual kind */
		k_ptr = &k_info[k_idx];

		/* Hack -- prevent embedded chests*/
		if ((object_generation_mode == OB_GEN_MODE_CHEST)
			    && (k_ptr->tval == TV_CHEST)) continue;

		/* Accept */
		table[i].prob3 = table[i].prob2;

		/* Total */
		total += table[i].prob3;
	}

	/* No legal objects */
	if (total <= 0) return (0);

	/* Pick an object */
	value = rand_int(total);

	/* Find the object */
	for (i = 0; i < alloc_kind_size; i++)
	{
		/* Found the entry */
		if (value < table[i].prob3) break;

		/* Decrement */
		value = value - table[i].prob3;
	}


	/* Power boost */
	p = rand_int(100);

	/* Try for a "better" object once (50%) or twice (10%) */
	if (p < 60)
	{
		/* Save old */
		j = i;

		/* Pick a object */
		value = rand_int(total);

		/* Find the monster */
		for (i = 0; i < alloc_kind_size; i++)
		{
			/* Found the entry */
			if (value < table[i].prob3) break;

			/* Decrement */
			value = value - table[i].prob3;
		}

		/* Keep the "best" one */
		if (table[i].level < table[j].level) i = j;
	}

	/* Try for a "better" object twice (10%) */
	if (p < 10)
	{
		/* Save old */
		j = i;

		/* Pick a object */
		value = rand_int(total);

		/* Find the object */
		for (i = 0; i < alloc_kind_size; i++)
		{
			/* Found the entry */
			if (value < table[i].prob3) break;

			/* Decrement */
			value = value - table[i].prob3;
		}

		/* Keep the "best" one */
		if (table[i].level < table[j].level) i = j;
	}


	/* Result */
	return (table[i].index);
}


/*
 * Known is true when the "attributes" of an object are "known".
 *
 * These attributes include tohit, todam, toac, cost, and pval (charges).
 *
 * Note that "knowing" an object gives you everything that an "awareness"
 * gives you, and much more.  In fact, the player is always "aware" of any
 * item which he "knows", except items in stores.
 *
 * But having full knowledge of, say, one "staff of Sanctity", does not, by
 * itself, give you knowledge, or even awareness, of other "staffs of Sanctity".
 * It happens that most "identify" routines (including "buying from a shop")
 * will make the player "aware" of the object as well as "know" it.
 *
 * This routine also removes any inscriptions generated by "feelings".
 */
void object_known(object_type *o_ptr)
{
	/* Remove special inscription, if any */
	if (o_ptr->discount >= INSCRIP_NULL) o_ptr->discount = 0;

	/* The object is not "sensed" */
	o_ptr->ident &= ~(IDENT_SENSE);

	/* Clear the "Empty" info */
	o_ptr->ident &= ~(IDENT_EMPTY);

	/* Now we know about the item */
	o_ptr->ident |= (IDENT_KNOWN);
}



/*
 * The player is now aware of the effects of the given object.
 */
void object_aware(object_type *o_ptr)
{
	int x, y;
	bool flag = k_info[o_ptr->k_idx].aware;

 	/* Fully aware of the effects */
 	k_info[o_ptr->k_idx].aware = TRUE;

	// If newly aware
	if (!flag && !p_ptr->leaving)
	{
		// gain experience for identification
		int new_exp = 100;
		gain_exp(new_exp);
		p_ptr->ident_exp += new_exp;
		
		// remove any autoinscription
		obliterate_autoinscription(o_ptr->k_idx);
	}

	/* If newly aware and squelched, must rearrange stacks */
	if ((!flag) && (k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS))
	{
		for (x = 0; x < p_ptr->cur_map_wid; x++)
		{
			for (y = 0; y < p_ptr->cur_map_hgt; y++)
			{
				rearrange_stack(y, x);
			}
		}
	}
}



/*
 * Something has been "sampled"
 */
void object_tried(object_type *o_ptr)
{
	/* Mark it as tried (even if "aware") */
	k_info[o_ptr->k_idx].tried = TRUE;
}


/*
 * Return the "value" of an "unknown" item
 * Make a guess at the value of non-aware items
 */
static s32b object_value_base(const object_type *o_ptr)
{
	int value = 0;
	
	object_kind *k_ptr = &k_info[o_ptr->k_idx];
	
	/* Use template cost for aware objects */
	if (object_aware_p(o_ptr))
	{		
		/* Give credit for hit bonus */
		value += ((o_ptr->att - k_ptr->att) * 100L);

		/* Give credit for evasion bonus */
		value += ((o_ptr->evn - k_ptr->evn) * 100L);

		/* Give credit for sides bonus */
		value += ((o_ptr->ps - k_ptr->ps) * o_ptr->pd * 100L);

		/* Give credit for dice bonus */
		value += ((o_ptr->pd - k_ptr->pd) * o_ptr->ps * 100L);
		
		/* Give credit for sides bonus */
		value += ((o_ptr->ds - k_ptr->ds) * 100L);

		/* Give credit for dice bonus */
		value += ((o_ptr->dd - k_ptr->dd) * o_ptr->ds * 100L);
		
		// Arrows are worth less since they are perishable
		if (o_ptr->tval == TV_ARROW) value /= 10;
		
		// add in the base cost from the template
		value += k_ptr->cost;
	}

	else
	{
	
		/* Analyze the type */
		switch (o_ptr->tval)
		{
			/* Un-aware Food */
			case TV_FOOD: return (5L);

			/* Un-aware Potions */
			case TV_POTION: return (20L);

			/* Un-aware Staffs */
			case TV_STAFF: return (70L);

			/* Un-aware Rods */
			case TV_HORN: return (90L);

			/* Un-aware Rings */
			case TV_RING: return (45L);

			/* Un-aware Amulets */
			case TV_AMULET: return (45L);
		}
	}

	return (value);
}


/*
 * Return the "real" price of a "known" item, not including discounts.
 *
 * Wand and staffs get cost for each charge.
 *
 * Armor is worth an extra 100 gold per bonus point to armor class.
 *
 * Weapons are worth an extra 100 gold per bonus point (AC,TH,TD).
 *
 * Missiles are only worth 5 gold per bonus point, since they
 * usually appear in groups of 20, and we want the player to get
 * the same amount of cash for any "equivalent" item.  Note that
 * missiles never have any of the "pval" flags, and in fact, they
 * only have a few of the available flags, primarily of the "slay"
 * and "brand" and "ignore" variety.
 *
 * Weapons with negative hit+damage bonuses are worthless.
 *
 * Every wearable item with a "pval" bonus is worth extra (see below).
 */
static s32b object_value_real(const object_type *o_ptr)
{
	s32b value;

	u32b f1, f2, f3;

	object_kind *k_ptr = &k_info[o_ptr->k_idx];

	/* Hack -- "worthless" items */
	if (!k_ptr->cost) return (0L);

	/* Base cost */
	value = k_ptr->cost;

	/* Extract some flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Artefact */
	if (o_ptr->name1)
	{
		artefact_type *a_ptr = &a_info[o_ptr->name1];

		/* Hack -- "worthless" artefacts */
		if (!a_ptr->cost) return (0L);

		/* Hack -- Use the artefact cost instead */
		value = a_ptr->cost;
	}

	/* Ego-Item */
	else if (o_ptr->name2)
	{
		ego_item_type *e_ptr = &e_info[o_ptr->name2];

		/* Hack -- "worthless" special items */
		if (!e_ptr->cost) return (0L);

		/* Hack -- Reward the special item with a bonus */
		value += e_ptr->cost;
	}


	/* Analyze pval bonus */
	switch (o_ptr->tval)
	{
		case TV_ARROW:
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		case TV_LIGHT:
		case TV_AMULET:
		case TV_RING:
		{
			/* Hack -- Negative "pval" is always bad */
			if (o_ptr->pval < 0) return (0L);

			/* No pval */
			if (!o_ptr->pval) break;

			/* Give credit for stat bonuses */
			if (f1 & (TR1_STR)) value += (o_ptr->pval * 300L);
			if (f1 & (TR1_DEX)) value += (o_ptr->pval * 300L);
			if (f1 & (TR1_CON)) value += (o_ptr->pval * 300L);
			if (f1 & (TR1_GRA)) value += (o_ptr->pval * 300L);
			if (f1 & (TR1_NEG_STR)) value -= (o_ptr->pval * 300L);
			if (f1 & (TR1_NEG_DEX)) value -= (o_ptr->pval * 300L);
			if (f1 & (TR1_NEG_CON)) value -= (o_ptr->pval * 300L);
			if (f1 & (TR1_NEG_GRA)) value -= (o_ptr->pval * 300L);

			/* Give credit for skills */
			if (f1 & (TR1_MEL)) value += (o_ptr->pval * 100L);
			if (f1 & (TR1_ARC)) value += (o_ptr->pval * 100L);
			if (f1 & (TR1_STL)) value += (o_ptr->pval * 100L);
			if (f1 & (TR1_PER)) value += (o_ptr->pval * 100L);
			if (f1 & (TR1_WIL)) value += (o_ptr->pval * 100L);
			if (f1 & (TR1_SMT)) value += (o_ptr->pval * 100L);
			if (f1 & (TR1_SNG)) value += (o_ptr->pval * 100L);

			/* Give credit for tunneling */
			if (f1 & (TR1_TUNNEL)) value += (o_ptr->pval * 50L);

			/* Give credit for speed bonus */
			if (f2 & (TR2_SPEED)) value += 1000L;

			break;
		}
	}


	/* Analyze the item */
	switch (o_ptr->tval)
	{
		/* Staffs */
		case TV_STAFF:
		{
			/* Pay extra for charges, depending on standard number of
			 * charges.  Handle new-style wands correctly.
			 */
			value += ((value / 20) * (o_ptr->pval / o_ptr->number));

			/* Done */
			break;
		}

		/* Rings/Amulets */
		case TV_RING:
		case TV_AMULET:
		{
			/* Hack -- negative bonuses are bad */
			if (o_ptr->att < 0) return (0L);
			if (o_ptr->evn < 0) return (0L);

			/* Give credit for bonuses */
			value += ((o_ptr->att + o_ptr->evn + o_ptr->ps) * 100L);

			/* Done */
			break;
		}

		/* Armor */
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_CROWN:
		case TV_HELM:
		case TV_SHIELD:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			/* Give credit for hit bonus */
			value += ((o_ptr->att - k_ptr->att) * 100L);

			/* Give credit for evasion bonus */
			value += ((o_ptr->evn - k_ptr->evn) * 100L);

			/* Give credit for sides bonus */
			value += ((o_ptr->ps - k_ptr->ps) * o_ptr->pd * 50L);

			/* Give credit for dice bonus */
			value += ((o_ptr->pd - k_ptr->pd) * o_ptr->ps * 50L);

			/* Done */
			break;
		}

		/* Bows/Weapons */
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_SWORD:
		case TV_POLEARM:
		{
			/* Give credit for hit bonus */
			value += ((o_ptr->att - k_ptr->att) * 100L);

			/* Give credit for evasion bonus */
			value += ((o_ptr->evn - k_ptr->evn) * 100L);

			/* Give credit for sides bonus */
			value += ((o_ptr->ds - k_ptr->ds) * o_ptr->dd * 51L);

			/* Give credit for dice bonus */
			value += ((o_ptr->dd - k_ptr->dd) * o_ptr->ds * 51L);

			/* Done */
			break;
		}

		/* Arrows */
		case TV_ARROW:
		{
			/* Give credit for hit bonus */
			value += ((o_ptr->att - k_ptr->att) * 10L);

			/* Done */
			break;
		}
	}

	/* No negative value */
	if (value < 0) value = 0;

	/* Return the value */
	return (value);
}


/*
 * Return the price of an item including plusses (and charges).
 *
 * This function returns the "value" of the given item (qty one).
 *
 * Never notice "unknown" bonuses or properties, including "curses",
 * since that would give the player information he did not have.
 *
 * Note that discounted items stay discounted forever.
 */
s32b object_value(const object_type *o_ptr)
{
	s32b value;


	/* Known items -- acquire the actual value */
	if (object_known_p(o_ptr))
	{
		/* Broken items -- worthless */
		if (broken_p(o_ptr)) return (0L);

		/* Cursed items -- worthless */
		if (cursed_p(o_ptr)) return (0L);

		/* Real value (see above) */
		value = object_value_real(o_ptr);
	}

	/* Unknown items -- acquire the base value */
	else
	{
		/* Hack -- Felt broken items */
		if ((o_ptr->ident & (IDENT_SENSE)) && broken_p(o_ptr)) return (0L);

		/* Hack -- Felt cursed items */
		if ((o_ptr->ident & (IDENT_SENSE)) && cursed_p(o_ptr)) return (0L);

		/* Base value (see above) */
		value = object_value_base(o_ptr);
	}

	/* Return the final value */
	return (value);
}





/*
 * Determine if an item can "absorb" a second item
 *
 * See "object_absorb()" for the actual "absorption" code.
 *
 * If permitted, we allow wands/staffs (if they are known to have equal
 * charges) and rods (if fully charged) to combine.  They will unstack
 * (if necessary) when they are used.
 *
 * If permitted, we allow weapons/armor to stack, if fully "known".
 *
 * Missiles will combine if both stacks have the same "known" status.
 * This is done to make unidentified stacks of missiles useful.
 *
 * Food, potions, and "easy know" items always stack.
 *
 * Chests, and activatable items, except rods, never stack (for various reasons).
 */
bool object_similar(const object_type *o_ptr, const object_type *j_ptr)
{
	/* Require identical object types */
	if (o_ptr->k_idx != j_ptr->k_idx) return (FALSE);

	/* Require identical weight */
	if (!(o_ptr->weight == j_ptr->weight)) return (FALSE);
		
	/* Analyze the items */
	switch (o_ptr->tval)
	{
		/* Chests */
		case TV_CHEST:
		{
			/* Never okay */
			return (FALSE);
		}

		/* Food and Potions */
		case TV_FOOD:
		case TV_POTION:
		{
			/* Assume okay */
			break;
		}

		/* Staves */
		case TV_STAFF:
		{
			/* Don't merge as it messes with charges etc. */
			return(FALSE);
		}

		/* Horns */
		case TV_HORN:
		{
			/* Assume okay */
			break;
		}

		/* Rings, Amulets, Lites and Books */
		case TV_RING:
		case TV_AMULET:
		case TV_LIGHT:
		{
			/* Require both items to be known */
			if (!object_known_p(o_ptr) || !object_known_p(j_ptr)) return (FALSE);
			
			/* Fall through */
		}

		/* Weapons and Armor */
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			/* Fall Through */
		}

		/* Missiles & most things from above */
		case TV_ARROW:
		{
			/* Require identical knowledge of both items */
			if (object_known_p(o_ptr) != object_known_p(j_ptr)) return (FALSE);

			/* Require identical "bonuses" */
			if (o_ptr->att != j_ptr->att) return (FALSE);
			if (o_ptr->evn != j_ptr->evn) return (FALSE);
			if (o_ptr->ds != j_ptr->ds) return (FALSE);
			if (o_ptr->dd != j_ptr->dd) return (FALSE);
			
			// only check protection if at least one item has it
			if ((o_ptr->pd * o_ptr->ps > 0) || (j_ptr->pd * j_ptr->ps > 0))
			{
				if (o_ptr->ps != j_ptr->ps) return (FALSE);
				if (o_ptr->pd != j_ptr->pd) return (FALSE);
			}

			/* Require identical "pval" code */
			if (o_ptr->pval != j_ptr->pval) return (FALSE);

			/* Require identical "artefact" names */
			if (o_ptr->name1 != j_ptr->name1) return (FALSE);

			/* Require identical "special item" names */
			if (o_ptr->name2 != j_ptr->name2) return (FALSE);

			/* Mega-Hack -- Handle lights */
			if (fuelable_light_p(o_ptr))
			{
				if (o_ptr->timeout != j_ptr->timeout) return (FALSE);
			}

			/* Hack -- Never stack recharging items */
			else if (o_ptr->timeout || j_ptr->timeout) return (FALSE);

			/* Probably okay */
			break;
		}

		/* Various */
		default:
		{
			/* Require knowledge */
			if (!object_known_p(o_ptr) || !object_known_p(j_ptr)) return (FALSE);

			/* Probably okay */
			break;
		}
	}

	/* Hack -- Require identical "cursed" and "broken" status */
	if (((o_ptr->ident & (IDENT_CURSED)) != (j_ptr->ident & (IDENT_CURSED))) ||
	    ((o_ptr->ident & (IDENT_BROKEN)) != (j_ptr->ident & (IDENT_BROKEN))))
	{
		return (FALSE);
	}

	/* Hack -- Require compatible inscriptions */
	if (o_ptr->obj_note != j_ptr->obj_note)
	{
		/* Normally require matching inscriptions */
		return (FALSE);
	}


	/* Hack -- Require compatible "discount" fields */
	if (o_ptr->discount != j_ptr->discount)
	{
		/* Both are (different) special inscriptions */
		if ((o_ptr->discount >= INSCRIP_NULL) &&
		    (j_ptr->discount >= INSCRIP_NULL))
		{
			/* Normally require matching inscriptions */
			return (FALSE);
		}

		/* One is a special inscription, one is a discount or nothing */
		else if ((o_ptr->discount >= INSCRIP_NULL) ||
		         (j_ptr->discount >= INSCRIP_NULL))
		{
			/* Normally require matching inscriptions */
			return (FALSE);
		}

		/* One is a discount, one is a (different) discount or nothing */
		else
		{
			/* require matching discounts */
			return (FALSE);
		}
	}

	/* Maximal "stacking" limit */
	//if (total >= MAX_STACK_SIZE) return (FALSE);
	if (o_ptr->number == MAX_STACK_SIZE - 1) return (FALSE);
	if (j_ptr->number == MAX_STACK_SIZE - 1) return (FALSE);

	/* They match, so they must be similar */
	return (TRUE);
}


/*
 * Allow one item to "absorb" another, assuming they are similar.
 *
 * The blending of the "note" field assumes that either (1) one has an
 * inscription and the other does not, or (2) neither has an inscription.
 * In both these cases, we can simply use the existing note, unless the
 * blending object has a note, in which case we use that note.
 *
 * The blending of the "discount" field assumes that either (1) one is a
 * special inscription and one is nothing, or (2) one is a discount and
 * one is a smaller discount, or (3) one is a discount and one is nothing,
 * or (4) both are nothing.  In all of these cases, we can simply use the
 * "maximum" of the two "discount" fields.
 *
 * These assumptions are enforced by the "object_similar()" code.
 */
void object_absorb(object_type *o_ptr, object_type *j_ptr)
{
	int total = o_ptr->number + j_ptr->number;

	/* Add together the item counts */
	o_ptr->number = ((total < MAX_STACK_SIZE) ? total : (MAX_STACK_SIZE - 1));

	// determine the new count for j_ptr
	j_ptr->number = ((total < MAX_STACK_SIZE) ? 0 : total - (MAX_STACK_SIZE - 1));

	/* Hack -- Blend "known" status */
	if (object_known_p(j_ptr)) object_known(o_ptr);
	if (object_known_p(o_ptr)) object_known(j_ptr);

	/* Hack -- Blend "notes" */
	if (j_ptr->obj_note != 0) o_ptr->obj_note = j_ptr->obj_note;
	if (o_ptr->obj_note != 0) j_ptr->obj_note = o_ptr->obj_note;

	/* Mega-Hack -- Blend "discounts" */
	if (o_ptr->discount < j_ptr->discount) o_ptr->discount = j_ptr->discount;
	if (j_ptr->discount < o_ptr->discount) j_ptr->discount = o_ptr->discount;
}



/*
 * Find the index of the object_kind with the given tval and sval
 */
s16b lookup_kind(int tval, int sval)
{
	int k;

	/* Look for it */
	for (k = 1; k < z_info->k_max; k++)
	{
		object_kind *k_ptr = &k_info[k];

		/* Found a match */
		if ((k_ptr->tval == tval) && (k_ptr->sval == sval)) return (k);
	}

	/* Oops */
	msg_format("No object (%d,%d)", tval, sval);

	/* Oops */
	return (0);
}


/*
 * Wipe an object clean.
 */
void object_wipe(object_type *o_ptr)
{
	/* Wipe the structure */
	(void)WIPE(o_ptr, object_type);
}


/*
 * Prepare an object based on an existing object
 */
void object_copy(object_type *o_ptr, const object_type *j_ptr)
{
	/* Copy the structure */
	COPY(o_ptr, j_ptr, object_type);
}


/*
 * Set Hallucinatory object kind
 */
int random_k_idx(void)
{
	object_kind *k_ptr;
	int kind_idx;
	
	while (1)
	{
		kind_idx = rand_int(z_info->k_max);
		k_ptr = &k_info[kind_idx];
		if (k_ptr->tval != 0) return(kind_idx);
	}
}



/*
 * Prepare an object based on an object kind.
 */
void object_prep(object_type *o_ptr, int k_idx)
{
	int i;

	object_kind *k_ptr = &k_info[k_idx];

	/* Clear the record */
	(void)WIPE(o_ptr, object_type);

	/* Save the kind index */
	o_ptr->k_idx = k_idx;

	/* Save the hallucinatory kind index */
	o_ptr->image_k_idx = random_k_idx();

	/* Efficiency -- tval/sval */
	o_ptr->tval = k_ptr->tval;
	o_ptr->sval = k_ptr->sval;

	/* Default "pval" */
	o_ptr->pval = k_ptr->pval;

	/* Default number */
	o_ptr->number = 1;

	/* Exact weight for most item, approximate weight for weapons and armour */
	switch (o_ptr->tval)
	{
		case TV_BOW:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			o_ptr->weight = Rand_normal(k_ptr->weight, k_ptr->weight/6 + 1);
			
            // round to the nearest multiple of 0.5 lb
            o_ptr->weight = div_round(o_ptr->weight*2, 10);
            o_ptr->weight *= 5;
            
			// restrict weight to within [2/3, 3/2] of the standard
            while (o_ptr->weight*3 < k_ptr->weight*2) o_ptr->weight += 5;
            while (o_ptr->weight*2 > k_ptr->weight*3) o_ptr->weight -= 5;
            
			break;
		}
		default:
			o_ptr->weight = k_ptr->weight;
	}
	
	/* Default bonuses to attack and defence */
	o_ptr->att = k_ptr->att;
	o_ptr->dd = k_ptr->dd;
	o_ptr->ds = k_ptr->ds;
	o_ptr->evn = k_ptr->evn;
	o_ptr->pd = k_ptr->pd;
	o_ptr->ps = k_ptr->ps;
	
	// add the abilities
	for (i = 0; i < k_ptr->abilities; i++)
	{
		o_ptr->skilltype[i] = k_ptr->skilltype[i];
		o_ptr->abilitynum[i] = k_ptr->abilitynum[i];
	}
	o_ptr->abilities = k_ptr->abilities;

	/* Hack -- worthless items are always "broken" */
	if (k_ptr->cost <= 0) o_ptr->ident |= (IDENT_BROKEN);

	/* Hack -- cursed items are always "cursed" */
	if (k_ptr->flags3 & (TR3_LIGHT_CURSE)) o_ptr->ident |= (IDENT_CURSED);
}



/*
 * Cheat -- describe a created object for the user
 */
static void object_mention(const object_type *o_ptr)
{
	char o_name[80];

	/* Describe */
	object_desc_spoil(o_name, sizeof(o_name), o_ptr, FALSE, 0);

	/* Artefact */
	if (artefact_p(o_ptr))
	{
		/* Silly message */
		msg_format("Artefact (%s)", o_name);
	}

	/* Ego-item */
	else if (ego_item_p(o_ptr))
	{
		/* Silly message */
		msg_format("Ego-item (%s)", o_name);
	}

	/* Normal item */
	else
	{
		/* Silly message */
		msg_format("Object (%s)", o_name);
	}
}


/*
 * Attempt to change an object into an special item -MWK-
 * Better only called by apply_magic().
 * The return value says if we picked a cursed item (if allowed) and is
 * passed on to a_m_aux1/2().
 * If no legal ego item is found, this routine returns 0, resulting in
 * an unenchanted item.
 */
static int make_special_item(object_type *o_ptr, bool only_good)
{
	int i, j, level;

	int e_idx;

	long value, total;

	ego_item_type *e_ptr;

	alloc_entry *table = alloc_ego_table;

	/* Fail if object already is ego or artefact */
	if (o_ptr->name1) return (FALSE);
	if (o_ptr->name2) return (FALSE);

	level = object_level;

	/* Boost level (like with object base types) */
	if (level > 0)
	{
		/* Occasional "boost" */
		if (one_in_(GREAT_SPECIAL))
		{
			// most of the time, choose a new deeper depth, weighted towards the current depth
			if (level < MORGOTH_DEPTH)
			{
				int x = rand_range(level+1, MORGOTH_DEPTH);
				int y = rand_range(level+1, MORGOTH_DEPTH);
				
				level = MIN(x,y);
			}
			
			// but if it was already very deep, just increment it
			else
			{
				level++;
			}
		}
	}

	/* Reset total */
	total = 0L;

	/* Process probabilities */
	for (i = 0; i < alloc_ego_size; i++)
	{
		/* Default */
		table[i].prob3 = 0;

		/* Objects are sorted by depth */
		if (table[i].level > level) continue;

		/* Get the index */
		e_idx = table[i].index;

		/* Get the actual kind */
		e_ptr = &e_info[e_idx];

		/* Some special items can't be generated too deep */
		if ((e_ptr->max_level > 0) && (p_ptr->depth > e_ptr->max_level)) continue;

		/* If we force fine/special, don't create cursed */
		if (only_good && (e_ptr->flags3 & TR3_LIGHT_CURSE)) continue;

		/* If we force fine/special, don't useless */
		if (only_good && (e_ptr->cost == 0)) continue;

		/* Test if this is a legal special item type for this object */
		for (j = 0; j < EGO_TVALS_MAX; j++)
		{
			/* Require identical base type */
			if (o_ptr->tval == e_ptr->tval[j])
			{
				/* Require sval in bounds, lower */
				if (o_ptr->sval >= e_ptr->min_sval[j])
				{
					/* Require sval in bounds, upper */
					if (o_ptr->sval <= e_ptr->max_sval[j])
					{
						/* Accept */
						table[i].prob3 = table[i].prob2;
					}
				}
			}
		}

		/* Total */
		total += table[i].prob3;
	}

	// If there aren't *any* valid items to choose from give up
	if (total == 0)
	{
		return (0);
	}

	/* Pick an special item */
	value = rand_int(total);

	/* Find the object */
	for (i = 0; i < alloc_ego_size; i++)
	{
		/* Found the entry */
		if (value < table[i].prob3) break;

		/* Decrement */
		value = value - table[i].prob3;
	}

	/* We have one */
	e_idx = (byte)table[i].index;
	o_ptr->name2 = e_idx;

	return ((e_info[e_idx].flags3 & TR3_LIGHT_CURSE) ? -2 : 2);
}

/*
 * As artefacts are generated, there is an increasing chance to fail to make the next one
 */
static bool too_many_artefacts(void)
{
	int i;
	
	for (i = 0; i < p_ptr->artefacts; i++)
	{
		if (percent_chance(10)) return (TRUE);
	}
	
	return (FALSE);
}


/*
 * Mega-Hack -- Attempt to create one of the "Special Objects".
 *
 * We are only called from "make_object()", and we assume that
 * "apply_magic()" is called immediately after we return.
 *
 * Note -- see "make_artefact()" and "apply_magic()".
 *
 * We *prefer* to create the special artefacts in order, but this is
 * normally outweighed by the "rarity" rolls for those artefacts.
 */
static bool make_artefact_special(object_type *o_ptr)
{
	int i;

	int k_idx;

	int depth_check = ((object_generation_mode) ?  object_level : p_ptr->depth);

	/* No artefacts, do nothing */
	if (adult_no_artefacts) return (FALSE);

	// as more artefacts are generated, the chance for another decreases
	if (too_many_artefacts()) return (FALSE);

	/* Check the special artefacts */
	for (i = 0; i < z_info->art_spec_max; ++i)
	{
		artefact_type *a_ptr = &a_info[i];

		/* Skip "empty" artefacts */
		if (a_ptr->tval + a_ptr->sval == 0) continue;

		/* Cannot make an artefact twice */
		if (a_ptr->cur_num) continue;

		/* Enforce minimum "depth" (loosely) */
		if (a_ptr->level > depth_check)
		{
			/* Get the "out-of-depth factor" */
			int d = (a_ptr->level - depth_check) * 2;

			/* Roll for out-of-depth creation */
			if (rand_int(d) != 0) continue;
		}

		/* Artefact "rarity roll" */
		if (rand_int(a_ptr->rarity) != 0) continue;

		/* Find the base object */
		k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);

		/* Enforce minimum "object" level (loosely) */
		if (k_info[k_idx].level > depth_check)
		{
			/* Get the "out-of-depth factor" */
			int d = (k_info[k_idx].level - depth_check) * 5;

			/* Roll for out-of-depth creation */
			if (rand_int(d) != 0) continue;
		}

		/* Assign the template */
		object_prep(o_ptr, k_idx);

		/* Mark the item as an artefact */
		o_ptr->name1 = i;

		/* Success */
		return (TRUE);
	}

	/* Failure */
	return (FALSE);
}

/*
 * Attempt to change an object into an artefact
 *
 * This routine should only be called by "apply_magic()"
 *
 * Note -- see "make_artefact_special()" and "apply_magic()"
 */
static bool make_artefact(object_type *o_ptr, bool allow_insta)
{
	int i;

	int depth_check = ((object_generation_mode) ?  object_level : p_ptr->depth);

	/* No artefacts, do nothing */
	if (adult_no_artefacts) return (FALSE);
	
	// as more artefacts are generated, the chance for another decreases
	if (too_many_artefacts()) return (FALSE);

	/* First try to create a randart, if allowed */
	if ((can_be_randart(o_ptr)) && (!adult_no_xtra_artefacts))
	{
		/*occasionally make a randart*/
		if(one_in_(depth_check + 50))
		{
			/*artefact power is based on depth*/
			int randart_power = 10 + depth_check;

			/*occasional power boost*/
			while (one_in_(25)) randart_power += 25;

			/*
			 * Make a randart.  This should always succeed, unless
			 * there is no space for another randart
		     */
			if (make_one_randart(o_ptr, randart_power, FALSE)) return (TRUE);
		}
	}

	/* Check the artefact list (skip the "specials" and randoms) */
	for (i = z_info->art_spec_max; i < z_info->art_norm_max; i++)
	{
		artefact_type *a_ptr = &a_info[i];
		
		/* Skip "empty" items */
		if (a_ptr->tval + a_ptr->sval == 0) continue;

		/* Cannot make an artefact twice */
		if (a_ptr->cur_num) continue;

		/* Must have the correct fields */
		if (a_ptr->tval != o_ptr->tval) continue;
		if (a_ptr->sval != o_ptr->sval) continue;

		/* Can only generate 'insta-arts' in certain situations */
		if ((a_ptr->flags3 & (TR3_INSTA_ART)) && !allow_insta)
		{
			continue;
		}

		/* XXX XXX Enforce minimum "depth" (loosely) */
		if (a_ptr->level > depth_check)
		{
			/* Get the "out-of-depth factor" */
			int d = (a_ptr->level - depth_check) * 2;

			/* Roll for out-of-depth creation */
			if (rand_int(d) != 0) continue;
		}

		/* We must make the "rarity roll" */
		if (!one_in_(a_ptr->rarity)) continue;

		/* Mark the item as an artefact */
		o_ptr->name1 = i;

		/* Paranoia -- no "plural" artefacts */
		o_ptr->number = 1;

		/* Success */
		return (TRUE);
	}

	/* Failure */
	return (FALSE);
}


/*
 * Charge a new staff.
 */
static void charge_staff(object_type *o_ptr)
{
	switch (o_ptr->sval)
	{
		case SV_STAFF_SECRETS:				o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_IMPRISONMENT:			o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_FREEDOM:				o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_LIGHT:				o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_SANCTITY:				o_ptr->pval = damroll(2,2); break;
		case SV_STAFF_UNDERSTANDING:		o_ptr->pval = damroll(2,2); break;
		case SV_STAFF_REVELATIONS:			o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_TREASURES:			o_ptr->pval = damroll(2,2); break;
		case SV_STAFF_FOES:					o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_SLUMBER:				o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_MAJESTY:				o_ptr->pval = damroll(4,2); break;
		case SV_STAFF_SELF_KNOWLEDGE:		o_ptr->pval = damroll(2,2); break;
		case SV_STAFF_WARDING:				o_ptr->pval = damroll(2,2); break;
		case SV_STAFF_EARTHQUAKES:			o_ptr->pval = damroll(2,2); break;
		case SV_STAFF_RECHARGING:			o_ptr->pval = damroll(2,2); break;
		
		case SV_STAFF_SUMMONING:			o_ptr->pval = damroll(6,2); break;
		case SV_STAFF_ENTRAPMENT:			o_ptr->pval = damroll(6,2); break;
	}
}

/*
 *
 * Determines the theme of a chest.  This function is called
 * from chest_death when the chest is being opened. JG
 *
 */
static int choose_chest_contents(void)
{
	/*
	 * chest theme # 2 is potions  (+ herbs of restoring)
	 * chest theme # 3 is staffs
	 * chest theme # 4 is shields
	 * chest theme # 5 is weapons
	 * chest theme # 6 is armor
	 * chest theme # 7 is boots
	 * chest theme # 8 is bow
	 * chest theme # 9 is cloak
	 * chest theme #10 is gloves
	 * chest theme #11 is edged weapons
	 * chest theme #12 is polearms
	 * chest theme #13 is helms and crowns
	 * chest theme #14 is jewellery
	 */

	return (dieroll(13) + 1);
}

/*
 * Apply magic to an item known to be a "weapon"
 *
 */
static void a_m_aux_1(object_type *o_ptr, int level)
{
	bool boost_dam = FALSE;
	bool boost_att = FALSE;

	// arrows can only have increased attack value
	if (o_ptr->tval == TV_ARROW)
	{
		o_ptr->att += 3;
		return;	
	}
    
	else
	{
		// small chance of boosting both
		if (percent_chance(level))
		{
			boost_dam = TRUE;
			boost_att = TRUE;
		}
		// otherwise 50/50 chance of dam or att
		else if (one_in_(2))
		{
			boost_dam = TRUE;
		}
		else
		{
			boost_att = TRUE;
		}
	}

	if (boost_dam)
	{
		o_ptr->ds++;
	}
	if (boost_att)
	{
		o_ptr->att++;
	}
}


/*
 * Apply magic to an item known to be "armor"
 *
 */
static void a_m_aux_2(object_type *o_ptr, int level)
{
	
	bool boost_prot = FALSE;
	bool boost_other = FALSE;
	
	// for cloaks and robes and filthy rags go for evasion only
	if ((o_ptr->tval == TV_CLOAK) || ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_ROBE))
	                              || ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_FILTHY_RAG)))
	{
		boost_other = TRUE;
	}
	// otherwise if there are no penalties to fix, then go for protection only
	else if ((o_ptr->att >= 0) && (o_ptr->evn >= 0))
	{
		boost_prot = TRUE;
	}
	// otherwise choose randomly (protection, other, or both)
	else
	{
		// small chance of boosting both
		if (percent_chance(level))
		{
			boost_prot = TRUE;
			boost_other = TRUE;
		}
		// otherwise 50/50 chance of dam or att
		else if (one_in_(2))
		{
			boost_prot = TRUE;
		}
		else
		{
			boost_other = TRUE;
		}
	}
	
	if (boost_other)
	{
		if ((o_ptr->att < 0) && (o_ptr->evn < 0))
		{
			if (one_in_(2)) o_ptr->evn++;
			else o_ptr->att++;
		}
		else if (o_ptr->att < 0)
		{
			o_ptr->att++;
		}
		else
		{
			o_ptr->evn++;
		}
	}
	
	if (boost_prot)
	{
		o_ptr->ps++;
	}

}



/*
 * Apply magic to an item known to be a "ring" or "amulet"
 *
 * Hack -- note special rating boost for ring of speed
 * Hack -- note special rating boost for certain amulets
 * Hack -- note special "pval boost" code for ring of speed
 * Hack -- note that some items must be cursed (or blessed)
 */
static void a_m_aux_3(object_type *o_ptr, int level)
{
	/* Apply magic (good or bad) according to type */
	switch (o_ptr->tval)
	{
		case TV_RING:
		{
			/* Analyze */
			switch (o_ptr->sval)
			{
				/* Strength, Dexterity */
				case SV_RING_STR:
				case SV_RING_DEX:
				{
					/* Stat bonus */
					o_ptr->pval = (level + dieroll(10)) / 10 - 1;
					
					// maximum of +1
					//if (o_ptr->pval > 1) o_ptr->pval = 1;

					/* Cursed */
					if (o_ptr->pval < 0)
					{
						/* Broken */
						o_ptr->ident |= (IDENT_BROKEN);

						/* Cursed */
						o_ptr->ident |= (IDENT_CURSED);
					}

					break;
				}

				/* Ring of damage */
				case SV_RING_DAMAGE:
				{
					/* Bonus to damage sides */
					o_ptr->pval = (level + dieroll(10)) / 10 - 1;
					
					// can't be zero
					if (o_ptr->pval == 0)
					{
						o_ptr->pval = -1;
					}

					/* Cursed */
					if (o_ptr->pval < 0)
					{
						/* Broken */
						o_ptr->ident |= (IDENT_BROKEN);
						
						/* Cursed */
						o_ptr->ident |= (IDENT_CURSED);
					}

					break;
				}

				/* Ring of Accuracy */
				case SV_RING_ACCURACY:
				{
					/* Bonus to attack */
					o_ptr->att = (level + dieroll(10)) / 7 - 1;
					
					// can't be zero
					if (o_ptr->att == 0)
					{
						o_ptr->att = +1;
					}

					/* Cursed */
					if (o_ptr->att < 0)
					{
						/* Broken */
						o_ptr->ident |= (IDENT_BROKEN);
						
						/* Cursed */
						o_ptr->ident |= (IDENT_CURSED);
					}
					
					break;
				}

				/* Ring of Protection */
				case SV_RING_PROTECTION:
				{
					/* Bonus to protection */
					o_ptr->pd = 1;
					o_ptr->ps = (level + dieroll(10)) / 14 + 1;
			
					break;
				}

				/* Ring of Evasion */
				case SV_RING_EVASION:
				{
					/* Bonus to evasion */
					o_ptr->evn = (level + dieroll(10)) / 7 - 1;
					
					// can't be zero
					if (o_ptr->evn == 0)
					{
						o_ptr->evn = +1;
					}
			
					/* Cursed */
					if (o_ptr->evn < 0)
					{
						/* Broken */
						o_ptr->ident |= (IDENT_BROKEN);
						
						/* Cursed */
						o_ptr->ident |= (IDENT_CURSED);
					}
					
					break;
				}

				/* Ring of Perception */
				case SV_RING_PERCEPTION:
				{
					/* Bonus to perception */
					o_ptr->pval = (level + dieroll(10)) / 5 - 1;
					
					// can't be zero
					if (o_ptr->pval == 0)
					{
						o_ptr->pval = +1;
					}
			
					/* Cursed */
					if (o_ptr->pval < 0)
					{
						/* Broken */
						o_ptr->ident |= (IDENT_BROKEN);
						
						/* Cursed */
						o_ptr->ident |= (IDENT_CURSED);
					}

					break;
				}
			}

			break;
		}

		case TV_AMULET:
		{
			/* Analyze */
			switch (o_ptr->sval)
			{
				/* Various amulets */
				case SV_AMULET_CON:
				case SV_AMULET_GRA:
				{
					/* Stat bonus */
					o_ptr->pval = (level + dieroll(10)) / 10 - 1;
					
					// maximum of +1
					//if (o_ptr->pval > 1) o_ptr->pval = 1;

					/* Cursed */
					if (o_ptr->pval < 0)
					{
						/* Broken */
						o_ptr->ident |= (IDENT_BROKEN);

						/* Cursed */
						o_ptr->ident |= (IDENT_CURSED);
					}

					break;
				}

				/* Amulet of the Blessed Realm */
				case SV_AMULET_BLESSED_REALM:
				{
					o_ptr->pval = 1;

					break;
				}

			}

			break;
		}
	}
}


/*
 * Apply magic to an item known to be "boring"
 *
 * Hack -- note the special code for various items
 */
static void a_m_aux_4(object_type *o_ptr, int level, bool fine, bool special)
{
	object_kind *k_ptr = &k_info[o_ptr->k_idx];

	/* Unused parameters */
	(void)level;

	/* Apply magic (good or bad) according to type */
	switch (o_ptr->tval)
	{
		case TV_LIGHT:
		{
			/* Hack -- Torches -- random fuel */
			if (o_ptr->sval == SV_LIGHT_TORCH)
			{
				if (one_in_(3))
				{
					o_ptr->timeout = rand_range(500, 2000);
				}
				else
				{
					o_ptr->timeout = 2000;
				}
			}

			/* Hack -- Lanterns -- random fuel */
			else if (o_ptr->sval == SV_LIGHT_LANTERN)
			{
				if (one_in_(3))
				{
					o_ptr->timeout = rand_range(500, 3000);
				}
				else
				{
					o_ptr->timeout = 3000;
				}
			}

			break;
		}

		case TV_STAFF:
		{
			/* Hack -- charge staffs */
			charge_staff(o_ptr);

			break;
		}

		case TV_HORN:
		{
			/* Transfer the pval. */
			o_ptr->pval = k_ptr->pval;
			break;
		}

		case TV_CHEST:
		{
			/* Hack -- chest level is fixed at player level at time of generation */
			o_ptr->pval = object_level;

			/*chest created with fine flag get a level boost*/
			if (fine) o_ptr->pval += 2;

			/*chest created with special flag also gets a level boost*/
			if (special) o_ptr->pval += 2;

			/*chests now increase level rating*/
			rating += 5;

			/* Don't exceed "chest level" of 25 */
			if (o_ptr->pval > 25) o_ptr->pval = 25;

			/*a minimum pval of 1, or else it will be empty on the surface*/
			if (o_ptr->pval < 1) o_ptr->pval = 1;

			/*save the chest theme in xtra1, used in chest death*/
			o_ptr->xtra1 = choose_chest_contents();
			
			break;
		}
	}
}

void object_into_artefact(object_type *o_ptr, artefact_type *a_ptr)
{
	int i;

	/* Extract the other fields */
	o_ptr->pval = a_ptr->pval;
	o_ptr->att = a_ptr->att;
	o_ptr->dd = a_ptr->dd;
	o_ptr->ds = a_ptr->ds;
	o_ptr->evn = a_ptr->evn;
	o_ptr->pd = a_ptr->pd;
	o_ptr->ps = a_ptr->ps;
	o_ptr->weight = a_ptr->weight;

	// add the abilities
	for (i = 0; i < a_ptr->abilities; i++)
	{
		o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
		o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
	}
	o_ptr->abilities += a_ptr->abilities;
	
	/* Hack - mark the depth of artefact creation for the notes function
	 * probably a bad idea to use this flag.  It is used when making special items,
	 * which currently fails when an item is an artefact.  If this was changed
	 * this would be the cause of some major bugs.
	 */
	if (p_ptr->depth)
	{
		o_ptr->xtra1 = p_ptr->depth;
	}
	
	/*hack - mark chest items with a special level so the notes patch
	 * knows where it is coming from.
	 */
	else if (object_generation_mode == OB_GEN_MODE_CHEST) o_ptr->xtra1 = CHEST_LEVEL;

	/* Hack -- extract the "broken" flag */
	if (!a_ptr->cost) o_ptr->ident |= (IDENT_BROKEN);

	/* Hack -- extract the "cursed" flag */
	if (a_ptr->flags3 & (TR3_LIGHT_CURSE)) o_ptr->ident |= (IDENT_CURSED);
}



void object_into_special(object_type *o_ptr, int lev, bool smithing)
{
	ego_item_type *e_ptr = &e_info[o_ptr->name2];
	u32b f1, f2, f3;
	int i;
	
	(void) lev; // Cast to soothe compilation warnings (currently unused variable)
	
	/* Examine the item */
	object_flags(o_ptr, &f1, &f2, &f3);
		
	// add the abilities
	for (i = 0; i < e_ptr->abilities; i++)
	{
		o_ptr->skilltype[i + o_ptr->abilities] = e_ptr->skilltype[i];
		o_ptr->abilitynum[i + o_ptr->abilities] = e_ptr->abilitynum[i];
	}
	o_ptr->abilities += e_ptr->abilities;
	
	/* Hack -- acquire "broken" flag */
	if (!e_ptr->cost) o_ptr->ident |= (IDENT_BROKEN);
	
	/* Hack -- acquire "cursed" flag */
	if (e_ptr->flags3 & (TR3_LIGHT_CURSE)) o_ptr->ident |= (IDENT_CURSED);
	
	/* Hack -- apply extra bonuses if needed */
	if (smithing)
	{
		/* Hack -- obtain bonuses */
		if (e_ptr->max_att > 0) o_ptr->att += 1;
		if (e_ptr->max_evn > 0) o_ptr->evn += 1;
		if (e_ptr->to_dd > 0) o_ptr->dd += 1;
		if (e_ptr->to_ds > 0) o_ptr->ds += 1;
		if (e_ptr->to_pd > 0) o_ptr->pd += 1;
		if (e_ptr->to_ps > 0) o_ptr->ps += 1;
		
		/* Hack -- obtain pval */
		if (e_ptr->max_pval > 0)
        {
            if (cursed_p(smith_o_ptr))  o_ptr->pval -= 1;
            else                        o_ptr->pval += 1;
        }
	}
	else
	{
		/* Hack -- obtain bonuses */
		if (e_ptr->max_att > 0) o_ptr->att += dieroll(e_ptr->max_att);
		if (e_ptr->max_evn > 0) o_ptr->evn += dieroll(e_ptr->max_evn);
		if (e_ptr->to_dd > 0) o_ptr->dd += dieroll(e_ptr->to_dd);
		if (e_ptr->to_ds > 0) o_ptr->ds += dieroll(e_ptr->to_ds);
		if (e_ptr->to_pd > 0) o_ptr->pd += dieroll(e_ptr->to_pd);
		if (e_ptr->to_ps > 0) o_ptr->ps += dieroll(e_ptr->to_ps);
		
		/* Hack -- obtain pval */
		if (cursed_p(o_ptr))
		{
			if (e_ptr->max_pval > 0) o_ptr->pval -= dieroll(e_ptr->max_pval);
		}
		else
		{
			if (e_ptr->max_pval > 0) o_ptr->pval += dieroll(e_ptr->max_pval);
		}
	}
	
	/* Cheat -- describe the item */
	if (cheat_peek) object_mention(o_ptr);
	
	// pseudo-id the item
	pseudo_id(o_ptr);
}


/*
 * Complete the "creation" of an object by applying "magic" to the item
 *
 * This includes not only rolling for random bonuses, but also putting the
 * finishing touches on special items and artefacts, giving charges to wands and
 * staffs, giving fuel to lites, and placing traps on chests.
 *
 * In particular, note that "Instant Artefacts", if "created" by an external
 * routine, must pass through this function to complete the actual creation.
 *
 * The base chance of the item being "fine" increases with the "level"
 * parameter, which is usually derived from the dungeon level, being equal
 * to (level)%.
 * The chance that the object will be "special" (special item or artefact), 
 * is also (level)%.
 * If "good" is true, then
 * the object is guaranteed to be either "fine" or "special". 
 * If "great" is true, then the object is guaranteed to be
 * both "fine" and "special".
 *
 * If "okay" is true, and the object is going to be "special", then there is
 * a chance that an artefact will be created.  This is true even if both the
 * "good" and "great" arguments are false.  Objects which have both "good" and "great"
 * flags get three extra "attempts" to become an artefact.
 *
 * If "allow_insta" is true, then INSTA_ART artefacts can be generated
 *
 * Note that in the above we are using the new terminology of 'fine' and 'special'
 * where Vanilla Angband used 'good' and 'great'. A big change is that these are now
 * independent: you can have ego items that don't have extra mundane bonuses
 * (+att, +evn, +sides...)
 */
void apply_magic(object_type *o_ptr, int lev, bool okay, bool good, bool great, bool allow_insta)
{
	int i, artefact_rolls;
	
	bool fine = FALSE;
	bool special = FALSE;

	/* Maximum "level" for various things */
	if (lev > MAX_DEPTH - 1) lev = MAX_DEPTH - 1;
		
	/* Roll for "fine" */
	if (percent_chance(lev*2))	fine = TRUE;
	
	/* Roll for "special" */
	if (percent_chance(lev*2))	special = TRUE;

	/* guarantee "fine" or "special" for "good" drops */
	if (good)
	{
		if (one_in_(2))	fine = TRUE;
		else			special = TRUE;
	}

	/* guarantee "fine" and "special" for "great" drops */
	if (great)
	{
		fine = TRUE;
		special = TRUE;
	}
	
	/* Assume no rolls */
	artefact_rolls = 0;

	/* Get 2 rolls if special */
	if (special) artefact_rolls = 2;

	/* Get 8 rolls if good and great are both set */
	if ((good) && (great)) artefact_rolls = 8;

	/* Get no rolls if not allowed */
	if (!okay || o_ptr->name1) artefact_rolls = 0;

	/* Roll for artefacts if allowed */
	for (i = 0; i < artefact_rolls; i++)
	{
		/* Roll for an artefact */
		if (make_artefact(o_ptr, allow_insta)) break;
	}

	/* Hack -- analyze artefacts */
	if (o_ptr->name1)
	{
		artefact_type *a_ptr = &a_info[o_ptr->name1];

		/* Hack -- Mark the artefact as "created" */
		a_ptr->cur_num = 1;

		object_into_artefact(o_ptr, a_ptr);

		/* Mega-Hack -- increase the rating */
		rating += 10;

		/* Set the good item flag */
		good_item_flag = TRUE;

		/* Cheat -- peek at the item */
		if (cheat_peek) object_mention(o_ptr);

		// pseudo-id the item
		pseudo_id(o_ptr);
		
		// keep count of artefacts generated (not including insta-arts)
		if (!(a_ptr->flags3 & (TR3_INSTA_ART)))  p_ptr->artefacts++;
		
		/* Done */
		return;
	}


	/* Apply magic */
	switch (o_ptr->tval)
	{
		case TV_SWORD:
		case TV_DIGGING:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_BOW:
		{
			// deathblades don't have normal fine and special types
			if ((o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DEATHBLADE))
			{
				while (one_in_(2)) o_ptr->att++;
				break;
			}
			
			// deal with special items
			if (special)
			{
				int ego_power;

				ego_power = make_special_item(o_ptr, (bool)(good || great));
				
				// if we were unlucky enough to have no valid special types
				// then at least let it be a fine item
				if (ego_power == 0) fine = TRUE;
			}

			// deal with fine items
			if (fine)
			{
				a_m_aux_1(o_ptr, lev);
			}
			
			// deal with throwing items
			if ((k_info[o_ptr->k_idx].flags3 & (TR3_THROWING)) && !artefact_p(o_ptr))
			{
				// throwing items always have typical weight to help with stacking
				o_ptr->weight = k_info[o_ptr->k_idx].weight;
				
				// and often come in multiples
				if (one_in_(2))
				{
					o_ptr->number = rand_range(2,5);
				}
			}

			break;
		}
		case TV_ARROW:
		{
			// note that arrows can't be both fine and special (fine trumps)
			
			if (special)
			{
				int ego_power;

				ego_power = make_special_item(o_ptr, (bool)(good || great));
				if (o_ptr->number > 1) o_ptr->number /= 2;
				
			}

			else if (fine)
			{
				a_m_aux_1(o_ptr, lev);
				if (o_ptr->number > 1) o_ptr->number /= 2;
			}

			break;
		}

		case TV_MAIL:
		case TV_SOFT_ARMOR:
		case TV_SHIELD:
		case TV_HELM:
		case TV_CROWN:
		case TV_CLOAK:
		case TV_GLOVES:
		case TV_BOOTS:
		{
			if (special)
			{
				int ego_power;

				ego_power = make_special_item(o_ptr, (bool)(good || great));

				// if we were unlucky enough to have no valid special types
				// then at least let it be a fine item
				if (ego_power == 0) fine = TRUE;
			}

			if (fine)
			{
				a_m_aux_2(o_ptr, lev);
			}
	
			break;
		}

		case TV_RING:
		case TV_AMULET:
		{
			a_m_aux_3(o_ptr, lev);
			break;
		}

		case TV_LIGHT:
		{
			if (special)
			{
				make_special_item(o_ptr, (bool)(good || great));
			}

			/* Fuel it */
			a_m_aux_4(o_ptr, lev, fine, special);
			break;
		}

		default:
		{
			a_m_aux_4(o_ptr, lev, fine, special);
			break;
		}
	}


	/* Hack -- analyze special items */
	if (o_ptr->name2)
	{
		// apply all the bonuses for the given special item type
		object_into_special(o_ptr, lev, FALSE);
				
		/* Done */
		return;
	}


	/* Examine real objects */
	if (o_ptr->k_idx)
	{
		object_kind *k_ptr = &k_info[o_ptr->k_idx];

		/* Hack -- acquire "broken" flag */
		if (!k_ptr->cost) o_ptr->ident |= (IDENT_BROKEN);

		/* Hack -- acquire "cursed" flag */
		if (k_ptr->flags3 & (TR3_LIGHT_CURSE)) o_ptr->ident |= (IDENT_CURSED);

		// identify non-special non-artefact weapons/armour		
		switch (o_ptr->tval)
		{
			case TV_DIGGING:
			case TV_HAFTED:
			case TV_POLEARM:
			case TV_SWORD:
			case TV_BOW:
			case TV_ARROW:
			case TV_MAIL:
			case TV_SOFT_ARMOR:
			case TV_SHIELD:
			case TV_HELM:
			case TV_CROWN:
			case TV_CLOAK:
			case TV_GLOVES:
			case TV_BOOTS:
			case TV_LIGHT:
			{
				/* Identify it */
				object_aware(o_ptr);
				object_known(o_ptr);
			}
		}
				
	}
}



/*
 * Hack -- determine if a template is "great".
 *
 * Note that this test only applies to the object *kind*, so it is
 * possible to choose a kind which is "great", and then later cause
 * the actual object to be cursed.  We do explicitly forbid objects
 * which are known to be boring or which start out somewhat damaged.
 */
static bool kind_is_great(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Armor -- great */
		case TV_MAIL:
		case TV_SOFT_ARMOR:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		{
			return (TRUE);
		}

		/* Weapons -- great */
		case TV_BOW:
		case TV_SWORD:
		case TV_HAFTED:
		case TV_POLEARM:
		{
			return (TRUE);
		}

		/* Chests -- great */
		case TV_CHEST:
		{
			return (TRUE);
		}

	}

	/* Assume not great */
	return (FALSE);
}

/*
 * Hack -- determine if a template is not a useless item
 *
 */
static bool kind_is_not_useless(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];
	
	/* Analyze the item type */
	switch (k_ptr->tval)
	{
			
		/* Useless -- Bad */
		case TV_USELESS:
		{
			return (FALSE);
		}
			
	}
	
	/* Assume good */
	return (TRUE);
}


/*
 * Hack -- determine if a template is a chest.
 *
 */
static bool kind_is_chest(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Chests -- */
		case TV_CHEST:
		{
			return (TRUE);
		}

	}

	/* Assume not chest */
	return (FALSE);
}

/*
 * Hack -- determine if a template is footwear.
 *
 */
static bool kind_is_boots(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* footwear -- */
		case TV_BOOTS:
		{
			return (TRUE);
		}

	}

	/* Assume not footwear */
	return (FALSE);
}

/*
 * Hack -- determine if a template is headgear.
 *
 */
static bool kind_is_headgear(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Headgear -- Suitable */
		case TV_HELM:
		case TV_CROWN:
		{
			return (TRUE);
		}
	}

	/* Assume not headgear */
	return (FALSE);
}

/*
 * Hack -- determine if a template is armor.
 *
 */
static bool kind_is_armor(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Armor -- suitable */
		case TV_MAIL:
		case TV_SOFT_ARMOR:
		{
			return (TRUE);
		}
	}

	/* Assume not armor */
	return (FALSE);
}

/*
 * Hack -- determine if a template is gloves.
 *
 */
static bool kind_is_gloves(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Gloves -- suitable */
		case TV_GLOVES:
		{
			return (TRUE);
		}
	}

	/* Assume not suitable  */
	return (FALSE);
}

/*
 * Hack -- determine if a template is a cloak.
 *
 */
static bool kind_is_cloak(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Cloaks -- suitable */

		case TV_CLOAK:
		{
			return (TRUE);
		}
	}

	/* Assume not a suitable  */
	return (FALSE);
}

/*
 * Hack -- determine if a template is a shield.
 *
 */
static bool kind_is_shield(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* shield -- suitable */
		case TV_SHIELD:
		{
			return (TRUE);
		}

	}

	/* Assume not suitable */
	return (FALSE);
}

/*
 * Hack -- determine if a template is a bow/arrow.
 */

static bool kind_is_bow(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{

		/* All bows and arrows are suitable  */
		case TV_BOW:
		{
			return (TRUE);
		}

		/*hack - don't allow arrow as a randart*/
		case TV_ARROW:
		{
			if (object_generation_mode == OB_GEN_MODE_RANDART)  return (FALSE);
			return (TRUE);

		}

	}

	/* Assume not suitable  */
	return (FALSE);
}



/*
 * Hack -- determine if a template is a "good" digging tool
 *
 */
static bool kind_is_digging_tool(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{

		/* Diggers -- Good */
		case TV_DIGGING:
		{
			return (TRUE);
		}

	}

	/* Assume not good */
	return (FALSE);
}

/*
 * Hack -- determine if a template is a edged weapon.
 */
static bool kind_is_edged(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Edged Weapons -- suitable */
		case TV_SWORD:
		{
			return (TRUE);
		}
	}

	/* Assume not suitable */
	return (FALSE);
}

/*
 * Hack -- determine if a template is a polearm.
 */
static bool kind_is_polearm(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Weapons -- suitable */
		case TV_POLEARM:
		{
			return (TRUE);
		}
	}

	/* Assume not suitable */
	return (FALSE);
}

/*
 * Hack -- determine if a template is a weapon.
 */
static bool kind_is_weapon(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Weapons -- suitable */
		case TV_SWORD:
		case TV_HAFTED:
		case TV_POLEARM:
		{
			return (TRUE);
		}
	}

	/* Assume not suitable */
	return (FALSE);
}


/*
 * Hack -- determine if a potion is good for a chest.
 * includes herb of restoring
 *
 */
static bool kind_is_potion(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{

		/*potions suitable for a chest*/
		case TV_POTION:
		{
			if (k_ptr->sval == SV_POTION_QUICKNESS) return (TRUE);
			if (k_ptr->sval == SV_POTION_MIRUVOR) return (TRUE);
			if (k_ptr->sval == SV_POTION_HEALING) return (TRUE);
			return (FALSE);
		}

		case TV_FOOD:
		/* HACK -  herbs of restoring can be with potions */
		{
			if ((k_ptr->sval == SV_FOOD_RESTORATION) &&
				((k_ptr->level + 5) >= object_level )) return (TRUE);
			return (FALSE);
		}

	}

	/* Assume not suitable */
	return (FALSE);
}

/*
 * Hack -- determine if a staff is good for a chest.
 *
 */
static bool kind_is_staff(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	if (k_ptr->tval == TV_STAFF)
	{
		/*staffs suitable for a chest*/
		if (k_ptr->sval == SV_STAFF_UNDERSTANDING) return (TRUE);
		if (k_ptr->sval == SV_STAFF_TREASURES) return (TRUE);
		if (k_ptr->sval == SV_STAFF_SLUMBER) return (TRUE);
		if (k_ptr->sval == SV_STAFF_WARDING) return (TRUE);
		if (k_ptr->sval == SV_STAFF_RECHARGING) return (TRUE);
	}

	/* Assume not suitable for a chest */
	return (FALSE);
}

/*
 * Hack -- determine if a template is "jewelry for chests".
 *
 */
static bool kind_is_jewelry(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Crowns are suitable for a chest */
		case TV_CROWN:
		{
			return (TRUE);
		}

		/* Sceptres are suitable for a chest */
		case TV_HAFTED:
		{
			if (k_ptr->sval == SV_SCEPTRE) return (TRUE);
			return (FALSE);
		}
			
		/*  Artefact rings are suitable for a chest */
		case TV_RING:
		{
			if (k_ptr->sval == SV_RING_BARAHIR) return (TRUE);
			if (k_ptr->sval == SV_RING_MELIAN) return (TRUE);
			return (FALSE);
		}

		/*  Artefact amulets and Blessed Realm are suitable for a chest */
		case TV_AMULET:
		{
			if (k_ptr->sval == SV_AMULET_TINFANG_GELION) return (TRUE);
			if (k_ptr->sval == SV_AMULET_NIMPHELOS) return (TRUE);
			if (k_ptr->sval == SV_AMULET_ELESSAR) return (TRUE);
			if (k_ptr->sval == SV_AMULET_DWARVES) return (TRUE);
			if (k_ptr->sval == SV_AMULET_BLESSED_REALM) return (TRUE);
			return (FALSE);
		}
			
	}

	/* Assume not suitable for a chest */
	return (FALSE);
}





/*
 * Hack -- determine if a template is "good".
 *
 * Note that this test only applies to the object *kind*, so it is
 * possible to choose a kind which is "good", and then later cause
 * the actual object to be cursed.  We do explicitly forbid objects
 * which are known to be boring or which start out somewhat damaged.
 */
static bool kind_is_good(int k_idx)
{
	object_kind *k_ptr = &k_info[k_idx];

	/* Analyze the item type */
	switch (k_ptr->tval)
	{
		/* Armor -- Good */
		case TV_MAIL:
		case TV_SOFT_ARMOR:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		{
			return (TRUE);
		}

		/* Weapons -- Good */
		case TV_BOW:
		case TV_SWORD:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_DIGGING:
		{
			return (TRUE);
		}

		/* Arrows -- Good */
		case TV_ARROW:
		{
			return (TRUE);
		}

		/* Rings -- Rings of Speed are good */
		case TV_RING:
		{
			return (FALSE);
		}

		/*the very powerful healing potions can be good*/
		case TV_POTION:
		{
			if (k_ptr->sval == SV_POTION_MIRUVOR) return (TRUE);
			if (k_ptr->sval == SV_POTION_QUICKNESS) return (TRUE);
			if (k_ptr->sval == SV_POTION_HEALING) return (TRUE);
			return (FALSE);
		}

		/* Chests -- Chests are good. */
		case TV_CHEST:
		{
			return (TRUE);
		}

	}

	/* Assume not good */
	return (FALSE);
}



/*
 * Attempt to make an object (normal or good/great)
 *
 * This routine plays nasty games to generate the "special artefacts".
 *
 * This routine uses "object_level" for the "generation level".
 *
 * We assume that the given object has been "wiped".
 */
bool make_object(object_type *j_ptr, bool good, bool great, int objecttype)
{
	int prob, base;
	bool generated_special = FALSE;

	/* Chance of "special object" */
	prob = ((good || great) ? 10 : 1000);

	/*better chance to check special artefacts if there is a jewelery theme*/
	if (objecttype == DROP_TYPE_JEWELRY) prob /= 2;

	/* Base level for the object */
	base = ((good || great) ? (object_level + 3) : object_level);

	// There is a one in prob chance of generating a "special artefact", such as Barahir
	if (one_in_(prob))
	{
		generated_special = make_artefact_special(j_ptr);
	}
	
	/* Attempt to generate a special artefact if prob = 0, or a normal object
	 * if not.
	 */
	if (!generated_special)
	{
		int k_idx;

		// unlike the others, this type can be overridden by 'great' and 'good'
		if (objecttype == DROP_TYPE_NOT_USELESS)				get_obj_num_hook = kind_is_not_useless;

		/*
		 * Next check if it is a themed drop, and
		 * only include objects from a pre-set theme.  But, it can be
		 * called from anywhere.
		 * First check to skip all these checks when unnecessary.
		 */
		 if ((good) || (great) || (objecttype > DROP_TYPE_NOT_USELESS))
		{
			if (objecttype == DROP_TYPE_POTION)						get_obj_num_hook = kind_is_potion;
			else if (objecttype == DROP_TYPE_STAFF)					get_obj_num_hook = kind_is_staff;
			else if (objecttype == DROP_TYPE_SHIELD) 				get_obj_num_hook = kind_is_shield;
			else if (objecttype == DROP_TYPE_WEAPON) 				get_obj_num_hook = kind_is_weapon;
			else if (objecttype == DROP_TYPE_ARMOR) 				get_obj_num_hook = kind_is_armor;
			else if (objecttype == DROP_TYPE_BOOTS) 				get_obj_num_hook = kind_is_boots;
			else if (objecttype == DROP_TYPE_BOW) 					get_obj_num_hook = kind_is_bow;
			else if (objecttype == DROP_TYPE_CLOAK)					get_obj_num_hook = kind_is_cloak;
			else if (objecttype == DROP_TYPE_GLOVES)				get_obj_num_hook = kind_is_gloves;
			else if (objecttype == DROP_TYPE_EDGED)					get_obj_num_hook = kind_is_edged;
			else if (objecttype == DROP_TYPE_POLEARM)				get_obj_num_hook = kind_is_polearm;
			else if (objecttype == DROP_TYPE_HEADGEAR)				get_obj_num_hook = kind_is_headgear;
			else if (objecttype == DROP_TYPE_JEWELRY)				get_obj_num_hook = kind_is_jewelry;
			else if (objecttype == DROP_TYPE_CHEST)					get_obj_num_hook = kind_is_chest;

			/*
			 *	If it isn't a chest, check good and great flags.
			 *  They each now have their own templates.
			 */
			else if (great)	get_obj_num_hook = kind_is_great;
			else if (good)	get_obj_num_hook = kind_is_good;
		}

		/* Prepare allocation table if needed*/
		if ((objecttype) || (good) || (great))
		{
			get_obj_num_prep();
		}

		/* Pick a random object */
		k_idx = get_obj_num(base);

		/* Clear the objects template*/
		if ((objecttype) ||	(good) || (great))
		{
			/* Clear restriction */
			get_obj_num_hook = NULL;

			/* Prepare allocation table */
			get_obj_num_prep();
		}


		/* Handle failure*/
		if (!k_idx) return (FALSE);

		/* Prepare the object */
		object_prep(j_ptr, k_idx);

	}

	/* Hack -- generate multiple arrows or pieces of mithril */
	switch (j_ptr->tval)
	{
		case TV_ARROW:
		{
			if (one_in_(3))
			{
				j_ptr->number = damroll(4, 6);
			}
			else
			{
				// 3/6 chance of 12, 2/6 chance of 24, 1/6 chance of 36
				j_ptr->number = 12;
				
				if (one_in_(2))
				{ 
					j_ptr->number += 12;
					if (one_in_(3)) j_ptr->number += 12;
				}
				
			}
		}
		
		case TV_METAL:
		{
			j_ptr->number = damroll(2, 40);
		}
	}

	/* Apply magic (allow artefacts) */
	if (generated_special)
	{
		// allow INSTA_ARTs
		apply_magic(j_ptr, object_level, TRUE, good, great, TRUE);	
	}
	else
	{
		// don't allow INSTA_ARTs
		apply_magic(j_ptr, object_level, TRUE, good, great, FALSE);
	}

	// apply the autoinscription (if any)
	apply_autoinscription(j_ptr);
	
	/* Notice "okay" out-of-depth objects */
	if (!cursed_p(j_ptr) && !broken_p(j_ptr) &&
	    (k_info[j_ptr->k_idx].level > p_ptr->depth))
	{
		/* Rating increase */
		rating += (k_info[j_ptr->k_idx].level - p_ptr->depth);

		/* Cheat -- peek at items */
		if (cheat_peek) object_mention(j_ptr);
	}

	/* Success */
	return (TRUE);
}


/*
 * Set the object theme
 */


/*
 * This is an imcomplete list of themes.  Returns false if theme not found.
 * Used primarily for Randarts
 */
bool prep_object_theme(int themetype)
{
	/*get the store creation mode*/
	switch (themetype)
	{
		case DROP_TYPE_SHIELD:
		{
			get_obj_num_hook = kind_is_shield;
			break;
		}
		case DROP_TYPE_WEAPON:
		{
			get_obj_num_hook = kind_is_weapon;
			break;
		}
		case DROP_TYPE_EDGED:
		{
			get_obj_num_hook = kind_is_edged;
			break;
		}
		case DROP_TYPE_POLEARM:
		{
			get_obj_num_hook = kind_is_polearm;
			break;
		}
		case DROP_TYPE_ARMOR:
		{
			get_obj_num_hook = kind_is_armor;
			break;
		}
		case DROP_TYPE_BOOTS:
		{
			get_obj_num_hook = kind_is_boots;
			break;
		}
		case DROP_TYPE_BOW:
		{
			get_obj_num_hook = kind_is_bow;
			break;
		}
		case DROP_TYPE_CLOAK:
		{
			get_obj_num_hook = kind_is_cloak;
			break;
		}
		case DROP_TYPE_GLOVES:
		{
			get_obj_num_hook = kind_is_gloves;
			break;
		}
		case DROP_TYPE_HEADGEAR:
		{
			get_obj_num_hook = kind_is_headgear;
			break;
		}
		case DROP_TYPE_DIGGING:
		{
			get_obj_num_hook = kind_is_digging_tool;

			break;
		}

		default: return (FALSE);
	}

	/*prepare the allocation table*/
	get_obj_num_prep();

	return(TRUE);

}


/*
 * Let the floor carry an object
 */
s16b floor_carry(int y, int x, object_type *j_ptr)
{
	int n = 0;

	s16b o_idx;

	s16b this_o_idx, next_o_idx = 0;


	/* Scan objects in that grid for combination */
	for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
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

		/* Count objects */
		n++;
	}

	/* The stack is already too large */
	if (n > MAX_FLOOR_STACK) return (0);

	// Sil: force no stacking
	if (n) return (0);

	/* Make an object */
	o_idx = o_pop();

	/* Success */
	if (o_idx)
	{
		object_type *o_ptr;

		/* Get the object */
		o_ptr = &o_list[o_idx];

		/* Structure Copy */
		object_copy(o_ptr, j_ptr);

		/* Location */
		o_ptr->iy = y;
		o_ptr->ix = x;

		/* Forget monster */
		o_ptr->held_m_idx = 0;

		/* Link the object to the pile */
		o_ptr->next_o_idx = cave_o_idx[y][x];

		/* Link the floor to the object */
		cave_o_idx[y][x] = o_idx;

		/* Rearrange to reflect squelching */
		rearrange_stack(y, x);

		/* Notice */
		note_spot(y, x);

		/* Redraw */
		lite_spot(y, x);
	}

	/* Result */
	return (o_idx);
}


/*
 * Let an object fall to the ground at or near a location.
 *
 * The initial location is assumed to be "in_bounds_fully()".
 *
 * This function takes a parameter "chance".  This is the percentage
 * chance that the item will "disappear" instead of drop.  If the object
 * has been thrown, then this is the chance of disappearance on contact.
 *
 * Hack -- this function uses "chance" to determine if it should produce
 * some form of "description" of the drop event (under the player).
 *
 * We check several locations to see if we can find a location at which
 * the object can combine, stack, or be placed.  Artefacts will try very
 * hard to be placed, including "teleporting" to a useful grid if needed.
 */
void drop_near(object_type *j_ptr, int chance, int y, int x)
{
	int i, k, d, s;

	int bs, bn;
	int by, bx;
	int dy, dx;
	int ty, tx;

	object_type *o_ptr;

	char o_name[80];

	bool flag = FALSE;

	bool plural = FALSE;


	/* Extract plural */
	if (j_ptr->number != 1) plural = TRUE;

	/* Describe object */
	object_desc(o_name, sizeof(o_name), j_ptr, FALSE, 0);
    
	/* Handle normal "breakage" */
	if (!artefact_p(j_ptr) && percent_chance(chance))
	{
		// The potion breaking message has already been displayed
		if (j_ptr->tval != TV_POTION)
		{
			/* Message */
			msg_format("The %s break%s.",
					   o_name, (plural ? "" : "s"));
		}

		/* Debug */
		//if (p_ptr->wizard) msg_print("Breakage (breakage).");

		/* Failure */
		return;
	}


	/* Score */
	bs = -1;

	/* Picker */
	bn = 0;

	/* Default */
	by = y;
	bx = x;

	/* Scan local grids */
	for (dy = -3; dy <= 3; dy++)
	{
		/* Scan local grids */
		for (dx = -3; dx <= 3; dx++)
		{
			bool comb = FALSE;
            ////int path_n;
            ////u16b path_g[256];
            ////int ty2, tx2; // store a copy of the target grid that can get changed by project_path()

			/* Calculate actual distance */
			d = (dy * dy) + (dx * dx);

			/* Ignore distant grids */
			if (d > 10) continue;

			/* Location */
			ty = y + dy;
			tx = x + dx;
            
            // copy of the variables
            ////ty2 = ty;
            ////tx2 = tx;

			/* Skip illegal grids */
			if (!in_bounds_fully(ty, tx)) continue;

			/* Require line of sight */
			if (!los(y, x, ty, tx)) continue;
            
            /* Calculate the path */
            ////path_n = project_path(path_g, 10, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_NO_CHASM);
            
            // if there was a chasm in the way, skip this spot
            ////if ((ty != ty2) || (tx != tx2)) continue;
            
			/* Require floor space */
			if (cave_feat[ty][tx] != FEAT_FLOOR) continue;

			/* No objects */
			k = 0;

			/* Scan objects in that grid */
			for (o_ptr = get_first_object(ty, tx); o_ptr; o_ptr = get_next_object(o_ptr))
			{
				/* Check for possible combination */
				if (object_similar(o_ptr, j_ptr)) comb = TRUE;

				/* Count objects */
				k++;
			}

			/* Add new object */
			if (!comb) k++;

			// Sil: force no stacking
			if (k > 1) continue;

			/* Paranoia */
			if (k > MAX_FLOOR_STACK) continue;

			/* Calculate score */
			s = 1000 - (d + k * 5);

			/* Skip bad values */
			if (s < bs) continue;

			/* New best value */
			if (s > bs) bn = 0;

			/* Apply the randomizer to equivalent values */
			if ((++bn >= 2) && (rand_int(bn) != 0)) continue;

			/* Keep score */
			bs = s;

			/* Track it */
			by = ty;
			bx = tx;

			/* Okay */
			flag = TRUE;
		}
	}


	/* Handle lack of space */
	if (!flag && (j_ptr->name1 != ART_MORGOTH_3) && ((j_ptr->tval != TV_LIGHT) || (j_ptr->sval != SV_LIGHT_SILMARIL)))
	{
		/* Message */
		if (player_has_los_bold(y,x))
		{
			msg_format("The %s disappear%s.", o_name, (plural ? "" : "s"));
		}

		/* Debug */
		if (p_ptr->wizard) msg_print("Breakage (no floor space).");

		/* Failure */
		return;
	}

	/* Prepared to clobber other items in order to place the crown or silmarils */
	for (i = 0; !flag; i++)
	{
		/* First try */
		if (i == 0)
		{
			ty = y;
			tx = x;
		}

		/* Bounce around */
		else if (i < 100)
		{
			ty = rand_range(by - 1, by + 1);
			tx = rand_range(bx - 1, bx + 1);
		}

		/* Get deperate and teleport it somewhere*/
		else
		{
			ty = rand_int(p_ptr->cur_map_hgt);
			tx = rand_int(p_ptr->cur_map_wid);
		}

		/* Require floor space */
		if (cave_feat[ty][tx] != FEAT_FLOOR) continue;
		
		/* Bounce to that location */
		by = ty;
		bx = tx;
		
		// Clear it if needed
		if (cave_o_idx[ty][tx] != 0)
		{
			object_type *o_ptr = &o_list[cave_o_idx[ty][tx]];
			if (((o_ptr->name1 < ART_MORGOTH_0) || (o_ptr->name1 > ART_MORGOTH_3)) && 
			    ((o_ptr->tval != TV_LIGHT) || (o_ptr->sval != SV_LIGHT_SILMARIL)))
			{
				/* Delete the object */
				delete_object_idx(cave_o_idx[ty][tx]);
			}
		}

		/* Require floor space */
		if (!cave_clean_bold(by, bx)) continue;
		
		/* Okay */
		flag = TRUE;
	}


	/* Give it to the floor */
	if (!floor_carry(by, bx, j_ptr))
	{
		/* Message */
		if (player_has_los_bold(y,x))
		{
			msg_format("The %s disappear%s.", o_name, (plural ? "" : "s"));
		}

		/* Debug */
		if (p_ptr->wizard) msg_print("Breakage (too many objects).");

		/* Failure */
		return;
	}

	// make sure to trigger the lore master ability
	update_stuff();

	/* Sound */
	sound(MSG_DROP);

	/* Mega-Hack -- no message if "dropped" by player */
	/* Message when an object falls under the player */
	if (chance && (cave_m_idx[by][bx] < 0))
	{
		msg_print("You feel something roll beneath your feet.");
	}
}


/*
 * Scatter some "great" objects near the player
 */
void acquirement(int y1, int x1, int num, bool great)
{
	object_type *i_ptr;
	object_type object_type_body;

	/* Acquirement */
	while (num--)
	{
		/* Get local object */
		i_ptr = &object_type_body;

		/* Wipe the object */
		object_wipe(i_ptr);

		/* Make a good (or great) object (if possible) */
		if (!make_object(i_ptr, TRUE, great, DROP_TYPE_NOT_USELESS)) continue;

		/* Drop the object */
		drop_near(i_ptr, -1, y1, x1);
	}
}


/*
 * Attempt to place an object (normal or good/great) at the given location.
 */
void place_object(int y, int x, bool good, bool great, int droptype)
{
	object_type *i_ptr;
	object_type object_type_body;

	/* Paranoia */
	if (!in_bounds(y, x)) return;

	/* Hack -- clean floor space */
	if (!cave_clean_bold(y, x)) return;

	/* Get local object */
	i_ptr = &object_type_body;

	/* Wipe the object */
	object_wipe(i_ptr);

	/* Make an object (if possible) */
	while (!make_object(i_ptr, good, great, droptype)) continue;

	/* Give it to the floor */
	if (!floor_carry(y, x, i_ptr))
	{
		/* Hack -- Preserve artefacts */
		a_info[i_ptr->name1].cur_num = 0;
	}
}

/*
 * Choose a trap type, place it in the dungeon at the given grid and 'hide' it
 *
 */
void place_trap(int y, int x)
{
	int feat;

	/* Paranoia */
	if (!in_bounds(y, x)) return;
	
	/* Require empty, clean, floor grid */
	if (!cave_naked_bold(y, x)) return;
	
	/* Pick a trap */
	while (1)
	{
		/* Hack -- pick a trap */
		feat = rand_range(FEAT_TRAP_HEAD, FEAT_TRAP_TAIL);

		switch (feat)
		{
			case FEAT_TRAP_FALSE_FLOOR:
			{
				// 5-18
				if (p_ptr->depth < 5) continue;
				if (p_ptr->depth > 18) continue;

				// skip half the time as they are otherwise too common
				if (one_in_(2)) continue;
				break;
			}
			case FEAT_TRAP_PIT:
			{
				// 5-10
				if (p_ptr->depth < 5) continue;
				if (p_ptr->depth > 10) continue;
				break;
			}
			case FEAT_TRAP_SPIKED_PIT:
			{
				// 0, 11-17
				if (p_ptr->depth == 0) break;
				if (p_ptr->depth < 11) continue;
				if (p_ptr->depth > 17) continue;
				break;
			}
			case FEAT_TRAP_DART:
			{
				// 8-15
				if (p_ptr->depth < 8) continue;
				if (p_ptr->depth > 15) continue;
				break;
			}
			case FEAT_TRAP_GAS_CONF:
			{
				// 1-13
				if (p_ptr->depth < 1) continue;
				if (p_ptr->depth > 13) continue;
				break;
			}
			case FEAT_TRAP_GAS_MEMORY:
			{
                // removed these for now due to player frustration
                continue;
                
 				// 14-
               
				//if (p_ptr->depth < 14) continue;
				//break;
			}
			case FEAT_TRAP_ALARM:
			{
				// 0-
				break;
			}
			case FEAT_TRAP_FLASH:
			{
				// 1-
				if (p_ptr->depth < 1) continue;
				break;
			}
			case FEAT_TRAP_CALTROPS:
			{
				// 0-
				break;
			}
			case FEAT_TRAP_ROOST:
			{
				// 0, 3-6
				if (p_ptr->depth == 0) break;
				if (p_ptr->depth < 3) continue;
				if (p_ptr->depth > 6) continue;
				break;
			}
			case FEAT_TRAP_WEB:
			{
				int d, dir, floor_count = 0;

				// 8-
				if (p_ptr->depth < 8) continue;
				
				// make sure there are at least two adjacent floor squares
				for (d = 0; d < 8; d++)
				{
					dir = cycle[d];
					
					if (cave_floor_bold(y + ddy[dir], x + ddx[dir])) floor_count++;
				}
				if (floor_count < 2) continue;
				
				break;
			}
			case FEAT_TRAP_DEADFALL:
			{
				// 0, 14-
				if (p_ptr->depth == 0) break;
				if (p_ptr->depth < 14) continue;
				break;
			}
			case FEAT_TRAP_ACID:
			{
				// 1-
				if (p_ptr->depth < 1) continue;
				break;
			}
		}

		/* Done */
		break;
	}

	/* Activate the trap */
	cave_set_feat(y, x, feat);
	
	// Hide the trap
	cave_info[y][x] |= (CAVE_HIDDEN);
}

/*
 *  Reveal a trap and mark its location on the map.
 */
void reveal_trap(int y, int x)
{
	// remove the 'hidden' flag from the grid
	cave_info[y][x] &= ~(CAVE_HIDDEN);
	
	/* Notice/Redraw */
	if (character_dungeon)
	{
		/* Notice */
		note_spot(y, x);
		
		/* Hack -- Memorize */
		cave_info[y][x] |= (CAVE_MARK);
		
		/* Redraw */
		lite_spot(y, x);
	}
}


/*
 * Place a secret door at the given location
 */
void place_secret_door(int y, int x)
{
	/* Create secret door */
	cave_set_feat(y, x, FEAT_SECRET);
}


/*
 * Place a random type of closed door at the given location.
 */
void place_closed_door(int y, int x)
{
	int tmp, power;

	/* Choose an object */
	tmp = rand_int(100);

	// vault generation
	if (cave_info[y][x] & (CAVE_ICKY))
	{
		/* Closed doors (88%) */
		if (tmp < 88)
		{
			/* Create closed door */
			cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
		}

		/* Locked doors (8%) */
		else if (tmp < 96)
		{
			/* Create locked door */
			power = (10 + p_ptr->depth + dieroll(15)) / 5;
			power = MIN(7, power);
			cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
		}

		/* Jammed doors (4%) */
		else
		{
			/* Create jammed door */
			power = (10 + p_ptr->depth + dieroll(15)) / 5;
			power = MIN(7, power);
			cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
		}
	}
	
	// normal generation
	else
	{
		/* Closed doors (75%) */
		if (tmp < 75)
		{
			/* Create closed door */
			cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
		}

		/* Locked doors (24%) */
		else if (tmp < 99)
		{
			/* Create locked door */
			power = (p_ptr->depth + dieroll(15)) / 5;
			power = MIN(7, power);
			cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
		}

		/* Stuck doors (1%) */
		else
		{
			/* Create jammed door */
			power = (p_ptr->depth + dieroll(15)) / 5;
			power = MIN(7, power);
			cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
		}
	}
}


/*
 * Place a random type of door at the given location.
 */
void place_random_door(int y, int x)
{
	int tmp;

	/* Choose an object */
	tmp = rand_int(60 + p_ptr->depth);

	/* Open doors */
	if (tmp < 20)
	{
		/* Create open door */
		cave_set_feat(y, x, FEAT_OPEN);
	}

	/* Closed, locked, or stuck doors */
	else if (tmp < 60)
	{
		/* Create closed door */
		place_closed_door(y, x);
	}

	/* Secret doors */
	else 
	{
		/* Create secret door */
		cave_set_feat(y, x, FEAT_SECRET);
	}

}


/*
 * Place a random type of forge at the given location.
 */
void place_forge(int y, int x)
{
	int uses, power, p, effective_depth, i;
	
	effective_depth = p_ptr->depth;
	
	if (cave_info[y][x] & (CAVE_G_VAULT))
	{
		effective_depth *= 2;
	}
	
	power = 1;
	
	// roll once per level of depth and keep the best roll
	for (i = 0; i < effective_depth; i++)
	{
		p = dieroll(1000);
		
		power = MAX(power, p);
	}

	uses = damroll(2,2);

	// to prevent start-scumming on the initial forge
	if (p_ptr->depth <= 2)
	{
		uses = 3;
		power = 0;
	}
	
	// unique forge
	if ((power >= 1000) && !p_ptr->unique_forge_made) 
	{
		uses = 3;
		cave_set_feat(y, x, FEAT_FORGE_UNIQUE_HEAD + uses);
		
		p_ptr->unique_forge_made = TRUE;

		if (cheat_room) msg_print("Orodruth.");
	}
	
	// enchanted forge
	else if (power >= 990) 
	{
		cave_set_feat(y, x, FEAT_FORGE_GOOD_HEAD + uses);
		if (cheat_room) msg_print("Enchanted forge.");
	}
	
	// normal forge
	else
	{
		cave_set_feat(y, x, FEAT_FORGE_NORMAL_HEAD + uses);
		if (cheat_room) msg_print("Forge.");
	}
	
}



/*
 * Describe the charges on an item in the inventory.
 */
void inven_item_charges(int item)
{
	object_type *o_ptr = &inventory[item];

	/* Require staff */
	if (o_ptr->tval != TV_STAFF) return;

	/* Require known item */
	if (!object_known_p(o_ptr)) return;

	/* Print a message */
	msg_format("You have %d charge%s remaining.", o_ptr->pval,
	           (o_ptr->pval != 1) ? "s" : "");
}


/*
 * Describe an item in the inventory.
 */
void inven_item_describe(int item)
{
	object_type *o_ptr = &inventory[item];

	char o_name[80];

	if (artefact_p(o_ptr) && object_known_p(o_ptr))
	{
		/* Get a description */
		object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 3);

		/* Print a message */
		msg_format("You no longer have the %s (%c).", o_name, index_to_label(item));
	}
	else
	{
		/* Get a description */
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

		/* Print a message */
		msg_format("You have %s (%c).", o_name, index_to_label(item));
	}
}


/*
 * Increase the "number" of an item in the inventory
 */
void inven_item_increase(int item, int num)
{
	object_type *o_ptr = &inventory[item];

	/* Apply */
	num += o_ptr->number;

	/* Bounds check */
	if (num > 255) num = 255;
	else if (num < 0) num = 0;

	/* Un-apply */
	num -= o_ptr->number;

	/* Change the number and weight */
	if (num)
	{
		/* Add the number */
		o_ptr->number += num;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Recalculate mana XXX */
		p_ptr->update |= (PU_MANA);

		/* Combine the pack */
		p_ptr->notice |= (PN_COMBINE);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN | PW_EQUIP);
	}
}


/*
 * Erase an inventory slot if it has no more items
 */
void inven_item_optimize(int item)
{
	object_type *o_ptr = &inventory[item];

	/* Only optimize real items */
	if (!o_ptr->k_idx) return;

	/* Only optimize empty items */
	if (o_ptr->number) return;

	/* The item is in the pack */
	if (item < INVEN_WIELD)
	{
		int i;

		/* One less item */
		p_ptr->inven_cnt--;

		/* Slide everything down */
		for (i = item; i < INVEN_PACK; i++)
		{
			/* Hack -- slide object */
			COPY(&inventory[i], &inventory[i+1], object_type);
		}

		/* Hack -- wipe hole */
		(void)WIPE(&inventory[i], object_type);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN);
	}

	/* The item is being wielded */
	else
	{
		/* One less item */
		p_ptr->equip_cnt--;

		/* Erase the empty slot */
		object_wipe(&inventory[item]);

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Recalculate mana XXX */
		p_ptr->update |= (PU_MANA);

		/* Window stuff */
		p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

		p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
	}
}


/*
 * Describe the charges on an item on the floor.
 */
void floor_item_charges(int item)
{
	object_type *o_ptr = &o_list[item];

	/* Require staff */
	if (o_ptr->tval != TV_STAFF) return;

	/* Require known item */
	if (!object_known_p(o_ptr)) return;

	/* Print a message */
	msg_format("There are %d charge%s remaining.", o_ptr->pval,
	           (o_ptr->pval != 1) ? "s" : "");
}



/*
 * Describe an item on the floor.
 */
void floor_item_describe(int item)
{
	object_type *o_ptr = &o_list[item];

	char o_name[80];

	/* Get a description */
	object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

	/* Print a message */
	msg_format("You see %s.", o_name);
	
}


/*
 * Increase the "number" of an item on the floor
 */
void floor_item_increase(int item, int num)
{
	object_type *o_ptr = &o_list[item];

	/* Apply */
	num += o_ptr->number;

	/* Bounds check */
	if (num > 255) num = 255;
	else if (num < 0) num = 0;

	/* Un-apply */
	num -= o_ptr->number;

	/* Change the number */
	o_ptr->number += num;
}


/*
 * Optimize an item on the floor (destroy "empty" items)
 */
void floor_item_optimize(int item)
{
	object_type *o_ptr = &o_list[item];

	/* Paranoia -- be sure it exists */
	if (!o_ptr->k_idx) return;

	/* Only optimize empty items */
	if (o_ptr->number) return;

	/* Delete the object */
	delete_object_idx(item);
}



/*
 *  overflow the player's backpack if needed
 */
void check_pack_overflow(void)
{
	if (inventory[INVEN_PACK].k_idx)
	{
		int item = INVEN_PACK;
		
		char o_name[80];
		
		object_type *o_ptr;
		
		/* Get the slot to be dropped */
		o_ptr = &inventory[item];
		
		/* Disturbing */
		disturb(0, 0);
		
		/* Warning */
		msg_print("Your pack overflows!");
		
		/* Describe */
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
		
		/* Message */
		msg_format("You drop %s (%c).", o_name, index_to_label(item));
		
		/* Drop it (carefully) near the player */
		drop_near(o_ptr, 0, p_ptr->py, p_ptr->px);
		
		/* Modify, Describe, Optimize */
		inven_item_increase(item, -255);
		inven_item_describe(item);
		inven_item_optimize(item);
		
		/* Notice stuff (if needed) */
		if (p_ptr->notice) notice_stuff();
		
		/* Update stuff (if needed) */
		if (p_ptr->update) update_stuff();
		
		/* Redraw stuff (if needed) */
		if (p_ptr->redraw) redraw_stuff();
		
		/* Window stuff (if needed) */
		if (p_ptr->window) window_stuff();
	}
	
}

/*
 * Check if we have space for an item in the pack without overflow
 */
bool inven_carry_okay(const object_type *o_ptr)
{
	int j;

	/* Empty slot? */
	if (p_ptr->inven_cnt < INVEN_PACK) return (TRUE);

	/* Similar slot? */
	for (j = 0; j < INVEN_PACK; j++)
	{
		object_type *j_ptr = &inventory[j];

		/* Skip non-objects */
		if (!j_ptr->k_idx) continue;

		/* Check if the two items can be combined */
		if (object_similar(j_ptr, o_ptr)) return (TRUE);
	}

	// Check for combining in quiver first
	if (o_ptr->tval == TV_ARROW)
	{
		int empty_quiver = 0;
		
		// arrows combine with similar arrows
		for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
		{
			object_type *j_ptr = &inventory[j];
			
			/* Skip non-objects */
			if (!j_ptr->k_idx)
			{
				// keep track of the first empty quiver
				if (empty_quiver == 0) empty_quiver = j;
				continue;
			}
			
			/* Check if the two items can be combined */
			if (object_similar(j_ptr, o_ptr))
			{
				return (TRUE);
			}
		}
		
		// arrows that have been fired can also fit back into an empty quiver slot
		if ((empty_quiver > 0) && o_ptr->pickup)
		{
			return (TRUE);
		}
	}
	
	/* Nope */
	return (FALSE);
}



/*
 * Add an item to the players inventory, and return the slot used.
 *
 * If the new item can combine with an existing item in the inventory,
 * it will do so, using "object_similar()" and "object_absorb()", else,
 * the item will be placed into the "proper" location in the inventory.
 *
 * This function can be used to "over-fill" the player's pack, but only
 * once, and such an action must trigger the "overflow" code immediately.
 * Note that when the pack is being "over-filled", the new item must be
 * placed into the "overflow" slot, and the "overflow" must take place
 * before the pack is reordered, but (optionally) after the pack is
 * combined.  This may be tricky.  See "dungeon.c" for info.
 *
 * Note that this code must remove any location/stack information
 * from the object once it is placed into the inventory.
 */
s16b inven_carry(object_type *o_ptr)
{
	int i = 1; // default value to soothe compilation warnings
	int j, k;
	int n = -1;
	
	object_type *j_ptr;

	/*paranoia, don't pick up "&nothings"*/
	if (!o_ptr->k_idx) return (-1);

	// Check for combining in quiver first
	if (o_ptr->tval == TV_ARROW)
	{
		int empty_quiver = 0;
				
		// arrows combine with similar arrows
		for (j = INVEN_QUIVER1; j <= INVEN_QUIVER2; j++)
		{
			j_ptr = &inventory[j];
			
			/* Skip non-objects */
			if (!j_ptr->k_idx)
			{
				// keep track of the first empty quiver
				if (empty_quiver == 0) empty_quiver = j;
				continue;
			}
			
			/* Check if the two items can be combined */
			if (object_similar(j_ptr, o_ptr))
			{
				/* Combine the items */
				object_absorb(j_ptr, o_ptr);
				
				/* Window stuff */
				p_ptr->window |= (PW_INVEN);
				
				if (o_ptr->number == 0)
				{
					/* Success */
					return (j);
				}
				else
				{
					char j_name[80];
					
					// combination message
					msg_print("You combine them with the arrows in your quiver.");

					/* Describe the object */
					object_desc(j_name, sizeof(j_name), j_ptr, TRUE, 3);
					
					/* Message */
					msg_format("You have %s (%c).", j_name, index_to_label(j));
				}
			}
		}
		
		// arrows that have been fired can also fit back into an empty quiver slot
		if ((empty_quiver > 0) && o_ptr->pickup)
		{
			o_ptr->pickup = FALSE;
			do_cmd_wield(o_ptr, -1);
			return (-1);
		}
	}

	/* Check for combining */
	for (j = 0; j < INVEN_PACK; j++)
	{
		j_ptr = &inventory[j];

		/* Skip non-objects */
		if (!j_ptr->k_idx) continue;

		/* Hack -- track last item */
		n = j;

		/* Check if the two items can be combined */
		if (object_similar(j_ptr, o_ptr))
		{
			/* Combine the items */
			object_absorb(j_ptr, o_ptr);

			/* Recalculate bonuses */
			p_ptr->update |= (PU_BONUS);

			/* Window stuff */
			p_ptr->window |= (PW_INVEN);

			if (o_ptr->number == 0)
			{
				/* Success */
				return (j);
			}
			else
			{
				char j_name[80];
				
				// combination message
				msg_print("You combine them with some items in your pack.");

				/* Describe the object */
				object_desc(j_name, sizeof(j_name), j_ptr, TRUE, 3);
				
				/* Message */
				msg_format("You have %s (%c).", j_name, index_to_label(j));
			}
		}
	}

	/* Paranoia */
	if (p_ptr->inven_cnt > INVEN_PACK) return (-1);

	/* Find an empty slot */
	for (j = 0; j <= INVEN_PACK; j++)
	{
		j_ptr = &inventory[j];
		
		/* Use it if found */
		if (!j_ptr->k_idx) break;
	}
	
	/* Use that slot */
	i = j;
	
	/* Apply an autoinscription */
	apply_autoinscription(o_ptr);
	
	/* Reset the pickup flag */
	o_ptr->pickup = FALSE;
	
	/* Reorder the pack */
	if (i < INVEN_PACK)
	{
		s32b o_value, j_value;
		
		/* Get the "value" of the item */
		o_value = object_value(o_ptr);
		
		/* Scan every occupied slot */
		for (j = 0; j < INVEN_PACK; j++)
		{
			j_ptr = &inventory[j];
			
			/* Use empty slots */
			if (!j_ptr->k_idx) break;
			
			/* Objects sort by decreasing type */
			if (o_ptr->tval > j_ptr->tval) break;
			if (o_ptr->tval < j_ptr->tval) continue;
			
			/* Non-aware (flavored) items always come last */
			if (!object_aware_p(o_ptr)) continue;
			if (!object_aware_p(j_ptr)) break;
			
			/* Objects sort by increasing sval */
			if (o_ptr->sval < j_ptr->sval) break;
			if (o_ptr->sval > j_ptr->sval) continue;
			
			/* Lites sort by decreasing fuel */
			if (o_ptr->tval == TV_LIGHT)
			{
				if (o_ptr->timeout > j_ptr->timeout) break;
				if (o_ptr->timeout < j_ptr->timeout) continue;
			}
			
			// This next bit is complicated: identified art > pseudo art > identified special > pseudo special > other
			
			/* Identified artefacts beat the rest */
			if (!(object_known_p(o_ptr) && artefact_p(o_ptr)) && (object_known_p(j_ptr) && artefact_p(j_ptr))) continue;
			if ((object_known_p(o_ptr) && artefact_p(o_ptr)) && !(object_known_p(j_ptr) && artefact_p(j_ptr))) break;
			
			/* Then pseudo-identified {artefact} */
			if (!(!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr)) && (!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr))) continue;
			if ((!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr)) && !(!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr))) break;
			
			/* Then identified specials */
			if (!(object_known_p(o_ptr) && ego_item_p(o_ptr)) && (object_known_p(j_ptr) && ego_item_p(j_ptr))) continue;
			if ((object_known_p(o_ptr) && ego_item_p(o_ptr)) && !(object_known_p(j_ptr) && ego_item_p(j_ptr))) break;
			
			/* Then pseudo-identified {special} */
			if (!(!object_known_p(o_ptr) && special_pseudo_p(o_ptr)) && (!object_known_p(j_ptr) && special_pseudo_p(j_ptr))) continue;
			if ((!object_known_p(o_ptr) && special_pseudo_p(o_ptr)) && !(!object_known_p(j_ptr) && special_pseudo_p(j_ptr))) break;
			
			/* Determine the "value" of the pack item */
			j_value = object_value(j_ptr);
			
			/* Objects sort by decreasing value */
			if (o_value > j_value) break;
			if (o_value < j_value) continue;
			
			/* Objects sort by increasing weight */
			if (o_ptr->weight < j_ptr->weight) break;
			if (o_ptr->weight > j_ptr->weight) continue;
		}
		
		/* Use that slot */
		i = j;
		
		/* Slide objects */
		for (k = n; k >= i; k--)
		{
			/* Hack -- Slide the item */
			object_copy(&inventory[k+1], &inventory[k]);
		}
		
		/* Wipe the empty slot */
		object_wipe(&inventory[i]);
	}
		
	/* Copy the item */
	object_copy(&inventory[i], o_ptr);

	/* Get the new object */
	j_ptr = &inventory[i];

	/* Forget stack */
	j_ptr->next_o_idx = 0;

	/* Forget monster */
	j_ptr->held_m_idx = 0;

	/* Forget location */
	j_ptr->iy = j_ptr->ix = 0;

	/* No longer marked */
	j_ptr->marked = FALSE;

	/* Count the items */
	p_ptr->inven_cnt++;

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Combine and Reorder pack */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);

	/* Window stuff */
	p_ptr->window |= (PW_INVEN);

	/* Return the slot */
	return (i);
}


/*
 * Take off (some of) a non-cursed equipment item
 *
 * Note that only one item at a time can be wielded per slot.
 *
 * Note that taking off an item when "full" may cause that item
 * to fall to the ground.
 *
 * Return the inventory slot into which the item is placed.
 */
s16b inven_takeoff(int item, int amt)
{
	int slot;

	object_type *o_ptr;

	object_type *i_ptr;
	object_type object_type_body;

	cptr act;

	char o_name[80];


	/* Get the item to take off */
	o_ptr = &inventory[item];

	/* Paranoia */
	if (amt <= 0) return (-1);

	/* Verify */
	if (amt > o_ptr->number) amt = o_ptr->number;

	/* Get local object */
	i_ptr = &object_type_body;

	/* Obtain a local object */
	object_copy(i_ptr, o_ptr);

	/* Modify quantity */
	i_ptr->number = amt;

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), i_ptr, TRUE, 3);

	/* Took off weapon */
	if ((item == INVEN_WIELD) || ((item == INVEN_ARM) && (i_ptr->tval != TV_SHIELD)))
	{
		act = "You were wielding";
	}

	/* Took off bow */
	else if (item == INVEN_BOW)
	{
		act = "You were holding";
	}

	/* Took off light */
	else if (item == INVEN_LITE)
	{
		act = "You were holding";
	}

	/* Took off arrows */
	else if ((item == INVEN_QUIVER1) || (item == INVEN_QUIVER2))
	{
		act = "You have removed from your quiver";
	}
	
	/* Took off something */
	else
	{
		act = "You were wearing";
	}

	/* Modify, Optimize */
	inven_item_increase(item, -amt);
	inven_item_optimize(item);

	/* Carry the object */
	slot = inven_carry(i_ptr);

	/* Message */
	msg_format("%s %s (%c).", act, o_name, index_to_label(slot));
	
	/* Return slot */
	return (slot);
}


/*
 * Drop (some of) a non-cursed inventory/equipment item
 *
 * The object will be dropped "near" the current location
 */
void inven_drop(int item, int amt)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	object_type *o_ptr;

	object_type *i_ptr;
	object_type object_type_body;

	char o_name[120];

	/* Get the original object */
	o_ptr = &inventory[item];

	/* Error check */
	if (amt <= 0) return;

	/* Not too many */
	if (amt > o_ptr->number) amt = o_ptr->number;


	/* Take off equipment */
	if (item >= INVEN_WIELD)
	{
		/* Take off first */
		item = inven_takeoff(item, amt);

		/* Get the original object */
		o_ptr = &inventory[item];
	}


	/* Get local object */
	i_ptr = &object_type_body;

	/* Obtain local object */
	object_copy(i_ptr, o_ptr);

	/* Modify quantity */
	i_ptr->number = amt;

	/* Describe local object */
	object_desc(o_name, sizeof(o_name), i_ptr, TRUE, 3);

	/* Message */
	msg_format("You drop %s (%c).", o_name, index_to_label(item));

	/* Drop it near the player */
	drop_near(i_ptr, 0, py, px);

	/* Modify, Describe, Optimize */
	inven_item_increase(item, -amt);
	inven_item_describe(item);
	inven_item_optimize(item);
}



/*
 * Combine items in the pack
 *
 * Note special handling of the "overflow" slot
 */
void combine_pack(void)
{
	int i, j, k;

	object_type *o_ptr;
	object_type *j_ptr;

	bool flag = FALSE;


	/* Combine the pack (backwards) */
	for (i = INVEN_PACK; i > 0; i--)
	{
		/* Get the item */
		o_ptr = &inventory[i];

		/* Skip empty items */
		if (!o_ptr->k_idx) continue;

		/* Scan the items above that item */
		for (j = 0; j < i; j++)
		{
			/* Get the item */
			j_ptr = &inventory[j];

			/* Skip empty items */
			if (!j_ptr->k_idx) continue;

			/* Can we drop "o_ptr" onto "j_ptr"? */
			if (object_similar(j_ptr, o_ptr))
			{
				/* Take note */
				flag = TRUE;

				/* Add together the item counts */
				object_absorb(j_ptr, o_ptr);

				/* Window stuff */
				p_ptr->window |= (PW_INVEN);
				
				if (o_ptr->number == 0)
				{
					/* One object is gone */
					p_ptr->inven_cnt--;
					
					/* Slide everything down */
					for (k = i; k < INVEN_PACK; k++)
					{
						/* Hack -- slide object */
						COPY(&inventory[k], &inventory[k+1], object_type);
					}
					
					/* Hack -- wipe hole */
					object_wipe(&inventory[k]);
					
					/* Done */
					break;
				}
			}
		}
	}

	/* Message */
	if (flag) msg_print("You combine some items in your pack.");
}


/*
 * Reorder items in the pack
 *
 * Note special handling of the "overflow" slot
 */
void reorder_pack(bool display_message)
{
	int i, j, k;

	s32b o_value;
	s32b j_value;

	object_type *o_ptr;
	object_type *j_ptr;

	object_type *i_ptr;
	object_type object_type_body;

	bool flag = FALSE;


	/* Re-order the pack (forwards) */
	for (i = 0; i < INVEN_PACK; i++)
	{
		/* Mega-Hack -- allow "proper" over-flow */
		if ((i == INVEN_PACK) && (p_ptr->inven_cnt == INVEN_PACK)) break;

		/* Get the item */
		o_ptr = &inventory[i];

		/* Skip empty slots */
		if (!o_ptr->k_idx) continue;

		/* Get the "value" of the item */
		o_value = object_value(o_ptr);

		/* Scan every occupied slot */
		for (j = 0; j < INVEN_PACK; j++)
		{
			/* Get the item already there */
			j_ptr = &inventory[j];

			/* Use empty slots */
			if (!j_ptr->k_idx) break;

			/* Objects sort by decreasing type */
			if (o_ptr->tval > j_ptr->tval) break;
			if (o_ptr->tval < j_ptr->tval) continue;

			/* Non-aware (flavored) items always come last */
			if (!object_aware_p(o_ptr)) continue;
			if (!object_aware_p(j_ptr)) break;

			/* Objects sort by increasing sval */
			if (o_ptr->sval < j_ptr->sval) break;
			if (o_ptr->sval > j_ptr->sval) continue;

			// This next bit is complicated: identified art > pseudo art > identified special > pseudo special > other

			/* Identified artefacts beat the rest */
			if (!(object_known_p(o_ptr) && artefact_p(o_ptr)) && (object_known_p(j_ptr) && artefact_p(j_ptr))) continue;
			if ((object_known_p(o_ptr) && artefact_p(o_ptr)) && !(object_known_p(j_ptr) && artefact_p(j_ptr))) break;
			
			/* Then pseudo-identified {artefact} */
			if (!(!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr)) && (!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr))) continue;
			if ((!object_known_p(o_ptr) && artefact_pseudo_p(o_ptr)) && !(!object_known_p(j_ptr) && artefact_pseudo_p(j_ptr))) break;

			/* Then identified specials */
			if (!(object_known_p(o_ptr) && ego_item_p(o_ptr)) && (object_known_p(j_ptr) && ego_item_p(j_ptr))) continue;
			if ((object_known_p(o_ptr) && ego_item_p(o_ptr)) && !(object_known_p(j_ptr) && ego_item_p(j_ptr))) break;

			/* Then pseudo-identified {special} */
			if (!(!object_known_p(o_ptr) && special_pseudo_p(o_ptr)) && (!object_known_p(j_ptr) && special_pseudo_p(j_ptr))) continue;
			if ((!object_known_p(o_ptr) && special_pseudo_p(o_ptr)) && !(!object_known_p(j_ptr) && special_pseudo_p(j_ptr))) break;

			/* Lites sort by decreasing fuel */
			if (o_ptr->tval == TV_LIGHT)
			{
				if (o_ptr->timeout > j_ptr->timeout) break;
				if (o_ptr->timeout < j_ptr->timeout) continue;
			}

			/* Determine the "value" of the pack item */
			j_value = object_value(j_ptr);
			
			/* Objects sort by decreasing value */
			if (o_value > j_value) break;
			if (o_value < j_value) continue;

			/* Objects sort by increasing weight */
			if (o_ptr->weight < j_ptr->weight) break;
			if (o_ptr->weight > j_ptr->weight) continue;
		}

		/* Never move down */
		if (j >= i) continue;

		/* Take note */
		flag = TRUE;

		/* Get local object */
		i_ptr = &object_type_body;

		/* Save a copy of the moving item */
		object_copy(i_ptr, &inventory[i]);

		/* Slide the objects */
		for (k = i; k > j; k--)
		{
			/* Slide the item */
			object_copy(&inventory[k], &inventory[k-1]);
		}

		/* Insert the moving item */
		object_copy(&inventory[j], i_ptr);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN);
		
		handle_stuff();
	}

	/* Message */
	if (flag && display_message) msg_print("You reorder some items in your pack.");
}

