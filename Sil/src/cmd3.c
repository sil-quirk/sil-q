/* File: cmd3.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * The "wearable" tester
 */
static bool item_tester_hook_wear(const object_type *o_ptr)
{
	// Despite being a crown, the Iron Crown cannot be worn
	if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3)) return (FALSE);
	
	/* Check for a usable slot */
	if (wield_slot(o_ptr) >= INVEN_WIELD) return (TRUE);
	
	/* Assume not wearable */
	return (FALSE);
}



/*
 * Use an item, a unified 'use' command.
 */
void do_cmd_use_item(void)
{
	int item;
	object_type *o_ptr;
	cptr q, s;
	
	/* Unrestricted choice */
	item_tester_tval = 0;
	
	/* Get an item */
	q = "Use which item? ";
	s = "You have no items use.";
	if (!get_item(&item, q, s, (USE_INVEN | USE_EQUIP | USE_FLOOR))) return;
	
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
	
	// determine the action based on the item type
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
		case TV_LIGHT:
		case TV_AMULET:
		case TV_RING:
		case TV_ARROW:
		case TV_FLASK:
		{
			if (item < INVEN_WIELD)
			{
				object_type *l_ptr = &inventory[INVEN_LITE];
				bool try_to_wield = TRUE;
				
				// possibly refuel a light
				if ((o_ptr->tval == TV_FLASK) || 
				    ((l_ptr->tval == o_ptr->tval) && (l_ptr->sval == o_ptr->sval) && 
				     ((o_ptr->sval == SV_LIGHT_TORCH) || (o_ptr->sval == SV_LIGHT_LANTERN))))
				{
					if ((l_ptr->sval == SV_LIGHT_TORCH) && (o_ptr->tval != TV_FLASK))
					{
						if ((o_ptr->timeout + l_ptr->timeout <= FUEL_TORCH) || 
							 get_check("Refueling from this torch will waste some fuel. Proceed? "))
						{
							do_cmd_refuel_torch(o_ptr, item);
						}
						else
						{
							try_to_wield = FALSE;
						}
						break;
					}
					else if (l_ptr->sval == SV_LIGHT_LANTERN)
					{
						if (((o_ptr->tval == TV_FLASK) && ((l_ptr->timeout + o_ptr->pval <= FUEL_LAMP) || get_check("Refueling from this flask will waste some fuel. Proceed? "))) ||
						    ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN) && ((l_ptr->timeout + o_ptr->timeout <= FUEL_LAMP) || get_check("Refueling from this lantern will waste some fuel. Proceed? "))))
						{
							do_cmd_refuel_lamp(o_ptr, item);
						}
						else
						{
							try_to_wield = FALSE;
						}
						break;
					}
					else if (o_ptr->tval == TV_FLASK)
					{
						msg_print("That can only be used to refuel a wielded lantern.");
						break;
					}
				}
				
				if (!item_tester_hook_wear(o_ptr))
				{
					msg_print("It is far too large to be worn.");
				}
				else if (try_to_wield)
				{
					do_cmd_wield(o_ptr, item);
				}
			}
			else
			{
				do_cmd_takeoff(o_ptr, item);
			}
			break;
		}
		case TV_NOTE:
		{
			note_info_screen(o_ptr);
			break;
		}
		case TV_METAL:
		{
			msg_print("To melt down pieces of mithril, take them to a forge and type (,).");
			break;
		}
		case TV_CHEST:
		{
			msg_print("You would need to put it down to open it.");
			break;
		}
		case TV_STAFF:
		{
			do_cmd_activate_staff(o_ptr, item);
			break;
		}
		case TV_HORN:
		{
			do_cmd_play_instrument(o_ptr, item);
			break;
		}
		case TV_POTION:
		{
			do_cmd_quaff_potion(o_ptr, item);
			break;
		}
		case TV_FOOD:
		{
			do_cmd_eat_food(o_ptr, item);
			break;
		}
		default:
		{
			msg_print("It has no use.");
			break;
		}
	}
}

/*
 * Display inventory
 */
void do_cmd_inven(void)
{
	/* Hack -- Start in "inventory" mode */
	p_ptr->command_wrk = (USE_INVEN);

	/* Save screen */
	screen_save();

	/* Hack -- show empty slots */
	item_tester_full = TRUE;

	/* Display the inventory */
	show_inven();

	/* Hack -- hide empty slots */
	item_tester_full = FALSE;

	/* Prompt for a command */
	prt("(Inventory) Command: ", 0, 0);

	/* Hack -- Get a new command */
	p_ptr->command_new = inkey();

	/* Load screen */
	screen_load();

	/* Hack -- Process "Escape" */
	if (p_ptr->command_new == ESCAPE)
	{
		/* Reset stuff */
		p_ptr->command_new = 0;
	}

	/* Hack -- Process normal keys */
	else
	{
		/* Hack -- Use "display" mode */
		p_ptr->command_see = TRUE;
	}
}


/*
 * Display equipment
 */
void do_cmd_equip(void)
{
	/* Hack -- Start in "equipment" mode */
	p_ptr->command_wrk = (USE_EQUIP);

	/* Save screen */
	screen_save();

	/* Hack -- show empty slots */
	item_tester_full = TRUE;

	/* Display the equipment */
	show_equip();

	/* Hack -- undo the hack above */
	item_tester_full = FALSE;

	/* Prompt for a command */
	prt("(Equipment) Command: ", 0, 0);

	/* Hack -- Get a new command */
	p_ptr->command_new = inkey();

	/* Load screen */
	screen_load();


	/* Hack -- Process "Escape" */
	if (p_ptr->command_new == ESCAPE)
	{
		/* Reset stuff */
		p_ptr->command_new = 0;
	}

	/* Hack -- Process normal keys */
	else
	{
		/* Enter "display" mode */
		p_ptr->command_see = TRUE;
	}
}


/*
 * Wield or wear a single item from the pack or floor
 */
void do_cmd_wield(object_type *default_o_ptr, int default_item)
{
	int item, slot;
	
	object_type *o_ptr;
	
	object_type *i_ptr;
	object_type object_type_body;
	
	cptr act;
	
	cptr q, s;
	
	int i, quantity, original_quantity;
	
	bool weapon_less_effective = FALSE;
	
	bool grants_two_weapon = FALSE;
	
	char o_name[80];
	
	bool combine = FALSE;
	
	// use specified item if possible
	if (default_o_ptr != NULL)
	{
		o_ptr = default_o_ptr;
		item = default_item;
	}
	/* Get an item */
	else
	{
		/* Restrict the choices */
		item_tester_hook = item_tester_hook_wear;
		
		/* Get an item */
		q = "Wear/Wield which item? ";
		s = "You have nothing you can wear or wield.";
		if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR))) return;
		
		/* Get the item (in the pack) */
		if (item >= 0)
		{
			o_ptr = &inventory[item];
		}
		else
		{
			o_ptr = &o_list[0 - item];
		}
	}
	
	// remember how many there were
	original_quantity = o_ptr->number;
	
	// Check whether it would be too heavy
	if ((item < 0) && (p_ptr->total_weight + o_ptr->weight > weight_limit()*3/2))
	{
		/* Describe it */
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
		
		if (o_ptr->k_idx) msg_format("You cannot lift %s.", o_name);
		
		/* Abort */
		return;
	}
	
	
	/* Check the slot */
	slot = wield_slot(o_ptr);
	
	/* Ask for ring to replace */
	if ((o_ptr->tval == TV_RING) &&
		inventory[INVEN_LEFT].k_idx &&
		inventory[INVEN_RIGHT].k_idx)
	{
		/* Restrict the choices */
		item_tester_tval = TV_RING;
		
		/* Choose a ring from the equipment only */
		q = "Replace which ring? ";
		s = "Oops.";
		if (!get_item(&slot, q, s, USE_EQUIP)) return;
	}

	// Special cases for merging arrows
	if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
	{
		slot = INVEN_QUIVER1;
		combine = TRUE;
	}
	else if (object_similar(&inventory[INVEN_QUIVER2], o_ptr))
	{
		slot = INVEN_QUIVER2;
		combine = TRUE;
	}
	/* Ask for arrow set to replace */
	else if ((o_ptr->tval == TV_ARROW) && inventory[INVEN_QUIVER1].k_idx && inventory[INVEN_QUIVER2].k_idx)
	{
		/* Restrict the choices */
		item_tester_tval = TV_ARROW;
		
		/* Choose a set of arrows from the equipment only */
		q = "Replace which set of arrows? ";
		s = "Oops.";
		if (!get_item(&slot, q, s, USE_EQUIP)) return;
	}
	
	// Ask about two weapon fighting if necessary
	for (i = 0; i < o_ptr->abilities; i++)
	{
		if ((o_ptr->skilltype[i] == S_MEL) && (o_ptr->abilitynum[i] == MEL_TWO_WEAPON) && object_known_p(o_ptr))
		{
			grants_two_weapon = TRUE;
		}
	}
	if ((p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON] || grants_two_weapon) && 
	    ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM) || (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING)))
	{
		if (!(k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED)) && !(k_info[o_ptr->k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
		{
			if (get_check("Do you wish to wield it in your off-hand? "))
			{
				slot = INVEN_ARM;
			}
		}
	}
	
	/* Prevent wielding into a cursed slot */
	if (cursed_p(&inventory[slot]))
	{
		/* Describe it */
		object_desc(o_name, sizeof(o_name), &inventory[slot], FALSE, 0);
		
		/* Message */
		msg_format("You cannot bear to give up the %s you are %s.",
		           o_name, describe_use(slot));
		
		/* Cancel the command */
		return;
	}
	
	/* Deal with wielding of two-handed weapons when already using a shield */
	if ((k_info[o_ptr->k_idx].flags3 & (TR3_TWO_HANDED)) && (inventory[INVEN_ARM].k_idx))
	{
		if (cursed_p(&inventory[INVEN_ARM]))
		{
			if (inventory[INVEN_ARM].tval == TV_SHIELD)
			{
				msg_print("You would need to remove your shield, but cannot bear to part with it.");
			}
			else
			{
				msg_print("You would need to remove your off-hand weapon, but cannot bear to part with it.");
			}
			
			/* Cancel the command */
			return;			
		}
		
		// warn about dropping item in left hand
		if ((item < 0) && (&inventory[INVEN_PACK-1])->tval)
		{
			/* Flush input */
			flush();
			
			if (inventory[INVEN_ARM].tval == TV_SHIELD)
			{
				if (!get_check("This would require removing (and dropping) your shield. Proceed? "))
				{
					/* Cancel the command */
					return;
				}
			}
			else
			{
				msg_print("This would require removing (and dropping) your off-hand weapon.");
				if (!get_check("Proceed? "))
				{
					/* Cancel the command */
					return;
				}
			}
		}
	}
	
	/* Deal with wielding of shield or second weapon when already wielding a two handed weapon */
	if ((slot == INVEN_ARM) && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))	     
	{
		if (cursed_p(&inventory[INVEN_WIELD]))
		{
            msg_print("You would need to put down your weapon, but cannot bear to part with it.");
			
			/* Cancel the command */
			return;
		}

		// warn about dropping item in left hand
		if ((item < 0) && (&inventory[INVEN_PACK-1])->tval)
		{
			/* Flush input */
			flush();
			
			if (inventory[INVEN_ARM].tval == TV_SHIELD)
			{
				if (!get_check("This would require removing (and dropping) your weapon. Proceed? "))
				{
					/* Cancel the command */
					return;
				}
			}
			else
			{
				msg_print("This would require removing (and dropping) your weapon.");
				if (!get_check("Proceed? "))
				{
					/* Cancel the command */
					return;
				}
			}
		}
	}
	
	/* Deal with wielding of shield or second weapon when already wielding a hand and a half weapon */
	if ((slot == INVEN_ARM) && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_HAND_AND_A_HALF))
	    && (!inventory[INVEN_ARM].k_idx))
	{
		weapon_less_effective = TRUE;
	}
	
	/* Take a turn */
	p_ptr->energy_use = 100;
	
	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Get local object */
	i_ptr = &object_type_body;
	
	/* Obtain local object */
	object_copy(i_ptr, o_ptr);
	
	// Handle quantity differently for arrows
	if (i_ptr->tval == TV_ARROW)
	{
		if (combine)	quantity = MIN(o_ptr->number, MAX_STACK_SIZE - 1 - (&inventory[slot])->number);
		else			quantity = o_ptr->number;
	}
	else
	{
		quantity = 1;
	}
	
	/* Modify quantity */
	i_ptr->number = quantity;
	
	/* Decrease the item (from the pack) */
	if (item >= 0)
	{
		inven_item_increase(item, -quantity);
		inven_item_optimize(item);
	}
	
	/* Decrease the item (from the floor) */
	else
	{
		floor_item_increase(0 - item, -quantity);
		floor_item_optimize(0 - item);
	}
	
	/* Get the wield slot */
	o_ptr = &inventory[slot];
	
	/* Take off existing item */
	if (o_ptr->k_idx && !combine)
	{
		/* Take off existing item */
		(void)inven_takeoff(slot, 255);
	}
	
	/* Deal with wielding of two-handed weapons when already using a shield */
	if ((k_info[i_ptr->k_idx].flags3 & (TR3_TWO_HANDED)) && (inventory[INVEN_ARM].k_idx))
	{
		/* Take off shield */
		check_pack_overflow();
		(void)inven_takeoff(INVEN_ARM, 255);
	}
	
	/* Deal with wielding of shield or second weapon when already wielding a two handed weapon */
	if ((slot == INVEN_ARM) && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_TWO_HANDED)))
	{
		/* Stop wielding two handed weapon */
		(void)inven_takeoff(INVEN_WIELD, 255);
	}
	
	/* Combine the new stuff into the equipment */
	if (combine)
	{
		msg_print("You combine them with some that are already in your quiver.");
		object_absorb(o_ptr, i_ptr);
	}
	/* Wear the new stuff */
	else
	{
		object_copy(o_ptr, i_ptr);
	}
	
	/* Increment the equip counter by hand */
	if (!combine) p_ptr->equip_cnt++;
	
	/* Where is the item now */
	if ((slot == INVEN_WIELD) || ((slot == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)))
	{
		act = "You are wielding";
	}
	else if (slot == INVEN_BOW)
	{
		act = "You are shooting with";
	}
	else if (slot == INVEN_LITE)
	{
		act = "Your light source is";
	}
	else if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
	{
		act = "In your quiver you have";
	}
	else
	{
		act = "You are wearing";
	}
	
	/* Describe the result */
	object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
	
	/* Message */
	msg_format("%s %s (%c).", act, o_name, index_to_label(slot));
	
	// Deal with wielding from the floor
	if (item < 0)
	{
		/* Forget monster */
		o_ptr->held_m_idx = 0;
		
		/* Forget location */
		o_ptr->iy = o_ptr->ix = 0;
		
		// Break the truce if picking up an item from the floor
		break_truce(FALSE);

		// Special effects when picking up all the items from the floor
		if (i_ptr->number == original_quantity)
		{
			/* No longer marked */
			o_ptr->marked = FALSE;
		}
	}
	
	/* Cursed! */
	if (cursed_p(o_ptr))
	{
		/* Warn the player */
		msg_print("You have a bad feeling about this...");
		
		/* Remove special inscription, if any */
		if (o_ptr->discount >= INSCRIP_NULL) o_ptr->discount = 0;
		
		/* Sense the object if allowed */
		if (o_ptr->discount == 0) o_ptr->discount = INSCRIP_CURSED;
		
		/* The object has been "sensed" */
		o_ptr->ident |= (IDENT_SENSE);
	}
	
	if (weapon_less_effective)
	{
		/* Describe it */
		object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], FALSE, 0);
		
		/* Message */
		msg_format("You are no longer able to wield your %s as effectively.",
				   o_name);
	}
	
	ident_on_wield(o_ptr);
	
	// activate all of its new abilities
	for (i = 0; i < o_ptr->abilities; i++)
	{
		if (!p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]])
		{
			p_ptr->have_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]] = TRUE;
			p_ptr->active_ability[o_ptr->skilltype[i]][o_ptr->abilitynum[i]] = TRUE;
		}
	}
	
	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);
	
	/* Recalculate mana */
	p_ptr->update |= (PU_MANA);
	
	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
	
	p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
}





/*
 * Take off an item
 */
void do_cmd_takeoff(object_type *default_o_ptr, int default_item)
{
	int item;

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
		q = "Remove which item? ";
		s = "You are not wearing anything to remove.";
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
	}

	/* Item is cursed */
	if (cursed_p(o_ptr))
	{
		if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
		{
			/* Message */
			msg_print("With a great strength of will, you break the curse!");
			
			/* Uncurse the object */
			uncurse_object(o_ptr);						
		}
		else
		{
			/* Oops */
			msg_print("You cannot bear to part with it.");
			
			/* Nope */
			return;
		}
	}


	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Take off the item */
	(void)inven_takeoff(item, 255);

	/* Deal with wielding of shield when already wielding a hand and a half weapon */
	if ((item == INVEN_ARM) && (k_info[inventory[INVEN_WIELD].k_idx].flags3 & (TR3_HAND_AND_A_HALF)))
	{
		char o_name[80];
		
		/* Describe it */
		object_desc(o_name, sizeof(o_name), &inventory[INVEN_WIELD], FALSE, 0);

		/* Message */
		msg_format("You can now wield your %s more effectively.",
				o_name);
	}

}


/*
 * Drop an item
 */
void do_cmd_drop(void)
{
	int item, amt;

	object_type *o_ptr;

	cptr q, s;

	/* Get an item */
	q = "Drop which item? ";
	s = "You have nothing to drop.";
	if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN))) return;

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

	/* Get a quantity */
	amt = get_quantity(NULL, o_ptr->number);

	/* Allow user abort */
	if (amt <= 0) return;

	/* Hack -- Cannot remove cursed items */
	if ((item >= INVEN_WIELD) && cursed_p(o_ptr))
	{
		if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
		{
			/* Message */
			msg_print("With a great strength of will, you break the curse!");
			
			/* Uncurse the object */
			uncurse_object(o_ptr);						
		}
		else
		{
			/* Oops */
			msg_print("You cannot bear to part with it.");
			
			/* Nope */
			return;
		}
	}

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
	
	/* Drop (some of) the item */
	inven_drop(item, amt);
}

/*
 * An "item_tester_hook" for destroying objects
 */
static bool item_tester_hook_destroy(const object_type *o_ptr)
{

	if (o_ptr) {} // suppresses warnings about this function

	//if (artefact_p(o_ptr))
	//{
    //	/* Ignore known or sensed artefacts */
    //	if ((object_known_p(o_ptr)) ||
	//	    (o_ptr->discount == INSCRIP_TERRIBLE) ||
	//		(o_ptr->discount == INSCRIP_SPECIAL) ||
	//		(o_ptr->discount == INSCRIP_INDESTRUCTIBLE)) return (FALSE);
	//}

	return (TRUE);
}

/*
 *  Shatter the player's wielded weapon.
 */
void shatter_weapon(int silnum)
{
	int i;
	object_type   *w_ptr = &inventory[INVEN_WIELD];
	char w_name[80];
	
	p_ptr->crown_shatter = TRUE;
			
	/* Get the basic name of the object */
	object_desc(w_name, sizeof(w_name), w_ptr, FALSE, 0);
	
	if (silnum == 2)	msg_print("You strive to free a second Silmaril, but it is not fated to be.");
	else				msg_print("You strive to free a third Silmaril, but it is not fated to be.");
	
	msg_format("As you strike the crown, your %s shatters into innumerable pieces.", w_name);
	
	// make more noise
	stealth_score -= 5;
	
	inven_item_increase(INVEN_WIELD, -1);
	inven_item_optimize(INVEN_WIELD);
	
	/* Process monsters */
	for (i = 1; i < mon_max; i++)
	{
		monster_type *m_ptr = &mon_list[i];
		
		/* If Morgoth, then anger him */
		if (m_ptr->r_idx == R_IDX_MORGOTH)
		{
			if ((m_ptr->cdis <= 5) && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
			{
				msg_print("A shard strikes Morgoth upon his cheek.");
				set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
			}
		}
	}
	
}

void prise_silmaril(void)
{
	object_type   *o_ptr;
	object_type   *w_ptr;
	artefact_type *a_ptr;

	object_type object_type_body;

	cptr freed_msg = NULL; // default to soothe compiler warnings

	bool freed = FALSE;

	int slot = 0;
	
	int dam = 0;
	int prt = 0;
	int net_dam = 0;
	int prt_percent = 0;
	int hit_result = 0;
	int crit_bonus_dice = 0;
	int pd = 0;
	int noise = 0;
	u32b dummy_noticed_flag;
	
	int mds = p_ptr->mds;
	int attack_mod = p_ptr->skill_use[S_MEL];

	char o_name[80];

	// the Crown is on the ground
	o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
				
	switch (o_ptr->name1)
	{
		case ART_MORGOTH_3:
		{
			pd = 15;
			noise = 5;
			freed_msg = "You have freed a Silmaril!";
			break;
		}
		case ART_MORGOTH_2:
		{
			pd = 25;
			noise = 10;
						
			if (p_ptr->crown_shatter)	freed_msg = "The fates be damned! You free a second Silmaril.";
			else						freed_msg = "You free a second Silmaril.";
			
			break;
		}
		case ART_MORGOTH_1:
		{
			pd = 30;
			noise = 15;
			
			freed_msg = "You free the final Silmaril. You have a very bad feeling about this.";
			
			msg_print("Looking into the hallowed light of the final Silmaril, you are filled with a strange dread.");
			if (!get_check("Are you sure you wish to proceed? ")) return;
			
			break;
		}
	}
	
	/* Get the weapon */
	w_ptr = &inventory[INVEN_WIELD];

	// undo rapid attack penalties
	if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
	{
		// undo strength adjustment to the attack
		mds = total_mds(w_ptr, 0);
		
		// undo the dexterity adjustment to the attack
		attack_mod += 3;
	}
	
	/* Test for hit */
	hit_result = hit_roll(attack_mod, 0, PLAYER, NULL, TRUE);
	
	/* Make some noise */
	stealth_score -= noise;
	
	// Determine damage
	if (hit_result > 0)
	{
		crit_bonus_dice = crit_bonus(hit_result, w_ptr->weight, &r_info[R_IDX_MORGOTH], S_MEL, FALSE);
		
		dam = damroll(p_ptr->mdd + crit_bonus_dice, mds);
		prt = damroll(pd, 4);
		
		prt_percent = prt_after_sharpness(w_ptr, &dummy_noticed_flag);
		prt = (prt * prt_percent) / 100;
		net_dam = dam - prt;
		
		/* No negative damage */
		if (net_dam < 0) net_dam = 0;
		
		//update_combat_rolls1b(PLAYER, TRUE);
		update_combat_rolls2(p_ptr->mdd + crit_bonus_dice, mds, dam, pd, 4, prt, prt_percent, GF_HURT, TRUE);
	}
	
	
	// if you succeed in prising out a Silmaril...
	if (net_dam > 0)
	{
		freed = TRUE;
		
		switch (o_ptr->name1)
		{
			case ART_MORGOTH_3:
			{
				break;
			}
			case ART_MORGOTH_2:
			{
				if (!p_ptr->crown_shatter && one_in_(2))
				{
					shatter_weapon(2);
					freed = FALSE;
				}
				break;
			}
			case ART_MORGOTH_1:
			{
				if (!p_ptr->crown_shatter)
				{
					shatter_weapon(3);
					freed = FALSE;
				}
				else
				{
					p_ptr->cursed = TRUE;
				}
				break;
			}
		}
		
		if (freed)
		{
			// change its type to that of the crown with one less silmaril
			o_ptr->name1--;
			
			// get the details of this new crown
			a_ptr = &a_info[o_ptr->name1];
			
			// modify the existing crown
			object_into_artefact(o_ptr, a_ptr);
			
			// report success
			msg_print(freed_msg);
			
			// Get new local object
			o_ptr = &object_type_body;
			
			// Make Silmaril
			object_prep(o_ptr, lookup_kind(TV_LIGHT, SV_LIGHT_SILMARIL));
			
			// Get it
			slot = inven_carry(o_ptr);

			/* Get the object again */
			o_ptr = &inventory[slot];
			
			/* Describe the object */
			object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
			
			/* Message */
			msg_format("You have %s (%c).", o_name, index_to_label(slot));

			// Break the truce (always)
			break_truce(TRUE);
		
			// add a note to the notes file
			do_cmd_note("Cut a Silmaril from Morgoth's crown", p_ptr->depth);
		}
	}
	
	// if you fail to prise out a Silmaril...
	else
	{
		msg_print("Try though you might, you were unable to free a Silmaril.");
		msg_print("Perhaps you should try again or use a different weapon.");

		if (pd == 15) msg_print("(The combat rolls window shows what is happening.)");

		// Break the truce if creatures see
		break_truce(FALSE);
	}

	// check for taking of final Silmaril
	if ((pd == 30) && freed)
	{
		msg_print("Until you escape you must now roll twice for every skill check, taking the worse result each time.");
		msg_print("You hear a cry of veangance echo through the iron hells.");
		wake_all_monsters(0);
	}
}

/*
 * Destroy an item
 */
void do_cmd_destroy(void)
{
	int item, amt;
	int old_number;
	int old_charges = 0;

	object_type *o_ptr;

	char o_name[80];

	char out_val[160];

	cptr q, s;

	item_tester_hook = item_tester_hook_destroy;


	// Special case for prising Silmarils from the Iron Crown of Morgoth
	o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
	if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
	{
		// Select the melee weapon
		o_ptr = &inventory[INVEN_WIELD];
		
		// No weapon
		if (!o_ptr->k_idx)
		{
			msg_print("To prise a Silmaril from the crown, you would need to wield a weapon.");
		}
		
		// Wielding a weapon
		else
		{
			if (get_check("Will you try to prise a Silmaril from the Iron Crown? "))
			{
				prise_silmaril();
				
				/* Take a turn */
				p_ptr->energy_use = 100;
				
				// store the action type
				p_ptr->previous_action[0] = ACTION_MISC;
				
				return;
			}
		}
	}


	/* Get an item */
	q = "Destroy which item? ";
	s = "You have nothing to destroy.";
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

	// Special case for Iron Crown of Morgoth, if it has Silmarils left
	if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
	{
		if (item >= 0)
		{
			msg_print("You would have to put it down first.");
		}
		else
		{
			/* No weapon */
			if (!o_ptr->k_idx)
			{
				msg_print("To prise a Silmaril from the crown, you would need to wield a weapon.");
			}
			else
			{
				msg_print("You decide to try to prise out a Silmaril after all.");
				
				prise_silmaril();
				
				/* Take a turn */
				p_ptr->energy_use = 100;
				
				// store the action type
				p_ptr->previous_action[0] = ACTION_MISC;
				
				return;
			}
		}
	}

	/* Get a quantity */
	amt = get_quantity(NULL, o_ptr->number);

	/* Allow user abort */
	if (amt <= 0) return;

	/* Describe the object */
	old_number = o_ptr->number;

	/* Hack, state the correct number of charges to be destroyed if staff*/
	if ((o_ptr->tval == TV_STAFF) && (amt < o_ptr->number))
	{
		/*save the number of charges*/
		old_charges = o_ptr->pval;

		/*distribute the charges*/
		o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;

		o_ptr->pval = old_charges - o_ptr->pval;
	}

	/*hack -  make sure we get the right amount displayed*/
	o_ptr->number = amt;

	/*now describe with correct amount*/
	object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

	/*reverse the hack*/
	o_ptr->number = old_number;

	/* Check for known special items */
	strnfmt(out_val, sizeof(out_val), "Really destroy %s? ", o_name);

	if (!get_check(out_val)) return;

	/* Take a turn */
	p_ptr->energy_use = 100;

	// store the action type
	p_ptr->previous_action[0] = ACTION_MISC;
		
	/* Message */
	msg_format("You destroy %s.", o_name);

	/*hack, restore the proper number of charges after the messages have printed
	 * so the proper number of charges are destroyed*/
	if (old_charges) o_ptr->pval = old_charges;

	/* Eliminate the item (from the pack) */
	if (item >= 0)
	{
		inven_item_increase(item, -amt);
		inven_item_describe(item);
		inven_item_optimize(item);
	}

	/* Eliminate the item (from the floor) */
	else
	{
		floor_item_increase(0 - item, -amt);
		floor_item_describe(0 - item);
		floor_item_optimize(0 - item);
	}
}


/*
 * Observe an item, displaying what is known about it
 */
void do_cmd_observe(void)
{
	int item;

	object_type *o_ptr;

	cptr q, s;

	/* Get an item */
	q = "Examine which item? ";
	s = "You have nothing to examine.";
	if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR))) return;

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

	/* Describe */
	object_info_screen(o_ptr);
}

/*
 * Helper function which actually removes the inscription
 */
void uninscribe(object_type *o_ptr)
{
	/* Remove the inscription */
	o_ptr->obj_note = 0;
	
	/*The object kind has an autoinscription*/
	// Sil-y: removed restriction to known items (through 'object_aware')
	if (!(k_info[o_ptr->k_idx].flags3 & (TR3_INSTA_ART)) &&
	    (get_autoinscription_index(o_ptr->k_idx) != -1))
	{
		char tmp_val[160];
		char o_name2[80];
		
		/*make a fake object so we can give a proper message*/
		object_type *i_ptr;
		object_type object_type_body;
		
		/* Get local object */
		i_ptr = &object_type_body;
		
		/* Wipe the object */
		object_wipe(i_ptr);
		
		/* Create the object */
		object_prep(i_ptr, o_ptr->k_idx);
		
		/*make it plural*/
		i_ptr->number = 2;
		
		/*now describe with correct amount*/
		object_desc(o_name2, sizeof(o_name2), i_ptr, FALSE, 0);
		
		/* Prompt */
		strnfmt(tmp_val, sizeof(tmp_val), "Remove automatic inscription for %s? ", o_name2);
		
		/* Auto-Inscribe if they want that */
		if (get_check(tmp_val)) obliterate_autoinscription(o_ptr->k_idx);
	}
	
	/* Message */
	msg_print("Inscription removed.");
	
	/* Combine the pack */
	p_ptr->notice |= (PN_COMBINE);
	
	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Remove the inscription from an object
 * XXX Mention item (when done)?
 */
void do_cmd_uninscribe(void)
{
	int item;

	object_type *o_ptr;

	cptr q, s;


	/* Get an item */
	q = "Un-inscribe which item? ";
	s = "You have nothing to un-inscribe.";
	if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR))) return;

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

	/* Nothing to remove */
	if (!o_ptr->obj_note)
	{
		msg_print("That item had no inscription to remove.");
		return;
	}
	
	// Do the work
	uninscribe(o_ptr);
}


/*
 * Inscribe an object with a comment
 */
void do_cmd_inscribe(void)
{
	int item;

	object_type *o_ptr;

	char o_name[80];

	char tmp[80];

	cptr q, s;

	/* Get an item */
	q = "Inscribe which item? ";
	s = "You have nothing to inscribe.";
	if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR))) return;

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

	/* Describe the activity */
	object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

	/* Message */
	msg_format("Inscribing %s.", o_name);
	message_flush();

	/* Start with nothing */
	my_strcpy(tmp, "", sizeof(tmp));

	/* Use old inscription */
	if (o_ptr->obj_note)
	{
		/* Start with the old inscription */
		strnfmt(tmp, sizeof(tmp), "%s", quark_str(o_ptr->obj_note));
	}

	/* Get a new inscription (possibly empty) */
	if (term_get_string("Inscription: ", tmp, sizeof(tmp)))
	{
		char tmp_val[160];
		char o_name2[80];

		/*make a fake object so we can give a proper message*/
		object_type *i_ptr;
		object_type object_type_body;

		// if given an empty inscription, then uninscribe instead
		if (strlen(tmp) == 0)
		{
			uninscribe(o_ptr);
			return;
		}
		
		/* Save the inscription */
		o_ptr->obj_note = quark_add(tmp);

		/* Add an autoinscription? */
		// Sil-y: removed restriction to known items (through 'object_aware')
		if (!(k_info[o_ptr->k_idx].flags3 & (TR3_INSTA_ART)))
		{
			/* Get local object */
			i_ptr = &object_type_body;

			/* Wipe the object */
			object_wipe(i_ptr);

			/* Create the object */
			object_prep(i_ptr, o_ptr->k_idx);

			/*make it plural*/
			i_ptr->number = 2;

			/*now describe with correct amount*/
			object_desc(o_name2, sizeof(o_name2), i_ptr, FALSE, 0);

			/* Prompt */
			strnfmt(tmp_val, sizeof(tmp_val), "Automatically inscribe all %s with '%s'? ",
					o_name2, tmp);

			/* Auto-Inscribe if they want that */
			if (get_check(tmp_val)) add_autoinscription(o_ptr->k_idx, tmp);
		}

		/* Combine the pack */
		p_ptr->notice |= (PN_COMBINE);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN | PW_EQUIP);
	}
}



/*
 * An "item_tester_hook" for refueling lanterns
 */
static bool item_tester_refuel_lantern(const object_type *o_ptr)
{
	/* Flasks of oil are okay */
	if (o_ptr->tval == TV_FLASK) return (TRUE);

	/* Non-empty lanterns are okay */
	if ((o_ptr->tval == TV_LIGHT) &&
	    (o_ptr->sval == SV_LIGHT_LANTERN) &&
	    (o_ptr->timeout > 0))
	{
		return (TRUE);
	}

	/* Assume not okay */
	return (FALSE);
}


/*
 * Refill the player's lamp (from the pack or floor)
 */
void do_cmd_refuel_lamp(object_type *default_o_ptr, int default_item)
{
	int item;

	object_type *o_ptr;
	object_type *j_ptr;

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
		/* Restrict the choices */
		item_tester_hook = item_tester_refuel_lantern;

		/* Get an item */
		q = "Refill with which source of oil? ";
		s = "You have no sources of oil.";
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
		
	/* Get the lantern */
	j_ptr = &inventory[INVEN_LITE];

	/* Refuel from a latern */
	if (o_ptr->sval == SV_LIGHT_LANTERN)
	{
	  j_ptr->timeout += o_ptr->timeout;
	}
	/* Refuel from a flask */
	else
	{
	  j_ptr->timeout += o_ptr->pval;
	}

	/* Message */
	msg_print("You fuel your lamp.");

	/* Comment */
	if (j_ptr->timeout >= FUEL_LAMP)
	{
		j_ptr->timeout = FUEL_LAMP;
		msg_print("Your lamp is full.");
	}

	/* Refilled from a latern */
	if (o_ptr->sval == SV_LIGHT_LANTERN)
	{
		/* Unstack if necessary */
		if (o_ptr->number > 1)
		{
			object_type *i_ptr;
			object_type object_type_body;

			/* Get local object */
			i_ptr = &object_type_body;

			/* Obtain a local object */
			object_copy(i_ptr, o_ptr);

			/* Modify quantity */
			i_ptr->number = 1;

			/* Remove fuel */
			i_ptr->timeout = 0;

			/* Unstack the used item */
			o_ptr->number--;

			/* Carry or drop */
			if (item >= 0)
				item = inven_carry(i_ptr);
			else
				drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
		}

		/* Empty a single latern */
		else
		{
			/* No more fuel */
			o_ptr->timeout = 0;
		}


		/* Combine / Reorder the pack (later) */
		p_ptr->notice |= (PN_COMBINE | PN_REORDER);

		/* Window stuff */
		p_ptr->window |= (PW_INVEN);
	}

	/* Refilled from a flask */
	else
	{
		/* Decrease the item (from the pack) */
		if (item >= 0)
		{
			inven_item_increase(item, -1);
			inven_item_describe(item);
			inven_item_optimize(item);
		}

		/* Decrease the item (from the floor) */
		else
		{
			floor_item_increase(0 - item, -1);
			floor_item_describe(0 - item);
			floor_item_optimize(0 - item);
		}

	}

	/* Window stuff */
	p_ptr->window |= (PW_EQUIP);
	
	// get another chance to identify the lamp
	ident_on_wield(j_ptr);
}



/*
 * An "item_tester_hook" for refueling torches
 */
static bool item_tester_refuel_torch(const object_type *o_ptr)
{
	/* Torches are okay */
	if ((o_ptr->tval == TV_LIGHT) &&
	    (o_ptr->sval == SV_LIGHT_TORCH)) return (TRUE);

	/* Assume not okay */
	return (FALSE);
}


/*
 * Refuel the player's torch (from the pack or floor)
 */
void do_cmd_refuel_torch(object_type *default_o_ptr, int default_item)
{
	int item;

	object_type *o_ptr;
	object_type *j_ptr;

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
		/* Restrict the choices */
		item_tester_hook = item_tester_refuel_torch;

		/* Get an item */
		q = "Refuel with which torch? ";
		s = "You have no extra torches.";
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
	
	/* Get the primary torch */
	j_ptr = &inventory[INVEN_LITE];

	/* Refuel */
	j_ptr->timeout += o_ptr->timeout + 5;

	/* Message */
	msg_print("You combine the torches.");

	/* Over-fuel message */
	if (j_ptr->timeout >= FUEL_TORCH)
	{
		j_ptr->timeout = FUEL_TORCH;
		msg_print("Your torch is fully fueled.");
	}

	/* Refuel message */
	else
	{
		msg_print("Your torch glows more brightly.");
	}

	/* Decrease the item (from the pack) */
	if (item >= 0)
	{
		inven_item_increase(item, -1);
		inven_item_describe(item);
		inven_item_optimize(item);
	}

	/* Decrease the item (from the floor) */
	else
	{
		floor_item_increase(0 - item, -1);
		floor_item_describe(0 - item);
		floor_item_optimize(0 - item);
	}

	/* Window stuff */
	p_ptr->window |= (PW_EQUIP);
	
	// get another chance to identify the torch
	ident_on_wield(j_ptr);
}




/*
 * Refuel the player's lamp or torch
 */
void do_cmd_refuel(void)
{
	object_type *o_ptr;

	/* Get the light */
	o_ptr = &inventory[INVEN_LITE];

	/* It is nothing */
	if (o_ptr->tval != TV_LIGHT)
	{
		msg_print("You are not wielding a light.");
	}

	/* It's a lamp */
	else if (o_ptr->sval == SV_LIGHT_LANTERN)
	{
		do_cmd_refuel_lamp(NULL, 0);
	}

	/* It's a torch */
	else if (o_ptr->sval == SV_LIGHT_TORCH)
	{
		do_cmd_refuel_torch(NULL, 0);
	}

	/* No torch to refuel */
	else
	{
		msg_print("Your light cannot be refueled.");
	}
}



/*
 * Target command
 */
void do_cmd_target(void)
{
	/* Target set */
	if (target_set_interactive(TARGET_KILL, 0))
	{
		msg_print("Target Selected.");
	}

	/* Target aborted */
	else
	{
		msg_print("Target Aborted.");
	}
}



/*
 * Look command
 */
void do_cmd_look(void)
{
	/* Look around */
	if (target_set_interactive(TARGET_LOOK, 0))
	{
		msg_print("Target Selected.");
	}
}



/*
 * Allow the player to examine other sectors on the map
 */
void do_cmd_locate(void)
{
	int dir, y1, x1, y2, x2;

	/* Start at current panel */
	y2 = y1 = p_ptr->wy;
	x2 = x1 = p_ptr->wx;

	/* Show panels until done */
	while (TRUE)
	{
		/* Assume no direction */
		dir = 0;

		/* Get a direction */
		while (!dir)
		{
			char command;

			/* Get a command (or Cancel) */
			if (!get_com("Shift viewpoint in which direction? ", &command)) break;

			/* Extract direction */
			dir = target_dir(command);

			/* Error */
			if (!dir) bell("Illegal direction for look (around dungeon)!");
		}

		/* No direction */
		if (!dir) break;

		/* Apply the motion */
		y2 += (ddy[dir] * PANEL_HGT);
		x2 += (ddx[dir] * PANEL_WID);

		/* Verify the row */
		if (y2 > p_ptr->cur_map_hgt - SCREEN_HGT) y2 = p_ptr->cur_map_hgt - SCREEN_HGT;
		if (y2 < 0) y2 = 0;

		/* Verify the col */
		if (x2 > p_ptr->cur_map_wid - SCREEN_WID) x2 = p_ptr->cur_map_wid - SCREEN_WID;
		if (x2 < 0) x2 = 0;

		/* Handle "changes" */
		if ((p_ptr->wy != y2) || (p_ptr->wx != x2))
		{
			/* Update panel */
			p_ptr->wy = y2;
			p_ptr->wx = x2;

			/* Redraw map */
			p_ptr->redraw |= (PR_MAP);

			/* Window stuff */
			p_ptr->window |= (PW_OVERHEAD);

			/* Handle stuff */
			handle_stuff();
		}
	}

	/* Verify panel */
	p_ptr->update |= (PU_PANEL);

	/* Handle stuff */
	handle_stuff();
}

/*
 * The table of "symbol info" -- each entry is a string of the form
 * "X:desc" where "X" is the trigger, and "desc" is the "info".
 */
static cptr ident_info[] =
{
	" :A dark grid",
	"!:A potion (or oil)",
	"\":An amulet",
	"#:A wall",
	/* "$:unused", */
	"%:A quartz vein",
	"&:A plant",
	"':An open door",
	"(:Soft armour",
	"):A shield",
	"*:A gem (or unseen monster)", 
	"+:A closed door",
	",:Food",
	"-:Arrows",
	".:Floor",
	"/:An axe or polearm",
	"0:A forge",
	/* "1:unused", */
	/* "2:unused", */
	/* "3:unused", */
	/* "4:unused", */
	/* "5:unused", */
	/* "6:unused", */
	/* "7:unused", */
	/* "8:unused", */
	/* "9:unused", */
	"::Rubble",
	";:A glyph of warding",
	"<:A staircase up",
	"=:A ring",
	">:A staircase down",
	"?:An instrument",
	"@:Elf, Dwarf, or Man",
	/* "A:unused", */
	/* "B:unused", */
	"C:Canine",
	"D:Dragon",
	/* "E:unused", */
	/* "F:unused", */
	"G:Giant",
	"H:Horror",
	"I:Insect",
	/* "J:unused", */
	/* "K:unused", */
	/* "L:unused", */
	"M:Spider",
	"N:Nameless Thing",
	/* "O:unused", */
	"P:Giant",
	/* "Q:unused", */
	"R:Rauko",
	"S:Ancient Serpent",
	"T:Troll",
	/* "U:unused", */
	"V:Valar",
	"W:Wight/Wraith",
	/* "X:unused", */
	/* "Y:unused", */
	/* "Z:unused", */
	"[:Mail",
	"\\:A blunt weapon (or digger)",
	"]:Misc. armour",
	"^:A trap",
	"_:A staff",
	/* "`:unused", */
	/* "a:unused", */
	"b:Bat/Bird",
	/* "c:unused", */
	"d:Dragon",
	/* "e:unused", */
	"f:Feline",
	/* "g:unused", */
	/* "h:unused", */
	/* "i:unused", */
	/* "j:unused", */
	/* "k:unused", */
	/* "l:unused", */
	"m:Young Spider",
	/* "n:unused", */
	"o:Orc",
	/* "p:unused", */
	/* "q:unused", */
	/* "r:unused", */
	"s:Serpent",
	/* "t:unused", */
	/* "u:unused", */
	"v:Vampire",
	"w:Creeping Shadow",
	/* "x:unused", */
	/* "y:unused", */
	/* "z:unused", */
	/* "{:unused", */
	"|:An edged weapon (sword/dagger/etc)",
	"}:A bow",
	"~:A tool (or miscellaneous item)",
	NULL
};


/*
 * Sorting hook -- Comp function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform on "u".
 */
bool ang_sort_comp_hook(const void *u, const void *v, int a, int b)
{
	u16b *who = (u16b*)(u);

	u16b *why = (u16b*)(v);

	int w1 = who[a];
	int w2 = who[b];

	int z1, z2;


	/* Sort by player kills */
	if (*why >= 4)
	{
		/* Extract player kills */
		z1 = l_list[w1].pkills;
		z2 = l_list[w2].pkills;

		/* Compare player kills */
		if (z1 < z2) return (TRUE);
		if (z1 > z2) return (FALSE);
	}


	/* Sort by total kills */
	if (*why >= 3)
	{
		/* Extract total kills */
		z1 = l_list[w1].tkills;
		z2 = l_list[w2].tkills;

		/* Compare total kills */
		if (z1 < z2) return (TRUE);
		if (z1 > z2) return (FALSE);
	}


	/* Sort by monster level */
	if (*why >= 2)
	{
		/* Extract levels */
		z1 = r_info[w1].level;
		z2 = r_info[w2].level;

		/* Compare levels */
		if (z1 < z2) return (TRUE);
		if (z1 > z2) return (FALSE);
	}


	/* Sort by monster depth */
	if (*why >= 1)
	{
		/* Extract experience */
		z1 = r_info[w1].level;
		z2 = r_info[w2].level;

		/* Compare experience */
		if (z1 < z2) return (TRUE);
		if (z1 > z2) return (FALSE);
	}


	/* Compare indexes */
	return (w1 <= w2);
}


/*
 * Sorting hook -- Swap function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform.
 */
void ang_sort_swap_hook(void *u, void *v, int a, int b)
{
	u16b *who = (u16b*)(u);

	u16b holder;

	/* Unused parameter */
	(void)v;

	/* Swap */
	holder = who[a];
	who[a] = who[b];
	who[b] = holder;
}


/*
 * Identify a character, allow recall of monsters
 *
 * Several "special" responses recall "multiple" monsters:
 *   ^A (all monsters)
 *   ^U (all unique monsters)
 *   ^N (all non-unique monsters)
 *
 * The responses may be sorted in several ways, see below.
 *
 *
 */
void do_cmd_query_symbol(void)
{
	int i, n, r_idx;
	char sym, query;
	char buf[128];

	bool all = FALSE;
	bool uniq = FALSE;
	bool norm = FALSE;

	bool recall = FALSE;

	u16b why = 0;
	u16b *who;


	/* Get a character, or abort */
	if (!get_com("Enter character to be identified: ", &sym)) return;

	/* Find that character info, and describe it */
	for (i = 0; ident_info[i]; ++i)
	{
		if (sym == ident_info[i][0]) break;
	}

	/* Describe */
	if (sym == KTRL('A'))
	{
		all = TRUE;
		my_strcpy(buf, "Full monster list.", sizeof (buf));
	}
	else if (sym == KTRL('U'))
	{
		all = uniq = TRUE;
		my_strcpy(buf, "Unique monster list.", sizeof (buf));
	}
	else if (sym == KTRL('N'))
	{
		all = norm = TRUE;
		my_strcpy(buf, "Non-unique monster list.", sizeof (buf));
	}
	else if (ident_info[i])
	{
		strnfmt(buf, sizeof(buf), "%c - %s.", sym, ident_info[i] + 2);
	}
	else
	{
		strnfmt(buf, sizeof(buf), "%c - %s.", sym, "Unknown Symbol");
	}

	/* Display the result */
	prt(buf, 0, 0);

	/* Allocate the "who" array */
	C_MAKE(who, z_info->r_max, u16b);

	/* Collect matching monsters */
	for (n = 0, i = 1; i < z_info->r_max - 1; i++)
	{
		monster_race *r_ptr = &r_info[i];
		monster_lore *l_ptr = &l_list[i];

		/* Nothing to recall */
		if (!cheat_know && !l_ptr->tsights && !p_ptr->active_ability[S_PER][PER_LORE2]) continue;

		/* Require non-unique monsters if needed */
		if (norm && (r_ptr->flags1 & (RF1_UNIQUE))) continue;

		/* Require unique monsters if needed */
		if (uniq && !(r_ptr->flags1 & (RF1_UNIQUE))) continue;

		// Ignore monsters that can't be generated
		if (r_ptr->level > 25) continue;

		/* Collect "appropriate" monsters */
		if (all || (r_ptr->d_char == sym)) who[n++] = i;
	}

	/* Nothing to recall */
	if (!n)
	{
		/* XXX XXX Free the "who" array */
		FREE(who);

		return;
	}


	/* Prompt */
	put_str("Recall details? (k/p/y/n): ", 0, 40);

	/* Query */
	query = inkey();

	/* Restore */
	prt(buf, 0, 0);


	/* Sort by kills (and level) */
	if (query == 'k')
	{
		why = 4;
		query = 'y';
	}

	/* Sort by level */
	if (query == 'p')
	{
		why = 2;
		query = 'y';
	}

	/* Catch "escape" */
	if (query != 'y')
	{
		/* XXX XXX Free the "who" array */
		FREE(who);

		return;
	}

	/* Sort if needed */
	if (why)
	{
		/* Select the sort method */
		ang_sort_comp = ang_sort_comp_hook;
		ang_sort_swap = ang_sort_swap_hook;

		/* Sort the array */
		ang_sort(who, &why, n);
	}


	/* Start at the end */
	i = n - 1;

	/* Scan the monster memory */
	while (1)
	{
		/* Extract a race */
		r_idx = who[i];

		/* Hack -- Auto-recall */
		monster_race_track(r_idx);

		/* Hack -- Handle stuff */
		handle_stuff();

		/* Hack -- Begin the prompt */
		roff_top(r_idx);

		/* Hack -- Complete the prompt */
		Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");

		/* Interact */
		while (1)
		{
			/* Recall (raging players don't get recall) */
			if (recall)
			{
				/* Save screen */
				screen_save();

				/* Recall on screen */
				screen_roff(who[i]);

				/* Hack -- Complete the prompt (again) */
				Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");
			}

			/* Command */
			query = inkey();

			/* Unrecall */
			if (recall)
			{
				/* Load screen */
				screen_load();
			}

			/* Normal commands */
			if (query != 'r') break;

			/* Toggle recall */
			recall = !recall;
		}

		/* Stop scanning */
		if (query == ESCAPE) break;

		/* Move to "prev" monster */
		if (query == '-')
		{
			if (++i == n)
			{
				i = 0;
			}
		}

		/* Move to "next" monster */
		else
		{
			if (i-- == 0)
			{
				i = n - 1;
			}
		}
	}


	/* Re-display the identity */
	prt(buf, 0, 0);

	/* Free the "who" array */
	FREE(who);
}

