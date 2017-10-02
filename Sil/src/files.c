/* File: files.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

// These are copied from birth.c and needed for displaying the character sheet
#define INSTRUCT_ROW	21
#define QUESTION_COL	2

/*
 * Hack -- drop permissions
 */
void safe_setuid_drop(void)
{

#ifdef SET_UID

# ifdef SAFE_SETUID

#  ifdef HAVE_SETEGID

	if (setegid(getgid()) != 0)
	{
		quit("setegid(): cannot set permissions correctly!");
	}

#  else /* HAVE_SETEGID */

#   ifdef SAFE_SETUID_POSIX

	if (setgid(getgid()) != 0)
	{
		quit("setgid(): cannot set permissions correctly!");
	}

#   else /* SAFE_SETUID_POSIX */

	if (setregid(getegid(), getgid()) != 0)
	{
		quit("setregid(): cannot set permissions correctly!");
	}

#   endif /* SAFE_SETUID_POSIX */

#  endif /* HAVE_SETEGID */

# endif /* SAFE_SETUID */

#endif /* SET_UID */

}


/*
 * Hack -- grab permissions
 */
void safe_setuid_grab(void)
{

#ifdef SET_UID

# ifdef SAFE_SETUID

#  ifdef HAVE_SETEGID

	if (setegid(player_egid) != 0)
	{
		quit("setegid(): cannot set permissions correctly!");
	}

#  else /* HAVE_SETEGID */

#   ifdef SAFE_SETUID_POSIX

	if (setgid(player_egid) != 0)
	{
		quit("setgid(): cannot set permissions correctly!");
	}

#   else /* SAFE_SETUID_POSIX */

	if (setregid(getegid(), getgid()) != 0)
	{
		quit("setregid(): cannot set permissions correctly!");
	}

#   endif /* SAFE_SETUID_POSIX */

#  endif /* HAVE_SETEGID */

# endif /* SAFE_SETUID */

#endif /* SET_UID */

}


#if 0

/*
 * Use this (perhaps) for Angband 2.8.4
 *
 * Extract "tokens" from a buffer
 *
 * This function uses "whitespace" as delimiters, and treats any amount of
 * whitespace as a single delimiter.  We will never return any empty tokens.
 * When given an empty buffer, or a buffer containing only "whitespace", we
r * will return no tokens.  We will never extract more than "num" tokens.
 *
 * By running a token through the "text_to_ascii()" function, you can allow
 * that token to include (encoded) whitespace, using "\s" to encode spaces.
 *
 * We save pointers to the tokens in "tokens", and return the number found.
 */
static s16b tokenize_whitespace(char *buf, s16b num, char **tokens)
{
	int k = 0;

	char *s = buf;


	/* Process */
	while (k < num)
	{
		char *t;

		/* Skip leading whitespace */
		for ( ; *s && isspace((unsigned char)*s); ++s) /* loop */;

		/* All done */
		if (!*s) break;

		/* Find next whitespace, if any */
		for (t = s; *t && !isspace((unsigned char)*t); ++t) /* loop */;

		/* Nuke and advance (if necessary) */
		if (*t) *t++ = '\0';

		/* Save the token */
		tokens[k++] = s;

		/* Advance */
		s = t;
	}

	/* Count */
	return (k);
}

#endif


/*
 * Extract the first few "tokens" from a buffer
 *
 * This function uses "colon" and "slash" as the delimeter characters.
 *
 * We never extract more than "num" tokens.  The "last" token may include
 * "delimeter" characters, allowing the buffer to include a "string" token.
 *
 * We save pointers to the tokens in "tokens", and return the number found.
 *
 * Hack -- Attempt to handle the 'c' character formalism
 *
 * Hack -- An empty buffer, or a final delimeter, yields an "empty" token.
 *
 * Hack -- We will always extract at least one token
 */
s16b tokenize(char *buf, s16b num, char **tokens)
{
	int i = 0;

	char *s = buf;


	/* Process */
	while (i < num - 1)
	{
		char *t;

		/* Scan the string */
		for (t = s; *t; t++)
		{
			/* Found a delimiter */
			if ((*t == ':') || (*t == '/')) break;

			/* Handle single quotes */
			if (*t == '\'')
			{
				/* Advance */
				t++;

				/* Handle backslash */
				if (*t == '\\') t++;

				/* Require a character */
				if (!*t) break;

				/* Advance */
				t++;

				/* Hack -- Require a close quote */
				if (*t != '\'') *t = '\'';
			}

			/* Handle back-slash */
			if (*t == '\\') t++;
		}

		/* Nothing left */
		if (!*t) break;

		/* Nuke and advance */
		*t++ = '\0';

		/* Save the token */
		tokens[i++] = s;

		/* Advance */
		s = t;
	}

	/* Save the token */
	tokens[i++] = s;

	/* Number found */
	return (i);
}



/*
 * Parse a sub-file of the "extra info" (format shown below)
 *
 * Each "action" line has an "action symbol" in the first column,
 * followed by a colon, followed by some command specific info,
 * usually in the form of "tokens" separated by colons or slashes.
 *
 * Blank lines, lines starting with white space, and lines starting
 * with pound signs ("#") are ignored (as comments).
 *
 * Note the use of "tokenize()" to allow the use of both colons and
 * slashes as delimeters, while still allowing final tokens which
 * may contain any characters including "delimiters".
 *
 * Note the use of "strtol()" to allow all "integers" to be encoded
 * in decimal, hexidecimal, or octal form.
 *
 * Note that "monster zero" is used for the "player" attr/char, "object
 * zero" will be used for the "stack" attr/char, and "feature zero" is
 * used for the "nothing" attr/char.
 *
 * Specify the attr/char values for "monsters" by race index.
 *   R:<num>:<a>/<c>
 *
 * Specify the attr/char values for "objects" by kind index.
 *   K:<num>:<a>/<c>
 *
 * Specify the attr/char values for "features" by feature index.
 *   F:<num>:<a>/<c>
 *
 * Specify the attr/char values for "special" things.
 *   S:<num>:<a>/<c>
 *
 * Specify the attribute values for inventory "objects" by kind tval.
 *   E:<tv>:<a>
 *
 * Define a macro action, given an encoded macro action.
 *   A:<str>
 *
 * Create a macro, given an encoded macro trigger.
 *   P:<str>
 *
 * Create a keymap, given an encoded keymap trigger.
 *   C:<num>:<str>
 *
 * Turn an option off, given its name.
 *   X:<str>
 *
 * Turn an option on, given its name.
 *   Y:<str>
 *
 * Turn a window flag on or off, given a window, flag, and value.
 *   W:<win>:<flag>:<value>
 *
 * Specify visual information, given an index, and some data.
 *   V:<num>:<kv>:<rv>:<gv>:<bv>
 *
 * Specify colors for message-types.
 *   M:<type>:<attr>
 *
 * Specify the attr/char values for "flavors" by flavors index.
 *   L:<num>:<a>/<c>
 */
errr process_pref_file_command(char *buf)
{
	long i, n1, n2, sq;

	char *zz[16];


	/* Skip "empty" lines */
	if (!buf[0]) return (0);

	/* Skip "blank" lines */
	if (isspace((unsigned char)buf[0])) return (0);

	/* Skip comments */
	if (buf[0] == '#') return (0);


	/* Paranoia */
	/* if (strlen(buf) >= 1024) return (1); */


	/* Require "?:*" format */
	if (buf[1] != ':') return (1);


	/* Process "R:<num>:<a>/<c>" -- attr/char for monster races */
	if (buf[0] == 'R')
	{
		if (tokenize(buf+2, 3, zz) == 3)
		{
			monster_race *r_ptr;
			i = strtol(zz[0], NULL, 0);
			n1 = strtol(zz[1], NULL, 0);
			n2 = strtol(zz[2], NULL, 0);
			if ((i < 0) || (i >= (long)z_info->r_max)) return (1);
			r_ptr = &r_info[i];
			if (n1) r_ptr->x_attr = (byte)n1;
			if (n2) r_ptr->x_char = (char)n2;
			return (0);
		}
	}

	/* Process "B:<k_idx>:inscription */
	else if (buf[0] == 'B')
	{
		if(2 == tokenize(buf + 2, 2, zz))
		{
			add_autoinscription(strtol(zz[0], NULL, 0), zz[1]);
			return (0);
		}
	}

	/* Process "Q:<idx>:<tval>:<sval>:<y|n>"  -- squelch bits   */
	/* and     "Q:<idx>:<val>"                -- squelch levels */
	/* and     "Q:<val>"                      -- auto_destroy   */
	else if (buf[0] == 'Q')
	{
		i = tokenize(buf+2, 4, zz);
		if (i == 2)
		{
			n1 = strtol(zz[0], NULL, 0);
			n2 = strtol(zz[1], NULL, 0);
			squelch_level[n1] = n2;
			return(0);
		}
		else if (i == 4)
		{
			i = strtol(zz[0], NULL, 0);
			n1 = strtol(zz[1], NULL, 0);
			n2 = strtol(zz[2], NULL, 0);
			sq = strtol(zz[3], NULL, 0);
			if ((k_info[i].tval == n1) && (k_info[i].sval == n2))
			{
				k_info[i].squelch = sq;
				return(0);
			}
			else
			{
				for (i = 1; i < z_info->k_max; i++)
				{
					if ((k_info[i].tval == n1) && (k_info[i].sval == n2))
					{
						k_info[i].squelch = sq;
						return(0);
					}
				}
			}
		}
	}


	/* Process "K:<num>:<a>/<c>"  -- attr/char for object kinds */
	else if (buf[0] == 'K')
	{
		if (tokenize(buf+2, 3, zz) == 3)
		{
			object_kind *k_ptr;
			i = strtol(zz[0], NULL, 0);
			n1 = strtol(zz[1], NULL, 0);
			n2 = strtol(zz[2], NULL, 0);
			if ((i < 0) || (i >= (long)z_info->k_max)) return (1);
			k_ptr = &k_info[i];
			if (n1) k_ptr->x_attr = (byte)n1;
			if (n2) k_ptr->x_char = (char)n2;
			return (0);
		}
	}


	/* Process "F:<num>:<a>/<c>" -- attr/char for terrain features */
	else if (buf[0] == 'F')
	{
		if (tokenize(buf+2, 3, zz) == 3)
		{
			feature_type *f_ptr;
			i = strtol(zz[0], NULL, 0);
			n1 = strtol(zz[1], NULL, 0);
			n2 = strtol(zz[2], NULL, 0);
			if ((i < 0) || (i >= (long)z_info->f_max)) return (1);
			f_ptr = &f_info[i];
			if (n1) f_ptr->x_attr = (byte)n1;
			if (n2) f_ptr->x_char = (char)n2;
			return (0);
		}
	}


	/* Process "L:<num>:<a>/<c>" -- attr/char for flavors */
	else if (buf[0] == 'L')
	{
		if (tokenize(buf+2, 3, zz) == 3)
		{
			flavor_type *flavor_ptr;
			i = strtol(zz[0], NULL, 0);
			n1 = strtol(zz[1], NULL, 0);
			n2 = strtol(zz[2], NULL, 0);
			if ((i < 0) || (i >= (long)z_info->flavor_max)) return (1);
			flavor_ptr = &flavor_info[i];
			if (n1) flavor_ptr->x_attr = (byte)n1;
			if (n2) flavor_ptr->x_char = (char)n2;
			return (0);
		}
	}


	/* Process "S:<num>:<a>/<c>" -- attr/char for special things */
	else if (buf[0] == 'S')
	{
		if (tokenize(buf+2, 3, zz) == 3)
		{
			i = strtol(zz[0], NULL, 0);
			n1 = strtol(zz[1], NULL, 0);
			n2 = strtol(zz[2], NULL, 0);
			if ((i < 0) || (i >= (long)N_ELEMENTS(misc_to_attr))) return (1);
			misc_to_attr[i] = (byte)n1;
			misc_to_char[i] = (char)n2;
			return (0);
		}
	}


	/* Process "E:<tv>:<a>" -- attribute for inventory objects */
	else if (buf[0] == 'E')
	{
		if (tokenize(buf+2, 2, zz) == 2)
		{
			i = strtol(zz[0], NULL, 0) % 128;
			n1 = strtol(zz[1], NULL, 0);
			if ((i < 0) || (i >= (long)N_ELEMENTS(tval_to_attr))) return (1);
			if (n1) tval_to_attr[i] = (byte)n1;
			return (0);
		}
	}


	/* Process "A:<str>" -- save an "action" for later */
	else if (buf[0] == 'A')
	{
		text_to_ascii(macro_buffer, sizeof(macro_buffer), buf+2);
		return (0);
	}

	/* Process "P:<str>" -- create macro */
	else if (buf[0] == 'P')
	{
		char tmp[1024];
		text_to_ascii(tmp, sizeof(tmp), buf+2);
		macro_add(tmp, macro_buffer);
		return (0);
	}

	/* Process "C:<num>:<str>" -- create keymap */
	else if (buf[0] == 'C')
	{
		long mode;

		char tmp[1024];

		if (tokenize(buf+2, 2, zz) != 2) return (1);

		mode = strtol(zz[0], NULL, 0);
		if ((mode < 0) || (mode >= KEYMAP_MODES)) return (1);

		text_to_ascii(tmp, sizeof(tmp), zz[1]);
		if (!tmp[0] || tmp[1]) return (1);
		i = (long)tmp[0];

		string_free(keymap_act[mode][i]);

		keymap_act[mode][i] = string_make(macro_buffer);

		return (0);
	}


	/* Process "V:<num>:<kv>:<rv>:<gv>:<bv>" -- visual info */
	else if (buf[0] == 'V')
	{
		if (tokenize(buf+2, 5, zz) == 5)
		{
			i = strtol(zz[0], NULL, 0);
			if ((i < 0) || (i >= 256)) return (1);
			angband_color_table[i][0] = (byte)strtol(zz[1], NULL, 0);
			angband_color_table[i][1] = (byte)strtol(zz[2], NULL, 0);
			angband_color_table[i][2] = (byte)strtol(zz[3], NULL, 0);
			angband_color_table[i][3] = (byte)strtol(zz[4], NULL, 0);
			return (0);
		}
	}

	/* set macro trigger names and a template */
	/* Process "T:<trigger>:<keycode>:<shift-keycode>" */
	/* Process "T:<template>:<modifier chr>:<modifier name>:..." */
	else if (buf[0] == 'T')
	{
		int tok;

		tok = tokenize(buf + 2, MAX_MACRO_MOD + 2, zz);

		/* Trigger template */
		if (tok >= 4)
		{
			int i;
			int num;

			/* Free existing macro triggers and trigger template */
			macro_trigger_free();

			/* Clear template done */
			if (*zz[0] == '\0') return 0;

			/* Count modifier-characters */
			num = strlen(zz[1]);

			/* One modifier-character per modifier */
			if (num + 2 != tok) return 1;

			/* Macro template */
			macro_template = string_make(zz[0]);

			/* Modifier chars */
			macro_modifier_chr = string_make(zz[1]);

			/* Modifier names */
			for (i = 0; i < num; i++)
			{
				macro_modifier_name[i] = string_make(zz[2+i]);
			}
		}
		/* Macro trigger */
		else if (tok >= 2)
		{
			char *buf;
			cptr s;
			char *t;

			if (max_macrotrigger >= MAX_MACRO_TRIGGER)
			{
				msg_print("Too many macro triggers!");
				return 1;
			}

			/* Buffer for the trigger name */
			C_MAKE(buf, strlen(zz[0]) + 1, char);

			/* Simulate strcpy() and skip the '\' escape character */
			s = zz[0];
			t = buf;

			while (*s)
			{
				if ('\\' == *s) s++;
				*t++ = *s++;
			}

			/* Terminate the trigger name */
			*t = '\0';

			/* Store the trigger name */
			macro_trigger_name[max_macrotrigger] = string_make(buf);

			/* Free the buffer */
			FREE(buf);

			/* Normal keycode */
			macro_trigger_keycode[0][max_macrotrigger] = string_make(zz[1]);

			/* Special shifted keycode */
			if (tok == 3)
			{
				macro_trigger_keycode[1][max_macrotrigger] = string_make(zz[2]);
			}
			/* Shifted keycode is the same as the normal keycode */
			else
			{
				macro_trigger_keycode[1][max_macrotrigger] = string_make(zz[1]);
			}

			/* Count triggers */
			max_macrotrigger++;
		}

		return 0;
	}


	/* Process "X:<str>" -- turn option off */
	else if (buf[0] == 'X')
	{
		/* Check non-adult options */
		for (i = 0; i < OPT_ADULT; i++)
		{
			if (option_text[i] && streq(option_text[i], buf + 2))
			{
				op_ptr->opt[i] = FALSE;
				return (0);
			}
		}

		/* Ignore unknown options */
		return (0);

	}

	/* Process "Y:<str>" -- turn option on */
	else if (buf[0] == 'Y')
	{
		/* Check non-adult options */
		for (i = 0; i < OPT_ADULT; i++)
		{
			if (option_text[i] && streq(option_text[i], buf + 2))
			{
				op_ptr->opt[i] = TRUE;
				return (0);
			}
		}

		/* Ignore unknown options */
		return (0);

	}


	/* Process "W:<win>:<flag>:<value>" -- window flags */
	else if (buf[0] == 'W')
	{
		long win, flag, value;

		if (tokenize(buf + 2, 3, zz) == 3)
		{
			win = strtol(zz[0], NULL, 0);
			flag = strtol(zz[1], NULL, 0);
			value = strtol(zz[2], NULL, 0);

			/* Ignore illegal windows */
			/* Hack -- Ignore the main window */
			if ((win <= 0) || (win >= ANGBAND_TERM_MAX)) return (1);

			/* Ignore illegal flags */
			if ((flag < 0) || (flag >= 32)) return (1);

			/* Require a real flag */
			if (window_flag_desc[flag])
			{
				if (value)
				{
					/* Turn flag on */
					op_ptr->window_flag[win] |= (1L << flag);
				}
				else
				{
					/* Turn flag off */
					op_ptr->window_flag[win] &= ~(1L << flag);
				}
			}

			/* Success */
			return (0);
		}
	}


	/* Process "M:<type>:<attr>" -- colors for message-types */
	else if (buf[0] == 'M')
	{
		if (tokenize(buf+2, 2, zz) == 2)
		{
			long type = strtol(zz[0], NULL, 0);
			int color = color_char_to_attr(zz[1][0]);

			/* Ignore illegal color */
			if (color < 0) return (1);

			/* Store the color */
			return (message_color_define((u16b)type, (byte)color));
		}
	}


	/* Failure */
	return (1);
}


/*
 * Helper function for "process_pref_file()"
 *
 * Input:
 *   v: output buffer array
 *   f: final character
 *
 * Output:
 *   result
 */
static cptr process_pref_file_expr(char **sp, char *fp)
{
	cptr v;

	char *b;
	char *s;

	char b1 = '[';
	char b2 = ']';

	char f = ' ';

	/* Initial */
	s = (*sp);

	/* Skip spaces */
	while (isspace((unsigned char)*s)) s++;

	/* Save start */
	b = s;

	/* Default */
	v = "?o?o?";

	/* Analyze */
	if (*s == b1)
	{
		const char *p;
		const char *t;

		/* Skip b1 */
		s++;

		/* First */
		t = process_pref_file_expr(&s, &f);

		/* Oops */
		if (!*t)
		{
			/* Nothing */
		}

		/* Function: IOR */
		else if (streq(t, "IOR"))
		{
			v = "0";
			while (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
				if (*t && !streq(t, "0")) v = "1";
			}
		}

		/* Function: AND */
		else if (streq(t, "AND"))
		{
			v = "1";
			while (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
				if (*t && streq(t, "0")) v = "0";
			}
		}

		/* Function: NOT */
		else if (streq(t, "NOT"))
		{
			v = "1";
			while (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
				if (*t && !streq(t, "0")) v = "0";
			}
		}

		/* Function: EQU */
		else if (streq(t, "EQU"))
		{
			v = "1";
			if (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
			}
			while (*s && (f != b2))
			{
				p = t;
				t = process_pref_file_expr(&s, &f);
				if (*t && !streq(p, t)) v = "0";
			}
		}

		/* Function: LEQ */
		else if (streq(t, "LEQ"))
		{
			v = "1";
			if (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
			}
			while (*s && (f != b2))
			{
				p = t;
				t = process_pref_file_expr(&s, &f);
				if (*t && (strcmp(p, t) >= 0)) v = "0";
			}
		}

		/* Function: GEQ */
		else if (streq(t, "GEQ"))
		{
			v = "1";
			if (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
			}
			while (*s && (f != b2))
			{
				p = t;
				t = process_pref_file_expr(&s, &f);
				if (*t && (strcmp(p, t) <= 0)) v = "0";
			}
		}

		/* Oops */
		else
		{
			while (*s && (f != b2))
			{
				t = process_pref_file_expr(&s, &f);
			}
		}

		/* Verify ending */
		if (f != b2) v = "?x?x?";

		/* Extract final and Terminate */
		if ((f = *s) != '\0') *s++ = '\0';
	}

	/* Other */
	else
	{
		/* Accept all printables except spaces and brackets */
		while (isprint((unsigned char)*s) && !strchr(" []", *s)) ++s;

		/* Extract final and Terminate */
		if ((f = *s) != '\0') *s++ = '\0';

		/* Variable */
		if (*b == '$')
		{
			/* System */
			if (streq(b+1, "SYS"))
			{
				v = ANGBAND_SYS;
			}

			/* Graphics */
			else if (streq(b+1, "GRAF"))
			{
				v = ANGBAND_GRAF;
			}

			/* Race */
			else if (streq(b+1, "RACE"))
			{
				v = p_name + rp_ptr->name;
			}

			/* Player */
			else if (streq(b+1, "nameless"))
			{
				v = op_ptr->base_name;
			}

			/* Game version */
			else if (streq(b+1, "VERSION"))
			{
				v = VERSION_STRING;
			}
		}

		/* Constant */
		else
		{
			v = b;
		}
	}

	/* Save */
	(*fp) = f;

	/* Save */
	(*sp) = s;

	/* Result */
	return (v);
}


/*
 * Open the "user pref file" and parse it.
 */
static errr process_pref_file_aux(cptr name)
{
	FILE *fp;

	char buf[1024];

	char old[1024];

	int line = -1;

	errr err = 0;

	bool bypass = FALSE;


	/* Open the file */
	fp = my_fopen(name, "r");

	/* No such file */
	if (!fp) return (-1);


	/* Process the file */
	while (0 == my_fgets(fp, buf, sizeof(buf)))
	{
		/* Count lines */
		line++;


		/* Skip "empty" lines */
		if (!buf[0]) continue;

		/* Skip "blank" lines */
		if (isspace((unsigned char)buf[0])) continue;

		/* Skip comments */
		if (buf[0] == '#') continue;


		/* Save a copy */
		my_strcpy(old, buf, sizeof(old));


		/* Process "?:<expr>" */
		if ((buf[0] == '?') && (buf[1] == ':'))
		{
			char f;
			cptr v;
			char *s;

			/* Start */
			s = buf + 2;

			/* Parse the expr */
			v = process_pref_file_expr(&s, &f);

			/* Set flag */
			bypass = (streq(v, "0") ? TRUE : FALSE);

			/* Continue */
			continue;
		}

		/* Apply conditionals */
		if (bypass) continue;


		/* Process "%:<file>" */
		if (buf[0] == '%')
		{
			/* Process that file if allowed */
			(void)process_pref_file(buf + 2);

			/* Continue */
			continue;
		}


		/* Process the line */
		err = process_pref_file_command(buf);

		/* Oops */
		if (err) break;
	}


	/* Error */
	if (err)
	{
		/* Print error message */
		/* ToDo: Add better error messages */
		msg_format("Error %d in line %d of file '%s'.", err, line, name);
		msg_format("Parsing '%s'", old);
		message_flush();
	}

	/* Close the file */
	my_fclose(fp);

	/* Result */
	return (err);
}



/*
 * Process the "user pref file" with the given name
 *
 * See the functions above for a list of legal "commands".
 *
 * We also accept the special "?" and "%" directives, which
 * allow conditional evaluation and filename inclusion.
 */
errr process_pref_file(cptr name)
{
	char buf[1024];

	errr err = 0;


	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_PREF, name);

	/* Process the pref file */
	err = process_pref_file_aux(buf);

	/* Stop at parser errors, but not at non-existing file */
	if (err < 1)
	{
		/* Build the filename */
		path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);

		/* Process the pref file */
		err = process_pref_file_aux(buf);
	}

	/* Result */
	return (err);
}




#ifdef CHECK_TIME

/*
 * Operating hours for ANGBAND (defaults to non-work hours)
 */
static char days[7][29] =
{
	"SUN:XXXXXXXXXXXXXXXXXXXXXXXX",
	"MON:XXXXXXXX.........XXXXXXX",
	"TUE:XXXXXXXX.........XXXXXXX",
	"WED:XXXXXXXX.........XXXXXXX",
	"THU:XXXXXXXX.........XXXXXXX",
	"FRI:XXXXXXXX.........XXXXXXX",
	"SAT:XXXXXXXXXXXXXXXXXXXXXXXX"
};

/*
 * Restict usage (defaults to no restrictions)
 */
static bool check_time_flag = FALSE;

#endif /* CHECK_TIME */


/*
 * Handle CHECK_TIME
 */
errr check_time(void)
{

#ifdef CHECK_TIME

	time_t c;
	struct tm *tp;

	/* No restrictions */
	if (!check_time_flag) return (0);

	/* Check for time violation */
	c = time((time_t *)0);
	tp = localtime(&c);

	/* Violation */
	if (days[tp->tm_wday][tp->tm_hour + 4] != 'X') return (1);

#endif /* CHECK_TIME */

	/* Success */
	return (0);
}



/*
 * Initialize CHECK_TIME
 */
errr check_time_init(void)
{

#ifdef CHECK_TIME

	FILE *fp;

	char buf[1024];


	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_FILE, "time.txt");

	/* Open the file */
	fp = my_fopen(buf, "r");

	/* No file, no restrictions */
	if (!fp) return (0);

	/* Assume restrictions */
	check_time_flag = TRUE;

	/* Parse the file */
	while (0 == my_fgets(fp, buf, sizeof(buf)))
	{
		/* Skip comments and blank lines */
		if (!buf[0] || (buf[0] == '#')) continue;

		/* Chop the buffer */
		buf[sizeof(days[0]) - 1] = '\0';

		/* Extract the info */
		if (prefix(buf, "SUN:")) my_strcpy(days[0], buf, sizeof(days[0]));
		if (prefix(buf, "MON:")) my_strcpy(days[1], buf, sizeof(days[1]));
		if (prefix(buf, "TUE:")) my_strcpy(days[2], buf, sizeof(days[2]));
		if (prefix(buf, "WED:")) my_strcpy(days[3], buf, sizeof(days[3]));
		if (prefix(buf, "THU:")) my_strcpy(days[4], buf, sizeof(days[4]));
		if (prefix(buf, "FRI:")) my_strcpy(days[5], buf, sizeof(days[5]));
		if (prefix(buf, "SAT:")) my_strcpy(days[6], buf, sizeof(days[6]));
	}

	/* Close it */
	my_fclose(fp);

#endif /* CHECK_TIME */

	/* Success */
	return (0);
}

static void display_skill(int skill, int row, int col)
{
	put_str(skill_names_full[skill], row, col);
	c_put_str(TERM_L_GREEN, format("%3d", p_ptr->skill_use[skill]), row, col+11);
	c_put_str(TERM_SLATE, "=", row, col+15);
	c_put_str(TERM_GREEN, format("%2d", p_ptr->skill_base[skill]), row, col+17);
	if (p_ptr->skill_stat_mod[skill] != 0)
		c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_stat_mod[skill]), row, col+20);
	if (p_ptr->skill_equip_mod[skill] != 0)
		c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_equip_mod[skill]), row, col+24);
	if (p_ptr->skill_misc_mod[skill] != 0)
		c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_misc_mod[skill]), row, col+28);
}

/*
 * Prints some "extra" information on the screen.
 *
 * mode 0 is normal
 * mode 1 highlights age, height, weight
 * mode 2 highlights history
 *
 * Space includes rows 3-9 cols 24-79
 * Space includes rows 10-17 cols 1-79
 * Space includes rows 19-22 cols 1-79
 */
void display_player_xtra_info(int mode)
{
	int col;
	
	int skill;
	
	int attacks = 1, shots = 1, mod = 2;

	object_type *o_ptr;

	char buf[160];

	byte ahw_attr = (mode == 1) ? TERM_YELLOW : TERM_L_BLUE; 
	byte history_attr = (mode == 2) ? TERM_YELLOW : TERM_WHITE; 

	/* Upper middle */
	col = 22;

	/* Age */
	Term_putstr(col, 2, -1, TERM_WHITE, "Age");
	if (p_ptr->age > 0)
	{
		comma_number(buf, (int)p_ptr->age);
		Term_putstr(col+7, 2, -1, ahw_attr, format("%5s", buf));
	}
	/* Height */
	/* put it in the format of feet and inches */
	Term_putstr(col, 3, -1, TERM_WHITE, "Height");
	if (p_ptr->ht > 0)
	{
		if ((int)(p_ptr->ht)%12 > 9)
		{
			Term_putstr(col+8, 3, -1, ahw_attr, format("%d'", (int)(p_ptr->ht)/12));
			Term_putstr(col+10, 3, -1, ahw_attr, format("%d", (int)(p_ptr->ht)%12));
		}
		else if ((int)(p_ptr->ht)%12 > 0)
		{
			Term_putstr(col+9, 3, -1, ahw_attr, format("%d'", (int)(p_ptr->ht)/12));
			Term_putstr(col+11, 3, -1, ahw_attr, format("%d", (int)(p_ptr->ht)%12));
		}
		else
		{
			Term_putstr(col+10, 3, -1, ahw_attr, format("%d'", (int)(p_ptr->ht)/12));
		}
	}
	
	/* Weight */
	Term_putstr(col, 4, -1, TERM_WHITE, "Weight");
	if (p_ptr->wt > 0)
	{
		Term_putstr(col+8, 4, -1, ahw_attr, format("%4d", (int)p_ptr->wt));
	}

	/* Left */
	col = 1;

	/* Game Turn */
	Term_putstr(col, 7, -1, TERM_WHITE, "Game Turn");
	comma_number(buf, playerturn);
	Term_putstr(col+10, 7, -1, TERM_L_GREEN, format("%8s", buf));

	/* Current Experience */
	Term_putstr(col, 8, -1, TERM_WHITE, "Exp Pool");
	comma_number(buf, p_ptr->new_exp);
	Term_putstr(col+10, 8, -1, TERM_L_GREEN, format("%8s", buf));

	/* Maximum Experience */
	Term_putstr(col, 9, -1, TERM_WHITE, "Total Exp");
	comma_number(buf, p_ptr->exp);
	Term_putstr(col+10, 9, -1, TERM_L_GREEN, format("%8s", buf));

	/* Burden (in pounds) */
	Term_putstr(col, 10, -1, TERM_WHITE, "Burden");
	strnfmt(buf, sizeof(buf), "%3d.%1d",
	        p_ptr->total_weight / 10L,
	        p_ptr->total_weight % 10L);
	if (p_ptr->total_weight <= weight_limit())
		Term_putstr(col+13, 10, -1, TERM_L_GREEN, buf);
	else
		Term_putstr(col+13, 10, -1, TERM_YELLOW, buf);
		
	/* Max Burden (in pounds) */
	Term_putstr(col, 11, -1, TERM_WHITE, "Max Burden");
	strnfmt(buf, sizeof(buf), "%3d.%1d",
	        weight_limit() / 10L,
	        weight_limit() % 10L);
	Term_putstr(col+13, 11, -1, TERM_L_GREEN, buf);

	if (turn > 0)
	{
		/* Current Depth */
		Term_putstr(col, 12, -1, TERM_WHITE, "Depth");
		strnfmt(buf, sizeof(buf), "%3d'", p_ptr->depth * 50);
		if (p_ptr->depth >= min_depth())
			Term_putstr(col+14, 12, -1, TERM_L_GREEN, buf);
		else
			Term_putstr(col+14, 12, -1, TERM_YELLOW, buf);
		
		/* Min Depth */
		Term_putstr(col, 13, -1, TERM_WHITE, "Min Depth");
		strnfmt(buf, sizeof(buf), "%3d'", min_depth() * 50);
		Term_putstr(col+14, 13, -1, TERM_L_GREEN, buf);
	}
	
	/* Light Radius */
	Term_putstr(col, 14, -1, TERM_WHITE, "Light Radius");
	strnfmt(buf, sizeof(buf), "%3d", p_ptr->cur_light);
	Term_putstr(col+15, 14, -1, TERM_L_GREEN, buf);
	
	/* Middle */
	col = 22;
	
	
	/* Melee attacks */
	strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
	Term_putstr(col, 7, -1, TERM_WHITE, "Melee");
	Term_putstr(col+5, 7, -1, TERM_L_BLUE, format("%11s", buf));
	if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
	{ 
		attacks++;
		Term_putstr(col+5, 8, -1, TERM_L_BLUE, format("%11s", buf));
	}
	if (p_ptr->mds2 > 0)
	{	
		attacks++;
		strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2, p_ptr->mds2);
		Term_putstr(col+5, 6+attacks, -1, TERM_L_BLUE, format("%11s", buf));
	}

	/* Range weapon */
	o_ptr = &inventory[INVEN_BOW];
	
	/* Range attacks */
	strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
	Term_putstr(col, 7+attacks, -1, TERM_WHITE, "Bows");
	Term_putstr(col+5, 7+attacks, -1, TERM_L_BLUE, format("%11s", buf));

	if (p_ptr->active_ability[S_ARC][ARC_RAPID_FIRE])
	{
		shots++;
		Term_putstr(col+5, 8+attacks, -1, TERM_L_BLUE, format("%11s", buf));
	}
	
	/* Total Armor */
	strnfmt(buf, sizeof(buf), " [%+d,%d-%d]", p_ptr->skill_use[S_EVN], p_min(GF_HURT, TRUE), p_max(GF_HURT, TRUE));
	Term_putstr(col, 7+attacks+shots, -1, TERM_WHITE, "Armor");
	Term_putstr(col+5, 7+attacks+shots, -1, TERM_L_BLUE, format("%11s", buf));

	// limit the amount we will move the fields around to 4 lines
	mod = MIN(attacks + shots, 4);
	
	/* Health */
	strnfmt(buf, sizeof(buf), "%d:%d", p_ptr->chp, p_ptr->mhp);
	Term_putstr(col, 9+mod, -1, TERM_WHITE, "Health");
	Term_putstr(col+8, 9+mod, -1, TERM_L_BLUE, format("%8s", buf));

	/* Voice */
	strnfmt(buf, sizeof(buf), "%d:%d", p_ptr->csp, p_ptr->msp);
	Term_putstr(col, 10+mod, -1, TERM_WHITE, "Voice");
	Term_putstr(col+8, 10+mod, -1, TERM_L_BLUE, format("%8s", buf));

	/* Song */
	if (p_ptr->song1 != SNG_NOTHING)
	{
		strnfmt(buf, sizeof(buf), "%s", b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
		Term_putstr(col, 11+mod, -1, TERM_WHITE, "Song");
		Term_putstr(col+5, 11+mod, -1, TERM_L_BLUE, format("%11s", buf + 8));
	}
	if (p_ptr->song2 != SNG_NOTHING)
	{
		strnfmt(buf, sizeof(buf), "%s", b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
		Term_putstr(col+5, 12+mod, -1, TERM_L_BLUE, format("%11s", buf + 8));
	}
		
	/* Right */
	col = 41;

	/* Skills */

	for (skill = 0; skill < S_MAX; skill++)
	{
		display_skill(skill, 7 + skill, col);
	}

	/* Indent output by 1 character, and wrap at column 72 */
	text_out_wrap = 72;
	text_out_indent = 1;

	/* History */
	Term_gotoxy(text_out_indent, 16);
	text_out_to_screen(history_attr, p_ptr->history);

	/* Reset text_out() vars */
	text_out_wrap = 0;
	text_out_indent = 0;
	
	/* Prompt */
	if (!character_dungeon)
	{
		Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"ESC restart the character");
		Term_putstr(QUESTION_COL + 38 + 4, INSTRUCT_ROW + 2, -1, TERM_SLATE,
					"q quit");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "ESC");
		Term_putstr(QUESTION_COL + 38 + 4, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "q");
	}
}


/*
 * Equippy chars
 */
static void display_player_equippy(int y, int x)
{
	int i;

	byte a;
	char c;

	object_type *o_ptr;


	/* Dump equippy chars */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
	{
		/* Object */
		o_ptr = &inventory[i];

		/* Skip empty objects */
		if (!o_ptr->k_idx) continue;

		/* Get attr/char for display */
		a = object_attr(o_ptr);
		c = object_char(o_ptr);

		/* Dump */
		Term_putch(x+i-INVEN_WIELD, y, a, c);
	}
}


/*
 * Hack -- see below
 */
static const byte display_player_flag_set[4] =
{
	1,
	2,
	2,
	1
};

/*
 * Hack -- see below
 */
static const u32b display_player_flag_head[4] =
{
	TR1_MEL,
	TR2_RES_COLD,
	TR2_SLOW_DIGEST,
	TR1_SLAY_ORC
};

/*
 * Hack -- see below
 */
static cptr display_player_flag_names[4][9] =
{
	{
		"  Mel:",
		"  Arc:",
		"  Stl:",
		"  Per:",
		"  Wil:",
		"  Smt:",
		"  Sng:",
		"#####:",
		"#####:"
	},

	{
		" Cold:",
		" Fire:",
		" Elec:",
		" Pois:",
		" Dark:",
		" Fear:",
		"Blind:",
		" Conf:",
		" Stun:",
	},

	{
		"Sustn:",	/* TR2_SLOW_DIGEST */
		"Light:",
		"Regen:",
		"Invis:",
		" Free:",
		"#####:",
		"Speed:",
		"#####:",
		"#####:"
	},

	{
		"  Orc:",
		"Troll:",
		" Wolf:",
		"Spidr:",
		" Undd:",
		"Rauko:",
		"Dragn:",
		"#####:",
		"#####:"
	}
};


/*
 * Special display, part 1
 */
static void display_player_flag_info(void)
{
	int x, y, i, n;

	int row, col;

	int set;
	u32b head;
	u32b flag;
	cptr name;

	u32b f[4];


	/* Four columns */
	for (x = 0; x < 4; x++)
	{

		/* Reset */
		row = 10;
		col = 20 * x - 2;

		/* Header */
		c_put_str(TERM_WHITE, "abcdefghijkl@", row++, col+8);

		/* Nine rows */
		for (y = 0; y < 9; y++)
		{
			byte name_attr = TERM_WHITE;

			/* Extract set */
			set = display_player_flag_set[x];

			/* Extract head */
			head = display_player_flag_head[x];

			/* Extract flag */
			flag = (head << y);

			/* Extract name */
			name = display_player_flag_names[x][y];

			/* Check equipment */
			for (n = 8, i = INVEN_WIELD; i < INVEN_TOTAL; ++i, ++n)
			{
				byte attr = TERM_SLATE;

				object_type *o_ptr;

				/* Object */
				o_ptr = &inventory[i];

				/* Known flags */
				object_flags_known(o_ptr, &f[1], &f[2], &f[3]);

				/* Color columns by parity */
				if (i % 2) attr = TERM_L_WHITE;

				/* Non-existant objects */
				if (!o_ptr->k_idx) attr = TERM_L_DARK;

				/* Check flags */
				if (f[set] & flag)
				{
					c_put_str(TERM_L_BLUE, "+", row, col+n);
					if (name_attr != TERM_L_GREEN) name_attr = TERM_L_BLUE;
				}

				/* Default */
				else
				{
					c_put_str(attr, ".", row, col+n);
				}
			}

			/* Default */
			c_put_str(TERM_SLATE, ".", row, col+n);

			/* Check flags */
			if (f[set] & flag)
			{
			    c_put_str(TERM_L_BLUE, "+", row, col+n);
				if (name_attr != TERM_L_GREEN) name_attr = TERM_L_BLUE;
			}

			/* Header */
			c_put_str(name_attr, name, row, col+2);

			/* Advance */
			row++;
		}

		/* Footer */
		c_put_str(TERM_WHITE, "abcdefghijkl@", row++, col+8);

		/* Equippy */
		display_player_equippy(row++, col+8);
	}
}


/*
 * Special display, part 2a
 */
static void display_player_misc_info(void)
{

	/* Name */
	put_str("Name", 2, 1);
	c_put_str(TERM_L_BLUE, op_ptr->full_name, 2, 8);

	/* Sex */
	put_str("Sex", 3, 1);
	c_put_str(TERM_L_BLUE, sp_ptr->title, 3, 8);

	/* Race */
	put_str("Race", 4, 1);
	c_put_str(TERM_L_BLUE, p_name + rp_ptr->name, 4, 8);

	if (p_ptr->phouse)
	{
		/* Title */
		put_str("House", 5, 1);
		c_put_str(TERM_L_BLUE, c_name + hp_ptr->short_name, 5, 8);
	}

}


/*
 * Special display, part 2b
 */
void display_player_stat_info(int row, int col)
{
	int i;

	char buf[80];

	/* Display the stats */
	for (i = 0; i < A_MAX; i++)
	{
		/* Reduced */
		if (p_ptr->stat_drain[i] < 0)
		{
			/* Use lowercase stat name */
			put_str(stat_names_reduced[i], row+i, col);
		}

		/* Normal */
		else
		{
			/* Assume uppercase stat name */
			put_str(stat_names[i], row+i, col);
		}

		/* Resulting "modified" maximum value */
		cnv_stat(p_ptr->stat_use[i], buf);
		
		if (p_ptr->stat_drain[i] < 0)
			c_put_str(TERM_YELLOW, buf, row+i, col+5);
		else
			c_put_str(TERM_L_GREEN, buf, row+i, col+5);

		/* Only display stat_equip_mod if not zero */
		if (p_ptr->stat_equip_mod[i] != 0)
		{
			c_put_str(TERM_SLATE, "=", row+i, col+8);

			/* Internal "natural" maximum value */
			cnv_stat(p_ptr->stat_base[i], buf);
			c_put_str(TERM_GREEN, buf, row+i, col+10);
		
			/* Equipment Bonus */
			strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_equip_mod[i]);
			c_put_str(TERM_SLATE, buf, row+i, col+13);	
		}

		/* Only display stat_drain if not zero */
		if (p_ptr->stat_drain[i] != 0)
		{
			c_put_str(TERM_SLATE, "=", row+i, col+8);

			/* Internal "natural" maximum value */
			cnv_stat(p_ptr->stat_base[i], buf);
			c_put_str(TERM_GREEN, buf, row+i, col+10);
			
			/* Reduction */
			strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_drain[i]);
			c_put_str(TERM_SLATE, buf, row+i, col+17);	
		}

		/* Only display stat_misc_mod if not zero */
		if (p_ptr->stat_misc_mod[i] != 0)
		{
			c_put_str(TERM_SLATE, "=", row+i, col+8);

			/* Internal "natural" maximum value */
			cnv_stat(p_ptr->stat_base[i], buf);
			c_put_str(TERM_GREEN, buf, row+i, col+10);
			
			/* Modifier */
			strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_misc_mod[i]);
			c_put_str(TERM_SLATE, buf, row+i, col+21);	
		}
	}
}


/*
 * Special display, part 2c
 *
 * How to print out the modifications and sustains.
 * Positive mods with no sustain will be light green.
 * Positive mods with a sustain will be dark green.
 * Sustains (with no modification) will be a dark green 's'.
 * Negative mods (from a curse) will be red.
 * Huge mods (>9), like from MICoMorgoth, will be a '*'
 * No mod, no sustain, will be a slate '.'
 */
static void display_player_sust_info(void)
{
	int i, row, col, stats;

	object_type *o_ptr;
	u32b f1, f2, f3;
	u32b ignore_f2, ignore_f3;

	byte a;
	char c;

	/* Row */
	row = 2;

	/* Column */
	col = 23;

	/* Header */
	c_put_str(TERM_WHITE, "abcdefghijkl@", row-1, col);

	/* Process equipment */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
	{
		/* Get the object */
		o_ptr = &inventory[i];

		/* Get the "known" flags */
		object_flags_known(o_ptr, &f1, &f2, &f3);

		/* Hack -- assume stat modifiers are known */
		object_flags(o_ptr, &f1, &ignore_f2, &ignore_f3);

		/* Initialize color based of sign of pval. */
		for (stats = 0; stats < A_MAX; stats++)
		{
			/* Default */
			a = TERM_SLATE;
			c = '.';

			/* Boost */
			if (f1 & (1<<stats))
			{
				/* Default */
				c = '*';

				/* Neutral */
				if (o_ptr->pval == 0)
				{
					/* Neutral */
					c = '.';
				}

				/* Good */
				if (o_ptr->pval > 0)
				{
					/* Good */
					a = TERM_L_GREEN;

					/* Label boost */
					if (o_ptr->pval < 10) c = I2D(o_ptr->pval);
				}

				/* Bad */
				if (o_ptr->pval < 0)
				{
					/* Bad */
					a = TERM_RED;

					/* Label boost */
					if (o_ptr->pval > -10) c = I2D(-(o_ptr->pval));
				}
			}

			/* Reverse Boost */
			if (f1 & (1<<(stats+A_MAX)))
			{
				/* Default */
				c = '*';

				/* Neutral */
				if (o_ptr->pval == 0)
				{
					/* Neutral */
					c = '.';
				}
				
				/* Good */
				if (o_ptr->pval < 0)
				{
					/* Good */
					a = TERM_L_GREEN;

					/* Label boost */
					if (o_ptr->pval > -10) c = I2D(-(o_ptr->pval));
				}

				/* Bad */
				if (o_ptr->pval > 0)
				{
					/* Bad */
					a = TERM_RED;

					/* Label boost */
					if (o_ptr->pval < 10) c = I2D(o_ptr->pval);
				}
			}

			/* Sustain */
			if (f2 & (1<<stats))
			{
				/* Dark green */
				if (a == TERM_RED)	a = TERM_ORANGE;
				else				a = TERM_GREEN;


				/* Convert '.' to 's' */
				if (c == '.') c = 's';
			}

			/* Dump proper character */
			Term_putch(col, row+stats, a, c);
		}

		/* Advance */
		col++;
	}

	/* Check stats */
	for (stats = 0; stats < A_MAX; ++stats)
	{
		/* Default */
		a = TERM_SLATE;
		c = '.';

		/* Sustain */
		if (f2 & (1<<stats))
		{
			/* Dark green "s" */
			a = TERM_GREEN;
			c = 's';
		}

		/* Dump */
		Term_putch(col, row+stats, a, c);
	}

	/* Column */
	col = 23;

	/* Footer */
	c_put_str(TERM_WHITE, "abcdefghijkl@", row+4, col);

	/* Equippy */
	display_player_equippy(row+5, col);
}


/*
 * Display the character on the screen (four different modes)
 *
 * The top two lines, and the bottom line (or two) are left blank
 * in the first two modes.
 *
 * Mode 0 = standard display with skills/history
 * Mode 1 = special display with equipment flags
 */
void display_player(int mode)
{
	/* Erase screen */
	clear_from(0);

	/* All Modes Use Stat info */
	display_player_stat_info(2, 41);

	if ((mode) < 2)
	{
		/* Misc info */
		display_player_misc_info();

		/* Special */
		if (mode)
		{
			/* Stat/Sustain flags */
			display_player_sust_info();

			/* Other flags */
			display_player_flag_info();
		}

		/* Standard */
		else
		{
			/* Extra info */
			display_player_xtra_info(0);
			
		}
	}
}


/*
 * Make a string lower case.
 */
static void string_lower(char *buf)
{
	char *s;

	/* Lowercase the string */
	for (s = buf; *s != 0; s++) *s = tolower((unsigned char)*s);
}


/*
 * Show the contents of a char buffer on the screen and allow scrolling.
 * Based on show_file.
 */
bool show_buffer(cptr main_buffer, cptr what, int line)
{
	int i, j, k;
	
	char ch;
	
	int next = 0;
	int size = 0;
	
	char buf[1024];
	
	int wid, hgt;
	
	// hack to soothe compiler warnings since 'what' is unused
	if (what) {}
	
	/* Get size */
	Term_get_size(&wid, &hgt);
	
	/* Count the lines in the buffer */
	for (j = 0; TRUE; j++)
	{
		if (main_buffer[j] == '\n')	next++;
		if (main_buffer[j] == '\0')	break;
	}
	
	// store the number of lines
	size = next;
	
	/* Display the file */
	while (TRUE)
	{
		/* Clear screen */
		Term_clear();
		
		/* Restrict the visible range */
		if (line > (size - (hgt - 5))) line = size - (hgt - 5);
		if (line < 0) line = 0;

		/* Goto the selected line */
		next = 0;
		for (j = 0; TRUE; j++)
		{
			if (main_buffer[j] == '\n') next++;
			
			if ((next == line) || (main_buffer[j] == '\0')) break;
		}
		
		// hack: need to step forward a character when not starting with the first line
		if (main_buffer[j] == '\n')	j++;

		/* Dump the next lines of the file */
		for (i = 0; i < hgt - 5; )
		{
			/* Get a line of the file or stop */
			k = 0;
			while (TRUE)
			{
				ch = main_buffer[j];
				
				if (ch == '\0')
				{
					break;
				}
				
				if (ch == '\n')
				{
					j++;
					break;
				}

				buf[k] = ch;
				
				k++;
				j++;			
			}
			buf[k] = '\0';
			
			/* Dump the line */
			Term_putstr(0, i+2, -1, TERM_WHITE, buf);
						
			/* Count the printed lines */
			i++;
		}
		
		/* Prompt -- small files */
		if (size <= hgt - 5)
		{
			/* Wait for it */
			Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit)");
			Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
			Term_putstr(20, hgt - 2, -1, TERM_L_WHITE, "");
		}
		
		/* Prompt -- large files */
		else
		{
			/* Wait for it */
			Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit, Space for next page, Arrows/Keypad to scroll)");
			Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
			Term_putstr(21, hgt - 2, -1, TERM_L_WHITE, "Space");
			Term_putstr(42, hgt - 2, -1, TERM_L_WHITE, "Arrows");
			Term_putstr(49, hgt - 2, -1, TERM_L_WHITE, "Keypad");
			Term_putstr(67, hgt - 2, -1, TERM_L_WHITE, "");
		}
		
		/* Get a keypress */
		ch = inkey();
		
		/* Back up one line */
		if ((ch == '8') || (ch == '='))
		{
			line = line - 1;
			if (line < 0) line = 0;
		}
		
		/* Advance one line */
		if ((ch == '2') || (ch == '\n') || (ch == '\r'))
		{
			line = line + 1;
		}
		
		/* Advance one full page */
		if ((ch == '3') || (ch == ' '))
		{
			line = line + (hgt - 5);
		}
		
		/* Exit on escape */
		if (ch == ESCAPE) break;
	}
	
	/* Done */
	return (TRUE);
}


/*
 * Recursive file perusal.
 *
 * Return FALSE on "?", otherwise TRUE.
 *
 * Process various special text in the input file, including the "menu"
 * structures used by the "help file" system.
 *
 * This function could be made much more efficient with the use of "seek"
 * functionality, especially when moving backwards through a file, or
 * forwards through a file by less than a page at a time.  XXX XXX XXX
 *
 * Consider using a temporary file, in which special lines do not appear,
 * and which could be pre-padded to 80 characters per line, to allow the
 * use of perfect seeking.  XXX XXX XXX
 *
 * Allow the user to "save" the current file.  XXX XXX XXX
 */
bool show_file(cptr name, cptr what, int line)
{
	int i, k, n;

	char ch;

	/* Number of "real" lines passed by */
	int next = 0;

	/* Number of "real" lines in the file */
	int size;

	/* Backup value for "line" */
	int back = 0;

	/* This screen has sub-screens */
	bool menu = FALSE;

	/* Case sensitive search */
	bool case_sensitive = FALSE;

	/* Current help file */
	FILE *fff = NULL;

	/* Find this string (if any) */
	char *find = NULL;

	/* Jump to this tag */
	cptr tag = NULL;

	/* Hold a string to find */
	char finder[80];

	/* Hold a string to show */
	char shower[80];

	/* Filename */
	char filename[1024];

	/* Describe this thing */
	char caption[128];

	/* Path buffer */
	char path[1024];

	/* General buffer */
	char buf[1024];

	/* Lower case version of the buffer, for searching */
	char lc_buf[1024];

	/* Sub-menu information */
	char hook[26][32];

	int wid, hgt;

	/* Wipe finder */
	my_strcpy(finder, "", sizeof (finder));

	/* Wipe shower */
	my_strcpy(shower, "", sizeof (shower));

	/* Wipe caption */
	my_strcpy(caption, "", sizeof (caption));

	/* Wipe the hooks */
	for (i = 0; i < 26; i++) hook[i][0] = '\0';

	/* Get size */
	Term_get_size(&wid, &hgt);

	/* Copy the filename */
	my_strcpy(filename, name, sizeof(filename));

	n = strlen(filename);

	/* Extract the tag from the filename */
	for (i = 0; i < n; i++)
	{
		if (filename[i] == '#')
		{
			filename[i] = '\0';
			tag = filename + i + 1;
			break;
		}
	}

	/* Redirect the name */
	name = filename;


	/* Hack XXX XXX XXX */
	if (what)
	{
		/* Caption */
		my_strcpy(caption, what, sizeof(caption));

		/* Get the filename */
		my_strcpy(path, name, sizeof(path));

		/* Open */
		fff = my_fopen(path, "r");
	}

	/* Oops */
	if (!fff)
	{
		/* Message */
		msg_format("Cannot open '%s'.", name);
		message_flush();

		/* Oops */
		return (TRUE);
	}


	/* Pre-Parse the file */
	while (TRUE)
	{
		/* Read a line or stop */
		if (my_fgets(fff, buf, sizeof(buf))) break;

		/* XXX Parse "menu" items */
		if (prefix(buf, "***** "))
		{
			char b1 = '[', b2 = ']';

			/* Notice "menu" requests */
			if ((buf[6] == b1) && isalpha((unsigned char)buf[7]) &&
			    (buf[8] == b2) && (buf[9] == ' '))
			{
				/* This is a menu file */
				menu = TRUE;

				/* Extract the menu item */
				k = A2I(buf[7]);

				/* Store the menu item (if valid) */
				if ((k >= 0) && (k < 26))
					my_strcpy(hook[k], buf + 10, sizeof(hook[0]));
			}
			/* Notice "tag" requests */
			else if (buf[6] == '<')
			{
				if (tag)
				{
					/* Remove the closing '>' of the tag */
					buf[strlen(buf) - 1] = '\0';

					/* Compare with the requested tag */
					if (streq(buf + 7, tag))
					{
						/* Remember the tagged line */
						line = next;
					}
				}
			}

			/* Skip this */
			continue;
		}

		/* Count the "real" lines */
		next++;
	}

	/* Save the number of "real" lines */
	size = next;



	/* Display the file */
	while (TRUE)
	{
		/* Clear screen */
		Term_clear();

		/* Restrict the visible range */
		if (line > (size - (hgt - 5))) line = size - (hgt - 5);
		if (line < 0) line = 0;

		/* Re-open the file if needed */
		if (next > line)
		{
			/* Close it */
			my_fclose(fff);

			/* Hack -- Re-Open the file */
			fff = my_fopen(path, "r");

			/* Oops */
			if (!fff) return (TRUE);

			/* File has been restarted */
			next = 0;
		}


		/* Goto the selected line */
		while (next < line)
		{
			/* Get a line */
			if (my_fgets(fff, buf, sizeof(buf))) break;

			/* Skip tags/links */
			if (prefix(buf, "***** ")) continue;

			/* Count the lines */
			next++;
		}


		/* Dump the next lines of the file */
		for (i = 0; i < hgt - 5; )
		{
			/* Hack -- track the "first" line */
			if (!i) line = next;

			/* Get a line of the file or stop */
			if (my_fgets(fff, buf, sizeof(buf))) break;

			/* Hack -- skip "special" lines */
			if (prefix(buf, "***** ")) continue;

			/* Count the "real" lines */
			next++;

			/* Make a copy of the current line for searching */
			my_strcpy(lc_buf, buf, sizeof(lc_buf));

			/* Make the line lower case */
			if (!case_sensitive) string_lower(lc_buf);

			/* Hack -- keep searching */
			if (find && !i && !strstr(lc_buf, find)) continue;

			/* Hack -- stop searching */
			find = NULL;

			/* Dump the line */
			Term_putstr(0, i+2, -1, TERM_WHITE, buf);

			/* Hilite "shower" */
			if (shower[0])
			{
				cptr str = lc_buf;

				/* Display matches */
				while ((str = strstr(str, shower)) != NULL)
				{
					int len = strlen(shower);

					/* Display the match */
					Term_putstr(str-lc_buf, i+2, len, TERM_YELLOW, &buf[str-lc_buf]);

					/* Advance */
					str += len;
				}
			}

			/* Count the printed lines */
			i++;
		}

		/* Hack -- failed search */
		if (find)
		{
			bell("Search string not found!");
			line = back;
			find = NULL;
			continue;
		}


		/* Show a general "title" */
//		prt(format("[%s %s, %s, Line %d-%d/%d]", VERSION_NAME, VERSION_STRING,
//	           caption, line, line + hgt - 4, size), 0, 0);


		/* Prompt -- menu screen */
		if (menu)
		{
			/* Wait for it */
			prt("[Press a Number, or ESC to exit.]", hgt - 1, 0);
		}

		/* Prompt -- small files */
		else if (size <= hgt - 5)
		{
			/* Wait for it */
			Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit)");
			Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
			Term_putstr(20, hgt - 2, -1, TERM_L_WHITE, "");
		}

		/* Prompt -- large files */
		else
		{
			/* Wait for it */
			Term_putstr(1, hgt - 2, -1, TERM_SLATE, "(press ESC to exit, Space for next page, Arrows/Keypad to scroll)");
			Term_putstr(8, hgt - 2, -1, TERM_L_WHITE, "ESC");
			Term_putstr(21, hgt - 2, -1, TERM_L_WHITE, "Space");
			Term_putstr(42, hgt - 2, -1, TERM_L_WHITE, "Arrows");
			Term_putstr(49, hgt - 2, -1, TERM_L_WHITE, "Keypad");
			Term_putstr(67, hgt - 2, -1, TERM_L_WHITE, "");
		}

		/* Get a keypress */
		ch = inkey();

		/* Exit the help */
		if (ch == '?') break;

		/* Toggle case sensitive on/off */
		if (ch == '!')
		{
			case_sensitive = !case_sensitive;
		}

		/* Try showing */
		if (ch == '&')
		{
			/* Get "shower" */
			prt("Show: ", hgt - 1, 0);
			(void)askfor_aux(shower, sizeof(shower));

			/* Make the "shower" lowercase */
			if (!case_sensitive) string_lower(shower);
		}

		/* Try finding */
		if (ch == '/')
		{
			/* Get "finder" */
			prt("Find: ", hgt - 1, 0);
			if (askfor_aux(finder, sizeof(finder)))
			{
				/* Find it */
				find = finder;
				back = line;
				line = line + 1;

				/* Make the "finder" lowercase */
				if (!case_sensitive) string_lower(finder);

				/* Show it */
				my_strcpy(shower, finder, sizeof(shower));
			}
		}

		/* Go to a specific line */
		if (ch == '#')
		{
			char tmp[80];
			prt("Goto Line: ", hgt - 1, 0);
			my_strcpy(tmp, "0", sizeof(tmp));
			if (askfor_aux(tmp, sizeof(tmp)))
			{
				line = atoi(tmp);
			}
		}

		/* Back up one line */
		if ((ch == '8') || (ch == '='))
		{
			line = line - 1;
			if (line < 0) line = 0;
		}

		/* Back up one half page */
		if (ch == '_')
		{
			line = line - ((hgt - 5) / 2);
		}

		/* Back up one full page */
		if ((ch == '9') || (ch == '-'))
		{
			line = line - (hgt - 5);
		}

		/* Back to the top */
		if (ch == '7')
		{
			line = 0;
 		}

		/* Advance one line */
		if ((ch == '2') || (ch == '\n') || (ch == '\r'))
		{
			line = line + 1;
		}

		/* Advance one half page */
		if (ch == '+')
		{
			line = line + ((hgt - 5) / 2);
		}

		/* Advance one full page */
		if ((ch == '3') || (ch == ' '))
		{
			line = line + (hgt - 5);
		}

		/* Advance to the bottom */
		if (ch == '1')
		{
			line = size;
		}

		/* Exit on escape */
		if (ch == ESCAPE) break;
	}

	/* Close the file */
	my_fclose(fff);

	/* Done */
	return (ch != '?');
}

void show_help_screen(int i)
{
	int row, col, col2;

	switch (i)
	{
		case 1:
		{
			row = 3;
			col = 3;
			col2 = col + 8;
			
			c_put_str(TERM_L_WHITE + TERM_SHADE, "Movement etc", row - 2, col - 1);
			
			c_put_str(TERM_WHITE,   "7 8 9",           row, col);
			c_put_str(TERM_SLATE,   " \\|/ ",          row + 1, col);
			c_put_str(TERM_WHITE,   "4 5 6",           row + 2, col);
			c_put_str(TERM_SLATE,   "-",               row + 2, col + 1);
			c_put_str(TERM_SLATE,   "-",               row + 2, col + 3);
			c_put_str(TERM_SLATE,   " /|\\ ",          row + 3, col);
			c_put_str(TERM_WHITE,   "1 2 3",           row + 4, col);
			
			c_put_str(TERM_SLATE, "Use the numbers or arrow keys",  row + 0, col2);
			c_put_str(TERM_WHITE, "numbers",                        row + 0, col2 + 8);
			c_put_str(TERM_WHITE, "arrow keys",                     row + 0, col2 + 19);
			c_put_str(TERM_SLATE, "to move, attack, or open doors",            row + 1, col2);
			c_put_str(TERM_SLATE, "(You may need numlock)",        row + 2, col2);
			c_put_str(TERM_WHITE, "numlock",                        row + 2, col2 + 14);
			c_put_str(TERM_SLATE, "Use 5 or z to wait a turn (& search)",     row + 4, col2);
			c_put_str(TERM_WHITE, "5",                              row + 4, col2 + 4);
			c_put_str(TERM_WHITE, "z",                              row + 4, col2 + 9);

			row += 6;

			c_put_str(TERM_SLATE, "Use shift or . to move continuously",            row, col);
			c_put_str(TERM_WHITE, "shift",   row, col + 4);
			c_put_str(TERM_WHITE, ".",   row, col + 13);
			row++;			
			c_put_str(TERM_SLATE, "- direction 5 or z rests until healed",                  row, col + 2);
			row++;
			row++;
			
			c_put_str(TERM_SLATE, "Use control or / to interact with a square:",            row, col);
			c_put_str(TERM_WHITE, "control",   row, col + 4);
			if (angband_keyset)	c_put_str(TERM_WHITE, "+",  row, col + 15);
			else				c_put_str(TERM_WHITE, "/",  row, col + 15);
			row++;
			
			c_put_str(TERM_SLATE, "- tunnels through rubble/walls",                  row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- closes open doors",                      row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- bashes closed doors",                    row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- disarms floor traps",                    row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- disarms/opens chests",              row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- attacks monsters without moving",        row, col + 2);
			row++;
			row++;
			c_put_str(TERM_SLATE, "Interacting with your own square also:",                  row, col);
			row++;
			c_put_str(TERM_SLATE, "- picks up an item",                      row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- uses a staircase/forge",                    row, col + 2);
			row++;
			c_put_str(TERM_SLATE, "- can be done by pressing ,",                    row, col + 2);
			c_put_str(TERM_WHITE, ",",                                       row, col + 28);
			row++;
			row++;

			
			row = 3;
			col = 52;
			
			c_put_str(TERM_L_WHITE + TERM_SHADE, "Miscellaneous", row - 2, col);
			
//			if (angband_keyset)	c_put_str(TERM_WHITE, " R",  row, col);
//			else				c_put_str(TERM_WHITE, " Z",  row, col);
//			c_put_str(TERM_SLATE, "rest (until recovered)",  row, col + 3);
//			row++;
//			if (angband_keyset)	c_put_str(TERM_WHITE, " R",  row, col);
//			else				c_put_str(TERM_WHITE, " ,",  row, col);
//			c_put_str(TERM_SLATE, "interact with own square",  row, col + 3);
//			row++;
			c_put_str(TERM_WHITE, "f F",                     row, col-1);
			c_put_str(TERM_SLATE, "/",                       row, col);
			c_put_str(TERM_SLATE, "fire from quiver 1/2",    row, col + 3);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, " a",  row, col);
			else				c_put_str(TERM_WHITE, " s",  row, col);
			c_put_str(TERM_SLATE, "sing",                    row, col + 3);
			row++;
			c_put_str(TERM_WHITE, " S",                      row, col);
			c_put_str(TERM_SLATE, "stealth mode",            row, col + 3);
			row++;
			c_put_str(TERM_WHITE, " n",                      row, col);
			c_put_str(TERM_SLATE, "repeat last command",     row, col + 3);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, " 0",  row, col);
			else				c_put_str(TERM_WHITE, " R",  row, col);
			c_put_str(TERM_SLATE, "repeat next command",     row, col + 3);
			row++;
			row++;
			c_put_str(TERM_WHITE, " l",                      row, col);
			c_put_str(TERM_SLATE, "look (at things)",        row, col + 3);
			row++;
			c_put_str(TERM_WHITE, " L",                      row, col);
			c_put_str(TERM_SLATE, "look (around dungeon)",   row, col + 3);
			row++;		
			c_put_str(TERM_WHITE, " M",                      row, col);
			c_put_str(TERM_SLATE, "display map of level",    row, col + 3);
			row++;		
			row++;		
			c_put_str(TERM_WHITE, " m",                      row, col);
			c_put_str(TERM_SLATE, "main menu",               row, col + 3);
			row++;
			c_put_str(TERM_WHITE, "Tab",                     row, col-1);
			c_put_str(TERM_SLATE, "display ability screen",  row, col + 3);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, " C",  row, col);
			else				c_put_str(TERM_WHITE, " @",  row, col);
			c_put_str(TERM_SLATE, "display character sheet", row, col + 3);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, " =",  row, col);
			else				c_put_str(TERM_WHITE, " O",  row, col);
			c_put_str(TERM_SLATE, "set options",             row, col + 3);
			row++;
			row++;
			c_put_str(TERM_WHITE, "^s",                     row, col);
			c_put_str(TERM_SLATE, "save",                   row, col + 3);
			row++;
			c_put_str(TERM_WHITE, "^x",                     row, col);
			c_put_str(TERM_SLATE, "save and quit",          row, col + 3);
			row++;
			c_put_str(TERM_WHITE, " Q",                     row, col);
			c_put_str(TERM_SLATE, "abort current game",     row, col + 3);
			row++;

			
			break;
		}
		case 2:
		{
			row = 3;
			col = 3;
			
			c_put_str(TERM_L_WHITE + TERM_SHADE, "Terrain ",           row - 2, col - 1);
			
			if (hybrid_walls)
			{
				c_put_str(TERM_L_WHITE + (MAX_COLORS * BG_DARK), "#",               row, col);
			}
			else if (solid_walls)
			{
				c_put_str(TERM_L_WHITE + (MAX_COLORS * BG_SAME), "#",               row, col);
			}
			else
			{
				c_put_str(TERM_L_WHITE, "#",               row, col);
			}
			c_put_str(TERM_WHITE,   "wall",             row, col + 2);
			row++;
			c_put_str(TERM_WHITE + (MAX_COLORS * BG_SAME),   "%",               row, col);
			c_put_str(TERM_WHITE,   "quartz vein",      row, col + 2);
			row++;
			c_put_str(TERM_SLATE,   ":",               row, col);
			c_put_str(TERM_WHITE,   "rubble",           row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, "+",               row, col);
			c_put_str(TERM_WHITE,   "closed door",      row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, "'",               row, col);
			c_put_str(TERM_WHITE,   "open door",        row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, ">",               row, col);
			c_put_str(TERM_WHITE,   "staircase down",   row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "<",               row, col);
			c_put_str(TERM_WHITE,   "staircase up",     row, col + 2);
			row++;
			c_put_str(TERM_SLATE,   "0",				row, col);
			c_put_str(TERM_WHITE,   "forge",			row, col + 2);
			row++;
			c_put_str(TERM_YELLOW,   "^",              row, col);
			c_put_str(TERM_WHITE,   "trap",             row, col + 2);
			row++;
			c_put_str(TERM_L_GREEN,  ";",              row, col);
			c_put_str(TERM_WHITE,   "warding glyph",    row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, ".",               row, col);
			c_put_str(TERM_WHITE,   "empty floor",      row, col + 2);
			row++;
			 
			 
			row = 3;
			col = 27;

			c_put_str(TERM_L_WHITE + TERM_SHADE, "Items", row - 2, col - 1);
									
			c_put_str(TERM_L_WHITE, "| ",               row, col);
			c_put_str(TERM_WHITE,   "blades",           row, col + 2);
			row++;
			c_put_str(TERM_SLATE,   "/ ",               row, col);
			c_put_str(TERM_WHITE,   "axes & polearms",  row, col + 2);
			row++;
			c_put_str(TERM_UMBER,   "\\ ",              row, col);
			c_put_str(TERM_WHITE,   "blunt weapons",    row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, "( ",               row, col);
			c_put_str(TERM_WHITE,   "soft armour",      row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "[ ",               row, col);
			c_put_str(TERM_WHITE,   "mail",             row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, ") ",               row, col);
			c_put_str(TERM_WHITE,   "shields",          row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "] ",               row, col);
			c_put_str(TERM_WHITE,   "misc armour",      row, col + 2);
			row++;
			c_put_str(TERM_RED,     "= ",               row, col);
			c_put_str(TERM_WHITE,   "rings",            row, col + 2);
			row++;
			c_put_str(TERM_ORANGE,  "\" ",              row, col);
			c_put_str(TERM_WHITE,   "amulets",          row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, "~ ",               row, col);
			c_put_str(TERM_WHITE,   "light sources",    row, col + 2);
			row++;
			c_put_str(TERM_UMBER,   "} ",               row, col);
			c_put_str(TERM_WHITE,   "bows",             row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, "- ",               row, col);
			c_put_str(TERM_WHITE,   "arrows",           row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, ", ",               row, col);
			c_put_str(TERM_WHITE,   "food",             row, col + 2);
			row++;
			c_put_str(TERM_L_BLUE,  "! ",               row, col);
			c_put_str(TERM_WHITE,   "potions",          row, col + 2);
			row++;
			c_put_str(TERM_UMBER,   "_ ",               row, col);
			c_put_str(TERM_WHITE,   "staves",           row, col + 2);
			row++;
			c_put_str(TERM_L_UMBER, "? ",               row, col);
			c_put_str(TERM_WHITE,   "instruments",      row, col + 2);
			row++;
			c_put_str(TERM_YELLOW,  "! ",               row, col);
			c_put_str(TERM_WHITE,   "flasks of oil",    row, col + 2);
			row++;
			
			
			row = 3;
			col = 52;
			
			c_put_str(TERM_L_WHITE + TERM_SHADE, "Item Commands", row - 2, col - 1);
			
			if (angband_keyset)	c_put_str(TERM_WHITE, "U",  row, col);
			else				c_put_str(TERM_WHITE, "u",  row, col);
			c_put_str(TERM_SLATE, "use",  row, col + 2);
			row++;
			c_put_str(TERM_WHITE, "d",                      row, col);
			c_put_str(TERM_SLATE, "drop",                   row, col + 2);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, "I",  row, col);
			else				c_put_str(TERM_WHITE, "x",  row, col);
			c_put_str(TERM_SLATE, "examine",                row, col + 2);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, "v",  row, col);
			else				c_put_str(TERM_WHITE, "t",  row, col);
			c_put_str(TERM_SLATE, "throw",                  row, col + 2);
			row++;
			if (angband_keyset)	c_put_str(TERM_WHITE, "^v",  row, col - 1);
			else				c_put_str(TERM_WHITE, "^t",  row, col - 1);
			c_put_str(TERM_SLATE, "throw (auto-target)", row, col + 2);
			row++;
			c_put_str(TERM_WHITE, "k",                      row, col);
			c_put_str(TERM_SLATE, "destroy",                row, col + 2);
			row++;
			c_put_str(TERM_WHITE, "{",                      row, col);
			c_put_str(TERM_SLATE, "inscribe",               row, col + 2);
			row++;
			
			
			break;
		}
		
			
		case 3:
		{
			row = 3;
			col = 3;
			
			c_put_str(TERM_L_DARK + TERM_SHADE, "Superfluous", row - 2, col - 1);
			
			c_put_str(TERM_L_WHITE, "i",                      row, col);
			c_put_str(TERM_L_DARK, "display inventory",       row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "e",                      row, col);
			c_put_str(TERM_L_DARK, "display equipped items",  row, col + 2);
			row++;
			row++;
			c_put_str(TERM_L_WHITE, "g",                      row, col);
			c_put_str(TERM_L_DARK, "get",                     row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "w",                      row, col);
			c_put_str(TERM_L_DARK, "wear/wield",              row, col + 2);
			row++;
			if (angband_keyset)	c_put_str(TERM_L_WHITE, "t",  row, col);
			else				c_put_str(TERM_L_WHITE, "r",  row, col);
			c_put_str(TERM_L_DARK, "remove",                  row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "E",                      row, col);
			c_put_str(TERM_L_DARK, "eat food",                row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "q",                      row, col);
			c_put_str(TERM_L_DARK, "quaff potion",            row, col + 2);
			row++;
			if (angband_keyset)	c_put_str(TERM_L_WHITE, "u",  row, col);
			else				c_put_str(TERM_L_WHITE, "a",  row, col);
			c_put_str(TERM_L_DARK, "activate staff",          row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "p",                      row, col);
			c_put_str(TERM_L_DARK, "play instrument",         row, col + 2);
			row++;
			row++;
			
			c_put_str(TERM_L_WHITE, "o",                      row, col);
			c_put_str(TERM_L_DARK, "open door/chest",      row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "c",                      row, col);
			c_put_str(TERM_L_DARK, "close door",             row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "b",                      row, col);
			c_put_str(TERM_L_DARK, "bash door",              row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "D",                      row, col);
			c_put_str(TERM_L_DARK, "disarm trap",            row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "T",                      row, col);
			c_put_str(TERM_L_DARK, "tunnel",                 row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, ">",                      row, col);
			c_put_str(TERM_L_DARK, "descend stairs",         row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "<",                      row, col);
			c_put_str(TERM_L_DARK, "ascend stairs",          row, col + 2);
			row++;
			c_put_str(TERM_L_WHITE, "0",                      row, col);
			c_put_str(TERM_L_DARK, "forge an item",   row, col + 2);
			row++;
			
			
			row = 3;
			col = 34;
			
			c_put_str(TERM_L_DARK + TERM_SHADE, "Advanced", row - 2, col);
			
			c_put_str(TERM_L_WHITE, " :",                      row, col);
			c_put_str(TERM_L_DARK, "write a note",            row, col + 3);
			row++;
			c_put_str(TERM_L_WHITE, " )",                      row, col);
			c_put_str(TERM_L_DARK, "save screen shot",        row, col + 3);
			row++;
			row++;
			if (angband_keyset)	c_put_str(TERM_L_WHITE, " @",  row, col);
			else				c_put_str(TERM_L_WHITE, " $",  row, col);
			c_put_str(TERM_L_DARK, "set macros",              row, col + 3);
			row++;
			c_put_str(TERM_L_WHITE, " &",                      row, col);
			c_put_str(TERM_L_DARK, "set colours",             row, col + 3);
			row++;
			row++;
			c_put_str(TERM_L_WHITE, "^p",                      row, col);
			c_put_str(TERM_L_DARK, "display prior messages",  row, col + 3);
			row++;
			c_put_str(TERM_L_WHITE, "^r",                      row, col);
			c_put_str(TERM_L_DARK, "redraw screen",           row, col + 3);
			row++;
			c_put_str(TERM_L_WHITE, "^e",                      row, col);
			c_put_str(TERM_L_DARK, "switch inven/equip display in windows",  row, col + 3);
			row++;
			c_put_str(TERM_L_WHITE, " V",                      row, col);
			c_put_str(TERM_L_DARK, "version information",     row, col + 3);
			row++;
			

			row = 16;
			col = 35;
			col2 = 43;
			
			c_put_str(TERM_L_DARK + TERM_SHADE, "hjkl movement", row - 2, col - 1);
			
			c_put_str(TERM_L_WHITE,   "y k u",           row, col);
			c_put_str(TERM_L_DARK,   " \\|/ ",          row + 1, col);
			c_put_str(TERM_L_WHITE,   "h z l",           row + 2, col);
			c_put_str(TERM_L_DARK,   "-",               row + 2, col + 1);
			c_put_str(TERM_L_DARK,   "-",               row + 2, col + 3);
			c_put_str(TERM_L_DARK,   " /|\\ ",          row + 3, col);
			c_put_str(TERM_L_WHITE,   "b j n",           row + 4, col);
			
			c_put_str(TERM_L_DARK, "If the hjkl movement option is on",  row, col2);
			c_put_str(TERM_L_DARK, "then these keys move you around",  row + 1, col2);
			
			c_put_str(TERM_L_DARK, "Use shift to 'run'",            row + 3, col2);
			c_put_str(TERM_L_WHITE, "shift",   row + 3, col2 + 4);
			c_put_str(TERM_L_DARK, "Use control for the underlying",            row + 4, col2);
			c_put_str(TERM_L_WHITE, "control",   row + 4, col2 + 4);
			c_put_str(TERM_L_DARK, "key-commands",            row + 5, col2);
			
			break;
		}
	
			
	}
}

/*
 * Peruse the On-Line-Help
 */
void do_cmd_help(void)
{
	int i = 1;
	char ch;

	/* Save screen */
	screen_save();

	/* Interact until done */
	while (1)
	{
		/* Clear screen */
		Term_clear();
		
		show_help_screen(i);
		
		/* Prompt */
		c_put_str(TERM_L_WHITE, "(press any key)", 23, 53);
		ch = inkey();
		
		/* Most keys take you a page forwards, 'up' or '-' or '8' take you back */
		if (ch != EOF)
		{ 
			if ((ch == '8') || (ch == '-'))
			{
				i--;
				if (i < 1)	i = 1;
			}
			else
			{
				i++;
			}
		}
		
		/* Done */
		if (i > 3) break;
		
		/* Flush messages */
		message_flush();
	}	

	/* Load screen */
	screen_load();
}



/*
 * Process the player name and extract a clean "base name".
 *
 * If "sf" is TRUE, then we initialize "savefile" based on player name.
 *
 * Some platforms (Windows, Macintosh, Amiga) leave the "savefile" empty
 * when a new character is created, and then when the character is done
 * being created, they call this function to choose a new savefile name.
 */
void process_player_name(bool sf)
{
	int i;


	/* Process the player name */
	for (i = 0; op_ptr->full_name[i]; i++)
	{
		char c = op_ptr->full_name[i];

		/* No control characters */
		if (iscntrl((unsigned char)c))
		{
			/* Illegal characters */
			quit_fmt("Illegal control char (0x%02X) in player name", c);
		}

		/* Convert all non-alphanumeric symbols */
		if (!isalpha((unsigned char)c) && !isdigit((unsigned char)c)) c = '_';

		/* Build "base_name" */
		op_ptr->base_name[i] = c;
	}

#if defined(WINDOWS) || defined(MSDOS)

	/* Max length */
	if (i > 8) i = 8;

#endif

	/* Terminate */
	op_ptr->base_name[i] = '\0';

	/* Require a "base" name */
	if (!op_ptr->base_name[0])
	{
		my_strcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));
	}


	/* Pick savefile name if needed */
	if (sf)
	{
		char temp[128];

#ifdef SAVEFILE_USE_UID
		/* Rename the savefile, using the player_uid and base_name */
		strnfmt(temp, sizeof(temp), "%d.%s", player_uid, op_ptr->base_name);
#else
		/* Rename the savefile, using the base name */
		strnfmt(temp, sizeof(temp), "%s", op_ptr->base_name);
#endif

#ifdef VM
		/* Hack -- support "flat directory" usage on VM/ESA */
		strnfmt(temp, sizeof(temp), "%s.sv", op_ptr->base_name);
#endif /* VM */

		/* Build the filename */
		path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, temp);
	}
}

/*
 * Gets a name for the character, reacting to name changes.
 */
bool get_name(void)
{
	char tmp[14];
	char old_name[14];
	bool name_selected = FALSE;

	// Clear the names
	tmp[0] = '\0';
	old_name[0] = '\0';

	/* Display the player */
	display_player(0);

	/* Prompt */
	Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
				"Enter accept name");
	Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
				"  Tab random name");
	
	/* Hack - highlight the key names */
	Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "Enter");
	Term_putstr(QUESTION_COL + 2, INSTRUCT_ROW + 2, - 1, TERM_L_WHITE, "Tab");

	/* Special Prompt? */
	if (character_dungeon)
	{
		Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
					"ESC abort name change                  ");
		
		/* Hack - highlight the key names */
		Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, - 1, TERM_L_WHITE, "ESC");
	}

	// use old name as a default
	my_strcpy(tmp, op_ptr->full_name, sizeof(tmp));

	// save a copy too
	my_strcpy(old_name, op_ptr->full_name, sizeof(old_name));

	/* Prompt for a new name */
	Term_gotoxy(8, 2);
	
	while (!name_selected)
	{
		if (askfor_name(tmp, sizeof(tmp)))
		{
			my_strcpy(op_ptr->full_name, tmp, sizeof(op_ptr->full_name));
			p_ptr->redraw |= (PR_MISC);
		}
		else
		{
			my_strcpy(op_ptr->full_name, old_name, sizeof(op_ptr->full_name));
			return (FALSE);
		}
		
		if (tmp[0] != '\0')	name_selected = TRUE;
		else				bell("You must choose a name.");
	}
	
	/* Process the player name */
	process_player_name(FALSE);

	return (TRUE);
}


/*
 * Hack -- escape from Angband
 */
void do_cmd_escape(void)
{
	time_t ct = time((time_t*)0);
	char long_day[40];
	char buf[120];

	/* set the escaped flag */
	p_ptr->escaped = TRUE;

	/* Flush input */
	flush();

	/* Commit suicide */
	p_ptr->is_dead = TRUE;

	/* Stop playing */
	p_ptr->playing = FALSE;

	/* Leaving */
	p_ptr->leaving = TRUE;

	
	/* Get time */
	(void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));
	
	/* Add note */
	my_strcat(notes_buffer, "\n", sizeof(notes_buffer));

	/*killed by */
	sprintf(buf, "You escaped the Iron Hells on %s.", long_day);
	
	/* Write message */
	do_cmd_note(buf,  p_ptr->depth);
	
	// make a note
	switch (silmarils_possessed())
	{
		case 0:		do_cmd_note("You returned empty handed.", p_ptr->depth); break;
		case 1:		do_cmd_note("You brought back a Silmaril from Morgoth's crown!", p_ptr->depth); break;
		case 2:		do_cmd_note("You brought back two Silmarils from Morgoth's crown!", p_ptr->depth); break;
		case 3:		do_cmd_note("You brought back all three Silmarils from Morgoth's crown!", p_ptr->depth); break;
		default:	do_cmd_note("You brought back so many Silmarils that people should be suspicious!", p_ptr->depth); break;
	}
	
	my_strcat(notes_buffer, "\n", sizeof(notes_buffer));

	
	/* Cause of death */
	my_strcpy(p_ptr->died_from, "ripe old age", sizeof(p_ptr->died_from));
}


/*
 * Hack -- commit suicide
 */
void do_cmd_suicide(void)
{
	char ch;
	
	/* Flush input */
	flush();

	/* Verify */
	if (!get_check("This will destroy the current character: are you sure? ")) return;

	/* Special Verification for suicide */
	prt("Please verify ABORTING by typing the '@' sign: ", 0, 0);
	flush();
	ch = inkey();
	prt("", 0, 0);
	if (ch != '@') return;

	/* Commit suicide */
	p_ptr->is_dead = TRUE;

	/* Stop playing */
	p_ptr->playing = FALSE;

	/* Leaving */
	p_ptr->leaving = TRUE;

	/* Cause of death */
	if (p_ptr->psex)
	{
		my_strcpy(p_ptr->died_from, "his own hand", sizeof(p_ptr->died_from));
	}
	else
	{
		my_strcpy(p_ptr->died_from, "her own hand", sizeof(p_ptr->died_from));
	}
}



/*
 * Save the game
 */
void do_cmd_save_game(void)
{
	/* Disturb the player */
	disturb(1, 0);

	// in final deployment versions, you cannot save in the tutorial
	if (DEPLOYMENT && p_ptr->game_type != 0)
	{
		if (!save_game_quietly)
		{
			/* Message */
			msg_print("You cannot save games during the tutorial.");
		}
		return;
	}

	/* Clear messages */
	message_flush();

	/* Handle stuff */
	handle_stuff();

	if (!save_game_quietly)
	{
		/* Message */
		prt("Saving game...", 0, 0);
	}
	
	/* Refresh */
	Term_fresh();

	/* The player is not dead */
	my_strcpy(p_ptr->died_from, "(saved)", sizeof(p_ptr->died_from));

	/* Forbid suspend */
	signals_ignore_tstp();

	/* Save the player */
	if (save_player())
	{
		if (!save_game_quietly)
		{
			prt("Saving game... done.", 0, 0);
		}
	}

	/* Save failed (oops) */
	else
	{
		prt("Saving game... failed!", 0, 0);
	}

	/* Allow suspend again */
	signals_handle_tstp();

	/* Refresh */
	Term_fresh();

	/* Note that the player is not dead */
	my_strcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
	
	/* Reset the quietly flag */
	save_game_quietly = FALSE;
}



/*
 * Hack - save the time of death
 */
static time_t death_time = (time_t)0;


/*
 * Display a "tomb-stone"
 */
static void print_tomb(high_score *the_score)
{
	if (p_ptr->escaped)
	{
		Term_putstr(15, 2, -1, TERM_L_BLUE, "You have escaped");
	}
	else
	{
		Term_putstr(15, 2, -1, TERM_L_BLUE, "You have been slain");
	}

	display_single_score(TERM_WHITE, 1, 0, 0, FALSE, the_score);

	prt_mini_screenshot(5, 12);
}



/*
 * Display some character info
 */
static void show_info(void)
{

	/* Display player */
	display_player(0);

	/* Prompt for inventory */
	Term_putstr(30, 22, -1, TERM_L_WHITE, "(press any key)");

	/* Allow abort at this point */
	if (inkey() == ESCAPE) return;

	/* Show equipment and inventory */

	/* Equipment -- if any */
	if (p_ptr->equip_cnt)
	{
		Term_clear();
		item_tester_full = TRUE;
		show_equip();
		prt("You are using:", 0, 0);
		Term_putstr(30, 16, -1, TERM_L_WHITE, "(press any key)");
		if (inkey() == ESCAPE) return;
		item_tester_full = FALSE;
	}

	/* Inventory -- if any */
	if (p_ptr->inven_cnt)
	{
		Term_clear();
		item_tester_full = TRUE;
		show_inven();
		prt("You are carrying:", 0, 0);
		Term_putstr(30, p_ptr->inven_cnt + 2, -1, TERM_L_WHITE, "(press any key)");
		if (inkey() == ESCAPE) return;
		item_tester_full = FALSE;
	}

	// Display notes
	do_cmd_knowledge_notes();

}


/*
 * Special version of 'do_cmd_examine'
 */
static void death_examine(void)
{
	int item;

	object_type *o_ptr;

	cptr q, s;


	/* Start out in "display" mode */
	p_ptr->command_see = TRUE;

	/* Get an item */
	q = "Examine which item? ";
	s = "You have nothing to examine.";

	while (TRUE)
	{
		if (!get_item(&item, q, s, (USE_INVEN | USE_EQUIP))) return;

		/* Get the item */
		o_ptr = &inventory[item];

		/* Describe */
		object_info_screen(o_ptr);
	}
}


/*
 * The "highscore" file descriptor, if available.
 */
static int highscore_fd = -1;

/*
 * Seek score 'i' in the highscore file
 */
static int highscore_seek(int i)
{
	/* Seek for the requested record */
	return (fd_seek(highscore_fd, i * sizeof(high_score)));
}


/*
 * Read one score from the highscore file
 */
static errr highscore_read(high_score *score)
{
	/* Read the record, note failure */
	return (fd_read(highscore_fd, (char*)(score), sizeof(high_score)));
}


/*
 * Write one score to the highscore file
 */
static int highscore_write(const high_score *score)
{
	/* Write the record, note failure */
	return (fd_write(highscore_fd, (cptr)(score), sizeof(high_score)));
}


/*
 * An integer value representing the player's "points".
 *
 * In reality it isn't so much a score as a number that has the same ordering
 * as the scores.
 *
 * It ranges from 100,000 to 141,399,999
 */
int score_points(high_score *score)
{
	int points = 0;
	int silmarils;
	
	int maxturns = 100000;
	int silmarils_factor = maxturns;
	int depth_factor = silmarils_factor * 10;
	int morgoth_factor = depth_factor * 100;

	// these lines fix a few potential problems with the score record...
	score->silmarils[1] = '\0';
	score->cur_dun[3] = '\0';

	// points from turns taken (00000 to 99999) 
	points = maxturns - atoi(score->turns);
	if (points < 0)			points = 0;
	if (points >= maxturns)	points = maxturns - 1;
	
	// points from silmarils (0 00000 to 3 00000)
	silmarils = atoi(score->silmarils);
	points += silmarils_factor * silmarils;

	// points from depth (01 0 00000 to 40 0 00000)
	if (silmarils == 0)
	{
		points += depth_factor * atoi(score->cur_dun);
	}
	else
	{
		points += depth_factor * (40 - atoi(score->cur_dun));
	}

	// points for escaping (changes 40 0 00000 to 41 0 00000)
	if (score->escaped[0] == 't')
	{
		points += depth_factor;
	}
	
	// points slaying Morgoth  (0 00 0 00000 to 1 00 0 00000)
	if (score->morgoth_slain[0] == 't')
	{
		points += morgoth_factor;
	}
	
	return (points);
}


/*
 * Just determine where a new score *would* be placed
 * Return the location (0 is best) or -1 on failure
 */
static int highscore_where(high_score *score)
{
	int i;

	high_score the_score;

	/* Paranoia -- it may not have opened */
	if (highscore_fd < 0) return (-1);

	/* Go to the start of the highscore file */
	if (highscore_seek(0)) return (-1);

	/* Read until we get to a lower score (or the end of the scores) */
	for (i = 0; i < MAX_HISCORES; i++)
	{
		if (highscore_read(&the_score)) return (i);
		if (score_points(score) > score_points(&the_score)) return (i);
	}

	/* The "last" entry is always usable */
	return (MAX_HISCORES - 1);
}


/*
 * Actually place an entry into the high score file
 * Return the location (0 is best) or -1 on "failure"
 */
static int highscore_add(high_score *score)
{
	int i, slot;
	bool done = FALSE;

	high_score the_score, tmpscore;


	/* Paranoia -- it may not have opened */
	if (highscore_fd < 0) return (-1);

	/* Determine where the score should go */
	slot = highscore_where(score);

	/* Hack -- Not on the list */
	if (slot < 0) return (-1);

	/* Hack -- prepare to dump the new score */
	the_score = (*score);

	/* Slide all the scores down one */
	for (i = slot; !done && (i < MAX_HISCORES); i++)
	{
		/* Read the old guy, note errors */
		if (highscore_seek(i)) return (-1);
		if (highscore_read(&tmpscore)) done = TRUE;

		/* Back up and dump the score we were holding */
		if (highscore_seek(i)) return (-1);
		if (highscore_write(&the_score)) return (-1);

		/* Hack -- Save the old score, for the next pass */
		the_score = tmpscore;
	}

	/* Return location used */
	return (slot);
}

/*
 * Prints a nice comma spaced natural number
 */
void comma_number(char *output, int number)
{
	if (number >= 1000000)
	{
		sprintf(output, "%d,%03d,%03d",
				number / 1000000, (number % 1000000) / 1000, number % 1000);
	}
	else if (number >= 1000)
	{
		sprintf(output, "%d,%03d",
				number / 1000, number % 1000);
	}
	else
	{
		sprintf(output, "%d", number);
	}
}

/*
 * Converts a number into the three letter code of a month
 */
void atomonth(int number, char *output)
{
	switch (number)
	{
		case 1:
			sprintf(output, "Jan");
			break;
		case 2:
			sprintf(output, "Feb");
			break;
		case 3:
			sprintf(output, "Mar");
			break;
		case 4:
			sprintf(output, "Apr");
			break;
		case 5:
			sprintf(output, "May");
			break;
		case 6:
			sprintf(output, "Jun");
			break;
		case 7:
			sprintf(output, "Jul");
			break;
		case 8:
			sprintf(output, "Aug");
			break;
		case 9:
			sprintf(output, "Sep");
			break;
		case 10:
			sprintf(output, "Oct");
			break;
		case 11:
			sprintf(output, "Nov");
			break;
		case 12:
			sprintf(output, "Dec");
			break;			
	}
}

/*
 * Display a single score.
 * Assumes the high score list is already open.
 */
extern void display_single_score(byte attr, int row, int col, int place, int fake, high_score *the_score)
{
	int pr, ph, clev, cdun, mdun;
	int aged, depth;
			
	cptr user, when;

	char out_val[160];
	char tmp_val[160];

	char aged_commas[15];
	char depth_commas[15];

	/* Extract the race/house */
	pr = atoi(the_score->p_r);
	ph = atoi(the_score->p_h);

	/* Extract the level info */
	clev = atoi(the_score->cur_lev);
	cdun = atoi(the_score->cur_dun);
	mdun = atoi(the_score->max_dun);
	
	/* Hack -- extract the turns and such */
	for (user = the_score->uid; isspace((unsigned char)*user); user++) /* loop */;
	for (when = the_score->day; isspace((unsigned char)*when); when++) /* loop */;

	aged = atoi(the_score->turns);
	depth = atoi(the_score->cur_dun) * 50;
	
	comma_number(aged_commas, aged);
	comma_number(depth_commas, depth);

	/* Clean up standard encoded form of "when" */
	if ((*when == '@') && strlen(when) == 9)
	{
		char month[4];
		
		sprintf(month,"%.2s", when + 5);
		atomonth(atoi(month), month);
		
		if (*(when + 7) == '0')		sprintf(tmp_val, "%.1s %.3s %.4s", when + 8, month, when + 1);
		else						sprintf(tmp_val, "%.2s %.3s %.4s", when + 7, month, when + 1);
				
		when = tmp_val;
	}

	/* if not displayed in a place, then don't write the place number */
	if (place == 0)
	{
		/* Prepare the first line, with the race only */
		strnfmt(out_val, sizeof(out_val),
				"     %5s ft  %s of %s",
				depth_commas, the_score->who,
				c_name + c_info[ph].alt_name);
	}
	else
	{
		/* Prepare the first line, with the house only */
		strnfmt(out_val, sizeof(out_val),
				"%3d. %5s ft  %s of %s",
				place, depth_commas, the_score->who,
				c_name + c_info[ph].alt_name);
	}

	/* Possibly ammend the first line */
	if (the_score->morgoth_slain[0] == 't')
	{
		my_strcat(out_val,     ", who defeated Morgoth in his dark halls", sizeof(out_val));
	}
	else
	{
		if (the_score->silmarils[0] == '1')
		{
			my_strcat(out_val, ", who freed a Silmaril", sizeof(out_val));
		}
		if (the_score->silmarils[0] == '2')
		{
			my_strcat(out_val, ", who freed two Silmarils", sizeof(out_val));
		}
		if (the_score->silmarils[0] == '3')
		{
			my_strcat(out_val, ", who freed all three Silmarils", sizeof(out_val));
		}
		if (the_score->silmarils[0] > '3')
		{
			my_strcat(out_val, ", who freed suspiciously many Silmarils", sizeof(out_val));
		}
	}

	/* Dump the first line */
	c_put_str(attr, out_val, row + 3, col);


	/* Prepare the second line for escapees */
	if (the_score->escaped[0] == 't')
	{
		strnfmt(out_val, sizeof(out_val),
			"               Escaped the iron hells");

		if ((the_score->morgoth_slain[0] == 't') || (the_score->silmarils[0] > '0'))
		{
			my_strcat(out_val, " and brought back the light of Valinor", sizeof(out_val));
		}
		else
		{
			if (the_score->sex[0] == 'f')	my_strcat(out_val, " with her task unfulfilled", sizeof(out_val));
			else							my_strcat(out_val, " with his task unfulfilled", sizeof(out_val));
		}
	}
	
	/* If character is still alive, display differently */
	else if (fake)
	{
		strnfmt(out_val, sizeof(out_val),
				"               Lives still, deep within Angband's vaults");

	}
	
	/* Prepare the second line for those slain */
	else
	{
		strnfmt(out_val, sizeof(out_val),
				"               Slain by %s",
				the_score->how);

		/* Mark those with a silmaril */
		if (the_score->silmarils[0] > '0')
		{
			my_strcat(out_val, " during", sizeof(out_val));
			if (the_score->sex[0] == 'f')
			{
				my_strcat(out_val, " her", sizeof(out_val));
			}
			else
			{
				my_strcat(out_val, " his", sizeof(out_val));
			}
			my_strcat(out_val, " escape", sizeof(out_val));
		}
	}
	
	/* Dump the info */
	c_put_str(attr, out_val, row + 4, col);

	/* Don't print date for living characters */
	if (fake)
	{
		strnfmt(out_val, sizeof(out_val), "               after %s turns.", aged_commas);
		c_put_str(attr, out_val, row + 5, col);
	}
	else
	{
		strnfmt(out_val, sizeof(out_val), "               after %s turns.  (%s)", aged_commas, when);
		c_put_str(attr, out_val, row + 5, col);
	}
	
	/* Print symbols for silmarils / slaying Morgoth */
	if (the_score->escaped[0] == 't')
	{
		c_put_str(attr, "  escaped", row + 3, col + 4);
	}
	if (the_score->silmarils[0] == '1')
	{
		c_put_str(attr, "         *", row + 5, col);
	}
	if (the_score->silmarils[0] == '2')
	{
		c_put_str(attr, "        * *", row + 5, col);
	}
	if (the_score->silmarils[0] > '2')
	{
		c_put_str(attr, "       * * *", row + 5, col);
	}
	if (the_score->morgoth_slain[0] == 't')
	{
		c_put_str(TERM_L_DARK, "         V", row + 4, col);
	}
		
}

/*
 * Display the scores in a given range.
 * Assumes the high score list is already open.
 * Only five entries per line, too much info.
 *
 * Mega-Hack -- allow "fake" entry at the given position.
 */
static void display_scores_aux(int from, int to, int note, high_score *score)
{
	char ch;

	int j, k, n;
	int count;
	int place, fake;

	high_score the_score;

	char tmp_val[160];

	byte attr;

	/* Paranoia -- it may not have opened */
	if (highscore_fd < 0) return;

	/* Assume we will show the first 10 */
	if (from < 0) from = 0;
	if (to < 0) to = 10;
	if (to > MAX_HISCORES) to = MAX_HISCORES;


	/* Seek to the beginning */
	if (highscore_seek(0)) return;

	/* Hack -- Count the high scores */
	for (count = 0; count < MAX_HISCORES; count++)
	{
		if (highscore_read(&the_score)) break;
	}

	/* Hack -- allow "fake" entry to be last */
	if ((note == count) && score) count++;

	/* Forget about the last entries */
	if (count > to) count = to;


	/* Show 5 per page, until "done" */
	for (k = from, j = from, place = k+1; k < count; k += 5)
	{
		/* Clear screen */
		Term_clear();

		/* Title */
		c_put_str(TERM_L_BLUE, "               Names of the Fallen", 1, 0);

		/* Indicate non-top scores */
		if (k > 0)
		{
			strnfmt(tmp_val, sizeof(tmp_val), "(from position %d)", place);
			put_str(tmp_val, 1, 40);
		}

		/* Dump 5 entries */
		for (n = 0; j < count && n < 5; place++, j++, n++)
		{
			/* Hack -- indicate death in white */
			attr = (j == note) ? TERM_WHITE : TERM_SLATE;
			
			/* Mega-Hack -- insert a "fake" record */
			if ((note == j) && score)
			{
				the_score = (*score);
				attr = TERM_WHITE;
				score = NULL;
				fake = TRUE;
				note = -1;
				j--;
			}

			/* Read a normal record */
			else
			{
				fake = FALSE;
				/* Read the proper record */
				if (highscore_seek(j)) break;
				if (highscore_read(&the_score)) break;
			}

			display_single_score(attr, n * 4, 0, place, fake, &the_score);
		}


		/* Wait for response */
		Term_putstr(15, 23, -1, TERM_L_WHITE, "(press any key)");
		ch = inkey();
		prt("", 23, 0);

		/* Hack -- notice Escape */
		if (ch == ESCAPE) break;
	}
}


/*
 * Hack -- Display the scores in a given range and quit.
 *
 * This function is only called from "main.c" when the user asks
 * to see the "high scores".
 */
void display_scores(int from, int to)
{
	char buf[1024];

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");

	/* Open the binary high score file, for reading */
	highscore_fd = fd_open(buf, O_RDONLY);

	/* Clear screen */
	Term_clear();

	/* Title */
	put_str("               Names of the Fallen", 0, 0);

	/* Display the scores */
	display_scores_aux(from, to, -1, NULL);

	/* Shut the high score file */
	fd_close(highscore_fd);

	/* Forget the high score fd */
	highscore_fd = -1;

	/* Wait for response */
	Term_putstr(15, 23, -1, TERM_L_WHITE, "(press any key)");
	(void)inkey();
	prt("", 23, 0);

	/* Quit */
	quit(NULL);
}


/*
 * Hack - save index of player's high score
 */
static int score_idx = -1;


/*
 * Counts the player's silmarils
 */
extern int silmarils_possessed(void)
{
	int silmarils = 0;
	int i;
	
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		if (((&inventory[i])->tval == TV_LIGHT) && ((&inventory[i])->sval == SV_LIGHT_SILMARIL))
			silmarils += (&inventory[i])->number;
		if ((&inventory[i])->name1 == ART_MORGOTH_1)
			silmarils += 1;
		if ((&inventory[i])->name1 == ART_MORGOTH_2)
			silmarils += 2;
		if ((&inventory[i])->name1 == ART_MORGOTH_3)
			silmarils += 3;
	}
	
	return silmarils;
}

/*
 * Creates a score record for the player
 */
static errr create_score(high_score *the_score)
{
	/* Clear the record */
	(void)WIPE(the_score, high_score);

	/* Save the version */
	strnfmt(the_score->what, sizeof(the_score->what), "%s", VERSION_STRING);

	/* Calculate and save the points */
	strnfmt(the_score->pts, sizeof(the_score->pts), "    ");
	the_score->pts[4] = '\0';

	/* Save the current player turn */
	strnfmt(the_score->turns, sizeof(the_score->turns), "%9lu", (long)playerturn);
	the_score->turns[9] = '\0';

	/* Save the date in standard encoded form */
	strftime(the_score->day, sizeof(the_score->day), "@%Y%m%d", localtime(&death_time));

	/* Save the player name (15 chars) */
	strnfmt(the_score->who, sizeof(the_score->who), "%-.15s", op_ptr->full_name);

	/* Save the player info XXX XXX XXX */
	strnfmt(the_score->uid, sizeof(the_score->uid), "%7u", player_uid);
	strnfmt(the_score->sex, sizeof(the_score->sex), "%c", (p_ptr->psex ? 'm' : 'f'));
	strnfmt(the_score->p_r, sizeof(the_score->p_r), "%2d", p_ptr->prace);
	strnfmt(the_score->p_h, sizeof(the_score->p_h), "%2d", p_ptr->phouse);

	/* Save the level and such */
	strnfmt(the_score->cur_dun, sizeof(the_score->cur_dun), "%3d", p_ptr->depth);
	the_score->cur_dun[3] = '\0';
	strnfmt(the_score->max_dun, sizeof(the_score->max_dun), "%3d", p_ptr->max_depth);
	the_score->max_dun[3] = '\0';

	/* Save the cause of death (49 chars) */
	strnfmt(the_score->how, sizeof(the_score->how), "%-.49s", p_ptr->died_from);

	/* Save the number of silmarils, whether morgoth is slain, whether the player has escaped */
	strnfmt(the_score->silmarils, sizeof(the_score->silmarils), "%1d", silmarils_possessed());
	the_score->silmarils[1] = '\0';

	if (p_ptr->morgoth_slain)
	{
		strnfmt(the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "t");
	}
	else
	{
		strnfmt(the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "f");
	}
	if (p_ptr->escaped)
	{
		strnfmt(the_score->escaped, sizeof(the_score->escaped), "t");
	}
	else
	{
		strnfmt(the_score->escaped, sizeof(the_score->escaped), "f");
	}
	
	return (0);
}

/*
 * Enters a player's name on a hi-score table, if "legal".
 *
 * Assumes "signals_ignore_tstp()" has been called.
 */
static errr enter_score(high_score *the_score)
{
#ifndef SCORE_CHEATERS
	int j;
#endif /* SCORE_CHEATERS */

	/* No score file */
	if (highscore_fd < 0)
	{
		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score file found)");
		return (0);
	}

#ifndef SCORE_WIZARDS
	/* Wizard-mode pre-empts scoring */
	if (p_ptr->noscore & 0x000F)
	{
		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score for wizards)");
		score_idx = -1;
		return (0);
	}
#endif

#ifndef SCORE_AUTOMATONS
	/* automaton-mode pre-empts scoring */
	if (p_ptr->noscore & 0x00F0)
	{
		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score for automaton)");
		score_idx = -1;
		return (0);
	}
#endif /* SCORE_AUTOMATONS */

	/* Hack -- Interupted */
	if (!p_ptr->escaped && streq(p_ptr->died_from, "Interrupting"))
	{
		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when interrupted)");
		score_idx = -1;
		return (0);
	}

	/* Hack -- Quitter */
	if (!p_ptr->escaped && strstr(p_ptr->died_from, "own hand"))
	{
		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when quitting)");
		score_idx = -1;
		return (0);
	}

#ifndef SCORE_CHEATERS
	/* Cheaters are not scored */
	for (j = OPT_SCORE; j < OPT_MAX; ++j)
	{
		if (!op_ptr->opt[j]) continue;

		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when cheating)");
		score_idx = -1;
		return (0);
	}
	
	// People who cheated death are not scored
	if (p_ptr->noscore & 0x0001)
	{
		Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when cheating)");
		score_idx = -1;
		return (0);
	}
#endif /* SCORE_CHEATERS */


	/* Grab permissions */
	safe_setuid_grab();

	/* Lock (for writing) the highscore file, or fail */
	if (fd_lock(highscore_fd, F_WRLCK)) return (1);

	/* Drop permissions */
	safe_setuid_drop();

	/* Add a new entry to the score list, see where it went */
	score_idx = highscore_add(the_score);

	/* Grab permissions */
	safe_setuid_grab();

	/* Unlock the highscore file, or fail */
	if (fd_lock(highscore_fd, F_UNLCK)) return (1);

	/* Drop permissions */
	safe_setuid_drop();

	/* Success */
	return (0);
}



/*
 * Enters a player's name on a hi-score table, if "legal", and in any
 * case, displays some relevant portion of the high score list.
 *
 * Assumes "signals_ignore_tstp()" has been called.
 */
static void top_twenty(void)
{
	/* Clear screen */
	Term_clear();

	/* No score file */
	if (highscore_fd < 0)
	{
		msg_print("Score file unavailable.");
		message_flush();
		return;
	}

	/* Player's score unavailable */
	if (score_idx == -1)
	{
		display_scores_aux(0, 10, -1, NULL);
		return;
	}

	/* Hack -- Display the top fifteen scores */
	else if (score_idx < 10)
	{
		display_scores_aux(0, 15, score_idx, NULL);
	}

	/* Display the scores surrounding the player */
	else
	{
		display_scores_aux(0, 5, score_idx, NULL);
		display_scores_aux(score_idx - 2, score_idx + 7, score_idx, NULL);
	}


	/* Success */
	return;
}


/*
 * Predict the player's location, and display it.
 */
static errr predict_score(void)
{
	int j;

	high_score the_score;

	/* No score file */
	if (highscore_fd < 0)
	{
		msg_print("Score file unavailable.");
		message_flush();
		return (0);
	}

	// create the fake score
	create_score(&the_score);

	/* See where the entry would be placed */
	j = highscore_where(&the_score);

	/* Hack -- Display the top fifteen scores */
	if (j < 10)
	{
		display_scores_aux(0, 15, j, &the_score);
	}

	/* Display some "useful" scores */
	else
	{
		display_scores_aux(0, 5, -1, NULL);
		display_scores_aux(j - 2, j + 7, j, &the_score);
	}

	/* Success */
	return (0);
}

void show_scores(void)
{
	char buf[1024];

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");

	/* Open the binary high score file, for reading */
	highscore_fd = fd_open(buf, O_RDONLY);

	/* Paranoia -- No score file */
	if (highscore_fd < 0)
	{
		msg_print("Score file unavailable.");
	}
	else
	{
		/* Save Screen */
		screen_save();

		/* Clear screen */
		Term_clear();

		/* Display the scores */
		if (character_generated)
			predict_score();
		else
			display_scores_aux(0, MAX_HISCORES, -1, NULL);

		/* Shut the high score file */
		(void)fd_close(highscore_fd);

		/* Forget the high score fd */
		highscore_fd = -1;

		/* Load screen */
		screen_load();

		/* Hack - Flush it */
		Term_fresh();
	}
}


/*
 * Hack -- Dump a character description file
 *
 * XXX XXX XXX Allow the "full" flag to dump additional info,
 * and trigger its usage from various places in the code.
 */
errr file_character(cptr name, bool full)
{
	int i, x, y;
	
	byte a;
	char c;
	
	int fd;
	
	FILE *fff = NULL;
	
	char o_name[80];
	
	char buf[1024];
	
	int holder;
	
	bool challenges = FALSE;
	
	high_score the_score;
	
	/* Unused parameter */
	(void)full;
	
	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);
	
	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);
	
	/* Check for existing file */
	fd = fd_open(buf, O_RDONLY);
	
	/* Existing file */
	if (fd >= 0)
	{
		char out_val[160];
		
		/* Close the file */
		fd_close(fd);
		
		/* Build query */
		strnfmt(out_val, sizeof(out_val), "Replace existing file %s? ", buf);
		
		/* Ask */
		if (get_check(out_val)) fd = -1;
	}
	
	/* Open the non-existing file */
	if (fd < 0) fff = my_fopen(buf, "w");
	
	/* Invalid file */
	if (!fff) return (-1);
	
	text_out_hook = text_out_to_file;
	text_out_file = fff;
	
	/* Begin dump */
	fprintf(fff, "  [%s %s Character Dump]\n\n",
	        VERSION_NAME, VERSION_STRING);
	
	/* Display player */
	display_player(0);
	
	/* Dump part of the screen */
	for (y = 2; y < 23; y++)
	{
		/* Dump each row */
		for (x = 0; x < 79; x++)
		{
			/* Get the attr/char */
			(void)(Term_what(x, y, &a, &c));
			
			/* Dump it */
			buf[x] = c;
		}
		
		/* Back up over spaces */
		while ((x > 0) && (buf[x-1] == ' ')) --x;
		
		/* Terminate */
		buf[x] = '\0';
		
		/* End the row */
		fprintf(fff, "%s\n", buf);
	}
	
	/* If dead, dump last messages and a mini screenshot */
	if (p_ptr->is_dead)
	{
		int x, y;

		i = message_num();
		if (i > 15) i = 15;
		fprintf(fff, "\n  [Last Messages]\n\n");
		while (i-- > 0)
		{
			fprintf(fff, "> %s\n", message_str((s16b)i));
		}
		fprintf(fff, "\n");

		
		fprintf(fff, "\n  [Screenshot]\n\n");

		// simple screenshot for those who died in Angband
		if (!p_ptr->escaped)
		{
			for (y = 0; y <= 6; y++)
			{
				fprintf(fff, "  ");
				for (x = 0; x <= 6; x++)
				{
					fprintf(fff, "%c", mini_screenshot_char[y][x]);
				}
				fprintf(fff, "\n");
			}
		}
		
		// Special Screenshot for escapees
		else
		{
			// grass
			fprintf(fff, "  .......\n");
			fprintf(fff, "  ~...#..\n");
			fprintf(fff, "  ~~.....\n");
			fprintf(fff, "  .~.@...\n");
			fprintf(fff, "  .~~...#\n");
			fprintf(fff, "  ..~~...\n");
			fprintf(fff, "  ...~...\n");
		}
		fprintf(fff, "\n");

	}
	
	/* Dump the equipment */
	if (p_ptr->equip_cnt)
	{
		fprintf(fff, "\n  [Equipment]\n\n");
		for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
		{
			object_type *o_ptr = &inventory[i];
			object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
			
			/* Display the weight if needed */
			if (o_ptr->weight && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM) || 
			                      (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING) || 
								  (o_ptr->tval == TV_BOW)))
			{
				int wgt = o_ptr->weight * o_ptr->number;
				char wgt_buf[80];
				
				sprintf(wgt_buf, " %d.%1d lb", wgt / 10, wgt % 10);
				my_strcat(o_name, wgt_buf, sizeof(o_name));
			}
			
			fprintf(fff, "%c) %s\n",
			        index_to_label(i), o_name);
			
			/* Describe random object attributes */
			identify_random_gen(o_ptr);
		}
		fprintf(fff, "\n\n");
	}
	
	/* Dump the inventory */
	fprintf(fff, "  [Inventory]\n\n");
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &inventory[i];
		if (!o_ptr->k_idx) break;
		
		object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
		
		/* Display the weight if needed */
		if (o_ptr->weight && ((o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_POLEARM) || 
							  (o_ptr->tval == TV_HAFTED) || (o_ptr->tval == TV_DIGGING) || 
							  (o_ptr->tval == TV_BOW)))
		{
			int wgt = o_ptr->weight * o_ptr->number;
			char wgt_buf[80];
			
			sprintf(wgt_buf, " %d.%1d lb", wgt / 10, wgt % 10);
			my_strcat(o_name, wgt_buf, sizeof(o_name));
		}
		
		fprintf(fff, "%c) %s\n",
		        index_to_label(i), o_name);
		
		/* Describe random object attributes */
		identify_random_gen(o_ptr);
	}
	fprintf(fff, "\n");
	
	fprintf(fff, "\n  [Notes]\n\n");
	
	/*dump notes to character file*/
	i = 0;
	holder = notes_buffer[i];

	while (holder != '\0')
	{
		/*get a character from the notes buffer*/
		holder = notes_buffer[i];
					
		/*output it to the character dump*/
		if (holder != '\0') fprintf(fff, "%c", holder);
		
		// increment location in notes buffer
		i++;
	}
	
	fprintf(fff, "\n");
	
	/* Count options */
	for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
	{
		if (option_desc[i] && op_ptr->opt[i])
		{
			challenges = TRUE;
		}
	}
	
	if (challenges)
	{
		/* Dump options */
		fprintf(fff, "  [Challenges]\n\n");
		
		/* Dump options */
		for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
		{
			if (option_desc[i] && op_ptr->opt[i])
			{
				fprintf(fff, "%-45s\n", option_desc[i]);
			}
		}
	}
	
	/* Skip some lines */
	fprintf(fff, "\n\n");
	
	// display a "score"
	create_score(&the_score);
	fprintf(fff, "  ['Score' %.9d]\n\n", score_points(&the_score));
	
	/* Close it */
	my_fclose(fff);
	
	/* Success */
	return (0);
}



static int final_menu(int *highlight)
{
	char ch;
	
	//char buf[80];
	
	//if (p_ptr->noscore & 0x0008)
	//{
	//	sprintf(buf, "Debugging info: %d forges generated", p_ptr->forge_count);
	//	Term_putstr(15, 21, -1, TERM_WHITE, buf);
	//}
	
	Term_putstr( 3, 10, -1, TERM_L_DARK, "____________________________________________________");
	Term_putstr(15, 12, -1, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE, "a) View scores");
	Term_putstr(15, 13, -1, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE, "b) View inventory and equipment");
	Term_putstr(15, 14, -1, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE, "c) View dungeon");
	Term_putstr(15, 15, -1, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE, "d) View final messages");
	Term_putstr(15, 16, -1, (*highlight == 5) ? TERM_L_BLUE : TERM_WHITE, "e) View character sheet");
	Term_putstr(15, 17, -1, (*highlight == 6) ? TERM_L_BLUE : TERM_WHITE, "f) Add comment to notes");
	Term_putstr(15, 18, -1, (*highlight == 7) ? TERM_L_BLUE : TERM_WHITE, "g) Save character sheet");
	Term_putstr(15, 19, -1, (*highlight == 8) ? TERM_L_BLUE : TERM_WHITE, "h) Exit");

	/* Flush the prompt */
	Term_fresh();

	/* Place cursor at current choice */
	Term_gotoxy(10, 18 + *highlight);

	/* Get key (while allowing menu commands) */
	hide_cursor = TRUE;
	ch = inkey();
	hide_cursor = FALSE;
	
	if (ch == 'a')
	{
		*highlight = 1;
		return (1);
	}

	if (ch == 'b')
	{
		*highlight = 2;
		return (2);
	}

	if (ch == 'c')
	{
		*highlight = 3;
		return (3);
	}
	
	if (ch == 'd')
	{
		*highlight = 4;
		return (4);
	}

	if (ch == 'e')
	{
		*highlight = 5;
		return (5);
	}

	if (ch == 'f')
	{
		*highlight = 6;
		return (6);
	}

	if (ch == 'g')
	{
		*highlight = 7;
		return (7);
	}

	if ((ch == 'h') || (ch == 'q') || (ch == 'Q'))
	{
		*highlight = 8;
		return (8);
	}

	/* Choose current  */
	if ((ch == '\r') || (ch == '\n') || (ch == ' '))
	{
		return (*highlight);
	}

	/* Prev item */
	if (ch == '8')
	{
		if (*highlight > 1) (*highlight)--;
		else if (*highlight == 1) *highlight = 8;
	}

	/* Next item */
	if (ch == '2')
	{
		if (*highlight < 8) (*highlight)++;
		else if (*highlight == 8) *highlight = 1;
	}

	return (0);
} 



/*
 * Handle character death
 */
static void close_game_aux(void)
{
	bool wants_to_quit = FALSE;
	high_score the_score;
	int choice = 0, highlight = 1;

    /* Dump bones file */
	//make_bones();

	/* Save dead player */
	if (!save_player())
	{
		msg_print("death save failed!");
		message_flush();
	}

	/* Get time of death */
	(void)time(&death_time);

	/* Clear screen */
	Term_clear();
	
	/* Enter player in high score list */
	create_score(&the_score);
	enter_score(&the_score);
	
	// cure hallucination and rage
	p_ptr->rage = 0;
	p_ptr->image = 0;

	/* You are dead */
	print_tomb(&the_score);

	/* Flush all input keys */
	flush();

	/* Flush messages */
	message_flush();

	/* Loop */
	while (!wants_to_quit)
	{
		choice = final_menu(&highlight);
				
		switch (choice)
		{
			// view scores
			case 1:
			{
				/* Save screen */
				screen_save();

				/* Show the scores */
				top_twenty();

				/* Load screen */
				screen_load();
				break;
			}
			
			// view inventory and equipment
			case 2:
			{
				/* Save screen */
				screen_save();

				/* Clear the screen */
				Term_clear();

				/* Examine items */
				death_examine();

				/* Load screen */
				screen_load();
				break;
			}
			
			// view dungeon
			case 3:
			{
				int i;
				
				/* Save screen */
				screen_save();
				
				// Identify all objects on the level
				for (i = 1; i < o_max; i++)
				{
					object_type *o_ptr = &o_list[i];

					/* Skip dead objects */
					if (!o_ptr->k_idx) continue;
					
					object_aware(o_ptr);
					object_known(o_ptr);
				}

				/* Light the level, show all monsters and redraw */
				Term_clear();
				wiz_light();
				do_cmd_wiz_unhide(255);
				p_ptr->redraw |= 0x0FFFFFFFL;
				handle_stuff();
				
				/* Allow the player to look around */
				do_cmd_look();

				/* Load screen */
				screen_load();

				break;
			}
			
			// view final messages
			case 4:
			{
				/* Save screen */
				screen_save();

				/* Display messages */
				do_cmd_messages();

				/* Load screen */
				screen_load();
				break;
			}
			
			// view character sheet
			case 5:
			{
				/* Save screen */
				screen_save();

				/* Show the character */
				show_info();

				/* Load screen */
				screen_load();
				break;
			}
			
			// add comment to notes
			case 6:
			{
				do_cmd_note("",  p_ptr->depth);
				break;
			}
			
			// save character sheet
			case 7:
			{
				char ftmp[80];

				strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

				if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
				{
					if (ftmp[0] && (ftmp[0] != ' '))
					{
						errr err;

						/* Save screen */
						screen_save();

						/* Dump a character file */
						err = file_character(ftmp, FALSE);

						/* Load screen */
						screen_load();

						/* Check result */
						if (err)
						{
							msg_print("Character dump failed!");
						}
						else
						{
							msg_print("Character dump successful.");
						}

						/* Flush messages */
						message_flush();
					}
				}
				break;
			}
			
			// exit
			case 8:
			{
				wants_to_quit = TRUE;
				break;
			}
		}
	}

}


/*
 * Close up the current game (player may or may not be dead)
 *
 * Note that the savefile is not saved until the tombstone is
 * actually displayed and the player has a chance to examine
 * the inventory and such.  This allows cheating if the game
 * is equipped with a "quit without save" method.  XXX XXX XXX
 */
void close_game(void)
{
	char buf[1024];


	/* Handle stuff */
	handle_stuff();

	/* Flush the messages */
	message_flush();

	/* Flush the input */
	flush();

	/* No suspending now */
	signals_ignore_tstp();

	/* Hack -- Increase "icky" depth */
	character_icky++;

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");

	/* Grab permissions */
	safe_setuid_grab();

	/* Open the high score file, for reading/writing */
	highscore_fd = fd_open(buf, O_RDWR);

	/* Drop permissions */
	safe_setuid_drop();

	/* Handle death */
	if (p_ptr->is_dead)
	{
		/* Auxiliary routine in normal games */
		if (p_ptr->game_type == 0)
		{
			close_game_aux();
		}
		else if (p_ptr->game_type == -1)
		{
			monster_lore *l_ptr = &l_list[R_IDX_ORC_ARCHER];
			
			if (p_ptr->chp <= 0)
			{
				if (l_ptr->psights == 0)
				{
					pause_with_text(tutorial_early_death_text, 5, 10);
				}
				else
				{
					pause_with_text(tutorial_late_death_text, 5, 10);
				}
			}
		}
				
		/* Now wipe the level */
		wipe_o_list();
		wipe_mon_list();
		cave_m_idx[p_ptr->py][p_ptr->px] = 0;
	}

	/* Still alive */
	else
	{
		/* Save the game */
		do_cmd_save_game();

        /* Prompt for scores XXX XXX XXX */
		Term_putstr(21, 0, -1, TERM_L_BLUE, "-more-");

		/* Predict score (or ESCAPE) */
		if (inkey() != ESCAPE) predict_score();

		// Sil-y: Sil used to crash on loading a saved game from the main menu 
		//        immediately after quitting via Control-X.
		//        adding the following lines seems to stop that.
		
		/* Now wipe the level */
		wipe_o_list();
		wipe_mon_list();
		
	}


	/* Shut the high score file */
	fd_close(highscore_fd);

	/* Forget the high score fd */
	highscore_fd = -1;

	/* Hack -- Decrease "icky" depth */
	character_icky--;

	/* Allow suspending now */
	signals_handle_tstp();
}


/*
 * Handle abrupt death of the visual system
 *
 * This routine is called only in very rare situations, and only
 * by certain visual systems, when they experience fatal errors.
 *
 * XXX XXX Hack -- clear the death flag when creating a HANGUP
 * save file so that player can see tombstone when restart.
 */
void exit_game_panic(void)
{
	/* If nothing important has happened, just quit */
	if (!character_generated || character_saved) quit("panic");

	/* Mega-Hack -- see "msg_print()" */
	msg_flag = FALSE;

	/* Clear the top line */
	prt("", 0, 0);

	/* Hack -- turn off some things */
	disturb(1, 0);

	/* Hack -- Delay death XXX XXX XXX */
	if (p_ptr->chp <= 0) p_ptr->is_dead = FALSE;

	/* Hardcode panic save */
	p_ptr->panic_save = 1;

	/* Forbid suspend */
	signals_ignore_tstp();

	/* Indicate panic save */
	my_strcpy(p_ptr->died_from, "(panic save)", sizeof(p_ptr->died_from));

	/* Panic save, or get worried */
	if (!save_player()) quit("panic save failed!");

	/* Successful panic save */
	quit("panic save succeeded!");
}



#ifdef HANDLE_SIGNALS


#include <signal.h>


typedef void (*Signal_Handler_t)(int);

/*
 * Wrapper around signal() which it is safe to take the address
 * of, in case signal itself is hidden by some some macro magic.
 */
static Signal_Handler_t wrap_signal(int sig, Signal_Handler_t handler)
{
	return signal(sig, handler);
}

/* Call this instead of calling signal() directly. */
Signal_Handler_t (*signal_aux)(int, Signal_Handler_t) = wrap_signal;


/*
 * Handle signals -- suspend
 *
 * Actually suspend the game, and then resume cleanly
 */
static void handle_signal_suspend(int sig)
{
	/* Protect errno from library calls in signal handler */
	int save_errno = errno;

	/* Disable handler */
	(void)(*signal_aux)(sig, SIG_IGN);

#ifdef SIGSTOP

	/* Flush output */
	Term_fresh();

	/* Suspend the "Term" */
	Term_xtra(TERM_XTRA_ALIVE, 0);

	/* Suspend ourself */
	(void)kill(0, SIGSTOP);

	/* Resume the "Term" */
	Term_xtra(TERM_XTRA_ALIVE, 1);

	/* Redraw the term */
	Term_redraw();

	/* Flush the term */
	Term_fresh();

#endif

	/* Restore handler */
	(void)(*signal_aux)(sig, handle_signal_suspend);

	/* Restore errno */
	errno = save_errno;
}


/*
 * Handle signals -- simple (interrupt and quit)
 *
 * This function was causing a *huge* number of problems, so it has
 * been simplified greatly.  We keep a global variable which counts
 * the number of times the user attempts to kill the process, and
 * we commit suicide if the user does this a certain number of times.
 *
 * We attempt to give "feedback" to the user as he approaches the
 * suicide thresh-hold, but without penalizing accidental keypresses.
 *
 * To prevent messy accidents, we should reset this global variable
 * whenever the user enters a keypress, or something like that.
 */
static void handle_signal_simple(int sig)
{
	/* Protect errno from library calls in signal handler */
	int save_errno = errno;

	/* Disable handler */
	(void)(*signal_aux)(sig, SIG_IGN);


	/* Nothing to save, just quit */
	if (!character_generated || character_saved) quit(NULL);


	/* Count the signals */
	signal_count++;

	/* Terminate dead characters */
	if (p_ptr->is_dead)
	{
		/* Mark the savefile */
		my_strcpy(p_ptr->died_from, "Aborting", sizeof(p_ptr->died_from));

		/* HACK - Skip the tombscreen if it is already displayed */
		if (score_idx == -1)
		{
			/* Close stuff */
			close_game();
		}

		/* Quit */
		quit("interrupt");
	}

	/* Allow suicide (after 5) */
	else if (signal_count >= 5)
	{
		/* Cause of "death" */
		my_strcpy(p_ptr->died_from, "Interrupting", sizeof(p_ptr->died_from));

		/* Commit suicide */
		p_ptr->is_dead = TRUE;

		/* Stop playing */
		p_ptr->playing = FALSE;

		/* Leaving */
		p_ptr->leaving = TRUE;

		/* Close stuff */
		close_game();

		/* Quit */
		quit("interrupt");
	}

	/* Give warning (after 4) */
	else if (signal_count >= 4)
	{
		/* Make a noise */
		Term_xtra(TERM_XTRA_NOISE, 0);

		/* Clear the top line */
		Term_erase(0, 0, 255);

		/* Display the cause */
		Term_putstr(0, 0, -1, TERM_WHITE, "Contemplating suicide!");

		/* Flush */
		Term_fresh();
	}

	/* Give warning (after 2) */
	else if (signal_count >= 2)
	{
		/* Make a noise */
		Term_xtra(TERM_XTRA_NOISE, 0);
	}

	/* Restore handler */
	(void)(*signal_aux)(sig, handle_signal_simple);

	/* Restore errno */
	errno = save_errno;
}


/*
 * Handle signal -- abort, kill, etc
 */
static void handle_signal_abort(int sig)
{
	/* Disable handler */
	(void)(*signal_aux)(sig, SIG_IGN);


	/* Nothing to save, just quit */
	if (!character_generated || character_saved) quit(NULL);


	/* Clear the bottom line */
	Term_erase(0, 23, 255);

	/* Give a warning */
	Term_putstr(0, 23, -1, TERM_RED,
	            "A gruesome software bug LEAPS out at you!");

	/* Message */
	Term_putstr(45, 23, -1, TERM_RED, "Panic save...");

	/* Flush output */
	Term_fresh();

	/* Panic Save */
	p_ptr->panic_save = 1;

	/* Panic save */
	my_strcpy(p_ptr->died_from, "(panic save)", sizeof(p_ptr->died_from));

	/* Forbid suspend */
	signals_ignore_tstp();

	/* Attempt to save */
	if (save_player())
	{
		Term_putstr(45, 23, -1, TERM_RED, "Panic save succeeded!");
	}

	/* Save failed */
	else
	{
		Term_putstr(45, 23, -1, TERM_RED, "Panic save failed!");
	}

	/* Flush output */
	Term_fresh();

	/* Quit */
	quit("software bug");
}




/*
 * Ignore SIGTSTP signals (keyboard suspend)
 */
void signals_ignore_tstp(void)
{

#ifdef SIGTSTP
	(void)(*signal_aux)(SIGTSTP, SIG_IGN);
#endif

}

/*
 * Handle SIGTSTP signals (keyboard suspend)
 */
void signals_handle_tstp(void)
{

#ifdef SIGTSTP
	(void)(*signal_aux)(SIGTSTP, handle_signal_suspend);
#endif

}


/*
 * Prepare to handle the relevant signals
 */
void signals_init(void)
{

#ifdef SIGHUP
	(void)(*signal_aux)(SIGHUP, SIG_IGN);
#endif


#ifdef SIGTSTP
	(void)(*signal_aux)(SIGTSTP, handle_signal_suspend);
#endif


#ifdef SIGINT
	(void)(*signal_aux)(SIGINT, handle_signal_simple);
#endif

#ifdef SIGQUIT
	(void)(*signal_aux)(SIGQUIT, handle_signal_simple);
#endif


#ifdef SIGFPE
	(void)(*signal_aux)(SIGFPE, handle_signal_abort);
#endif

#ifdef SIGILL
	(void)(*signal_aux)(SIGILL, handle_signal_abort);
#endif

#ifdef SIGTRAP
	(void)(*signal_aux)(SIGTRAP, handle_signal_abort);
#endif

#ifdef SIGIOT
	(void)(*signal_aux)(SIGIOT, handle_signal_abort);
#endif

#ifdef SIGKILL
	(void)(*signal_aux)(SIGKILL, handle_signal_abort);
#endif

#ifdef SIGBUS
	(void)(*signal_aux)(SIGBUS, handle_signal_abort);
#endif

#ifdef SIGSEGV
	(void)(*signal_aux)(SIGSEGV, handle_signal_abort);
#endif

#ifdef SIGTERM
	(void)(*signal_aux)(SIGTERM, handle_signal_abort);
#endif

#ifdef SIGPIPE
	(void)(*signal_aux)(SIGPIPE, handle_signal_abort);
#endif

#ifdef SIGEMT
	(void)(*signal_aux)(SIGEMT, handle_signal_abort);
#endif

/*
 * SIGDANGER:
 * This is not a common (POSIX, SYSV, BSD) signal, it is used by AIX(?) to
 * signal that the system will soon be out of memory.
 */
#ifdef SIGDANGER
	(void)(*signal_aux)(SIGDANGER, handle_signal_abort);
#endif

#ifdef SIGSYS
	(void)(*signal_aux)(SIGSYS, handle_signal_abort);
#endif

#ifdef SIGXCPU
	(void)(*signal_aux)(SIGXCPU, handle_signal_abort);
#endif

#ifdef SIGPWR
	(void)(*signal_aux)(SIGPWR, handle_signal_abort);
#endif

}


#else	/* HANDLE_SIGNALS */




/*
 * Do nothing
 */
void signals_ignore_tstp(void)
{
}

/*
 * Do nothing
 */
void signals_handle_tstp(void)
{
}

/*
 * Do nothing
 */
void signals_init(void)
{
}


#endif	/* HANDLE_SIGNALS */


static void write_html_escape_char(FILE *htm, char c)
{
	switch (c)
	{
		case '<':
			fprintf(htm, "&lt;");
			break;
		case '>':
			fprintf(htm, "&gt;");
			break;
		case '&':
			fprintf(htm, "&amp;");
			break;
		default:
			fprintf(htm, "%c", c);
			break;
	}
}

/*
 * Get the tile for a given screen location
 */
static void get_tile(int row, int col, byte *a_def, char *c_def)
{
	byte a;
	char c;

	/* Get the tile from the screen */
	a = Term->scr->a[row][col];
	c = Term->scr->c[row][col];

	/* Return the tile */
	*a_def = a;
	*c_def = c;
}

/*
 * Get the default (ASCII) tile for a given screen location
 *
 */
static void get_default_tile(int row, int col, byte *a_def, char *c_def)
{
	byte a;
	char c;

	int wid, hgt;
	int screen_wid, screen_hgt;

	int x;
	int y = row - ROW_MAP + p_ptr->wy;

	/* Retrieve current screen size */
	Term_get_size(&wid, &hgt);

	/* Calculate the size of dungeon map area (ignoring bigscreen) */
	screen_wid = wid - (COL_MAP + 1);
	screen_hgt = hgt - (ROW_MAP + 1);

	/* Get the tile from the screen */
	a = Term->scr->a[row][col];
	c = Term->scr->c[row][col];

	/* Skip bigtile placeholders */
	if (use_bigtile && (a == 255) && (c == -1))
	{
		/* Replace with "white space" */
		a = TERM_WHITE;
		c = ' ';
	}
	/* Convert the map display to the default characters */
	else if (!character_icky &&
	    ((col - COL_MAP) >= 0) && ((col - COL_MAP) < screen_wid) &&
	    ((row - ROW_MAP) >= 0) && ((row - ROW_MAP) < screen_hgt))
	{
		/* Bigtile uses double-width tiles */
		if (use_bigtile)
			x = (col - COL_MAP) / 2 + p_ptr->wx;
		else
			x = col - COL_MAP + p_ptr->wx;

		/* Convert dungeon map into default attr/chars */
		if (in_bounds(y, x))
		{
			/* Retrieve default attr/char */
			map_info_default(y, x, &a, &c);
		}
		else
		{
			/* "Out of bounds" is empty */
			a = TERM_WHITE;
			c = ' ';
		}

		if (c == '\0') c = ' ';
	}

	/* Filter out remaining graphics */
	if (a & 0xf0)
	{
		/* Replace with "white space" */
		a = TERM_WHITE;
		c = ' ';
	}

	/* Return the default tile */
	*a_def = a;
	*c_def = c;
}



/* Take an html screenshot */
void html_screenshot(cptr name)
{
	int y, x;
	int wid, hgt;

	byte a = TERM_WHITE; // a default value to soothe compilation warnings (and perhaps needed?)
	byte oa = TERM_WHITE;
	byte fg_colour = TERM_WHITE;
	int mode = 0;
	char c = ' ';

	FILE *htm;

	char buf[1024];

	/* Build the filename */
	path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);

	/* File type is "TEXT" */
	FILE_TYPE(FILE_TYPE_TEXT);

	/* Append to the file */
	htm = my_fopen(buf, "w");

	/* Oops */
	if (!htm)
	{
		plog_fmt("Cannot write the '%s' file!", buf);
		return;
	}

	/* Retrieve current screen size */
	Term_get_size(&wid, &hgt);

	fprintf(htm, "<HTML>\n");
	fprintf(htm, "<HEAD>\n");
    fprintf(htm, "<META NAME=\"GENERATOR\" Content=\"Sil %s\">\n", VERSION_STRING);
	fprintf(htm, "<TITLE>%s</TITLE>\n", name);
	fprintf(htm, "</HEAD>\n");
	fprintf(htm, "<BODY TEXT=\"#FFFFFF\" BGCOLOR=\"#000000\">");

	fprintf(htm, "<FONT COLOR=\"#%02X%02X%02X\">\n<PRE><TT>",
	             angband_color_table[TERM_WHITE][1],
	             angband_color_table[TERM_WHITE][2],
	             angband_color_table[TERM_WHITE][3]);

	/* Dump the screen */
	for (y = 0; y < hgt; y++)
	{
		for (x = 0; x < wid; x++)
		{
			/* Get the ASCII tile */
			
			// Sil-y: replaced the call to get_default_tile with get_tile
			//        this allows 'shades' to work
			//        If we want graphics to work in the future, then get_default_tile
			//        will need to be rejuvenated.
			
			// Sil-y: I still call this first to soothe compiler warnings
			//        It then gets overwritten immediately by the better version
			get_default_tile(y, x, &a, &c);

			get_tile(y, x, &a, &c);

			mode = a / MAX_COLORS;
			fg_colour = a % MAX_COLORS;

			/* Color change */
			if (oa != a)
			{
				/* From the default white to another color */
				if (oa == TERM_WHITE)
				{
					if (mode == BG_BLACK)
					{
						fprintf(htm, "<FONT COLOR=\"#%02X%02X%02X\">",
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3]);
					}
					else if (mode == BG_SAME)
					{
						fprintf(htm, "<FONT COLOR=\"#%02X%02X%02X\" style=\"BACKGROUND-COLOR: #%02X%02X%02X\">",
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3],
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3]);					
					}
					else
					{
						fprintf(htm, "<FONT COLOR=\"#%02X%02X%02X\" style=\"BACKGROUND-COLOR: #%02X%02X%02X\">",
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3],
								angband_color_table[16][1],
								angband_color_table[16][2],
								angband_color_table[16][3]);					
					}
				}
				/* From another color to the default white */
				else if (a == TERM_WHITE)
				{
					fprintf(htm, "</FONT>");
				}
				/* Change colors */
				else
				{
					if (mode == BG_BLACK)
					{
						fprintf(htm, "</FONT><FONT COLOR=\"#%02X%02X%02X\">",
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3]);
					}
					else if (mode == BG_SAME)
					{
						fprintf(htm, "</FONT><FONT COLOR=\"#%02X%02X%02X\" style=\"BACKGROUND-COLOR: #%02X%02X%02X\">",
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3],
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3]);					
					}
					else
					{
						fprintf(htm, "</FONT><FONT COLOR=\"#%02X%02X%02X\" style=\"BACKGROUND-COLOR: #%02X%02X%02X\">",
								angband_color_table[fg_colour][1],
								angband_color_table[fg_colour][2],
								angband_color_table[fg_colour][3],
								angband_color_table[16][1],
								angband_color_table[16][2],
								angband_color_table[16][3]);					
					}
				}

				/* Remember the last color */
				oa = a;
			}

			/* Write the character and escape special HTML characters */
			write_html_escape_char(htm, c);
		}

		/* End the row */
		fprintf(htm, "\n");
	}

	/* Close the last <font> tag if necessary */
	if (a != TERM_WHITE) fprintf(htm, "</FONT>");

	fprintf(htm, "</TT></PRE>\n");

	fprintf(htm, "</BODY>\n");
	fprintf(htm, "</HTML>\n");

	/* Close it */
	my_fclose(htm);
}

extern void mini_screenshot(void)
{
	int x, y, wid, hgt;
	byte a;
	char c;
	
	int player_y = 0, player_x = 0;
	
	// These widths and heights are meant to be bigger than the biggest possible terminal window
	// They are a bit of a hack.
	char screen_char[100][200];
	byte screen_attr[100][200];

	/* Retrieve current screen size */
	Term_get_size(&wid, &hgt);	
	
	/* Initialize the arrays */
	for (y = 0; y < 100; y++)
	{
		for (x = 0; x < 200; x++)
		{
			screen_char[y][x] = ' ';
			screen_attr[y][x] = TERM_DARK;
		}
	}
	
	/* Save the screen */
	for (y = 0; y < hgt; y++)
	{
		for (x = 0; x < wid; x++)
		{
				/* Get the ASCII tile */
				get_tile(y, x, &a, &c);
				
				// check to see if it is the player
				if ((c == '@') && ((a == TERM_WHITE) || (a == TERM_YELLOW) || (a == TERM_ORANGE) || 
								   (a == TERM_L_RED) || (a == TERM_RED)))
				{
					player_x = x;
					player_y = y;
				}
				
				screen_char[y][x] = c;
				screen_attr[y][x] = a;
		}
	}

	if (player_y > 0)
	{
		for (y = 0; y <= 6; y++)
		{
			for (x = 0; x <= 6; x++)
			{
				mini_screenshot_char[y][x] = screen_char[player_y - 3 + y][player_x - 3 + x];
				mini_screenshot_attr[y][x] = screen_attr[player_y - 3 + y][player_x - 3 + x];
			}
		}
	}
	else
	{
		for (y = 0; y <= 6; y++)
		{
			for (x = 0; x <= 6; x++)
			{
				mini_screenshot_char[y][x] = ' ';
				mini_screenshot_char[y][x] = TERM_DARK;
			}
		}
	}
}


extern void prt_mini_screenshot(int col, int row)
{
	int x, y;
	
	if (!p_ptr->escaped)
	{
		for (y = 0; y <= 6; y++)
		{
			for (x = 0; x <= 6; x++)
			{
				if ((x==3) && (y==3))
				{
					Term_putch(col+x, row+y, TERM_RED, mini_screenshot_char[y][x]);
				}
				else
				{
					Term_putch(col+x, row+y, mini_screenshot_attr[y][x], mini_screenshot_char[y][x]);
				}
			}
		}
	}
	else
	{
		// grass
		Term_putstr(col, row,   -1, TERM_L_GREEN, ".......");
		Term_putstr(col, row+1, -1, TERM_L_GREEN, ".......");
		Term_putstr(col, row+2, -1, TERM_L_GREEN, ".......");
		Term_putstr(col, row+3, -1, TERM_L_GREEN, ".......");
		Term_putstr(col, row+4, -1, TERM_L_GREEN, ".......");
		Term_putstr(col, row+5, -1, TERM_L_GREEN, ".......");
		Term_putstr(col, row+6, -1, TERM_L_GREEN, ".......");
		
		// river
		Term_putch(col,   row+1, TERM_BLUE, '~');
		Term_putch(col,   row+2, TERM_BLUE, '~');
		Term_putch(col+1, row+2, TERM_L_BLUE, '~');
		Term_putch(col+1, row+3, TERM_BLUE, '~');
		Term_putch(col+1, row+4, TERM_L_BLUE, '~');
		Term_putch(col+2, row+4, TERM_BLUE, '~');
		Term_putch(col+2, row+5, TERM_BLUE, '~');
		Term_putch(col+3, row+5, TERM_L_BLUE, '~');
		Term_putch(col+3, row+6, TERM_BLUE, '~');

		// trees
		Term_putch(col+4, row+1, TERM_GREEN, '#');
		Term_putch(col+6, row+4, TERM_GREEN, '#');
		
		// player
		Term_putch(col+3, row+3, TERM_WHITE, '@');
	}
	
}

