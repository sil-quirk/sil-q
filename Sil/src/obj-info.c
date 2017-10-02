/* File: obj-info.c */

/*
 * Copyright (c) 2002 Andrew Sidwell, Robert Ruehlmann
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/* TRUE if a paragraph break should be output before next p_text_out() */
static bool new_paragraph = FALSE;


static void p_text_out(cptr str)
{
	if (new_paragraph)
	{
		text_out("\n\n   ");
		new_paragraph = FALSE;
	}

	text_out(str);
}

static void output_list(cptr list[], int n)
{
	int i;

	char *conjunction = "and ";
	if (n < 0)
	{
		n = -n;
		conjunction = "or ";
	}


	for (i = 0; i < n; i++)
	{
		if (i != 0)

		{
			p_text_out((i == 1 && i == n - 1) ? " " : ", ");
			if (i == n - 1)	p_text_out(conjunction);

		}
		p_text_out(list[i]);
	}
}


static void output_desc_list(cptr intro, cptr list[], int n)
{
	if (n != 0)
	{
		/* Output intro */
		p_text_out(intro);

		/* Output list */
		output_list(list, n);

		/* Output end */
		p_text_out(".  ");
	}
}


/*
 * Describe stat modifications.
 */
static bool describe_stats(const object_type *o_ptr, u32b f1)
{
	cptr descs[A_MAX];
	int cnt = 0;
	int pval = (o_ptr->pval > 0 ? o_ptr->pval : -o_ptr->pval);

	/* Abort if the pval is zero */
	//if (!pval) return (FALSE);

	/* Collect stat bonuses */
	if (f1 & (TR1_STR)) descs[cnt++] = stat_names_full[A_STR];
	if (f1 & (TR1_DEX)) descs[cnt++] = stat_names_full[A_DEX];
	if (f1 & (TR1_CON)) descs[cnt++] = stat_names_full[A_CON];
	if (f1 & (TR1_GRA)) descs[cnt++] = stat_names_full[A_GRA];

	/* Skip */
	if (cnt == 0) return (FALSE);

	/* Shorten to "all stats", if appropriate. */
	if (cnt == A_MAX)
	{
		p_text_out(format("It %s all your stats", (o_ptr->pval >= 0 ? "increases" : "decreases")));
	}
	else
	{
		p_text_out(format("It %s your ", (o_ptr->pval >= 0 ? "increases" : "decreases")));

		/* Output list */
		output_list(descs, cnt);
	}

	/* Output end */
	p_text_out(format(" by %i.  ", pval));

	/* We found something */
	return (TRUE);
}

/*
 * Describe reversed stat modifications.
 */
static bool describe_neg_stats(const object_type *o_ptr, u32b f1)
{
	cptr descs[A_MAX];
	int cnt = 0;
	int pval = (o_ptr->pval > 0 ? o_ptr->pval : -o_ptr->pval);

	/* Abort if the pval is zero */
	//if (!pval) return (FALSE);

	/* Collect stat bonuses */
	if (f1 & (TR1_NEG_STR)) descs[cnt++] = stat_names_full[A_STR];
	if (f1 & (TR1_NEG_DEX)) descs[cnt++] = stat_names_full[A_DEX];
	if (f1 & (TR1_NEG_CON)) descs[cnt++] = stat_names_full[A_CON];
	if (f1 & (TR1_NEG_GRA)) descs[cnt++] = stat_names_full[A_GRA];

	/* Skip */
	if (cnt == 0) return (FALSE);

	/* Shorten to "all stats", if appropriate. */
	if (cnt == A_MAX)
	{
		p_text_out(format("It %s all your stats", (o_ptr->pval < 0 ? "increases" : "decreases")));
	}
	else
	{
		p_text_out(format("It %s your ", (o_ptr->pval < 0 ? "increases" : "decreases")));

		/* Output list */
		output_list(descs, cnt);
	}

	/* Output end */
	p_text_out(format(" by %i.  ", pval));

	/* We found something */
	return (TRUE);
}

/*
 * Describe "secondary bonuses" of an item.
 */
static bool describe_secondary(const object_type *o_ptr, u32b f1)
{
	cptr descs[8];
	int cnt = 0;
	int pval = (o_ptr->pval > 0 ? o_ptr->pval : -o_ptr->pval);

	/* Collect */
	if (f1 & (TR1_MEL))     descs[cnt++] = "melee";
	if (f1 & (TR1_ARC))     descs[cnt++] = "archery";
	if (f1 & (TR1_STL))     descs[cnt++] = "stealth";
	if (f1 & (TR1_PER))     descs[cnt++] = "perception";
	if (f1 & (TR1_WIL))     descs[cnt++] = "will";
	if (f1 & (TR1_SMT))     descs[cnt++] = "smithing";
	if (f1 & (TR1_SNG))     descs[cnt++] = "song";
	if (f1 & (TR1_TUNNEL))  descs[cnt++] = "tunneling";
	if (f1 & (TR1_DAMAGE_SIDES))  descs[cnt++] = "damage sides";

	/* Skip */
	if (!cnt) return (FALSE);

	/* Start */
	p_text_out(format("It %s your ", (o_ptr->pval >= 0 ? "improves" : "worsens")));

	/* Output list */
	output_list(descs, cnt);

	/* Output end */
	p_text_out(format(" by %i.  ", pval));

	/* We found something */
	return (TRUE);
}


/*
 * Describe the special slays and executes of an item.
 */
static bool describe_slay(const object_type *o_ptr, u32b f1)
{
	cptr slays[8];
	int slcnt = 0;

	/* Unused parameter */
	(void)o_ptr;

	/* Collect brands */
	if (f1 & (TR1_SLAY_WOLF))   slays[slcnt++] = "wolves";
	if (f1 & (TR1_SLAY_ORC))    slays[slcnt++] = "orcs";
	if (f1 & (TR1_SLAY_TROLL))  slays[slcnt++] = "trolls";
	if (f1 & (TR1_SLAY_SPIDER)) slays[slcnt++] = "spiders";
	if (f1 & (TR1_SLAY_DRAGON))	slays[slcnt++] = "dragons";
	if (f1 & (TR1_SLAY_RAUKO))	slays[slcnt++] = "raukar";
	if (f1 & (TR1_SLAY_UNDEAD))	slays[slcnt++] = "undead";

	/* Describe */
	if (slcnt)
	{
		if (o_ptr->number == 1)
		{
			/* Output intro */
			p_text_out("It slays ");
		}
		else
		{
			/* Output intro */
			p_text_out("They slay ");
		}

		/* Output list */
		output_list(slays, slcnt);

		/* Output end */
		p_text_out(".  ");
	}

	/* We are done here */
	return ((slcnt) ? TRUE : FALSE);
}


/*
 * Describe elemental brands.
 */
static bool describe_brand(const object_type *o_ptr, u32b f1)
{
	cptr descs[5];
	int cnt = 0;

	/* Unused parameter */
	(void)o_ptr;

	/* Collect brands */
	if (f1 & (TR1_BRAND_ELEC)) descs[cnt++] = "lightning";
	if (f1 & (TR1_BRAND_FIRE)) descs[cnt++] = "flame";
	if (f1 & (TR1_BRAND_COLD)) descs[cnt++] = "frost";
	if (f1 & (TR1_BRAND_POIS)) descs[cnt++] = "venom";

	if (o_ptr->number == 1)
	{
		/* Describe brands */
		output_desc_list("It is branded with ", descs, cnt);
	}
	else
	{
		/* Describe brands */
		output_desc_list("They are branded with ", descs, cnt);
	}

	/* We are done here */
	return (cnt ? TRUE : FALSE);
}



/*
 * Describe misc weapon attributes.
 */
static bool describe_misc_weapon_attributes(const object_type *o_ptr, u32b f1)
{
	bool message = FALSE;

	/* Unused parameter */
	(void)o_ptr;

	if (f1 & (TR1_SHARPNESS))
	{
		if (o_ptr->number == 1)	p_text_out("It cuts easily through armour.  ");
		else					p_text_out("They cut easily through armour.  ");
		message = TRUE;
	}
	if (f1 & (TR1_SHARPNESS2))
	{
		p_text_out("It cuts very easily through armour.  ");
		message = TRUE;
	}
	if (f1 & (TR1_VAMPIRIC))
	{
		p_text_out("It drains life from your enemies.  ");
		message = TRUE;
	}
	
	return (message);
}

/*
 * Describe resistances granted by an object.
 */
static bool describe_resist(const object_type *o_ptr, u32b f2)
{
	cptr vp[17];
	int vn = 0;

	/* Unused parameter */
	(void)o_ptr;

	/* Collect resistances */
	if (f2 & (TR2_RES_COLD))	vp[vn++] = "cold";
	if (f2 & (TR2_RES_FIRE))	vp[vn++] = "fire";
	if (f2 & (TR2_RES_ELEC))	vp[vn++] = "lightning";
	if (f2 & (TR2_RES_POIS))	vp[vn++] = "poison";
	if (f2 & (TR2_RES_DARK))	vp[vn++] = "dark";

	if (f2 & (TR2_RES_FEAR))	vp[vn++] = "fear";
	if (f2 & (TR2_RES_BLIND))	vp[vn++] = "blindness";
	if (f2 & (TR2_RES_CONFU))	vp[vn++] = "confusion";
	if (f2 & (TR2_RES_STUN))	vp[vn++] = "stunning";
	if (f2 & (TR2_RES_HALLU))	vp[vn++] = "hallucination";

	/* Describe resistances */
	output_desc_list("It provides resistance to ", vp, vn);

	/* We are done here */
	return (vn ? TRUE : FALSE);
}

/*
 * Describe resistances granted by an object.
 */
static bool describe_vulnerability(const object_type *o_ptr, u32b f2)
{
	cptr vp[17];
	int vn = 0;
	
	/* Unused parameter */
	(void)o_ptr;
	
	/* Collect vaulnerabilities */
	if (f2 & (TR2_VUL_COLD))	vp[vn++] = "cold";
	if (f2 & (TR2_VUL_FIRE))	vp[vn++] = "fire";
	if (f2 & (TR2_VUL_POIS))	vp[vn++] = "poison";
		
	/* Describe resistances */
	output_desc_list("It makes you more vulnerable to ", vp, vn);
	
	/* We are done here */
	return (vn ? TRUE : FALSE);
}


/*
 * Describe the 'handedness' of a weapon
 */
static bool describe_handedness(const object_type *o_ptr, u32b f3)
{
	int n = o_ptr->tval;

	if ((n == TV_DIGGING) || (n == TV_HAFTED) ||
		(n == TV_POLEARM) || (n == TV_SWORD))
	{
		if (f3 & (TR3_HAND_AND_A_HALF))		p_text_out("It does extra damage when wielded with both hands.  ");
		else if (f3 & (TR3_TWO_HANDED))		p_text_out("It requires both hands to wield it properly.  ");
		else								return (FALSE);

		return (TRUE);
	}

	return (FALSE);
}

/*
 * Describe the 'polearmness' of a weapon
 */
static bool describe_polearmness(u32b f3)
{
	if (f3 & (TR3_POLEARM))
	{
		p_text_out("It counts as a type of polearm.  ");
		return (TRUE);
	}
	
	return (FALSE);
}



/*
 * Describe 'ignores' of an object.
 */
static bool describe_ignores(const object_type *o_ptr, u32b f3)
{
//	cptr list[4];
	int n = 0;

	/* Unused parameter */
	(void)o_ptr;

	/* Collect the ignores */
//	if ((f3 & (TR3_IGNORE_ACID)) && hates_acid(o_ptr)) list[n++] = "acid";
//	if ((f3 & (TR3_IGNORE_ELEC)) && hates_elec(o_ptr)) list[n++] = "electricity";
//	if ((f3 & (TR3_IGNORE_FIRE)) && hates_fire(o_ptr)) list[n++] = "fire";
//	if ((f3 & (TR3_IGNORE_COLD)) && hates_cold(o_ptr)) list[n++] = "cold";

	/* Describe ignores */
	if ((f3 & (TR3_IGNORE_ACID)) && (f3 & (TR3_IGNORE_FIRE)) && (f3 & (TR3_IGNORE_COLD)))
	{
		p_text_out("It cannot be harmed by the elements.  ");
		
		return (TRUE);
	}
//	else
//		output_desc_list("It cannot be harmed by ", list, - n);
//
	return ((n > 0) ? TRUE : FALSE);
}


/*
 * Describe stat sustains.
 */
static bool describe_sustains(const object_type *o_ptr, u32b f2)
{
	cptr list[A_MAX];
	int n = 0;

	/* Unused parameter */
	(void)o_ptr;

	/* Collect the sustains */
	if (f2 & (TR2_SUST_STR)) list[n++] = stat_names_full[A_STR];
	if (f2 & (TR2_SUST_DEX)) list[n++] = stat_names_full[A_DEX];
	if (f2 & (TR2_SUST_CON)) list[n++] = stat_names_full[A_CON];
	if (f2 & (TR2_SUST_GRA)) list[n++] = stat_names_full[A_GRA];

	/* Describe immunities */
	if (n == A_MAX)
		p_text_out("It sustains all your stats.  ");
	else
		output_desc_list("It sustains your ", list, n);

	/* We are done here */
	return (n ? TRUE : FALSE);
}


/*
 * Describe miscellaneous powers such as see invisible, free action,
 * permanent light, etc; also note curses and penalties.
 */
static bool describe_misc_magic(const object_type *o_ptr, u32b f2, u32b f3)
{
	cptr good[7], bad[6];
	int gc = 0, bc = 0;
	bool something = FALSE;

	/* Throwing weapons. */
	if (f3 & (TR3_THROWING))
	{
		good[gc++] = (format("can be thrown effectively (%d squares)",throwing_range(o_ptr)));
	}

	/* Collect stuff which can't be categorized */
	if (((o_ptr->tval == TV_LIGHT) && artefact_p(o_ptr)) || ((o_ptr->tval != TV_LIGHT) && (f2 & (TR2_LIGHT))))
		good[gc++] = "lights the dungeon around you";
	if ((f2 & (TR2_LIGHT)) && (o_ptr->tval == TV_LIGHT))	good[gc++] = "burns brightly, increasing your light radius by an additional square";
	if (f2 & (TR2_SLOW_DIGEST))								good[gc++] = "reduces your need for food";
	if ((f2 & (TR2_RADIANCE)) && (o_ptr->tval == TV_BOW))	good[gc++] = "fires shining arrows";
	if ((f2 & (TR2_RADIANCE)) && (o_ptr->tval == TV_BOOTS))	good[gc++] = "lights your path behind you";
	if (f2 & (TR2_REGEN))									good[gc++] = "speeds your regeneration (which also increases your hunger)";

	/* Describe */
	output_desc_list("It ", good, gc);

	/* Set "something" */
	if (gc) something = TRUE;

	/* Collect granted powers */
	gc = 0;
	if (f2 & (TR2_SPEED))     good[gc++] = "great speed";
	if (f2 & (TR2_FREE_ACT))  good[gc++] = "freedom of movement";
	if (f2 & (TR2_SEE_INVIS)) good[gc++] = "the ability to see invisible creatures";

	/* Collect penalties */
	if (f2 & (TR2_DANGER))	   bad[bc++] = "makes you encounter more dangerous creatures (even when not worn)";
	if (f2 & (TR2_FEAR))	   bad[bc++] = "causes you to panic in combat";
	if (f2 & (TR2_HUNGER))	   bad[bc++] = "increases your hunger";
	if (f2 & (TR2_DARKNESS))   bad[bc++] = "creates an unnatural darkness";
	if (f2 & (TR2_SLOWNESS))   bad[bc++] = "slows your movement";
	if (f2 & (TR2_AGGRAVATE))  bad[bc++] = "enrages nearby creatures";
	if (f2 & (TR2_HAUNTED))	   bad[bc++] = "draws wraiths to your level";

	/* Deal with cursed stuff */
	if (cursed_p(o_ptr))
	{
		if (f3 & (TR3_PERMA_CURSE)) bad[bc++] = "is permanently cursed";
		else if (f3 & (TR3_HEAVY_CURSE)) bad[bc++] = "is heavily cursed";
		else if (object_known_p(o_ptr)) bad[bc++] = "is cursed";
	}

	/* Describe */
	if (gc)
	{
		/* Output intro */
		p_text_out("It grants you ");

		/* Output list */
		output_list(good, gc);

		/* Output end (if needed) */
		if (!bc) p_text_out(".  ");
	}

	if (bc)
	{
		/* Output intro */
		if (gc) p_text_out(", but it also ");
		else p_text_out("It ");

		/* Output list */
		output_list(bad, bc);

		/* Output end */
		p_text_out(".  ");
	}

	/* Set "something" */
	if (gc || bc) something = TRUE;

	/* Return "something" */
	return (something);
}


static cptr act_description[ACT_MAX] =
{
	"illumination",
	"magic mapping",
	"clairvoyance",
	"protection from evil",
	"dispel evil (x5)",
	"heal (500)",
	"heal (1000)",
	"cure wounds (4d7)",
	"haste self (20+d20 turns)",
	"haste self (75+d75 turns)",
	"fire bolt (9d8)",
	"fire ball (72)",
	"large fire ball (120)",
	"frost bolt (6d8)",
	"frost ball (48)",
	"frost ball (100)",
	"frost bolt (12d8)",
	"large frost ball (200)",
	"acid bolt (5d8)",
	"recharge item I",
	"sleep II",
	"lightning bolt (4d8)",
	"large lightning ball (250)",
	"banishment",
	"mass banishment",
	"*identify*",
	"drain life (90)",
	"drain life (120)",
	"bizarre things",
	"star ball (150)",
	"berserk rage, bless, and resistance",
	"phase door",
	"door and trap destruction",
	"detection",
	"resistance (20+d20 turns)",
	"teleport",
	"restore voice",
	"magic missile (2d6)",
	"a magical arrow (150)",
	"remove fear and cure poison",
	"stinking cloud (12)",
	"stone to mud",
	"teleport away",
	"word of recall",
	"confuse monster",
	"probing",
	"fire branding of bolts",
	"starlight (10d8)",
	"mana bolt (12d8)",
	"berserk rage (50+d50 turns)",
	"resist acid (20+d20 turns)",
	"resist electricity (20+d20 turns)",
	"resist fire (20+d20 turns)",
	"resist cold (20+d20 turns)",
	"resist poison (20+d20 turns)"
};

/*
 * Determine the "Activation" (if any) for an artefact
 */
static void describe_item_activation(const object_type *o_ptr, char *random_name, size_t max)
{
	u32b f1, f2, f3;

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Require activation ability */
	if (!(f3 & TR3_ACTIVATE)) return;

	/* Artefact activations */
	if ((o_ptr->name1) && (o_ptr->name1 < z_info->art_norm_max))
	{
		artefact_type *a_ptr = &a_info[o_ptr->name1];

		/* Paranoia */
		if (a_ptr->activation >= ACT_MAX)
		{
			return;
		}

		/* Some artefacts can be activated */
		my_strcat(random_name, act_description[a_ptr->activation], max);

		/* Output the number of turns */
		if (a_ptr->time && a_ptr->randtime)
			my_strcat(random_name, format(" every %d+d%d turns", a_ptr->time, a_ptr->randtime), max);
		else if (a_ptr->time)
			my_strcat(random_name, format(" every %d turns", a_ptr->time), max);
		else if (a_ptr->randtime)
			my_strcat(random_name, format(" every d%d turns", a_ptr->randtime), max);

		return;
	}

}



/*
 * Describe an object's activation, if any.
 */
static bool describe_activation(const object_type *o_ptr, u32b f3)
{
	/* Check for the activation flag */
	if (f3 & TR3_ACTIVATE)
	{
		char act_desc[120];

		u16b size;

		my_strcpy(act_desc, "It activates for ", sizeof(act_desc));

		/*get the size of the file*/
		size = strlen(act_desc);

		describe_item_activation(o_ptr, act_desc, sizeof(act_desc));

		/*if the previous function added length, we have an activation, so print it out*/
		if (strlen(act_desc) > size)
		{

			my_strcat(act_desc, format(".  "), sizeof(act_desc));

			/*print it out*/
			p_text_out(act_desc);

			return (TRUE);
		}
	}

	/* No activation */
	return (FALSE);
}


/*
 * Describe abilities granted by an object.
 */
static bool describe_abilities(const object_type *o_ptr)
{
	cptr ability[8];
	int ac = 0;
	ability_type *b_ptr;
	int i;
		
	// only describe when identified
	if (!object_known_p(o_ptr) && !(o_ptr->ident & (IDENT_SPOIL))) return (FALSE);
	
	// check its abilities
	for (i = 0; i < o_ptr->abilities; i++)
	{
		b_ptr = &b_info[ability_index(o_ptr->skilltype[i], o_ptr->abilitynum[i])];
		ability[ac++] = b_name + b_ptr->name;
	}
	
	/* Describe */
	if (ac)
	{
		/* Output intro */
		if (ac == 1)	p_text_out("It grants you the ability: ");
		else			p_text_out("It grants you the abilities: ");
				
		/* Output list */
		output_list(ability, ac);
		
		/* Output end (if needed) */
		p_text_out(".  ");
		
		/* It granted abilities */
		return (TRUE);
	}
	
	/* No abilities granted */
	return (FALSE);
}


/*
 * Describe attributes of bows and arrows.
 */
static bool describe_archery(const object_type *o_ptr)
{
	if (o_ptr->tval == TV_BOW)
	{
		p_text_out(format("It can shoot arrows %d squares (with your current strength).", archery_range(o_ptr)));
		return (TRUE);
	}
	if (o_ptr->tval == TV_ARROW)
	{
		if ((&inventory[INVEN_BOW])->k_idx)
		{
			if (o_ptr->number == 1)
			{
				p_text_out(format("It can be shot %d squares (with your current strength and bow).", archery_range(&inventory[INVEN_BOW])));
			}
			else
			{
				p_text_out(format("They can be shot %d squares (with your current strength and bow).", archery_range(&inventory[INVEN_BOW])));
			}
		}
		else
		{
			if (o_ptr->number == 1)
			{
				p_text_out("It can be shot by a bow.");
			}
			else
			{
				p_text_out("They can be shot by a bow.");
			}
		}
		return (TRUE);
	}
	
	/* Not archery related */
	return (FALSE);
}


/*
 * Output object information
 */
bool object_info_out(const object_type *o_ptr)
{
	u32b f1, f2, f3;
	u32b ff1, ff2, ff3;
	bool something = FALSE;

	/* Grab the object flags */
	object_info_out_flags(o_ptr, &f1, &f2, &f3);

	/* Hack - grab the ID-independent flags */
	/* Used to show handedness even when not ID'd */
	object_flags(o_ptr, &ff1, &ff2, &ff3);

	/* Describe the object */
	if (describe_stats(o_ptr, f1))						something = TRUE;
	if (describe_neg_stats(o_ptr, f1))					something = TRUE;
	if (describe_secondary(o_ptr, f1))					something = TRUE;
	if (describe_slay(o_ptr, f1))						something = TRUE;
	if (describe_brand(o_ptr, f1))						something = TRUE;
	if (describe_misc_weapon_attributes(o_ptr, f1))		something = TRUE;
	if (describe_resist(o_ptr, f2))						something = TRUE;
	if (describe_vulnerability(o_ptr, f2))				something = TRUE;
	if (describe_sustains(o_ptr, f2))					something = TRUE;
	if (describe_misc_magic(o_ptr, f2, f3))				something = TRUE;
	if (describe_activation(o_ptr, f3))					something = TRUE;
	if (describe_ignores(o_ptr, f3))					something = TRUE;
	if (describe_abilities(o_ptr))						something = TRUE;
	
	if (describe_handedness(o_ptr, ff3))				something = TRUE;
	if (describe_polearmness(ff3))						something = TRUE;
	if (describe_archery(o_ptr))						something = TRUE;

	
	/* We are done. */
	return something;
}


/*
 * Header for additional information when printing to screen.
 *
 * Header for additional information when printing to screen.
 */
static bool screen_out_head(const object_type *o_ptr)
{
	char *o_name;
	int name_size = Term->wid;

	bool has_description = FALSE;

	/* Allocate memory to the size of the screen */
	o_name = C_RNEW(name_size, char);

	/* Description */
	object_desc(o_name, name_size, o_ptr, TRUE, 3);

	/* Print, in colour */
	text_out_c(TERM_YELLOW, format("%^s", o_name));

	/* Free up the memory */
	FREE(o_name);

	/* Display the known artefact description */
	if (!adult_rand_artefacts && o_ptr->name1 &&
	    object_known_p(o_ptr) && a_info[o_ptr->name1].text)

	{
		p_text_out("\n\n   ");
		p_text_out(a_text + a_info[o_ptr->name1].text);
		has_description = TRUE;
	}
	/* Display the known object description */
	else if (object_aware_p(o_ptr) || object_known_p(o_ptr))
	{
		if (k_info[o_ptr->k_idx].text)
		{
			p_text_out("\n\n   ");
			p_text_out(k_text + k_info[o_ptr->k_idx].text);
			has_description = TRUE;
		}

		/* Display an additional special item description */
		if (o_ptr->name2 && object_known_p(o_ptr) && e_info[o_ptr->name2].text)
		{
			p_text_out("\n\n   ");
			p_text_out(e_text + e_info[o_ptr->name2].text);
			has_description = TRUE;
		}
	}

	return (has_description);
}


/*
 * Display the text from a note on the screen.
 */
void note_info_screen(const object_type *o_ptr)
{	
	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	
	/* Save the screen */
	screen_save();
			
	/* Indent output by 14 characters, and wrap at column 60 */
	text_out_wrap = 60;
	text_out_indent = 14;
	
	/* Note intro */
	Term_gotoxy(text_out_indent, 0);
	text_out_c(TERM_L_WHITE+TERM_SHADE, "The note here reads:\n\n");

	/* Note text */
	Term_gotoxy(text_out_indent, 2);
	text_out_to_screen(TERM_WHITE, k_text + k_info[o_ptr->k_idx].text);

	/* Note outro */
	text_out_c(TERM_L_WHITE+TERM_SHADE, "\n\n(press any key)\n");
	
	/* Reset text_out() vars */
	text_out_wrap = 0;
	text_out_indent = 0;
	
	/* Wait for input */
	(void)inkey();
	
	/* Load the screen */
	screen_load();
	
	return;
}


/*
 * Place an item description on the screen.
 */
void object_info_screen(const object_type *o_ptr)
{
	bool has_description, has_info;

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;

	/* Save the screen */
	screen_save();

	has_description = screen_out_head(o_ptr);

	object_info_out_flags = object_flags_known;

	/* Dump the info */
	new_paragraph = TRUE;
	has_info = object_info_out(o_ptr);
	new_paragraph = FALSE;

	if (!object_known_p(o_ptr))
	{
		p_text_out("\n\n   This item has not been identified.");
	}
	else if ((!has_description) && (!has_info))
	{
		p_text_out("\n\n   This item does not seem to possess any special abilities.");
	}

	text_out_c(TERM_L_BLUE, "\n\n(press any key)\n");

	/* Wait for input */
	(void)inkey();

	/* Load the screen */
	screen_load();

	return;
}

