/* File: cmd4.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/* String used to show a color sample */
#define COLOR_SAMPLE "###"

/*max length of note output*/
#define LINEWRAP	75

/*used for knowledge display*/
#define BROWSER_ROWS			16

/*
 *  Header and footer marker string for pref file dumps
 */
static cptr dump_seperator = "#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#";

typedef struct monster_list_entry monster_list_entry;
/*
 * Structure for building monster "lists"
 */
struct monster_list_entry
{
	s16b r_idx;			/* Monster race index */

	byte amount;
};

typedef struct object_list_entry object_list_entry;
struct object_list_entry
{
    enum { OBJ_NONE, OBJ_NORMAL, OBJ_SPECIAL } type;
    int idx;
    int e_idx;
    int tval, sval;
};



/*
 * Remove old lines from pref files
 */
static void remove_old_dump(cptr orig_file, cptr mark)
{
	FILE *tmp_fff, *orig_fff;

	char tmp_file[1024];
	char buf[1024];
	bool between_marks = FALSE;
	bool changed = FALSE;
	char expected_line[1024];

	/* Open an old dump file in read-only mode */
	orig_fff = my_fopen(orig_file, "r");

	/* If original file does not exist, nothing to do */
	if (!orig_fff) return;

	/* Open a new temporary file */
	tmp_fff = my_fopen_temp(tmp_file, sizeof(tmp_file));

	if (!tmp_fff)
	{
	    msg_format("Failed to create temporary file %s.", tmp_file);
	    msg_print(NULL);
	    return;
	}

	strnfmt(expected_line, sizeof(expected_line),
	        "%s begin %s", dump_seperator, mark);

	/* Loop for every line */
	while (TRUE)
	{
		/* Read a line */
		if (my_fgets(orig_fff, buf, sizeof(buf)))
		{
			/* End of file but no end marker */
			if (between_marks) changed = FALSE;

			break;
		}

		/* Is this line a header/footer? */
		if (strncmp(buf, dump_seperator, strlen(dump_seperator)) == 0)
		{
			/* Found the expected line? */
			if (strcmp(buf, expected_line) == 0)
			{
				if (!between_marks)
				{
					/* Expect the footer next */
					strnfmt(expected_line, sizeof(expected_line),
					        "%s end %s", dump_seperator, mark);

					between_marks = TRUE;

					/* There are some changes */
					changed = TRUE;
				}
				else
				{
					/* Expect a header next - XXX shouldn't happen */
					strnfmt(expected_line, sizeof(expected_line),
					        "%s begin %s", dump_seperator, mark);

					between_marks = FALSE;

					/* Next line */
					continue;
				}
			}
			/* Found a different line */
			else
			{
				/* Expected a footer and got something different? */
				if (between_marks)
				{
					/* Abort */
					changed = FALSE;
					break;
				}
			}
		}

		if (!between_marks)
		{
			/* Copy orginal line */
			fprintf(tmp_fff, "%s\n", buf);
		}
	}

	/* Close files */
	my_fclose(orig_fff);
	my_fclose(tmp_fff);

	/* If there are changes, overwrite the original file with the new one */
	if (changed)
	{
		/* Copy contents of temporary file */
		tmp_fff = my_fopen(tmp_file, "r");
		orig_fff = my_fopen(orig_file, "w");

		while (!my_fgets(tmp_fff, buf, sizeof(buf)))
		{
			fprintf(orig_fff, "%s\n", buf);
		}

		my_fclose(orig_fff);
		my_fclose(tmp_fff);
	}

	/* Kill the temporary file */
	fd_kill(tmp_file);
}


/*
 * Output the header of a pref-file dump
 */
static void pref_header(FILE *fff, cptr mark)
{
	/* Start of dump */
	fprintf(fff, "%s begin %s\n", dump_seperator, mark);

	fprintf(fff, "# *Warning!*  The lines below are an automatic dump.\n");
	fprintf(fff, "# Don't edit them; changes will be deleted and replaced automatically.\n");
}


/*
 * Output the footer of a pref-file dump
 */
static void pref_footer(FILE *fff, cptr mark)
{
	fprintf(fff, "# *Warning!*  The lines above are an automatic dump.\n");
	fprintf(fff, "# Don't edit them; changes will be deleted and replaced automatically.\n");

	/* End of dump */
	fprintf(fff, "%s end %s\n", dump_seperator, mark);
}


/*
 * Hack -- redraw the screen
 *
 * This command performs various low level updates, clears all the "extra"
 * windows, does a total redraw of the main window, and requests all of the
 * interesting updates and redraws that I can think of.
 *
 * This command is also used to "instantiate" the results of the user
 * selecting various things, such as graphics mode, so it must call
 * the "TERM_XTRA_REACT" hook before redrawing the windows.
 */
void do_cmd_redraw(void)
{
	int j;

	term *old = Term;

	/* Low level flush */
	Term_flush();

	/* Reset "inkey()" */
	flush();


	/* Hack -- React to changes */
	Term_xtra(TERM_XTRA_REACT, 0);


	/* Combine and Reorder the pack (later) */
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);

	/* Update stuff */
	p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw everything */
	p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

	/* Window stuff */
	p_ptr->window |= (PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT);

	/* Clear screen */
	Term_clear();
	
	/* Hack -- update */
	handle_stuff();

	/* Redraw every window */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		/* Dead window */
		if (!angband_term[j]) continue;

		/* Activate */
		Term_activate(angband_term[j]);

		/* Redraw */
		Term_redraw();

		/* Refresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}


/*
 * Hack -- character sheet
 */
void do_cmd_character_sheet(void)
{
	char ch;

	int mode = 0;

	/* Save screen */
	screen_save();

	/* Forever */
	while (1)
	{
		/* Display the player */
		display_player(mode);

		/* Prompt */
		Term_putstr(1, 23, -1, TERM_SLATE, "notes   change name   save to a file   abilities   increase skills   ESC");
		Term_putstr(1, 23, -1, TERM_L_WHITE, "n");
		Term_putstr(9, 23, -1, TERM_L_WHITE, "c");
		Term_putstr(23, 23, -1, TERM_L_WHITE, "s");
		Term_putstr(40, 23, -1, TERM_L_WHITE, "a");
		Term_putstr(52, 23, -1, TERM_L_WHITE, "i");
		Term_putstr(70, 23, -1, TERM_L_WHITE, "ESC");
				
		/* Query */
		ch = inkey();

		/* Exit */
		if (ch == ESCAPE) break;
		if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q')) break;

		/* Increase skills */
		if (ch == 'i')
		{
			gain_skills();
		}

		/* Show notes */
		else if ((ch == 'n') || (ch == ' '))
		{
			do_cmd_knowledge_notes();
		}
		
		/* Change name */
		else if (ch == 'c')
		{
			(void) get_name();
		}
		
		/* Abilities */
		else if ((ch == 'a') || (ch == '\t'))
		{
			(void) do_cmd_ability_screen();
		}
		
		/* File dump */
		else if (ch == 's')
		{
			char ftmp[80];

			strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

			if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
			{
				if (ftmp[0] && (ftmp[0] != ' '))
				{
					if (file_character(ftmp, FALSE))
					{
						msg_print("Character dump failed!");
					}
					else
					{
						msg_print("Character dump successful.");
					}
				}
			}
		}

		/* Oops */
		else
		{
			bell("Illegal command for character sheet!");
		}

		/* Flush messages */
		message_flush();
	}

	/* Load screen */
	screen_load();
}

#define	COL_SKILL		 2
#define COL_ABILITY		17
#define COL_DESCRIPTION	42


int ability_index(int skilltype, int abilitynum)
{
	int i;
	ability_type *b_ptr;
	
	for (i = 0; i < z_info->b_max; i++)
	{
		b_ptr = &b_info[i];
		
		/* Skip non-entries */
		if (!b_ptr->name) continue;
		
		/* Skip entries for the wrong skill type */
		if (b_ptr->skilltype != skilltype) continue;
		
		/* Stop if you get the correct ability number */
		if (b_ptr->abilitynum == abilitynum) return (i);
	}
	
	// Hack: there is no reasonable default value, but this will do
	return (0);
}

/*
 *  Counts the number of innate abilities in a skill
 */

int abilities_in_skill(int skilltype)
{
	int i;
	ability_type *b_ptr;
	int count = 0;
	
	for (i = 0; i < z_info->b_max; i++)
	{
		b_ptr = &b_info[i];
		
		/* Skip non-entries */
		if (!b_ptr->name) continue;
		
		/* Skip entries for the wrong skill type */
		if (b_ptr->skilltype != skilltype) continue;
		
		/* Add to the count */
		if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum]) count++;
	}
	
	return (count);
}

bool prereqs(int skilltype, int abilitynum)
{
	int i;
	ability_type *b_ptr;
	
	b_ptr = &b_info[ability_index(skilltype,abilitynum)];
	
	if (p_ptr->skill_base[skilltype] < b_ptr->level) return (FALSE);
	
	if (b_ptr->prereqs > 0)
	{
		for (i = 0; i < b_ptr->prereqs; i++)
		{
			if (p_ptr->innate_ability[b_ptr->prereq_skilltype[i]][b_ptr->prereq_abilitynum[i]]) return (TRUE);
		}
		return (FALSE);
	}
	
	return (TRUE);
}


/*
 * Display the available songs (modelled on show_inven).
 */
void show_songs(void)
{
	int i, j, k = 0;
		
	int col = 26;
	
	char tmp_val[80];
	
	int out_index[24];
	char out_desc[24][80];
		
	/* Display the songs */
	for (k = 0, i = 0; i < SNG_WOVEN_THEMES; i++)
	{
		/* Is this song acceptable? */
		if (!p_ptr->active_ability[S_SNG][i]) continue;
		
		/* Save the index */
		out_index[k] = i;
		
		/* Save the song name */
		my_strcpy(out_desc[k], b_name + (&b_info[ability_index(S_SNG,i)])->name, sizeof(out_desc[0]));
		
		/* Advance to next "line" */
		k++;
	}
	
	// add a line for the 'stop singing' command

	/* Clear the line */
	prt("", 1, col -2);
	
	/* Clear the line with the (possibly indented) index */
	put_str("s)", 1, col);
	
	/* Display the entry itself */
	c_put_str(TERM_SLATE, "Stop Singing", 1, col + 3);

	/* Output each entry */
	for (j = 0; j < k; j++)
	{
		/* Get the index */
		i = out_index[j];
		
		/* Clear the line */
		prt("", j + 2, col -2);
		
		/* Prepare an index --(-- */
		sprintf(tmp_val, "%c)", index_to_label(i));
		
		/* Clear the line with the (possibly indented) index */
		put_str(tmp_val, j + 2, col);
		
		/* Display the entry itself */
		c_put_str(TERM_L_WHITE, out_desc[j], j + 2, col + 3);
	}
	
	// add a line for the 'exchange themes' command
	if (p_ptr->song2 != SNG_NOTHING)
	{
		/* Clear the line */
		prt("", j + 2, col -2);

		/* Clear the line with the (possibly indented) index */
		put_str("x)", j + 2, col);
		
		/* Display the entry itself */
		c_put_str(TERM_L_BLUE, "Exchange themes", j + 2, col + 3);
		
		j++;
	}
	
	/* Make a "shadow" below the list (only if needed) */
	if (j && (j < 23)) prt("", j + 2, col - 2);
}



void do_cmd_change_song()
{
	int i;
	bool done = FALSE;
	bool allow_list;
	
	int options = 0;
	int final_song = 0;
	int song_choice = -1;
	
	char out_val[80];
	char tmp_val[80];
	
	char which;

	// count the abilities
	for (i = 0; i < SNG_WOVEN_THEMES; i++)
	{
		// keep track of the number of options and final song
		if (p_ptr->active_ability[S_SNG][i])
		{
			options += 1;
		}
	}
	
	// abort if you know no songs
	if (options == 0)
	{
		msg_print("You do not know any songs of power.");
		return;
	}
	
	/* Flush the prompt */
	Term_fresh();

	/* Option to always show a list */
	if (auto_display_lists)
	{
		p_ptr->command_see = TRUE;
	}
	
	/* Start out in "display" mode */
	if (p_ptr->command_see)
	{
		/* Save screen */
		screen_save();
	}
	
	/* Repeat until done */
	while (!done)
	{
		/* Redraw if needed */
		if (p_ptr->command_see) show_songs();
		
		/* Begin the prompt */
		sprintf(out_val, "Songs: s");

		// count the abilities
		for (i = 0; i < SNG_WOVEN_THEMES; i++)
		{
			// keep track of the number of options and final song
			if (p_ptr->active_ability[S_SNG][i])
			{
				final_song = i;
				
				my_strcat(out_val, ",", sizeof(out_val));
				sprintf(tmp_val, "%c", (char) 'a' + i);
								
				/* Append */
				my_strcat(out_val, tmp_val, sizeof(out_val));
			}
		}
		
		// add an 'x' option if using woven themes
		if (p_ptr->song2 != SNG_NOTHING)
		{
			/* Append */
			my_strcat(out_val, ",x", sizeof(out_val));
		}
		
		/* Indicate ability to "view" */
		if (!p_ptr->command_see) my_strcat(out_val, ", * to see", sizeof (out_val));
		
		/* Build the prompt */
		strnfmt(tmp_val, sizeof(tmp_val), "(%s) Sing which song: ", out_val);
		
		/* Show the prompt */
		prt(tmp_val, 0, 0);
		
		/* Hack - Find the origin of the next key */
		allow_list = interactive_input(TRUE);
		
		/* Get a key */
		which = inkey();
		
		/* Parse it */
		switch (which)
		{
			case ESCAPE:
			case '\r':
			{
				done = TRUE;
				break;
			}
				
			case '*':
			case '?':
			case ' ':
			{
				/* Hide the list */
				if (p_ptr->command_see)
				{
					/* Flip flag */
					p_ptr->command_see = FALSE;
					
					/* Load screen */
					screen_load();
				}
				
				/* Show the list */
				else
				{
					/* Save screen */
					screen_save();
					
					/* Flip flag */
					p_ptr->command_see = TRUE;
				}
				
				break;
			}
												
			case 's':
			{
				song_choice = SNG_NOTHING;
				done = TRUE;
				break;
			}

			case 'x':
			{
				if (p_ptr->song2 != SNG_NOTHING)
				{
					song_choice = SNG_EXCHANGE_THEMES;
					done = TRUE;
					break;
				}
				else
				{
					bell("Illegal song choice.");
					break;
				}
			}

			default:
			{
				if ((which >= 'a') && (which < 'a' + SNG_WOVEN_THEMES))
				{
					song_choice = (int) which - 'a';
					if (p_ptr->active_ability[S_SNG][song_choice])
					{
						done = TRUE;
						break;
					}
					else
					{
						song_choice = -1;
					}
				}
				
				bell("Illegal song choice.");
				break;
			}
		}
	}
	
	/* Fix the screen if necessary */
	if (p_ptr->command_see)
	{
		/* Load screen */
		screen_load();
		
		/* Hack -- Cancel "display" */
		p_ptr->command_see = FALSE;
	}
	
	/* Clear the prompt line */
	prt("", 0, 0);
	
	if (song_choice >= 0) change_song(song_choice);

}

void wipe_screen_from(int col)
{
	int i;
	
	for (i = 1; i < SCREEN_HGT; i++)
	{
		Term_putstr(col, i, -1, TERM_WHITE, "                                                                                 ");		
	}
}


int elf_bane_bonus(monster_type *m_ptr)
{
    monster_race *r_ptr;
    
    if (m_ptr == NULL)  return (0);
    else                r_ptr = &r_info[m_ptr->r_idx];
    
    // Sil-x: a bit of a hack. Noldor and Sindar are coded as races 0 and 1 in the races.txt file
    if ((r_ptr->flags2 & (RF2_ELFBANE)) && ((p_ptr->prace == 0) || (p_ptr->prace == 1)))
    {
        // Dagohir must have killed between 32 and 63 elves
        return (5);
    }
    
    return (0);
}

#define BANE_TYPES 9

static u32b bane_flag[] =
{
	0L,
	RF3_ORC,
	RF3_WOLF,
	RF3_SPIDER,
	RF3_TROLL,
	RF3_UNDEAD,
	RF3_RAUKO,
	RF3_SERPENT,
	RF3_DRAGON
};

static char *bane_name[] =
{
	"Nothing",
	"Orc",
	"Wolf",
	"Spider",
	"Troll",
	"Wraith",
	"Rauko",
	"Serpent",
	"Dragon"
};

int bane_type_killed(int i)
{
	int j;
	int k = 0;
	
	/* Scan the monster races */
	for (j = 1; j < z_info->r_max; j++)
	{
		monster_race *r_ptr = &r_info[j];
		monster_lore *l_ptr = &l_list[j];
		
		if (r_ptr->flags3 & (bane_flag[i]))
		{
			k += l_ptr->pkills;
		}
	}
	
	return (k);
}


int bane_bonus_aux(void)
{
	int i = 2;
	int bonus = 0;
	int killed;
	
	killed = bane_type_killed(p_ptr->bane_type);
	while (i <= killed)
	{
		i *= 2;
		bonus++;
	}
	
	return (bonus);
}

int bane_bonus(monster_type *m_ptr)
{
	int bonus = 0;
	monster_race *r_ptr;
	
	// paranoia
	if (m_ptr == NULL) return (0);
	
	// entranced players don't get the bonus
	if (p_ptr->entranced) return (0);

	// knocked out players don't get the bonus
	if (p_ptr->stun > 100) return (0);
	
	r_ptr = &r_info[m_ptr->r_idx];
	
	if (r_ptr->flags3 & (bane_flag[p_ptr->bane_type]))
	{
		bonus = bane_bonus_aux();
	}
	
	return (bonus);
}

int spider_bane_bonus(void)
{
	if (bane_flag[p_ptr->bane_type] == RF3_SPIDER)	return (bane_bonus_aux());
	else											return (0);
}

int bane_menu(int *highlight)
{
	int i, k;
	
	int ch;
	int options;
	
	char buf[80];
	
	byte attr;
	
	// bane title
	Term_putstr(COL_DESCRIPTION,  2, -1, TERM_WHITE, "Enemy types");
	
	// clear the description area
	wipe_screen_from(COL_DESCRIPTION);
	
	// list the enemies
	for (i = 1; i < BANE_TYPES; i++)
	{		
		k = bane_type_killed(i);

		// Determine the appropriate colour
		if (k >= 4)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
		}
		
		strnfmt(buf, 80, "%c) %s", (char) 'a' + i - 1, bane_name[i]);
		Term_putstr(COL_DESCRIPTION,  i + 3, -1, attr, buf);
		
		if (*highlight == i)
		{
			// highlight the label
			strnfmt(buf, 80, "%c)", (char) 'a' + i - 1);
			Term_putstr(COL_DESCRIPTION,  i + 3, -1, TERM_L_BLUE, buf);
			
			/* Indent output by 2 character, and wrap at column 70 */
			text_out_wrap = 79;
			text_out_indent = COL_DESCRIPTION;
			
			Term_gotoxy(text_out_indent, BANE_TYPES + 4);
			
			/* Information */
			if (k >= 4)
			{
				strnfmt(buf, 80, "You have slain %d of these foes.", k);
				text_out_to_screen(TERM_SLATE, buf);
			}
			else
			{
				strnfmt(buf, 80, "You have slain %d of these foes,   and need to slay %d more.", k, 4 - k);
				text_out_to_screen(TERM_L_DARK, buf);
			}
			
			/* Reset text_out() vars */
			text_out_wrap = 0;
			text_out_indent = 0;
		}
		
		// keep track of the number of options
		options = i;
	}
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(COL_DESCRIPTION, 3 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	if ((ch >= 'a') && (ch <= (char) 'a' + options - 1))
	{
		*highlight = (int) ch - 'a' + 1;	
		
		bane_menu(highlight);
		
		return (*highlight);
	}
	
	if ((ch >= 'A') && (ch <= (char) 'A' + options - 1))
	{
		*highlight = (int) ch - 'A' + 1;
		return (*highlight);
	}
	
	if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
	{
		return (BANE_TYPES+1);
	}
	
	if (ch == '\t')
	{
		return (BANE_TYPES+2);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		*highlight = (*highlight + (options-2)) % options + 1;
	}
	
	/* Next item */
	if (ch == '2')
	{
		*highlight = *highlight % options + 1;
	}
	
	return (0);
}


int abilities_menu1(int *highlight)
{
	int i;
	int ch;
	int options = S_MAX;
	
	char buf[80];
		
	// title
	Term_putstr(COL_SKILL,  2, -1, TERM_WHITE, "Skills");
	
	// list the skills
	for (i = 0; i < options; i++)
	{
		strnfmt(buf, 80, "%c) %s", (char) 'a' + i, skill_names_full[i]);
		
		Term_putstr(COL_SKILL,  i + 4, -1, (*highlight == i+1) ? TERM_L_BLUE : TERM_WHITE, buf);
	}

	// clear the abilities area
	wipe_screen_from(COL_ABILITY);

	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(COL_SKILL, 3 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	if ((ch >= 'a') && (ch <= (char) 'a' + options - 1))
	{
		*highlight = (int) ch - 'a' + 1;
		
		// relist the skills
		for (i = 0; i < options; i++)
		{
			strnfmt(buf, 80, "%c) %s", (char) 'a' + i, skill_names_full[i]);
			
			Term_putstr(COL_SKILL,  i + 4, -1, (*highlight == i+1) ? TERM_L_BLUE : TERM_WHITE, buf);
		}
		
		return (*highlight);
	}

	if ((ch >= 'A') && (ch <= (char) 'A' + options - 1))
	{
		*highlight = (int) ch - 'A' + 1;

		// relist the skills
		for (i = 0; i < options; i++)
		{
			strnfmt(buf, 80, "%c) %s", (char) 'a' + i, skill_names_full[i]);
			
			Term_putstr(COL_SKILL,  i + 4, -1, (*highlight == i+1) ? TERM_L_BLUE : TERM_WHITE, buf);
		}

		return (*highlight);
	}
	
	if ((ch == ESCAPE) || (ch == 'q') || (ch == '\t'))
	{
		return (options+1);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		*highlight = (*highlight + (options-2)) % options + 1;
	}
	
	/* Next item */
	if (ch == '2')
	{
		*highlight = *highlight % options + 1;
	}
	
	return (0);
} 


int abilities_menu2(int skilltype, int *highlight)
{
	int i, j;

	ability_type *b_ptr;
	ability_type *b_ptr_hi;

	int ch;
	int options = 0; // a default value to soothe compilation warnings
	
	char buf[80];

	byte attr;
	
	// clear the abilities and description area
	wipe_screen_from(COL_ABILITY);

	// abilities title
	Term_putstr(COL_ABILITY,  2, -1, TERM_WHITE, "Abilities");
		
	// list the abilities
	for (i = 0; i < z_info->b_max; i++)
	{
		b_ptr = &b_info[i];
		
		/* Skip non-entries */
		if (!b_ptr->name) continue;
		
		/* Skip entries for the wrong skill type */
		if (b_ptr->skilltype != skilltype) continue;

		// Determine the appropriate colour
		if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
		{
			if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
			{
				if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
				{
					attr = TERM_WHITE;
				}
				else
				{
					attr = TERM_RED;
				}
			}
			else
			{
				if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
				{
					attr = TERM_L_GREEN;
				}
				else
				{
					attr = TERM_RED;
				}
			}
		}
		else
		{
			if (prereqs(skilltype, b_ptr->abilitynum)) attr = TERM_SLATE;
			else									   attr = TERM_L_DARK;
		}
		
		if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE) && (p_ptr->bane_type > 0))
		{
			strnfmt(buf, 80, "%c) %s-%s", (char) 'a' + b_ptr->abilitynum , bane_name[p_ptr->bane_type], (b_name + b_ptr->name));
		}
		else
		{
			strnfmt(buf, 80, "%c) %s", (char) 'a' + b_ptr->abilitynum , (b_name + b_ptr->name));
		}
		Term_putstr(COL_ABILITY,  b_ptr->abilitynum + 4, -1, attr, buf);

		if (*highlight == b_ptr->abilitynum + 1)
		{
			b_ptr_hi = b_ptr;

			// highlight the label
			strnfmt(buf, 80, "%c)", (char) 'a' + b_ptr->abilitynum);
			Term_putstr(COL_ABILITY,  b_ptr->abilitynum + 4, -1, TERM_L_BLUE, buf);
			
			// print the description of the highlighted ability
			if ((b_text + b_ptr->text) != NULL)
			{
				/* Indent output by 2 character, and wrap at column 70 */
				text_out_wrap = 79;
				text_out_indent = COL_DESCRIPTION;
				
				/* History */
				Term_gotoxy(text_out_indent, 4);
				text_out_to_screen(TERM_L_WHITE, b_text + b_ptr->text);
				
				/* Reset text_out() vars */
				text_out_wrap = 0;
				text_out_indent = 0;
			}
			
			// print more info if you don't have the skill
			if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
			{
				// print the prerequisites
				Term_putstr(COL_DESCRIPTION,  10, -1, attr, "Prerequisites:");
				
				strnfmt(buf, 80, "%d skill points (you have %d)", b_ptr->level, p_ptr->skill_base[skilltype]);
				Term_putstr(COL_DESCRIPTION + 2, 12, -1, TERM_L_DARK, buf);
				if (b_ptr->level <= p_ptr->skill_base[skilltype])
				{
					strnfmt(buf, 80, "%d skill points", b_ptr->level);
					Term_putstr(COL_DESCRIPTION + 2, 12, -1, TERM_SLATE, buf);
				}
				for (j = 0; j < b_ptr->prereqs; j++)
				{
					if (j == 0)
					{
						strnfmt(buf, 80, "%s", b_name + (&b_info[ability_index(b_ptr->prereq_skilltype[j], b_ptr->prereq_abilitynum[j])])->name);
					}
					else
					{
						strnfmt(buf, 80, "or %s", b_name + (&b_info[ability_index(b_ptr->prereq_skilltype[j], b_ptr->prereq_abilitynum[j])])->name);
					}
					Term_putstr(COL_DESCRIPTION + 2, 13 + j, -1, TERM_L_DARK, buf);
					if (p_ptr->innate_ability[b_ptr->prereq_skilltype[j]][b_ptr->prereq_abilitynum[j]])
					{
						strnfmt(buf, 80, "%s", b_name + (&b_info[ability_index(b_ptr->prereq_skilltype[j], b_ptr->prereq_abilitynum[j])])->name);
						if (j == 0)
						{
							Term_putstr(COL_DESCRIPTION + 2, 13 + j, -1, TERM_SLATE, buf);
						}
						else
						{
							Term_putstr(COL_DESCRIPTION + 5, 13 + j, -1, TERM_SLATE, buf);
						}
					}
				}

				if (prereqs(skilltype, b_ptr->abilitynum))
				{
					int exp_cost = (abilities_in_skill(skilltype) + 1) * 500;
					
					// give free abilties based on affinities
					exp_cost -= 500 * affinity_level(skilltype);
					if (exp_cost < 0) exp_cost = 0;
					
					// print the cost
					Term_putstr(COL_DESCRIPTION,  16, -1, TERM_L_DARK, "Current price:");
					strnfmt(buf, 80, "%d experience (you have %d)", exp_cost, p_ptr->new_exp);
					Term_putstr(COL_DESCRIPTION + 2, 18, -1, TERM_L_DARK, buf);

					if (exp_cost <= p_ptr->new_exp)
					{
						Term_putstr(COL_DESCRIPTION,  16, -1, TERM_SLATE, "Current price:");
						strnfmt(buf, 80, "%d experience", exp_cost, p_ptr->new_exp);
						Term_putstr(COL_DESCRIPTION + 2, 18, -1, TERM_SLATE, buf);
					}
				}
			}
			
			// if you have the ability and it is Bane...
			else if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE) && (p_ptr->bane_type > 0))
			{
				Term_putstr(COL_DESCRIPTION,  10, -1, TERM_WHITE, format("%s-Bane:", bane_name[p_ptr->bane_type]));
				Term_putstr(COL_DESCRIPTION,  12, -1, TERM_WHITE,
				            format("  %d slain, giving a %+d bonus", bane_type_killed(p_ptr->bane_type), bane_bonus_aux()));
			}
		}
		
		// keep track of the number of options
		options = b_ptr->abilitynum + 1;
	}

	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(COL_ABILITY, 3 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	if ((ch >= 'a') && (ch <= (char) 'a' + options - 1))
	{
		*highlight = (int) ch - 'a' + 1;	
		
		abilities_menu2(skilltype, highlight);
				
		return (*highlight);
	}
	
	if ((ch >= 'A') && (ch <= (char) 'A' + options - 1))
	{
		*highlight = (int) ch - 'A' + 1;

		abilities_menu2(skilltype, highlight);

		return (*highlight);
	}
	
	if ((ch == ESCAPE) || (ch == 'q') || (ch == '4'))
	{
		return (ABILITIES_MAX+1);
	}
	
	if (ch == '\t')
	{
		return (ABILITIES_MAX+2);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		*highlight = (*highlight + (options-2)) % options + 1;
	}
	
	/* Next item */
	if (ch == '2')
	{
		*highlight = *highlight % options + 1;
	}
		
	return (0);
}

/*
 * Hack -- ability screen
 */
void do_cmd_ability_screen(void)
{
	int skilltype = -1;
	int abilitynum = -1;
	int banechoice = -1;
	
	int highlight1 = 1;
	int highlight2 = 1;
	int highlight3 = 1;
			
	bool return_to_game = FALSE;
	bool return_to_skills = FALSE;
	bool return_to_abilities = FALSE;
	
	bool skip_purchase = FALSE;
	
	/* Save screen */
	screen_save();

	/* Clear screen */
	Term_clear();
	
	/* Process Events until "Return to Game" is selected */
	while (!return_to_game)
	{
		skilltype = abilities_menu1(&highlight1) - 1;
		
		// if a skill has been selected...
		if ((skilltype >= 0) && (skilltype < S_MAX))
		{
			while (!return_to_skills)
			{
				abilitynum = abilities_menu2(skilltype, &highlight2) - 1;

				if ((abilitynum >= 0) && (abilitynum < ABILITIES_MAX))
				{
					if (!p_ptr->have_ability[skilltype][abilitynum])
					{
						if (prereqs(skilltype, abilitynum))
						{
							int exp_cost = (abilities_in_skill(skilltype) + 1) * 500;
							
							// give free abilties based on affinities
							exp_cost -= 500 * affinity_level(skilltype);
							if (exp_cost < 0) exp_cost = 0;
							
							if (exp_cost > p_ptr->new_exp)
							{
								bell("You do not have enough experience to acquire this ability.");
							}
							else
							{
								// special menu for bane
								if ((skilltype == S_PER) && (abilitynum == PER_BANE))
								{
									while (!return_to_abilities)
									{
										skip_purchase = FALSE;

										banechoice = bane_menu(&highlight3);
										
										if ((banechoice >= 1) && (banechoice <= BANE_TYPES))
										{
											if (bane_type_killed(banechoice) < 4)
											{
												return_to_abilities = FALSE;
												skip_purchase = TRUE;
												bell("Insufficient kills to become a bane.");
											}
											else
											{
												return_to_abilities = TRUE;
											}
										}
										else if (banechoice == BANE_TYPES)
										{
											return_to_abilities = TRUE;
											skip_purchase = TRUE;
										}
										else if (banechoice == BANE_TYPES + 1)
										{
											return_to_abilities = TRUE;
											return_to_skills = TRUE;
											return_to_game = TRUE;
											skip_purchase = TRUE;
										}
									}
									
									return_to_abilities = FALSE;
								}
								
								if (!skip_purchase)
								{
									if (get_check("Are you sure you wish to gain this ability? "))
									{
										p_ptr->innate_ability[skilltype][abilitynum] = TRUE;
										p_ptr->have_ability[skilltype][abilitynum] = TRUE;
										p_ptr->active_ability[skilltype][abilitynum] = TRUE;
										Term_putstr(0, 0, -1, TERM_WHITE, "Ability gained.");
										p_ptr->new_exp -= exp_cost;
										
										if (banechoice <= 0)
										{
											// make a note in the notes file
											do_cmd_note(format("(%s)", 
															   b_name + (&b_info[ability_index(skilltype,abilitynum)])->name), p_ptr->depth);
										}
										else
										{
											// set the new bane type
											p_ptr->bane_type = banechoice;
											
											// and make a note in the notes file
											do_cmd_note(format("(%s-%s)", bane_name[banechoice],
															   b_name + (&b_info[ability_index(skilltype,abilitynum)])->name), p_ptr->depth);
										}
										
										/* Set the redraw flag for everything */
										p_ptr->redraw |= (PR_EXP | PR_BASIC);
										
										/* Recalculate bonuses */
										p_ptr->update |= (PU_BONUS);									
										p_ptr->update |= (PU_MANA);
									}
								}
								skip_purchase = FALSE;
								banechoice = -1;
								
							}
						}
						else
						{
							bell("Insufficient prerequisites for ability!");
						}
					}
					
					// if you already have the ability...
					else
					{
						// toggle its activity
						if (p_ptr->active_ability[skilltype][abilitynum])
						{
							p_ptr->active_ability[skilltype][abilitynum] = FALSE;
							Term_putstr(0, 0, -1, TERM_WHITE, "Ability now switched off.");
							
							// need to cancel second song in some cases
							if ((skilltype == S_SNG) && (abilitynum == SNG_WOVEN_THEMES))
							{
								p_ptr->song2 = SNG_NOTHING;
							}
						}
						else
						{
							p_ptr->active_ability[skilltype][abilitynum] = TRUE;
							Term_putstr(0, 0, -1, TERM_WHITE, "Ability now switched on. ");
						}
						
						/* Set the redraw flag for everything */
						p_ptr->redraw |= (PR_EXP | PR_BASIC);
						
						/* Recalculate bonuses */
						p_ptr->update |= (PU_BONUS);									
						p_ptr->update |= (PU_MANA);
					}
				}
				else if (abilitynum == ABILITIES_MAX)
				{
					return_to_skills = TRUE;
				}
				else if (abilitynum == ABILITIES_MAX + 1)
				{
					return_to_skills = TRUE;
					return_to_game = TRUE;
				}
			}
			
			// reset some things for the next time around
			highlight2 = 1;
			return_to_skills = FALSE;
		}
		else if (skilltype >= S_MAX)
		{
			return_to_game = TRUE;
		}
	}
	
	/* Flush messages */
	//message_flush();
	
	/* Load screen */
	screen_load();
}


bool enchant_then_numbers;

/*
 * A structure to hold a tval and its description
 */
typedef struct smithing_tval_desc
{
	int category;
	int tval;
	cptr desc;
} smithing_tval_desc;

// object being created
object_type smith_o_body;
object_type *smith_o_ptr = &smith_o_body;

// backup object
object_type smith2_o_body;
object_type *smith2_o_ptr = &smith2_o_body;

// super backup object
object_type smith3_o_body;
object_type *smith3_o_ptr = &smith3_o_body;

// artefact being created
#define smith_a_name	(z_info->art_self_made_max - 1)
#define smith_a_ptr		(&a_info[smith_a_name])

// backup artefact
#define smith2_a_name	(z_info->art_self_made_max - 2)
#define smith2_a_ptr	(&a_info[smith2_a_name])

/*
 * A structure to hold the costs of smithing something
 */
typedef struct smithing_cost_type
{
	int str;
	int dex;
	int con;
	int gra;
	int exp;
	int smt;
	int mithril;
	int uses;
	int drain;
    int weaponsmith;
    int armoursmith;
    int jeweller;
    int enchantment;
    int artistry;
    int artifice;
} smithing_cost_type;

smithing_cost_type smithing_cost;

#define CAT_WEAPON  0
#define CAT_ARMOUR  1
#define CAT_JEWELRY 2

#define MAX_SMITHING_TVALS 18

#define SMT_MENU_CREATE   1
#define SMT_MENU_ENCHANT  2
#define SMT_MENU_ARTEFACT 3
#define SMT_MENU_NUMBERS  4
#define SMT_MENU_MELT     5
#define SMT_MENU_ACCEPT   6

#define SMT_MENU_MAX      6

#define SMT_NUM_MENU_I_ATT	 1
#define SMT_NUM_MENU_D_ATT	 2
#define SMT_NUM_MENU_I_DS	 3
#define SMT_NUM_MENU_D_DS	 4
#define SMT_NUM_MENU_I_EVN	 5
#define SMT_NUM_MENU_D_EVN	 6
#define SMT_NUM_MENU_I_PS	 7
#define SMT_NUM_MENU_D_PS	 8
#define SMT_NUM_MENU_I_PVAL	 9
#define SMT_NUM_MENU_D_PVAL	10
#define SMT_NUM_MENU_I_WGT	11
#define SMT_NUM_MENU_D_WGT	12

#define SMT_NUM_MENU_MAX	12


#define	COL_SMT1		 2
#define COL_SMT2		16
#define COL_SMT3		36
#define COL_SMT4		66

			
/*
 * A list of tvals and their textual names
 */
static const smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS] =
{
	{ CAT_WEAPON,  TV_SWORD,             "Sword"                },
	{ CAT_WEAPON,  TV_POLEARM,           "Axe or Polearm"       },
	{ CAT_WEAPON,  TV_HAFTED,            "Blunt Weapon"         },
	{ CAT_WEAPON,  TV_DIGGING,           "Digger"               },
	{ CAT_WEAPON,  TV_BOW,               "Bow"                  },
	{ CAT_WEAPON,  TV_ARROW,             "Arrows"               },
	{ CAT_JEWELRY, TV_RING,              "Ring"                 },
	{ CAT_JEWELRY, TV_AMULET,            "Amulet"               },
	{ CAT_JEWELRY, TV_LIGHT,             "Light"                },
	{ CAT_JEWELRY, TV_HORN,              "Horn"                 },
	{ CAT_ARMOUR,  TV_SOFT_ARMOR,        "Soft Armour"           },
	{ CAT_ARMOUR,  TV_MAIL,              "Mail"                 },
	{ CAT_ARMOUR,  TV_CLOAK,             "Cloak"                },
	{ CAT_ARMOUR,  TV_SHIELD,            "Shield"               },
	{ CAT_ARMOUR,  TV_HELM,              "Helm"                 },
	{ CAT_ARMOUR,  TV_CROWN,             "Crown"                },
	{ CAT_ARMOUR,  TV_GLOVES,            "Gloves"               },
	{ CAT_ARMOUR,  TV_BOOTS,             "Boots"                },
};


/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_cat
{
	int category;
	cptr desc;
} smithing_flag_cat;

#define CAT_STAT	1
#define CAT_SUST	2
#define CAT_SKILL	3
#define CAT_MEL		4
#define CAT_SLAY	5
#define CAT_RES		6
#define CAT_CURSE	7
#define CAT_MISC	8

#define MAX_CATS	8

#define MAX_SMITHING_FLAGS (32*3)

static const smithing_flag_cat smithing_flag_cats[] =
{
	{ CAT_STAT,		"Stat bonuses"	},
	{ CAT_SUST,		"Sustains"		},
	{ CAT_SKILL,	"Skill bonuses"	},
	{ CAT_MEL,		"Melee powers"	},
	{ CAT_SLAY,		"Slays"			},
	{ CAT_RES,		"Resistances"	},
	{ CAT_CURSE,	"Curses"		},
	{ CAT_MISC,		"Misc"			}
};

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_desc
{
	int category;
	u32b flag;
	int flagset;
	cptr desc;
} smithing_flag_desc;

/*
 * A list of tvals and their textual names
 */
static const smithing_flag_desc smithing_flag_types[] =
{
	{ CAT_STAT,		TR1_STR,			1,	"Str bonus"		},
	{ CAT_STAT,		TR1_DEX,			1,	"Dex bonus"		},
	{ CAT_STAT,		TR1_CON,			1,	"Con bonus"		},
	{ CAT_STAT,		TR1_GRA,			1,	"Gra bonus"		},
	{ CAT_STAT,		TR1_NEG_STR,		1,	"Str penalty"	},
	{ CAT_STAT,		TR1_NEG_DEX,		1,	"Dex penalty"	},
	{ CAT_STAT,		TR1_NEG_CON,		1,	"Con penalty"	},
	{ CAT_STAT,		TR1_NEG_GRA,		1,	"Gra penalty"	},
	{ CAT_SKILL,	TR1_MEL,			1,	"Melee"			},
	{ CAT_SKILL,	TR1_ARC,			1,	"Archery"		},
	{ CAT_SKILL,	TR1_STL,			1,	"Stealth"		},
	{ CAT_SKILL,	TR1_PER,			1,	"Perception"	},
	{ CAT_SKILL,	TR1_WIL,			1,	"Will"			},
	{ CAT_SKILL,	TR1_SMT,			1,	"Smithing"		},
	{ CAT_SKILL,	TR1_SNG,			1,	"Song"			},
	{ CAT_MISC,		TR1_DAMAGE_SIDES,	1,	"Damage bonus"			},
	{ CAT_MISC,		TR2_LIGHT,			2,	"Light"					},
	{ CAT_MISC,		TR2_SLOW_DIGEST,	2,	"Sustenance"			},
	{ CAT_MISC,		TR2_REGEN,			2,	"Regeneration"			},
	{ CAT_MISC,		TR2_SEE_INVIS,		2,	"See Invisible"			},
	{ CAT_MISC,		TR2_FREE_ACT,		2,	"Free Action"			},
	{ CAT_MISC,		TR2_SPEED,			2,	"Speed"					},
	{ CAT_MISC,		TR2_RADIANCE,		2,	"Radiance"				},
	{ CAT_MEL,		TR1_TUNNEL,			1,	"Tunneling Bonus"		},
	{ CAT_MEL,		TR1_SHARPNESS,		1,	"Sharpness"				},
	{ CAT_MEL,		TR1_VAMPIRIC,		1,	"Vampiric"				},
	{ CAT_SLAY,		TR1_SLAY_ORC,		1,	"Slay Orc"				},
	{ CAT_SLAY,		TR1_SLAY_TROLL,		1,	"Slay Troll"			},
	{ CAT_SLAY,		TR1_SLAY_WOLF,		1,	"Slay Wolf"				},
	{ CAT_SLAY,		TR1_SLAY_SPIDER,	1,	"Slay Spider"			},
	{ CAT_SLAY,		TR1_SLAY_UNDEAD,	1,	"Slay Undead"			},
	{ CAT_SLAY,		TR1_SLAY_RAUKO,		1,	"Slay Rauko"			},
	{ CAT_SLAY,		TR1_SLAY_DRAGON,	1,	"Slay Dragon"			},
	{ CAT_SLAY,		TR1_BRAND_COLD,		1,	"Brand with Cold"		},
	{ CAT_SLAY,		TR1_BRAND_FIRE,		1,	"Brand with Fire"		},
	{ CAT_SLAY,		TR1_BRAND_POIS,		1,	"Brand with Poison"		},
	{ CAT_SUST,		TR2_SUST_STR,		2,	"Sustain Str"			},
	{ CAT_SUST,		TR2_SUST_DEX,		2,	"Sustain Dex"			},
	{ CAT_SUST,		TR2_SUST_CON,		2,	"Sustain Con"			},
	{ CAT_SUST,		TR2_SUST_GRA,		2,	"Sustain Gra"			},
	{ CAT_RES,		TR2_RES_COLD,		2,	"Resist Cold"			},
	{ CAT_RES,		TR2_RES_FIRE,		2,	"Resist Fire"			},
	{ CAT_RES,		TR2_RES_POIS,		2,	"Resist Poison"			},
	{ CAT_RES,		TR2_RES_FEAR,		2,	"Resist Fear"			},
	{ CAT_RES,		TR2_RES_BLIND,		2,	"Resist Blindness"		},
	{ CAT_RES,		TR2_RES_CONFU,		2,	"Resist Confusion"		},
	{ CAT_RES,		TR2_RES_STUN,		2,	"Resist Stunning"		},
	{ CAT_RES,		TR2_RES_HALLU,		2,	"Resist Hallucination"	},
	{ CAT_CURSE,	TR2_DANGER,			2,	"Danger"				},
	{ CAT_CURSE,	TR2_FEAR,			2,	"Terror"				},
	{ CAT_CURSE,	TR2_HUNGER,			2,	"Hunger"				},
	{ CAT_CURSE,	TR2_DARKNESS,		2,	"Darkness"				},
	{ CAT_CURSE,	TR2_AGGRAVATE,		2,	"Wrath"				    },
	{ CAT_CURSE,	TR2_HAUNTED,		2,	"Haunted"			    },
//	{ CAT_CURSE,	TR2_SLOWNESS,		2,	"Slow"					},
	{ CAT_CURSE,	TR2_VUL_COLD,		2,	"Cold Vulnerability"	},
	{ CAT_CURSE,	TR2_VUL_FIRE,		2,	"Fire Vulnerability"	},
	{ CAT_CURSE,	TR2_VUL_POIS,		2,	"Poison Vulnerability"	},
	{ CAT_CURSE,	TR3_LIGHT_CURSE,	3,	"Cursed"				},
	{ 0,			0,					0,	""						}
};



/*
 * Determines whether the attack bonus of an item is eligible for modification.
 */
int att_valid(void)
{
	switch (smith_o_ptr->tval)
	{
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		case TV_ARROW:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			return (TRUE);
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_ACCURACY) return (TRUE);
			if (smith_o_ptr->name1) return (TRUE);
		}
	}
	
	return (FALSE);
}


/*
 * Determines the maximum legal attack bonus for an item.
 */
int att_max(bool assume_artistry)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int att = 0;
    bool artistry = assume_artistry || p_ptr->active_ability[S_SMT][SMT_FINE];
	
	switch (smith_o_ptr->tval)
	{
		case TV_ARROW:
		{
			att = 0;
			if (artistry)               att += 3;
			if (smith_o_ptr->name1)     att += 8;
			if (smith_o_ptr->name2)     att = 0;
			break;
		}
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		{
			att = k_ptr->att;
			if (artistry)               att += 1;
			if (smith_o_ptr->name2)		att += e_ptr->max_att;
			if (smith_o_ptr->name1)		att += 4;
			break;
		}
		case TV_BOOTS:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			att = k_ptr->att;
			if (artistry)               att += 1;
			if (att > 0)				att =  0;
			if (smith_o_ptr->name2)		att += e_ptr->max_att;
			if (smith_o_ptr->name1)		att += 1;
			break;
		}
		case TV_GLOVES:
		{
			att = k_ptr->att;
			if (artistry)               att += 1;
			if (att > 0)				att =  0;
			if (smith_o_ptr->name2)		att += e_ptr->max_att;
			if (smith_o_ptr->name1)		att += 2;
			break;
		}
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_ACCURACY)	att = 4;
			if (smith_o_ptr->name1)						att = 4;
			break;
		}
	}
	
	return (att);
}


/*
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int att = 0;
	
	switch (smith_o_ptr->tval)
	{
		case TV_ARROW:
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			att = k_ptr->att;
			if (smith_o_ptr->name2 && (e_ptr->max_att > 0)) att += 1;
			break;
		}
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_ACCURACY) att = 1;
			break;
		}
	}
	
	return (att);
}


/*
 * Determines whether the damage sides of an item is eligible for modification.
 */
int ds_valid(void)
{
	switch (smith_o_ptr->tval)
	{
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		{
			return (TRUE);
		}
	}
	
	return (FALSE);
}


/*
 * Determines the maximum legal damage sides for an item.
 */
int ds_max(bool assume_artistry)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int ds = 0;
    bool artistry = assume_artistry || p_ptr->active_ability[S_SMT][SMT_FINE];
	
	switch (smith_o_ptr->tval)
	{
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		{
			ds = k_ptr->ds;
			if (artistry)                               ds += 1;
			if (smith_o_ptr->name2)						ds += e_ptr->to_ds;
			if (smith_o_ptr->name1)						ds += 2;
			break;
		}
	}
	
	return (ds);
}


/*
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int ds = 0;
	
	switch (smith_o_ptr->tval)
	{
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		{
			ds = k_ptr->ds;
			if (smith_o_ptr->name2 && (e_ptr->to_ds > 0)) ds += 1;
			break;
		}
	}
	
	return (ds);
}


/*
 * Determines whether the evasion bonus of an item is eligible for modification.
 */
int evn_valid(void)
{
	switch (smith_o_ptr->tval)
	{
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			return (TRUE);
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_EVASION) return (TRUE);
			if (smith_o_ptr->name1) return (TRUE);
		}
	}
	
	if (smith_o_ptr->name1 && ((smith_o_ptr->tval == TV_SWORD) || (smith_o_ptr->tval == TV_POLEARM) || (smith_o_ptr->tval == TV_HAFTED)))
	{
		return (TRUE);
	}
	
	return (FALSE);
}


/*
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max(bool assume_artistry)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int evn = 0;
    bool artistry = assume_artistry || p_ptr->active_ability[S_SMT][SMT_FINE];
    
	switch (smith_o_ptr->tval)
	{
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			evn = k_ptr->evn;
			if (artistry)                               evn += 1;
			if (smith_o_ptr->name2)						evn += e_ptr->max_evn;
			if (smith_o_ptr->name1)						evn += 1;
			break;
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_EVASION)	evn = 4;
			if (smith_o_ptr->name1)						evn = 4;
			break;
		}
			
		default:
		{
			evn = k_ptr->evn;
			if (smith_o_ptr->name2) evn += e_ptr->max_evn;
			if (smith_o_ptr->name1) evn += 1;
		}
	}
	
	return (evn);
}


/*
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int evn = 0;
	
	switch (smith_o_ptr->tval)
	{
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			evn = k_ptr->evn;
			if (smith_o_ptr->name2 && (e_ptr->max_evn > 0)) evn += 1;
			break;
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_EVASION) evn = 1;
			break;
		}
			
		default:
		{
			evn = k_ptr->evn;
			if (smith_o_ptr->name2 && (e_ptr->max_evn > 0)) evn += 1;
		}
	}
	
	return (evn);
}


/*
 * Determines whether the protection sides of an item is eligible for modification.
 */
int ps_valid(void)
{
	switch (smith_o_ptr->tval)
	{
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			return (TRUE);
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_PROTECTION)    return (TRUE);
			if (smith_o_ptr->name1)                         return (TRUE);
		}
	}
	
	return (FALSE);
}


/*
 * Determines the maximum legal protection sides for an item.
 */
int ps_max(bool assume_artistry)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int ps = 0;
    
    bool artistry = assume_artistry || p_ptr->active_ability[S_SMT][SMT_FINE];
	
	switch (smith_o_ptr->tval)
	{
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			ps = k_ptr->ps;
			
			if (artistry)	ps += 1;

			// cloaks, robes and filthy rags cannot get extra protection sides
			if ((smith_o_ptr->tval == TV_CLOAK) || 
			    ((smith_o_ptr->tval == TV_SOFT_ARMOR) && (smith_o_ptr->sval == SV_FILTHY_RAG)) ||
				((smith_o_ptr->tval == TV_SOFT_ARMOR) && (smith_o_ptr->sval == SV_ROBE)) )
			{
				ps = 0;
			}
			
			if (smith_o_ptr->name2) ps += e_ptr->to_ps;
			if (smith_o_ptr->name1) ps += 2;
			break;
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_PROTECTION)	ps = 3;
			if (smith_o_ptr->name1)							ps = 3;
			break;
		}
	}
	
	return (ps);
}


/*
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	int ps = 0;
	
	switch (smith_o_ptr->tval)
	{
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_MAIL:
		{
			ps = k_ptr->ps;
			if (smith_o_ptr->name2 && (e_ptr->to_ps > 0)) ps += 1;
			break;
		}
			
		case TV_RING:
		{
			if (smith_o_ptr->sval == SV_RING_PROTECTION) ps = 1;
			break;
		}
	}
	
	return (ps);
}


/*
 * Determines whether the pval of an item is eligible for modification.
 */
int pval_valid(void)
{
	u32b f1, f2, f3;
	
	object_flags(smith_o_ptr, &f1, &f2, &f3);
	
	return (f1 & (TR1_PVAL_MASK));
}


/*
 * Determines the maximum legal pval for an item.
 */
int pval_max(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	u32b f1, f2, f3;
	int pval = 4;
	
	object_flags(smith_o_ptr, &f1, &f2, &f3);
	
	// start with the base pval
	pval = k_ptr->pval;
	
	// artefacts have pvals that are mostly unlimited 
	if (smith_o_ptr->name1)
	{
		pval += 4;
	}
	
	// non-artefact rings and amulets have a maximum pval of 4
	else if ((smith_o_ptr->tval == TV_RING) || (smith_o_ptr->tval == TV_AMULET))
	{
		pval = 4;
	}
	
	// special items have pvals that are limited by their 'special.txt' entries
	if (smith_o_ptr->name2)
	{
		if (cursed_p(smith_o_ptr))
		{
			if (e_ptr->max_pval > 0) pval -= 1;
		}
		else
		{
			pval += e_ptr->max_pval;
		}
	}
	
	return (pval);
}


/*
 * Determines the minimum legal pval for an item.
 */
int pval_min(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	ego_item_type *e_ptr = &e_info[smith_o_ptr->name2];
	u32b f1, f2, f3;
	int pval = 0;
	
	object_flags(smith_o_ptr, &f1, &f2, &f3);
	
	// start with the base pval
	pval = k_ptr->pval;
	
	// artefacts have pvals that are mostly unlimited 
	if (smith_o_ptr->name1)
	{
		pval -= 4;
	}
	
	// non-artefact rings and amulets have a maximum pval of 4
	else if ((smith_o_ptr->tval == TV_RING) || (smith_o_ptr->tval == TV_AMULET))
	{
		pval = -4;
	}

	// special items have pvals that are limited by their 'special.txt' entries
	if (smith_o_ptr->name2)
	{
		if (cursed_p(smith_o_ptr))
		{
			if (e_ptr->max_pval > 0) pval -= e_ptr->max_pval;
		}
		else
		{
			if (e_ptr->max_pval > 0) pval += 1;
		}
	}
	
	return (pval);
}


/*
 * Determines whether the weight of an item is eligible for modification.
 */
int wgt_valid(void)
{
	switch (smith_o_ptr->tval)
	{
		case TV_ARROW:
		case TV_RING:
		case TV_AMULET:
		case TV_LIGHT:
		case TV_HORN:
		{
			return (FALSE);
		}
	}
	
	return (TRUE);
}


/*
 * Determines the maximum legal weight for an item.
 */
int wgt_max(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
	
	return (k_ptr->weight * 2);
}


/*
 * Determines the minimum legal weight for an item.
 */
int wgt_min(void)
{
	object_kind *k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 10) * 5;
    
	return (weight);
}


/*
 * Moves the light blue highlighted letter.
 */
void move_displayed_highlight(int old_highlight, byte old_attr, int new_highlight, int col)
{
	char buf[80];
	
	// remove highlight from the old label
	strnfmt(buf, 80, "%c)", (char) 'a' + old_highlight - 1);
	Term_putstr(col, old_highlight + 1, -1, old_attr, buf);
	
	// highlight the new label
	strnfmt(buf, 80, "%c)", (char) 'a' + new_highlight - 1);
	Term_putstr(col, new_highlight + 1, -1, TERM_L_BLUE, buf);
}



bool melt_mithril_item(int item_num)
{
	int number = 0;
	int item, i;
	u32b f1, f2, f3;
	
	for (item = 0; item < INVEN_TOTAL; item++)
	{
		object_type *o_ptr = &inventory[item];
		
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (f3 & (TR3_MITHRIL))
		{
			number += 1;
		}
		
		if (number == item_num)
		{
			int slots_needed = o_ptr->weight / 99;
			int empty_slots = 0;
			
			// Equipments needs an extra slot
			if (item >= INVEN_WIELD) slots_needed++;
			
			// Count empty slots
			for (i = INVEN_PACK - 1; i > 0; i--)
			{
				if (!(&inventory[i])->k_idx) empty_slots++;
			}
			
			if (empty_slots < slots_needed)
			{
				msg_print("You do not have enough room in your pack.");
				if (slots_needed - empty_slots == 1)
				{
					msg_print("You must free up another slot.");
				}
				else
				{
					msg_format("You must free up %d more slots.", slots_needed - empty_slots);
				}
				return (FALSE);
			}
				
			if (get_check("Are you sure you wish to melt this item down? "))
			{
				int slot;
				object_type *i_ptr;
				object_type object_type_body;

				// Get local object
				i_ptr = &object_type_body;
				
				// Prepare the base object for the mithril
				object_prep(i_ptr, lookup_kind(TV_METAL, SV_METAL_MITHRIL));
							
				// set the appropriate quantity
				i_ptr->number = o_ptr->weight;

				// remove the item
				inven_item_increase(item, -1);
				inven_item_describe(item);
				inven_item_optimize(item);
				window_stuff();
				
				// give the mithril to the player...
				
				// if there is too much, then break it up
				while (i_ptr->number > 99)
				{
					object_type *i_ptr2;
					object_type object_type_body2;

					// Get local object
					i_ptr2 = &object_type_body2;
					
					// decrease the main stack
					i_ptr->number -= 99;

					// Prepare the base object for the mithril
					object_prep(i_ptr2, lookup_kind(TV_METAL, SV_METAL_MITHRIL));

					// increase the new stack
					i_ptr2->number = 99;
					
					// give it to the player
					slot = inven_carry(i_ptr2);
					inven_item_optimize(slot);
					inven_item_describe(slot);
					window_stuff();
				}
				
				// now give the last stack of mithril to the player
				slot = inven_carry(i_ptr);
				inven_item_optimize(slot);
				inven_item_describe(slot);
				window_stuff();
				
				return (TRUE);
			}
			
			else return (FALSE);
			
		}
	}
	
	return (FALSE);
}


int mithril_items_carried(void)
{
	int number = 0;
	int item;
	u32b f1, f2, f3;
	
	for (item = 0; item < INVEN_TOTAL; item++)
	{
		object_type *o_ptr = &inventory[item];
		
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (f3 & (TR3_MITHRIL))
		{
			number += 1;
		}
	}
	
	return (number);
}

int mithril_carried(void)
{
	int w = 0;
	int item;
	
	for (item = 0; item < INVEN_WIELD; item++)
	{
		object_type *o_ptr = &inventory[item];
		
		if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == SV_METAL_MITHRIL))
		{
			w += o_ptr->number;
		}
	}
	
	return (w);
}


void use_mithril(int cost)
{
	int item;
	
	for (item = INVEN_WIELD - 1; item >= 0; item--)
	{
		object_type *o_ptr = &inventory[item];
		
		if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == SV_METAL_MITHRIL))
		{
			if (o_ptr->number >= cost)
			{
				inven_item_increase(item, -cost);
				inven_item_describe(item);
				inven_item_optimize(item);
				return;
			}
			else
			{
				cost -= o_ptr->number;
				inven_item_increase(item, -o_ptr->number);
				inven_item_describe(item);
				inven_item_optimize(item);
			}
		}
	}
	
	return;
}


/*
 * Determines how many uses are left for a given forge.
 */
int forge_uses(int y, int x)
{
	byte feat = cave_feat[y][x];
	
	if (!cave_forge_bold(y, x)) return (0);
	
	if (feat <= FEAT_FORGE_NORMAL_TAIL)		return (feat - FEAT_FORGE_NORMAL_HEAD);
	if (feat <= FEAT_FORGE_GOOD_TAIL)		return (feat - FEAT_FORGE_GOOD_HEAD);
	else									return (feat - FEAT_FORGE_UNIQUE_HEAD);
}


/*
 * Determines how high a bonus is provided by a given forge.
 */
int forge_bonus(int y, int x)
{
	byte feat = cave_feat[y][x];
	
	if (!cave_forge_bold(y, x)) return (0);
	
	if (feat <= FEAT_FORGE_NORMAL_TAIL)		return (0);
	if (feat <= FEAT_FORGE_GOOD_TAIL)		return (3);
	else									return (7);
}


/*
 * Determines the difficulty modifier for pvals.
 *
 * The marginal difficulty of increasing a pval increases by 1 each time, if the base is up to 5,
 * by 2 each time if the base is 6--10, and so on.
 */
void dif_mod(int value, int positive_base, int *dif_inc)
{
	int mod = 1 + ((positive_base-1) / 5);
	
	// deal with positive values in a triangular number influenced way
	if (value > 0)
	{
		*dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
	}
}


/*
 * Determines the difficulty of a given object.
 */
int object_difficulty(object_type *o_ptr)
{
	object_kind *k_ptr = &k_info[o_ptr->k_idx];
	int x, new, base;
	int i;
	int dif = 0;
	int dif_inc = 0;
	int dif_dec = 0;
	int weight_factor;
	u32b f1, f2, f3;
	int brands = 0;
	int dif_mult = 100;
    int cat = 0; // default to soothe compilation warnings
		
	// reset smithing costs
	smithing_cost.str = 0;
	smithing_cost.dex = 0;
	smithing_cost.con = 0;
	smithing_cost.gra = 0;
	smithing_cost.exp = 0;
	smithing_cost.mithril = 0;
	smithing_cost.uses = 1;
	smithing_cost.drain = 0;
    smithing_cost.weaponsmith = 0;
    smithing_cost.armoursmith = 0;
    smithing_cost.jeweller = 0;
    smithing_cost.enchantment = 0;
    smithing_cost.artistry = 0;
    smithing_cost.artifice = 0;
    
	// extract object flags
	object_flags(o_ptr, &f1, &f2, &f3);
	
    // special rules for horns
    if (o_ptr->tval == TV_HORN)
    {
        dif_inc += k_ptr->level;
        switch (o_ptr->sval)
        {
            case SV_HORN_TERROR:    smithing_cost.gra += 1; break;
            case SV_HORN_THUNDER:   smithing_cost.dex += 1; break;
            case SV_HORN_FORCE:     smithing_cost.str += 1; break;
            case SV_HORN_BLASTING:  smithing_cost.con += 1; break;
            // SV_HORN_WARNING
        }
    }
    
    // different rules for most other items
	else if (!((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET)))
	{
		// We need to ignore the flags that are basic 
		// to the object type and focus on the special/artefact ones. We can do this by 
		// subtracting out the basic flags
		f1 &= ~(k_ptr->flags1);
		f2 &= ~(k_ptr->flags2);
		f3 &= ~(k_ptr->flags3);
		
		// need to add tunneling back in...
		if (k_ptr->flags1 & TR1_TUNNEL) f1 |= TR1_TUNNEL;
		
		// need to add stealth back in...
		if (k_ptr->flags1 & TR1_STL) f1 |= TR1_STL;

		// base item
		dif_inc += k_ptr->level / 2;
	}
		
	// unsual weight
    if (o_ptr->weight == 0)                 weight_factor = 1100;
	else if (o_ptr->weight > k_ptr->weight)	weight_factor = 100 * o_ptr->weight / k_ptr->weight;
	else                                    weight_factor = 100 * k_ptr->weight / o_ptr->weight;
	dif_inc += (weight_factor - 100) / 10;

	// attack bonus
	x = o_ptr->att - k_ptr->att;

	// special costs for attack bonus for arrows (half difficulty modifier)
	if ((o_ptr->tval == TV_ARROW) && (x > 0))
	{	
		int old_di = dif_inc;
		
		dif_mod(x, 5, &dif_inc);
		dif_inc = (dif_inc - old_di) / 2;
	}
	
	// normal costs for other items
	else
	{
		dif_mod(x, 5, &dif_inc);
	}
	
	// evasion bonus
	x = o_ptr->evn - k_ptr->evn;
	dif_mod(x, 5, &dif_inc);
	
	// damage bonus
	x = (o_ptr->ds - k_ptr->ds);
	dif_mod(x, 8 + o_ptr->dd, &dif_inc);

	// protection bonus
	base = (k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0;
	new = (o_ptr->ps > 0) ? ((o_ptr->ps + 1) * o_ptr->pd) : 0;
	x = new - base;
	dif_mod(x, 4, &dif_inc);

	// weapon modifiers
	if (f1 & TR1_SLAY_ORC)			{	dif_inc += 5;	}
	if (f1 & TR1_SLAY_TROLL)		{	dif_inc += 5;	}
	if (f1 & TR1_SLAY_WOLF)			{	dif_inc += 6;	}
	if (f1 & TR1_SLAY_SPIDER)		{	dif_inc += 6;	}
	if (f1 & TR1_SLAY_UNDEAD)		{	dif_inc += 6;	}
	if (f1 & TR1_SLAY_RAUKO)		{	dif_inc += 7;	}
	if (f1 & TR1_SLAY_DRAGON)		{	dif_inc += 7;	}
	
	if (f1 & TR1_BRAND_COLD)		{	dif_inc += 24;	smithing_cost.str += 2;	brands++; }
	if (f1 & TR1_BRAND_FIRE)		{	dif_inc += 20;	smithing_cost.str += 2;	brands++; }
	if (f1 & TR1_BRAND_POIS)		{	dif_inc += 16;	smithing_cost.str += 2;	brands++; }
	if (brands > 1)					{	dif_inc += (brands-1) * 20;						  }
	
	if (f1 & TR1_SHARPNESS)			{	dif_inc += 20;	smithing_cost.str += 2;	}
	if (f1 & TR1_SHARPNESS2)		{	dif_inc += 40;	smithing_cost.str += 4;	} // not available in smithing
	if (f1 & TR1_VAMPIRIC)			{	dif_inc += 10;	smithing_cost.str += 2;	}
	
	// pval dependent bonuses
	if (f1 & TR1_TUNNEL)
	{
		x = o_ptr->pval - k_ptr->pval;
		dif_mod(x, 10, &dif_inc);
		smithing_cost.str += (x > 0) ? x : 0;
	}
	if (o_ptr->pval != 0)
	{
		x = (o_ptr->pval > 0) ? o_ptr->pval : 0;
		
		if (f1 & TR1_DAMAGE_SIDES)	{	dif_mod(x, 15, &dif_inc);	smithing_cost.str += x;		}
		if (f1 & TR1_STR)			{	dif_mod(x, 12, &dif_inc);	smithing_cost.str += x;		}
		if (f1 & TR1_DEX)			{	dif_mod(x, 12, &dif_inc);	smithing_cost.dex += x;		}
		if (f1 & TR1_CON)			{	dif_mod(x, 12, &dif_inc);	smithing_cost.con += x;		}
		if (f1 & TR1_GRA)			{	dif_mod(x, 12, &dif_inc);	smithing_cost.gra += x;		}
		if (f1 & TR1_MEL)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}
		if (f1 & TR1_ARC)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}
		if (f1 & TR1_STL)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}
		if (f1 & TR1_PER)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}
		if (f1 & TR1_WIL)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}
		if (f1 & TR1_SMT)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}
		if (f1 & TR1_SNG)			{	dif_mod(x, 4, &dif_inc);	smithing_cost.exp += x*100;	}

		x = (o_ptr->pval < 0) ? o_ptr->pval : 0;

		if (f1 & TR1_NEG_STR)		{	dif_mod(-x, 12, &dif_inc);	smithing_cost.str -= x;		}
		if (f1 & TR1_NEG_DEX)		{	dif_mod(-x, 12, &dif_inc);	smithing_cost.dex -= x;		}
		if (f1 & TR1_NEG_CON)		{	dif_mod(-x, 12, &dif_inc);	smithing_cost.con -= x;		}
		if (f1 & TR1_NEG_GRA)		{	dif_mod(-x, 12, &dif_inc);	smithing_cost.gra -= x;		}
	}
	
	// Sustains
	if (f2 & TR2_SUST_STR)		{	dif_inc += 3;	smithing_cost.str += 1;	}
	if (f2 & TR2_SUST_DEX)		{	dif_inc += 3;	smithing_cost.dex += 1;	}
	if (f2 & TR2_SUST_CON)		{	dif_inc += 3;	smithing_cost.con += 1;	}
	if (f2 & TR2_SUST_GRA)		{	dif_inc += 3;	smithing_cost.gra += 1;	}
	
	// Abilities
	if (f2 & TR2_SLOW_DIGEST) 	{	dif_inc += 4;							}
	if (f2 & TR2_RADIANCE) 		{	dif_inc += 8;	smithing_cost.gra += 1;	}
	if (f2 & TR2_LIGHT)			{	dif_inc += 8;	smithing_cost.gra += 1;	}
	if (f2 & TR2_REGEN) 		{	dif_inc += 8;	smithing_cost.con += 1;	}
	if (f2 & TR2_SEE_INVIS) 	{	dif_inc += 8;							}
	if (f2 & TR2_FREE_ACT) 		{	dif_inc += 8;							}
	if (f2 & TR2_SPEED)			{	dif_inc += 30;	smithing_cost.con += 5;	}
	
	// Elemental Resistances
	if (f2 & TR2_RES_COLD)		{	dif_inc += 8;	smithing_cost.con += 1;	}
	if (f2 & TR2_RES_FIRE)		{	dif_inc += 8;	smithing_cost.con += 1;	}
	if (f2 & TR2_RES_POIS)		{	dif_inc += 8;	smithing_cost.con += 1;	}
	if (f2 & TR2_RES_DARK)		{	dif_inc += 8;	smithing_cost.gra += 1;	}
	
	// Other Resistances
	if (f2 & TR2_RES_BLIND)		{	dif_inc += 6;							}
	if (f2 & TR2_RES_CONFU)		{	dif_inc += 6;							}
	if (f2 & TR2_RES_STUN)		{	dif_inc += 3;							}
	if (f2 & TR2_RES_FEAR)		{	dif_inc += 3;							}
	if (f2 & TR2_RES_HALLU)		{	dif_inc += 3;							}

	// Penalty Flags
	if (f2 & TR2_FEAR)			{	dif_dec += 0;	}
	if (f2 & TR2_HUNGER)		{	dif_dec += 0;	}
	if (f2 & TR2_DARKNESS)		{	dif_dec += 0;	}
	if (f2 & TR2_DANGER)		{	dif_dec += 5;	} // only Danger counts
	if (f2 & TR2_AGGRAVATE)		{	dif_dec += 0;	}
	if (f2 & TR2_HAUNTED)		{	dif_dec += 0;	}
	if (f2 & TR2_VUL_COLD)		{	dif_dec += 0;	}
	if (f2 & TR2_VUL_FIRE)		{	dif_dec += 0;	}
	if (f2 & TR2_VUL_POIS)		{	dif_dec += 0;	}
	

	// Abilities
	for (i = 0; i < o_ptr->abilities; i++)
	{
		dif_inc += 5 + (&b_info[ability_index(o_ptr->skilltype[i],o_ptr->abilitynum[i])])->level / 2;
		smithing_cost.exp += 500;
	}
	
	// Mithirl
	if (k_ptr->flags3 & TR3_MITHRIL)	{	smithing_cost.mithril += o_ptr->weight;	}
	
	// Penalty for being an artefact
	if (o_ptr->name1)					{	smithing_cost.uses += 2;	}
	
	// Cap the difficulty reduction at 8
	if (dif_dec > 8) dif_dec = 8;
	
	// Set the overall difficulty
	dif = dif_inc - dif_dec;
	
	// Increased difficulties for minor slots
	switch (wield_slot(o_ptr))
	{
		//case INVEN_WIELD:
		//case INVEN_BOW:
		case INVEN_LEFT:
		case INVEN_RIGHT:
		//case INVEN_NECK:
		case INVEN_LITE:
		//case INVEN_BODY:
		case INVEN_OUTER:
		//case INVEN_ARM:
		//case INVEN_HEAD:
		case INVEN_HANDS:
		case INVEN_FEET:
		case INVEN_QUIVER1:
		case INVEN_QUIVER2:
		{
			dif_mult += 20;
			break;
		}
	}
    
	// Decreased difficulties for easily enchatable items
	if (k_ptr->flags3 & (TR3_ENCHANTABLE))
	{
		dif_mult -= 20;
	}
	
	// Apply the difficulty multiplier
	dif = dif * dif_mult / 100;

    // Artefact arrows are much easier
    if ((o_ptr->tval == TV_ARROW) && (o_ptr->number == 1)) dif /= 2;
        
	// Deal with masterpiece
	if ((dif > p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px)) && p_ptr->active_ability[S_SMT][SMT_MASTERPIECE])
	{
		smithing_cost.drain += dif - (p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
	}

    // determine which additional smithing abilities would be required
    for (i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].tval == smith_o_ptr->tval) cat = smithing_tvals[i].category;
    }
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
    {
		smithing_cost.weaponsmith = 1;
    }
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
    {
		smithing_cost.armoursmith = 1;
    }
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
    {
		smithing_cost.jeweller = 1;
    }
    if (smith_o_ptr->name1 && !p_ptr->active_ability[S_SMT][SMT_ARTEFACT])
    {
		smithing_cost.artifice = 1;
    }
    if (smith_o_ptr->name2 && !p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
    {
		smithing_cost.enchantment = 1;
    }
    if ((att_valid() && (smith_o_ptr->att > att_max(FALSE))) ||
        (ds_valid() && (smith_o_ptr->ds > ds_max(FALSE))) ||
        (evn_valid() && (smith_o_ptr->evn > evn_max(FALSE))) ||
        (ps_valid() && (smith_o_ptr->ps > ps_max(FALSE))))
    {
		smithing_cost.artistry = 1;
    }
    
	return (dif);
}


/*
 * Clears the object's name and description at the bottom of the screen.
 */
void wipe_object_description(void)
{
	int i;
	
	for (i = 0; i < 5; i++)
	{
		Term_putstr(1, MAX_SMITHING_TVALS + 3 + i, -1, TERM_WHITE, "                                                                           ");
	}
}


/*
 * Displays the object's name and description at the bottom of the screen.
 */
void prt_object_description(void)
{
	char o_desc[80];
	char buf[80];
	int display_flag;

	wipe_object_description();

	if (p_ptr->smithing_leftover)
	{
		Term_putstr(COL_SMT1, MAX_SMITHING_TVALS + 3, -1, TERM_L_BLUE, "In progress:");
		sprintf(buf, "%3d turns left", p_ptr->smithing_leftover);
		Term_putstr(COL_SMT1-1, MAX_SMITHING_TVALS + 5, -1, TERM_BLUE, buf);
	}

	// abort if there is no object to display
	if (smith_o_ptr->tval == 0) return;

	if (smith_o_ptr->number > 1)		display_flag = TRUE;
	else								display_flag = FALSE;
	
	object_desc(o_desc, sizeof(o_desc), smith_o_ptr, display_flag, 2);
	
	my_strcat(o_desc, format("   %d.%d lb", smith_o_ptr->weight * smith_o_ptr->number / 10, 
	                                        (smith_o_ptr->weight  * smith_o_ptr->number) % 10), sizeof(o_desc));
	
	Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, TERM_L_WHITE, o_desc);
	
	Term_gotoxy(COL_SMT2, MAX_SMITHING_TVALS + 4);
	
	/* Set hooks for character dump */
	object_info_out_flags = object_flags;
	
	/* Set the indent/wrap */
	text_out_indent = COL_SMT2;
	text_out_wrap = 79;
	
	text_out_hook = text_out_to_screen;
	
	text_out_c(TERM_WHITE, k_text + k_info[smith_o_ptr->k_idx].text);
	
	if ((k_text + k_info[smith_o_ptr->k_idx].text)[0] != '\0') text_out(" ");
	
	/* Dump the info */
	if (object_info_out(smith_o_ptr))
		text_out("\n");
	
	/* Reset indent/wrap */
	text_out_indent = 0;
	text_out_wrap = 0;
}


/*
 * Determines whether an item is too difficult to make.
 */
int too_difficult(object_type *o_ptr)
{
	int ability = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);
	int dif = object_difficulty(o_ptr);
	
	if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) ability += p_ptr->skill_base[S_SMT];
	
	if (ability < dif)	return (TRUE);
	else				return (FALSE);
}


/*
 * Displays the object's difficulty and costs in the right hand side of the screen.
 */
void prt_object_difficulty(void)
{
	int dif;
	char buf[80];
	int costs = 0;
	byte attr;
	bool affordable = TRUE;
    
	Term_putstr(COL_SMT4, 3, -1, TERM_WHITE, "                 ");
	
	// abort if there is no object to display
	if (smith_o_ptr->tval == 0) return;
	
	// display difficulty information
	if (too_difficult(smith_o_ptr)) attr = TERM_L_DARK;
	else							attr = TERM_SLATE;
	
	Term_putstr(COL_SMT4,     2, -1, attr, "Difficulty:");

	// change colour if smithing drain is required
	if ((smithing_cost.drain > 0) && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
	{
		attr = TERM_BLUE;
	}

	// calculate difficulty (and costs)
	dif = object_difficulty(smith_o_ptr);
		
	sprintf(buf, "%d", dif);
	Term_putstr(COL_SMT4 + 2, 4, -1, attr, buf);

	sprintf(buf, "(max %d)", p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
	Term_putstr(COL_SMT4 + 5, 4, -1, TERM_L_DARK, buf);
	
	// display cost information
    if (smithing_cost.weaponsmith)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Weaponsmith");
        costs++;
    }
    if (smithing_cost.armoursmith)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Armoursmith");
        costs++;
    }
    if (smithing_cost.jeweller)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Jeweller");
        costs++;
    }
    if (smithing_cost.enchantment)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Enchantment");
        costs++;
    }
    if (smithing_cost.artistry)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Artistry");
        costs++;
    }
    if (smithing_cost.artifice)
    {
        Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_RED, "Artifice");
        costs++;
    }
	if (smithing_cost.uses > 0)
	{
		if (forge_uses(p_ptr->py,p_ptr->px) >= smithing_cost.uses)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}		
		if (smithing_cost.uses == 1)
		{
			sprintf(buf, "%d Use", smithing_cost.uses);
		}
		else
		{
			sprintf(buf, "%d Uses", smithing_cost.uses);
		}
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		
		sprintf(buf, "(of %d)", forge_uses(p_ptr->py,p_ptr->px));
		Term_putstr(COL_SMT4 + 9, 10 + costs, -1, TERM_L_DARK, buf);
		costs++;
	}
	if (smithing_cost.drain > 0)
	{
		if (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
		{
			attr = TERM_BLUE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}													
		sprintf(buf, "%d Smithing", smithing_cost.drain);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	if (smithing_cost.mithril > 0)
	{
		if (smithing_cost.mithril <= mithril_carried())
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}													
		sprintf(buf, "%d.%d lb Mithril", smithing_cost.mithril / 10, smithing_cost.mithril % 10);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	if (smithing_cost.str > 0)
	{
		if (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR] - smithing_cost.str >= -5)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}													
		sprintf(buf, "%d Str", smithing_cost.str);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	if (smithing_cost.dex > 0)
	{
		if (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX] - smithing_cost.dex >= -5)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}													
		sprintf(buf, "%d Dex", smithing_cost.dex);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	if (smithing_cost.con > 0)
	{
		if (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON] - smithing_cost.con >= -5)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}															
		sprintf(buf, "%d Con", smithing_cost.con);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	if (smithing_cost.gra > 0)
	{
		if (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA] - smithing_cost.gra >= -5)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}			
		sprintf(buf, "%d Gra", smithing_cost.gra);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	if (smithing_cost.exp > 0)
	{
		if (p_ptr->new_exp >= smithing_cost.exp)
		{
			attr = TERM_SLATE;
		}
		else
		{
			attr = TERM_L_DARK;
			affordable = FALSE;
		}		
		sprintf(buf, "%d Exp", smithing_cost.exp);
		Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
		costs++;
	}
	
	attr = TERM_SLATE;
	sprintf(buf, "%d Turns", MAX(10, dif * 10));
	Term_putstr(COL_SMT4 + 2, 10 + costs, -1, attr, buf);
	costs++;
	
	//if (costs == 0)
	//{
	//	Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_SLATE, "-");
	//}
	
	// display cost title
	if (affordable)	attr = TERM_SLATE;
	else	attr = TERM_L_DARK;
	Term_putstr(COL_SMT4,     8, -1, attr, "Cost:");

}


/*
 * Checks whether you can pay the costs in terms of ability points and
 * experience needed to make the object.
 */
bool affordable(object_type *o_ptr)
{
	bool can_afford = TRUE;
	
	// can't afford non-existant items
	if (o_ptr->tval == 0) return (FALSE);
	
	if (too_difficult(o_ptr)) can_afford = FALSE;
	if ((smithing_cost.str > 0) && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR] - smithing_cost.str < -5)) can_afford = FALSE;
	if ((smithing_cost.dex > 0) && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX] - smithing_cost.dex < -5)) can_afford = FALSE;
	if ((smithing_cost.con > 0) && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON] - smithing_cost.con < -5)) can_afford = FALSE;
	if ((smithing_cost.gra > 0) && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA] - smithing_cost.gra < -5)) can_afford = FALSE;
	if (smithing_cost.exp > p_ptr->new_exp) can_afford = FALSE;
	if ((smithing_cost.mithril > 0) && (smithing_cost.mithril > mithril_carried())) can_afford = FALSE;
	if (forge_uses(p_ptr->py,p_ptr->px) < smithing_cost.uses) can_afford = FALSE;
    
    if (smithing_cost.weaponsmith || smithing_cost.armoursmith || smithing_cost.jeweller ||
        smithing_cost.enchantment || smithing_cost.artistry || smithing_cost.artifice) can_afford = FALSE;

	return (can_afford);
}


/*
 * Pay the costs in terms of ability points and experience needed to make the object.
 */
void pay_costs()
{
	if (smithing_cost.str > 0) p_ptr->stat_drain[A_STR] -= smithing_cost.str;
	if (smithing_cost.dex > 0) p_ptr->stat_drain[A_DEX] -= smithing_cost.dex;
	if (smithing_cost.con > 0) p_ptr->stat_drain[A_CON] -= smithing_cost.con;
	if (smithing_cost.gra > 0) p_ptr->stat_drain[A_GRA] -= smithing_cost.gra;
	
	if (smithing_cost.exp > 0) p_ptr->new_exp -= smithing_cost.exp;
	if (smithing_cost.mithril > 0) use_mithril(smithing_cost.mithril);	
	if (smithing_cost.uses > 0) cave_feat[p_ptr->py][p_ptr->px] -= smithing_cost.uses;
	if (smithing_cost.drain > 0) p_ptr->skill_base[S_SMT] -= smithing_cost.drain;

	/* Calculate the bonuses */
	p_ptr->update |= (PU_BONUS);
	
	/* Set the redraw flag for everything */
	p_ptr->redraw |= (PR_EXP | PR_BASIC);	
}


/*
 * Creates the base object (not in the dungeon, but just as a work in progress).
 */
void create_base_object(int tval, int sval)
{
	/* Wipe the object */
	object_wipe(smith_o_ptr);
	
	/* Prepare the item */
	object_prep(smith_o_ptr, lookup_kind(tval, sval));
	
	// set the pval to 1 if needed (and evasion/accuracy for rings)
	apply_magic_fake(smith_o_ptr);
	
	// use a default weight
	smith_o_ptr->weight = (&k_info[smith_o_ptr->k_idx])->weight;
	
	// display all attributes
	smith_o_ptr->ident |= (IDENT_KNOWN | IDENT_SPOIL);

	// create arrows by the two dozen
	if (tval == TV_ARROW)
	{
		smith_o_ptr->number = 24;
	}
}


/*
 * Performs the interface and selection work for the sval part of the base item menu.
 */
int create_sval_menu_aux(int tval, int *highlight)
{
	char ch;
	int i, num;
	char buf[80];
	bool valid[20];
	int sval[20];
		
	// clear the right of the screen
	wipe_screen_from(COL_SMT4);

	/* We have to search the whole itemlist. */
	for (num = 0, i = 1; i < z_info->k_max; i++)
	{
		object_kind *k_ptr = &k_info[i];
		char name[80];
		
		/* Analyze matching items */
		if (k_ptr->tval == tval)
		{
			/* Skip instant artefact item types */
			if (k_ptr->flags3 & (TR3_INSTA_ART)) continue;

			/* Skip certain item types that cannot be made */
			if (k_ptr->flags3 & (TR3_NO_SMITHING)) continue;
			
			/* Get the "name" of object "i" */
			strip_name(name, i);
			
			// make a simple version of the object
			create_base_object(tval, k_ptr->sval);

			// Check whether it is a valid choice for creating
			if (affordable(smith_o_ptr))	valid[num] = TRUE;
			else							valid[num] = FALSE;
			
			/* Print it */
			strnfmt(buf, 80, "%c) %s", (char) 'a' + num, name);
			Term_putstr(COL_SMT3, num + 2, -1, valid[num] ? TERM_WHITE : TERM_SLATE, buf);
			
			/* Remember the object sval */
			sval[num] = k_ptr->sval;
			
			// count the applicable items
			num++;
		}
	}
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);
		
	// make a simple version of the object
	create_base_object(tval, sval[*highlight - 1]);
	
	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	if ((ch >= 'a') && (ch <= (char) 'a' + MAX_SMITHING_TVALS - 1))
	{
		*highlight = (int) ch - 'a' + 1;
		
		// make a simple version of the object
		create_base_object(tval, sval[*highlight - 1]);
		
		return (*highlight);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	if ((ch == '4') || (ch == ESCAPE))
	{
		*highlight = -1;
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = num;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < num) (*highlight)++;
		else if (*highlight == num) *highlight = 1;
	}
	
	return (0);
} 


/*
 * Displays a menu for choosing a base item's sval.
 */
bool create_sval_menu(int tval)
{
	int choice = -1;
	int highlight = 1;
		
	bool leave_menu = FALSE;
	bool completed = FALSE;
	
	/* Save screen */
	screen_save();
	
	/* Process Events until "Return to Game" is selected */
	while (!leave_menu)
	{
		choice = create_sval_menu_aux(tval, &highlight);
		
		if (choice >= 1)
		{
			leave_menu = TRUE;
			completed = TRUE;
		}
		else if (choice == -1)
		{
			/* Wipe the object */
			object_wipe(smith_o_ptr);
			
			leave_menu = TRUE;
		}
	}
	
	/* Load screen */
	screen_load();
	
	return (completed);
}


/*
 * Performs the interface and selection work for the tval part of the base item menu.
 */
int create_tval_menu_aux(int *highlight)
{
	char ch;
	int i;
	char buf[80];
	bool valid[MAX_SMITHING_TVALS];
    byte valid_attr = TERM_WHITE; // default to soothe compilation warnings
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT2);

	// clear bottom of the screen
	wipe_object_description();

	/* Wipe the smithing object */
	object_wipe(smith_o_ptr);

	for (i = 0; i < MAX_SMITHING_TVALS; i++)
	{
		strnfmt(buf, 80, "%c) %s", (char) 'a' + i, smithing_tvals[i].desc);
        
		if (smithing_tvals[i].category == CAT_WEAPON)
		{
			valid[i] = TRUE;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH] ? TERM_WHITE : TERM_RED;
		}
		if (smithing_tvals[i].category == CAT_ARMOUR)
		{
			valid[i] = TRUE;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH] ? TERM_WHITE : TERM_RED;
		}
		if (smithing_tvals[i].category == CAT_JEWELRY)
		{
			valid[i] = TRUE;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_JEWELLER] ? TERM_WHITE : TERM_RED;
		}
		
		Term_putstr(COL_SMT2, i + 2, -1, valid[i] ? valid_attr : TERM_L_DARK, buf);
	}

	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	// choose an option by letter
	if ((ch >= 'a') && (ch <= (char) 'a' + MAX_SMITHING_TVALS - 1))
	{
		int old_highlight = *highlight;
	
		*highlight = (int) ch - 'a' + 1;
		
		// move the light blue highlight
		move_displayed_highlight(old_highlight, valid[old_highlight] ? TERM_WHITE : TERM_L_DARK, *highlight, COL_SMT2);
		
		if (valid[*highlight-1])	return (*highlight);
		else						bell("Invalid choice.");
	}
		
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		if (valid[*highlight-1])	return (*highlight);
		else						bell("Invalid choice.");
	}

	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = MAX_SMITHING_TVALS;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < MAX_SMITHING_TVALS) (*highlight)++;
		else if (*highlight == MAX_SMITHING_TVALS) *highlight = 1;
	}

	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		return (-1);
	}
		
	return (0);
} 


/*
 * Displays a menu for choosing a base item's tval.
 */
void create_tval_menu(void)
{
	int choice = -1;
	int highlight = 1;
	
	bool leave_menu = FALSE;
	
	/* Save screen */
	screen_save();
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = create_tval_menu_aux(&highlight);
		
		if (choice >= 1)
		{
			if (create_sval_menu(smithing_tvals[choice-1].tval))
			{
				leave_menu = TRUE;
			}
		}
		else if (choice == -1)
		{
			leave_menu = TRUE;
		}
	}
	
    enchant_then_numbers = FALSE;
    
	/* Load screen */
	screen_load();
}


/*
 * Actually modifies the numbers on an item.
 */
void modify_numbers(int choice)
{
	switch (choice)
	{
		case SMT_NUM_MENU_I_ATT:
		{
			if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)	smith_o_ptr->att += 3;
			else														smith_o_ptr->att++;
			break;
		}
		case SMT_NUM_MENU_D_ATT:
		{
			if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)	smith_o_ptr->att -= 3;
			else														smith_o_ptr->att--;
			break;
		}
		case SMT_NUM_MENU_I_DS:		smith_o_ptr->ds++;		break;
		case SMT_NUM_MENU_D_DS:		smith_o_ptr->ds--;		break;
		case SMT_NUM_MENU_I_EVN:	smith_o_ptr->evn++;		break;
		case SMT_NUM_MENU_D_EVN:	smith_o_ptr->evn--;		break;
		case SMT_NUM_MENU_I_PS:		smith_o_ptr->ps++;		break;
		case SMT_NUM_MENU_D_PS:		smith_o_ptr->ps--;		break;
		case SMT_NUM_MENU_I_PVAL:	smith_o_ptr->pval++;	break;
		case SMT_NUM_MENU_D_PVAL:	smith_o_ptr->pval--;	break;
		case SMT_NUM_MENU_I_WGT:	smith_o_ptr->weight += 5;	break;
		case SMT_NUM_MENU_D_WGT:	smith_o_ptr->weight -= 5;	break;
	}
	
	return;
}


/*
 * Performs the interface and selection work for the numbers menu.
 */
int numbers_menu_aux(int *highlight)
{
	int i;
	char ch;
	char buf[80];
	byte attr[SMT_NUM_MENU_MAX];
	bool valid[SMT_NUM_MENU_MAX];
	bool can_afford[SMT_NUM_MENU_MAX];
    bool needs_art[SMT_NUM_MENU_MAX] = {FALSE};
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT2);
	
	valid[SMT_NUM_MENU_I_ATT-1]     = att_valid() && (smith_o_ptr->att < att_max(TRUE));
    needs_art[SMT_NUM_MENU_I_ATT-1] = att_valid() && !(smith_o_ptr->att < att_max(FALSE));
	valid[SMT_NUM_MENU_D_ATT-1]     = att_valid() && (smith_o_ptr->att > att_min());
	valid[SMT_NUM_MENU_I_DS-1]      = ds_valid() && (smith_o_ptr->ds < ds_max(TRUE));
    needs_art[SMT_NUM_MENU_I_DS-1]  = ds_valid() && !(smith_o_ptr->ds < ds_max(FALSE));
	valid[SMT_NUM_MENU_D_DS-1]      = ds_valid() && (smith_o_ptr->ds > ds_min());
	valid[SMT_NUM_MENU_I_EVN-1]     = evn_valid() && (smith_o_ptr->evn < evn_max(TRUE));
    needs_art[SMT_NUM_MENU_I_EVN-1]  = evn_valid() && !(smith_o_ptr->evn < evn_max(FALSE));
	valid[SMT_NUM_MENU_D_EVN-1]     = evn_valid() && (smith_o_ptr->evn > evn_min());
	valid[SMT_NUM_MENU_I_PS-1]      = ps_valid() && (smith_o_ptr->ps < ps_max(TRUE));
    needs_art[SMT_NUM_MENU_I_PS-1]  = ps_valid() && !(smith_o_ptr->ps < ps_max(FALSE));
	valid[SMT_NUM_MENU_D_PS-1]      = ps_valid() && (smith_o_ptr->ps > ps_min());
	valid[SMT_NUM_MENU_I_PVAL-1]    = pval_valid() && (smith_o_ptr->pval < pval_max());
	valid[SMT_NUM_MENU_D_PVAL-1]    = pval_valid() && (smith_o_ptr->pval > pval_min());
	valid[SMT_NUM_MENU_I_WGT-1]     = wgt_valid() && ((smith_o_ptr->weight + 5) <= wgt_max());
	valid[SMT_NUM_MENU_D_WGT-1]     = wgt_valid() && ((smith_o_ptr->weight - 5) >= wgt_min());
	
	// retrieve a super backup of the object
	object_copy(smith3_o_ptr, smith_o_ptr);
	for (i = 0; i < SMT_NUM_MENU_MAX; i++)
	{
		if (valid[i])
		{
			modify_numbers(i+1);
			can_afford[i] = affordable(smith_o_ptr);

			// retrieve a super backup of the object
			object_copy(smith_o_ptr, smith3_o_ptr);
		}
		
		attr[i] = valid[i] ? (needs_art[i] ? TERM_RED : (can_afford[i] ? TERM_WHITE : TERM_SLATE)) : TERM_L_DARK;
	}
	
	Term_putstr(COL_SMT2,  2, -1, attr[SMT_NUM_MENU_I_ATT-1],  "a) increase attack bonus");
	Term_putstr(COL_SMT2,  3, -1, attr[SMT_NUM_MENU_D_ATT-1],  "b) decrease attack bonus");
	Term_putstr(COL_SMT2,  4, -1, attr[SMT_NUM_MENU_I_DS-1],   "c) increase damage sides");
	Term_putstr(COL_SMT2,  5, -1, attr[SMT_NUM_MENU_D_DS-1],   "d) decrease damage sides");
	Term_putstr(COL_SMT2,  6, -1, attr[SMT_NUM_MENU_I_EVN-1],  "e) increase evasion bonus");
	Term_putstr(COL_SMT2,  7, -1, attr[SMT_NUM_MENU_D_EVN-1],  "f) decrease evasion bonus");
	Term_putstr(COL_SMT2,  8, -1, attr[SMT_NUM_MENU_I_PS-1],   "g) increase protection sides");
	Term_putstr(COL_SMT2,  9, -1, attr[SMT_NUM_MENU_D_PS-1],   "h) decrease protection sides");
	Term_putstr(COL_SMT2, 10, -1, attr[SMT_NUM_MENU_I_PVAL-1], "i) increase special bonus");
	Term_putstr(COL_SMT2, 11, -1, attr[SMT_NUM_MENU_D_PVAL-1], "j) decrease special bonus");
	Term_putstr(COL_SMT2, 12, -1, attr[SMT_NUM_MENU_I_WGT-1],  "k) increase weight");
	Term_putstr(COL_SMT2, 13, -1, attr[SMT_NUM_MENU_D_WGT-1],  "l) decrease weight");
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);
	
	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(2, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	// choose an option by letter
	if ((ch >= 'a') && (ch <= (char) 'a' + SMT_NUM_MENU_MAX - 1))
	{
		int old_highlight = *highlight;
		
		*highlight = (int) ch - 'a' + 1;
		
		// move the light blue highlight
		move_displayed_highlight(old_highlight, attr[old_highlight], *highlight, COL_SMT2);
		
		if (valid[*highlight-1])	return (*highlight);
		else						bell("Invalid choice.");
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		if (valid[*highlight-1])	return (*highlight);
		else						bell("Invalid choice.");
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = SMT_NUM_MENU_MAX;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < SMT_NUM_MENU_MAX) (*highlight)++;
		else if (*highlight == SMT_NUM_MENU_MAX) *highlight = 1;
	}
	
	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		*highlight = -1;
		return (*highlight);
	}
	
	return (0);
} 


/*
 * Displays a menu for modifying numerical bonuses and weight of an item.
 */
void numbers_menu(void)
{
	int choice = -1;
	int highlight = 1;
	
	bool leave_menu = FALSE;
	
    if (smith_o_ptr->name2) enchant_then_numbers = TRUE;
    
	/* Save screen */
	screen_save();
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = numbers_menu_aux(&highlight);
		
		switch (choice)
		{
			case -1:
			{
				leave_menu = TRUE;
				break;
			}
			
			default:
			{
				modify_numbers(choice);
				break;
			}
		}
	}
	
	/* Load screen */
	screen_load();
	
	return;
}


void create_special(int name2)
{
	// retrieve a backup of the object
	object_copy(smith_o_ptr, smith2_o_ptr);
	
	// set its 'special' name to reflect the chosen type
	smith_o_ptr->name2 = name2;
	
	// make it into that special type
	object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], TRUE);
}


/*
 * Performs the interface and selection work for the enchantment menu.
 */
int enchant_menu_aux(int *highlight)
{
	char ch;
	int i, j, num;
	char buf[80];
	bool valid[20];
	int choice[20];
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT2);
	
	/* We have to search the whole special item list. */
	for (num = 0, i = 1; i < z_info->e_max; i++)
	{
		ego_item_type *e_ptr = &e_info[i];
		bool acceptable = FALSE;
		
		/* Don't create cursed */
		//if (e_ptr->flags3 & TR3_LIGHT_CURSE) continue;
		
		/* Don't create useless */
		//if (e_ptr->cost == 0) continue;
		
		/* Test if this is a legal special item type for this object */
		for (j = 0; j < EGO_TVALS_MAX; j++)
		{
			/* Require identical base type */
			if (smith_o_ptr->tval == e_ptr->tval[j])
			{
				/* Require sval in bounds, lower */
				if (smith_o_ptr->sval >= e_ptr->min_sval[j])
				{
					/* Require sval in bounds, upper */
					if (smith_o_ptr->sval <= e_ptr->max_sval[j])
					{
						/* Accept */
						acceptable = TRUE;
					}
				}
			}
		}
		
		if (acceptable)
		{
			// make a 'special' version of the object
			create_special(i);

			// Check whether it is a valid choice for creating
			if (affordable(smith_o_ptr))
			{
				valid[num] = TRUE;
			}
			else
			{
				valid[num] = FALSE;
			}
			
			/* Print it */
			strnfmt(buf, 80, "%c) %s", (char) 'a' + num, e_name + e_ptr->name);
			Term_putstr(COL_SMT2, num + 2, -1, valid[num] ? TERM_WHITE : TERM_SLATE, buf);
			
			/* Remember the object index */
			choice[num] = i;
			
			// count the applicable items
			num++;
		}
	}
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);
	
	// make a 'special' version of the object
	create_special(choice[*highlight-1]);
	
	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	/* Choose by letter */
	if ((ch >= 'a') && (ch <= (char) 'a' + num - 1))
	{
		*highlight = (int) ch - 'a' + 1;

		// make a 'special' version of the object
		create_special(choice[*highlight-1]);
		
		return (*highlight);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		*highlight = -1;
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = num;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < num) (*highlight)++;
		else if (*highlight == num) *highlight = 1;
	}
	
	return (0);
} 


/*
 * Brings up a menu for making an item into a {special} item.
 */
bool enchant_menu(void)
{
	int choice = -1;
	int highlight = 1;
	
	bool leave_menu = FALSE;
	bool completed = FALSE;
    
	/* Save screen */
	screen_save();

	// stop the item being an artefact, if it was
	smith_o_ptr->name1 = 0;
	smith2_o_ptr->name1 = 0;
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = enchant_menu_aux(&highlight);
		
		if (choice >= 1)
		{
			leave_menu = TRUE;
			completed = TRUE;
		}
		else if (choice == -1)
		{
			leave_menu = TRUE;
		}
	}
	
	/* Load screen */
	screen_load();
	
	return (completed);
}


/*
 * Copies an artefact structure over the top of another one.
 */
void artefact_copy(artefact_type *a1_ptr, artefact_type *a2_ptr)
{
	/* Copy the structure */
	COPY(a1_ptr, a2_ptr, artefact_type);
}


/*
 * Fills in the details on the artefact type being created.
 */
void add_artefact_details(void)
{
	smith_a_ptr->tval = smith_o_ptr->tval;
	smith_a_ptr->sval = smith_o_ptr->sval;
	smith_a_ptr->pval = smith_o_ptr->pval;
	smith_a_ptr->att = smith_o_ptr->att;
	smith_a_ptr->evn = smith_o_ptr->evn;
	smith_a_ptr->dd = smith_o_ptr->dd;
	smith_a_ptr->ds = smith_o_ptr->ds;
	smith_a_ptr->pd = smith_o_ptr->pd;
	smith_a_ptr->ps = smith_o_ptr->ps;
	smith_a_ptr->weight = smith_o_ptr->weight;
	smith_a_ptr->flags1 |= (&k_info[smith_o_ptr->k_idx])->flags1;
	smith_a_ptr->flags2 |= (&k_info[smith_o_ptr->k_idx])->flags2;
	smith_a_ptr->flags3 |= (&k_info[smith_o_ptr->k_idx])->flags3;
	smith_a_ptr->cur_num = 1;
	smith_a_ptr->found_num = 1;
	smith_a_ptr->max_num = 1;
	smith_a_ptr->level = object_difficulty(smith_o_ptr);
	smith_a_ptr->rarity = 10;	
}


/*
 * Prepares an artefact for modification.
 */
void prepare_artefact(void)
{
    int i;
    
	// retrieve a backup of the artefact
	artefact_copy(smith_a_ptr, smith2_a_ptr);
	
	// retrieve a backup of the object
	object_copy(smith_o_ptr, smith2_o_ptr);

	// set its 'artefact' name to reflect the chosen type
	smith_o_ptr->name1 = smith_a_name;
	
	// make sure there is only one of the item (needed for arrows)
	smith_o_ptr->number = 1;

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}


/*
 * Does the given object type support the given flag type?
 */
bool applicable_flag(u32b f, int flagset, object_type *o_ptr)
{
	bool ok = FALSE;
	int i;
	u32b f1, f2, f3;
	
	/* Extract the object flags */
	object_flags(o_ptr, &f1, &f2, &f3);

	/* Go through the list of artefacts and see if the flag is applicable for this type  */
	for (i = ART_ULTIMATE; i < z_info->art_norm_max; i++)
	{
		/* Access the artefact */
		artefact_type *a_ptr = &a_info[i];
		
		/* Skip other types of artefacts */
		if (a_ptr->tval != o_ptr->tval) continue;
		
		switch (flagset)
		{
			case 1:	{	if (a_ptr->flags1 & f) ok = TRUE; break;	}
			case 2:	{	if (a_ptr->flags2 & f) ok = TRUE; break;	}
			case 3:	{	if (a_ptr->flags3 & f) ok = TRUE; break;	}
		}
	}
	
	// Smithing is OK for War Hammers
	if ((o_ptr->tval == TV_HAFTED) && (o_ptr->sval == SV_WAR_HAMMER))
	{
		if ((flagset == 1) && (f & (TR1_SMT))) ok = TRUE;
	}
	
	// Special case for brands
	//if ((flagset == 1) && (f & (TR1_BRAND_MASK)))
	//{
	//	// If the object doesn't already have the flag, but it does have a brand, then disallow it
	//	if (!(f1 & f) && (f1 & (TR1_BRAND_MASK)))
	//	{
	//		ok = FALSE;
	//	}
	//}
	
	return (ok);
}


/*
 * Adds a given flag to the dummy artefact.
 */
void add_artefact_flag(u32b f, int flagset)
{	
	// prepare the artefact and object for modification
	prepare_artefact();
	
	// set new flag on the artefact
	if (flagset == 1)	smith_a_ptr->flags1 |= f;
	if (flagset == 2)	smith_a_ptr->flags2 |= f;
	if (flagset == 3)	smith_a_ptr->flags3 |= f;
}


/*
 * Removes a given flag from the dummy artefact.
 */
void remove_artefact_flag(u32b f, int flagset)
{
	// prepare the artefact and object for modification
	prepare_artefact();
	
	// unset new flag on the artefact
	if (flagset == 1)	smith_a_ptr->flags1 &= ~(f);
	if (flagset == 2)	smith_a_ptr->flags2 &= ~(f);
	if (flagset == 3)	smith_a_ptr->flags3 &= ~(f);
}


/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_flag_menu_aux(int category, int *highlight)
{
	char ch;
	int i, num = 0;
	char buf[80];
	bool flag_present[MAX_SMITHING_FLAGS]    = {FALSE};
	bool flag_valid[MAX_SMITHING_FLAGS]      = {FALSE};
	bool flag_affordable[MAX_SMITHING_FLAGS] = {FALSE};
	u32b flag[MAX_SMITHING_FLAGS];
	int flagset[MAX_SMITHING_FLAGS];
	byte attr;
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT3);
	
	// display the categories
	for (i = 0; smithing_flag_types[i].flag != 0; i++)
	{
		if (category == smithing_flag_types[i].category)
		{
			flag[num] = smithing_flag_types[i].flag;
			flagset[num] = smithing_flag_types[i].flagset;

			if (((flagset[num] == 1) && (smith2_a_ptr->flags1 & flag[num])) ||
			    ((flagset[num] == 2) && (smith2_a_ptr->flags2 & flag[num])) ||
				((flagset[num] == 3) && (smith2_a_ptr->flags3 & flag[num])))
			{
				flag_present[num] = TRUE;
				flag_valid[num] = TRUE;
			}

			else
			{
				// require that the flag can be present on the object
				if (applicable_flag(flag[num], flagset[num], smith_o_ptr))
				{
					flag_valid[num] = TRUE;
					
					// add this flag to the dummy artefact under construction
					add_artefact_flag(flag[num], flagset[num]);
					
					// Check whether it is a valid choice for creating (needs to be affordable and successful)
					if (affordable(smith_o_ptr))
					{
						flag_affordable[num] = TRUE;
					}
				}
			}
			
			attr = flag_present[num] ? TERM_BLUE : (flag_valid[num] ? (flag_affordable[num] ? TERM_WHITE : TERM_SLATE) : TERM_L_DARK);
			
			/* Display the line */
			strnfmt(buf, 80, "%c) %s", (char) 'a' + num, smithing_flag_types[i].desc);
			Term_putstr(COL_SMT3, num + 2, -1, attr, buf);

			num++;
		}
	}
		
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);

	// add this flag to the dummy artefact under construction
	add_artefact_flag(flag[*highlight-1], flagset[*highlight-1]);

	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	/* Abort if there are no choices */
	if (num == 0)
	{
		return (-1);
	}
	
	/* Choose by letter */
	if ((ch >= 'a') && (ch <= (char) 'a' + num - 1))
	{
		if (flag_valid[*highlight-1])
		{
			if ((int) ch - 'a' + 1 == *highlight)
			{
				// remove a flag if it already existed
				if (flag_present[*highlight-1])	remove_artefact_flag(flag[*highlight-1], flagset[*highlight-1]);
			}
			else
			{
				// restore the artefact from backup
				artefact_copy(smith_a_ptr, smith2_a_ptr);
				
				*highlight = (int) ch - 'a' + 1;		
				
				// remove a flag if it already existed
				if (flag_present[*highlight-1])	remove_artefact_flag(flag[*highlight-1], flagset[*highlight-1]);
				
				// otherwise add it
				else							add_artefact_flag(flag[*highlight-1], flagset[*highlight-1]);
			}
			
			// backup the new artefact
			artefact_copy(smith2_a_ptr, smith_a_ptr);
			
			return (*highlight);
		}
		else
		{
			bell("Invalid choice.");
		}
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		if (flag_valid[*highlight-1])
		{
			// remove a flag if it already existed
			if (flag_present[*highlight-1])	remove_artefact_flag(flag[*highlight-1], flagset[*highlight-1]);
			
			// backup the new artefact
			artefact_copy(smith2_a_ptr, smith_a_ptr);
			
			return (*highlight);
		}
		
		else
		{
			bell("Invalid choice.");
		}
	}
	
	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		*highlight = -1;

		// restore the backup artefact
		artefact_copy(smith_a_ptr, smith2_a_ptr);
		
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = num;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < num) (*highlight)++;
		else if (*highlight == num) *highlight = 1;
	}
	
	return (0);
} 


/*
 * Brings up a menu to select individual flags of a given type to 
 * add to (or subtract from) an artefact.
 */
void artefact_flag_menu(int category)
{
	int choice = -1;
	int highlight = 1;
	
	bool leave_menu = FALSE;
	
	/* Save screen */
	screen_save();
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = artefact_flag_menu_aux(category, &highlight);
		
		if (choice >= 1)
		{
			// don't leave the menu
		}
		else if (choice == -1)
		{
			leave_menu = TRUE;
		}
	}
	
	/* Load screen */
	screen_load();
}


/*
 * Does the given object type support the given ability type?
 */
bool applicable_ability(ability_type *b_ptr, object_type *o_ptr)
{
	bool ok = FALSE;
	int j;
	
	u32b f1, f2, f3;
	
	/* Test if this is a legal item type for this ability */
	for (j = 0; j < ABILITY_TVALS_MAX; j++)
	{
		/* Require identical base type */
		if (o_ptr->tval == b_ptr->tval[j])
		{
			/* Require sval in bounds, lower */
			if (o_ptr->sval >= b_ptr->min_sval[j])
			{
				/* Require sval in bounds, upper */
				if (o_ptr->sval <= b_ptr->max_sval[j])
				{
					/* Accept */
					ok = TRUE;
				}
			}
		}
	}
	
	// Throwing Mastery is OK for throwing items
	object_flags(o_ptr, &f1, &f2, &f3);
	if (f3 & TR3_THROWING)
	{
		if ((b_ptr->skilltype == S_MEL) && (b_ptr->abilitynum == MEL_THROWING)) ok = TRUE;
	}

	// Polearm Mastery is OK for Polearms
	object_flags(o_ptr, &f1, &f2, &f3);
	if (f3 & TR3_POLEARM)
	{
		if ((b_ptr->skilltype == S_MEL) && (b_ptr->abilitynum == MEL_POLEARMS)) ok = TRUE;
	}
	
	
	return (ok);
}

/*
 * Adds a given ability to the dummy artefact.
 */
void add_artefact_ability(int skilltype, int abilitynum)
{	
	int i;

	// prepare the artefact and object for modification
	prepare_artefact();
	
	// set new ability on the artefact
	if (smith_a_ptr->abilities < 4)
	{
		bool already_present = FALSE;
		
		for (i = 0; i < smith_a_ptr->abilities; i++)
		{
			if ((smith_a_ptr->skilltype[i] == skilltype) && (smith_a_ptr->abilitynum[i] == abilitynum))
			{
				already_present = TRUE;
			}
		}
		
		if (!already_present)
		{
			smith_a_ptr->skilltype[smith_a_ptr->abilities] = skilltype;
			smith_a_ptr->abilitynum[smith_a_ptr->abilities] = abilitynum;
			smith_a_ptr->abilities++;
		}
	}
	
    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
    
	
}


/*
 * Removes a given ability from the dummy artefact.
 */
void remove_artefact_ability(int skilltype, int abilitynum)
{	
	int i;
	int location = -1;
	
	// prepare the artefact and object for modification
	prepare_artefact();
	
	// remove new ability on the artefact
	for (i = 0; i < smith_a_ptr->abilities; i++)
	{
		if ((smith_a_ptr->skilltype[i] == skilltype) && (smith_a_ptr->abilitynum[i] == abilitynum))
		{
			location = i;
		}
	}
	
	if (location >= 0)
	{
		for (i = location; i < smith_a_ptr->abilities - 1; i++)
		{
			smith_a_ptr->skilltype[i] = smith_a_ptr->skilltype[i+1];
			smith_a_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i+1];
		}
		
		smith_a_ptr->skilltype[smith_a_ptr->abilities-1] = 0;
		smith_a_ptr->abilitynum[smith_a_ptr->abilities-1] = 0;
		
		smith_a_ptr->abilities--;
	}
	
    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
	
}


/*
 * Determines if an artefact type has a given ability.
 */
bool has_ability(artefact_type *a_ptr, int skilltype, int abilitynum)
{
	int i;
	
	for (i = 0; i < a_ptr->abilities; i++)
	{
		if ((a_ptr->skilltype[i] == skilltype) && (a_ptr->abilitynum[i] == abilitynum)) return (TRUE);
	}
	
	return (FALSE);
}


/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_ability_menu_aux(int skill, int *highlight)
{
	char ch;
	int i, num = 0;
	char buf[80];
	bool ability_present[20]    = {FALSE};
	bool ability_valid[20]      = {FALSE};
	bool ability_affordable[20] = {FALSE};
	ability_type *b_ptr;
	ability_type *b2_ptr;
	byte attr;
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT3);
	
	// list the abilities
	for (i = 0; i < z_info->b_max - 1; i++)
	{
		b_ptr = &b_info[i];
		b2_ptr = &b_info[i+1];
		
		/* Skip non-entries */
		if (!b_ptr->name) continue;

		/* Skip entries where the next entry is not defined (to avoid the stat-improvements) */
		if (!b2_ptr->name) continue;
		
		/* Skip entries for the wrong skill type */
		if (b_ptr->skilltype != skill) continue;
		
		// Determine the appropriate colour
		if (has_ability(smith2_a_ptr, skill, num))
		{
			ability_present[num] = TRUE;
			ability_valid[num] = TRUE;
		}
		else
		{
			// require that the ability can be present on the object
			if (applicable_ability(b_ptr, smith_o_ptr))
			{
				ability_valid[num] = TRUE;

				// add this flag to the dummy artefact under construction
				add_artefact_ability(skill, num);
				
				// require that the ability was successfully added
				if (has_ability(smith_a_ptr, skill, num))
				{
					// Check whether it is a valid choice for creating (needs to be affordable and successful)
					if (affordable(smith_o_ptr))
					{
						ability_affordable[num] = TRUE;
					}
				}
				
				// if the ability wasn't added properly (the item had too many), then it is not valid after all
				else
				{
					ability_valid[num] = FALSE;
				}
			}
		}
		
		attr = ability_present[num] ? TERM_BLUE : (ability_valid[num] ? (ability_affordable[num] ? TERM_WHITE : TERM_SLATE) : TERM_L_DARK);
			
		/* Display the line */
		strnfmt(buf, 80, "%c) %s", (char) 'a' + num, b_name + b_ptr->name);
		Term_putstr(COL_SMT3, num + 2, -1, attr, buf);
			
		num++;
	}
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT3, *highlight + 1, -1, TERM_L_BLUE, buf);
	
	// add this ability to the dummy artefact under construction
	add_artefact_ability(skill, *highlight-1);
	
	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	/* Abort if there are no choices */
	if (num == 0)
	{
		return (-1);
	}
	
	/* Choose by letter */
	if ((ch >= 'a') && (ch <= (char) 'a' + num - 1))
	{
		if (ability_valid[*highlight-1])
		{
			if ((int) ch - 'a' + 1 == *highlight)
			{
				// remove an ability if it already existed
				if (ability_present[*highlight-1])	remove_artefact_ability(skill, *highlight-1);
			}
			else
			{
				// restore the artefact from backup
				artefact_copy(smith_a_ptr, smith2_a_ptr);
				
				*highlight = (int) ch - 'a' + 1;		
				
				// remove an ability if it already existed
				if (ability_present[*highlight-1])	remove_artefact_ability(skill, *highlight-1);
				
				// otherwise add it
				else								add_artefact_ability(skill, *highlight-1);
			}
			
			// backup the new artefact
			artefact_copy(smith2_a_ptr, smith_a_ptr);
			
			return (*highlight);
		}
		else
		{
			bell("Invalid choice.");
		}
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		if (ability_valid[*highlight-1])
		{
			// remove an ability if it already existed
			if (ability_present[*highlight-1])	remove_artefact_ability(skill, *highlight-1);
			
			// backup the new artefact
			artefact_copy(smith2_a_ptr, smith_a_ptr);
            
			return (*highlight);
		}
		else
		{
			bell("Invalid choice.");
		}
	}
	
	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		// remove any tentatively-added ability from the object
		if (!ability_present[*highlight-1]) remove_artefact_ability(skill, *highlight-1);

		// restore the backup artefact
		artefact_copy(smith_a_ptr, smith2_a_ptr);
		
		*highlight = -1;
		
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = num;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < num) (*highlight)++;
		else if (*highlight == num) *highlight = 1;
	}
	
	return (0);
} 


/*
 * Brings up a menu to select individual abilities of a given skill to 
 * add to (or subtract from) an artefact.
 */
void artefact_ability_menu(int skill)
{
	int choice = -1;
	int highlight = 1;
	
	bool leave_menu = FALSE;
	
	/* Save screen */
	screen_save();
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = artefact_ability_menu_aux(skill, &highlight);
		
		if (choice >= 1)
		{
			// don't leave the menu
		}
		else if (choice == -1)
		{
			leave_menu = TRUE;
		}
	}
	
	/* Load screen */
	screen_load();
}



/*
 * Allows the player to choose a new name for an artefact.
 */
void rename_artefact(void)
{
	char tmp[20];
	char old_name[20];
	char o_desc[30];
	bool name_selected = FALSE;
	
	// Clear the names
	tmp[0] = '\0';
	old_name[0] = '\0';
		
	// Clear object name
	Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, TERM_L_WHITE, "                                                        ");

	// Determine object name
	object_desc(o_desc, sizeof(o_desc), smith_o_ptr, FALSE, -1);

	// Display shortened object name
	Term_putstr(COL_SMT2, MAX_SMITHING_TVALS + 3, -1, TERM_L_WHITE, o_desc);

	// use old name as a default
	my_strcpy(tmp, smith2_a_ptr->name, sizeof(tmp));
	
	// save a copy too
	my_strcpy(old_name, op_ptr->full_name, sizeof(old_name));
	
	/* Prompt for a new name */
	Term_gotoxy(COL_SMT2 + strlen(o_desc) + 1, MAX_SMITHING_TVALS + 3);
	
	while (!name_selected)
	{
		if (askfor_name(tmp, sizeof(tmp)))
		{
			my_strcpy(smith2_a_ptr->name, tmp, MAX_LEN_ART_NAME);
			p_ptr->redraw |= (PR_MISC);
		}
		else
		{
			my_strcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
			return;
		}
		
		if (tmp[0] != '\0')	name_selected = TRUE;
		else				my_strcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
	}
	
	// retrieve a backup of the artefact (all the modifications were done to this backup copy)
	artefact_copy(smith_a_ptr, smith2_a_ptr);

}


/*
 * Performs the interface and selection work for the 1st level artefact menu.
 */
int artefact_menu_aux(int *highlight)
{
	char ch;
	int i , num;
	char buf[80];
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT2);
	
	// display the categories for flags
	for (i = 0; i < MAX_CATS; i++)
	{
		strnfmt(buf, 80, "%c) %s", (char) 'a' + i, smithing_flag_cats[i].desc);
		Term_putstr(COL_SMT2, i + 2, -1, TERM_WHITE, buf);
	}

	// display the categories for abilities
	for (i = 0; i < S_MAX; i++)
	{
		strnfmt(buf, 80, "%c) %s", (char) 'a' + MAX_CATS + i, skill_names_full[i]);
		Term_putstr(COL_SMT2, i + MAX_CATS + 2, -1, TERM_WHITE, buf);
	}
	
	num = MAX_CATS + S_MAX + 1;
	
	// Menu item for naming artefacts
	strnfmt(buf, 80, "%c) %s", (char) 'a' + num - 1, "Name Artefact");
	Term_putstr(COL_SMT2, num + 1, -1, TERM_WHITE, buf);
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);
	
	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	/* Choose by letter */
	if ((ch >= 'a') && (ch <= (char) 'a' + num - 1))
	{
		int old_highlight = *highlight;
		
		*highlight = (int) ch - 'a' + 1;

		// move the light blue highlight
		move_displayed_highlight(old_highlight, TERM_WHITE, *highlight, COL_SMT2);
		
		return (*highlight);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		*highlight = -1;
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = num;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < num) (*highlight)++;
		else if (*highlight == num) *highlight = 1;
	}
	
	return (0);
} 


/*
 * Brings up a menu for making a base item into an artefact,
 * by adding flags of various types.
 */
void artefact_menu(void)
{
	int choice = -1;
	int highlight = 1;
	
	char buf[30];
	bool leave_menu = FALSE;
	
	/* Save screen */
	screen_save();

	if (!smith_o_ptr->name1)
	{
		// wipe the existing artefact (and its backup)
		artefact_wipe(smith_a_name);
		artefact_wipe(smith2_a_name);

		// add 'ignore all'
		smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

		// change the SV for rings and amulets when they start to get made into artefacts
        if (smith_o_ptr->tval == TV_RING)
        {
            create_base_object(TV_RING, SV_RING_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_o_ptr->pd = 1;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
        }
    }

	// set the backup artefact name to the player character's name
	if (strlen(smith2_a_ptr->name) == 0)
	{
		sprintf(buf, "of %s", op_ptr->full_name);
		my_strcpy(smith2_a_ptr->name, buf, MAX_LEN_ART_NAME);
	}
	
	// prepare the artefact and object for modification
    prepare_artefact();
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = artefact_menu_aux(&highlight);

		if (choice == MAX_CATS + S_MAX + 1)
		{
			rename_artefact();
		}
		else if (choice >= MAX_CATS + 1)
		{
			artefact_ability_menu(choice - MAX_CATS - 1);
		}
		else if (choice >= 1)
		{
			artefact_flag_menu(choice);
		}
		else if (choice == -1)
		{
			leave_menu = TRUE;
		}
	}
		
	/* Load screen */
	screen_load();
	
	return;
}


/*
 * Performs the interface and selection work for the melting menu.
 */
int melt_menu_aux(int *highlight)
{
	char ch;
	int i;
	int num = 0;
	object_type *o_ptr;
	u32b f1, f2, f3;
	char desc[80];
	char buf[80];
	
	// clear the right of the screen
	wipe_screen_from(COL_SMT2);
	
	// clear bottom of the screen
	wipe_object_description();
	
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];
		
		object_flags(o_ptr, &f1, &f2, &f3);
		
		if (f3 & (TR3_MITHRIL))
		{
			object_desc(desc, 80, o_ptr, FALSE, 2);
			strnfmt(buf, 80, "%c) %s", (char) 'a' + num, desc);
			
			Term_putstr(COL_SMT2, num + 2, -1, TERM_WHITE, buf);

			strnfmt(buf, 80, "%2d.%d lb", o_ptr->weight / 10, o_ptr->weight % 10);
			Term_putstr(COL_SMT2 + 40, num + 2, -1, TERM_WHITE, buf);
			
			num++;
		}
	}
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT2, *highlight + 1, -1, TERM_L_BLUE, buf);
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(14, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	// choose an option by letter
	if ((ch >= 'a') && (ch <= (char) 'a' + num - 1))
	{
		int old_highlight = *highlight;
		
		*highlight = (int) ch - 'a' + 1;
		
		// move the light blue highlight
		move_displayed_highlight(old_highlight, TERM_WHITE, *highlight, COL_SMT2);
		
		return (*highlight);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = num;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < num) (*highlight)++;
		else if (*highlight == num) *highlight = 1;
	}
	
	/* Exit */
	if ((ch == '4') || (ch == ESCAPE))
	{
		return (-1);
	}
	
	return (0);
} 


/*
 * Produces the menu for melting down mithril items into pieces of mithril.
 */
void melt_menu(void)
{
	int choice = -1;
	int highlight = 1;
	
	bool leave_menu = FALSE;
	
	/* Save screen */
	screen_save();
	
	/* Process Events until menu is abandoned */
	while (!leave_menu)
	{
		choice = melt_menu_aux(&highlight);
		
		if (choice >= 1)
		{
			if (melt_mithril_item(choice))
			{
				leave_menu = TRUE;
			}
		}
		else if (choice == -1)
		{
			leave_menu = TRUE;
		}
	}
	
	/* Load screen */
	screen_load();
}

/*
 * Performs the interface and selection work for the smithing screen.
 */
int smithing_menu_aux(int *highlight)
{
	char ch;
    byte valid_attr;
	bool valid[SMT_MENU_MAX];
	char buf[80];

    // clear the right of the screen
	wipe_screen_from(COL_SMT2);
    
	// determine whether or not we can actually make objects here
	if (!cave_forge_bold(p_ptr->py, p_ptr->px))
	{
		Term_putstr(COL_SMT1, 0, -1, TERM_L_BLUE, "Exploration mode:  Smithing requires a forge.");
	}
	else if (forge_uses(p_ptr->py, p_ptr->px) == 0)
	{
		Term_putstr(COL_SMT1, 0, -1, TERM_L_BLUE, "Exploration mode:  Smithing requires a forge with resources left.");
	}

	valid[SMT_MENU_CREATE-1]   = TRUE;
	valid[SMT_MENU_ENCHANT-1]  = (!smith_o_ptr->name1) &&
                                 (!enchant_then_numbers) &&
                                 (smith_o_ptr->tval != 0) &&
                                 (smith_o_ptr->tval != TV_RING) &&
                                 (smith_o_ptr->tval != TV_AMULET) &&
                                 (smith_o_ptr->tval != TV_HORN) &&
                                 !((smith_o_ptr->tval == TV_DIGGING) && (smith_o_ptr->sval == SV_SHOVEL));
	valid[SMT_MENU_ARTEFACT-1] = (!smith_o_ptr->name2) &&
                                 (smith_o_ptr->tval != 0) &&
                                 (smith_o_ptr->tval != TV_HORN) &&
                                 (p_ptr->self_made_arts < z_info->art_self_made_max - z_info->art_rand_max - 2);
	valid[SMT_MENU_NUMBERS-1]  = (smith_o_ptr->tval != 0);
	valid[SMT_MENU_MELT-1]     = mithril_items_carried() && cave_forge_bold(p_ptr->py, p_ptr->px) && (forge_uses(p_ptr->py, p_ptr->px) > 0);
	valid[SMT_MENU_ACCEPT-1]   = affordable(smith_o_ptr) && cave_forge_bold(p_ptr->py, p_ptr->px) && (forge_uses(p_ptr->py, p_ptr->px) > 0);
	

	// display labels
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH] ||
                  p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH] ||
                  p_ptr->active_ability[S_SMT][SMT_JEWELLER]) ? TERM_WHITE : TERM_RED;
	Term_putstr(COL_SMT1, 2, -1, valid[SMT_MENU_CREATE-1]   ? valid_attr : TERM_L_DARK, "a) Base Item");
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT]) ? TERM_WHITE : TERM_RED;
	Term_putstr(COL_SMT1, 3, -1, valid[SMT_MENU_ENCHANT-1]  ? valid_attr : TERM_L_DARK, "b) Enchant");
    valid_attr = (p_ptr->active_ability[S_SMT][SMT_ARTEFACT]) ? TERM_WHITE : TERM_RED;
	Term_putstr(COL_SMT1, 4, -1, valid[SMT_MENU_ARTEFACT-1] ? valid_attr : TERM_L_DARK, "c) Artifice");
	Term_putstr(COL_SMT1, 5, -1, valid[SMT_MENU_NUMBERS-1]  ? TERM_WHITE : TERM_L_DARK, "d) Numbers");
	Term_putstr(COL_SMT1, 6, -1, valid[SMT_MENU_MELT-1]     ? TERM_WHITE : TERM_L_DARK, "e) Melt");
	
	if (p_ptr->smithing_leftover == 0)
	{
		Term_putstr(COL_SMT1, 7, -1, valid[SMT_MENU_ACCEPT-1]   ? TERM_WHITE : TERM_L_DARK, "f) Accept");
	}
	else
	{
		Term_putstr(COL_SMT1, 7, -1, valid[SMT_MENU_ACCEPT-1]   ? TERM_WHITE : TERM_L_DARK, "f) Resume");
	}
	
	// display information about the selected item
	switch (*highlight)
	{
		case SMT_MENU_CREATE:	{	Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "Start with a new base item."); break;	}
		case SMT_MENU_ENCHANT:	{	Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "Choose a special enchantment to add");
                                    Term_putstr(COL_SMT2+2, 3, -1, TERM_SLATE, "to the base item.");
                                    if (smith_o_ptr->name1)
                                        Term_putstr(COL_SMT2+2, 5, -1, TERM_L_DARK, "(not compatible with Artifice)");
                                    if (enchant_then_numbers)
                                    {
                                        Term_putstr(COL_SMT2+2, 5, -1, TERM_L_DARK, "(Enchantment cannot be changed");
                                        Term_putstr(COL_SMT2+2, 6, -1, TERM_L_DARK, "after using the Numbers menu)");
                                    }
                                    break;	}
		case SMT_MENU_ARTEFACT:	{	Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "Design your own artefact.");
                                    if (smith_o_ptr->name2)
                                        Term_putstr(COL_SMT2+2, 4, -1, TERM_L_DARK, "(not compatible with Enchant)"); break;	}
		case SMT_MENU_NUMBERS:	{	Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "Change the item's key numbers."); break;	}
		case SMT_MENU_MELT:		{	Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "Choose a mithril item to melt down."); break;	}
		case SMT_MENU_ACCEPT:
		{
			if (forge_uses(p_ptr->py, p_ptr->px) > 0)
			{
				Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "Create the item you have designed.");
                Term_putstr(COL_SMT2+2, 4, -1, TERM_SLATE, "(to cancel it instead, just press Escape)");
			}
			else if (cave_forge_bold(p_ptr->py, p_ptr->px))
			{
				Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "This forge has no resources left, so you");
				Term_putstr(COL_SMT2+2, 3, -1, TERM_SLATE, "cannot create items. To exit, press Escape."); 
			}
			else
			{
				Term_putstr(COL_SMT2+2, 2, -1, TERM_SLATE, "You are not at a forge and thus cannot");
				Term_putstr(COL_SMT2+2, 3, -1, TERM_SLATE, "create items. To exit, press Escape."); 
			}
			break;	
		}
	}
	
	// highlight the label
	strnfmt(buf, 80, "%c)", (char) 'a' + *highlight - 1);
	Term_putstr(COL_SMT1, *highlight + 1, -1, TERM_L_BLUE, buf);

	// display the object description
	prt_object_description();
	
	// display the object difficulty
	prt_object_difficulty();
	
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(2, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	// choose an option by letter
	if ((ch >= 'a') && (ch <= (char) 'a' + SMT_MENU_MAX - 1))
	{
		int old_highlight = *highlight;
		
		*highlight = (int) ch - 'a' + 1;
		
		// move the light blue highlight
		move_displayed_highlight(old_highlight, valid[old_highlight] ? TERM_WHITE : TERM_L_DARK, *highlight, COL_SMT1);
				
		if (valid[*highlight-1])	return (*highlight);
		else						bell("Invalid choice.");
	}

	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		if (valid[*highlight-1])	return (*highlight);
		else						bell("Invalid choice.");
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = SMT_MENU_MAX;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < SMT_MENU_MAX) (*highlight)++;
		else if (*highlight == SMT_MENU_MAX) *highlight = 1;
	}

	/* Leave menu */
	if ((ch == ESCAPE) || (ch == '4'))
	{
		return (-1);
	}
	
	return (0);
} 


/*
 * Brings up a screen for making new items (only works at a forge).
 * Leads to many submenus which help to determine the item's attributes.
 */
void do_cmd_smithing_screen(void)
{
	int actiontype = -1;
	int highlight = 1;
	bool leave_menu = FALSE;
	bool create = FALSE;
	
	//if (!cave_forge_bold(p_ptr->py, p_ptr->px))
	//{
	//	msg_print("You can only create items at a forge.");
	//	return;
	//}
	
	if (cave_forge_bold(p_ptr->py, p_ptr->px) && forge_uses(p_ptr->py, p_ptr->px) == 0)
	{
		msg_print("The resources of this forge are exhausted.");
		msg_print("You will be able to browse the options but not make new things.");
	}
	
	/* Save screen */
	screen_save();
	
	/* Clear screen */
	Term_clear();
	
	// Hack: flag that we are in the middle of smithing
	p_ptr->smithing = 1;
	
	// deal with previous interruptions
	if (p_ptr->smithing_leftover > 0)
	{
		// default to 'resume' if an item is already in progress
		highlight = SMT_MENU_ACCEPT;
		
		// and backup the smithing item
		object_copy(smith2_o_ptr, smith_o_ptr);
	}
	
	// otherwise wipe the smithing item
	else
	{
		object_wipe(smith_o_ptr);
	}
		
	/* Process Events until "Return to Game" is selected */
	while (!leave_menu)
	{
		actiontype = smithing_menu_aux(&highlight);
		
		// if an action has been selected...
		switch (actiontype)
		{
			case SMT_MENU_CREATE:
			{
				// this is not a resumption of smithing an item
				p_ptr->smithing_leftover = 0;
				
				create_tval_menu();

				// backup the smithing object
				object_copy(smith2_o_ptr, smith_o_ptr);
				
				break;
			}
			case SMT_MENU_ENCHANT:
			{
				if (smith_o_ptr->tval)
				{
					// this is not a resumption of smithing an item
					p_ptr->smithing_leftover = 0;
                    
					if (!enchant_menu())
					{
						// restore the smithing object
						object_copy(smith_o_ptr, smith2_o_ptr);
					}
				}
				else
				{
					bell("You must first select a base item.");
				}
				
				break;
			}
			case SMT_MENU_ARTEFACT:
			{
				if (smith_o_ptr->tval)
				{
					// this is not a resumption of smithing an item
					p_ptr->smithing_leftover = 0;
                    
					artefact_menu();
				}
				else
				{
					bell("You must first select a base item.");
				}
                
				break;
			}
			case SMT_MENU_NUMBERS:
			{
				if (smith_o_ptr->tval)
				{
					// this is not a resumption of smithing an item
					p_ptr->smithing_leftover = 0;

					numbers_menu();
					
					// backup the smithing object
					object_copy(smith2_o_ptr, smith_o_ptr);
				}
				else
				{
					bell("You must first select a base item.");
				}
				
				break;
			}
			case SMT_MENU_MELT:
			{
				if (mithril_items_carried())
				{
					// this is not a resumption of smithing an item
					p_ptr->smithing_leftover = 0;

					melt_menu();
				}
				else
				{
					bell("You don't have any mithril items.");
				}
				
				break;
			}
			case SMT_MENU_ACCEPT:
			{
				if (smithing_cost.drain > 0)
				{
					char buf[80];
					
					sprintf(buf, "This will drain your smithing skill by %d points. Proceed? ", smithing_cost.drain);
					if (!get_check(buf)) break;
				}
				
				create = TRUE;				
				leave_menu = TRUE;
				break;
			}
			case -1:
			{
				leave_menu = TRUE;
				break;
			}
		}
	}
	
	if (create)
	{
		// Display a message
		msg_print("You begin your work.");
		
		// add the details to the artefact type if applicable
		if (smith_o_ptr->name1) add_artefact_details();

		/* Cancel stealth mode */
		p_ptr->stealth_mode = FALSE;
		
		// Allow the resumption of interrupted smithing
		if (p_ptr->smithing_leftover > 0)
		{
			p_ptr->smithing = p_ptr->smithing_leftover;
		}
		else
		{
			// Set smithing counter
			p_ptr->smithing = MAX(10, object_difficulty(smith_o_ptr) * 10);
			
			// Also set the smithing leftover counter (to allow you to resume if interrupted)
			p_ptr->smithing_leftover = p_ptr->smithing;
		}
		
		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);
		
		/* Redraw the state */
		p_ptr->redraw |= (PR_STATE);
		
		/* Handle stuff */
		handle_stuff();
		
		/* Refresh */
		Term_fresh();
	}
	
	else
	{
		if (p_ptr->smithing_leftover == 0)
		{
			/* Wipe the smithing object */
			object_wipe(smith_o_ptr);
		}
		
		// Hack: flag that we are done with smithing
		p_ptr->smithing = 0;
	}
	
	/* Load screen */
	screen_load();
}


/*
 * Actually creates the item.
 */
void create_smithing_item(void)
{
	int slot;
	object_type *o_ptr;
	char o_name[80];
		
	// pay the ability/experience costs of smithing
	pay_costs();
		
	// if making an artefact, copy its attributes into the proper place in the a_info array
	if (smith_o_ptr->name1)
	{
		smith_o_ptr->name1 = z_info->art_rand_max + p_ptr->self_made_arts;
		
		artefact_copy(&a_info[smith_o_ptr->name1], smith_a_ptr);
		p_ptr->self_made_arts++;

		// make sure to display it as cursed if it is so
		if (smith_a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
		{
			smith_o_ptr->ident |= (IDENT_CURSED);
		}
		
		// Store the depth at which it was created
		smith_o_ptr->xtra1 = p_ptr->depth;		
	}
	
	// remove the spoiler ident flag
	smith_o_ptr->ident &= ~(IDENT_SPOIL);

	// identify the object
	ident(smith_o_ptr);

	// create description
	object_desc(o_name, sizeof(o_name), smith_o_ptr, TRUE, 3);
	
	// Record the depth where the object was created
	do_cmd_note(format("Made %s  %d.%d lb", 
	                    o_name, 
						(smith_o_ptr->weight * smith_o_ptr->number) / 10, 
						(smith_o_ptr->weight * smith_o_ptr->number) % 10),
				p_ptr->depth);
	
	// Get the slot of the forged item
	slot = inven_carry(smith_o_ptr);
	
	// Get the item itself
	o_ptr = &inventory[slot];
	
	// Describe the object
	object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
	
	// Message
	if (slot >= 0)	msg_format("You have %s (%c).", o_name, index_to_label(slot));

	// Wipe the smithing object
	object_wipe(smith_o_ptr);	
}

#define MAIN_MENU_RETURN			 1
#define MAIN_MENU_CHARACTER			 2
#define MAIN_MENU_OPTIONS			 3
#define MAIN_MENU_MAP				 4
#define MAIN_MENU_SCORES			 5
#define MAIN_MENU_KNOWN_OBJECTS      6
#define MAIN_MENU_KNOWN_ARTEFACTS    7
#define MAIN_MENU_KNOWN_MONSTERS     8
#define MAIN_MENU_SLAIN_MONSTERS     9
#define MAIN_MENU_NOTE				10
#define MAIN_MENU_SCREENSHOT		11
#define MAIN_MENU_MACROS			12
#define MAIN_MENU_COLORS			13
#define MAIN_MENU_MESSAGES			14
#define MAIN_MENU_VERSION			15
#define MAIN_MENU_ABORT				16
#define MAIN_MENU_SAVE				17
#define MAIN_MENU_SAVE_QUIT			18

#define MAIN_MENU_MAX				18

#define	COL_MAIN					30


/*
 * Performs the interface and selection work for the main menu.
 */
int main_menu_aux(int *highlight)
{
	char ch;
	int i;
	
	for (i = 0; i < MAIN_MENU_MAX + 3; i++)
	{
		Term_putstr(COL_MAIN - 2, i, -1, TERM_WHITE, "                           ");
	}

	Term_putstr(COL_MAIN, 2, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE, "a) Return to game");
	Term_putstr(COL_MAIN, 3, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE, "b) Character sheet");
	Term_putstr(COL_MAIN, 4, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE, "c) Options");
	Term_putstr(COL_MAIN, 5, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE, "d) Map");
	Term_putstr(COL_MAIN, 6, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE, "e) Names of the fallen");
	Term_putstr(COL_MAIN, 7, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE, "f) Known objects");
	Term_putstr(COL_MAIN, 8, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE, "g) Known artefacts");
	Term_putstr(COL_MAIN, 9, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE, "h) Known monsters");
	Term_putstr(COL_MAIN,10, -1, (*highlight == 9) ? TERM_L_BLUE : TERM_WHITE, "i) Slain monsters");
	Term_putstr(COL_MAIN,11, -1, (*highlight ==10) ? TERM_L_BLUE : TERM_WHITE, "j) Write a note");
	Term_putstr(COL_MAIN,12, -1, (*highlight ==11) ? TERM_L_BLUE : TERM_WHITE, "k) Take HTML screenshot");
	Term_putstr(COL_MAIN,13, -1, (*highlight ==12) ? TERM_L_BLUE : TERM_WHITE, "l) Set macros");
	Term_putstr(COL_MAIN,14, -1, (*highlight ==13) ? TERM_L_BLUE : TERM_WHITE, "m) Set colours");
	Term_putstr(COL_MAIN,15, -1, (*highlight ==14) ? TERM_L_BLUE : TERM_WHITE, "n) Show old messages");
	Term_putstr(COL_MAIN,16, -1, (*highlight ==15) ? TERM_L_BLUE : TERM_WHITE, "o) Sil version info");
	Term_putstr(COL_MAIN,17, -1, (*highlight ==16) ? TERM_L_BLUE : TERM_WHITE, "p) Abort current game");
	Term_putstr(COL_MAIN,18, -1, (*highlight ==17) ? TERM_L_BLUE : TERM_WHITE, "q) Save game");
	Term_putstr(COL_MAIN,19, -1, (*highlight ==18) ? TERM_L_BLUE : TERM_WHITE, "r) Save and quit");
		
	/* Flush the prompt */
	Term_fresh();
	
	/* Place cursor at current choice */
	Term_gotoxy(COL_MAIN, 1 + *highlight);
	
	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	// choose an option by letter
	if ((ch >= 'a') && (ch <= (char) 'a' + MAIN_MENU_MAX - 1))
	{
		*highlight = (int) ch - 'a' + 1;
		
		return (*highlight);
	}
	
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
	{
		return (*highlight);
	}
	
	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = MAIN_MENU_MAX;
	}
	
	/* Next item */
	if (ch == '2')
	{
		if (*highlight < MAIN_MENU_MAX) (*highlight)++;
		else if (*highlight == MAIN_MENU_MAX) *highlight = 1;
	}
	
	/* Leave menu */
	if ((ch == ESCAPE) || (ch == '4'))
	{
		return (-1);
	}
	
	return (0);
} 

/*
 * Brings up a menu for choosing some of the game's more abstruse options.
 */
void do_cmd_main_menu(void)
{
	int actiontype = -1;
	int highlight = 1;
	bool leave_menu = FALSE;
	bool take_screen_shot = FALSE;
	
	/* Save screen */
	screen_save();
	
	/* Process Events until "Return to Game" is selected */
	while (!leave_menu)
	{
		actiontype = main_menu_aux(&highlight);
		leave_menu = TRUE;
		
		// if an action has been selected...
		switch (actiontype)
		{
			case MAIN_MENU_RETURN:
			{
				break;
			}
			case MAIN_MENU_CHARACTER:
			{
				do_cmd_character_sheet();
				break;
			}
			case MAIN_MENU_OPTIONS:
			{
				do_cmd_options();
				break;
			}
			case MAIN_MENU_MAP:
			{
				do_cmd_view_map();
				break;
			}
			case MAIN_MENU_SCORES:
			{
				show_scores();
				break;
			}
			case MAIN_MENU_KNOWN_OBJECTS:
			{
				do_cmd_knowledge_objects();
				break;
			}
			case MAIN_MENU_KNOWN_ARTEFACTS:
			{
				do_cmd_knowledge_artefacts();
				break;
			}
			case MAIN_MENU_KNOWN_MONSTERS:
			{
				do_cmd_knowledge_monsters();
				break;
			}
			case MAIN_MENU_SLAIN_MONSTERS:
			{
				do_cmd_knowledge_kills();
				break;
			}
			case MAIN_MENU_NOTE:
			{
				do_cmd_note("",  p_ptr->depth);
				break;
			}
			case MAIN_MENU_SCREENSHOT:
			{
				// have to do this later to avoid taking a shot of this very menu
				take_screen_shot = TRUE;
				break;
			}
			case MAIN_MENU_MACROS:
			{
				do_cmd_macros();
				break;
			}
			case MAIN_MENU_COLORS:
			{
				do_cmd_colors();
				break;
			}
			case MAIN_MENU_MESSAGES:
			{
				do_cmd_messages();
				break;
			}
			case MAIN_MENU_VERSION:
			{
				do_cmd_version();
				break;
			}
			case MAIN_MENU_ABORT:
			{
				do_cmd_suicide();
				break;
			}
			case MAIN_MENU_SAVE:
			{
				do_cmd_save_game();
				break;
			}
			case MAIN_MENU_SAVE_QUIT:
			{
				/* Stop playing */
				p_ptr->playing = FALSE;
				
				/* Leaving */
				p_ptr->leaving = TRUE;
				break;
			}
			case -1:
			{
				break;
			}
			default:
			{
				leave_menu = FALSE;
			}
		}
	}
		
	/* Load screen */
	screen_load();
	
	if (take_screen_shot)
	{
		do_cmd_save_screen();
	}
}



/*
 * Recall the most recent message
 */
void do_cmd_message_one(void)
{
	/* Recall one message XXX XXX XXX */
	c_prt(message_color(0), format( "> %s", message_str(0)), 0, 0);
}


/*
 * Show previous messages to the user
 *
 * The screen format uses line 0 and 23 for headers and prompts,
 * skips line 1 and 22, and uses line 2 thru 21 for old messages.
 *
 * This command shows you which commands you are viewing, and allows
 * you to "search" for strings in the recall.
 *
 * Note that messages may be longer than 80 characters, but they are
 * displayed using "infinite" length, with a special sub-command to
 * "slide" the virtual display to the left or right.
 *
 * Attempt to only hilite the matching portions of the string.
 */
void do_cmd_messages(void)
{
	char ch;

	int i, j, n, q;
	int wid, hgt;

	char shower[80];
	char finder[80];


	/* Wipe finder */
	my_strcpy(finder, "", sizeof(finder));

	/* Wipe shower */
	my_strcpy(shower, "", sizeof(shower));


	/* Total messages */
	n = message_num();

	/* Start on first message */
	i = 0;

	/* Start at leftmost edge */
	q = 0;

	/* Get size */
	Term_get_size(&wid, &hgt);

	/* Save screen */
	screen_save();

	/* Process requests until done */
	while (1)
	{
		/* Clear screen */
		Term_clear();

		/* Dump messages */
		for (j = 0; (j < hgt - 4) && (i + j < n); j++)
		{
			cptr msg = message_str((s16b)(i+j));
			byte attr = message_color((s16b)(i+j));

			/* Apply horizontal scroll */
			msg = ((int)strlen(msg) >= q) ? (msg + q) : "";

			/* Dump the messages, bottom to top */
			Term_putstr(0, hgt - 3 - j, -1, attr, msg);

			/* Hilite "shower" */
			if (shower[0])
			{
				cptr str = msg;

				/* Display matches */
				while ((str = strstr(str, shower)) != NULL)
				{
					int len = strlen(shower);

					/* Display the match */
					Term_putstr(str-msg, hgt - 3 - j, len, TERM_YELLOW, shower);

					/* Advance */
					str += len;
				}
			}
		}

		/* Display header XXX XXX XXX */
		prt(format("Message Recall (%d-%d of %d), Offset %d",
		           i, i + j - 1, n, q), 0, 0);

		/* Display prompt (not very informative) */
		prt("[Press 'p' for older, 'n' for newer, ..., or ESCAPE]", hgt - 1, 0);

		/* Get a command */
		ch = inkey();

		/* Exit on Escape */
		if (ch == ESCAPE) break;

		/* Hack -- Save the old index */
		j = i;

		/* Horizontal scroll */
		if (ch == '4')
		{
			/* Scroll left */
			q = (q >= wid / 2) ? (q - wid / 2) : 0;

			/* Success */
			continue;
		}

		/* Horizontal scroll */
		if (ch == '6')
		{
			/* Scroll right */
			q = q + wid / 2;

			/* Success */
			continue;
		}

		/* Hack -- handle show */
		if (ch == '=')
		{
			/* Prompt */
			prt("Show: ", hgt - 1, 0);

			/* Get a "shower" string, or continue */
			if (!askfor_aux(shower, sizeof(shower))) continue;

			/* Okay */
			continue;
		}

		/* Hack -- handle find */
		if (ch == '/')
		{
			s16b z;

			/* Prompt */
			prt("Find: ", hgt - 1, 0);

			/* Get a "finder" string, or continue */
			if (!askfor_aux(finder, sizeof(finder))) continue;

			/* Show it */
			my_strcpy(shower, finder, sizeof(shower));

			/* Scan messages */
			for (z = i + 1; z < n; z++)
			{
				cptr msg = message_str(z);

				/* Search for it */
				if (strstr(msg, finder))
				{
					/* New location */
					i = z;

					/* Done */
					break;
				}
			}
		}

		/* Recall 20 older messages */
		if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
		{
			/* Go older if legal */
			if (i + 20 < n) i += 20;
		}

		/* Recall 10 older messages */
		if (ch == '+')
		{
			/* Go older if legal */
			if (i + 10 < n) i += 10;
		}

		/* Recall 1 older message */
		if ((ch == '8') || (ch == '\n') || (ch == '\r'))
		{
			/* Go newer if legal */
			if (i + 1 < n) i += 1;
		}

		/* Recall 20 newer messages */
		if ((ch == 'n') || (ch == KTRL('N')))
		{
			/* Go newer (if able) */
			i = (i >= 20) ? (i - 20) : 0;
		}

		/* Recall 10 newer messages */
		if (ch == '-')
		{
			/* Go newer (if able) */
			i = (i >= 10) ? (i - 10) : 0;
		}

		/* Recall 1 newer messages */
		if (ch == '2')
		{
			/* Go newer (if able) */
			i = (i >= 1) ? (i - 1) : 0;
		}

		/* Hack -- Error of some kind */
		if (i == j) bell(NULL);
	}

	/* Load screen */
	screen_load();
}



/*
 * Ask for a "user pref line" and process it
 */
void do_cmd_pref(void)
{
	char tmp[80];

	/* Default */
	my_strcpy(tmp, "", sizeof(tmp));

	/* Ask for a "user pref command" */
	if (!term_get_string("Pref: ", tmp, sizeof(tmp))) return;

	/* Process that pref command */
	(void)process_pref_file_command(tmp);
}


/*
 * Ask for a "user pref file" and process it.
 *
 * This function should only be used by standard interaction commands,
 * in which a standard "Command:" prompt is present on the given row.
 *
 * Allow absolute file names?  XXX XXX XXX
 */
static void do_cmd_pref_file_hack(int row)
{
	char ftmp[80];

	/* Prompt */
	Term_putstr(2, row + 2, -1, TERM_SLATE, "(Escape to cancel)");

	/* Prompt */
	prt("File: ", row, 2);

	/* Default filename */
	strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

	/* Ask for a file (or cancel) */
	if (!askfor_aux(ftmp, sizeof(ftmp))) return;

	/* Process the given filename */
	if (process_pref_file(ftmp))
	{
		/* Mention failure */
		msg_format("Failed to load '%s'!", ftmp);
	}
	else
	{
		/* Mention success */
		msg_format("Loaded '%s'.", ftmp);
	}
}



/*
 * Interact with some options
 */
extern void do_cmd_options_aux(int page, cptr info)
{
	char ch;

	int i, k = 0, n = 0;

	int opt[OPT_PAGE_PER];

	char buf[80];

	int dir;


	/* Scan the options */
	for (i = 0; i < OPT_PAGE_PER; i++)
	{
		/* Collect options on this "page" */
		if (option_page[page][i] != OPT_NONE)
		{
			opt[n++] = option_page[page][i];
		}
	}


	/* Clear screen */
	Term_clear();

	/* Interact with the player */
	while (TRUE)
	{
		/* Prompt XXX XXX XXX */
		strnfmt(buf, sizeof(buf), "%s", info);
		Term_putstr(2, 1, -1, TERM_WHITE, buf);

		/* Display the options */
		for (i = 0; i < n; i++)
		{
			byte a = TERM_WHITE;

			/* Color current option */
			if (i == k) a = TERM_L_BLUE;

			/* Display the option text */
			if (opt[i] == OPT_delay_factor)
			{
				strnfmt(buf, sizeof(buf), "%-48s: %d",
						"Delay factor for animation (0 to 9)", op_ptr->delay_factor);
			}
			else if (opt[i] == OPT_hitpoint_warning)
			{
				strnfmt(buf, sizeof(buf), "%-48s: %d%%",
						"Hitpoint warning threshold (0% to 90%)", op_ptr->hitpoint_warn * 10);
			}
			else
			{
				strnfmt(buf, sizeof(buf), "%-48s: %s",
						option_desc[opt[i]],
						op_ptr->opt[opt[i]] ? "yes" : "no ");
			}
					
			
			c_prt(a, buf, i + 3, 2);
		}

		if (page == CHALLENGE_PAGE)
		{
			Term_putstr(2, n+4, -1, TERM_L_WHITE, "Challenge Options can only be altered during character creation");
			Term_putstr(2, n+5, -1, TERM_L_WHITE, "or on the very first turn");
			
			if (playerturn == 0)
			{
				Term_putstr(2, n+7, -1, TERM_SLATE, "(direction keys to set, Return/Escape to accept)");
			}
			else
			{
				Term_putstr(2, n+7, -1, TERM_SLATE, "(press Return to go back)");
			}
		}
		else
		{
			Term_putstr(2, n+4, -1, TERM_SLATE, "(direction keys to set, Return/Escape to accept)");
		}

		/* Hilite current option */
		move_cursor(k + 3, 52);

		/* Get a key */
		hide_cursor = TRUE;
		ch = inkey();
		hide_cursor = FALSE;

		/*
		 * HACK - Try to translate the key into a direction
		 * to allow using the roguelike keys for navigation.
		 */
		dir = target_dir(ch);
		if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
			ch = I2D(dir);

		/* Analyze */
		switch (ch)
		{
			case ESCAPE:
			case '\n':
			case '\r':
			{
				/* Hack -- Notice use of any "cheat" options */
				for (i = OPT_CHEAT; i < OPT_ADULT; i++)
				{
					if (op_ptr->opt[i])
					{
						/* Set score option */
						op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = TRUE;
					}
				}

				return;
			}

			case '-':
			case '8':
			{
				k = (n + k - 1) % n;
				break;
			}

			case '2':
			{
				k = (k + 1) % n;
				break;
			}

			case 't':
			case '5':
			case ' ':
			{
				if ((page != CHALLENGE_PAGE) || (playerturn == 0))
				{
					op_ptr->opt[opt[k]] = !op_ptr->opt[opt[k]];
				}
				break;
			}

			case 'y':
			case '6':
			{
				if ((page != CHALLENGE_PAGE) || (playerturn == 0))
				{
					if (opt[k] == OPT_delay_factor)
					{
						op_ptr->delay_factor = (op_ptr->delay_factor < 9) ? op_ptr->delay_factor + 1 : 9;
					}
					else if (opt[k] == OPT_hitpoint_warning)
					{
						op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn < 9) ? op_ptr->hitpoint_warn + 1 : 9;
					}
					else
					{
						op_ptr->opt[opt[k]] = TRUE;
					}
				}
				break;
			}

			case 'n':
			case '4':
			{
				if ((page != CHALLENGE_PAGE) || (playerturn == 0))
				{
					if (opt[k] == OPT_delay_factor)
					{
						op_ptr->delay_factor = (op_ptr->delay_factor > 0) ? op_ptr->delay_factor - 1 : 0;
					}
					else if (opt[k] == OPT_hitpoint_warning)
					{
						op_ptr->hitpoint_warn = (op_ptr->hitpoint_warn > 0) ? op_ptr->hitpoint_warn - 1 : 0;
					}
					else
					{
						op_ptr->opt[opt[k]] = FALSE;
					}
				}
				break;
			}

			default:
			{
				bell("Illegal command for normal options!");
				break;
			}
		}
	}
}



/*
 * Write all current options to the given preference file in the
 * lib/user directory. Modified from KAmband 1.8.
 */
static errr option_dump(cptr fname)
{
	static cptr mark = "Options Dump";

	int i, j;

	FILE *fff;

	char buf[1024];

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Remove old options */
	remove_old_dump(buf, mark);

	/* Append to the file */
	fff = my_fopen(buf, "a");

	/* Failure */
	if (!fff) return (-1);

	/* Output header */
	pref_header(fff, mark);

	/* Skip some lines */
	fprintf(fff, "\n\n");

	/* Start dumping */
	fprintf(fff, "# Automatic option dump\n\n");

	/* Dump options (skip cheat, adult, score) */
	for (i = 0; i < OPT_CHEAT; i++)
	{
		/* Require a real option */
		if (!option_text[i]) continue;

		/* Comment */
		fprintf(fff, "# Option '%s'\n", option_desc[i]);

		/* Dump the option */
		if (op_ptr->opt[i])
		{
			fprintf(fff, "Y:%s\n", option_text[i]);
		}
		else
		{
			fprintf(fff, "X:%s\n", option_text[i]);
		}

		/* Skip a line */
		fprintf(fff, "\n");
	}

	/* Dump window flags */
	for (i = 1; i < ANGBAND_TERM_MAX; i++)
	{
		/* Require a real window */
		if (!angband_term[i]) continue;

		/* Check each flag */
		for (j = 0; j < 32; j++)
		{
			/* Require a real flag */
			if (!window_flag_desc[j]) continue;

			/* Comment */
			fprintf(fff, "# Window '%s', Flag '%s'\n",
			        angband_term_name[i], window_flag_desc[j]);

			/* Dump the flag */
			if (op_ptr->window_flag[i] & (1L << j))
			{
				fprintf(fff, "W:%d:%d:1\n", i, j);
			}
			else
			{
				fprintf(fff, "W:%d:%d:0\n", i, j);
			}

			/* Skip a line */
			fprintf(fff, "\n");
		}
	}

	/* Output footer */
	pref_footer(fff, mark);

	/* Close */
	my_fclose(fff);

	/* Success */
	return (0);
}



int options_menu(int *highlight)
{
	int ch;
	int options = 6;
	
	if (p_ptr->noscore) options++;

	Term_putstr(2,  1, -1, TERM_WHITE, "Options");
	
	Term_putstr(2,  3, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE, "a) Interface Options");
	Term_putstr(2,  4, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE, "b) Visual Options");
	Term_putstr(2,  5, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE, "c) Challenge Options");
	Term_putstr(2,  6, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE, "d) Load a 'Pref' File");
	Term_putstr(2,  7, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE, "e) Append Options to a 'Pref' File");
	Term_putstr(2,  8, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE, "f) Return to Game");
	
	if (p_ptr->noscore)
	{
		Term_putstr(2, 9, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE, "g) Debugging Options");
	}

	/* Flush the prompt */
	Term_fresh();

	/* Place cursor at current choice */
	Term_gotoxy(2, 2 + *highlight);

	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	if ((ch == 'a') || (ch == 'A'))
	{
		*highlight = 1;
		return (1);
	}

	if ((ch == 'b') || (ch == 'B'))
	{
		*highlight = 2;
		return (2);
	}

	if ((ch == 'c') || (ch == 'C'))
	{
		*highlight = 3;
		return (3);
	}

	if ((ch == 'd') || (ch == 'D'))
	{
		*highlight = 4;
		return (4);
	}

	if ((ch == 'e') || (ch == 'E'))
	{
		*highlight = 5;
		return (5);
	}

	if ((ch == 'f') || (ch == 'F') || (ch == ESCAPE) || (ch == 'q'))
	{
		*highlight = 6;
		return (6);
	}

	if ((ch == 'g') || (ch == 'G'))
	{
		*highlight = 7;
		return (7);
	}
		
	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' '))
	{
		return (*highlight);
	}

	/* Prev item */
	if (ch == '8')
	{
		*highlight = (*highlight + (options-2)) % options + 1;
	}

	/* Next item */
	if (ch == '2')
	{
		*highlight = *highlight % options + 1;
	}

	return (0);
} 


/*
 * Set or unset various options.
 *
 * After using this command, a complete redraw should be performed,
 * in case any visual options have been changed.
 */
void do_cmd_options(void)
{
	int choice = 0;
	int highlight = 1;

	char ftmp[80];
	
	bool return_to_game = FALSE;

	/* Save screen */
	screen_save();

	/* Clear screen */
	Term_clear();
	
	/* Process Events until "Return to Game" is selected */
	while (!return_to_game)
	{

		choice = options_menu(&highlight);
		
		switch (choice)
		{
			case 1:
			{
				do_cmd_options_aux(INTERFACE_PAGE, "Interface Options");
				Term_clear();
				break;
			}
			case 2:
			{
				do_cmd_options_aux(VISUAL_PAGE, "Visual Options");
				Term_clear();
				break;
			}
			case 3:
			{
				do_cmd_options_aux(CHALLENGE_PAGE, "Challenge Options");
				Term_clear();
				break;
			}
			case 4:
			{
				/* Ask for and load a user pref file */
				do_cmd_pref_file_hack(12);
				Term_clear();
				break;
			}
			case 5:
			{
				/* Prompt */
				Term_putstr(2, 14, -1, TERM_SLATE, "(Escape to cancel)");

				/* Prompt */
				prt("File: ", 12, 2);

				/* Default filename */
				strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

				/* Ask for a file */
				if (!askfor_aux(ftmp, sizeof(ftmp)))
				{
					Term_clear();
					continue;
				}
				
				/* Dump the options */
				if (option_dump(ftmp))
				{
					/* Failure */
					msg_print("Failed!");
				}
				else
				{
					/* Success */
					msg_print("Done.");
				}
				
				Term_clear();
				break;
			}
			case 6:
			{
				return_to_game = TRUE;
				Term_clear();
				break;
			}
			case 7:
			{
				do_cmd_options_aux(DEBUG_PAGE, "Debugging Options");
				Term_clear();
				break;
			}
		}
	}

	/* Flush messages */
	message_flush();

	/* Load screen */
	screen_load();
}



#ifdef ALLOW_MACROS

/*
 * Hack -- append all current macros to the given file
 */
static errr macro_dump(cptr fname)
{
	static cptr mark = "Macro Dump";

	int i;

	FILE *fff;

	char buf[1024];


	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Remove old macros */
	remove_old_dump(buf, mark);

	/* Append to the file */
	fff = my_fopen(buf, "a");

	/* Failure */
	if (!fff) return (-1);

	/* Output header */
	pref_header(fff, mark);

	/* Skip some lines */
	fprintf(fff, "\n\n");

	/* Start dumping */
	fprintf(fff, "# Automatic macro dump\n\n");

	/* Dump them */
	for (i = 0; i < macro__num; i++)
	{
		/* Start the macro */
		fprintf(fff, "# Macro '%d'\n\n", i);

		/* Extract the macro action */
		ascii_to_text(buf, sizeof(buf), macro__act[i]);

		/* Dump the macro action */
		fprintf(fff, "A:%s\n", buf);

		/* Extract the macro pattern */
		ascii_to_text(buf, sizeof(buf), macro__pat[i]);

		/* Dump the macro pattern */
		fprintf(fff, "P:%s\n", buf);

		/* End the macro */
		fprintf(fff, "\n\n");
	}

	/* Start dumping */
	fprintf(fff, "\n\n\n\n");

	/* Output footer */
	pref_footer(fff, mark);

	/* Close */
	my_fclose(fff);

	/* Success */
	return (0);
}


/*
 * Hack -- ask for a "trigger" (see below)
 *
 * Note the complex use of the "inkey()" function from "util.c".
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux(char *buf)
{
	char ch;

	int n = 0;

	char tmp[1024];


	/* Flush */
	flush();

	/* Do not process macros */
	inkey_base = TRUE;

	/* First key */
	ch = inkey();

	/* Read the pattern */
	while (ch != '\0')
	{
		/* Save the key */
		buf[n++] = ch;

		/* Do not process macros */
		inkey_base = TRUE;

		/* Do not wait for keys */
		inkey_scan = TRUE;

		/* Attempt to read a key */
		ch = inkey();
	}

	/* Terminate */
	buf[n] = '\0';

	/* Flush */
	flush();


	/* Convert the trigger */
	ascii_to_text(tmp, sizeof(tmp), buf);

	/* Hack -- display the trigger */
	Term_addstr(-1, TERM_WHITE, tmp);
}


/*
 * Hack -- ask for a keymap "trigger" (see below)
 *
 * Note that both "flush()" calls are extremely important.  This may
 * no longer be true, since "util.c" is much simpler now.  XXX XXX XXX
 */
static void do_cmd_macro_aux_keymap(char *buf)
{
	char tmp[1024];


	/* Flush */
	flush();


	/* Get a key */
	buf[0] = inkey();
	buf[1] = '\0';


	/* Convert to ascii */
	ascii_to_text(tmp, sizeof(tmp), buf);

	/* Hack -- display the trigger */
	Term_addstr(-1, TERM_WHITE, tmp);


	/* Flush */
	flush();
}


/*
 * Hack -- Append all keymaps to the given file.
 *
 * Hack -- We only append the keymaps for the "active" mode.
 */
static errr keymap_dump(cptr fname)
{
	static cptr mark = "Keymap Dump";

	int i;

	FILE *fff;

	char buf[1024];

	int mode;

	// Determine the keyset
	if (!hjkl_movement && !angband_keyset)		mode = KEYMAP_MODE_SIL;
	else if (hjkl_movement && !angband_keyset)	mode = KEYMAP_MODE_SIL_HJKL;
	else if (!hjkl_movement && angband_keyset)	mode = KEYMAP_MODE_ANGBAND;
	else										mode = KEYMAP_MODE_ANGBAND_HJKL;

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Remove old keymaps */
	remove_old_dump(buf, mark);

	/* Append to the file */
	fff = my_fopen(buf, "a");

	/* Failure */
	if (!fff) return (-1);

	/* Output header */
	pref_header(fff, mark);

	/* Skip some lines */
	fprintf(fff, "\n\n");

	/* Start dumping */
	fprintf(fff, "# Automatic keymap dump\n\n");

	/* Dump them */
	for (i = 0; i < (int)N_ELEMENTS(keymap_act[mode]); i++)
	{
		char key[2] = "?";

		cptr act;

		/* Loop up the keymap */
		act = keymap_act[mode][i];

		/* Skip empty keymaps */
		if (!act) continue;

		/* Encode the action */
		ascii_to_text(buf, sizeof(buf), act);

		/* Dump the keymap action */
		fprintf(fff, "A:%s\n", buf);

		/* Convert the key into a string */
		key[0] = i;

		/* Encode the key */
		ascii_to_text(buf, sizeof(buf), key);

		/* Dump the keymap pattern */
		fprintf(fff, "C:%d:%s\n", mode, buf);

		/* Skip a line */
		fprintf(fff, "\n");
	}

	/* Skip some lines */
	fprintf(fff, "\n\n\n");

	/* Output footer */
	pref_footer(fff, mark);

	/* Close */
	my_fclose(fff);

	/* Success */
	return (0);
}


#endif


/*
 * Interact with "macros"
 *
 * Could use some helpful instructions on this page.  XXX XXX XXX
 */
void do_cmd_macros(void)
{
	char ch;

	char tmp[1024];

	char pat[1024];

	int mode;


	// Determine the keyset
	if (!hjkl_movement && !angband_keyset)		mode = KEYMAP_MODE_SIL;
	else if (hjkl_movement && !angband_keyset)	mode = KEYMAP_MODE_SIL_HJKL;
	else if (!hjkl_movement && angband_keyset)	mode = KEYMAP_MODE_ANGBAND;
	else										mode = KEYMAP_MODE_ANGBAND_HJKL;
	

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);


	/* Save screen */
	screen_save();


	/* Process requests until done */
	while (1)
	{
		/* Clear screen */
		Term_clear();

		/* Describe */
		prt("Interact with Macros", 2, 0);

		/* Describe that action */
		prt("Current action (if any) shown below:", 20, 0);

		/* Analyze the current action */
		ascii_to_text(tmp, sizeof(tmp), macro_buffer);

		/* Display the current action */
		prt(tmp, 22, 0);


		/* Selections */
		prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_MACROS
		prt("(2) Append macros to a file", 5, 5);
		prt("(3) Query a macro", 6, 5);
		prt("(4) Create a macro", 7, 5);
		prt("(5) Remove a macro", 8, 5);
		prt("(6) Append keymaps to a file", 9, 5);
		prt("(7) Query a keymap", 10, 5);
		prt("(8) Create a keymap", 11, 5);
		prt("(9) Remove a keymap", 12, 5);
		prt("(0) Enter a new action", 13, 5);
#endif /* ALLOW_MACROS */

		/* Prompt */
		prt("Command: ", 16, 0);

		/* Get a command */
		ch = inkey();

		/* Leave */
		if (ch == ESCAPE) break;

		/* Load a user pref file */
		if (ch == '1')
		{
			/* Ask for and load a user pref file */
			do_cmd_pref_file_hack(16);
		}

#ifdef ALLOW_MACROS

		/* Save macros */
		else if (ch == '2')
		{
			char ftmp[80];

			/* Prompt */
			prt("Command: Append macros to a file", 16, 0);

			/* Prompt */
			prt("File: ", 18, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Ask for a file */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Dump the macros */
			(void)macro_dump(ftmp);

			/* Prompt */
			msg_print("Appended macros.");
		}

		/* Query a macro */
		else if (ch == '3')
		{
			int k;

			/* Prompt */
			prt("Command: Query a macro", 16, 0);

			/* Prompt */
			prt("Trigger: ", 18, 0);

			/* Get a macro trigger */
			do_cmd_macro_aux(pat);

			/* Get the action */
			k = macro_find_exact(pat);

			/* Nothing found */
			if (k < 0)
			{
				/* Prompt */
				msg_print("Found no macro.");
			}

			/* Found one */
			else
			{
				/* Obtain the action */
				my_strcpy(macro_buffer, macro__act[k], sizeof(macro_buffer));

				/* Analyze the current action */
				ascii_to_text(tmp, sizeof(tmp), macro_buffer);

				/* Display the current action */
				prt(tmp, 22, 0);

				/* Prompt */
				msg_print("Found a macro.");
			}
		}

		/* Create a macro */
		else if (ch == '4')
		{
			/* Prompt */
			prt("Command: Create a macro", 16, 0);

			/* Prompt */
			prt("Trigger: ", 18, 0);

			/* Get a macro trigger */
			do_cmd_macro_aux(pat);

			/* Clear */
			clear_from(20);

			/* Prompt */
			prt("Action: ", 20, 0);

			/* Convert to text */
			ascii_to_text(tmp, sizeof(tmp), macro_buffer);

			/* Get an encoded action */
			if (askfor_aux(tmp, 80))
			{
				/* Convert to ascii */
				text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

				/* Link the macro */
				macro_add(pat, macro_buffer);

				/* Prompt */
				msg_print("Added a macro.");
			}
		}

		/* Remove a macro */
		else if (ch == '5')
		{
			/* Prompt */
			prt("Command: Remove a macro", 16, 0);

			/* Prompt */
			prt("Trigger: ", 18, 0);

			/* Get a macro trigger */
			do_cmd_macro_aux(pat);

			/* Link the macro */
			macro_add(pat, pat);

			/* Prompt */
			msg_print("Removed a macro.");
		}

		/* Save keymaps */
		else if (ch == '6')
		{
			char ftmp[80];

			/* Prompt */
			prt("Command: Append keymaps to a file", 16, 0);

			/* Prompt */
			prt("File: ", 18, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Ask for a file */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Dump the macros */
			(void)keymap_dump(ftmp);

			/* Prompt */
			msg_print("Appended keymaps.");
		}

		/* Query a keymap */
		else if (ch == '7')
		{
			cptr act;

			/* Prompt */
			prt("Command: Query a keymap", 16, 0);

			/* Prompt */
			prt("Keypress: ", 18, 0);

			/* Get a keymap trigger */
			do_cmd_macro_aux_keymap(pat);

			/* Look up the keymap */
			act = keymap_act[mode][(byte)(pat[0])];

			/* Nothing found */
			if (!act)
			{
				/* Prompt */
				msg_print("Found no keymap.");
			}

			/* Found one */
			else
			{
				/* Obtain the action */
				my_strcpy(macro_buffer, act, sizeof(macro_buffer));

				/* Analyze the current action */
				ascii_to_text(tmp, sizeof(tmp), macro_buffer);

				/* Display the current action */
				prt(tmp, 22, 0);

				/* Prompt */
				msg_print("Found a keymap.");
			}
		}

		/* Create a keymap */
		else if (ch == '8')
		{
			/* Prompt */
			prt("Command: Create a keymap", 16, 0);

			/* Prompt */
			prt("Keypress: ", 18, 0);

			/* Get a keymap trigger */
			do_cmd_macro_aux_keymap(pat);

			/* Clear */
			clear_from(20);

			/* Prompt */
			prt("Action: ", 20, 0);

			/* Convert to text */
			ascii_to_text(tmp, sizeof(tmp), macro_buffer);

			/* Get an encoded action */
			if (askfor_aux(tmp, 80))
			{
				/* Convert to ascii */
				text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);

				/* Free old keymap */
				string_free(keymap_act[mode][(byte)(pat[0])]);

				/* Make new keymap */
				keymap_act[mode][(byte)(pat[0])] = string_make(macro_buffer);

				/* Prompt */
				msg_print("Added a keymap.");
			}
		}

		/* Remove a keymap */
		else if (ch == '9')
		{
			/* Prompt */
			prt("Command: Remove a keymap", 16, 0);

			/* Prompt */
			prt("Keypress: ", 18, 0);

			/* Get a keymap trigger */
			do_cmd_macro_aux_keymap(pat);

			/* Free old keymap */
			string_free(keymap_act[mode][(byte)(pat[0])]);

			/* Make new keymap */
			keymap_act[mode][(byte)(pat[0])] = NULL;

			/* Prompt */
			msg_print("Removed a keymap.");
		}

		/* Enter a new action */
		else if (ch == '0')
		{
			/* Prompt */
			prt("Command: Enter a new action", 16, 0);

			/* Go to the correct location */
			Term_gotoxy(0, 22);

			/* Analyze the current action */
			ascii_to_text(tmp, sizeof(tmp), macro_buffer);

			/* Get an encoded action */
			if (askfor_aux(tmp, 80))
			{
				/* Extract an action */
				text_to_ascii(macro_buffer, sizeof(macro_buffer), tmp);
			}
		}

#endif /* ALLOW_MACROS */

		/* Oops */
		else
		{
			/* Oops */
			bell("Illegal command for macros!");
		}

		/* Flush messages */
		message_flush();
	}


	/* Load screen */
	screen_load();
}

/*
 * Asks to the player for an extended color. It is done in two steps:
 * 1. Asks for the base color.
 * 2. Asks for a specific shade.
 * It erases the given line.
 * If the user press ESCAPE no changes are made to attr.
 */
static void askfor_shade(byte *attr, int y)
{
 	byte base, shade, temp;
  	bool changed = FALSE;
  	char *msg, *pos;
  	int ch;

  	/* Start with the given base color */
  	base = GET_BASE_COLOR(*attr);

  	/* 1. Query for base color */
  	while (1)
  	{
    	/* Clear the line */
    	Term_erase(0, y, 255);

    	/* Format the query */
    	msg = format("1. Choose base color (use arrows) " COLOR_SAMPLE
					" %s (attr = %d) ", color_names[base], base);

   	 	/* Display it */
  	  	c_put_str(TERM_WHITE, msg, y, 0);

    	/* Find the sample */
    	pos = strstr(msg, COLOR_SAMPLE);

    	/* Show it using the proper color */
    	c_put_str(base, COLOR_SAMPLE, y, pos - msg);

    	/* Place the cursor at the end of the message */
    	Term_gotoxy(strlen(msg), y);

    	/* Get a command */
    	ch = inkey();

    	/* Cancel */
    	if (ch == ESCAPE)
    	{
      		/* Clear the line */
      		Term_erase(0, y, 255);
      		return;
    	}

    	/* Accept the current base color */
    	if ((ch == '\r') || (ch == '\n')) break;

    	/* Move to the previous color if possible */
    	if ((ch == '4') && (base > 0))
    	{
     		--base;
      		/* Reset the shade, see below */
     	 	changed = TRUE;
      		continue;
    	}

    	/* Move to the next color if possible */
    	if ((ch == '6') && (base < MAX_BASE_COLORS - 1))
    	{
      		++base;
      		/* Reset the shade, see below */
      		changed = TRUE;
      		continue;
    	}
  	}

  	/* The player selected a different base color, start from shade 0 */
  	if (changed)	shade = 0;
  	/* We assume that the player is editing the current shade, go there */
  	else		shade = GET_SHADE(*attr);

  	/* 2. Query for specific shade */
  	while (1)
  	{
    	/* Clear the line */
    	Term_erase(0, y, 255);

    	/* Create the real color */
    	temp = MAKE_EXTENDED_COLOR(base, shade);

    	/* Format the message */
    	msg = format("2. Choose shade (use arrows) " COLOR_SAMPLE
			" %s (attr = %d) ", get_ext_color_name(temp), temp);

    	/* Display it */
    	c_put_str(TERM_WHITE, msg, y, 0);

    	/* Find the sample */
    	pos = strstr(msg, COLOR_SAMPLE);

    	/* Show it using the proper color */
    	c_put_str(temp, COLOR_SAMPLE, y, pos - msg);

    	/* Place the cursor at the end of the message */
    	Term_gotoxy(strlen(msg), y);

    	/* Get a command */
    	ch = inkey();

    	/* Cancel */
    	if (ch == ESCAPE)
    	{
      		/* Clear the line */
      		Term_erase(0, y, 255);
      		return;
    	}

    	/* Accept the current shade */
    	if ((ch == '\r') || (ch == '\n')) break;

    	/* Move to the previous shade if possible */
    	if ((ch == '4') && (shade > 0))
    	{
      		--shade;
      		continue;
    	}

    	/* Move to the next shade if possible */
    	if ((ch == '6') && (shade < MAX_SHADES - 1))
    	{
      		++shade;
      		continue;
    	}
 	}

  	/* Assign the selected shade */
  	*attr = temp;

  	/* Clear the line. It is needed to fit in the current UI */
  	Term_erase(0, y, 255);
}

/*
 * Interact with "visuals"
 */
void do_cmd_visuals(void)
{
	int ch;
	int cx;

	int i;

	FILE *fff;

	char buf[1024];


	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);


	/* Save screen */
	screen_save();


	/* Interact until done */
	while (1)
	{
		/* Clear screen */
		Term_clear();

		/* Ask for a choice */
		prt("Interact with Visuals", 2, 0);

		/* Give some choices */
		prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_VISUALS
		prt("(2) Dump monster attr/chars", 5, 5);
		prt("(3) Dump object attr/chars", 6, 5);
		prt("(4) Dump feature attr/chars", 7, 5);
		prt("(5) Dump flavor attr/chars", 8, 5);
		prt("(6) Change monster attr/chars", 9, 5);
		prt("(7) Change object attr/chars", 10, 5);
		prt("(8) Change feature attr/chars", 11, 5);
		prt("(9) Change flavor attr/chars", 12, 5);
#endif
		prt("(0) Reset visuals", 13, 5);

		/* Prompt */
		prt("Command: ", 15, 0);

		/* Prompt */
		ch = inkey();

		/* Done */
		if (ch == ESCAPE) break;

		/* Load a user pref file */
		if (ch == '1')
		{
			/* Ask for and load a user pref file */
			do_cmd_pref_file_hack(15);
		}

#ifdef ALLOW_VISUALS

		/* Dump monster attr/chars */
		else if (ch == '2')
		{
			static cptr mark = "Monster attr/chars";
			char ftmp[80];

			/* Prompt */
			prt("Command: Dump monster attr/chars", 15, 0);

			/* Prompt */
			prt("File: ", 17, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Get a filename */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Build the filename */
			path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp);

			/* Remove old attr/chars */
			remove_old_dump(buf, mark);

			/* Append to the file */
			fff = my_fopen(buf, "a");

			/* Failure */
			if (!fff) continue;

			/* Output header */
			pref_header(fff, mark);

			/* Skip some lines */
			fprintf(fff, "\n\n");

			/* Start dumping */
			fprintf(fff, "# Monster attr/char definitions\n\n");

			/* Dump monsters */
			for (i = 0; i < z_info->r_max; i++)
			{
				monster_race *r_ptr = &r_info[i];

				/* Skip non-entries */
				if (!r_ptr->name) continue;

				/* Dump a comment */
				fprintf(fff, "# %s\n", (r_name + r_ptr->name));

				/* Dump the monster attr/char info */
				fprintf(fff, "R:%d:0x%02X:0x%02X\n\n", i,
				        (byte)(r_ptr->x_attr), (byte)(r_ptr->x_char));
			}

			/* All done */
			fprintf(fff, "\n\n\n\n");

			/* Output footer */
			pref_footer(fff, mark);

			/* Close */
			my_fclose(fff);

			/* Message */
			msg_print("Dumped monster attr/chars.");
		}

		/* Dump object attr/chars */
		else if (ch == '3')
		{
			static cptr mark = "Object attr/chars";
			char ftmp[80];

			/* Prompt */
			prt("Command: Dump object attr/chars", 15, 0);

			/* Prompt */
			prt("File: ", 17, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Get a filename */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Build the filename */
			path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp);

			/* Remove old attr/chars */
			remove_old_dump(buf, mark);

			/* Append to the file */
			fff = my_fopen(buf, "a");

			/* Failure */
			if (!fff) continue;

			/* Output header */
			pref_header(fff, mark);

			/* Skip some lines */
			fprintf(fff, "\n\n");

			/* Start dumping */
			fprintf(fff, "# Object attr/char definitions\n\n");

			/* Dump objects */
			for (i = 0; i < z_info->k_max; i++)
			{
				object_kind *k_ptr = &k_info[i];

				/* Skip non-entries */
				if (!k_ptr->name) continue;

				/* Dump a comment */
				fprintf(fff, "# %s\n", (k_name + k_ptr->name));

				/* Dump the object attr/char info */
				fprintf(fff, "K:%d:0x%02X:0x%02X\n\n", i,
				        (byte)(k_ptr->x_attr), (byte)(k_ptr->x_char));
			}

			/* All done */
			fprintf(fff, "\n\n\n\n");

			/* Output footer */
			pref_footer(fff, mark);

			/* Close */
			my_fclose(fff);

			/* Message */
			msg_print("Dumped object attr/chars.");
		}

		/* Dump feature attr/chars */
		else if (ch == '4')
		{
			static cptr mark = "Feature attr/chars";
			char ftmp[80];

			/* Prompt */
			prt("Command: Dump feature attr/chars", 15, 0);

			/* Prompt */
			prt("File: ", 17, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Get a filename */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Build the filename */
			path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp);

			/* Remove old attr/chars */
			remove_old_dump(buf, mark);

			/* Append to the file */
			fff = my_fopen(buf, "a");

			/* Failure */
			if (!fff) continue;

			/* Output header */
			pref_header(fff, mark);

			/* Skip some lines */
			fprintf(fff, "\n\n");

			/* Start dumping */
			fprintf(fff, "# Feature attr/char definitions\n\n");

			/* Dump features */
			for (i = 0; i < z_info->f_max; i++)
			{
				feature_type *f_ptr = &f_info[i];

				/* Skip non-entries */
				if (!f_ptr->name) continue;

				/* Dump a comment */
				fprintf(fff, "# %s\n", (f_name + f_ptr->name));

				/* Dump the feature attr/char info */
				fprintf(fff, "F:%d:0x%02X:0x%02X\n\n", i,
				        (byte)(f_ptr->x_attr), (byte)(f_ptr->x_char));
			}

			/* All done */
			fprintf(fff, "\n\n\n\n");

			/* Output footer */
			pref_footer(fff, mark);

			/* Close */
			my_fclose(fff);

			/* Message */
			msg_print("Dumped feature attr/chars.");
		}

		/* Dump flavor attr/chars */
		else if (ch == '5')
		{
			static cptr mark = "Flavor attr/chars";
			char ftmp[80];

			/* Prompt */
			prt("Command: Dump flavor attr/chars", 15, 0);

			/* Prompt */
			prt("File: ", 17, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Get a filename */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Build the filename */
			path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp);

			/* Remove old attr/chars */
			remove_old_dump(buf, mark);

			/* Append to the file */
			fff = my_fopen(buf, "a");

			/* Failure */
			if (!fff) continue;

			/* Output header */
			pref_header(fff, mark);

			/* Skip some lines */
			fprintf(fff, "\n\n");

			/* Start dumping */
			fprintf(fff, "# Flavor attr/char definitions\n\n");

			/* Dump flavors */
			for (i = 0; i < z_info->flavor_max; i++)
			{
				flavor_type *flavor_ptr = &flavor_info[i];

				/* Dump a comment */
				fprintf(fff, "# %s\n", (flavor_text + flavor_ptr->text));

				/* Dump the flavor attr/char info */
				fprintf(fff, "L:%d:0x%02X:0x%02X\n\n", i,
				        (byte)(flavor_ptr->x_attr), (byte)(flavor_ptr->x_char));
			}

			/* All done */
			fprintf(fff, "\n\n\n\n");

			/* Output footer */
			pref_footer(fff, mark);

			/* Close */
			my_fclose(fff);

			/* Message */
			msg_print("Dumped flavor attr/chars.");
		}

		/* Modify monster attr/chars */
		else if (ch == '6')
		{
			static int r = 0;

			/* Prompt */
			prt("Command: Change monster attr/chars", 15, 0);

			/* Hack -- query until done */
			while (1)
			{
				monster_race *r_ptr = &r_info[r];

				byte da = (byte)(r_ptr->d_attr);
				byte dc = (byte)(r_ptr->d_char);
				byte ca = (byte)(r_ptr->x_attr);
				byte cc = (byte)(r_ptr->x_char);

				/* Label the object */
				Term_putstr(5, 17, -1, TERM_WHITE,
				            format("Monster = %d, Name = %-40.40s",
				                   r, (r_name + r_ptr->name)));

				/* Label the Default values */
				Term_putstr(10, 19, -1, TERM_WHITE,
				            format("Default attr/char = %3u / %3u", da, dc));
				Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 19, da, dc);

				if (use_bigtile)
				{
					if (da & 0x80)
						Term_putch(44, 19, 255, -1);
					else
						Term_putch(44, 19, 0, ' ');
				}

				/* Label the Current values */
				Term_putstr(10, 20, -1, TERM_WHITE,
				            format("Current attr/char = %3u / %3u", ca, cc));
				Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 20, ca, cc);

				if (use_bigtile)
				{
					if (ca & 0x80)
						Term_putch(44, 20, 255, -1);
					else
						Term_putch(44, 20, 0, ' ');
				}

				/* Prompt */
				Term_putstr(0, 22, -1, TERM_WHITE,
				            "Command (n/N/a/A/c/C/'s'hade): ");

				/* Get a command */
				cx = inkey();

				/* All done */
				if (cx == ESCAPE) break;

				/* Analyze */
				if (cx == 'n') r = (r + z_info->r_max + 1) % z_info->r_max;
				if (cx == 'N') r = (r + z_info->r_max - 1) % z_info->r_max;
				if (cx == 'a') r_ptr->x_attr = (byte)(ca + 1);
				if (cx == 'A') r_ptr->x_attr = (byte)(ca - 1);
				if (cx == 'c') r_ptr->x_char = (byte)(cc + 1);
				if (cx == 'C') r_ptr->x_char = (byte)(cc - 1);
				if (cx == 's')
				{
				  askfor_shade(&r_ptr->x_attr, 22);
				}
			}
		}

		/* Modify object attr/chars */
		else if (ch == '7')
		{
			static int k = 0;

			/* Prompt */
			prt("Command: Change object attr/chars", 15, 0);

			/* Hack -- query until done */
			while (1)
			{
				object_kind *k_ptr = &k_info[k];

				byte da = (byte)(k_ptr->d_attr);
				byte dc = (byte)(k_ptr->d_char);
				byte ca = (byte)(k_ptr->x_attr);
				byte cc = (byte)(k_ptr->x_char);

				/* Label the object */
				Term_putstr(5, 17, -1, TERM_WHITE,
				            format("Object = %d, Name = %-40.40s",
				                   k, (k_name + k_ptr->name)));

				/* Label the Default values */
				Term_putstr(10, 19, -1, TERM_WHITE,
				            format("Default attr/char = %3d / %3d", da, dc));
				Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 19, da, dc);

				if (use_bigtile)
				{
					if (da & 0x80)
						Term_putch(44, 19, 255, -1);
					else
						Term_putch(44, 19, 0, ' ');
				}

				/* Label the Current values */
				Term_putstr(10, 20, -1, TERM_WHITE,
				            format("Current attr/char = %3d / %3d", ca, cc));
				Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 20, ca, cc);

				if (use_bigtile)
				{
					if (ca & 0x80)
						Term_putch(44, 20, 255, -1);
					else
						Term_putch(44, 20, 0, ' ');
				}

				/* Prompt */
				Term_putstr(0, 22, -1, TERM_WHITE,
				            "Command (n/N/a/A/c/C/'s'hade): ");

				/* Get a command */
				cx = inkey();

				/* All done */
				if (cx == ESCAPE) break;

				/* Analyze */
				if (cx == 'n') k = (k + z_info->k_max + 1) % z_info->k_max;
				if (cx == 'N') k = (k + z_info->k_max - 1) % z_info->k_max;
				if (cx == 'a') k_info[k].x_attr = (byte)(ca + 1);
				if (cx == 'A') k_info[k].x_attr = (byte)(ca - 1);
				if (cx == 'c') k_info[k].x_char = (byte)(cc + 1);
				if (cx == 'C') k_info[k].x_char = (byte)(cc - 1);
				if (cx == 's')
				{
				  askfor_shade(&k_info[k].x_attr, 22);
				}
			}
		}

		/* Modify feature attr/chars */
		else if (ch == '8')
		{
			static int f = 0;

			/* Prompt */
			prt("Command: Change feature attr/chars", 15, 0);

			/* Hack -- query until done */
			while (1)
			{
				feature_type *f_ptr = &f_info[f];

				byte da = (byte)(f_ptr->d_attr);
				byte dc = (byte)(f_ptr->d_char);
				byte ca = (byte)(f_ptr->x_attr);
				byte cc = (byte)(f_ptr->x_char);

				/* Label the object */
				Term_putstr(5, 17, -1, TERM_WHITE,
				            format("Terrain = %d, Name = %-40.40s",
				                   f, (f_name + f_ptr->name)));

				/* Label the Default values */
				Term_putstr(10, 19, -1, TERM_WHITE,
				            format("Default attr/char = %3d / %3d", da, dc));
				Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 19, da, dc);

				if (use_bigtile)
				{
					if (da & 0x80)
						Term_putch(44, 19, 255, -1);
					else
						Term_putch(44, 19, 0, ' ');
				}

				/* Label the Current values */
				Term_putstr(10, 20, -1, TERM_WHITE,
				            format("Current attr/char = %3d / %3d", ca, cc));
				Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 20, ca, cc);

				if (use_bigtile)
				{
					if (ca & 0x80)
						Term_putch(44, 20, 255, -1);
					else
						Term_putch(44, 20, 0, ' ');
				}

				/* Prompt */
				Term_putstr(0, 22, -1, TERM_WHITE,
				            "Command (n/N/a/A/c/C/'s'hade): ");

				/* Get a command */
				cx = inkey();

				/* All done */
				if (cx == ESCAPE) break;

				/* Analyze */
				if (cx == 'n') f = (f + z_info->f_max + 1) % z_info->f_max;
				if (cx == 'N') f = (f + z_info->f_max - 1) % z_info->f_max;
				if (cx == 'a') f_info[f].x_attr = (byte)(ca + 1);
				if (cx == 'A') f_info[f].x_attr = (byte)(ca - 1);
				if (cx == 'c') f_info[f].x_char = (byte)(cc + 1);
				if (cx == 'C') f_info[f].x_char = (byte)(cc - 1);
				if (cx == 's')
				{
				  askfor_shade(&f_info[f].x_attr, 22);
				}
			}
		}

		/* Modify flavor attr/chars */
		else if (ch == '9')
		{
			static int f = 0;

			/* Prompt */
			prt("Command: Change flavor attr/chars", 15, 0);

			/* Hack -- query until done */
			while (1)
			{
				flavor_type *flavor_ptr = &flavor_info[f];

				byte da = (byte)(flavor_ptr->d_attr);
				byte dc = (byte)(flavor_ptr->d_char);
				byte ca = (byte)(flavor_ptr->x_attr);
				byte cc = (byte)(flavor_ptr->x_char);

				/* Label the object */
				Term_putstr(5, 17, -1, TERM_WHITE,
				            format("Flavor = %d, Text = %-40.40s",
				                   f, (flavor_text + flavor_ptr->text)));

				/* Label the Default values */
				Term_putstr(10, 19, -1, TERM_WHITE,
				            format("Default attr/char = %3d / %3d", da, dc));
				Term_putstr(40, 19, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 19, da, dc);
				Term_putch(43, 19, da, dc);

				if (use_bigtile)
				{
					if (da & 0x80)
						Term_putch(44, 19, 255, -1);
					else
						Term_putch(44, 19, 0, ' ');
				}

				/* Label the Current values */
				Term_putstr(10, 20, -1, TERM_WHITE,
				            format("Current attr/char = %3d / %3d", ca, cc));
				Term_putstr(40, 20, -1, TERM_WHITE, "<< ? >>");
				Term_putch(43, 20, ca, cc);

				if (use_bigtile)
				{
					if (ca & 0x80)
						Term_putch(44, 20, 255, -1);
					else
						Term_putch(44, 20, 0, ' ');
				}

				/* Prompt */
				Term_putstr(0, 22, -1, TERM_WHITE,
				            "Command (n/N/a/A/c/C/'s'hade): ");

				/* Get a command */
				cx = inkey();

				/* All done */
				if (cx == ESCAPE) break;

				/* Analyze */
				if (cx == 'n') f = (f + z_info->flavor_max + 1) % z_info->flavor_max;
				if (cx == 'N') f = (f + z_info->flavor_max - 1) % z_info->flavor_max;
				if (cx == 'a') flavor_info[f].x_attr = (byte)(ca + 1);
				if (cx == 'A') flavor_info[f].x_attr = (byte)(ca - 1);
				if (cx == 'c') flavor_info[f].x_char = (byte)(cc + 1);
				if (cx == 'C') flavor_info[f].x_char = (byte)(cc - 1);
				if (cx == 's')
				{
				  askfor_shade(&flavor_info[f].x_attr, 22);
				}
			}
		}

#endif /* ALLOW_VISUALS */

		/* Reset visuals */
		else if (ch == '0')
		{
			/* Reset */
			reset_visuals(TRUE);

			/* Message */
			msg_print("Visual attr/char tables reset.");
		}

		/* Unknown option */
		else
		{
			bell("Illegal command for visuals!");
		}

		/* Flush messages */
		message_flush();
	}


	/* Load screen */
	screen_load();
}


/*
 * Asks to the user for specific color values.
 * Returns TRUE if the color was modified.
 */
static bool askfor_color_values(int idx)
{
  	char str[10];

  	int k, r, g, b;

  	/* Get the default value */
  	sprintf(str, "%d", angband_color_table[idx][1]);

  	/* Query, check for ESCAPE */
  	if (!term_get_string("Red (0-255) ", str, sizeof(str))) return FALSE;

  	/* Convert to number */
  	r = atoi(str);

  	/* Check bounds */
  	if (r < 0) r = 0;
  	if (r > 255) r = 255;

  	/* Get the default value */
  	sprintf(str, "%d", angband_color_table[idx][2]);

  	/* Query, check for ESCAPE */
  	if (!term_get_string("Green (0-255) ", str, sizeof(str))) return FALSE;

  	/* Convert to number */
  	g = atoi(str);

  	/* Check bounds */
  	if (g < 0) g = 0;
  	if (g > 255) g = 255;

  	/* Get the default value */
  	sprintf(str, "%d", angband_color_table[idx][3]);

 	/* Query, check for ESCAPE */
  	if (!term_get_string("Blue (0-255) ", str, sizeof(str))) return FALSE;

 	/* Convert to number */
  	b = atoi(str);

  	/* Check bounds */
  	if (b < 0) b = 0;
  	if (b > 255) b = 255;

  	/* Get the default value */
  	sprintf(str, "%d", angband_color_table[idx][0]);

  	/* Query, check for ESCAPE */
  	if (!term_get_string("Extra (0-255) ", str, sizeof(str))) return FALSE;

  	/* Convert to number */
  	k = atoi(str);

  	/* Check bounds */
  	if (k < 0) k = 0;
  	if (k > 255) k = 255;

  	/* Do nothing if the color is not modified */
  	if ((k == angband_color_table[idx][0]) &&
        (r == angband_color_table[idx][1]) &&
        (g == angband_color_table[idx][2]) &&
        (b == angband_color_table[idx][3])) return FALSE;

  	/* Modify the color table */
 	angband_color_table[idx][0] = k;
 	angband_color_table[idx][1] = r;
 	angband_color_table[idx][2] = g;
  	angband_color_table[idx][3] = b;

  	/* Notify the changes */
  	return TRUE;
}


/* These two are used to place elements in the grid */
#define COLOR_X(idx) (((idx) / MAX_BASE_COLORS) * 5 + 1)
#define COLOR_Y(idx) ((idx) % MAX_BASE_COLORS + 6)

/* Hack - Note the cast to "int" to prevent overflow */
#define IS_BLACK(idx) \
((int)angband_color_table[idx][1] + (int)angband_color_table[idx][2] + \
 (int)angband_color_table[idx][3] == 0)

/* We show black as dots to see the shape of the grid */
#define BLACK_SAMPLE "..."

/*
 * The screen used to modify the color table. Only 128 colors can be modified.
 * The remaining entries of the color table are reserved for graphic mode.
 */
static void modify_colors(void)
{
	int x, y, idx, old_idx;
	char ch;
	char msg[100];

	/* Flags */
 	bool do_move, do_update;

  	/* Clear the screen */
  	Term_clear();

  	/* Draw the color table */
  	for (idx = 0; idx < MAX_COLORS; idx++)
  	{
    	/* Get coordinates, the x value is adjusted to show a fake cursor */
    	x = COLOR_X(idx) + 1;
    	y = COLOR_Y(idx);

    	/* Show a sample of the color */
    	if (IS_BLACK(idx)) c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
    	else c_put_str(idx, COLOR_SAMPLE, y, x);
  	}

  	/* Show screen commands and help */
  	y = 2;
  	x = 42;
  	c_put_str(TERM_WHITE, "Commands:", y, x);
  	c_put_str(TERM_WHITE, "ESC: Return", y + 2, x);
  	c_put_str(TERM_WHITE, "Arrows: Move to color", y + 3, x);
  	c_put_str(TERM_WHITE, "k,K: Incr,Decr extra value", y + 4, x);
  	c_put_str(TERM_WHITE, "r,R: Incr,Decr red value", y + 5, x);
  	c_put_str(TERM_WHITE, "g,G: Incr,Decr green value", y + 6, x);
  	c_put_str(TERM_WHITE, "b,B: Incr,Decr blue value", y + 7, x);
  	c_put_str(TERM_WHITE, "c: Copy from color", y + 8, x);
  	c_put_str(TERM_WHITE, "v: Set specific values", y + 9, x);
  	c_put_str(TERM_WHITE, "First column: base colors", y + 11, x);
  	c_put_str(TERM_WHITE, "Second column: first shade, etc.", y + 12, x);

  	c_put_str(TERM_WHITE, "Shades look like base colors in 16 color ports.",
      			23, 0);

  	/* Hack - We want to show the fake cursor */
  	do_move = TRUE;
	do_update = TRUE;

  	/* Start with the first color */
  	idx = 0;

  	/* Used to erase the old position of the fake cursor */
  	old_idx = -1;

  	while (1)
  	{
    	/* Movement request */
    	if (do_move)
    	{

      		/* Erase the old fake cursor */
      		if (old_idx >= 0)
      		{
				/* Get coordinates */
				x = COLOR_X(old_idx);
				y = COLOR_Y(old_idx);

				/* Draw spaces */
				c_put_str(TERM_WHITE, " ", y, x);
				c_put_str(TERM_WHITE, " ", y, x + 4);
      		}

      		/* Show the current fake cursor */
      		/* Get coordinates */
      		x = COLOR_X(idx);
      		y = COLOR_Y(idx);

      		/* Draw the cursor */
      		c_put_str(TERM_WHITE, ">", y, x);
      		c_put_str(TERM_WHITE, "<", y, x + 4);

      		/* Format the name of the color */
      		my_strcpy(msg, format("Color = %d (0x%02X), Name = %s", idx, idx,
	    	get_ext_color_name(idx)), sizeof(msg));

      		/* Show the name and some whitespace */
      		c_put_str(TERM_WHITE, format("%-40s", msg), 2, 0);
    	}

    	/* Color update request */
    	if (do_update)
    	{
      		/* Get coordinates, adjust x */
      		x = COLOR_X(idx) + 1;
      		y = COLOR_Y(idx);

      		/* Hack - Redraw the sample if needed */
      		if (IS_BLACK(idx)) c_put_str(TERM_WHITE, BLACK_SAMPLE, y, x);
      		else c_put_str(idx, COLOR_SAMPLE, y, x);

      		/* Notify the changes in the color table to the terminal */
      		Term_xtra(TERM_XTRA_REACT, 0);

      		/* The user is playing with white, redraw all */
      		if (idx == TERM_WHITE) Term_redraw();

      		/* Or reduce flickering by redrawing the changes only */
      		else Term_redraw_section(x, y, x + 2, y);
    	}

    	/* Common code, show the values in the color table */
    	if (do_move || do_update)
    	{
      		/* Format the view of the color values */
		  	my_strcpy(msg, format("K = %d / R,G,B = %d, %d, %d",
	    			angband_color_table[idx][0],
	    			angband_color_table[idx][1],
	    			angband_color_table[idx][2],
					angband_color_table[idx][3]), sizeof(msg));

			/* Show color values and some whitespace */
      		c_put_str(TERM_WHITE, format("%-40s", msg), 4, 0);

    	}

    	/* Reset flags */
    	do_move = FALSE;
    	do_update = FALSE;
    	old_idx = -1;

    	/* Get a command */
    	if (!get_com("Command: Modify colors ", &ch)) break;

    	switch(ch)
    	{
      		/* Down */
      		case '2':
			{
	  			/* Check bounds */
	  			if (idx + 1 >= MAX_COLORS) break;

	  			/* Erase the old cursor */
	  			old_idx = idx;

	  			/* Get the new position */
	  			++idx;

	  			/* Request movement */
	  			do_move = TRUE;
	  			break;
			}

      		/* Up */
      		case '8':
			{

				/* Check bounds */
	  			if (idx - 1 < 0) break;

	  			/* Erase the old cursor */
	  			old_idx = idx;

	  			/* Get the new position */
	  			--idx;

	  			/* Request movement */
	  			do_move = TRUE;
	  			break;
			}

      		/* Left */
      		case '4':
			{
	  			/* Check bounds */
	  			if (idx - 16 < 0) break;

	  			/* Erase the old cursor */
	  			old_idx = idx;

	  			/* Get the new position */
	  			idx -= 16;

	  			/* Request movement */
	  			do_move = TRUE;
	  			break;
			}

	  		/* Right */
      		case '6':
			{
	  			/* Check bounds */
	  			if (idx + 16 >= MAX_COLORS) break;

	  			/* Erase the old cursor */
	  			old_idx = idx;

	  			/* Get the new position */
	  			idx += 16;

	  			/* Request movement */
	  			do_move = TRUE;
	  			break;
			}

			/* Copy from color */
      		case 'c':
			{
	  			char str[10];
	  			int src;

	  			/* Get the default value, the base color */
	  			sprintf(str, "%d", GET_BASE_COLOR(idx));

	  			/* Query, check for ESCAPE */
	  			if (!term_get_string(format("Copy from color (0-%d, def. base) ",
					MAX_COLORS - 1), str, sizeof(str))) break;

	  			/* Convert to number */
	  			src = atoi(str);

	  			/* Check bounds */
	  			if (src < 0) src = 0;
	  			if (src >= MAX_COLORS) src = MAX_COLORS - 1;

	  			/* Do nothing if the colors are the same */
	  			if (src == idx) break;

	  			/* Modify the color table */
	  			angband_color_table[idx][0] = angband_color_table[src][0];
	  			angband_color_table[idx][1] = angband_color_table[src][1];
	  			angband_color_table[idx][2] = angband_color_table[src][2];
	  			angband_color_table[idx][3] = angband_color_table[src][3];

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

      		/* Increase the extra value */
      		case 'k':
			{
	  			/* Get a pointer to the proper value */
	  			byte *k_ptr = &angband_color_table[idx][0];

	  			/* Modify the value */
	  			*k_ptr = (byte)(*k_ptr + 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

      		/* Decrease the extra value */
      		case 'K':
			{

	  			/* Get a pointer to the proper value */
	  			byte *k_ptr = &angband_color_table[idx][0];

	  			/* Modify the value */
	  			*k_ptr = (byte)(*k_ptr - 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

      		/* Increase the red value */
      		case 'r':
			{
	  			/* Get a pointer to the proper value */
	  			byte *r_ptr = &angband_color_table[idx][1];

	  			/* Modify the value */
	  			*r_ptr = (byte)(*r_ptr + 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

      		/* Decrease the red value */
      		case 'R':
			{

	  			/* Get a pointer to the proper value */
	  			byte *r_ptr = &angband_color_table[idx][1];

	  			/* Modify the value */
	  			*r_ptr = (byte)(*r_ptr - 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

	  		/* Increase the green value */
      		case 'g':
			{
	  			/* Get a pointer to the proper value */
	  			byte *g_ptr = &angband_color_table[idx][2];

	  			/* Modify the value */
	  			*g_ptr = (byte)(*g_ptr + 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

	  		/* Decrease the green value */
      		case 'G':
			{
	  			/* Get a pointer to the proper value */
	  			byte *g_ptr = &angband_color_table[idx][2];

	  			/* Modify the value */
	  			*g_ptr = (byte)(*g_ptr - 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

	  		/* Increase the blue value */
      		case 'b':
			{
	  			/* Get a pointer to the proper value */
	  			byte *b_ptr = &angband_color_table[idx][3];

	  			/* Modify the value */
	  			*b_ptr = (byte)(*b_ptr + 1);

				/* Request update */
	  			do_update = TRUE;
	  			break;
			}

      		/* Decrease the blue value */
      		case 'B':
			{
	  			/* Get a pointer to the proper value */
	  			byte *b_ptr = &angband_color_table[idx][3];

				/* Modify the value */
	  			*b_ptr = (byte)(*b_ptr - 1);

	  			/* Request update */
	  			do_update = TRUE;
	  			break;
			}

	  		/* Ask for specific values */
      		case 'v':
			{
	  			do_update = askfor_color_values(idx);
	  			break;
			}
    	}
  	}
}


/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
	int ch;

	int i;

	FILE *fff;

	char buf[1024];

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Save screen */
	screen_save();

	/* Interact until done */
	while (1)
	{
		/* Clear screen */
		Term_clear();

		/* Ask for a choice */
		prt("Interact with Colors", 2, 0);

		/* Give some choices */
		prt("(1) Load a user pref file", 4, 5);
#ifdef ALLOW_COLORS
		prt("(2) Dump colors", 5, 5);
		prt("(3) Modify colors", 6, 5);
#endif /* ALLOW_COLORS */

		/* Prompt */
		prt("Command: ", 8, 0);

		/* Prompt */
		ch = inkey();

		/* Done */
		if (ch == ESCAPE) break;

		/* Load a user pref file */
		if (ch == '1')
		{
			/* Ask for and load a user pref file */
			do_cmd_pref_file_hack(8);

			/* Could skip the following if loading cancelled XXX XXX XXX */

			/* Mega-Hack -- React to color changes */
			Term_xtra(TERM_XTRA_REACT, 0);

			/* Mega-Hack -- Redraw physical windows */
			Term_redraw();
		}

#ifdef ALLOW_COLORS

		/* Dump colors */
		else if (ch == '2')
		{
			static cptr mark = "Colors";
			char ftmp[80];

			/* Prompt */
			prt("Command: Dump colors", 8, 0);

			/* Prompt */
			prt("File: ", 10, 0);

			/* Default filename */
			strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

			/* Get a filename */
			if (!askfor_aux(ftmp, sizeof(ftmp))) continue;

			/* Build the filename */
			path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp);

			/* Remove old colors */
			remove_old_dump(buf, mark);

			/* Append to the file */
			fff = my_fopen(buf, "a");

			/* Failure */
			if (!fff) continue;

			/* Output header */
			pref_header(fff, mark);

			/* Skip some lines */
			fprintf(fff, "\n\n");

			/* Start dumping */
			fprintf(fff, "# Color redefinitions\n\n");

			/* Dump colors */
			for (i = 0; i < 256; i++)
			{
				int kv = angband_color_table[i][0];
				int rv = angband_color_table[i][1];
				int gv = angband_color_table[i][2];
				int bv = angband_color_table[i][3];

				cptr name = "unknown";

				/* Skip non-entries */
				if (!kv && !rv && !gv && !bv) continue;

				/* Extract the color name */
				if (i < 16) name = color_names[i];

				/* Dump a comment */
				fprintf(fff, "# Color '%s'\n", name);

				/* Dump the monster attr/char info */
				fprintf(fff, "V:%d:0x%02X:0x%02X:0x%02X:0x%02X\n\n",
				        i, kv, rv, gv, bv);
			}

			/* All done */
			fprintf(fff, "\n\n\n\n");

			/* Output footer */
			pref_footer(fff, mark);

			/* Close */
			my_fclose(fff);

			/* Message */
			msg_print("Dumped color redefinitions.");
		}

		/* Edit colors */
		else if (ch == '3')
		{
			modify_colors();
		}

#endif /* ALLOW_COLORS */

		/* Unknown option */
		else
		{
			bell("Illegal command for colors!");
		}

		/* Flush messages */
		message_flush();
	}


	/* Load screen */
	screen_load();
}


 /*
  * Take notes.  There are two ways this can happen, either in the message recall or
  * a file.  The command can also be passed a string, which will automatically be
  * written. -CK-
  */
void do_cmd_note(char *note, int what_depth)
{
	char buf[120];
	char turn_string[16];

	int length, length_info;
	char info_note[40];
	char depths[10];

 	/* Default */
 	my_strcpy(buf, "", sizeof (buf));

 	/* If a note is passed, use that, otherwise accept user input. */
 	if (streq(note, ""))
	{
 	  	if (!term_get_string("Note: ", buf, 57)) return;
 	}
	else
	{
		my_strcpy(buf, note, sizeof(buf));
	}

 	/* Ignore empty notes */
 	if (!buf[0] || (buf[0] == ' ')) return;

	/* write it to the notes file */

	/*Artefacts use depth artefact created.  All others use player depth.*/

	/*get depth for recording\
	 */
	if (what_depth == 0)
	{
		my_strcpy(depths, "   Gates", sizeof (depths));
	}
	else if (what_depth == CHEST_LEVEL)
	{
		my_strcpy(depths, "   Chest", sizeof (depths));
	}
	else
	{
		comma_number(depths, what_depth * 50);
		strnfmt(depths, sizeof(depths), "%5s ft", depths);
	}

	comma_number(turn_string, playerturn);

	/* Make preliminary part of note */
	strnfmt(info_note, sizeof(info_note), "%7s  %s   ", turn_string, depths);

	/*write the info note*/
	my_strcat(notes_buffer, info_note, sizeof(notes_buffer));
	
	/*get the length of the notes*/
	length_info = strlen(info_note);
	length = strlen(buf);

	/*break up long notes*/
	if((length + length_info) > LINEWRAP)
	{
		bool keep_going = TRUE;
		int startpoint = 0;
		int endpoint, n;

		while (keep_going)
		{
			/*don't print more than the set linewrap amount*/
			endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;

			/*find a breaking point*/
			while (TRUE)
			{
				/*are we at the end of the line?*/
				if (endpoint >= length)
				{
					/*print to the end*/
					endpoint = length;
					keep_going = FALSE;
					break;
				}

				/* Mark the most recent space or dash in the string */
				else if ((buf[endpoint] == ' ') ||
						 (buf[endpoint] == '-')) break;

				/*no spaces in the line, so break in the middle of text*/
				else if (endpoint == startpoint)
				{
					endpoint = startpoint + LINEWRAP - strlen(info_note) + 1;
					break;
				}

				/* check previous char */
				endpoint--;
			}

			/*make a continued note if applicable*/
			if (startpoint) my_strcat(notes_buffer, "                    ", sizeof(notes_buffer));

			/* Write that line to file */
			for (n = startpoint; n <= endpoint; n++)
			{
				char ch;

				/* Ensure the character is printable */
				ch = (isprint(buf[n]) ? buf[n] : ' ');

				/* Write out the character */
				my_strcat(notes_buffer, format("%c", ch), sizeof(notes_buffer));

			}

			/*break the line*/
			my_strcat(notes_buffer, "\n", sizeof(notes_buffer));

			/*prepare for the next line*/
			startpoint = endpoint + 1;
		}

	}

	/* Add note to buffer */
	else
	{
		my_strcat(notes_buffer, format("%s\n", buf), sizeof(notes_buffer));
	}

}


/*
 * Mention the current version
 */
void do_cmd_version(void)
{
	/* Silly message */
	msg_format("You are playing %s %s.  Type '?' for more info.",
	           VERSION_NAME, VERSION_STRING);
}



/*
 * Array of feeling strings
 */
static cptr do_cmd_feeling_text[LEV_THEME_HEAD] =
{
	"Looks like any other level.",
	"You feel there is something special about this level.",
	"You have a superb feeling about this level.",
	"You have an excellent feeling...",
	"You have a very good feeling...",
	"You have a good feeling...",
	"You feel strangely lucky...",
	"You feel your luck is turning...",
	"You like the look of this place...",
	"This level can't be all bad...",
	"What a boring place..."
};


/*
 * Note that "feeling" is set to zero unless some time has passed.
 * Note that this is done when the level is GENERATED, not entered.
 */
void do_cmd_feeling(void)
{

	/* No useful feeling on the surface */
	if (!p_ptr->depth)
	{
		msg_print("You stand once again upon the surface. Freedom awaits.");
		return;
	}

	/* No useful feelings until enough time has passed */
	if (!do_feeling)
	{
		msg_print("You are still uncertain about this level...");
		return;
	}

	/* Display the feeling */
	else msg_print(do_cmd_feeling_text[feeling]);
}



/*
 * Array of feeling strings
 */
static cptr do_cmd_challenge_text[14] =
{
	"challenges you from beyond the grave!",
	"thunders 'Prove worthy of your traditions - or die ashamed!'.",
	"desires to test your mettle!",
	"has risen from the dead to test you!",
	"roars 'Fight, or know yourself for a coward!'.",
	"summons you to a duel of life and death!",
	"desires you to know that you face a mighty champion of yore!",
	"demands that you prove your worthiness in combat!",
	"calls you unworthy of your ancestors!",
	"challenges you to a deathmatch!",
	"walks Middle-Earth once more!",
	"challenges you to demonstrate your prowess!",
	"demands you prove yourself here and now!",
	"asks 'Can ye face the best of those who came before?'."
};




/*
 * Personalize, randomize, and announce the challenge of a player ghost. -LM-
 */
void ghost_challenge(void)
{
	monster_race *r_ptr = &r_info[r_ghost];

	/*paranoia*/
	/* Check there is a name/ghost first */
	if (ghost_name[0] == '\0')
	{
		/*there wasn't a ghost*/
		bones_selector = 0;
		return;
	}

	msg_format("%^s, the %^s %s", ghost_name, r_name + r_ptr->name,
		do_cmd_challenge_text[rand_int(14)]);

	message_flush();
}


/*display the notes file*/
void do_cmd_knowledge_notes(void)
{
	show_buffer(notes_buffer, "Notes", 0);
}

/*
 * Hack -- save a screen dump to a file
 */
void do_cmd_save_screen(void)
{
	char tmp_val[256];
		
	/* Ask for a file */
	sprintf(tmp_val, "%s.html", op_ptr->base_name);
	if (!term_get_string("File: ", tmp_val, sizeof(tmp_val))) return;
	
	html_screenshot(tmp_val);
	msg_print("HTML screenshot saved.");
}

/*
 * Description of each object group.
 */
static cptr object_group_text[] =
{
	"Herbs",
	"Potions",
	"Rings",
	"Amulets",
	"Staves",
	"Horns",
	"Swords",
	"Axes & Polearms",
	"Blunt Weapons",
	"Diggers",
	"Bows",
//	"Arrows",
	"Light Sources",
	"Soft Armour",
	"Mail",
	"Shields",
	"Cloaks",
	"Gloves",
	"Helms",
	"Crowns",
	"Boots",
	"Chests",
	NULL
};



/*
 * TVALs of items in each group
 */
static byte object_group_tval[] =
{
	TV_FOOD,
	TV_POTION,
	TV_RING,
	TV_AMULET,
	TV_STAFF,
	TV_HORN,
	TV_SWORD,
	TV_POLEARM,
	TV_HAFTED,
	TV_DIGGING,
	TV_BOW,
//	TV_ARROW,
	TV_LIGHT,
	TV_SOFT_ARMOR,
	TV_MAIL,
	TV_SHIELD,
	TV_CLOAK,
	TV_GLOVES,
	TV_HELM,
	TV_CROWN,
	TV_BOOTS,
	TV_CHEST,
	0
};

/*
 * Build a list of objects indexes in the given group. Return the number
 * of objects in the group. object_idx[] must be one element larger than the
 * largest number of objects that will be collected.
 *  (Incorporates some code from jdh)
 */
static int collect_objects(int grp_cur, object_list_entry object_idx[])
{
	int i, j, k, object_cnt = 0;
	int max_sval = -1;
	int norm = 0;
	bool known_sval[256] = {};

	/* Get a list of x_char in this group */
	byte group_tval = object_group_tval[grp_cur];

	/* Check every object */
	for (i = 0; i < z_info->k_max; i++)
	{
		/* Access the object type */
		object_kind *k_ptr = &k_info[i];

		/*used to check for allocation*/
		k = 0;

		/* Skip empty objects */
		if (!k_ptr->name) continue;

		/* Skip items with no distribution (including special artefacts) */
		/* Scan allocation pairs */
		for (j = 0; j < 4; j++)
		{
			/*add the rarity, if there is one*/
			k += k_ptr->chance[j];
		}
		/*not in allocation table*/
		if (!(k))  continue;

		/* Require objects ever seen*/
		//if (!(k_ptr->aware && k_ptr->everseen)) continue;
		if (!(k_ptr->everseen)) continue;

		/* Check for object in the group */
		if (k_ptr->tval == group_tval)
		{
			known_sval[k_ptr->sval] = TRUE;

			/* Save the highest sval in the group for later */
			if (k_ptr->sval > max_sval)
			{
				max_sval = k_ptr->sval;
			}

			/* Add the object type */
			if (object_idx)
			{
			    object_idx[object_cnt].type = OBJ_NORMAL;
			    object_idx[object_cnt].idx = i;
			}
			
			object_cnt++;
		}
	}

	norm = object_cnt;

	/* Add special items to the list */
	/* Skip this part if we don't know any normal items */
	for (i = 0; object_cnt > 0 && i < z_info->e_max; i++)
	{
		/* Access the object type */
		ego_item_type *e_ptr = &e_info[i];

		/* Skip empty objects */
		if (!e_ptr->name) continue;

		/* Require objects ever seen*/
		if (!(e_ptr->everseen)) continue;

		/* Check for object in the group */
		for (j = 0; j < EGO_TVALS_MAX; j++)
		{
			if (e_ptr->tval[j] == group_tval)
			{
				if (object_idx)
				{
					object_idx[object_cnt].type = OBJ_SPECIAL;
					object_idx[object_cnt].idx = -1;
					object_idx[object_cnt].e_idx = i;
					object_idx[object_cnt].tval = group_tval;
					object_idx[object_cnt].sval = -1;
				}
				object_cnt++;

				break;
			}
		}
	}

	/* Terminate the list */
	if (object_idx) object_idx[object_cnt].type = OBJ_NONE;

	/* Return the number of object types */
	return object_cnt;
}



/*
 * Build a list of artefact indexes in the given group. Return the number
 * of eligible artefacts in that group.
 */
static int collect_artefacts(int grp_cur, int object_idx[])
{
	int i, object_cnt = 0;
	bool *okay;
	bool know_all = cheat_know || p_ptr->active_ability[S_PER][PER_LORE2];

	/* Get a list of x_char in this group */
	byte group_tval = object_group_tval[grp_cur];

	/*make a list of artefacts not found*/
	/* Allocate the "object_idx" array */
	C_MAKE(okay, z_info->art_max, bool);

	/* Default first,  */
	for (i = 0; i < z_info->art_max; i++)
	{
		artefact_type *a_ptr = &a_info[i];

		/*start with false*/
		okay[i] = FALSE;

		/* Skip "empty" artefacts */
		if (a_ptr->tval + a_ptr->sval == 0) continue;

		/* Skip "unfound" artefacts, unless in wizard mode or with Lore Mastery or cheating */
		if (!know_all && !p_ptr->wizard && !a_ptr->found_num) continue;

		/* Skip "ungenerated" artefacts, unless with Lore Mastery or cheating */
		if (!know_all && !a_ptr->cur_num) continue;

		/* Skip the later versions of the Iron Crown */
		if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1) || (i == ART_MORGOTH_2)) continue;

		/* Skip the special smithing template artefacts */
		if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max)) continue;

		/*assume all created artefacts are good at this point*/
		okay[i] = TRUE;
	}

	/* Finally, go through the list of artefacts and categorize the good ones */
	for (i = 0; i < z_info->art_max; i++)
	{
		/* Access the artefact */
		artefact_type *a_ptr = &a_info[i];

		/* Skip empty artefacts */
		if (a_ptr->tval + a_ptr->sval == 0) continue;

		/* Require artefacts ever seen*/
		if (okay[i] == FALSE) continue;

		/* Check for race in the group */
		if (a_ptr->tval == group_tval)
		{
			/* Add the race */
			object_idx[object_cnt++] = i;
		}
	}

	/* Terminate the list */
	object_idx[object_cnt] = 0;

	/*clear the array*/
	KILL(okay);

	/* Return the number of races */
	return object_cnt;

}


/*
 * Display the object groups.
 */
static void display_group_list(int col, int row, int wid, int per_page,
	int grp_idx[], cptr group_text[], int grp_cur, int grp_top)
{
	int i;

	/* Display lines until done */
	for (i = 0; i < per_page && (grp_idx[i] >= 0); i++)
	{
		/* Get the group index */
		int grp = grp_idx[grp_top + i];

		/* Choose a color */
		byte attr = (grp_top + i == grp_cur) ? TERM_L_BLUE : TERM_WHITE;

		/* Erase the entire line */
		Term_erase(col, row + i, wid);

		/* Display the group label */
		c_put_str(attr, group_text[grp], row + i, col);
	}
}

/*
 * Move the cursor in a browser window
 */
static void browser_cursor(char ch, int *column, int *grp_cur, int grp_cnt,
						   int *list_cur, int list_cnt)
{
	int d;
	int col = *column;
	int grp = *grp_cur;
	int list = *list_cur;

	/* Extract direction */
	d = target_dir(ch);

	if (!d) return;

	/* Diagonals - hack */
	if ((ddx[d] > 0) && ddy[d])
	{
		/* Browse group list */
		if (!col)
		{
			int old_grp = grp;

			/* Move up or down */
			grp += ddy[d] * BROWSER_ROWS;

			/* Verify */
			if (grp >= grp_cnt)	grp = grp_cnt - 1;
			if (grp < 0) grp = 0;
			if (grp != old_grp)	list = 0;
		}

		/* Browse sub-list list */
		else
		{
			/* Move up or down */
			list += ddy[d] * BROWSER_ROWS;

			/* Verify */
			if (list >= list_cnt) list = list_cnt - 1;
			if (list < 0) list = 0;
		}

		(*grp_cur) = grp;
		(*list_cur) = list;

		return;
	}

	if (ddx[d])
	{
		col += ddx[d];
		if (col < 0) col = 0;
		if (col > 1) col = 1;

		(*column) = col;

		return;
	}

	/* Browse group list */
	if (!col)
	{
		int old_grp = grp;

		/* Move up or down */
		grp += ddy[d];

		/* Verify */
		if (grp >= grp_cnt)	grp = grp_cnt - 1;
		if (grp < 0) grp = 0;
		if (grp != old_grp)	list = 0;
	}

	/* Browse sub-list list */
	else
	{
		/* Move up or down */
		list += ddy[d];

		/* Verify */
		if (list >= list_cnt) list = list_cnt - 1;
		if (list < 0) list = 0;
	}

	(*grp_cur) = grp;
	(*list_cur) = list;
}

/*
 * Hack -- Create a "forged" artefact
 */
static bool prepare_fake_artefact(object_type *o_ptr, byte name1)
{
	s16b i;

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

	/*identify it*/
	object_known(o_ptr);

	/*make it a spoiler item*/
	o_ptr->ident |= IDENT_SPOIL;

	/* Hack -- extract the "cursed" flag */
	if (a_ptr->flags3 & (TR3_LIGHT_CURSE)) o_ptr->ident |= (IDENT_CURSED);

	/* Success */
	return (TRUE);
}


/*
 * Describe fake artefact
 */
void desc_art_fake(int a_idx)
{
	object_type *i_ptr;
	object_type object_type_body;

	/* Get local object */
	i_ptr = &object_type_body;

	/* Wipe the object */
	object_wipe(i_ptr);

	/* Make fake artefact */
	prepare_fake_artefact(i_ptr, a_idx);

	/* Hack -- Handle stuff */
	handle_stuff();

	/* Reset the cursor */
	Term_gotoxy(0, 0);

	object_info_screen(i_ptr);
}

/*
 * Display the objects in a group.
 */
static void display_artefact_list(int col, int row, int per_page, int object_idx[],
	int object_cur, int object_top)
{
	int i;
	char o_name[80];
	object_type *i_ptr;
	object_type object_type_body;

	/* Display lines until done */
	for (i = 0; i < per_page && object_idx[i]; i++)
	{
		/* Get the object index */
		int a_idx = object_idx[object_top + i];

		/* Choose a color */
		byte attr = TERM_WHITE;
		byte cursor = TERM_L_BLUE;
		attr = ((i + object_top == object_cur) ? cursor : attr);

		/* Get local object */
		i_ptr = &object_type_body;

		/* Wipe the object */
		object_wipe(i_ptr);

		/* Make fake artefact */
		prepare_fake_artefact(i_ptr, a_idx);

		/* Get its name */
		object_desc(o_name, sizeof(o_name), i_ptr, TRUE, 0);

		/* Display the name */
		c_prt(attr, o_name, row + i, col);


		if (cheat_know)
		{
			artefact_type *a_ptr = &a_info[a_idx];

			c_prt(attr, format ("%3d", a_idx), row + i, 68);
			c_prt(attr, format ("%3d", a_ptr->level), row + i, 72);
			c_prt(attr, format ("%3d", a_ptr->rarity), row + i, 76);

		}

	}

	/* Clear remaining lines */
	for (; i < per_page; i++)
	{
		Term_erase(col, row + i, 255);
	}
}

/*
 * Display known artefacts
 */
void do_cmd_knowledge_artefacts(void)
{
	int i, len, max;
	int grp_cur, grp_top;
	int artefact_old, artefact_cur, artefact_top;
	int grp_cnt, grp_idx[100];
	int artefact_cnt;
	int *artefact_idx;

	int column = 0;
	bool flag;
	bool redraw;

	/* Allocate the "artefact_idx" array */
	C_MAKE(artefact_idx, z_info->art_max, int);

	max = 0;
	grp_cnt = 0;

	/* Check every group */
	for (i = 0; object_group_text[i] != NULL; i++)
	{
		/* Measure the label */
		len = strlen(object_group_text[i]);

		/* Save the maximum length */
		if (len > max) max = len;

		/* See if artefact are known */
		if (collect_artefacts(i, artefact_idx))
		{
			/* Build a list of groups with known artefacts */
			grp_idx[grp_cnt++] = i;
		}
	}

	/* Terminate the list */
	grp_idx[grp_cnt] = -1;

	grp_cur = grp_top = 0;
	artefact_cur = artefact_top = 0;
	artefact_old = -1;

	flag = FALSE;
	redraw = TRUE;

	while (!flag)
	{
		char ch;

		if (redraw)
		{
			clear_from(0);

			prt("Knowledge - Artefacts", 2, 0);
			prt("Group", 4, 0);
			prt("Name", 4, max + 3);

			if (cheat_know)
			{
				prt("Idx", 4, 68);
				prt("Dep", 4, 72);
				prt("Rar", 4, 76);
			}

			for (i = 0; i < 78; i++)
			{
				Term_putch(i, 5, TERM_L_DARK, '=');
			}

			for (i = 0; i < BROWSER_ROWS; i++)
			{
				Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');
			}

			redraw = FALSE;
		}

		/* Scroll group list */
		if (grp_cur < grp_top) grp_top = grp_cur;
		if (grp_cur >= grp_top + BROWSER_ROWS) grp_top = grp_cur - BROWSER_ROWS + 1;

		/* Scroll artefact list */
		if (artefact_cur < artefact_top) artefact_top = artefact_cur;
		if (artefact_cur >= artefact_top + BROWSER_ROWS) artefact_top = artefact_cur - BROWSER_ROWS + 1;

		/* Display a list of object groups */
		display_group_list(0, 6, max, BROWSER_ROWS, grp_idx, object_group_text, grp_cur, grp_top);

		/* Get a list of objects in the current group */
		artefact_cnt = collect_artefacts(grp_idx[grp_cur], artefact_idx);

		/* Display a list of objects in the current group */
		display_artefact_list(max + 3, 6, BROWSER_ROWS, artefact_idx, artefact_cur, artefact_top);

		/* Prompt */
		Term_putstr(1, 23, -1, TERM_SLATE, "<dir>   recall   ESC");
		Term_putstr(1, 23, -1, TERM_L_WHITE, "<dir>");
		Term_putstr(9, 23, -1, TERM_L_WHITE, "r");
		Term_putstr(18, 23, -1, TERM_L_WHITE, "ESC");
		
		/* The "current" object changed */
		if (artefact_old != artefact_idx[artefact_cur])
		{
			/* Hack -- handle stuff */
			handle_stuff();

			/* Remember the "current" object */
			artefact_old = artefact_idx[artefact_cur];
		}

		if (!column)
		{
			Term_gotoxy(0, 6 + (grp_cur - grp_top));
		}
		else
		{
			Term_gotoxy(max + 3, 6 + (artefact_cur - artefact_top));
		}

		ch = inkey();

		switch (ch)
		{
			case ESCAPE:
			{
				flag = TRUE;
				break;
			}

			case 'R':
			case 'r':
			{
				/* Recall on screen */
				desc_art_fake(artefact_idx[artefact_cur]);

				redraw = TRUE;
				break;
			}

			default:
			{
				/* Move the cursor */
				browser_cursor(ch, &column, &grp_cur, grp_cnt, &artefact_cur, artefact_cnt);
				break;
			}
		}
	}

	/* XXX XXX Free the "object_idx" array */
	KILL(artefact_idx);
}

/*
 * Description of each monster group.
 */
static cptr monster_group_text[] =
{
	"Uniques",						/*All uniques, all letters*/
	/*Unused*/						/*'a'*/
	/*Unused*/						/*'A'*/
	"Bats & Birds",					/*'b'*/
	/*Unused*/						/*'B'*/
	/*Unused*/						/*'c'*/
	"Canines",						/*'C'*/
	"Young Dragons",				/*'d'*/
	"Great Dragons",				/*'D'*/
	/*Unused*/						/*'e'*/
	/*Unused*/						/*'E'*/
	"Felines",						/*'f'*/
	/*Unused*/						/*'F'*/
	/*Unused*/						/*'g'*/
	"Giants",						/*'G'*/
	/*Unused*/						/*'h'*/
	"Horrors",						/*'H'*/
	/*Unused*/						/*'i'*/
	"Insects",						/*'I'*/
	/*Unused*/						/*'j'*/
	/*Unused*/						/*'J'*/
	/*Unused*/						/*'k'*/
	/*Unused*/						/*'K'*/
	/*Unused*/						/*'l'*/
	/*Unused*/						/*'L'*/
	"Young Spiders",				/*'m'*/
	"Spiders",						/*'M'*/
	/*Unused*/						/*'n'*/
	"Nameless Things",				/*'N'*/
	"Orcs",							/*'o'*/
	/*Unused*/						/*'O'*/
	/*Unused*/						/*'p'*/
	/*Unused*/						/*'P'*/
	/*Unused*/						/*'q'*/
	/*Unused*/						/*'Q'*/
	/*Unused*/						/*'r'*/
	"Raukar",						/*'R'*/
	"Serpents",						/*'s'*/
	"Ancient Serpents",				/*'S'*/
	/*Unused*/						/*'t'*/
	"Trolls",						/*'T'*/
	/*Unused*/						/*'u'*/
	/*Unused*/						/*'U'*/
	"Vampires",						/*'v'*/
	"Valar",						/*'V'*/
	"Creeping Shadows",             /*'w'*/
	"Wights and Wraiths",			/*'W'*/
	/*Unused*/						/*'x'*/
	/*Unused*/						/*'X'*/
	/*Unused*/						/*'y'*/
	/*Unused*/						/*'Y'*/
	/*Unused*/						/*'Z'*/
	/*Unused*/						/*'Z'*/
	"Plants",						/*'&'*/
	"People",						/*'@'*/
	"Blades",					    /*'|'*/
	NULL
};

/*
 * Symbols of monsters in each group. Note the "Uniques" group
 * is handled differently.
 */
static cptr monster_group_char[] =
{
	(char *) -1L,
	/*"a", Unused*/
	/*"A", Unused*/
	"b",
	/*"B", Unused*/
	/*"c", Unused*/
	"C",
	"d",
	"D",
	/*"e", Unused*/
	/*"E", Unused*/
	"f",
	/*"F", Unused*/
	/*"g", Unused*/
	"G",
	/*"h", Unused*/
	"H",
	/*"i", Unused*/
	"I",
	/*"j", Unused*/
	/*"J", Unused*/
	/*"k", Unused*/
	/*"K", Unused*/
	/*"l", Unused*/
	/*"L", Unused*/
	"m",
	"M",
	/*"n", Unused*/
	"N",
	"o",
	/*"O", Unused*/
	/*"p", Unused*/
	/*"P", Unused*/
	/*"q", Unused*/
	/*"Q", Unused*/
	/*"r", Unused*/
	"R",
	"s",
	"S",
	/*"t", Unused*/
	"T",
	/*"u", Unused*/
	/*"U", Unused*/
	"v",
	"V",
	"w",
	"W",
	/*"x", Unused*/
	/*"X", Unused*/
	/*"y", Unused*/
	/*"Y", Unused*/
	/*"z", Unused*/
	/*"Z", Unused*/
	"&",  // plants
	"@",  // human/elf/dwarf
	"|",  // deathblades
	NULL
};

/*
 * Build a list of monster indexes in the given group. Return the number
 * of monsters in the group.
 */
static int collect_monsters(int grp_cur, monster_list_entry *mon_idx, int mode)
{
	int i, mon_count = 0;

	/* Get a list of x_char in this group */
	cptr group_char = monster_group_char[grp_cur];

	/* XXX Hack -- Check if this is the "Uniques" group */
	bool grp_unique = (monster_group_char[grp_cur] == (char *) -1L);
	
	/* Check every race */
	for (i = 1; i < z_info->r_max; i++)
	{
		/* Access the race */
		monster_race *r_ptr = &r_info[i];
		monster_lore *l_ptr = &l_list[i];

		/* Is this a unique? */
		bool unique = (r_ptr->flags1 & (RF1_UNIQUE));

		/* Skip empty race */
		if (!r_ptr->name) continue;

		if (grp_unique && !(unique)) continue;

		/* Require known monsters */
		if (!(mode & 0x02) && (!cheat_know) && (!p_ptr->active_ability[S_PER][PER_LORE2]) && (!(l_ptr->tsights))) continue;

		// Ignore monsters that can't be generated
		if (r_ptr->level > 25) continue;

		/* Check for race in the group */
		if ((grp_unique) || (strchr(group_char, r_ptr->d_char)))
		{
			/* Add the race */
			mon_idx[mon_count++].r_idx = i;
						
			/* XXX Hack -- Just checking for non-empty group */
			if (mode & 0x01) break;
		}
	}

	/* Terminate the list */
	mon_idx[mon_count].r_idx = 0;

	/* Return the number of races */
	return (mon_count);
}

/*
 * Display the monsters in a group.
 */
static void display_monster_list(int col, int row, int per_page, monster_list_entry *mon_idx,
	int mon_cur, int mon_top, int grp_cur)
{
	int i;

	u32b known_uniques, dead_uniques, slay_count;

	/* Start with 0 kills*/
	known_uniques = dead_uniques = slay_count = 0;

	/* Count up monster kill counts */
	for (i = 1; i < z_info->r_max - 1; i++)
	{
		monster_race *r_ptr = &r_info[i];
		monster_lore *l_ptr = &l_list[i];

		// skip monsters that cannot be generated
		if ((r_ptr->rarity == 0) || (r_ptr->level > 25)) continue;
		
		/* Require non-unique monsters */
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			/*Count if we have seen the unique*/
			if (l_ptr->tsights)
			{
				known_uniques++;

				/*Count if the unique is dead*/
				if (r_ptr->max_num == 0)
				{
					dead_uniques++;
					slay_count++;
				}
			}
			
			// increase the uniques count anyway for loremasters or cheaters
			else if (p_ptr->active_ability[S_PER][PER_LORE2] || cheat_know)
			{
				known_uniques++;
			}

		}

		/* Collect "appropriate" monsters */
		else slay_count += l_ptr->pkills;
	}

	/* Display lines until done */
	for (i = 0; i < per_page && mon_idx[i].r_idx; i++)
	{
		byte attr;

		/* Get the race index */
		int r_idx = mon_idx[mon_top + i].r_idx;

		/* Access the race */
		monster_race *r_ptr = &r_info[r_idx];
		monster_lore *l_ptr = &l_list[r_idx];

		char race_name[80];

		/* Get the monster race name (singular)*/
		monster_desc_race(race_name, sizeof(race_name), r_idx);

		/* Choose a color */
		attr = ((i + mon_top == mon_cur) ? TERM_L_BLUE : TERM_WHITE);

		/* Display the name */
		c_prt(attr, race_name, row + i, col);

		if (cheat_know)
		{
			c_prt(attr, format ("%d", r_idx), row + i, 60);
		}

		/* Display symbol */
		Term_putch(68, row + i, r_ptr->x_attr, r_ptr->x_char);

		/* Display kills */
		if (r_ptr->flags1 & (RF1_UNIQUE))
		{
			/*use alive/dead for uniques*/
			put_str(format("%s", (r_ptr->max_num == 0) ? " dead" : "alive"),
			        row + i, 73);
		}
		else put_str(format("%5d", l_ptr->pkills), row + i, 73);

	}

	/* Clear remaining lines */
	for (; i < per_page; i++)
	{
		Term_erase(col, row + i, 255);
	}

	/*Clear the monster count line*/
	Term_erase(0, 23, 255);

	if (monster_group_char[grp_cur] != (char *) -1L)
   	{

		c_put_str(TERM_L_BLUE, format("Total Creatures Slain: %d. ", slay_count), 23, col+2);
	}
	else
	{
		c_put_str(TERM_L_BLUE, format("Known Uniques: %d, Slain Uniques: %d.", known_uniques, dead_uniques),
						23, col+2);
	}
}


/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
	int i, len, max;
	int grp_cur, grp_top;
	int mon_cur, mon_top;
	int grp_cnt, grp_idx[100];
	monster_list_entry *mon_idx;
	int monster_count;

	int column = 0;
	bool flag;
	bool redraw;

	/* Allocate the "mon_idx" array */
	C_MAKE(mon_idx, z_info->r_max, monster_list_entry);

	max = 0;
	grp_cnt = 0;

	/* Check every group */
	for (i = 0; monster_group_text[i] != NULL; i++)
	{

		/* Measure the label */
		len = strlen(monster_group_text[i]);

		/* Save the maximum length */
		if (len > max) max = len;

		/* See if any monsters are known */
		if ((monster_group_char[i] == ((char *) -1L)) || collect_monsters(i, mon_idx, 0x01))
		{
			/* Build a list of groups with known monsters */
			grp_idx[grp_cnt++] = i;
		}

	}

	/* Terminate the list */
	grp_idx[grp_cnt] = -1;

	grp_cur = grp_top = 0;
	mon_cur = mon_top = 0;

	flag = FALSE;
	redraw = TRUE;

	while (!flag)
	{
		char ch;

		if (redraw)
		{
			clear_from(0);

			prt("Knowledge - Monsters", 2, 0);
			prt("Group", 4, 0);
			prt("Name", 4, max + 3);
			if (cheat_know) prt("Idx", 4, 60);
			prt("Sym   Kills", 4, 67);

			for (i = 0; i < 78; i++)
			{
				Term_putch(i, 5, TERM_L_DARK, '=');
			}

			for (i = 0; i < BROWSER_ROWS; i++)
			{
				Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');
			}

			redraw = FALSE;
		}

		/* Scroll group list */
		if (grp_cur < grp_top) grp_top = grp_cur;
		if (grp_cur >= grp_top + BROWSER_ROWS) grp_top = grp_cur - BROWSER_ROWS + 1;

		/* Scroll monster list */
		if (mon_cur < mon_top) mon_top = mon_cur;
		if (mon_cur >= mon_top + BROWSER_ROWS) mon_top = mon_cur - BROWSER_ROWS + 1;

		/* Display a list of monster groups */
		display_group_list(0, 6, max, BROWSER_ROWS, grp_idx, monster_group_text, grp_cur, grp_top);

		/* Get a list of monsters in the current group */
		monster_count = collect_monsters(grp_idx[grp_cur], mon_idx, 0x00);

		/* Display a list of monsters in the current group */
		display_monster_list(max + 3, 6, BROWSER_ROWS, mon_idx, mon_cur, mon_top, grp_cur);

		/* Track selected monster, to enable recall in sub-win*/
		p_ptr->monster_race_idx = mon_idx[mon_cur].r_idx;

		/* Prompt */
		Term_putstr(1, 23, -1, TERM_SLATE, "<dir>   recall   ESC");
		Term_putstr(1, 23, -1, TERM_L_WHITE, "<dir>");
		Term_putstr(9, 23, -1, TERM_L_WHITE, "r");
		Term_putstr(18, 23, -1, TERM_L_WHITE, "ESC");
		
		/* Hack -- handle stuff */
		handle_stuff();

		if (!column)
		{
			Term_gotoxy(0, 6 + (grp_cur - grp_top));
		}
		else
		{
			Term_gotoxy(max + 3, 6 + (mon_cur - mon_top));
		}

		ch = inkey();

		switch (ch)
		{
			case ESCAPE:
			{
				flag = TRUE;
				break;
			}

			case 'R':
			case 'r':
			{
				/* Recall on screen */
				if (mon_idx[mon_cur].r_idx)
				{
					screen_roff(mon_idx[mon_cur].r_idx);

					(void) inkey();

					redraw = TRUE;
				}
				break;
			}

			default:
			{
				/* Move the cursor */
				browser_cursor(ch, &column, &grp_cur, grp_cnt, &mon_cur, monster_count);

				/*Update to a new monster*/
				p_ptr->window |= (PW_MONSTER);

				break;
			}
		}
	}

	/* XXX XXX Free the "mon_idx" array */
	KILL(mon_idx);
}

/*
 * Add a pval so the object descriptions don't look strange*
 */
void apply_magic_fake(object_type *o_ptr)
{
	/* Analyze type */
	switch (o_ptr->tval)
	{
		case TV_DIGGING:
		{
			if (o_ptr->pval < 1) o_ptr->pval = 1;
			break;
		}

		/*many rings need a pval*/
		case TV_RING:
		{
			/* Analyze */
			switch (o_ptr->sval)
			{
				/* Strength, Dexterity */
				case SV_RING_STR:
				case SV_RING_DEX:
				{
					if (o_ptr->pval < 1) o_ptr->pval = 1;

					break;
				}

				/* Ring of damage */
				case SV_RING_DAMAGE:
				{
					if (o_ptr->pval < 1) o_ptr->pval = 1;
					
					break;
				}

				/* Ring of Accuracy */
				case SV_RING_ACCURACY:
				{
					/* Bonus to hit */
					if (o_ptr->att < 1) o_ptr->att = 1;
		
					break;
				}

				/* Ring of Protection */
				case SV_RING_PROTECTION:
				{
					/* Bonus to protection */
					o_ptr->pd = 1;
					if (o_ptr->ps < 1) o_ptr->ps = 1;
			
					break;
				}

				/* Ring of Evasion */
				case SV_RING_EVASION:
				{
					/* Bonus to evasion */
					if (o_ptr->evn < 1) o_ptr->evn = 1;
								
					break;
				}

				/* Ring of Perception */
				case SV_RING_PERCEPTION:
				{
					/* Bonus to perception */
					if (o_ptr->pval < 1) o_ptr->pval = 1;
								
					break;
				}
			}

			/*break for TVAL-Rings*/
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
					if (o_ptr->pval < 1) o_ptr->pval = 1;
					break;
				}

				/* Amulet of the Blessed Realm */
				case SV_AMULET_BLESSED_REALM:
				{
					if (o_ptr->pval < 1) o_ptr->pval = 1;
					break;
				}

				default: break;

			}
			/*break for TVAL-Amulets*/
			break;
		}

		case TV_LIGHT:
		{
			/* Analyze */
			switch (o_ptr->sval)
			{
				case SV_LIGHT_TORCH:
				case SV_LIGHT_LANTERN:
				{
					o_ptr->timeout = 0;

					break;
				}

			}
			/*break for TVAL-Lights*/
			break;
		}

		/*give them one charge*/
		case TV_STAFF:
		{
			if (o_ptr->pval < 1) o_ptr->pval = 1;

			break;
		}

	}

}

/*
 * Describe fake object
 */
static void desc_obj_fake(int k_idx)
{
	object_type *i_ptr;
	object_type object_type_body;

	/* Get local object */
	i_ptr = &object_type_body;

	/* Wipe the object */
	object_wipe(i_ptr);

	/* Create the object */
	object_prep(i_ptr, k_idx);

	/*add minimum bonuses so the descriptions don't look strange*/
	apply_magic_fake(i_ptr);

	/* It's fully known */
	i_ptr->ident |= IDENT_KNOWN;

	/* Hack -- Handle stuff */
	handle_stuff();

	/* Reset the cursor */
	Term_gotoxy(0, 0);

	object_info_screen(i_ptr);
}

/*
 * Display the objects in a group. (Incorporates some code from jdh)
 */
static void display_object_list(int col, int row, int per_page, object_list_entry object_idx[],
	int object_cur, int object_top)
{
	int i;

	/* Display lines until done */
	for (i = 0; i < per_page && object_idx[i].type != OBJ_NONE; i++)
	{
		char buf[80];

		/* Get the object index */
		int oidx = object_top + i;
		object_list_entry *obj = &object_idx[oidx];
		object_kind *k_ptr;
		ego_item_type *e_ptr;
		byte attr, cursor;

		switch (obj->type)
		{
			case OBJ_NORMAL:
				/* Access the object */
				k_ptr = &k_info[obj->idx];

				/* Choose a color */
				attr = ((k_ptr->aware) ? TERM_WHITE : TERM_SLATE);
				cursor = ((k_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
				attr = ((oidx == object_cur) ? cursor : attr);

				/* Acquire the basic "name" of the object*/
				strip_name(buf, obj->idx);

				/* Display the name */
				c_prt(attr, buf, row + i, col);

				if (cheat_know) c_prt(attr, format ("%d", obj->idx), row + i, 70);

				if (k_ptr->aware)
				{

					/* Obtain attr/char */
					byte a = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_attr): k_ptr->d_attr;
					byte c = k_ptr->flavor ? (flavor_info[k_ptr->flavor].x_char): k_ptr->d_char;

					/* Display symbol */
					Term_putch(76, row + i, a, c);
				}

				break;

			case OBJ_SPECIAL:
				e_ptr = &e_info[obj->e_idx];

				/* Choose a color */
				attr = ((e_ptr->aware) ? TERM_WHITE : TERM_SLATE);
				cursor = ((e_ptr->aware) ? TERM_L_BLUE : TERM_BLUE);
				attr = ((oidx == object_cur) ? cursor : attr);

				if (obj->sval == -1)
				{
					buf[0] = '\0';
                    snprintf(buf, sizeof(buf), "  %s", &e_name[e_ptr->name]);
				}
				else
				{
					int j;
                    char buf2[80];

					/* Find the specific type */
					buf[0] = '\0';
					buf2[0] = '\0';
					for (j = 0; j < z_info->k_max; ++j)
					{
						if ((k_info[j].tval == obj->tval) && (k_info[j].sval == obj->sval))
                        {
							strip_name(buf2, j);
							break;
						}
					}
                    
                    snprintf(buf, sizeof(buf), "%s %s", buf2, &e_name[e_ptr->name]);
				}

				c_prt(attr, buf, row + i, col);
				
				break;

			case OBJ_NONE:
			default:
				break;
		}
	}

	/* Clear remaining lines */
	for (; i < per_page; i++)
	{
		Term_erase(col, row + i, 255);
	}
}


/*
 * Display known objects
 */
void do_cmd_knowledge_objects(void)
{
	int i, len, max;
	int grp_cur, grp_top, grp_max;
	int object_old, object_cur, object_top;
	int grp_cnt, grp_idx[100];
	int object_cnt;
	object_list_entry *object_idx;

	int column = 0;
	bool flag;
	bool redraw;

	max = 0;
	grp_max = 0;
	grp_cnt = 0;

	/* Check every group */
	for (i = 0; object_group_text[i] != NULL; i++)
	{
		/* Measure the label */
		len = strlen(object_group_text[i]);

		/* Save the maximum length */
		if (len > max) max = len;

		/* See if any monsters are known */
		object_cnt = collect_objects(i, NULL);
		if (object_cnt)
		{
			/* Build a list of groups with known monsters */
			grp_idx[grp_cnt++] = i;
		}

		if (object_cnt > grp_max) grp_max = object_cnt;
	}

	/* Terminate the list */
	grp_idx[grp_cnt] = -1;

	/* Allocate the "object_idx" array */
	C_MAKE(object_idx, 1 + grp_max, object_list_entry);

	grp_cur = grp_top = 0;
	object_cur = object_top = 0;
	object_old = -1;

	flag = FALSE;
	redraw = TRUE;

	while (!flag)
	{
		char ch;

		if (redraw)
		{
			clear_from(0);

			prt("Knowledge - Objects", 2, 0);
			prt("Group", 4, 0);
			prt("Name", 4, max + 3);
			if (cheat_know) prt("Idx", 4, 70);
			prt("Sym", 4, 75);

			for (i = 0; i < 78; i++)
			{
				Term_putch(i, 5, TERM_L_DARK, '=');
			}

			for (i = 0; i < BROWSER_ROWS; i++)
			{
				Term_putch(max + 1, 6 + i, TERM_L_DARK, '|');
			}

			redraw = FALSE;
		}

		/* Scroll group list */
		if (grp_cur < grp_top) grp_top = grp_cur;
		if (grp_cur >= grp_top + BROWSER_ROWS) grp_top = grp_cur - BROWSER_ROWS + 1;

		/* Scroll monster list */
		if (object_cur < object_top) object_top = object_cur;
		if (object_cur >= object_top + BROWSER_ROWS) object_top = object_cur - BROWSER_ROWS + 1;

		/* Display a list of object groups */
		display_group_list(0, 6, max, BROWSER_ROWS, grp_idx, object_group_text, grp_cur, grp_top);

		/* Get a list of objects in the current group */
		object_cnt = collect_objects(grp_idx[grp_cur], object_idx);

		/* Display a list of objects in the current group */
		display_object_list(max + 3, 6, BROWSER_ROWS, object_idx, object_cur, object_top);

		/* Prompt */
		Term_putstr(1, 23, -1, TERM_SLATE, "<dir>   recall   ESC");
		Term_putstr(1, 23, -1, TERM_L_WHITE, "<dir>");
		Term_putstr(9, 23, -1, TERM_L_WHITE, "r");
		Term_putstr(18, 23, -1, TERM_L_WHITE, "ESC");

		/* Mega Hack -- track this monster race */
		if (object_cnt) object_kind_track(object_idx[object_cur].idx);

		/* The "current" object changed */
		if (object_old != object_cur)
		{
			/* Hack -- handle stuff */
			handle_stuff();

			/* Remember the "current" object */
			object_old = object_cur;
		}

		if (!column)
		{
			Term_gotoxy(0, 6 + (grp_cur - grp_top));
		}
		else
		{
			Term_gotoxy(max + 3, 6 + (object_cur - object_top));
		}

		ch = inkey();

		switch (ch)
		{
			case ESCAPE:
			{
				flag = TRUE;
				break;
			}

			case 'R':
			case 'r':
			{
				object_list_entry *obj = &object_idx[object_cur];
				if (obj->type == OBJ_NORMAL &&
						k_info[obj->idx].aware)
				{
					/* Recall on screen */
					desc_obj_fake(obj->idx);
					
					redraw = TRUE;
				}
				break;
			}

			default:
			{
				/* Move the cursor */
				browser_cursor(ch, &column, &grp_cur, grp_cnt, &object_cur, object_cnt);
				break;
			}
		}
	}

	/* XXX XXX Free the "object_idx" array */
	KILL(object_idx);
}


/*
 * Display kill counts
 */
void do_cmd_knowledge_kills(void)
{
	int n, i;

	FILE *fff;

	char file_name[1024];

	u16b *who;
//	u16b why = 4;


	/* Temporary file */
	fff = my_fopen_temp(file_name, sizeof(file_name));

	/* Failure */
	if (!fff) return;


	/* Allocate the "who" array */
	C_MAKE(who, z_info->r_max, u16b);

	/* Collect matching monsters */
	for (n = 0, i = 1; i < z_info->r_max - 1; i++)
	{
		//monster_race *r_ptr = &r_info[i];
		monster_lore *l_ptr = &l_list[i];

		/* Require non-unique monsters */
		//if (r_ptr->flags1 & RF1_UNIQUE) continue;

		/* Collect "appropriate" monsters */
		if (l_ptr->pkills > 0) who[n++] = i;
	}

	/* Select the sort method */
	//ang_sort_comp = ang_sort_comp_hook;
	//ang_sort_swap = ang_sort_swap_hook;

	/* Sort by kills (and level) */
	//ang_sort(who, &why, n);

	/* Print the monsters (highest kill counts first) */
	for (i = n - 1; i >= 0; i--)
	{
		monster_race *r_ptr = &r_info[who[i]];
		monster_lore *l_ptr = &l_list[who[i]];

		if (r_ptr->flags1 & (RF1_UNIQUE))
		{
			/* Print a message */
			fprintf(fff, "         %-40s\n",
					(r_name + r_ptr->name));
		}
		else
		{
			/* Print a message */
			fprintf(fff, "  %5d  %-40s\n",
					l_ptr->pkills, (r_name + r_ptr->name));
		}
	}

	/* Free the "who" array */
	FREE(who);

	/* Close the file */
	my_fclose(fff);

	/* Display the file contents */
	show_file(file_name, "Kill counts", 0);

	/* Remove the file */
	fd_kill(file_name);
}




/*
 * Interact with "knowledge"
 */
void do_cmd_knowledge(void)
{
	char ch;

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Save screen */
	screen_save();

	/* Interact until done */
	while (1)
	{
		/* Clear screen */
		Term_clear();

		/* Ask for a choice */
		prt("Display current knowledge", 2, 0);

		/* Give some choices */
		prt("(1) Display known artefacts", 4, 5);
		prt("(2) Display known monsters", 5, 5);
		prt("(3) Display known objects", 6, 5);
		prt("(4) Display names of the fallen", 7, 5);
		prt("(5) Display kill counts", 8, 5);

		/*allow the player to see the notes taken if that option is selected*/
		c_put_str(TERM_WHITE,
					"(6) Display character notes file", 9, 5);

		/* Prompt */
		prt("Command: ", 12, 0);

		/* Prompt */
		ch = inkey();

		/* Done */
		if (ch == ESCAPE) break;

		/* Artefacts */
		if (ch == '1')
		{
			do_cmd_knowledge_artefacts();
		}

		/* Uniques */
		else if (ch == '2')
		{
			do_cmd_knowledge_monsters();
		}

		/* Objects */
		else if (ch == '3')
		{
			do_cmd_knowledge_objects();
		}

		/* Scores */
		else if (ch == '4')
		{
			show_scores();
		}

		/* Scores */
		else if (ch == '5')
		{
			do_cmd_knowledge_kills();
		}

		/* Notes file, if one exists */
		else if (ch == '6')
		{
			/* Spawn */
			do_cmd_knowledge_notes();
		}

		/* Unknown option */
		else
		{
			bell("Illegal command for knowledge!");
		}

		/* Flush messages */
		message_flush();
	}


	/* Load screen */
	screen_load();
}
