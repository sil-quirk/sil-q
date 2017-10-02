/* File: wizard1.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


#ifdef ALLOW_SPOILERS


/*
 * The spoiler file being created
 */
static FILE *fff = NULL;


/*
 * Write out `n' of the character `c' to the spoiler file
 */
static void spoiler_out_n_chars(int n, char c)
{
	while (--n >= 0) fputc(c, fff);
}

/*
 * Write out `n' blank lines to the spoiler file
 */
static void spoiler_blanklines(int n)
{
	spoiler_out_n_chars(n, '\n');
}

/*
 * Write a line to the spoiler file and then "underline" it with hypens
 */
static void spoiler_underline(cptr str, char c)
{
	text_out(str);
	text_out("\n");
	spoiler_out_n_chars(strlen(str), c);
	text_out("\n");
}


/*
 * A tval grouper
 */
typedef struct
{
	byte tval;
	cptr name;
} grouper;



/*
 * Item Spoilers by Ben Harrison (benh@phial.com)
 */


/*
 * The basic items categorized by type
 */
static const grouper group_item[] =
{
	{ TV_ARROW,			"Arrow" },

	{ TV_BOW,			"Bows" },

	{ TV_SWORD,			"Weapons" },
	{ TV_POLEARM,		NULL },
	{ TV_HAFTED,		NULL },
	{ TV_DIGGING,		NULL },

	{ TV_SOFT_ARMOR,	"Armour (Body)" },
	{ TV_MAIL,			NULL },

	{ TV_CLOAK,			"Armour (Misc)" },
	{ TV_SHIELD,		NULL },
	{ TV_HELM,			NULL },
	{ TV_CROWN,			NULL },
	{ TV_GLOVES,		NULL },
	{ TV_BOOTS,			NULL },

	{ TV_AMULET,		"Amulets" },
	{ TV_RING,			"Rings" },

	{ TV_POTION,		"Potions" },
	{ TV_FOOD,			"Food" },

	{ TV_HORN,	    	"Horns" },
	{ TV_STAFF,			"Staffs" },

	{ TV_CHEST,			"Chests" },

	{ TV_LIGHT,			"Various" },
	{ TV_FLASK,			NULL },
	{ TV_USELESS,		NULL },
	{ TV_METAL,			NULL },

	{ 0, "" }
};





/*
 * Describe the kind
 */
static void kind_info(char *d_char, char *buf, char *wgt, int *lev, int *rar, int k)
{
	object_kind *k_ptr;

	object_type *i_ptr;
	object_type object_type_body;


	/* Get local object */
	i_ptr = &object_type_body;

	/* Prepare a fake item */
	object_prep(i_ptr, k);

	/* Obtain the "kind" info */
	k_ptr = &k_info[i_ptr->k_idx];

	/* Cancel bonuses */
	i_ptr->pval = 0;

	/* Level */
	(*lev) = k_ptr->locale[0];

	/* Make known */
	i_ptr->ident |= (IDENT_KNOWN);

	/* Rarity */
	(*rar) = k_ptr->chance[0];

	/* Symbol */
	(*d_char) = k_ptr->d_char;

	/* Hack */
	if (!buf || !wgt) return;

	/* Description (too brief) */
	object_desc_spoil(buf, 80, i_ptr, FALSE, 1);

	/* Weight */
	sprintf(wgt, "%3d.%d lb", k_ptr->weight / 10, k_ptr->weight % 10);
}


/*
 * Create a spoiler file for items
 */
static void spoil_obj_desc(cptr fname)
{
	int i, k, s, n = 0;

	u16b who[200];

	char buf[1024];

	char wgt[80];
	
	char d_char;

	cptr format = " %-42s  %7s%8s%9s\n";

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Open the file */
	fff = my_fopen(buf, "w");

	/* Oops */
	if (!fff)
	{
		msg_print("Cannot create spoiler file.");
		return;
	}


	/* Header */
	fprintf(fff, "Spoiler File -- Basic Items (%s)\n\n\n", VERSION_STRING);

	/* More Header */
	fprintf(fff, format, "Description", "Weight", "Level", "/ Rarity");
	fprintf(fff, format, "----------------------------------------",
	        "-------", "-----", "---------");

	/* List the groups */
	for (i = 0; TRUE; i++)
	{
		/* Write out the group title */
		if (group_item[i].name)
		{
			/* Hack -- bubble-sort by cost and then level */
			/*for (s = 0; s < n - 1; s++)
			{
				for (t = 0; t < n - 1; t++)
				{
					int i1 = t;
					int i2 = t + 1;

					int e1;
					int e2;

					s32b t1;
					s32b t2;

					kind_info(NULL, NULL, NULL, NULL, &e1, &t1, who[i1]);
					kind_info(NULL, NULL, NULL, NULL, &e2, &t2, who[i2]);

					if ((t1 > t2) || ((t1 == t2) && (e1 > e2)))
					{
						int tmp = who[i1];
						who[i1] = who[i2];
						who[i2] = tmp;
					}
				}
			}*/

			/* Spoil each item */
			for (s = 0; s < n; s++)
			{
				int e;
				int r;

				/* Describe the kind */
				kind_info(&d_char, buf, wgt, &e, &r, who[s]);

				/* Dump it */
				fprintf(fff, "%c %-42s%7s%8d / %2d\n",
				        d_char, buf, wgt, e, r);
			}

			/* Start a new set */
			n = 0;

			/* Notice the end */
			if (!group_item[i].tval) break;

			/* Start a new set */
			fprintf(fff, "\n\n%s\n\n", group_item[i].name);
		}

		/* Get legal item types */
		for (k = 1; k < z_info->k_max; k++)
		{
			object_kind *k_ptr = &k_info[k];

			/* Skip wrong tval's */
			if (k_ptr->tval != group_item[i].tval) continue;

			/* Hack -- Skip instant-artefacts */
			if (k_ptr->flags3 & (TR3_INSTA_ART)) continue;

			/* Save the index */
			who[n++] = k;
		}
	}


	/* Check for errors */
	if (ferror(fff) || my_fclose(fff))
	{
		msg_print("Cannot close spoiler file.");
		return;
	}

	/* Message */
	msg_print("Successfully created a spoiler file.");
}



/*
 * Artefact Spoilers by: randy@PICARD.tamu.edu (Randy Hutson)
 *
 * (Mostly) rewritten in 2002 by Andrew Sidwell and Robert Ruehlmann.
 */


/*
 * The artefacts categorized by type
 */
static const grouper group_artefact[] =
{
	{ TV_SWORD,         "Blades" },
	{ TV_POLEARM,       "Axes/Polearms" },
	{ TV_HAFTED,        "Blunt Weapons" },
	{ TV_BOW,           "Bows" },
	{ TV_DIGGING,       "Diggers" },

	{ TV_SOFT_ARMOR,    "Body Armour" },
	{ TV_MAIL,			NULL },

	{ TV_CLOAK,         "Cloaks" },
	{ TV_SHIELD,        "Shields" },
	{ TV_HELM,          "Helms/Crowns" },
	{ TV_CROWN,         NULL },
	{ TV_GLOVES,        "Gloves" },
	{ TV_BOOTS,         "Boots" },

	{ TV_LIGHT,          "Light Sources" },
	{ TV_AMULET,        "Amulets" },
	{ TV_RING,          "Rings" },

	{ 0, NULL }
};


/*
 * Hack -- Create a "forged" artefact
 */
bool make_fake_artefact(object_type *o_ptr, byte name1)
{
	int i;

	artefact_type *a_ptr = &a_info[name1];

	/* Ignore "empty" artefacts */
	if (a_ptr->tval + a_ptr->sval == 0) return FALSE;

	/* Get the "kind" index */
	i = lookup_kind(a_ptr->tval, a_ptr->sval);

	/* Oops */
	if (!i) return (FALSE);

	/* Create the artefact */
	object_prep(o_ptr, i);

	/* Save the name */
	o_ptr->name1 = name1;

	/* Extract the fields */
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
	
	/* Hack -- extract the "cursed" flag */
	if (a_ptr->flags3 & (TR3_LIGHT_CURSE)) o_ptr->ident |= (IDENT_CURSED);

	/* Success */
	return (TRUE);
}


/*
 * Create a spoiler file for artefacts
 */
static void spoil_artefact(cptr fname)
{
	int i, j;

	object_type *i_ptr;
	object_type object_type_body;

	char buf[1024];


	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Open the file */
	fff = my_fopen(buf, "w");

	/* Oops */
	if (!fff)
	{
		msg_print("Cannot create spoiler file.");
		return;
	}

	/* Dump to the spoiler file */
	text_out_hook = text_out_to_file;
	text_out_file = fff;

	/* Set object_info_out() hook */
	object_info_out_flags = object_flags;

	/* Dump the header */
	spoiler_underline(format("Artefact Spoilers for %s %s",
	                         VERSION_NAME, VERSION_STRING), '=');

	/* List the artefacts by tval */
	for (i = 0; group_artefact[i].tval; i++)
	{
		/* Write out the group title */
		if (group_artefact[i].name)
		{
			spoiler_blanklines(2);
			spoiler_underline(group_artefact[i].name, '=');
			spoiler_blanklines(1);
		}

		/* Now search through all of the artefacts */
		for (j = 1; j < z_info->art_max; ++j)
		{
			artefact_type *a_ptr = &a_info[j];
			char buf[80];

			if (j >= ART_ULTIMATE) continue;

			/* We only want objects in the current group */
			if (a_ptr->tval != group_artefact[i].tval) continue;

			/* Get local object */
			i_ptr = &object_type_body;

			/* Wipe the object */
			object_wipe(i_ptr);

			/* Attempt to "forge" the artefact */
			if (!make_fake_artefact(i_ptr, (byte)j)) continue;

			/* Grab artefact name */
			object_desc_spoil(buf, sizeof(buf), i_ptr, TRUE, 1);
			
			my_strcat(buf, format("     %d.%d lb",(a_ptr->weight / 10),(a_ptr->weight % 10)), sizeof (buf));

			/* Print name and underline */
			spoiler_underline(buf, '-');

			// mark as a spoiled item
			i_ptr->ident |= (IDENT_SPOIL);

			/* Write out the artefact description to the spoiler file */
			object_info_out(i_ptr);

			/*
			 * Determine the minimum depth an artefact can appear and its rarity.
			 */
			
			text_out(format("\n(level %u / rarity %u / quality %d)\n", a_ptr->level, a_ptr->rarity, object_difficulty(i_ptr)));

			/* Terminate the entry */
			spoiler_blanklines(2);
		}
	}

	/* Check for errors */
	if (ferror(fff) || my_fclose(fff))
	{
		msg_print("Cannot close spoiler file.");
		return;
	}

	/* Message */
	msg_print("Successfully created a spoiler file.");
}



/*
 * Create a spoiler file for monsters
 */
static void spoil_mon_desc(cptr fname)
{
	int i, n = 0;

	char buf[1024];

	char nam[80];
	char lev[80];
	char rar[80];
	char spd[80];
	char hp[80];
	char def1[80];
	char def2[80];
	char att1[80];
	char att2[80];

	u16b *who;
	u16b why = 2;


	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Open the file */
	fff = my_fopen(buf, "w");

	/* Oops */
	if (!fff)
	{
		msg_print("Cannot create spoiler file.");
		return;
	}

	/* Dump the header */
	fprintf(fff, "Monster Spoilers for %s Version %s\n",
	        VERSION_NAME, VERSION_STRING);
	fprintf(fff, "------------------------------------------\n\n");

	/* Dump the header */
	fprintf(fff, "%-42.42s%10s%6s%8s%13s%30s\n",
	        "Name", "Lev / Rar", "Spd", "Health", "Defence", "Attacks        ");
	fprintf(fff, "%-42.42s%10s%6s%8s%13s%30s\n",
	        "-----------------------------------------", "---------", "---", "------", "----------", "--------------------------");

	/* Allocate the "who" array */
	C_MAKE(who, z_info->r_max, u16b);

	/* Scan the monsters */
	for (i = 1; i < z_info->r_max; i++)
	{
		monster_race *r_ptr = &r_info[i];

		/* Use all monsters that can appear */
		if (r_ptr->name && (r_ptr->rarity > 0) && (r_ptr->level <= 25)) who[n++] = (u16b)i;
	}

	/* Select the sort method */
	ang_sort_comp = ang_sort_comp_hook;
	ang_sort_swap = ang_sort_swap_hook;

	/* Sort the array by dungeon depth of monsters */
	ang_sort(who, &why, n);

	/* Scan again */
	for (i = 0; i < n; i++)
	{
		monster_race *r_ptr = &r_info[who[i]];

		cptr name = (r_name + r_ptr->name);

		/* Get the "name" */
		if (r_ptr->flags1 & (RF1_UNIQUE))
		{
			strnfmt(nam, sizeof(nam), "* %c  %s", r_ptr->d_char, name);
		}
		else
		{
			strnfmt(nam, sizeof(nam), "  %c  %s", r_ptr->d_char, name);
		}

		/* Level */
		sprintf(lev, "%d", r_ptr->level);

		/* Rarity */
		sprintf(rar, "%d", r_ptr->rarity);

		/* Speed */
		sprintf(spd, "%d", r_ptr->speed);

		/* Hitpoints */
		if ((r_ptr->flags1 & (RF1_UNIQUE)) || (r_ptr->hside == 1))
		{
			sprintf(hp, "%d", r_ptr->hdice * (1 + r_ptr->hside) / 2);
		}
		else
		{
			sprintf(hp, "%dd%d", r_ptr->hdice, r_ptr->hside);
		}

		/* Defence */
		sprintf(def1, "[%+2d", r_ptr->evn);
		if (r_ptr->pd == 0)
		{
			sprintf(def2, "]");
		}
		else
		{
			sprintf(def2, ", %dd%d]", r_ptr->pd, r_ptr->ps);
		}

		/* Attack 1 */
		if (r_ptr->blow[0].method != 0)
		{
			char special = ' ';
			int effect = r_ptr->blow[0].effect;

			if (effect > RBE_HURT)   special = '*';
			
			if (r_ptr->blow[0].method == RBM_SPORE)
			{
				sprintf(att1, "(%dd%d)%c", r_ptr->blow[0].dd, r_ptr->blow[0].ds, special);
			}
			else
			{
				sprintf(att1, "(%+2d, %dd%d)%c", r_ptr->blow[0].att,
												 r_ptr->blow[0].dd, r_ptr->blow[0].ds, special);
			}
		}
		else
		{
			sprintf(att1, " ");
		}

		/* Attack 2 */
		if (r_ptr->blow[1].method != 0)
		{
			char special = ' ';
			int effect = r_ptr->blow[1].effect;

			if (effect > RBE_HURT)   special = '*';
			
			if (r_ptr->blow[1].method == RBM_SPORE)
			{
				sprintf(att2, "(%dd%d)%c", r_ptr->blow[1].dd, r_ptr->blow[1].ds, special);
			}
			else
			{
				sprintf(att2, "(%+2d, %dd%d)%c", r_ptr->blow[1].att,
												 r_ptr->blow[1].dd, r_ptr->blow[1].ds, special);
			}
		}
		else
		{
			sprintf(att2, " ");
		}

		// /* Complex Visual */
		// strnfmt(vis, sizeof(vis), "%s '%c'", attr_to_text(r_ptr->d_attr), r_ptr->d_char);

		/* Dump the info */
		fprintf(fff, "%-42.42s%4s /%3s%7s%8s%7s%-6s%16s%14s\n",
		        nam, lev, rar, spd, hp, def1, def2, att1, att2);
	}

	/* End it */
	fprintf(fff, "\n");

	/* Free the "who" array */
	FREE(who);


	/* Check for errors */
	if (ferror(fff) || my_fclose(fff))
	{
		msg_print("Cannot close spoiler file.");
		return;
	}

	/* Worked */
	msg_print("Successfully created a spoiler file.");
}


/*
 * Create a spoiler file for monsters
 */
static void spoil_mon_ss(cptr fname)
{
	int i, n = 0;
	
	char buf[1024];
	
	char nam[80];
	char lev[80];
	char rar[80];
	char spd[80];
	char hp[80];
	char def1[80];
	char def2[80];
	char att1[80];
	char att2[80];
	
	u16b *who;
	u16b why = 2;
	
	
	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);
	
	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);
	
	/* Open the file */
	fff = my_fopen(buf, "w");
	
	/* Oops */
	if (!fff)
	{
		msg_print("Cannot create spoiler file.");
		return;
	}
	
	/* Allocate the "who" array */
	C_MAKE(who, z_info->r_max, u16b);
	
	/* Scan the monsters */
	for (i = 1; i < z_info->r_max; i++)
	{
		monster_race *r_ptr = &r_info[i];
		
		/* Use all monsters that can appear */
		if (r_ptr->name && (r_ptr->rarity > 0) && (r_ptr->level <= 25)) who[n++] = (u16b)i;
	}
	
	/* Select the sort method */
	ang_sort_comp = ang_sort_comp_hook;
	ang_sort_swap = ang_sort_swap_hook;
	
	/* Sort the array by dungeon depth of monsters */
	ang_sort(who, &why, n);
	
	/* Scan again */
	for (i = 0; i < n; i++)
	{
		monster_race *r_ptr = &r_info[who[i]];
		
		cptr name = (r_name + r_ptr->name);
		
		/* Get the "name" */
		if (r_ptr->flags1 & (RF1_UNIQUE))
		{
			strnfmt(nam, sizeof(nam), "*\t%c\t%s", r_ptr->d_char, name);
		}
		else
		{
			strnfmt(nam, sizeof(nam), "\t%c\t%s", r_ptr->d_char, name);
		}
		
		/* Level */
		sprintf(lev, "%d", r_ptr->level);
		
		/* Rarity */
		sprintf(rar, "%d", r_ptr->rarity);
		
		/* Speed */
		sprintf(spd, "%d", r_ptr->speed);
		
		/* Hitpoints */
		sprintf(hp, "%d", r_ptr->hdice);
		
		/* Defence */
		sprintf(def1, "%+d", r_ptr->evn);
		if (r_ptr->pd == 0)
		{
			sprintf(def2, "0\t0");
		}
		else
		{
			sprintf(def2, "%d\t%d", r_ptr->pd, r_ptr->ps);
		}
		
		/* Attack 1 */
		if (r_ptr->blow[0].method != 0)
		{
			char special = ' ';
			int effect = r_ptr->blow[0].effect;
			
			if (effect > RBE_HURT)   special = '*';
			
			if (r_ptr->blow[0].method == RBM_SPORE)
			{
				sprintf(att1, "\t%d\t%d\t%c", r_ptr->blow[0].dd, r_ptr->blow[0].ds, special);
			}
			else
			{
				sprintf(att1, "%+d\t%d\t%d\t%c", r_ptr->blow[0].att,
						r_ptr->blow[0].dd, r_ptr->blow[0].ds, special);
			}
		}
		else
		{
			sprintf(att1, "\t\t\t");
		}
		
		/* Attack 2 */
		if (r_ptr->blow[1].method != 0)
		{
			char special = ' ';
			int effect = r_ptr->blow[1].effect;
			
			if (effect > RBE_HURT)   special = '*';
			
			if (r_ptr->blow[1].method == RBM_SPORE)
			{
				sprintf(att2, "\t%d\t%d\t%c", r_ptr->blow[1].dd, r_ptr->blow[1].ds, special);
			}
			else
			{
				sprintf(att2, "%+d\t%d\t%d\t%c", r_ptr->blow[1].att,
						r_ptr->blow[1].dd, r_ptr->blow[1].ds, special);
			}
		}
		else
		{
			sprintf(att2, "\t\t\t");
		}
		
		// /* Complex Visual */
		// strnfmt(vis, sizeof(vis), "%s '%c'", attr_to_text(r_ptr->d_attr), r_ptr->d_char);
		
		/* Dump the info */
		fprintf(fff, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
		        nam, lev, rar, spd, hp, def1, def2, att1, att2);
	}
	
	/* End it */
	fprintf(fff, "\n");
	
	/* Free the "who" array */
	FREE(who);
	
	
	/* Check for errors */
	if (ferror(fff) || my_fclose(fff))
	{
		msg_print("Cannot close spoiler file.");
		return;
	}
	
	/* Worked */
	msg_print("Successfully created a spoiler file.");
}




/*
 * Monster spoilers originally by: smchorse@ringer.cs.utsa.edu (Shawn McHorse)
 */

/*
 * Create a spoiler file for monsters (-SHAWN-)
 */
static void spoil_mon_info(cptr fname)
{
	char buf[1024];
	int i, n;
	u16b why = 2;
	u16b *who;
	int count = 0;


	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Open the file */
	fff = my_fopen(buf, "w");

	/* Oops */
	if (!fff)
	{
		msg_print("Cannot create spoiler file.");
		return;
	}

	/* Dump to the spoiler file */
	text_out_hook = text_out_to_file;
	text_out_file = fff;

	/* Dump the header */
	strnfmt(buf, sizeof(buf), "Monster Spoilers for %s Version %s\n",
	        VERSION_NAME, VERSION_STRING);
	text_out(buf);
	text_out("------------------------------------------\n\n");

	/* Allocate the "who" array */
	C_MAKE(who, z_info->r_max, u16b);

	/* Scan the monsters */
	for (i = 1; i < z_info->r_max; i++)
	{
		monster_race *r_ptr = &r_info[i];

		/* Use that monster */
		if (r_ptr->name) who[count++] = (u16b)i;
	}

	/* Select the sort method */
	ang_sort_comp = ang_sort_comp_hook;
	ang_sort_swap = ang_sort_swap_hook;

	/* Sort the array by dungeon depth of monsters */
	ang_sort(who, &why, count);

	/*
	 * List all monsters in order.
	 */
	for (n = 0; n < count; n++)
	{
		int r_idx = who[n];
		monster_race *r_ptr = &r_info[r_idx];

		/* Prefix */
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			text_out("[U] ");
		}
		else
		{
			text_out("The ");
		}

		/* Name */
		strnfmt(buf, sizeof(buf), "%s  (", (r_name + r_ptr->name));	/* ---)--- */
		text_out(buf);

		/* Color */
		text_out(attr_to_text(r_ptr->d_attr));

		/* Symbol --(-- */
		sprintf(buf, " '%c')\n", r_ptr->d_char);
		text_out(buf);


		/* Indent */
		sprintf(buf, "=== ");
		text_out(buf);

		/* Number */
		sprintf(buf, "Num:%d  ", r_idx);
		text_out(buf);

		/* Level */
		sprintf(buf, "Lev:%d  ", r_ptr->level);
		text_out(buf);

		/* Rarity */
		sprintf(buf, "Rar:%d  ", r_ptr->rarity);
		text_out(buf);

		/* Speed */
		sprintf(buf, "Spd:%d  ", r_ptr->speed);
		text_out(buf);

		/* Hitpoints */
		if ((r_ptr->flags1 & RF1_UNIQUE) || (r_ptr->hside == 1))
		{
			sprintf(buf, "Hp:%d  ", r_ptr->hdice * (1 + r_ptr->hside) / 2);
		}
		else
		{
			sprintf(buf, "Hp:%dd%d  ", r_ptr->hdice, r_ptr->hside);
		}
		text_out(buf);

		/* Describe */
		describe_monster(r_idx, TRUE);

		/* Terminate the entry */
		text_out("\n");
	}

	/* Free the "who" array */
	FREE(who);

	/* Check for errors */
	if (ferror(fff) || my_fclose(fff))
	{
		msg_print("Cannot close spoiler file.");
		return;
	}

	msg_print("Successfully created a spoiler file.");
}



/*
 * Create Spoiler files
 */
void do_cmd_spoilers(void)
{
	char ch;


	/* Save screen */
	screen_save();

	/* Interact */
	while (1)
	{
		/* Clear screen */
		Term_clear();

		/* Info */
		prt("Create a spoiler file.", 2, 0);

		/* Prompt for a file */
		prt("(1) Object List (obj-list.txt)", 5, 5);
		prt("(2) Full Artefact Info (art-info.txt)", 6, 5);
		prt("(3) Monster List (mon-list.txt)", 7, 5);
		prt("(4) Full Monster Info (mon-info.txt)", 8, 5);
		prt("(5) Monster Stat Spreadsheet (mon-ss.txt)", 9, 5);

		/* Prompt */
		prt("Command: ", 13, 0);

		/* Get a choice */
		ch = inkey();

		/* Escape */
		if (ch == ESCAPE)
		{
			break;
		}

		/* Option (1) */
		else if (ch == '1')
		{
			spoil_obj_desc("obj-list.txt");
		}

		/* Option (2) */
		else if (ch == '2')
		{
			spoil_artefact("art-info.txt");
		}

		/* Option (3) */
		else if (ch == '3')
		{
			spoil_mon_desc("mon-list.txt");
		}

		/* Option (4) */
		else if (ch == '4')
		{
			spoil_mon_info("mon-info.txt");
		}

		/* Option (5) */
		else if (ch == '5')
		{
			spoil_mon_ss("mon-ss.txt");
		}

		/* Oops */
		else
		{
			bell("Illegal command for spoilers!");
		}

		/* Flush messages */
		message_flush();
	}


	/* Load screen */
	screen_load();
}


#else

#ifdef MACINTOSH
static int i = 0;
#endif

#endif
