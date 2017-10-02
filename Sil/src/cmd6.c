/* File: cmd6.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * This file includes code for eating food, drinking potions,
 * using staffs, playing instruments, and activating artefacts.
 *
 * In all cases, if the player becomes "aware" of the item's use
 * by testing it, mark it as "aware" and reward some experience
 * based on the object's level, always rounding up.  If the player
 * remains "unaware", mark that object "kind" as "tried".
 *
 * Note the overly paranoid warning about potential pack
 * overflow, which allows the player to use and drop a stacked item.
 *
 * In all "unstacking" scenarios, the "used" object is "carried" as if
 * the player had just picked it up.  In particular, this means that if
 * the use of an item induces pack overflow, that item will be dropped.
 *
 * For simplicity, these routines induce a full "pack reorganization"
 * which not only combines similar items, but also reorganizes various
 * items to obey the current "sorting" method.  This may require about
 * 400 item comparisons, but only occasionally.
 *
 * There may be a BIG problem with any "effect" that can cause "changes"
 * to the inventory.  For example, a "scroll of recharging" used to be
 * able to cause a staff to "disappear", moving the inventory up.  Luckily, the
 * scrolls all appear BEFORE the staffs/wands, so this is not a problem.
 * But, for example, a "staff of recharging" could cause MAJOR problems.
 * In such a case, it will be best to either (1) "postpone" the effect
 * until the end of the function, or (2) "change" the effect, say, into
 * giving a staff "negative" charges, or "turning a staff into a stick".
 * It seems as though a "rod of recharging" might in fact cause problems.
 * The basic problem is that the act of recharging (and destroying) an
 * item causes the inducer of that action to "move", causing "o_ptr" to
 * no longer point at the correct item, with horrifying results.
 *
 * Note that food/potions/scrolls no longer use bit-flags for effects,
 * but instead use the "sval" (which is also used to sort the objects).
 */



/*
 * Eat some food (from the pack or floor)
 */
void do_cmd_eat_food(object_type *default_o_ptr, int default_item)
{
	int item, lev;
	bool ident;
	bool aware;
	int kind_index;

	object_type *o_ptr;

	cptr q, s;

	// use specified item if possible
	if (default_o_ptr != NULL)
	{
		o_ptr = default_o_ptr;
		item = default_item;
	}
	/* Get an item */
	else
	{
		/* Restrict choices to food */
		item_tester_tval = TV_FOOD;

		/* Get an item */
		q = "Eat which item? ";
		s = "You have nothing to eat.";
		if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR))) return;

		/* Get the item (in the pack) */
		if (item >= 0)
		{
			o_ptr = &inventory[item];
		}

		/* Get the item (on the floor) */
		else
		{
			o_ptr = &o_list[0 - item];
		}
	}

	/* If gorged, you cannot eat food */
	if ((p_ptr->food >= PY_FOOD_MAX) && ((o_ptr->pval > 0) || (o_ptr->sval == SV_FOOD_SUSTENANCE)))
	{
		msg_print("You are too full to eat it.");
		return;
	}
		
	/* Sound */
	sound(MSG_EAT);

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Identity not known yet */
	ident = FALSE;

	// Save the k_idx and awareness info
	kind_index = o_ptr->k_idx;
	aware = object_aware_p(o_ptr);
		
	/* Object level */
	lev = k_info[o_ptr->k_idx].level;

	/* Eat the food */
	use_object(o_ptr, &ident);

	/* We have tried it */
	object_tried(o_ptr);

	/* Combine / Reorder the pack (later) */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);
	
	/* The player is now aware of the object */
	if (ident && !object_aware_p(o_ptr))
	{
		object_aware(o_ptr);
	}

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP);

	/* Destroy a food in the pack */
	if (item >= 0)
	{
		inven_item_increase(item, -1);
		inven_item_describe(item);
		inven_item_optimize(item);
	}

	/* Destroy a food on the floor */
	else
	{
		floor_item_increase(0 - item, -1);
		floor_item_describe(0 - item);
		floor_item_optimize(0 - item);
	}
	
	// allow autoinscribing of the herb
	if (!ident && !aware)
	{
		if (easter_time())
		{
			if (get_check("Autoinscribe this easter egg type? "))
			{
				do_cmd_autoinscribe_item(kind_index);
			}
		}
		else
		{
			if (get_check("Autoinscribe this herb type? "))
			{
				do_cmd_autoinscribe_item(kind_index);
			}
		}
	}
		
}




/*
 * Quaff a potion (from the pack or the floor)
 */
void do_cmd_quaff_potion(object_type *default_o_ptr, int default_item)
{
	int item, lev;
	bool ident;
	bool aware;
	int kind_index;
	object_type *o_ptr;
	cptr q, s;

	// use specified item if possible
	if (default_o_ptr != NULL)
	{
		o_ptr = default_o_ptr;
		item = default_item;
	}
	/* Get an item */
	else
	{
		/* Restrict choices to potions */
		item_tester_tval = TV_POTION;

		/* Get an item */
		q = "Quaff which potion? ";
		s = "You have no potions to quaff.";
		if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR))) return;

		/* Get the item (in the pack) */
		if (item >= 0)
		{
			o_ptr = &inventory[item];
		}

		/* Get the item (on the floor) */
		else
		{
			o_ptr = &o_list[0 - item];
		}
	}

	/* If gorged, you cannot drink potions */
	if ((p_ptr->food >= PY_FOOD_MAX) && (o_ptr->pval > 0))
	{
		msg_print("You are too full to drink it.");
		return;
	}
	
	/* Sound */
	sound(MSG_QUAFF);

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Not identified yet */
	ident = FALSE;

	// Save the k_idx and awareness info
	kind_index = o_ptr->k_idx;
	aware = object_aware_p(o_ptr);
		
	/* Object level */
	lev = k_info[o_ptr->k_idx].level;

	/* Quaff the potion */
	use_object(o_ptr, &ident);

	/* Combine / Reorder the pack (later) */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);

	/* The item has been tried */
	object_tried(o_ptr);

	/* An identification was made */
	if (ident && !object_aware_p(o_ptr))
	{
		object_aware(o_ptr);
	}

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP);

	/* Destroy a potion in the pack */
	if (item >= 0)
	{
		inven_item_increase(item, -1);
		inven_item_describe(item);
		inven_item_optimize(item);
	}

	/* Destroy a potion on the floor */
	else
	{
		floor_item_increase(0 - item, -1);
		floor_item_describe(0 - item);
		floor_item_optimize(0 - item);
	}
	
	// allow autoinscribing of the potion
	if (!ident && !aware)
	{
		if (get_check("Autoinscribe this potion type? "))
		{
			do_cmd_autoinscribe_item(kind_index);
		}
	}
	
}


/*
 * Play an instrument
 */
void do_cmd_play_instrument(object_type *default_o_ptr, int default_item)
{
	int item;
	
	bool ident;
	
	object_type *o_ptr;
		
	cptr q, s;
	
	// use specified item if possible
	if (default_o_ptr != NULL)
	{
		o_ptr = default_o_ptr;
		item = default_item;
	}
	/* Get an item */
	else
	{		
		/* Restrict choices to instruments */
		item_tester_tval = TV_HORN;
		
		/* Get an item */
		q = "Play which instrument? ";
		s = "You have no instrument to play.";
		if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR))) return;
		
		/* Get the item (in the pack) */
		if (item >= 0)
		{
			o_ptr = &inventory[item];
		}
		
		/* Get the item (on the floor) */
		else
		{
			o_ptr = &o_list[0 - item];
		}
	}
	
	/* Not identified yet */
	ident = FALSE;
	
	/* Play the instrument */
	if (!use_object(o_ptr, &ident)) return;
	
	/* Take a turn */
	p_ptr->energy_use = 100;
	
	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;

    // end the current song
	change_song(SNG_NOTHING);
    
	/* Combine / Reorder the pack (later) */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);
	
	/* Tried the object */
	object_tried(o_ptr);
	
	/* Successfully determined the object function */
	if (ident && !object_aware_p(o_ptr))
	{
		object_aware(o_ptr);
	}
	
	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP);
	
}





/*
 * Use a staff
 *
 * One charge of one staff disappears.
 *
 * Hack -- staffs of identify can be "cancelled".
 */
void do_cmd_activate_staff(object_type *default_o_ptr, int default_item)
{
	int item, score, difficulty, lev;

	bool ident;

	object_type *o_ptr;

	bool use_charge;

	cptr q, s;


	// use specified item if possible
	if (default_o_ptr != NULL)
	{
		o_ptr = default_o_ptr;
		item = default_item;
	}
	/* Get an item */
	else
	{
		/* Restrict choices to staves */
		item_tester_tval = TV_STAFF;

		/* Get an item */
		q = "Activate which staff? ";
		s = "You have no staff to activate.";
		if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR))) return;

		/* Get the item (in the pack) */
		if (item >= 0)
		{
			o_ptr = &inventory[item];
		}

		/* Get the item (on the floor) */
		else
		{
			o_ptr = &o_list[0 - item];
		}
	}

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Not identified yet */
	ident = FALSE;

	/* Extract the item level */
	lev = k_info[o_ptr->k_idx].level;

	/* Base chance of success */
	score = p_ptr->skill_use[S_WIL];

	// bonus to roll for 'channeling' ability
	if (p_ptr->active_ability[S_WIL][WIL_CHANNELING])
	{
		score += 5;
	}

	// Base difficulty
	difficulty = lev / 2;

	/* Confusion hurts skill */
	if (p_ptr->confused) difficulty += 5;

	/* Roll for usage */
	if (skill_check(PLAYER, score, difficulty, NULL) <= 0)
	{
		flush();
		msg_print("You failed to use the staff properly.");
		return;
	}

	/* Notice empty staffs */
	if (o_ptr->pval <= 0)
	{
		flush();
		msg_print("The staff has no charges left.");
		o_ptr->ident |= (IDENT_EMPTY);
		p_ptr->notice |= (PN_COMBINE | PN_REORDER);
		p_ptr->window |= (PW_INVEN);
		return;
	}


	/* Sound */
	sound(MSG_ZAP);

	/* Use the staff */
	use_charge = use_object(o_ptr, &ident);

	// Break the truce
	break_truce(FALSE);

	/* Combine / Reorder the pack (later) */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);

	/* Tried the item */
	object_tried(o_ptr);

	/* An identification was made */
	if (ident && !object_aware_p(o_ptr))
	{
		object_aware(o_ptr);
	}

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP);

	/* Hack -- some uses are "free" */
	if (!use_charge) return;

	/* Use a single charge */
	o_ptr->pval--;

	// mark times used
	o_ptr->xtra1++;

	/* Describe charges in the pack */
	if (item >= 0)
	{
		inven_item_charges(item);
	}

	/* Describe charges on the floor */
	else
	{
		floor_item_charges(0 - item);
	}
}


/*
 * Hook to determine if an object is activatable
 */
static bool item_tester_hook_activate(const object_type *o_ptr)
{
	u32b f1, f2, f3;

	/* Not known */
	if (!object_known_p(o_ptr)) return (FALSE);

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Check activation flag */
	if (f3 & (TR3_ACTIVATE)) return (TRUE);

	/* Assume not */
	return (FALSE);
}


/*
 * Activate a wielded object.  Wielded objects never stack.
 * And even if they did, activatable objects never stack.
 *
 * Note that it always takes a turn to activate an artefact, even if
 * the user hits "escape" at the "direction" prompt.
 */
void do_cmd_activate(void)
{
	int item, lev, score, difficulty;
	bool ident;
	object_type *o_ptr;

	cptr q, s;


	/* Prepare the hook */
	item_tester_hook = item_tester_hook_activate;

	/* Get an item */
	q = "Activate which item? ";
	s = "You have nothing to activate.";
	if (!get_item(&item, q, s, (USE_EQUIP))) return;

	/* Get the item (in the pack) */
	if (item >= 0)
	{
		o_ptr = &inventory[item];
	}

	/* Get the item (on the floor) */
	else
	{
		o_ptr = &o_list[0 - item];
	}


	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Extract the item level */
	lev = k_info[o_ptr->k_idx].level;

	/* Hack -- use artefact level instead */
	if (artefact_p(o_ptr)) lev = a_info[o_ptr->name1].level;

	/* Base chance of success */
	score = p_ptr->skill_use[S_WIL];

	// bonus to roll for 'channeling' ability
	if (p_ptr->active_ability[S_WIL][WIL_CHANNELING])
	{
		score += 5;
	}

	// Base difficulty
	difficulty = lev / 2;

	/* Confusion hurts skill */
	if (p_ptr->confused) difficulty += 5;

	/* Roll for usage */
	if (skill_check(PLAYER, score, difficulty, NULL) <= 0)
	{
		flush();
		msg_print("You could not draw upon its powers.");
		return;
	}

	/* Activate the object */
	(void)use_object(o_ptr, &ident);
}
