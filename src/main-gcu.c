/* File: main-gcu.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */


/*
 * This file helps Angband run on Unix/Curses machines.
 *
 *
 * To use this file, you must define "USE_GCU" in the Makefile.
 *
 *
 * Note that this file is not "intended" to support non-Unix machines,
 * nor is it intended to support VMS or other bizarre setups.
 *
 * Also, this package assumes that the underlying "curses" handles both
 * the "nonl()" and "cbreak()" commands correctly, see the "OPTION" below.
 *
 * This code should work with most versions of "curses" or "ncurses",
 * and the "main-ncu.c" file (and USE_NCU define) are no longer used.
 *
 * See also "USE_CAP" and "main-cap.c" for code that bypasses "curses"
 * and uses the "termcap" information directly, or even bypasses the
 * "termcap" information and sends direct vt100 escape sequences.
 *
 * This file provides up to 4 term windows.
 *
 * This file will attempt to redefine the screen colors to conform to
 * standard Angband colors.  It will only do so if the terminal type
 * indicates that it can do so.  See the page:
 *
 *     http://www.umr.edu/~keldon/ang-patch/ncurses_color.html
 *
 * for information on this.
 *
 * Consider the use of "savetty()" and "resetty()".  XXX XXX XXX
 */


#include "angband.h"


#ifdef USE_GCU

#include "main.h"

/*
 * Hack -- play games with "bool" and "term"
 */
#undef bool

/* Avoid 'struct term' name conflict with <curses.h> (via <term.h>) on AIX */
#define term System_term

/*
 * Include the proper "header" file
 */
#ifdef USE_NCURSES
# include <ncurses.h>
#else
# include <curses.h>
#endif

#undef term

/*
 * Hack -- try to guess which systems use what commands
 * Hack -- allow one of the "USE_Txxxxx" flags to be pre-set.
 * Mega-Hack -- try to guess when "POSIX" is available.
 * If the user defines two of these, we will probably crash.
 */
#if !defined(USE_TPOSIX)
# if !defined(USE_TERMIO) && !defined(USE_TCHARS)
#  if defined(_POSIX_VERSION)
#   define USE_TPOSIX
#  else
#   if defined(USG) || defined(linux) || defined(SOLARIS)
#    define USE_TERMIO
#   else
#    define USE_TCHARS
#   endif
#  endif
# endif
#endif

/*
 * POSIX stuff
 */
#ifdef USE_TPOSIX
# include <sys/ioctl.h>
# include <termios.h>
#endif

/*
 * One version needs these files
 */
#ifdef USE_TERMIO
# include <sys/ioctl.h>
# include <termio.h>
#endif

/*
 * The other needs these files
 */
#ifdef USE_TCHARS
# include <sys/ioctl.h>
# include <sys/resource.h>
# include <sys/param.h>
# include <sys/file.h>
# include <sys/types.h>
#endif


/*
 * XXX XXX Hack -- POSIX uses "O_NONBLOCK" instead of "O_NDELAY"
 *
 * They should both work due to the "(i != 1)" test below.
 */
#ifndef O_NDELAY
# define O_NDELAY O_NONBLOCK
#endif


/*
 * OPTION: some machines lack "cbreak()"
 * On these machines, we use an older definition
 */
/* #define cbreak() crmode() */


/*
 * OPTION: some machines cannot handle "nonl()" and "nl()"
 * On these machines, we can simply ignore those commands.
 */
/* #define nonl() */
/* #define nl() */


/*
 * Save the "normal" and "angband" terminal settings
 */

#ifdef USE_TPOSIX

static struct termios  norm_termios;

static struct termios  game_termios;

#endif

#ifdef USE_TERMIO

static struct termio  norm_termio;

static struct termio  game_termio;

#endif

#ifdef USE_TCHARS

static struct ltchars norm_special_chars;
static struct sgttyb  norm_ttyb;
static struct tchars  norm_tchars;
static int            norm_local_chars;

static struct ltchars game_special_chars;
static struct sgttyb  game_ttyb;
static struct tchars  game_tchars;
static int            game_local_chars;

#endif

/*
 * Information about a term
 */
typedef struct term_data term_data;

struct term_data
{
	term t;                 /* All term info */

	WINDOW *win;            /* Pointer to the curses window */
};

/* Max number of windows on screen */
#define MAX_TERM_DATA 4

/* Information about our windows */
static term_data data[MAX_TERM_DATA];


/*
 * Hack -- Number of initialized "term" structures
 */
static int active = 0;


#ifdef A_COLOR

/*
 * Hack -- define "A_BRIGHT" to be "A_BOLD", because on many
 * machines, "A_BRIGHT" produces ugly "inverse" video.
 */
#ifndef A_BRIGHT
# define A_BRIGHT A_BOLD
#endif

/*
 * Software flag -- we are allowed to use color
 */
static int can_use_color = FALSE;

/*
 * Simple Angband to Curses color conversion table
 */
static int colortable[16];

#endif

/*
 * Background color we should draw with; either BLACK or DEFAULT
 */
static int bg_color = COLOR_BLACK;


/*
 * Place the "keymap" into its "normal" state
 */
static void keymap_norm(void)
{

#ifdef USE_TPOSIX

	/* restore the saved values of the special chars */
	(void)tcsetattr(0, TCSAFLUSH, &norm_termios);

#endif

#ifdef USE_TERMIO

	/* restore the saved values of the special chars */
	(void)ioctl(0, TCSETA, (char *)&norm_termio);

#endif

#ifdef USE_TCHARS

	/* restore the saved values of the special chars */
	(void)ioctl(0, TIOCSLTC, (char *)&norm_special_chars);
	(void)ioctl(0, TIOCSETP, (char *)&norm_ttyb);
	(void)ioctl(0, TIOCSETC, (char *)&norm_tchars);
	(void)ioctl(0, TIOCLSET, (char *)&norm_local_chars);

#endif

}


/*
 * Place the "keymap" into the "game" state
 */
static void keymap_game(void)
{

#ifdef USE_TPOSIX

	/* restore the saved values of the special chars */
	(void)tcsetattr(0, TCSAFLUSH, &game_termios);

#endif

#ifdef USE_TERMIO

	/* restore the saved values of the special chars */
	(void)ioctl(0, TCSETA, (char *)&game_termio);

#endif

#ifdef USE_TCHARS

	/* restore the saved values of the special chars */
	(void)ioctl(0, TIOCSLTC, (char *)&game_special_chars);
	(void)ioctl(0, TIOCSETP, (char *)&game_ttyb);
	(void)ioctl(0, TIOCSETC, (char *)&game_tchars);
	(void)ioctl(0, TIOCLSET, (char *)&game_local_chars);

#endif

}


/*
 * Save the normal keymap
 */
static void keymap_norm_prepare(void)
{

#ifdef USE_TPOSIX

	/* Get the normal keymap */
	tcgetattr(0, &norm_termios);

#endif

#ifdef USE_TERMIO

	/* Get the normal keymap */
	(void)ioctl(0, TCGETA, (char *)&norm_termio);

#endif

#ifdef USE_TCHARS

	/* Get the normal keymap */
	(void)ioctl(0, TIOCGETP, (char *)&norm_ttyb);
	(void)ioctl(0, TIOCGLTC, (char *)&norm_special_chars);
	(void)ioctl(0, TIOCGETC, (char *)&norm_tchars);
	(void)ioctl(0, TIOCLGET, (char *)&norm_local_chars);

#endif

}


/*
 * Save the keymaps (normal and game)
 */
static void keymap_game_prepare(void)
{

#ifdef USE_TPOSIX

	/* Acquire the current mapping */
	tcgetattr(0, &game_termios);

	/* Force "Ctrl-C" to interupt */
	game_termios.c_cc[VINTR] = (char)3;

	/* Force "Ctrl-Z" to suspend */
	game_termios.c_cc[VSUSP] = (char)26;

	/* Hack -- Leave "VSTART/VSTOP" alone */

	/* Disable the standard control characters */
	game_termios.c_cc[VQUIT] = (char)-1;
	game_termios.c_cc[VERASE] = (char)-1;
	game_termios.c_cc[VKILL] = (char)-1;
	game_termios.c_cc[VEOF] = (char)-1;
	game_termios.c_cc[VEOL] = (char)-1;

	/* Normally, block until a character is read */
	game_termios.c_cc[VMIN] = 1;
	game_termios.c_cc[VTIME] = 0;

#endif

#ifdef USE_TERMIO

	/* Acquire the current mapping */
	(void)ioctl(0, TCGETA, (char *)&game_termio);

	/* Force "Ctrl-C" to interupt */
	game_termio.c_cc[VINTR] = (char)3;

	/* Force "Ctrl-Z" to suspend */
	game_termio.c_cc[VSUSP] = (char)26;

	/* Hack -- Leave "VSTART/VSTOP" alone */

	/* Disable the standard control characters */
	game_termio.c_cc[VQUIT] = (char)-1;
	game_termio.c_cc[VERASE] = (char)-1;
	game_termio.c_cc[VKILL] = (char)-1;
	game_termio.c_cc[VEOF] = (char)-1;
	game_termio.c_cc[VEOL] = (char)-1;

#if 0
	/* Disable the non-posix control characters */
	game_termio.c_cc[VEOL2] = (char)-1;
	game_termio.c_cc[VSWTCH] = (char)-1;
	game_termio.c_cc[VDSUSP] = (char)-1;
	game_termio.c_cc[VREPRINT] = (char)-1;
	game_termio.c_cc[VDISCARD] = (char)-1;
	game_termio.c_cc[VWERASE] = (char)-1;
	game_termio.c_cc[VLNEXT] = (char)-1;
	game_termio.c_cc[VSTATUS] = (char)-1;
#endif

	/* Normally, block until a character is read */
	game_termio.c_cc[VMIN] = 1;
	game_termio.c_cc[VTIME] = 0;

#endif

#ifdef USE_TCHARS

	/* Get the default game characters */
	(void)ioctl(0, TIOCGETP, (char *)&game_ttyb);
	(void)ioctl(0, TIOCGLTC, (char *)&game_special_chars);
	(void)ioctl(0, TIOCGETC, (char *)&game_tchars);
	(void)ioctl(0, TIOCLGET, (char *)&game_local_chars);

	/* Force suspend (^Z) */
	game_special_chars.t_suspc = (char)26;

	/* Cancel some things */
	game_special_chars.t_dsuspc = (char)-1;
	game_special_chars.t_rprntc = (char)-1;
	game_special_chars.t_flushc = (char)-1;
	game_special_chars.t_werasc = (char)-1;
	game_special_chars.t_lnextc = (char)-1;

	/* Force interupt (^C) */
	game_tchars.t_intrc = (char)3;

	/* Force start/stop (^Q, ^S) */
	game_tchars.t_startc = (char)17;
	game_tchars.t_stopc = (char)19;

	/* Cancel some things */
	game_tchars.t_quitc = (char)-1;
	game_tchars.t_eofc = (char)-1;
	game_tchars.t_brkc = (char)-1;

#endif

}




/*
 * Suspend/Resume
 */
static errr Term_xtra_gcu_alive(int v)
{
	int x, y;


	/* Suspend */
	if (!v)
	{
		/* Go to normal keymap mode */
		keymap_norm();

		/* Restore modes */
		nocbreak();
		echo();
		nl();

		/* Hack -- make sure the cursor is visible */
		Term_xtra(TERM_XTRA_SHAPE, 1);

		/* Flush the curses buffer */
		(void)refresh();

		/* Get current cursor position */
		getyx(curscr, y, x);

		/* Move the cursor to bottom right corner */
		mvcur(y, x, LINES - 1, 0);

		/* Exit curses */
		endwin();

		/* Flush the output */
		(void)fflush(stdout);
	}

	/* Resume */
	else
	{
		/* Refresh */
		/* (void)touchwin(curscr); */
		/* (void)wrefresh(curscr); */

		/* Restore the settings */
		cbreak();
		noecho();
		nonl();

		/* Go to angband keymap mode */
		keymap_game();
	}

	/* Success */
	return (0);
}


#ifdef USE_NCURSES
const char help_gcu[] = "NCurses, for terminal console, subopts -b(ig screen)";
#else /* USE_NCURSES */
const char help_gcu[] = "Curses, for terminal console, subopts -b(ig screen)";
#endif /* USE_NCURSES */


/*
 * Init the "curses" system
 */
static void Term_init_gcu(term *t)
{
	term_data *td = (term_data *)(t->data);

#ifdef USE_GETCH
	/*
	 * This is necessary to keep the first call to getch()
	 * from clearing the screen
	 */
	wrefresh(stdscr);
#endif /* USE_GETCH */

	/* Count init's, handle first */
	if (active++ != 0) return;

	/* Erase the window */
	(void)wclear(td->win);

	/* Reset the cursor */
	(void)wmove(td->win, 0, 0);

	/* Flush changes */
	(void)wrefresh(td->win);

	/* Game keymap */
	keymap_game();
}


/*
 * Nuke the "curses" system
 */
static void Term_nuke_gcu(term *t)
{
	int x, y;
	term_data *td = (term_data *)(t->data);

	/* Delete this window */
	delwin(td->win);

	/* Count nuke's, handle last */
	if (--active != 0) return;

	/* Hack -- make sure the cursor is visible */
	Term_xtra(TERM_XTRA_SHAPE, 1);

#ifdef A_COLOR
	/* Reset colors to defaults */
	start_color();
#endif

	/* Get current cursor position */
	getyx(curscr, y, x);

	/* Move the cursor to bottom right corner */
	mvcur(y, x, LINES - 1, 0);

	/* Flush the curses buffer */
	(void)refresh();

	/* Exit curses */
	endwin();

	/* Flush the output */
	(void)fflush(stdout);

	/* Normal keymap */
	keymap_norm();
}




#ifdef USE_GETCH

/*
 * Process events, with optional wait
 */
static errr Term_xtra_gcu_event(int v)
{
	int i, k;

	/* Wait */
	if (v)
	{
		/* Paranoia -- Wait for it */
		nodelay(stdscr, FALSE);

		/* Get a keypress */
		i = getch();

		/* Mega-Hack -- allow graceful "suspend" */
		for (k = 0; (k < 10) && (i == ERR); k++) i = getch();

		/* Broken input is special */
		if (i == ERR) exit_game_panic();
		if (i == EOF) exit_game_panic();
	}

	/* Do not wait */
	else
	{
		/* Do not wait for it */
		nodelay(stdscr, TRUE);

		/* Check for keypresses */
		i = getch();

		/* Wait for it next time */
		nodelay(stdscr, FALSE);

		/* None ready */
		if (i == ERR) return (1);
		if (i == EOF) return (1);
	}

	/* Enqueue the keypress */
	Term_keypress(i);

	/* Success */
	return (0);
}

#else	/* USE_GETCH */

/*
 * Process events (with optional wait)
 */
static errr Term_xtra_gcu_event(int v)
{
	int i, k;

	char buf[2];

	/* Wait */
	if (v)
	{
		/* Wait for one byte */
		i = read(0, buf, 1);

		/* Hack -- Handle bizarre "errors" */
		if ((i <= 0) && (errno != EINTR)) exit_game_panic();
	}

	/* Do not wait */
	else
	{
		/* Get the current flags for stdin */
		k = fcntl(0, F_GETFL, 0);

		/* Oops */
		if (k < 0) return (1);

		/* Tell stdin not to block */
		if (fcntl(0, F_SETFL, k | O_NDELAY) < 0) return (1);

		/* Read one byte, if possible */
		i = read(0, buf, 1);

		/* Replace the flags for stdin */
		if (fcntl(0, F_SETFL, k)) return (1);
	}

	/* Ignore "invalid" keys */
	if ((i != 1) || (!buf[0])) return (1);

	/* Enqueue the keypress */
	Term_keypress(buf[0]);

	/* Success */
	return (0);
}

#endif	/* USE_GETCH */

static int scale_color(int i, int j, int scale)
{
    return (angband_color_table[i][j] * (scale - 1) + 127) / 255;
}

static int create_color(int i, int scale)
{
    int r = scale_color(i, 1, scale);
    int g = scale_color(i, 2, scale);
    int b = scale_color(i, 3, scale);
    int rgb = 16 + scale * scale * r + scale * g + b;
    /* In the case of white and black we need to use the ANSI colors */
    if (r == g && g == b)
    {
        if (b == 0) rgb = 0;
        if (b == scale) rgb = 15;
    }
    return rgb;
}

/*
 * React to changes
 */
static errr Term_xtra_gcu_react(void)
{

#ifdef A_COLOR
    if (COLORS == 256 || COLORS == 88)
    {
        /* CTK: I snagged this color handling from current Vanilla */
        /* If we have more than 16 colors, find the best matches. These numbers
        * correspond to xterm/rxvt's builtin color numbers--they do not
        * correspond to curses' constants OR with curses' color pairs.
        *
        * XTerm has 216 (6*6*6) RGB colors, with each RGB setting 0-5.
        * RXVT has 64 (4*4*4) RGB colors, with each RGB setting 0-3.
        *
        * Both also have the basic 16 ANSI colors, plus some extra grayscale
        * colors which we do not use.
        */
        int i;
        int scale = COLORS == 256 ? 6 : 4;
        for (i = 0; i < 16; i++)
        {
            int fg = create_color(i, scale);
            init_pair(i + 1, fg, bg_color);
            colortable[i] = COLOR_PAIR(i + 1) | A_NORMAL;
        }
    }

#endif

	/* Success */
	return (0);
}


/*
 * Handle a "special request"
 */
static errr Term_xtra_gcu(int n, int v)
{
	term_data *td = (term_data *)(Term->data);

	/* Analyze the request */
	switch (n)
	{
		/* Clear screen */
		case TERM_XTRA_CLEAR:
		touchwin(td->win);
		(void)wclear(td->win);
		return (0);

		/* Make a noise */
		case TERM_XTRA_NOISE:
		(void)write(1, "\007", 1);
		return (0);

		/* Flush the Curses buffer */
		case TERM_XTRA_FRESH:
		(void)wrefresh(td->win);
		return (0);

#ifdef USE_CURS_SET

		/* Change the cursor visibility */
		case TERM_XTRA_SHAPE:
		curs_set(v);
		return (0);

#endif

		/* Suspend/Resume curses */
		case TERM_XTRA_ALIVE:
		return (Term_xtra_gcu_alive(v));

		/* Process events */
		case TERM_XTRA_EVENT:
		return (Term_xtra_gcu_event(v));

		/* Flush events */
		case TERM_XTRA_FLUSH:
		while (!Term_xtra_gcu_event(FALSE));
		return (0);

		/* Delay */
		case TERM_XTRA_DELAY:
		if (v > 0) usleep(1000 * v);
		return (0);

		/* React to events */
		case TERM_XTRA_REACT:
		Term_xtra_gcu_react();
		return (0);
	}

	/* Unknown */
	return (1);
}


/*
 * Actually MOVE the hardware cursor
 */
static errr Term_curs_gcu(int x, int y)
{
	term_data *td = (term_data *)(Term->data);

	/* Literally move the cursor */
	wmove(td->win, y, x);

	/* Success */
	return (0);
}


/*
 * Erase a grid of space
 * Hack -- try to be "semi-efficient".
 */
static errr Term_wipe_gcu(int x, int y, int n)
{
	term_data *td = (term_data *)(Term->data);

	/* Place cursor */
	wmove(td->win, y, x);

	/* Clear to end of line */
	if (x + n >= td->t.wid)
	{
		wclrtoeol(td->win);
	}

	/* Clear some characters */
	else
	{
		while (n-- > 0) waddch(td->win, ' ');
	}

	/* Success */
	return (0);
}


/*
 * Place some text on the screen using an attribute
 */
static errr Term_text_gcu(int x, int y, int n, byte a, cptr s)
{
	term_data *td = (term_data *)(Term->data);

	int i;

#ifdef A_COLOR
	/* Set the color */
	if (can_use_color) wattrset(td->win, colortable[a & 0x0F]);
#endif

	/* Move the cursor */
	wmove(td->win, y, x);

	/* Draw each character */
	for (i = 0; i < n; i++)
	{
#ifdef USE_GRAPHICS
		int pic;

		/* Special character */
		if (use_graphics && (s[i] & 0x80))
		{
			/* Determine picture to use */
			switch (s[i] & 0x7F)
			{

#ifdef ACS_CKBOARD
				/* Wall */
				case '#':
					pic = ACS_CKBOARD;
					break;
#endif /* ACS_CKBOARD */

#ifdef ACS_BOARD
				/* Mineral vein */
				case '%':
					pic = ACS_BOARD;
					break;
#endif /* ACS_BOARD */

				/* XXX */
				default:
					pic = '?';
					break;
			}

			/* Draw the picture */
			waddch(td->win, pic);

			/* Next character */
			continue;
		}
#endif /* USE_GRAPHICS */

		/* Draw a normal character */
		waddch(td->win, (byte)s[i]);
	}

	/* Success */
	return (0);
}


/*
 * Create a window for the given "term_data" argument.
 *
 * Assumes legal arguments.
 */
static errr term_data_init_gcu(term_data *td, int rows, int cols, int y, int x)
{
	term *t = &td->t;

	/* Create new window */
	td->win = newwin(rows, cols, y, x);

	/* Check for failure */
	if (!td->win)
	{
		/* Error */
		quit("Failed to setup curses window.");
	}

	/* Initialize the term */
	term_init(t, cols, rows, 256);

	/* Avoid bottom right corner */
	t->icky_corner = TRUE;

	/* Erase with "white space" */
	t->attr_blank = TERM_WHITE;
	t->char_blank = ' ';

	/* Set some hooks */
	t->init_hook = Term_init_gcu;
	t->nuke_hook = Term_nuke_gcu;

	/* Set some more hooks */
	t->text_hook = Term_text_gcu;
	t->wipe_hook = Term_wipe_gcu;
	t->curs_hook = Term_curs_gcu;
	t->xtra_hook = Term_xtra_gcu;

	/* Save the data */
	t->data = td;

	/* Activate it */
	Term_activate(t);

	/* Success */
	return (0);
}


static void hook_quit(cptr str)
{
	/* Unused */
	(void)str;

	/* Exit curses */
	endwin();
}


/*
 * Prepare "curses" for use by the file "z-term.c"
 *
 * Installs the "hook" functions defined above, and then activates
 * the main screen "term", which clears the screen and such things.
 *
 * Someone should really check the semantics of "initscr()"
 */
errr init_gcu(int argc, char **argv)
{
	int i;

	int num_term = MAX_TERM_DATA, next_win = 0;

	bool use_big_screen = FALSE;


	/* Parse args */
	for (i = 1; i < argc; i++)
	{
		if (prefix(argv[i], "-b"))
		{
			use_big_screen = TRUE;
			continue;
		}

		plog_fmt("Ignoring option: %s", argv[i]);
	}


	/* Extract the normal keymap */
	keymap_norm_prepare();

	/* Initialize */
	if (initscr() == NULL) return (-1);

	/* RAW */
	raw();

	/* Activate hooks */
	quit_aux = hook_quit;
	core_aux = hook_quit;

	/* Require standard size screen */
	if ((LINES < 24) || (COLS < 80))
	{
		quit("Angband needs at least an 80x24 'curses' screen");
	}


#ifdef USE_GRAPHICS

	/* Set graphics */
	if (arg_graphics)
	{
		use_graphics = GRAPHICS_PSEUDO;

		ANGBAND_GRAF = "pseudo";
	}


#endif /* USE_GRAPHICS */

#ifdef A_COLOR

	/*** Init the Color-pairs and set up a translation table ***/

	/* Do we have color, and enough color, available? */
	can_use_color = ((start_color() != ERR) && has_colors() &&
	                 (COLORS >= 8) && (COLOR_PAIRS >= 8));

	/* Attempt to use colors */
	if (can_use_color)
	{
		/* Color-pair 0 is *always* WHITE on BLACK */

		/* Prepare the color pairs */
		init_pair(1, COLOR_RED,     bg_color);
		init_pair(2, COLOR_GREEN,   bg_color);
		init_pair(3, COLOR_YELLOW,  bg_color);
		init_pair(4, COLOR_BLUE,    bg_color);
		init_pair(5, COLOR_MAGENTA, bg_color);
		init_pair(6, COLOR_CYAN,    bg_color);
		init_pair(7, COLOR_BLACK,   bg_color);

		/* Prepare the colors */
		colortable[0] = (COLOR_PAIR(7) | A_NORMAL);	/* Black */
		colortable[1] = (COLOR_PAIR(0) | A_BRIGHT);	/* White */
		colortable[2] = (COLOR_PAIR(0) | A_NORMAL);	/* Grey XXX */
		colortable[3] = (COLOR_PAIR(1) | A_BRIGHT);	/* Orange XXX */
		colortable[4] = (COLOR_PAIR(1) | A_NORMAL);	/* Red */
		colortable[5] = (COLOR_PAIR(2) | A_NORMAL);	/* Green */
		colortable[6] = (COLOR_PAIR(4) | A_NORMAL);	/* Blue */
		colortable[7] = (COLOR_PAIR(3) | A_NORMAL);	/* Umber */
		colortable[8] = (COLOR_PAIR(7) | A_BRIGHT);	/* Dark-grey XXX */
		colortable[9] = (COLOR_PAIR(0) | A_NORMAL);	/* Light-grey XXX */
		colortable[10] = (COLOR_PAIR(5) | A_NORMAL);	/* Purple */
		colortable[11] = (COLOR_PAIR(3) | A_BRIGHT);	/* Yellow */
		colortable[12] = (COLOR_PAIR(5) | A_BRIGHT);	/* Light Red XXX */
		colortable[13] = (COLOR_PAIR(2) | A_BRIGHT);	/* Light Green */
		colortable[14] = (COLOR_PAIR(4) | A_BRIGHT);	/* Light Blue */
		colortable[15] = (COLOR_PAIR(3) | A_NORMAL);	/* Light Umber XXX */
	}

#endif


	/*** Low level preparation ***/

#ifdef USE_GETCH

	/* Paranoia -- Assume no waiting */
	nodelay(stdscr, FALSE);

#endif

	/* Prepare */
	cbreak();
	noecho();
	nonl();

	/* Extract the game keymap */
	keymap_game_prepare();


	/*** Now prepare the term(s) ***/

	/* Big screen -- one big term */
	if (use_big_screen)
	{
		/* Create a term */
		term_data_init_gcu(&data[0], LINES, COLS, 0, 0);

		/* Remember the term */
		angband_term[0] = &data[0].t;
	}

	/* No big screen -- create as many term windows as possible */
	else
	{
		/*
		 * If we have a REALLY big screen, try to put any
		 * extra real estate into the upper-left window.
		 * Hack -- these constants rely on a-priori knowledge
		 * of the sort of things that go in the windows.
		 *
		 * This patch is by 'bron' from the Angband Forum
		 * under the directions: 'Feel free to use this (or not) as you see fit.'
		 */

		/* Minimum size for UpperLeft window */
		const int ul_min_rows = 24;
		const int ul_min_cols = 80;

		/* Maximum (useful) columns for UpperRight window */
		const int ur_max_cols = 80;

		/* Maximum (useful) rows for LowerLeft window */
		const int ll_max_rows = 26;


		/* Actual size of UpperLeft window */
		int ul_rows = MAX(ul_min_rows, LINES - (ll_max_rows + 1));
		int ul_cols = MAX(ul_min_cols, COLS - (ur_max_cols + 1));



		/* Create several terms */
		for (i = 0; i < num_term; i++)
		{
			int rows, cols, y, x;

			/* Decide on size and position */
			switch (i)
			{
				/* Upper left */
				case 0:
				{
					rows = ul_rows;
					cols = ul_cols;
					y = x = 0;
					break;
				}

				/* Lower left */
				case 1:
				{
					rows = LINES - (ul_rows + 1);
					cols = ul_cols;
					y = ul_rows + 1;
					x = 0;
					break;
				}

				/* Upper right */
				case 2:
				{
					rows = ul_rows;
					cols = COLS - (ul_cols + 1);
					y = 0;
					x = ul_cols + 1;
					break;
				}

				/* Lower right */
				case 3:
				{
					rows = LINES - (ul_rows + 1);
					cols = COLS - (ul_cols + 1);
					y = ul_rows + 1;
					x = ul_cols + 1;
					break;
				}

				/* XXX */
				default:
				{
					rows = cols = y = x = 0;
					break;
				}
			}

			/* Skip non-existant windows */
			if (rows <= 0 || cols <= 0) continue;

			/* Create a term */
			term_data_init_gcu(&data[next_win], rows, cols, y, x);

			/* Remember the term */
			angband_term[next_win] = &data[next_win].t;

			/* One more window */
			next_win++;
		}
	}

	/* Activate the "Angband" window screen */
	Term_activate(&data[0].t);

	/* Remember the active screen */
	term_screen = &data[0].t;

	/* Success */
	return (0);
}


#endif /* USE_GCU */
