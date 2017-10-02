/* File: birth.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/* Locations of the tables on the screen */
#define HEADER_ROW		 1
#define QUESTION_ROW	 3
#define TABLE_ROW		 4
#define DESCRIPTION_ROW	14
#define INSTRUCT_ROW	21

#define QUESTION_COL	2
#define SEX_COL			2
#define RACE_COL		2
#define RACE_AUX_COL    17
#define CLASS_COL		17
#define CLASS_AUX_COL   31
#define TOTAL_AUX_COL   43
#define INVALID_CHOICE 255

/*
 * Forward declare
 */
typedef struct birther birther;
typedef struct birth_menu birth_menu;

/*
 * A structure to hold "rolled" information
 */
struct birther
{
	s16b age;
	s16b wt;
	s16b ht;
	s16b sc;

	s16b stat[A_MAX];

	char history[250];
};

/*
 * A structure to hold the menus
 */
struct birth_menu
{
	bool ghost;
	cptr name;
	cptr text;
};


/*
 * Generate some info that the auto-roller ignores
 */
static void get_extra(void)
{
	/* Experience */
	p_ptr->new_exp = p_ptr->exp = PY_START_EXP;

	/* Player is not singing */
	p_ptr->song1 = SNG_NOTHING;
	p_ptr->song2 = SNG_NOTHING;
}


/*
 * Get the racial history, and social class, using the "history charts".
 */
static void get_history_aux(void)
{
	int i, chart, roll;

	/* Clear the previous history strings */
	p_ptr->history[0] = '\0';

	/* Starting place */
	chart = rp_ptr->hist;

	/* Process the history */
	while (chart)
	{
		/* Start over */
		i = 0;

		/* Roll for nobility */
		roll = dieroll(100);

		/* Get the proper entry in the table */
		while ((chart != h_info[i].chart))  i++;
		
		if (h_info[i].house)
		{
			while ((p_ptr->phouse != h_info[i].house) && h_info[i].house) i++;
		}

		while (roll > h_info[i].roll) i++;

		/* Get the textual history */
		my_strcat(p_ptr->history, (h_text + h_info[i].text), sizeof(p_ptr->history));

		/* Add a space */
		my_strcat(p_ptr->history, " ", sizeof(p_ptr->history));

		/* Enter the next chart */
		chart = h_info[i].next;
	}

	/* Save the social class */
	p_ptr->sc = 1;
}

/*
 * Get the racial history, and social class, using the "history charts".
 */
static bool get_history(void)
{
	int i;
	char line[70];
	char query2;
	int loopagain = TRUE;
	
	// hack to see whether we are opening an old player file
	bool roll_history = !(p_ptr->history[0]);
		
	while (loopagain == TRUE)
	{
		if (roll_history)
		{
			/*get the random history, display for approval. */
			get_history_aux();
		}
		else
		{
			roll_history = TRUE;
		}

		/* Display the player */
		display_player(0);

		// Highlight relevant info
		display_player_xtra_info(2);
		
		/* Prompt */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"Enter accept history");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
					"Space reroll history");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 3, -1, TERM_SLATE,
					"    m manually enter history");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "Enter");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "Space");
		Term_putstr(QUESTION_COL + 4, INSTRUCT_ROW + 3, - 1, TERM_L_WHITE, "m");

		/* Move the cursor */
		Term_gotoxy(0, INSTRUCT_ROW + 1);
		
		/* Query */
		query2 = inkey();

		if ((query2 == '\r') || (query2 == '\n'))
		{
			/* got a history*/
			loopagain = FALSE;

			p_ptr->redraw |= (PR_MISC);
		}

		else if ((query2 == 'm') || (query2 == 'M'))
		{
			/* don't want a random history */
			
			/* Clear the previous history strings */
			p_ptr->history[0] = '\0';

			/* Display the player */
			display_player(0);

			for (i = 1; i <= 3; i++)
			{
				// clear line
				line[0] = '\0';
								
				Term_gotoxy(1, 15+i);
				
				/* Prompt for a new history */
				if (askfor_aux(line, sizeof(line)))
				{

					/* Get the textual history */
					my_strcat(p_ptr->history, line, sizeof(p_ptr->history));

					/* Add a space */
					my_strcat(p_ptr->history, "\n", sizeof(p_ptr->history));
					
					p_ptr->redraw |= (PR_MISC);

					/* Display the player */
					display_player(0);
				}
				else
				{
					return (FALSE);
				}
			}

			// confirm the choices
			if (!get_history()) return (FALSE);

			loopagain = FALSE;
		}
		
		else if (query2 == ESCAPE)  return (FALSE);

		else if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0)) quit (NULL);

	}

	return (TRUE);
}

/*
 * Get the Sex.
 */
static bool get_sex(void)
{
	char query2;
	int loopagain = TRUE;
	
	// Set the default sex info to female
	if (p_ptr->psex == SEX_UNDEFINED)
	{
		p_ptr->psex = SEX_FEMALE;
		sp_ptr = &sex_info[SEX_FEMALE];
	}
	
	while (loopagain == TRUE)
	{
		/* Display the player */
		display_player(0);

		// Highlight the relevant feature
		c_put_str(TERM_YELLOW, sp_ptr->title, 3, 8);
		
		/* Prompt */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"Enter accept sex");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
					"Space change sex");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "Enter");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "Space");
		
		/* Move the cursor */
		Term_gotoxy(0, INSTRUCT_ROW + 1);
				
		/* Query */
		query2 = inkey();
		
		if ((query2 == '\r') || (query2 == '\n'))
		{
			/* got a sex*/
			loopagain = FALSE;
			
			p_ptr->redraw |= (PR_MISC);
		}
		
		else if (query2 == ESCAPE)  return (FALSE);
		
		else if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0)) quit (NULL);

		else
		{
			if (p_ptr->psex == SEX_FEMALE)	p_ptr->psex = SEX_MALE;
			else							p_ptr->psex = SEX_FEMALE;
			
			sp_ptr = &sex_info[p_ptr->psex];
		}
	}
	
	return (TRUE);
}



/*
 * Computes character's age, height, and weight
 */
static void get_ahw_aux(void)
{
	/* Calculate the age */
	p_ptr->age = rand_range(rp_ptr->b_age, rp_ptr->m_age);

	/* Calculate the height/weight for males */
	if (p_ptr->psex == SEX_MALE)
	{
		p_ptr->ht = Rand_normal(rp_ptr->m_b_ht, rp_ptr->m_m_ht);
		p_ptr->wt = Rand_normal(rp_ptr->m_b_wt, rp_ptr->m_m_wt);
	}

	/* Calculate the height/weight for females */
	else if (p_ptr->psex == SEX_FEMALE)
	{
		p_ptr->ht = Rand_normal(rp_ptr->f_b_ht, rp_ptr->f_m_ht);
		p_ptr->wt = Rand_normal(rp_ptr->f_b_wt, rp_ptr->f_m_wt);
	}
}

/*
 * Get the Age, Height and Weight.
 */
static bool get_ahw(void)
{
	char prompt[50];
	char line[70];
	char query2;
	int loopagain = TRUE;

	// hack to see whether we are opening an old player file
	bool roll_ahw = !p_ptr->age;
	
	//put_str("(a)ccept age/height/weight, (r)eroll, (m)anually enter ", 0, 0);
	
	while (loopagain == TRUE)
	{
		if (roll_ahw)
		{
			/*get the random age/height/weight, display for approval. */
			get_ahw_aux();
		}
		else
		{
			roll_ahw = TRUE;
		}

		/* Display the player */
		display_player(0);

		// Highlight relevant info
		display_player_xtra_info(1);

		/* Prompt */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"Enter accept age/height/weight");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
					"Space reroll");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 3, -1, TERM_SLATE,
					"    m manually enter");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "Enter");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "Space");
		Term_putstr(QUESTION_COL + 4, INSTRUCT_ROW + 3, - 1, TERM_L_WHITE, "m");
		
		/* Move the cursor */
		Term_gotoxy(0, INSTRUCT_ROW + 1);

		/* Query */
		query2 = inkey();

		if ((query2 == '\r') || (query2 == '\n'))
		{
			/* got ahw*/
			loopagain = FALSE;

			p_ptr->redraw |= (PR_MISC);
		}

		else if ((query2 == 'm') || (query2 == 'M'))
		{
			/* don't want random stats */
			int age = 0;
			int height = 0;
			int weight = 0;
			int age_l, age_h, height_l, height_h, weight_l, weight_h;

			age_l = 10;
			age_h = 4865;
		
			if (p_ptr->psex == SEX_MALE)
			{
				height_l = rp_ptr->m_b_ht - 5 * (rp_ptr->m_m_ht);
				height_h = rp_ptr->m_b_ht + 5 * (rp_ptr->m_m_ht);
				weight_l = rp_ptr->m_b_wt / 3;
				weight_h = rp_ptr->m_b_wt * 2;
			}
			else
			{
				height_l = rp_ptr->f_b_ht - 5 * (rp_ptr->f_m_ht);
				height_h = rp_ptr->f_b_ht + 5 * (rp_ptr->f_m_ht);
				weight_l = rp_ptr->f_b_wt / 3;
				weight_h = rp_ptr->f_b_wt * 2;
			}
			
			// clear line
			line[0] = '\0';
			
			while ((age < age_l) || (age > age_h))
			{
				sprintf(prompt, "Enter age (%d-%d): ", age_l, age_h);
				if (!term_get_string(prompt, line, sizeof(line))) return (FALSE);
				age = atoi(line);
				p_ptr->age = age;
			}
			
			/* Display the player */
			p_ptr->redraw |= (PR_MISC);
			display_player(0);
				
			// clear line
			line[0] = '\0';

			while ((height < height_l) || (height > height_h))
			{
				sprintf(prompt, "Enter height in inches (%d-%d): ", height_l, height_h);
				if (!term_get_string(prompt, line, sizeof(line))) return (FALSE);
				height = atoi(line);
				p_ptr->ht = height;
			}
			
			/* Display the player */
			p_ptr->redraw |= (PR_MISC);
			display_player(0);
			
			// clear line
			line[0] = '\0';

			while ((weight < weight_l) || (weight > weight_h))
			{
				sprintf(prompt, "Enter weight in pounds (%d-%d): ", weight_l, weight_h);
				if (!term_get_string(prompt, line, sizeof(line))) return (FALSE);
				weight = atoi(line);
				p_ptr->wt = weight;
			}
			
			/* Display the player */
			p_ptr->redraw |= (PR_MISC);
			display_player(0);

			// confirm the choices
			if (!get_ahw()) return (FALSE);

			loopagain = FALSE;
		}

		else if (query2 == ESCAPE)  return (FALSE);

		else if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0)) quit (NULL);
	}
	
	return (TRUE);
}


/*
 * Clear all the global "character" data
 */
static void player_wipe(void)
{
	int i;
	char history[250];
	int stat[A_MAX];

	/* Backup the player choices */
	// Initialized to soothe compilation warnings
	byte psex = 0;
	byte prace = 0;
	byte phouse = 0;
	int age = 0;
	int height = 0;
	int weight = 0;
		
	// only save the old information if there was a character loaded
	if (character_loaded_dead)
	{
		/* Backup the player choices */
		psex = p_ptr->psex;
		prace = p_ptr->prace;
		phouse = p_ptr->phouse;
		age = p_ptr->age;
		height = p_ptr->ht;
		weight = p_ptr->wt;
		sprintf(history, "%s", p_ptr->history);
        
		for (i = 0; i < A_MAX; i++)
		{
			if (!(p_ptr->noscore & 0x0008))
                stat[i] = p_ptr->stat_base[i] - (rp_ptr->r_adj[i] + hp_ptr->h_adj[i]);
            else
                stat[i] = 0;
		}
	}

	/* Wipe the player */
	(void)WIPE(p_ptr, player_type);

	// only save the old information if there was a character loaded
	if (character_loaded_dead)
	{
		/* Restore the choices */
		p_ptr->psex = psex;
		p_ptr->prace = prace;
		p_ptr->phouse = phouse;
		p_ptr->game_type = 0;
		p_ptr->age = age;
		p_ptr->ht = height;
		p_ptr->wt = weight;
		sprintf(p_ptr->history, "%s", history);
		for (i = 0; i < A_MAX; i++)
		{
			p_ptr->stat_base[i] = stat[i];
		}
	}
	else
	{
		/* Reset */
		p_ptr->psex = SEX_UNDEFINED;
		p_ptr->prace = 0;
		p_ptr->phouse = 0;
		p_ptr->game_type = 0;
		p_ptr->age = 0;
		p_ptr->ht = 0;
		p_ptr->wt = 0;
		p_ptr->history[0] = '\0';
		for (i = 0; i < A_MAX; i++)
		{
			p_ptr->stat_base[i] = 0;
		}
	}
	

	/* Clear the inventory */
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		object_wipe(&inventory[i]);
	}

	/* Start with no artefacts made yet */
	/* and clear the slots for in-game randarts */
	for (i = 0; i < z_info->art_max; i++)
	{
		artefact_type *a_ptr = &a_info[i];

		a_ptr->cur_num = 0;
		a_ptr->found_num = 0;
	}

	/*re-set the object_level*/
	object_level = 0;

	/* Reset the "objects" */
	for (i = 1; i < z_info->k_max; i++)
	{
		object_kind *k_ptr = &k_info[i];

		/* Reset "tried" */
		k_ptr->tried = FALSE;

		/* Reset "aware" */
		k_ptr->aware = FALSE;
	}


	/* Reset the "monsters" */
	for (i = 1; i < z_info->r_max; i++)
	{
		monster_race *r_ptr = &r_info[i];
		monster_lore *l_ptr = &l_list[i];

		/* Hack -- Reset the counter */
		r_ptr->cur_num = 0;

		/* Hack -- Reset the max counter */
		r_ptr->max_num = 100;

		/* Hack -- Reset the max counter */
		if (r_ptr->flags1 & (RF1_UNIQUE)) r_ptr->max_num = 1;

		/* Clear player sights/kills */
		l_ptr->psights = 0;
		l_ptr->pkills = 0;
	}

	/*No current player ghosts*/
	bones_selector = 0;

	// give the player the most food possible without a message showing
	p_ptr->food = PY_FOOD_FULL - 1;
	
	// reset the stair info
	p_ptr->stairs_taken = 0;
	p_ptr->staircasiness = 0;

	// reset the forge info
	p_ptr->forge_drought = 5000;
	p_ptr->forge_count = 0;

	/*re-set the thefts counter*/
	recent_failed_thefts = 0;

	/*re-set the altered inventory counter*/
	allow_altered_inventory = 0;

	// reset some unique flags
	p_ptr->unique_forge_made = FALSE;
	p_ptr->unique_forge_seen = FALSE;
	for (i = 0; i < MAX_GREATER_VAULTS; i++)
	{
		p_ptr->greater_vaults[i] = 0;
	}
	
}



/*
 * Init players with some belongings
 *
 * Having an item identifies it and makes the player "aware" of its purpose.
 */
static void player_outfit(void)
{
	int i, slot, inven_slot;
	const start_item *e_ptr;
	object_type *i_ptr;
	object_type object_type_body;
	object_type *o_ptr;
	
	time_t c;       // time variables
	struct tm *tp;  //

	/* Hack -- Give the player his equipment */
	for (i = 0; i < MAX_START_ITEMS; i++)
	{
		/* Access the item */
		e_ptr = &(rp_ptr->start_items[i]);

		/* Get local object */
		i_ptr = &object_type_body;

		/* Hack	-- Give the player an object */
		if (e_ptr->tval > 0)
		{
			/* Get the object_kind */
			s16b k_idx = lookup_kind(e_ptr->tval, e_ptr->sval);

			/* Valid item? */
			if (!k_idx) continue;

			/* Prepare the item */
			object_prep(i_ptr, k_idx);
			i_ptr->number = (byte)rand_range(e_ptr->min, e_ptr->max);

			//object_aware(i_ptr);
			//object_known(i_ptr);
		}

		/* Check the slot */
		slot = wield_slot(i_ptr);
		
		/* give light sources a duration */
		if (slot == INVEN_LITE)
		{
			i_ptr->timeout = 2000;
		}

		/*put it in the inventory*/
		inven_slot = inven_carry(i_ptr);

		/*if player can wield an item, do so*/
		if (slot >= INVEN_WIELD)
		{
			/* Get the wield slot */
			o_ptr = &inventory[slot];

			/* Wear the new stuff */
			object_copy(o_ptr, i_ptr);

			/* Modify quantity */
			o_ptr->number = 1;

			/* Decrease the item */
			inven_item_increase(inven_slot, -1);
			inven_item_optimize(inven_slot);

			/* Increment the equip counter by hand */
			p_ptr->equip_cnt++;
		}

		/*Bugfix:  So we don't get duplicate objects*/
		object_wipe (i_ptr);

	}

	// Christmas presents:
	
	/* Make sure it is Dec 20-31 */
	c = time((time_t *)0);
	tp = localtime(&c);
	if ((tp->tm_mon == 11) && (tp->tm_mday >= 20) && (tp->tm_mday <= 31))
	{
		/* Get local object */
		i_ptr = &object_type_body;
		
		/* Get the object_kind */
		s16b k_idx = lookup_kind(TV_CHEST, SV_CHEST_PRESENT);
		
		/* Prepare the item */
		object_prep(i_ptr, k_idx);
		i_ptr->number = 1;
		i_ptr->pval = -20;
		
		//object_aware(i_ptr);
		//object_known(i_ptr);
		
		/*put it in the inventory*/
		inven_slot = inven_carry(i_ptr);
	}


	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Recalculate mana */
	p_ptr->update |= (PU_MANA);

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

	p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);;
}

/*
 * Clear the previous question
 */
static void clear_question(void)
{
	int i;

	for (i = QUESTION_ROW; i < TABLE_ROW; i++)
	{
		/* Clear line, position cursor */
		Term_erase(0, i, 255);
	}
}

/*
 * Generic "get choice from menu" function
 */
static int get_player_choice(birth_menu *choices, int num, int def, int col, int wid, void (*hook)(birth_menu))
{
	int top = 0, next;
	int i, dir;
	char c;
	char buf[300];
	bool done = FALSE;
	int hgt;
	byte attr;
	int cur = (def) ? def : 0; 

	/* Autoselect if able */
	//if (num == 1) done = TRUE;

	/* Clear */
	for (i = TABLE_ROW; i < DESCRIPTION_ROW + 4; i++)
	{
		/* Clear */
		Term_erase(col, i, Term->wid - wid);
	}

	/* Choose */
	while (TRUE)
	{
		hgt = Term->hgt - TABLE_ROW - 1;

		/* Redraw the list */
		for (i = 0; ((i + top < num) && (i <= hgt)); i++)
		{
			if (i + top < 26)
			{
				strnfmt(buf, sizeof(buf), "%c) %s", I2A(i + top), choices[i + top].name);
			}
			else
			{
				/* ToDo: Fix the ASCII dependency */
				strnfmt(buf, sizeof(buf), "%c) %s", 'A' + (i + top - 26), choices[i + top].name);
			}

			/* Clear */
			Term_erase(col, i + TABLE_ROW, wid);

			/* Display */
			if (i == (cur - top))
			{
				/* Highlight the current selection */
				if (choices[i + top].ghost) attr = TERM_BLUE;
				else attr = TERM_L_BLUE;
			}
			else
			{
				if (choices[i + top].ghost) attr = TERM_SLATE;
				else attr = TERM_WHITE;
			}

			Term_putstr(col, i + TABLE_ROW, wid, attr, buf);
		}

		Term_erase(0, DESCRIPTION_ROW + 0, 255);
		Term_erase(0, DESCRIPTION_ROW + 1, 255);
		Term_erase(0, DESCRIPTION_ROW + 2, 255);
		Term_erase(0, DESCRIPTION_ROW + 3, 255);
		Term_erase(0, DESCRIPTION_ROW + 4, 255);
		
		if (choices[cur+top].text != NULL)
		{

			/* Indent output by 2 character, and wrap at column 70 */
			text_out_wrap = 70;
			text_out_indent = 2;

			/* History */
			Term_gotoxy(text_out_indent, DESCRIPTION_ROW);
			text_out_to_screen(TERM_L_WHITE, choices[cur+top].text);

			/* Reset text_out() vars */
			text_out_wrap = 0;
			text_out_indent = 0;
		}
		
		else
		{
			/* Extra info */
//			Term_putstr(QUESTION_COL, DESCRIPTION_ROW, -1, TERM_L_WHITE,
//						"Your sex has no gameplay effect.");
			
		}

		if (done) return (cur);

		/* Display auxiliary information if any is available. */
		if (hook) hook(choices[cur]);


		/* Move the cursor */
		put_str("", TABLE_ROW + cur - top, col);

		hide_cursor = TRUE;
		c = inkey();
		hide_cursor = FALSE;

		/* Exit the game */
		if ((c == 'Q') || (c == 'q'))	quit(NULL);

		/* Hack - go back */
		if ((c == ESCAPE)|| (c == '4')) return (INVALID_CHOICE);

		/* Make a choice */
		if ((c == '\n') || (c == '\r') || (c == '6')) return (cur);

		/* Random choice */
		if (c == '*')
		{
			/* Ensure legal choice */
			do { cur = rand_int(num); } while (choices[cur].ghost);

			/* Done */
			done = TRUE;
		}

		/* Alphabetic choice */
		else if (isalpha(c))
		{

			/* Options */
			if ((c == 'O') || (c == 'o'))
			{
				do_cmd_options();
			}

			else
			{
				int choice;

				if (islower(c)) choice = A2I(c);
				else choice = c - 'A' + 26;
				
				/* Validate input */
				if ((choice > -1) && (choice < num) && !(choices[choice].ghost))
				{
					cur = choice;
					
					/* Done */
					done = TRUE;
				}
				else if (choices[choice].ghost)
				{
					bell("Your race cannot choose that house.");
				}
				else
				{
					bell("Illegal response to question!");
				}
			}
		}

		/* Move */
		else if (isdigit(c))
		{
			/* Get a direction from the key */
			dir = target_dir(c);

			/* Going up? */
			if (dir == 8)
			{
				next = -1;
				for (i = 0; i < cur; i++)
				{
					if (!(choices[i].ghost))
					{
						next = i;
					}
				}
				
				/* Move selection */
				if (next != -1) cur = next;
				/* if (cur != 0) cur--; */

				/* Scroll up */
				if ((top > 0) && ((cur - top) < 4))	top--;		
			}

			/* Going down? */
			if (dir == 2)
			{
				next = -1;
				for (i = num - 1; i > cur; i--)
				{
					if (!(choices[i].ghost))
					{
						next = i;
					}
				}
				
				/* Move selection */
				if (next != -1) cur = next;
				/* if (cur != (num - 1)) cur++; */

				/* Scroll down */
				if ((top + hgt < (num - 1)) && ((top + hgt - cur) < 4)) top++;
			}
		}

		/* Invalid input */
		else bell("Illegal response to question!");

		/* If choice is off screen, move it to the top */
		if ((cur < top) || (cur > top + hgt)) top = cur;
	}

	return (INVALID_CHOICE);
}

static void print_rh_flags(int race, int house, int col, int row)
{
	int flags = 0;
	
	byte attr_affinity = TERM_GREEN;
	byte attr_mastery = TERM_L_GREEN;
	byte attr_penalty = TERM_RED;
	
	if ((p_info[race].flags & RHF_BLADE_PROFICIENCY) && (c_info[house].flags & RHF_BLADE_PROFICIENCY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "blade mastery       ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_BLADE_PROFICIENCY) || (c_info[house].flags & RHF_BLADE_PROFICIENCY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "blade proficiency      ");
		flags++;
	}

	if ((p_info[race].flags & RHF_AXE_PROFICIENCY) && (c_info[house].flags & RHF_AXE_PROFICIENCY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "axe mastery         ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_AXE_PROFICIENCY) || (c_info[house].flags & RHF_AXE_PROFICIENCY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "axe proficiency        ");
		flags++;
	}

	if ((p_info[race].flags & RHF_MEL_AFFINITY) && (c_info[house].flags & RHF_MEL_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "melee mastery     ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_MEL_AFFINITY) || (c_info[house].flags & RHF_MEL_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "melee affinity    ");
		flags++;
	}
	if ((p_info[race].flags & RHF_MEL_PENALTY) || (c_info[house].flags & RHF_MEL_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "melee penalty     ");
		flags++;
	}
	
	if ((p_info[race].flags & RHF_ARC_AFFINITY) && (c_info[house].flags & RHF_ARC_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "archery mastery     ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_ARC_AFFINITY) || (c_info[house].flags & RHF_ARC_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "archery affinity    ");
		flags++;
	}
	if ((p_info[race].flags & RHF_ARC_PENALTY) || (c_info[house].flags & RHF_ARC_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "archery penalty     ");
		flags++;
	}

	if ((p_info[race].flags & RHF_EVN_AFFINITY) && (c_info[house].flags & RHF_EVN_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "evasion mastery     ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_EVN_AFFINITY) || (c_info[house].flags & RHF_EVN_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "evasion affinity    ");
		flags++;
	}
	if ((p_info[race].flags & RHF_EVN_PENALTY) || (c_info[house].flags & RHF_EVN_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "evasion penalty     ");
		flags++;
	}

	if ((p_info[race].flags & RHF_STL_AFFINITY) && (c_info[house].flags & RHF_STL_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "stealth mastery     ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_STL_AFFINITY) || (c_info[house].flags & RHF_STL_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "stealth affinity    ");
		flags++;
	}
	if ((p_info[race].flags & RHF_STL_PENALTY) || (c_info[house].flags & RHF_STL_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "stealth penalty     ");
		flags++;
	}

	if ((p_info[race].flags & RHF_PER_AFFINITY) && (c_info[house].flags & RHF_PER_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "perception mastery  ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_PER_AFFINITY) || (c_info[house].flags & RHF_PER_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "perception affinity ");
		flags++;
	}
	if ((p_info[race].flags & RHF_PER_PENALTY) || (c_info[house].flags & RHF_PER_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "perception penalty  ");
		flags++;
	}

	if ((p_info[race].flags & RHF_WIL_AFFINITY) && (c_info[house].flags & RHF_WIL_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "will mastery        ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_WIL_AFFINITY) || (c_info[house].flags & RHF_WIL_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "will affinity       ");
		flags++;
	}
	if ((p_info[race].flags & RHF_WIL_PENALTY) || (c_info[house].flags & RHF_WIL_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "will penalty        ");
		flags++;
	}

	if ((p_info[race].flags & RHF_SMT_AFFINITY) && (c_info[house].flags & RHF_SMT_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "smithing mastery    ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_SMT_AFFINITY) || (c_info[house].flags & RHF_SMT_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "smithing affinity   ");
		flags++;
	}
	if ((p_info[race].flags & RHF_SMT_PENALTY) || (c_info[house].flags & RHF_SMT_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "smithing penalty    ");
		flags++;
	}

	if ((p_info[race].flags & RHF_SNG_AFFINITY) && (c_info[house].flags & RHF_SNG_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_mastery, "song mastery        ");
		flags++;
	}
	else if ((p_info[race].flags & RHF_SNG_AFFINITY) || (c_info[house].flags & RHF_SNG_AFFINITY))
	{
		Term_putstr(col, row + flags, -1, attr_affinity, "song affinity       ");
		flags++;
	}
	if ((p_info[race].flags & RHF_SNG_PENALTY) || (c_info[house].flags & RHF_SNG_PENALTY))
	{
		Term_putstr(col, row + flags, -1, attr_penalty, "song penalty        ");
		flags++;
	}


}

/*
 * Display additional information about each race during the selection.
 */
static void race_aux_hook(birth_menu r_str)
{
	int race, i, adj;
	char s[50];
	byte attr;

	/* Extract the proper race index from the string. */
	for (race = 0; race < z_info->p_max; race++)
	{
		if (!strcmp(r_str.name, p_name + p_info[race].name)) break;
	}

	if (race == z_info->p_max) return;

	/* Display the stats */
	for (i = 0; i < A_MAX; i++)
	{
		/*dump the stats*/
		strnfmt(s, sizeof(s), "%s", stat_names[i]);
		Term_putstr(RACE_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);
		
		adj = p_info[race].r_adj[i];
		strnfmt(s, sizeof(s), "%+d", adj);
		
		if (adj < 0)		attr = TERM_RED;
		else if (adj == 0)	attr = TERM_L_DARK;
		else if (adj == 1)	attr = TERM_GREEN;
		else if (adj == 2)	attr = TERM_L_GREEN;
		else				attr = TERM_L_BLUE;
		
		Term_putstr(RACE_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
	}

	/* Display the race flags */
	
	Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE, "                    ");
	Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE, "                    ");
	Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE, "                    ");
	Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE, "                    ");
	
	print_rh_flags(race, 0, RACE_AUX_COL, TABLE_ROW + A_MAX + 1);
	
}


/*
 * Player race
 */
static bool get_player_race(void)
{
	int i;
	birth_menu *races;
	int race;

	C_MAKE(races, z_info->p_max, birth_menu);

	/* Extra info */
	//Term_putstr(QUESTION_COL, QUESTION_ROW, -1, TERM_YELLOW,
	//	"Your race affects your ability scores and other factors.");

	/* Tabulate races */
	for (i = 0; i < z_info->p_max; i++)
	{
		races[i].name = p_name + p_info[i].name;
		races[i].ghost = FALSE;
		races[i].text = p_text + p_info[i].text;
	}

	race = get_player_choice(races, z_info->p_max, p_ptr->prace, RACE_COL, 15, race_aux_hook);

	/* No selection? */
	if (race == INVALID_CHOICE)
	{
		return (FALSE);
	}

	// if different race to last time, then wipe the history, age, height, weight
	if (race != p_ptr->prace)
	{
		p_ptr->psex = SEX_UNDEFINED;
		p_ptr->history[0] = '\0';
		p_ptr->age = 0;
		p_ptr->ht = 0;
		p_ptr->wt = 0;
		for (i = 0; i < A_MAX; i++)
		{
			p_ptr->stat_base[i] = 0;
		}
	}
	p_ptr->prace = race;

	/* Save the race pointer */
	rp_ptr = &p_info[p_ptr->prace];

	FREE(races);

	/* Success */
	return (TRUE);
}

/*
 * Display additional information about each house during the selection.
 */
static void house_aux_hook(birth_menu c_str)
{
	int house_idx, i, adj;
	char s[128];
	byte attr;

	/* Extract the proper house index from the string. */
	for (house_idx = 0; house_idx < z_info->c_max; house_idx++)
	{
		if (!strcmp(c_str.name, c_name + c_info[house_idx].name)) break;
	}

	if (house_idx == z_info->c_max) return;

	/* Display relevant details. */
	for (i = 0; i < A_MAX; i++)
	{
		/*dump potential total stats*/
		strnfmt(s, sizeof(s), "%s", stat_names[i]);
		Term_putstr(TOTAL_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

		adj = c_info[house_idx].h_adj[i] + rp_ptr->r_adj[i];
		strnfmt(s, sizeof(s), "%+d", adj);
		
		if (adj < 0)		attr = TERM_RED;
		else if (adj == 0)	attr = TERM_L_DARK;
		else if (adj == 1)	attr = TERM_GREEN;
		else if (adj == 2)	attr = TERM_L_GREEN;
		else				attr = TERM_L_BLUE;
		
		Term_putstr(TOTAL_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
	}

	/* Display the race flags */
	
	Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE, "                    ");
	Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE, "                    ");
	Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE, "                    ");
	Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE, "                    ");
	Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 5, -1, TERM_WHITE, "                    ");
	
	print_rh_flags(p_ptr->prace,house_idx,TOTAL_AUX_COL,TABLE_ROW + A_MAX + 1);
	
}

/*
 * Player house
 */
static bool get_player_house(void)
{
	int i;
	int house = 0;
	int house_choice;
	int old_house_choice = 0;
	
	birth_menu *houses;

	// select 'houseless' automatically if there are no available houses
	if ((rp_ptr->choice & 1))
	{
		p_ptr->phouse = 0;
		hp_ptr = &c_info[p_ptr->phouse];
		return (TRUE);
	}
	
	C_MAKE(houses, z_info->c_max, birth_menu);

	/* Extra info */
	//Term_putstr(QUESTION_COL, QUESTION_ROW, -1, TERM_YELLOW,
	//	"Your house modifies your race bonuses.");

	/* Tabulate houses */
	for (i = 0; i < z_info->c_max; i++)
	{
		/* Analyze */
		if (rp_ptr->choice & (1L << i))
		{
			houses[house].ghost = FALSE;
			houses[house].name = c_name + c_info[i].name;
			houses[house].text = c_text + c_info[i].text;
			if (p_ptr->phouse == i) old_house_choice = house;
			house++;
		}
	}

	house_choice = get_player_choice(houses, house, old_house_choice, CLASS_COL, 22, house_aux_hook);

	/* No selection? */
	if (house_choice == INVALID_CHOICE)
	{
		return (FALSE);
	}

	/* Get house from choice number */
	house = 0;
	for (i = 0; i < z_info->c_max; i++)
	{
		if (rp_ptr->choice & (1L << i))
		{
			if (house_choice == house) 
			{
				// if different house to last time, then wipe the history, age, height, weight
				if (i != p_ptr->phouse)
				{
					int j;
					
					p_ptr->psex = SEX_UNDEFINED;
					p_ptr->history[0] = '\0';
					p_ptr->age = 0;
					p_ptr->ht = 0;
					p_ptr->wt = 0;
					for (j = 0; j < A_MAX; j++)
					{
						p_ptr->stat_base[j] = 0;
					}
				}
				p_ptr->phouse = i;
			}
			house++;
		}
	}

	/* Set house */
	hp_ptr = &c_info[p_ptr->phouse];

	FREE(houses);

	return (TRUE);
}


/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a sex, race, and house, and
 * modify options (including the birth options).
 */
static bool player_birth_aux_1(void)
{
	int i, j;

	int phase = 1;

	/*** Instructions ***/

	/* Clear screen */
	Term_clear();

	/* Display some helpful information */
	Term_putstr(QUESTION_COL, HEADER_ROW, -1, TERM_L_BLUE,
	            "Character Creation:");
				
	Term_putstr(QUESTION_COL, INSTRUCT_ROW, -1, TERM_SLATE,
	            "Arrow keys navigate the menu    Enter select the current menu item");
	Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
	            "         * random menu item       ESC restart the character");
	Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
	            "         = game options             q quit");

	/* Hack - highlight the key names */
	Term_putstr(QUESTION_COL + 0, INSTRUCT_ROW, - 1, TERM_L_WHITE, "Arrow keys");
	Term_putstr(QUESTION_COL + 32, INSTRUCT_ROW, - 1, TERM_L_WHITE, "Enter");
	Term_putstr(QUESTION_COL + 9, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "*");
	Term_putstr(QUESTION_COL + 34, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "ESC");
	Term_putstr(QUESTION_COL + 9, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "O");
	Term_putstr(QUESTION_COL + 36, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "q");

	// Set blank sex info for new characters
	// hack to see whether we are opening an old player file
	if (!p_ptr->age)
	{
		p_ptr->psex = SEX_UNDEFINED;
		sp_ptr = &sex_info[SEX_UNDEFINED];
	}
	// Or default to previous sex for old characters
	else
	{
		sp_ptr = &sex_info[p_ptr->psex];
	}
	
	while (phase <= 2)
	{
		clear_question();

		if (phase == 1)
		{
			/* Choose the player's race */
			if (!get_player_race())
			{
				continue;
			}

			/* Clean up */
			clear_question();

			phase++;
		}


		if (phase == 2)
		{
			/* Choose the player's house */
			if (!get_player_house())
			{
				phase--;
				continue;
			}

			/* Clean up */
			clear_question();

			phase++;
		}
	
	}

	/* Clear the base values of the skills */
	for (i = 0; i < A_MAX; i++) p_ptr->skill_base[i] = 0;

	/* Clear the abilities */
	for (i = 0; i < S_MAX; i++)
	{
		for (j = 0; j < ABILITIES_MAX; j++)
		{
			p_ptr->innate_ability[i][j] = FALSE;
			p_ptr->active_ability[i][j] = FALSE;
		}
	}
	
	/* Set adult options from birth options */
	for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
	{
		op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];
	}

	/* Reset score options from cheat options */
	for (i = OPT_CHEAT; i < OPT_ADULT; i++)
	{
		op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];
	}

	// Set a default value for hitpoint warning / delay factor unless this is an old game file
	if (strlen(op_ptr->full_name) == 0)
	{
		op_ptr->hitpoint_warn = 3;
		op_ptr->delay_factor = 5;
	}
	
	/* reset squelch bits */

	for (i = 0; i < z_info->k_max; i++)
	{
		k_info[i].squelch = SQUELCH_NEVER;
	}
	/*Clear the squelch bytes*/
	for (i = 0; i < SQUELCH_BYTES; i++)
	{
		squelch_level[i] = SQUELCH_NONE;
	}
	/* Clear the special item squelching flags */
	for (i = 0; i < z_info->e_max; i++)
	{
		e_info[i].aware = FALSE;
		e_info[i].squelch = FALSE;
	}

	/* Clear */
	Term_clear();

	/* Done */
	return (TRUE);
}


/*
 * Initial stat costs.
 */
static const int birth_stat_costs[11] = { -4, -3, -2, -1, 0, 1, 3, 6, 10, 15, 21};

#define MAX_COST	13

/*
 * Helper function for 'player_birth()'.
 *
 * This function handles "point-based" character creation.
 *
 * The player selects, for each stat, a value from 6 to 18 (inclusive),
 * each costing a certain amount of points (as above), from a pool of 30
 * available points, to which race/house modifiers are then applied.
 *
 * Each unused point is converted into 100 experience points.
 */
static bool player_birth_aux_2(void)
{
	int i;

	int row = 2;
	int col = 42;

	int stat = 0;

	int stats[A_MAX];

	int cost;

	char ch;

	char buf[80];


	/* Initialize stats */
	for (i = 0; i < A_MAX; i++)
	{
		/* Initial stats */
		stats[i] = p_ptr->stat_base[i];
	}


	/* Determine experience and things */
	get_extra();

	/* Interact */
	while (1)
	{
		/* Reset cost */
		cost = 0;

		/* Process stats */
		for (i = 0; i < A_MAX; i++)
		{
			/* Obtain a "bonus" for "race" */
			int bonus = rp_ptr->r_adj[i] + hp_ptr->h_adj[i];

			/* Apply the racial bonuses */
			p_ptr->stat_base[i] = stats[i] + bonus;
			p_ptr->stat_drain[i] = 0;

			/* Total cost */
			cost += birth_stat_costs[stats[i] + 4];
		}

		/* Restrict cost */
		if (cost > MAX_COST)
		{
			/* Warning */
			bell("Excessive stats!");

			/* Reduce stat */
			stats[stat]--;

			/* Recompute costs */
			continue;
		}

		p_ptr->new_exp = p_ptr->exp = PY_START_EXP;

		/* Calculate the bonuses and hitpoints */
		p_ptr->update |= (PU_BONUS | PU_HP);

		/* Update stuff */
		update_stuff();

		/* Fully healed */
		p_ptr->chp = p_ptr->mhp;

		/* Fully rested */
		calc_voice();
		p_ptr->csp = p_ptr->msp;

		/* Display the player */
		display_player(0);

		/* Display the costs header */
		c_put_str(TERM_WHITE, "Points Left:", 0, col + 21);
		strnfmt(buf, sizeof(buf), "%2d", MAX_COST - cost);
		c_put_str(TERM_L_GREEN, buf, 0, col + 34);
		
		/* Display the costs */
		for (i = 0; i < A_MAX; i++)
		{
			byte attr = (i == stat) ? TERM_L_BLUE : TERM_L_WHITE;
			
			/* Display cost */
			strnfmt(buf, sizeof(buf), "%4d", birth_stat_costs[stats[i] + 4]);
			c_put_str(attr, buf, row + i, col + 32);
		}
		
		/* Prompt */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"Arrow keys allocate points to stats");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
					"     Enter accept current values");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "Arrow keys");
		Term_putstr(QUESTION_COL + 5, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "Enter");
		
		/* Get key */
		hide_cursor = TRUE;
		ch = inkey();
		hide_cursor = FALSE;

		/* Quit */
		if ((ch == 'Q') || (ch == 'q')) quit(NULL);

		/* Start over */
		if (ch == ESCAPE) return (FALSE);

		/* Done */
		if ((ch == '\r') || (ch == '\n')) break;

		/* Prev stat */
		if (ch == '8')
		{
			stat = (stat + A_MAX - 1) % A_MAX;
		}

		/* Next stat */
		if (ch == '2')
		{
			stat = (stat + 1) % A_MAX;
		}

		/* Decrease stat */
		if ((ch == '4') && (stats[stat] > 0))
		{
			stats[stat]--;
		}

		/* Increase stat */
		if (ch == '6')
		{
			stats[stat]++;
		}
	}

	/* Done */
	return (TRUE);
}


/*
 * Skill point costs.
 *
 * The nth skill point costs (100*n) experience points
 */
static int skill_cost(int base, int points)
{
	int total_cost = (points + base) * (points + base + 1) / 2;
	int prev_cost = (base) * (base + 1) / 2;
	return ((total_cost - prev_cost) * 100);
}

/*
 * Increase your skills by spending experience points
 */
extern bool gain_skills(void)
{
	int i;

	int row = 7;
	int col = 42;

	int skill = 0;

	int old_base[S_MAX];
	int skill_gain[S_MAX];
	
	int old_new_exp = p_ptr->new_exp;
	int total_cost = 0;
	
	//int old_csp = p_ptr->csp;
	//int old_msp = p_ptr->msp;

	char ch;

	char buf[80];
	
	bool accepted;
	
	int tab = 0;
	
	// hack global variable
	skill_gain_in_progress = TRUE;
	
	/* save the old skills */
	for (i = 0; i < S_MAX; i++) old_base[i] = p_ptr->skill_base[i];

	/* initialise the skill gains */
	for (i = 0; i < S_MAX; i++) skill_gain[i] = 0;

	/* Interact */
	while (1)
	{	
		// reset the total cost
		total_cost = 0;
		
		/* Process skills */
		for (i = 0; i < S_MAX; i++)
		{
			/* Total cost */
			total_cost += skill_cost(old_base[i], skill_gain[i]);
		}
		
		// set the new experience pool total
		p_ptr->new_exp = old_new_exp - total_cost;

		/* Restrict cost */
		if (p_ptr->new_exp < 0)
		{
			/* Warning */
			bell("Excessive skills!");

			/* Reduce stat */
			skill_gain[skill]--;

			/* Recompute costs */
			continue;
		}

		/* Calculate the bonuses */
		p_ptr->update |= (PU_BONUS);
		
		/* Set the redraw flag for everything */
		p_ptr->redraw |= (PR_EXP | PR_BASIC);

		/* update the skills */
		for (i = 0; i < S_MAX; i++)
		{
			p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
		}
		
		/* Update stuff */
		update_stuff();

		/* Update voice level */
		//if (p_ptr->msp == 0) p_ptr->csp = 0;
		//else                 p_ptr->csp = (old_csp * p_ptr->msp) / old_msp;

		/* Display the player */
		display_player(0);

		/* Display the costs header */
		if (!character_dungeon)
		{
			if (p_ptr->new_exp >= 10000)		tab = 0;
			else if (p_ptr->new_exp >= 1000)	tab = 1;
			else if (p_ptr->new_exp >= 100)		tab = 2;
			else if (p_ptr->new_exp >= 10)		tab = 3;
			else								tab = 4;
			
			strnfmt(buf, sizeof(buf), "%6d", p_ptr->new_exp);
			c_put_str(TERM_L_GREEN, buf, row - 2, col + 30);
			c_put_str(TERM_WHITE, "Points Left:", row - 2, col + 17 + tab);
		}
		
		/* Display the costs */
		for (i = 0; i < S_MAX; i++)
		{
			byte attr = (i == skill) ? TERM_L_BLUE : TERM_L_WHITE;

			/* Display cost */
			strnfmt(buf, sizeof(buf), "%6d", skill_cost(old_base[i], skill_gain[i]));
			c_put_str(attr, buf, row + i, col + 30);
		}
		
		/* Special Prompt? */
		if (character_dungeon)
		{
			Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
						"ESC abort skill increases                  ");
			
			/* Hack - highlight the key names */
			Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "ESC");
		}

		/* Prompt */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"Arrow keys allocate points to skills");
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
					"     Enter accept current values");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "Arrow keys");
		Term_putstr(QUESTION_COL + 5, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "Enter");
		
		/* Get key */
		hide_cursor = TRUE;
		ch = inkey();
		hide_cursor = FALSE;

		/* Quit (only if haven't begun game yet) */
		if (((ch == 'Q') || (ch == 'q')) && (turn == 0)) quit(NULL);

		/* Done */
		if ((ch == '\r') || (ch == '\n'))
		{
			accepted = TRUE;
			break;
		}
	
		/* Abort */
		if (ch == ESCAPE)
		{
			p_ptr->new_exp = old_new_exp;
			for (i=0; i<S_MAX; i++) p_ptr->skill_base[i] = old_base[i];
			//p_ptr->csp = old_csp;
			accepted = FALSE;
			break;
		}
		
		/* Prev skill */
		if (ch == '8')
		{
			skill = (skill + S_MAX - 1) % S_MAX;
		}

		/* Next skill */
		if (ch == '2')
		{
			skill = (skill + 1) % S_MAX;
		}

		/* Decrease skill */
		if ((ch == '4') && (skill_gain[skill] > 0))
		{
			skill_gain[skill]--;
		}

		/* Increase stat */
		if (ch == '6')
		{
			skill_gain[skill]++;
		}
	}

	// reset hack global variable
	skill_gain_in_progress = FALSE;

	/* Calculate the bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Update stuff */
	update_stuff();

	/* Done */
	return (accepted);
}

#define BASE_COLUMN		7
#define STAT_TITLE_ROW	14
#define BASE_STAT_ROW	16


/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static bool player_birth_aux(void)
{
	/* Ask questions */
	if (!player_birth_aux_1()) return (FALSE);

	/* Point-based stats */
	if (!player_birth_aux_2()) return (FALSE);

	/* Point-based skills */
	if (!gain_skills()) return (FALSE);

	/* Choose sex */
	if (!get_sex()) return (FALSE);

	/* Roll for history */
	if (!get_history()) return (FALSE);

	/* Roll for age/height/weight */
	if (!get_ahw()) return (FALSE);

	/* Get a name, prepare savefile */
	if (!get_name()) return (FALSE);

	// Reset the number of artefacts
	p_ptr->artefacts = 0;

	/* Accept */
	return (TRUE);
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
void player_birth()
{
	int i;

	char raw_date[25];
	char clean_date[25];
	char month[4];
	time_t ct = time((time_t*)0);
	
	/* Create a new character */
	while (1)
	{
		/* Wipe the player */
		player_wipe();

		/* Roll up a new character */
		if (player_birth_aux()) break;
	}

	for (i = 0; i < NOTES_LENGTH; i++)
	{
		notes_buffer[i] = '\0';
	}

	/* Get date */
	(void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));
	
	sprintf(month,"%.2s", raw_date + 5);
	atomonth(atoi(month), month);
	
	if (*(raw_date + 7) == '0')		sprintf(clean_date, "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
	else							sprintf(clean_date, "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);
	
	/* Add in "character start" information */
	my_strcat(notes_buffer, format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name), sizeof(notes_buffer));
	my_strcat(notes_buffer, format("Entered Angband on %s\n", clean_date), sizeof(notes_buffer));
	my_strcat(notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));
	
	/* Note player birth in the message recall */
	message_add(" ", MSG_GENERIC);
	message_add("  ", MSG_GENERIC);
	message_add("====================", MSG_GENERIC);
	message_add("  ", MSG_GENERIC);
	message_add(" ", MSG_GENERIC);


	/* Hack -- outfit the player */
	player_outfit();

}
