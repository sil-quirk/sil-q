/* File: melee1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * Critical hits by monsters can inflict cuts and stuns.
 *
 * The chance is greater for WOUND and BATTER attacks
 */
static bool monster_cut_or_stun(int crit_bonus_dice, int net_dam, int effect)
{
	if (net_dam <= 0) return (FALSE);

	/* Special case -- wounding/battering attack */
	if ((effect == RBE_WOUND) || (effect == RBE_BATTER))
	{
		if (crit_bonus_dice >= dieroll(2)) return (TRUE);
	}

	/* Standard attack */
	else
 	{
		if (one_in_(10))
		{
			if (crit_bonus_dice >= dieroll(2)) return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Determine whether there is a bonus die for an elemental attack that
 * the player doesn't resist
 */
int elem_bonus(int effect)
{
	int resistance = 1;

	switch (effect)
	{
		case RBE_FIRE:
			resistance = resist_fire();
			break;
		case RBE_COLD:
			resistance = resist_cold();
			break;
		case RBE_POISON:
			resistance = resist_pois();
			break;
		case RBE_DARK:
			resistance = resist_dark();
			break;
		default:
			return (0);
	}
	
	if (resistance == 1) return (1);
	if (resistance < 0)  return (-resistance);
	
	return (0);
}

/*
 * Roll the protection dice for all parts of the player's armour
 */
extern int protection_roll(int typ, bool melee)
{
	int i;
	object_type *o_ptr;
	int prt = 0;
	int mult = 1;
	int armour_weight = 0;
	
	// things that always count:
	
	if (singing(SNG_STAYING))
	{
		prt += damroll(1, MAX(1, ability_bonus(S_SNG, SNG_STAYING)));
	}
	
	if (p_ptr->active_ability[S_WIL][WIL_HARDINESS])
	{
		prt += damroll(1, p_ptr->skill_use[S_WIL] / 6);
	}
	
	// armour:
	
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
        
        // off-hand weapons are not armour, so skip them
		if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)) continue;
        
		if (i >= INVEN_BODY) armour_weight += o_ptr->weight;
        
		// fire and cold and generic 'hurt' all check the shield
		if (i == INVEN_ARM)
		{
			if ((typ == GF_HURT) || (typ == GF_FIRE) || (typ == GF_COLD))
			{
				if (p_ptr->active_ability[S_EVN][EVN_BLOCKING] && (!melee || (p_ptr->previous_action[0] == 5)))
				{
					mult = 2;
				}
				if (o_ptr->pd > 0)
				{
					prt += damroll(o_ptr->pd * mult, o_ptr->ps);
				}
			}
		}
		
		// also add protection if damage is generic 'hurt' or it is a ring or amulet slot
		else if ((typ == GF_HURT) || (i == INVEN_LEFT) || (i == INVEN_RIGHT) || (i == INVEN_NECK))
		{
			if (o_ptr->ps > 0)
			{
				prt += damroll(o_ptr->pd, o_ptr->ps);
			}
		}
	}

	// heavy armour bonus
	if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR] && (typ == GF_HURT))
	{
		prt += damroll(1, armour_weight / 150);
	}
	
	return prt;
}


/*
 * Roll the protection dice for all parts of the player's armour
 */
extern int p_min(int typ, bool melee)
{
	int i;
	object_type *o_ptr;
	int prt = 0;
	int armour_weight = 0;
	int mult = 1;
	
	// things that always count:
	
	if (singing(SNG_STAYING))
	{
		if (ability_bonus(S_SNG, SNG_STAYING) > 0)
		{
			prt += 1;
		}
	}
	
	if (p_ptr->active_ability[S_WIL][WIL_HARDINESS])
	{
		if (p_ptr->skill_use[S_WIL] >= 6) prt += 1;
	}
	
	// armour:
	
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
        
        // off-hand weapons are not armour, so skip them
		if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)) continue;

		if (i >= INVEN_BODY) armour_weight += o_ptr->weight;

		// fire and cold and generic 'hurt' all check the shield
		if (i == INVEN_ARM)
		{
			if ((typ == GF_HURT) || (typ == GF_FIRE) || (typ == GF_COLD))
			{
				if (p_ptr->active_ability[S_EVN][EVN_BLOCKING] && 
				    (!melee || ((p_ptr->previous_action[0] == 5) || ((p_ptr->previous_action[0] == ACTION_NOTHING) && (p_ptr->previous_action[1] == 5)))))
				{
					mult = 2;
				}
				if (o_ptr->pd > 0)
				{
					prt += o_ptr->pd * mult;
				}
			}
		}
		
		// generic 'hurt' uses everything else too
		else if ((typ == GF_HURT) || (i == INVEN_LEFT) || (i == INVEN_RIGHT) || (i == INVEN_NECK))
		{
			if (o_ptr->ps > 0)
			{
				prt += o_ptr->pd;
			}
		}
	}

	// heavy armour bonus
	if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR] && (typ == GF_HURT))
	{
		if (armour_weight / 150 > 0) prt += 1;
	}
	
	return prt;
}


/*
 * Roll the protection dice for all parts of the player's armour
 */
extern int p_max(int typ, bool melee)
{
	int i;
	object_type *o_ptr;
	int prt = 0;
	int armour_weight = 0;
	int mult = 1;
	
	// things that always count:
	
	if (singing(SNG_STAYING))
	{
		prt += ability_bonus(S_SNG, SNG_STAYING);
	}
	
	if (p_ptr->active_ability[S_WIL][WIL_HARDINESS])
	{
		prt += p_ptr->skill_use[S_WIL] / 6;
	}
	
	// armour:
	
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
        
        // off-hand weapons are not armour, so skip them
		if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD)) continue;

        if (i >= INVEN_BODY) armour_weight += o_ptr->weight;

		// fire and cold and generic 'hurt' all check the shield
		if (i == INVEN_ARM)
		{
			if ((typ == GF_HURT) || (typ == GF_FIRE) || (typ == GF_COLD))
			{
				if (p_ptr->active_ability[S_EVN][EVN_BLOCKING] && 
				    (!melee || ((p_ptr->previous_action[0] == 5) || ((p_ptr->previous_action[0] == ACTION_NOTHING) && (p_ptr->previous_action[1] == 5)))))
				{
					mult = 2;
				}
				if (o_ptr->pd > 0)
				{
					prt += o_ptr->pd * mult * o_ptr->ps;
				}
			}
		}
		
		// generic 'hurt' uses everything else too
		else if ((typ == GF_HURT) || (i == INVEN_LEFT) || (i == INVEN_RIGHT) || (i == INVEN_NECK))
		{
			if (o_ptr->ps > 0)
			{
				prt += o_ptr->pd * o_ptr->ps;
			}
		}
	}
	
	// heavy armour bonus
	if (p_ptr->active_ability[S_EVN][EVN_HEAVY_ARMOUR] && (typ == GF_HURT))
	{
		prt += armour_weight / 150;
	}
	
	return prt;
}



/*
 * determines the size of the evasion bonus due to dodging (if any)
 */

int dodging_bonus(void)
{
	if (p_ptr->active_ability[S_EVN][EVN_DODGING] &&
	    (p_ptr->previous_action[0] >= 1) &&
	    (p_ptr->previous_action[0] <= 9) &&
	    (p_ptr->previous_action[0] != 5))
	{
		return 3;
	}
	else
	{
		return 0;
	}
}

/*
 * Determine whether a monster is making a valid charge attack
 */
bool monster_charge(monster_type *m_ptr)
{
    int d, i;
    
    int speed;
    
    int deltay = p_ptr->py - m_ptr->fy;
    int deltax = p_ptr->px - m_ptr->fx;
    
    monster_race *r_ptr = &r_info[m_ptr->r_idx];
    
    // paranoia
    if (distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) > 1) return (FALSE);
        
    // determine the monster speed
    speed = r_ptr->speed;
    if (m_ptr->slowed) speed--;
    
    // if it has the ability and isn't slow
    if ((r_ptr->flags2 & (RF2_CHARGE)) && (speed >= 2))
    {
        // try all three directions
        for (i = -1; i <= 1; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];
            
            if (m_ptr->previous_action[1] == d)
            {
                return (TRUE);
            }		
        }
    }
    
    return (FALSE);
}

/*
 * Attack the player via physical attacks.
 */
bool make_attack_normal(monster_type *m_ptr)
{
	int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	int i, k, heal;
	int do_cut, do_stun;
	
	int b, blows;
	
	bool alive = TRUE;

	object_type *o_ptr;

	char o_name[120];

	char m_name[80];

	char ddesc[80];

	bool blinked;
	
	int prt_percent = 100;   // a default value to soothe compilation warnings
	
	int dam_type;

	/* Not allowed to attack */
	if (r_ptr->flags1 & (RF1_NEVER_BLOW)) return (FALSE);

	/* Get the monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Get the "died from" information (i.e. "a white worm mass") */
	monster_desc(ddesc, sizeof(ddesc), m_ptr, 0x88);

	/* Assume no blink */
	blinked = FALSE;

	/* Calculate the number of blows this monster gets */
	for (b = 0; b < MONSTER_BLOW_MAX; b++)
	{
		if (!r_ptr->blow[b].method) break;
	}
	blows = b;
	
	/* Monsters might notice */
	attacked_player = TRUE;

	// use the alternate attack one in three times
	if ((blows > 1) && one_in_(3))	b = 1;
	else							b = 0;

	// introduce a new code block to all us to declare all these variables
	if (TRUE)
	{
		bool pure_dam = FALSE;
		bool visible = FALSE;
		bool obvious = FALSE;

		int total_attack_mod = 0;
		int total_evasion_mod = 0;
		int hit_result = 0;

		bool no_crit = FALSE;
		int crit_bonus_dice = 0;
		int elem_bonus_dice = 0;
		
		int dam = 0, prt = 0;
		int net_dam = 0;
		
		cptr act = NULL;
		char msg[80];

		/* Extract the attack information */
		int effect = r_ptr->blow[b].effect;
		int method = r_ptr->blow[b].method;
		int att = r_ptr->blow[b].att;
		int dd = r_ptr->blow[b].dd;
		int ds = r_ptr->blow[b].ds;

		/* Hack -- no more attacks */
		//if (!method) break;  // Sil-y: not needed as this is no longer a loop

		/* Handle "leaving" */
		//if (p_ptr->leaving) break;   // Sil-y: not needed as this is no longer a loop

		/* Extract visibility (before blink) */
		if (m_ptr->ml) visible = TRUE;

		/* Assume no cut, stun, or touch */
		do_cut = do_stun = 0;

		// determine the monster's attack score
		total_attack_mod = total_monster_attack(m_ptr, att);
        
        if (monster_charge(m_ptr))
        {
            total_attack_mod += 3;
            ds += 3;
        }
	
		// determine the player's evasion score
		total_evasion_mod = total_player_evasion(m_ptr, FALSE);

		/* Check if the player was hit */
		// spores always hit (and never critical)
		if (method == RBM_SPORE)
		{
			pure_dam = TRUE;
			hit_result = 1;
			update_combat_rolls1b(m_ptr, PLAYER, m_ptr->ml); 
		}
		else
		{
			hit_result = hit_roll(total_attack_mod, total_evasion_mod, m_ptr, PLAYER, TRUE);
		}
		
		/* Monster hits player */
		if (!effect || (hit_result > 0))
		{
			/* Always disturbing */
			disturb(1, 0);

			/* Describe the attack method, apply special hit chance mods. */
			switch (method)
			{
				case RBM_HIT:
				{
					/* Handle special effect types */
					if (effect == RBE_BATTER)
					{
						act = "batters you";
					}
					else
					{
						act = "hits you";
					}
                    
					do_cut = do_stun = 1;

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_TOUCH:
				{
					act = "touches you";
					
					// ignores armor
					prt_percent = 0;
					
					// can't do criticals
					no_crit = TRUE;
					
					break;
				}
				case RBM_CLAW:
				{
                    act = "claws you";
					do_cut = 1;

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_BITE:
				{
					act = "bites you";
					do_cut = 1;

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_PECK:
				{
					act = "pecks you";
					do_cut = 1;

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_STING:
				{
					act = "stings you";

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_CRUSH:
				{
					if (dam >= 10) act = "crushes you";
					do_stun = 1;

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_ENGULF:
				{
					act = "engulfs you";

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_CRAWL:
				{
					act = "crawls on you";

					// stopped by armor
					prt_percent = 100;

					break;
				}
				case RBM_THORN:
				{
					act = "tears at you";
                    
					// stopped by armor
					prt_percent = 100;
                    
					break;
				}
				case RBM_SPORE:
				{
					act = "releases a cloud of spores";

					// ignores armor
					prt_percent = 0;

					// can't do criticals
					no_crit = TRUE;

					break;
				}
				case RBM_WHIP:
				{
					act = "whips you";

					// stopped by armor
					prt_percent = 100;

					break;
				}
			}

			/* Determine critical-hit bonus dice (if any) */
			// treats attack a weapon weighing 2 pounds per damage die
			crit_bonus_dice = crit_bonus(hit_result, 20 * dd, &r_info[0], S_MEL, FALSE);

			/* Determine elemental attack bonus dice (if any)  */
			elem_bonus_dice = elem_bonus(effect);
			
			/* certain attacks can't do criticals */
			if (no_crit) crit_bonus_dice = 0;
			
			/* Roll out the damage */
			dam = damroll(dd + crit_bonus_dice + elem_bonus_dice, ds);
			
			/* Determine the armour based damage-reduction for the player */
			/* Note that some attack types should ignore this             */
			prt = protection_roll(GF_HURT, TRUE);

			// now calculate net_dam, taking (modified) protection into account
			prt = (prt * prt_percent) / 100;
			net_dam = (dam - prt > 0) ? (dam - prt) : 0;

			/* Message */
			if (act)
			{
				char punctuation[20];

				// determine the punctuation for the attack ("...", ".", "!" etc)
				attack_punctuation(punctuation, net_dam, crit_bonus_dice);
							
                if (monster_charge(m_ptr))
                {
                    // remember that the monster can do this
                    if (m_ptr->ml)  l_ptr->flags2 |= (RF2_CHARGE);
                    
                    act = "charges you";
                }
                
				/* Message */
				if (act) msg_format("%^s %s%s", m_name, act, punctuation);
			}

			/* Hack -- assume all attacks are obvious */
			obvious = TRUE;
			
			// default damage type:
			dam_type = GF_HURT;

			/* Apply appropriate damage */
			switch (effect)
			{

				/* No effect */
				case 0:
				{
					/* Hack -- Assume obvious */
					obvious = TRUE;

					/* Hack -- No damage */
					net_dam = 0;

					break;
				}

				/* Ordinary hit */
				case RBE_HURT:
				{
					/* Obvious */
					obvious = TRUE;

					/* Take damage */
					take_hit(net_dam, ddesc);

					break;
				}

				/* Hit with increased chance to wound */
				case RBE_WOUND:
 				{
					/* Obvious */
					obvious = TRUE;

 					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Usually don't stun */
					if ((do_stun) && (!one_in_(5))) do_stun = FALSE;

					/* Always give a chance to inflict cuts */
					do_cut = TRUE;

					break;
				}

				/* Hit with increased chance to stun */
				case RBE_BATTER:
				{
					/* Obvious */
					obvious = TRUE;

					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Usually don't cut */
					if ((do_cut) && (!one_in_(5))) do_cut = FALSE;

					/* Always give a chance to inflict stuns */
					do_stun = TRUE;

					break;
				}

				/* Hit to cause earthquakes */
				case RBE_SHATTER:
				{
					/* Obvious */
					obvious = TRUE;

					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Usually don't cut */
					if ((do_cut) && (!one_in_(5))) do_cut = FALSE;

					/* Always give a chance to inflict stuns */
					do_stun = TRUE;

 					break;
 				}

				/* Hit to disenchant */
 				case RBE_UN_BONUS:
 				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Apply disenchantment */
					if (apply_disenchant(0)) obvious = TRUE;

					break;
				}

				/* Hit to reduce charges of magical items */
				case RBE_UN_POWER:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Find an item */
					for (k = 0; k < 20; k++)
					{

						/* Blindly hunt ten times for an item. */
						i = rand_int(INVEN_PACK);

						/* Obtain the item */
						o_ptr = &inventory[i];

						/* Skip non-objects */
						if (!o_ptr->k_idx) continue;

						/* Drain charged staffs */
						if (o_ptr->tval == TV_STAFF)
						{
							if (o_ptr->pval)
							{
								int counter;

								heal = r_ptr->level;

								/* Message */
								msg_print("Energy drains from your pack!");

								/* Obvious */
								obvious = TRUE;

								/*get the number of rods/wands/staffs to be drained*/
								if (o_ptr->tval == TV_STAFF)
								{
									counter = o_ptr->pval;

									/*get the number of charges to be drained*/
									while ((counter > 1) && (!one_in_(counter)))
									{
										/*reduce by one*/
										counter--;
									}

									/*drain the wands/staffs*/
									o_ptr->pval -= counter;

									/*factor healing times the difference*/
									heal *= counter;
								}

								/* Message */
								if ((m_ptr->hp < m_ptr->maxhp) && (heal))
								{
									if (m_ptr->ml) msg_format("%^s looks healthier.",  m_name);
									else msg_format("%^s sounds healthier.", m_name);
								}

								/*heal is greater than monster wounds, restore mana too*/
								if (heal > (m_ptr->maxhp - m_ptr->hp))
								{

									/*leave some left over for mana*/
									heal -= (m_ptr->maxhp - m_ptr->hp);

									/*fully heal the monster*/
									m_ptr->hp = m_ptr->maxhp;

									/*mana is more powerful than HP*/
									heal /= 10;

									/* if heal was less than 10, make it 1*/
									if (heal < 1) heal = 1;

									/*give message if anything left over*/
									if (m_ptr->mana < MON_MANA_MAX)
									{
										if (m_ptr->ml) msg_format("%^s looks refreshed.", m_name);
										else msg_format("%^s sounds refreshed.", m_name);
									}

									/*add mana*/
									m_ptr->mana += heal;

									if (m_ptr->mana > MON_MANA_MAX) m_ptr->mana = MON_MANA_MAX;
								}

								/* Simple Heal */
								else m_ptr->hp += heal;

								/* Redraw (later) if needed */
								if (p_ptr->health_who == m_idx)
									p_ptr->redraw |= (PR_HEALTHBAR);

								/* Combine / Reorder the pack */
								p_ptr->notice |= (PN_COMBINE | PN_REORDER);

								/* Window stuff */
								p_ptr->window |= (PW_INVEN);

								/* not more than one inventory
								 * slot effected. */
								break;
							}
						}
					}

					break;
				}

				/* Hit to reduce mana */
				case RBE_LOSE_MANA:
				{
					int drain;

					char msg_tmp[80];
					my_strcpy(msg_tmp, msg, sizeof(msg_tmp));

					/* Obvious */
					obvious = TRUE;

					/* Damage (mana) */
					if (net_dam > 0 || dam == 0)
					{
						if (saving_throw(m_ptr, 0))
						{
							my_strcat(msg_tmp, "  You resist the effects.", sizeof(msg_tmp));
						}
						else
						{
							if (p_ptr->csp)
							{
								/* Drain depends on maximum mana */
								drain = 2 + rand_int(p_ptr->msp / 10);

								/* Drain the mana */
								if (drain > p_ptr->csp)
								{
									p_ptr->csp = 0;
									p_ptr->csp_frac = 0;

									my_strcat(msg_tmp, "  Your voice fails you!", sizeof(msg_tmp));
								}
								else
								{
									p_ptr->csp -= drain;
									my_strcat(msg_tmp, "  Your voice wavers.", sizeof(msg_tmp));
								}

								/* Redraw mana */
								p_ptr->redraw |= (PR_VOICE);

								/* Window stuff */
								p_ptr->window |= (PW_PLAYER_0);
							}
						}
					}
					/* Damage (physical) */
					take_hit(net_dam, ddesc);

					break;
				}

				/* Hit to slow */
				case RBE_SLOW:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);
					
					/* Increase "slow" */
					if (net_dam > 0 || dam == 0)
					{
						if (!allow_player_slow(m_ptr))
						{
							msg_print("You resist the effects!");
							obvious = TRUE;
						}
						else if (set_slow(p_ptr->slow + damroll(2,4)))
						{
							obvious = TRUE;
						}
					}

					break;
				}

				/* Hit to steal objects from the pack */
				case RBE_EAT_ITEM:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Blindly scrabble in the backpack ten times */
					for (k = 0; k < 10; k++)
					{
						object_type *i_ptr;
						object_type object_type_body;

						/* Pick an item */
						i = rand_int(INVEN_PACK);

						/* Obtain the item */
						o_ptr = &inventory[i];

						/* Skip non-objects */
						if (!o_ptr->k_idx) continue;

						/* Skip artefacts */
						if (artefact_p(o_ptr)) continue;

						/* Get a description */
						object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 3);

						// Sil-y: perhaps need a PER check to notice?

						/* Message */
						msg_format("%sour %s (%c) was stolen!",
						           ((o_ptr->number > 1) ? "One of y" : "Y"),
						           o_name, index_to_label(i));

						/* Get local object */
						i_ptr = &object_type_body;

						/* Obtain local object */
						object_copy(i_ptr, o_ptr);

						/* One item is stolen at a time. */
						i_ptr->number = 1;

						/* Carry the object */
						(void)monster_carry(m_idx, i_ptr);

						/* Steal the items */
						inven_item_increase(i, -1);
						inven_item_optimize(i);

						/* Obvious */
						obvious = TRUE;

						/* Done */
						break;
					}

					break;
				}

				/* Hit to eat food */
				case RBE_EAT_FOOD:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Steal some food */
					for (k = 0; k < 6; k++)
					{
						/* Pick an item from the pack */
						i = rand_int(INVEN_PACK);

						/* Get the item */
						o_ptr = &inventory[i];

						/* Skip non-objects */
						if (!o_ptr->k_idx) continue;

						/* Skip non-food objects */
						if (o_ptr->tval != TV_FOOD) continue;

						/* Get a description */
						object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

						/* Message */
						msg_format("%s %s (%c) was eaten!",
							   ((o_ptr->number > 1) ? "One of your" : "Your last"),
							   o_name, index_to_label(i));

						/* Steal the items */
						inven_item_increase(i, -1);
						inven_item_optimize(i);

						/* Obvious */
						obvious = TRUE;

						/* Done */
						break;
					}

					break;
				}

				/* Hit to reduce nutrition */
				case RBE_HUNGER:
				{
					int amount = 500;

					obvious = TRUE;

					/* Take damage */
					take_hit(net_dam, ddesc);

					/* We're not dead yet */
					if (!p_ptr->is_dead && (net_dam > 0 || dam == 0))
					{
						/* Message -- only if appropriate */
						if (!saving_throw(m_ptr, 0))
						{
							msg_print("You feel an unnatural hunger...");
							
							// modify the hunger caused by the player's hunger rate
							// but go up/down by factors of 1.5 rather than 3
							if (p_ptr->hunger < 0)
							{ 
								amount *= int_exp(2,-(p_ptr->hunger));
								amount /= int_exp(3,-(p_ptr->hunger));
							}
							if (p_ptr->hunger > 0)
							{ 
								amount *= int_exp(3,p_ptr->hunger);
								amount /= int_exp(2,p_ptr->hunger);
							}
							
							/* Reduce food counter, but not too much. */
							set_food(p_ptr->food - amount);
						}
					}

					break;
				}

				/* Hit to inflict acid damage */
				case RBE_ACID:
				{
					/* Obvious */
					obvious = TRUE;

					/* Message */
					msg_print("You are covered in acid!");

					/* Special damage */
					acid_dam(net_dam, ddesc);

					dam_type = GF_ACID;


					break;
				}

				/* Hit to electrocute */
				case RBE_ELEC:
				{
					/* Obvious */
					obvious = TRUE;

					/* Message */
					if (net_dam > 0) msg_print("You are struck by electricity!");

					/* Take damage (special) */
					elec_dam(net_dam, ddesc);

					dam_type = GF_ELEC;

					break;
				}

				/* Hit to darken */
				case RBE_DARK:
				{
					/* Obvious */
					obvious = TRUE;

					/* Take damage */
					dark_dam_mixed(net_dam, ddesc);
					
					dam_type = GF_DARK;
					
					break;
				}
					
					/* Hit to poison */
				case RBE_POISON:
				{
					/* Take "poison" effect */
					pois_dam_mixed(net_dam);
					
					if (net_dam > 0)
					{
						obvious = TRUE;
					}
					
					dam_type = GF_POIS;
					
					break;
					
				}

				/* Hit to burn */
				case RBE_FIRE:
				{					
					/* Obvious */
					obvious = TRUE;

					/* Message */
					if (net_dam > 0) msg_print("You are enveloped in flames!");

					/* Take damage (special) */
					fire_dam_mixed(net_dam, ddesc);

					dam_type = GF_FIRE;

					break;
				}

				case RBE_COLD:
				{
					/* Obvious */
					obvious = TRUE;

					/* Message */
					if (net_dam > 0) msg_print("You are covered with frost!");

					/* Take damage (special) */
					cold_dam_mixed(net_dam, ddesc);

					dam_type = GF_COLD;

					break;
				}

				case RBE_BLIND:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Increase blindness */
					if (net_dam > 0 || dam == 0)
					{
						if (allow_player_blind(m_ptr))
						{
							if (set_blind(p_ptr->blind + damroll(5,4)))
							{
								obvious = TRUE;
							}
						}
						else
						{
							if (!p_ptr->blind)
							{
								obvious = TRUE;
								msg_print("Your vision quickly clears.");								
							}
						}
					}
					break;
				}

				/* Hit to confuse */
				case RBE_CONFUSE:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Increase "confused" */
					if (net_dam > 0 || dam == 0)
					{
						if (!allow_player_confusion(m_ptr))
						{
							msg_print("You resist the effects.");
							obvious = TRUE;
						}
						else if (set_confused(p_ptr->confused + damroll(2,4)))
						{
							obvious = TRUE;
						}

					}
					break;
				}

				/* Hit to frighten */
				case RBE_TERRIFY:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Increase "afraid" */
					if (!allow_player_fear(m_ptr))
					{
						msg_print("You stand your ground!");
						obvious = TRUE;
					}
					else if (set_afraid(p_ptr->afraid + damroll(2,4)))
					{
						obvious = TRUE;
					}

					break;
				}

				/* Hit to entrance (never cumulative) */
				case RBE_ENTRANCE:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					/* Increase "entranced" */
					if (net_dam > 0 || dam == 0)
					{
						if (!allow_player_entrancement(m_ptr))
						{
							msg_print("You are unaffected!");
							obvious = TRUE;
						}
						else if (!p_ptr->entranced && !p_ptr->was_entranced)
						{
							if (set_entranced(damroll(4, 4)))
							{
								obvious = TRUE;
							}
						}
					}
					break;
				}

				/* Hit to cause disease */
				case RBE_DISEASE:
 				{
					int do_disease = net_dam;

					/* Take (adjusted) damage */
					take_hit(net_dam, ddesc);

					/* Inflict disease */
					if (net_dam > 0 || dam == 0)
					{
						disease(&do_disease);
					}
 					break;
 				}

				case RBE_LOSE_STR:
				case RBE_LOSE_DEX:
				case RBE_LOSE_CON:
				case RBE_LOSE_GRA:
				case RBE_LOSE_STR_CON:
				case RBE_LOSE_ALL:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					if (net_dam > 0 || dam == 0)
					{
						/* Reduce strength */
						if ((effect == RBE_LOSE_STR) || (effect == RBE_LOSE_STR_CON) || (effect == RBE_LOSE_ALL))
						{
							if (do_dec_stat(A_STR, m_ptr)) obvious = TRUE;
						}

						/* Reduce dexterity */
						if ((effect == RBE_LOSE_DEX) || (effect == RBE_LOSE_ALL))
						{
							if (do_dec_stat(A_DEX, m_ptr)) obvious = TRUE;
						}

						/* Reduce constitution */
						if ((effect == RBE_LOSE_CON) || (effect == RBE_LOSE_STR_CON) || (effect == RBE_LOSE_ALL))
						{
							if (do_dec_stat(A_CON, m_ptr)) obvious = TRUE;
						}

						/* Reduce grace */
						if ((effect == RBE_LOSE_GRA) || (effect == RBE_LOSE_ALL))
						{
							if (do_dec_stat(A_GRA, m_ptr)) obvious = TRUE;
						}
					}
					break;
				}

				/* Hit to disarm */
				case RBE_DISARM:
				{
					object_type *o_ptr;
					char o_name[120];

					object_type *i_ptr;
					object_type object_type_body;
					
					int near_y, near_x;
					
					int item = INVEN_WIELD;
					
					int difficulty;
				
					/* Select the melee weapon */
					o_ptr = &inventory[INVEN_WIELD];

					/* Nothing to disamr */
					if (!o_ptr->k_idx) break;

					/* Describe */
					object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

					/* Base difficulty */
					difficulty = 2;
					
					/* Adjustment for two handed weapons */
					if (two_handed_melee())
					{
						difficulty -= 4;
					}
					
					/* Attempt a skill check against strength */
					if (skill_check(m_ptr, difficulty, p_ptr->stat_use[A_STR] * 2, PLAYER) <= 0)
					{
						msg_format("%^s tries to disarm you, but you keep a grip on your weapon.",
								   m_name);
					}

					/* failed check... */
					else
					{
						/* Oops */
						msg_format("%^s disarms you! Your %s falls to the ground nearby.",
								   m_name, o_name);

						/* Get the original object */
						o_ptr = &inventory[item];

						/* Take off equipment */
						if (item >= INVEN_WIELD)
						{
							/* Take off first */
							item = inven_takeoff(item, 1);

							/* Get the original object */
							o_ptr = &inventory[item];
						}

						/* Get local object */
						i_ptr = &object_type_body;

						/* Obtain local object */
						object_copy(i_ptr, o_ptr);

						/* Modify quantity */
						i_ptr->number = 1;

						for (i=0; i < 1000; i++)
						{
							near_y = p_ptr->py + rand_range(-1,1);
							near_x = p_ptr->px + rand_range(-1,1);
							
							if (cave_floor_bold(near_y,near_x)) break;					
						}

						/* Drop it near the player */
						drop_near(i_ptr, 0, near_y, near_x);

						/* Modify, Optimize */
						inven_item_increase(item, -1);
						inven_item_optimize(item);
					}
									
					break;
				}
				
				case RBE_HALLU:
				{
					/* Take damage */
					take_hit(net_dam, ddesc);

					if (net_dam > 0 || dam == 0)
					{
						/* Increase "image" */
						if (!allow_player_image(m_ptr))
						{
							msg_print("You resist the effects.");
							obvious = TRUE;
						}
						else if (set_image(p_ptr->image + damroll(10,4)))
						{
							obvious = TRUE;
						}
					}
					break;
				}
			}

			update_combat_rolls2(dd + crit_bonus_dice + elem_bonus_dice, ds, dam, -1, -1, prt, prt_percent, dam_type, TRUE); 
		
			display_hit(p_ptr->py, p_ptr->px, net_dam, dam_type, p_ptr->is_dead);

			/* Handle character death */
			if (p_ptr->is_dead && (l_ptr->deaths < MAX_SHORT))
			{
				l_ptr->deaths++;

				p_ptr->window |= (PW_COMBAT_ROLLS);
				window_stuff();

				/* Leave immediately */
				return (TRUE);
			}

			/* Hack -- only one of cut or stun */
			if (do_cut && do_stun)
			{
				/* Cancel cut */
				if (one_in_(2))
				{
					do_cut = 0;
				}

				/* Cancel stun */
				else
				{
					do_stun = 0;
				}
			}

			/* Handle cut */
			if (do_cut)
			{
				/* Critical hit */
				if (monster_cut_or_stun(crit_bonus_dice, net_dam, effect))
				{
					(void)set_cut(p_ptr->cut + (net_dam / 2));
				}
			}

			/* Handle stun */
			if (do_stun)
			{
				/* Critical hit */
				if (monster_cut_or_stun(crit_bonus_dice, net_dam, effect))
				{
					if (allow_player_stun(NULL))
					{ 
						(void)set_stun(p_ptr->stun + net_dam);
					}
				}
			}
            
            // deal with Cruel Blow
            if ((r_ptr->flags2 & (RF2_CRUEL_BLOW)) && (crit_bonus_dice >= 1) && (net_dam > 0))
            {
                // Sil-y: ideally we'd use a call to allow_player_confuse() here, but that doesn't
                //        work as it can't take the level of the critical into account.
                //        Sadly my solution doesn't let you ID confusion resistance items.
                int difficulty = p_ptr->skill_use[S_WIL] + (p_ptr->resist_confu * 10);
                
                if (skill_check(m_ptr, crit_bonus_dice * 4, difficulty, PLAYER) > 0)
                {
                    // remember that the monster can do this
                    if (m_ptr->ml)  l_ptr->flags2 |= (RF2_CRUEL_BLOW);
                    
                    msg_format("You reel in pain!");
                    
                    // confuse the player
                    set_confused(p_ptr->confused + crit_bonus_dice);
                }
            }

            // deal with Knock Back
            if (r_ptr->flags2 & (RF2_KNOCK_BACK))
            {
                // only happens on the main attack (so that bites don't knock you back)
                if (b == 0)
                {
                    // determine if the player is knocked back
                    if (skill_check(m_ptr, monster_stat(m_ptr, A_STR) * 2, p_ptr->stat_use[A_CON] * 2, PLAYER) > 0)
                    {
                        // do the knocking back
                        knock_back(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px);
                        
                        // remember that the monster can do this
                        if (m_ptr->ml)  l_ptr->flags2 |= (RF2_KNOCK_BACK);
                    }
                }
            }
            
            // deal with cowardice
			if ((p_ptr->cowardice > 0) && (net_dam >= 10 / p_ptr->cowardice))
			{
                if (!p_ptr->afraid)
                {
                    if (allow_player_fear(m_ptr))
                    {
                        set_afraid(p_ptr->afraid + damroll(10,4));
                        set_fast(p_ptr->fast + damroll(5,4));
                        
                        // give the player a chance to identify what is causing it
                        ident_cowardice();
                    }
                }
			}

		}

		/* Monster missed player */
		else
		{
			/* Analyze failed attacks */
			switch (method)
			{
				case RBM_HIT:
				case RBM_TOUCH:
				case RBM_CLAW:
				case RBM_BITE:
				case RBM_PECK:
				case RBM_STING:
				case RBM_WHIP:
				case RBM_CRUSH:

				/* Visible monsters */
				if (m_ptr->ml && !p_ptr->confused)
				{
					/* Disturbing */
					disturb(1, 0);

					// deal with earthquakes if they miss you by 1 or 2 or 3 points
					if ((effect == RBE_SHATTER) && (hit_result > -3))
					{
						/* Message */
						msg_format("%^s just misses you.", m_name);
						
						/* Gender based message */
						// No female earthquake causers
						if (r_ptr->flags1 & (RF1_FEMALE))
						{
							msg_print("Her blow slams into the floor where you stood, and the ground shakes violently!");
						}
						
						// Morgoth
						else if (r_ptr->flags1 & (RF1_MALE))
						{
							msg_print("You leap aside as his great hammer slams into the floor.");
							msg_print("The ground shakes violently with the force of the blow!");

							/* Radius 5 earthquake centered on the monster */
							earthquake(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px, 5, cave_m_idx[m_ptr->fy][m_ptr->fx]);
						}
						
						// Kemenrauko
						else
						{
							msg_print("You leap aside as its stony fist slams into the floor.");
							msg_print("The ground shakes violently with the force of the blow!");

							/* Radius 4 earthquake centered on the monster */
							earthquake(m_ptr->fy, m_ptr->fx, -1, -1, 4, cave_m_idx[m_ptr->fy][m_ptr->fx]);
						}
						

					}
					
					// a normal miss
					else
					{
						/* Message */
						msg_format("%^s misses you.", m_name);

						// allow for ripostes
						if (p_ptr->active_ability[S_EVN][EVN_RIPOSTE] &&
							(p_ptr->ripostes < 1) &&
							!p_ptr->afraid &&
							!p_ptr->confused &&
							!p_ptr->entranced &&
							(p_ptr->stun <= 100) &&
							m_ptr->ml &&
							(hit_result <= -10 - (((&inventory[INVEN_WIELD])->weight + 9) / 10)))
						{
							msg_print("You riposte!");
							p_ptr->ripostes++;
							py_attack_aux(m_ptr->fy, m_ptr->fx, ATT_RIPOSTE);
						}
					}
					
					
				}

				break;
			}
			
		}
		
		/* Analyze "visible" monsters only */
		if (visible)
		{
			/* Count "obvious" attacks (and ones that cause damage) */
			if (obvious || dam || (l_ptr->blows[b] > 10))
			{
				/* Count attacks of this type */
				if (l_ptr->blows[b] < MAX_UCHAR)
				{
					l_ptr->blows[b]++;
				}
			}
		}

		/*hack - stop attacks if monster and player are no longer next to each other*/
		//if (do_break) break; // Sil-y: not needed as this is no longer a loop

	}


	/* Blink away */
	if ((blinked) && (alive))
	{
		msg_print("There is a puff of smoke!");
		teleport_away(m_idx, MAX_SIGHT * 2 + 5);
	}

	p_ptr->window |= (PW_COMBAT_ROLLS);

	/* Assume we attacked */
	return (TRUE);
}







/*********************************************************************/
/*                                                                   */
/*                      Monster Ranged Attacks                       */
/*                                                                   */
/*********************************************************************/


/*
 * Gets the number of sides used in the monster attack
 */
int get_sides(int attack)
{
	int sides;

	if (attack >= 128) return (FALSE);
	else if (attack >=  96)
	{
		sides = spell_info_RF4[attack-96][COL_SPELL_SIDES];
	}
	else return (FALSE);

	return (sides);
}



/*
 * Cast a bolt at the player
 * Stop if we hit a monster
 * Affect monsters and the player
 */
static void mon_bolt(int m_idx, int typ, int dd, int ds, int dif)
{
	monster_type *m_ptr = &mon_list[m_idx];
	int py = p_ptr->py;
	int px = p_ptr->px;
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;

	u32b flg = PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY;

	/* Target the player with a bolt attack */
	(void)project(m_idx, 0, fy, fx, py, px, dd, ds, dif, typ, flg, 0, FALSE);
}

/*
 * Cast a beam at the player, sometimes with limited range.
 * Do not stop if we hit a monster
 * Affect grids, monsters, and the player
 */
 /*
static void mon_beam(int m_idx, int typ, int dd, int ds, int dif, int range)
{
	monster_type *m_ptr = &mon_list[m_idx];
	int py = p_ptr->py;
	int px = p_ptr->px;
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;

	u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL |
				PROJECT_PLAY;

	// Target the player with a beam attack
	(void)project(m_idx, range, fy, fx, py, px, dd, ds, dif, typ, flg, 0, TRUE);
}
*/


/*
 * Release a cloud, which is a ball centered on the monster that does not
 * affect other monsters (mostly to avoid annoying messages).
 *
 */
void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad)
{
	monster_type *m_ptr = &mon_list[m_idx];
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;
	
	bool notice;

	//u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_PLAY | PROJECT_HIDE;
	u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_PLAY | PROJECT_KILL | PROJECT_HIDE;

	/* Surround the monster with a cloud */
	notice = project(m_idx, rad, fy, fx, fy, fx, dd + 2, ds, dif, typ, flg, 0, 0);
	
}


/*
 * Breathe or cast an arc-shaped spell at the player.
 * Use an arc spell of specified range and width.
 * Optionally, do not harm monsters with the same r_idx.
 * Affect grids, objects, monsters, and (specifically) the player
 *
 * Monster breaths do not lose strength with distance at the same rate
 * that normal arc spells do.  If the monster is "powerful", they lose
 * less strength; otherwise, they lose more.
 */
static void mon_arc(int m_idx, int typ, bool noharm, int dd, int ds, int dif, int rad, int degrees_of_arc)
{
	monster_type *m_ptr = &mon_list[m_idx];

	int py = p_ptr->py;
	int px = p_ptr->px;
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;

	u32b flg = PROJECT_ARC | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM |
	           PROJECT_KILL | PROJECT_PLAY;

	/*unused variable*/
	(void)noharm;

	/* Radius of zero means no fixed limit. */
	if (rad == 0) rad = MAX_SIGHT;

	/* Target the player with an arc-shaped attack. */
	(void)project(m_idx, rad, fy, fx, py, px, dd + 2, ds, dif, typ, flg, degrees_of_arc, FALSE);

}


// a monster calls for help

extern void shriek(monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	char m_name[80];

	/* Get the monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

	if (m_ptr->ml)
	{
		if (singing(SNG_SILENCE))
		{
			if (r_ptr->flags2 & (RF2_SMART))
				msg_format("%^s lets out a muffled shout for help.", m_name);
			else
				msg_format("%^s lets out a muffled shriek.", m_name);
		}
		else
		{
			if (r_ptr->flags2 & (RF2_SMART))
				msg_format("%^s shouts for help.", m_name);
			else
				msg_format("%^s makes a high pitched shriek.", m_name);			
		}
	}
	else
	{
		if (singing(SNG_SILENCE))
		{
			if (r_ptr->flags2 & (RF2_SMART))
				msg_print("You hear a muffled shout for help.");
			else
				msg_print("You hear a muffled shriek.");
		}
		else
		{
			if (r_ptr->flags2 & (RF2_SMART))
				msg_print("You hear a shout for help.");
			else
				msg_print("You hear a shriek.");
		}
	}

	// disturb the player
	disturb(0, 0);
	
	/* Make a lot of noise */
	update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
	monster_perception(FALSE, FALSE, -10);
	
	// makes monster noise too
	m_ptr->noise += 10;
}

/*
 * Monster attempts to make a ranged (non-melee) attack.
 *
 * Determine if monster can attack at range, then see if it will.  Use
 * the helper function "choose_attack_spell()" to pick a physical ranged
 * attack, magic spell, or summon.  Execute the attack chosen.  Process
 * its effects, and update character knowledge of the monster.
 *
 * Perhaps monsters should breathe at locations *near* the player,
 * since this would allow them to inflict "partial" damage.
 */
bool make_attack_ranged(monster_type *m_ptr, int attack)
{
	int spower, manacost;

	int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];

	char m_name[80];
	char m_poss[80];

	char ddesc[80];

	/* Summon level */
	int summon_lev;

	/* Is the player blind? */
	bool blind = (p_ptr->blind ? TRUE : FALSE);

	/* Can the player see the monster casting the spell? */
	bool seen = (!blind && m_ptr->ml);

	/* Determine mana cost */
	if (attack >= 128) return (FALSE);
	else if (attack >=  96) manacost = spell_info_RF4[attack- 96][COL_SPELL_MANA_COST];
	else return (FALSE);

	/* Spend mana (for non-songs) */
    if (attack < 96 + RF4_SNG_HEAD)   m_ptr->mana -= manacost;  // Sil-x: this is a hack to only have you pay mana for things other than songs

	/*** Get some info. ***/

	/* Extract the monster's spell power.  Must be at least 1. */
	spower = MAX(1, r_ptr->spell_power);

	/* Get the monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

	/* Get the monster possessive ("his"/"her"/"its") */
	monster_desc(m_poss, sizeof(m_name), m_ptr, 0x22);

	/* Hack -- Get the "died from" name */
	monster_desc(ddesc, sizeof(m_name), m_ptr, 0x88);

	/* Get the summon level */
	summon_lev = r_ptr->level - 1;

	// Sil-y: no chance of spell failure anymore
	
	/*Monster has cast a spell*/
	m_ptr->mflag &= ~(MFLAG_ALWAYS_CAST);
	
	/*** Execute the ranged attack chosen. ***/
	switch (attack)
	{
		/* RF4_ARROW1, RF4_ARROW2 */
		case 96+0:
		case 96+1:
		{
            int dd = (attack == 96+0) ? 1 : 2;
            
			disturb(1, 0);
			if (spower < 2)
			{
				if (blind) msg_print("You hear a twang.");
				else msg_format("%^s fires an arrow.", m_name);
			}
			else
			{
				if (blind) msg_print("You hear a loud thwang.");
				else msg_format("%^s fires an arrow.", m_name);
			}
			
			mon_bolt(m_idx, GF_ARROW, dd, get_sides(attack), -1);
			
			break;
		}

		/* RF4_BOULDER */
		case 96+2:
		{
			disturb(1, 0);
			if (blind) msg_print("You hear something grunt with exertion.");
			else if (spower < 8) msg_format("%^s hurls a rock at you.", m_name);
			else msg_format("%^s hurls a boulder at you.", m_name);
			
			mon_bolt(m_idx, GF_BOULDER, 6, get_sides(attack), -1);
			
			break;
		}
			
		/* RF4_BRTH_FIRE */
		case 96+3:
		{
			disturb(1, 0);
			if (blind) msg_format("%^s breathes.", m_name);
			else msg_format("%^s breathes fire.", m_name);
			mon_arc(m_idx, GF_FIRE, TRUE, r_ptr->spell_power, get_sides(attack), -1,
			        r_ptr->spell_power/2, 60);
			
			/* Make a lot of noise */
			update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
			monster_perception(FALSE, FALSE, -10);
			
			break;
		}
			
		/* RF4_BRTH_COLD */
		case 96+4:
		{
			disturb(1, 0);
			if (blind) msg_format("%^s breathes.", m_name);
			else msg_format("%^s breathes frost.", m_name);
			mon_arc(m_idx, GF_COLD, TRUE, r_ptr->spell_power, get_sides(attack), -1,
					r_ptr->spell_power/2, 60);
			
			/* Make a lot of noise */
			update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
			monster_perception(FALSE, FALSE, -10);
			
			break;
		}
			
		/* RF4_BRTH_POIS */
		case 96+5:
		{
			disturb(1, 0);
			if (blind) msg_format("%^s breathes.", m_name);
			else msg_format("%^s breathes poisonous gas.", m_name);
			mon_arc(m_idx, GF_POIS, TRUE, r_ptr->spell_power, get_sides(attack), -1,
			        r_ptr->spell_power/2, 90);
			
			/* Make a lot of noise */
			update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
			monster_perception(FALSE, FALSE, -10);
			
			break;
		}
			
		/* RF4_BRTH_DARK */
		case 96+6:
		{
			disturb(1, 0);
			if (blind) msg_format("%^s breathes.", m_name);
			msg_format("%^s breathes darkness.", m_name);
			mon_arc(m_idx, GF_DARK, TRUE, r_ptr->spell_power, get_sides(attack), -1,
					r_ptr->spell_power/2, 60);

			/* Make a lot of noise */
			update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
			monster_perception(FALSE, FALSE, -10);
						
			break;
		}
			
		/* RF4_EARTHQUAKE */
		case 96+7:
		{
			int pit_y, pit_x, dy, dx;
			
			dy = (m_ptr->fy > p_ptr->py) ? -1 : ((m_ptr->fy < p_ptr->py) ? 1 : 0);
			dx = (m_ptr->fx > p_ptr->px) ? -1 : ((m_ptr->fx < p_ptr->px) ? 1 : 0);
			pit_y = m_ptr->fy + dy;
			pit_x = m_ptr->fx + dx;

			msg_format("%^s slams his hammer into the ground.", m_name);
			
			earthquake(m_ptr->fy, m_ptr->fx, pit_y, pit_x, 5, cave_m_idx[m_ptr->fy][m_ptr->fx]);
			break;
		}
			
		/* RF4_SHRIEK */
		case 96+8:
		{
			disturb(0, 0);
			
			shriek(m_ptr);
			break;
		}

		/* RF4_SCREECH */
		case 96+9:
		{
			disturb(1, 0);
			if (p_ptr->stun || !seen)
			{
				if (singing(SNG_SILENCE))
				{
					msg_print("The air is filled with a muffled screeching.");
				}
				else
				{
					msg_print("The air is filled with an unearthly screeching.");
				}
			}
			else
			{
				if (singing(SNG_SILENCE))
				{
					msg_format("%^s fixes its malevolent gaze upon you and lets out a muffled screech.", m_name);
				}
				else
				{
					msg_format("%^s fixes its malevolent gaze upon you and lets out a terrible screech.", m_name);
				}
			}
			
			if (allow_player_stun(m_ptr))
			{
				if (p_ptr->stun < 100)
				{
					msg_print("Your mind reels.");
					
					set_stun(p_ptr->stun + 20);
				}
			}
			
			if (allow_player_fear(m_ptr))
			{
				(void)set_afraid(p_ptr->afraid + damroll(2,4));
			}
			
			/* Make a lot of noise */
			update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
			monster_perception(FALSE, FALSE, -20);
			
			break;
		}			

		/* RF4_DARKNESS */
		case 96+10:
		{
			disturb(0, 0);
			
			if (blind) msg_format("%^s mutters.", m_name);
			else msg_format("%^s gestures in shadow.", m_name);
			
			(void)darken_area(0, 0, 3);
			break;
		}

		/* RF4_FORGET */
		case 96+11:
		{
			disturb(0, 0);
			
			// special message for Mewlips
			if ((r_ptr->d_char == 'H') && (r_ptr->d_attr == TERM_UMBER))
			{
				if (blind) msg_format("%^s rings its bell.", m_name);
				else msg_format("%^s rings its bell.", m_name);
			}
			else
			{
				msg_format("%^s tries to blank your mind.", m_name);
			}
			
			if (saving_throw(m_ptr, 0))
			{
				msg_print("You resist!");
			}
			else
			{
				msg_print("Your memories fade away.");
				wiz_dark();
			}
			break;
		}

		/* RF4_SCARE */
		case 96+12:
		{
			disturb(1, 0);
			if (!m_ptr->ml || one_in_(2))
			{
				msg_format("%^s lets out a terrible cry.", m_name);
				
				/* Make a lot of noise */
				update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
				monster_perception(FALSE, FALSE, -10);
			}
			else
			{ 
				msg_format("%^s looks into your eyes.", m_name);
			}
			if (!allow_player_fear(m_ptr) && !(p_ptr->afraid))
			{
				msg_print("You are unafraid.");
			}
			else
			{
				(void)set_afraid(p_ptr->afraid + damroll(3,4));
			}
			break;
		}

		/* RF4_CONF */
		case 96+13:
		{
			disturb(1, 0);
			if (blind) msg_format("%^s mutters.", m_name);
			else msg_format("%^s glares at you.", m_name);
			if (allow_player_confusion(m_ptr))
			{
				(void)set_confused(p_ptr->confused + damroll(2,4));
			}
			break;
		}

		/* RF4_HOLD */
		case 96+14:
		{
			disturb(1, 0);
			if (blind) msg_format("%^s mutters.", m_name);
				else msg_format("%^s stares deep into your eyes.", m_name);
				
			if (!allow_player_entrancement(m_ptr))
			{
				if (!p_ptr->entranced) msg_print("You stare back unafraid!");
			}
			// Must not already be entranced or entranced last round, as chaining entrancement is too nasty
			else if (!p_ptr->entranced && !p_ptr->was_entranced)
			{
				(void)set_entranced(damroll(4, 4));
			}
			break;
		}

        /* RF4_SLOW */
		case 96+15:
		{
			disturb(1, 0);
			msg_format("%^s whispers of fading and decay.", m_name);
            
			if (!allow_player_slow(m_ptr))
			{
				msg_print("You resist.");
			}
			else
			{
				(void)set_slow(p_ptr->slow + damroll(2, 4));
			}
			break;
		}
            
        // Sil-x: only songs after this point as 96+RF4_SNG_HEAD is used in the spell code to distinguish songs from non-songs
            
        /* RF4_SNG_BINDING */
		case 96+16:
		{
            song_of_binding(m_ptr);

			break;
		}
            
        /* RF4_SNG_PIERCING */
		case 96+17:
		{
            song_of_piercing(m_ptr);
            
			break;
		}

        /* RF4_SNG_OATHS */
		case 96+18:
		{
            song_of_oaths(m_ptr);
            
			break;
		}

		/* Paranoia */
		default:
		{
			msg_print("A monster tried to cast a spell that has not yet been defined.");
		}
	}

	/* Mark minimum desired range for recalculation */
	m_ptr->min_range = 0;

	/* Remember what the monster did to us */
	if (seen)
	{
		/* Innate spell */
		if (attack < 32*4)
		{
			l_ptr->flags4 |= (1L << (attack - 32*3));
			if (l_ptr->ranged < MAX_UCHAR) l_ptr->ranged++;
		}
	}

	//if (seen && p_ptr->wizard)
	//	msg_format("%^s has %i mana remaining.", m_name, m_ptr->mana);

	/* Always take note of monsters that kill you */
	if (p_ptr->is_dead && (l_ptr->deaths < MAX_SHORT))
	{
		l_ptr->deaths++;
	}

	/* A spell was cast */
 	return (TRUE);
}


/*
 * Some monsters are surrounded by poison gas, terrible heat, cold, darkness etc
 * Process any such affects.
 */
void cloud_surround(int r_idx, int *typ, int *dd, int *ds, int *rad)
{
	monster_race *r_ptr = &r_info[r_idx];

	*typ = 0;
	*dd = r_ptr->spell_power / 4;
	*ds = 4;
	*rad = 1;

	/*** Determine the kind of cloud we're supposed to be giving off ***/

	/* If breaths and attrs match, the choice is clear. */
	if (r_ptr->flags4)
	{
		/* This is mostly for the serpents */
		if ((r_ptr->flags4 & (RF4_BRTH_FIRE)) &&
		    (r_ptr->flags4 & (RF4_BRTH_POIS)) &&
			(r_ptr->flags4 & (RF4_BRTH_COLD)) &&
			(r_ptr->flags4 & (RF4_BRTH_DARK)))
		{
			int rand_num = dieroll(4);
			
			switch (rand_num)
			{
				case 1:
					*typ = GF_COLD;
					break;
				case 2:
					*typ = GF_FIRE;
					break;
				case 3:
					*typ = GF_POIS;
					break;
				case 4:
					*typ = GF_DARK;
					break;
			}
		}
		else if (r_ptr->flags4 & (RF4_BRTH_POIS)) *typ = GF_POIS;
		else if (r_ptr->flags4 & (RF4_BRTH_FIRE)) *typ = GF_FIRE;
		else if (r_ptr->flags4 & (RF4_BRTH_COLD)) *typ = GF_COLD;
		else if (r_ptr->flags4 & (RF4_BRTH_DARK)) *typ = GF_DARK;
	}

}

void new_combat_round(void)
{
	int i;
	
	if (combat_number != 0) combat_number_old = combat_number;
	combat_number = 0;
	turns_since_combat++;

	if (turns_since_combat == 1)
	{
		// copy previous round's rolls into old round's rolls
		for (i = 0; i < MAX_COMBAT_ROLLS; i++)
		{
			COPY(&combat_rolls[1][i], &combat_rolls[0][i], combat_roll);
		}
	}
	else if (turns_since_combat == 11)
	{
		// reset old round's rolls
		combat_number_old = 0;
		for (i = 0; i < MAX_COMBAT_ROLLS; i++)
		{
			combat_rolls[1][i].att_type = COMBAT_ROLL_NONE;
		}
	}
	
	// reset new round's rolls
	for (i = 0; i < MAX_COMBAT_ROLLS; i++)
	{
		combat_rolls[0][i].att_type = COMBAT_ROLL_NONE;
	}
	
	
}


/*
 * Update combat roll table part 1 (the attack rolls)
 */
void update_combat_rolls1(const monster_type *m_ptr1, const monster_type *m_ptr2, bool vis, int att, int att_roll, int evn, int evn_roll)
{
	monster_race *r_ptr1;
	monster_race *r_ptr2;
	
	if (m_ptr1 == PLAYER)
	{
		r_ptr1 = &r_info[0];
	}
	else if (m_ptr1 == NULL)
	{
		// hack for traps hitting you
		r_ptr1 = NULL;
	}
	else if (p_ptr->image)
	{
		r_ptr1 = &r_info[m_ptr1->image_r_idx];
	}
	else
	{
		r_ptr1 = &r_info[m_ptr1->r_idx];
	}

	if (m_ptr2 == PLAYER)
	{
		r_ptr2 = &r_info[0];
	}
	else if (m_ptr2 == NULL)
	{
		// hack for attacking Morgoth's crown
		r_ptr2 = NULL;
	}
	else if (p_ptr->image)
	{
		r_ptr2 = &r_info[m_ptr2->image_r_idx];
	}
	else
	{
		r_ptr2 = &r_info[m_ptr2->r_idx];
	}
		
	if (combat_number < MAX_COMBAT_ROLLS)
	{
		combat_rolls[0][combat_number].att_type = COMBAT_ROLL_ROLL;
		
		if (m_ptr1 == NULL)
		{
			combat_rolls[0][combat_number].attacker_char = combat_roll_special_char;
			combat_rolls[0][combat_number].attacker_attr = combat_roll_special_attr;
		}
		else if (vis || (m_ptr1 == PLAYER))
		{
			combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;

			if (p_ptr->rage && (m_ptr1 != PLAYER))
			{
				combat_rolls[0][combat_number].attacker_attr = TERM_RED;
			}
			else
			{
				combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
			}
		}
		else
		{
			combat_rolls[0][combat_number].attacker_char = '?';
			combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
		}

		// hack for Iron Crown
		if (m_ptr2 == NULL)
		{
			combat_rolls[0][combat_number].defender_char = ']';
			combat_rolls[0][combat_number].defender_attr = TERM_L_DARK;
		}
		else if (vis || (m_ptr2 == PLAYER))
		{
			combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
			
			if (p_ptr->rage && (m_ptr2 != PLAYER))
			{
				combat_rolls[0][combat_number].defender_attr = TERM_RED;
			}
			else
			{
				combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
			}
		}
		else
		{
			combat_rolls[0][combat_number].defender_char = '?';
			combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
		}

		combat_rolls[0][combat_number].att = att;
		combat_rolls[0][combat_number].att_roll = att_roll;
		combat_rolls[0][combat_number].evn = evn;
		combat_rolls[0][combat_number].evn_roll = evn_roll;
		
		combat_number++;
		turns_since_combat = 0;
	}
	
	/* Window stuff */
	p_ptr->window |= (PW_COMBAT_ROLLS);
}

/*
 * Update combat roll table part 1b (the attack when there is no roll made -- eg breath attack)
 */
void update_combat_rolls1b(const monster_type *m_ptr1, const monster_type *m_ptr2, bool vis)
{
	monster_race *r_ptr1;
	monster_race *r_ptr2;

	if (m_ptr1 == PLAYER)
	{
		r_ptr1 = &r_info[0];
	}
	else if (m_ptr1 == NULL)
	{
		// hack for traps hitting you
		r_ptr1 = NULL;
	}	
	else if (p_ptr->image)
	{
		r_ptr1 = &r_info[m_ptr1->image_r_idx];
	}
	else
	{
		r_ptr1 = &r_info[m_ptr1->r_idx];
	}

	if (m_ptr2 == PLAYER)
	{
		r_ptr2 = &r_info[0];
	}
	else if (p_ptr->image)
	{
		r_ptr2 = &r_info[m_ptr2->image_r_idx];
	}
	else
	{
		r_ptr2 = &r_info[m_ptr2->r_idx];
	}
	
	if (combat_number < MAX_COMBAT_ROLLS)
	{
		combat_rolls[0][combat_number].att_type = COMBAT_ROLL_AUTO;
		
		if (m_ptr1 == NULL)
		{
			combat_rolls[0][combat_number].attacker_char = combat_roll_special_char;
			combat_rolls[0][combat_number].attacker_attr = combat_roll_special_attr;
		}
		else if (vis || (m_ptr1 == PLAYER))
		{
			combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
			
			if (p_ptr->rage && (m_ptr1 != PLAYER))
			{
				combat_rolls[0][combat_number].attacker_attr = TERM_RED;
			}
			else
			{
				combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
			}
		}
		else
		{
			combat_rolls[0][combat_number].attacker_char = '?';
			combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
		}
		
		if (vis || (m_ptr2 == PLAYER))
		{
			combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
			
			if (p_ptr->rage && (m_ptr2 != PLAYER))
			{
				combat_rolls[0][combat_number].defender_attr = TERM_RED;
			}
			else
			{
				combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
			}
		}
		else
		{
			combat_rolls[0][combat_number].defender_char = '?';
			combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
		}
		
		combat_number++;
		turns_since_combat = 0;
	}
	
	/* Window stuff */
	p_ptr->window |= (PW_COMBAT_ROLLS);
}


/*
 * Update combat roll table part 2 (the damage rolls)
 */
void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps, int prot, int prt_percent, int dam_type, bool melee)
{
	if (combat_number-1 < MAX_COMBAT_ROLLS)
	{
		combat_rolls[0][combat_number-1].dam_type = dam_type;
		combat_rolls[0][combat_number-1].dd = dd;
		combat_rolls[0][combat_number-1].ds = ds;
		combat_rolls[0][combat_number-1].dam = dam;
		combat_rolls[0][combat_number-1].pd = pd;
		combat_rolls[0][combat_number-1].ps = ps;
		combat_rolls[0][combat_number-1].prot = prot;
		combat_rolls[0][combat_number-1].prt_percent = prt_percent;
		combat_rolls[0][combat_number-1].melee = melee;
		
		// deal with protection for the player
		// this hackishly uses the pd and ps to store the min and max prot for the player
		if (pd == -1)
		{
			// use the protection values for pure elemental types if there was no attack roll
			if (combat_rolls[0][combat_number-1].att_type == COMBAT_ROLL_AUTO)
			{
				combat_rolls[0][combat_number-1].pd = p_min(dam_type, melee);
				combat_rolls[0][combat_number-1].ps = p_max(dam_type, melee);
			}
			// otherwise use the normal protection values 
			else
			{
				combat_rolls[0][combat_number-1].pd = p_min(GF_HURT, melee);
				combat_rolls[0][combat_number-1].ps = p_max(GF_HURT, melee);
			}
		}
	}
}


/*
 * Display combat rolls in a window
 */
void display_combat_rolls(void)
{
	int i;
	int line = 0;
	char buf[80];

	int net_att = 0;   // a default value (required)
	int net_dam;

	int a_att;
	int a_evn;
	int a_hit;
	int a_dam_roll;
	int a_prot_roll;
	int a_net_dam;
	
	int round;
	int combat_num_for_round = combat_number;
	
	int total_player_attacks = 0;
	int player_attacks = 0;
	int monster_attacks = 0;
	
	int line_jump = 0;
	
	int res = 1;   // a default value to soothe compilation warnings
		
	/* Clear the window */
	for (i = 0; i < Term->hgt; i++)
	{
		/* Erase the line */
		Term_erase(0, i, 255);
	}	
	
	for (round = 0; round < 2; round++)
	{
		// initialise some things
		if (round == 1)
		{
			combat_num_for_round = combat_number_old;
			line_jump = player_attacks + monster_attacks + 2;
			if (player_attacks > 0) line_jump++;
			if (monster_attacks > 0) line_jump++;
			if (combat_number + combat_number_old > 0)
			{
				Term_putstr(0, line_jump-1, 80, TERM_L_DARK, "_______________________________________________________________________________");
			}
		}
		total_player_attacks = 0;
		player_attacks = 0;
		monster_attacks = 0;		

		for (i = 0; i < combat_num_for_round; i++)
		{
			if ((combat_rolls[round][i].attacker_char == r_info[0].d_char) &&
				(combat_rolls[round][i].attacker_attr == r_info[0].d_attr))
			{
				total_player_attacks++;
			}
		}
		
		for (i = 0; i < combat_num_for_round; i++)
		{
			// default values:
			a_net_dam = TERM_L_RED;
			res = 1;
			
			// determine the appropriate resistance if the player was attacked
			if ((combat_rolls[round][i].defender_char == r_info[0].d_char) &&
				(combat_rolls[round][i].defender_attr == r_info[0].d_attr))
			{
				switch (combat_rolls[round][i].dam_type)
				{
					case GF_FIRE:
						res = resist_fire();
						break;
					case GF_COLD:
						res = resist_cold();
						break;
					case GF_POIS:
						res = resist_pois();
						a_net_dam = TERM_GREEN;
						break;
					case GF_DARK:
						res = resist_dark();
						break;
				}
			}
			
			if ((combat_rolls[round][i].attacker_char == r_info[0].d_char) &&
				(combat_rolls[round][i].attacker_attr == r_info[0].d_attr))
			{
				player_attacks++;
				
				a_att = TERM_L_BLUE;
				a_evn = TERM_WHITE;
				a_hit = TERM_L_RED;
				a_dam_roll = TERM_L_BLUE;
				if (combat_rolls[round][i].prt_percent >= 100)
					a_prot_roll = TERM_WHITE;
				else if (combat_rolls[round][i].prt_percent >= 1)
					a_prot_roll = TERM_SLATE;
				else
					a_prot_roll = TERM_DARK;
				
				line = player_attacks + line_jump;
			}
			else
			{
				monster_attacks++;
				
				a_att = TERM_WHITE;
				a_evn = TERM_L_BLUE;
				a_hit = TERM_L_RED;
				a_dam_roll = TERM_WHITE;
				if (combat_rolls[round][i].prt_percent >= 100)
					a_prot_roll = TERM_L_BLUE;
				else if (combat_rolls[round][i].prt_percent >= 1)
					a_prot_roll = TERM_BLUE;
				else
					a_prot_roll = TERM_DARK;
				
				line = 1 + total_player_attacks + monster_attacks + line_jump;
				if (total_player_attacks == 0) line--;
			}
			
			
			
			/* Display the entry itself */
			Term_putstr(0, line, 1, TERM_WHITE, " ");
			Term_addch(combat_rolls[round][i].attacker_attr, combat_rolls[round][i].attacker_char);
			
			
			// First display the attack side of the roll
			
			// don't print attack info if there isn't any (i.e. if it is a breath or other elemental attack)
			if (combat_rolls[round][i].att_type == COMBAT_ROLL_ROLL)
			{
				if (combat_rolls[round][i].att < 10)
				{
					strnfmt(buf, sizeof (buf), "  (%+d)", combat_rolls[round][i].att);
				}
				else
				{
					strnfmt(buf, sizeof (buf), " (%+d)", combat_rolls[round][i].att);
				}
				Term_addstr(-1, a_att, buf);
				
				strnfmt(buf, sizeof (buf), "%4d", combat_rolls[round][i].att + combat_rolls[round][i].att_roll);
				Term_addstr(-1, a_att, buf);
				
				net_att =   combat_rolls[round][i].att_roll + combat_rolls[round][i].att
				- combat_rolls[round][i].evn_roll - combat_rolls[round][i].evn;
				if (net_att > 0)
				{
					strnfmt(buf, sizeof (buf), "%4d", net_att);
					Term_addstr(-1, a_hit, buf);
				}
				else
				{
					Term_addstr(-1, TERM_SLATE, "   -");
				}
				
				strnfmt(buf, sizeof (buf), "%4d", combat_rolls[round][i].evn + combat_rolls[round][i].evn_roll);
				Term_addstr(-1, a_evn, buf);
				
				if (combat_rolls[round][i].evn < 10)
				{
					strnfmt(buf, sizeof (buf), "   [%+d]", combat_rolls[round][i].evn);
				}
				else
				{
					strnfmt(buf, sizeof (buf), "  [%+d]", combat_rolls[round][i].evn);
				}
				Term_addstr(-1, a_evn, buf);
				
				// add the defender char
				Term_addch(TERM_WHITE, ' ');
				Term_addch(combat_rolls[round][i].defender_attr, combat_rolls[round][i].defender_char);
			}
			else if (combat_rolls[round][i].att_type == COMBAT_ROLL_AUTO)
			{
				Term_addstr(-1, TERM_L_DARK, "                         ");
				
				// add the defender char
				Term_addch(TERM_WHITE, ' ');
				Term_addch(combat_rolls[round][i].defender_attr, combat_rolls[round][i].defender_char);
			}
			
			// Now display the damage side of the roll
			
			if ((net_att > 0) || (combat_rolls[round][i].att_type == COMBAT_ROLL_AUTO))
			{
				Term_addstr(-1, TERM_L_DARK, "  ->");
				
				if (combat_rolls[round][i].ds < 10)
				{
					strnfmt(buf, sizeof (buf), "   (%dd%d)", combat_rolls[round][i].dd, combat_rolls[round][i].ds);
				}
				else
				{
					strnfmt(buf, sizeof (buf), "  (%dd%d)", combat_rolls[round][i].dd, combat_rolls[round][i].ds);
				}
				Term_addstr(-1, a_dam_roll, buf);
				
				strnfmt(buf, sizeof (buf), "%4d", combat_rolls[round][i].dam);
				Term_addstr(-1, a_dam_roll, buf);
				
				if (combat_rolls[round][i].att_type == COMBAT_ROLL_ROLL)
				{
					net_dam =   combat_rolls[round][i].dam - combat_rolls[round][i].prot;
					
					if (net_dam > 0)
					{
						strnfmt(buf, sizeof (buf), "%4d", net_dam);
						Term_addstr(-1, a_net_dam, buf);
					}
					else
					{
						Term_addstr(-1, TERM_SLATE, "   -");
					}
					
					strnfmt(buf, sizeof (buf), "%4d", combat_rolls[round][i].prot);
					Term_addstr(-1, a_prot_roll, buf);
					
					// if monster is being hit, show protection dice
					if ((combat_rolls[round][i].defender_char != r_info[0].d_char) || (combat_rolls[round][i].defender_attr != r_info[0].d_attr))
					{
						if ((combat_rolls[round][i].ps < 1) || (combat_rolls[round][i].pd < 1))
						{
							my_strcpy(buf, "        ", sizeof (buf));
							Term_addstr(-1, a_prot_roll, buf);
						}
						else if (combat_rolls[round][i].ps < 10)
						{
							strnfmt(buf, sizeof (buf), "   [%dd%d]", combat_rolls[round][i].pd, combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						else
						{
							strnfmt(buf, sizeof (buf), "  [dd%d]", combat_rolls[round][i].pd, combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						if ((combat_rolls[round][i].prt_percent > 0) && (combat_rolls[round][i].prt_percent < 100))
						{
							strnfmt(buf, sizeof (buf), " (%d%%)", combat_rolls[round][i].prt_percent);
							Term_addstr(-1, a_prot_roll, buf);
						}
					}
					
					// if player is being hit, show protection *range*
					else
					{
						strnfmt(buf, sizeof (buf), "  [%d-%d]", (combat_rolls[round][i].pd * combat_rolls[round][i].prt_percent) / 100,
								(combat_rolls[round][i].ps * combat_rolls[round][i].prt_percent) / 100);
						Term_addstr(-1, a_prot_roll, buf);
					}
					
				}
				
				// display attacks that don't use hit rolls
				else if (combat_rolls[round][i].att_type == COMBAT_ROLL_AUTO)
				{
					// shield etc protection and resistance
					if (combat_rolls[round][i].melee)	net_dam = combat_rolls[round][i].dam - combat_rolls[round][i].prot;
					else if (res > 0)					net_dam = (combat_rolls[round][i].dam / res) - combat_rolls[round][i].prot;
					else								net_dam = (combat_rolls[round][i].dam * (-res)) - combat_rolls[round][i].prot;
					
					if (net_dam > 0)
					{
						strnfmt(buf, sizeof (buf), "%4d", net_dam);
						Term_addstr(-1, a_net_dam, buf);
					}
					else
					{
						Term_addstr(-1, TERM_SLATE, "   -");
					}
					
					strnfmt(buf, sizeof (buf), "%4d", combat_rolls[round][i].prot);
					Term_addstr(-1, a_prot_roll, buf);
					
					// if monster is being hit, show protection dice
					if ((combat_rolls[round][i].defender_char != r_info[0].d_char) || (combat_rolls[round][i].defender_attr != r_info[0].d_attr))
					{
						if ((combat_rolls[round][i].ps < 1) || (combat_rolls[round][i].pd < 1))
						{
							my_strcpy(buf, "        ", sizeof (buf));
							Term_addstr(-1, a_prot_roll, buf);
						}
						else if (combat_rolls[round][i].ps < 10)
						{
							strnfmt(buf, sizeof (buf), "   [%dd%d]", combat_rolls[round][i].pd, combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						else
						{
							strnfmt(buf, sizeof (buf), "  [dd%d]", combat_rolls[round][i].pd, combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						if ((combat_rolls[round][i].prt_percent > 0) && (combat_rolls[round][i].prt_percent < 100))
						{
							strnfmt(buf, sizeof (buf), " (%d%%)", combat_rolls[round][i].prt_percent);
							Term_addstr(-1, a_prot_roll, buf);
						}
					}
					
					// if a player is being hit, show protection range etc
					else
					{
						if (!(combat_rolls[round][i].melee))
						{
							if (res > 1)
							{
								strnfmt(buf, sizeof (buf), "  1/%d then", res);
								Term_addstr(-1, TERM_L_BLUE, buf);
							}
							else if (res < 0)
							{
								strnfmt(buf, sizeof (buf), "  x%d then", -res);
								Term_addstr(-1, TERM_L_BLUE, buf);
							}
						}

						if (combat_rolls[round][i].ps < 10)
						{
							strnfmt(buf, sizeof (buf), "  [%d-%d]", combat_rolls[round][i].pd, combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						else
						{
							strnfmt(buf, sizeof (buf), " [%d-%d]", combat_rolls[round][i].pd, combat_rolls[round][i].ps);
							Term_addstr(-1, a_prot_roll, buf);
						}
						
						/*
						 // no protection, only resistance
						 else
						 {
						 net_dam = combat_rolls[round][i].dam / res;
						 strnfmt(buf, sizeof (buf), "%4d", net_dam);
						 Term_addstr(-1, a_net_dam, buf);
						 
						 if (res > 1)
						 {
						 strnfmt(buf, sizeof (buf), "  /%d", res);
						 Term_addstr(-1, TERM_L_BLUE, buf);
						 }
						 }
						 */
					}
				}
			}	
		}
	}
		
	

}

