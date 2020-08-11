/* File: config.h */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

/*
 * Look through the following lines, and where a comment includes the
 * tag "OPTION:", examine the associated "#define" statements, and decide
 * whether you wish to keep, comment, or uncomment them.  You should not
 * have to modify any lines not indicated by "OPTION".
 *
 * Note: Also examine the "system" configuration file "h-config.h".
 *
 * And finally, remember that the "Makefile" will specify some rather
 * important compile time options, like what visual module to use.
 */

/*
 * OPTION: See the Makefile(s), where several options may be declared.
 *
 * Some popular options include "USE_GCU" (allow use with Unix "curses"),
 * "USE_X11" (allow basic use with Unix X11), "USE_XAW" (allow use with
 * Unix X11 plus the Athena Widget set), and "USE_CAP" (allow use with
 * the "termcap" library, or with hard-coded vt100 terminals).
 *
 * The old "USE_NCU" option has been replaced with "USE_GCU".
 *
 * Several other such options are available for non-unix machines,
 * such as "MACINTOSH", "WINDOWS", "USE_IBM", "USE_EMX".
 *
 * You may also need to specify the "system", using defines such as
 * "SOLARIS" (for Solaris), etc, see "h-config.h" for more info.
 */

/*
 * OPTION: Use the POSIX "termios" methods in "main-gcu.c"
 */
/* #define USE_TPOSIX */

/*
 * OPTION: Use the "termio" methods in "main-gcu.c"
 */
/* #define USE_TERMIO */

/*
 * OPTION: Use the icky BSD "tchars" methods in "main-gcu.c"
 */
/* #define USE_TCHARS */

/*
 * OPTION: Use "blocking getch() calls" in "main-gcu.c".
 * Hack -- Note that this option will NOT work on many BSD machines
 * Currently used whenever available, if you get a warning about
 * "nodelay()" undefined, then make sure to undefine this.
 */
#if defined(SYS_V) || defined(AMIGA)
#define USE_GETCH
#endif

/*
 * OPTION: Use the "curs_set()" call in "main-gcu.c".
 * Hack -- This option will not work on most BSD machines
 */
#ifdef SYS_V
#define USE_CURS_SET
#endif

/*
 * OPTION: Include "ncurses.h" instead of "curses.h" in "main-gcu.c"
 */
/* #define USE_NCURSES */

/*
 * OPTION: for multi-user machines running the game setuid to some other
 * user (like 'games') this SAFE_SETUID option allows the program to drop
 * its privileges when saving files that allow for user specified pathnames.
 * This lets the game be installed system wide without major security
 * concerns.  There should not be any side effects on any machines.
 *
 * This will handle "gids" correctly once the permissions are set right.
 */
#define SAFE_SETUID

/*
 * This flag enables the "POSIX" methods for "SAFE_SETUID".
 */

/* XXX - hack to get it to compile under tiger

#ifdef _POSIX_SAVED_IDS
# define SAFE_SETUID_POSIX
#endif

*/

/*
 * Prevent problems on (non-Solaris) Suns using "SAFE_SETUID".
 * The SAFE_SETUID code is weird, use it at your own risk...
 */
#if defined(SUNOS) && !defined(SOLARIS)
#undef SAFE_SETUID_POSIX
#endif

/*
 * OPTION: Forbid the use of "fiddled" savefiles.  As far as I can tell,
 * a fiddled savefile is one with an internal timestamp different from
 * the actual timestamp.  Thus, turning this option on forbids one from
 * copying a savefile to a different name.  Combined with disabling the
 * ability to save the game without quitting, and with some method of
 * stopping the user from killing the process at the tombstone screen,
 * this should prevent the use of backup savefiles.  It may also stop
 * the use of savefiles from other platforms, so be careful.
 */
/* #define VERIFY_TIMESTAMP */

/*
 * OPTION: Forbid the "savefile over-write" cheat, in which you simply
 * run another copy of the game, loading a previously saved savefile,
 * and let that copy over-write the "dead" savefile later.  This option
 * either locks the savefile, or creates a fake "xxx.lok" file to prevent
 * the use of the savefile until the file is deleted.  Not ready yet.
 */
/* #define VERIFY_SAVEFILE */

/*
 * OPTION: Hack -- Compile in support for "Debug Commands"
 */
#define ALLOW_DEBUG

/*
 * OPTION: Hack -- Compile in support for "Spoiler Generation"
 */
#define ALLOW_SPOILERS

/*
 * OPTION: Allow "do_cmd_colors" at run-time
 */
#define ALLOW_COLORS

/*
 * OPTION: Allow "do_cmd_visuals" at run-time
 */
#define ALLOW_VISUALS

/*
 * OPTION: Allow "do_cmd_macros" at run-time
 */
#define ALLOW_MACROS

/*
 * OPTION: Allow characteres to be "auto-rolled"
 */
#define ALLOW_AUTOROLLER

/*
 * OPTION: Allow parsing of the ascii template files in "init.c".
 * This must be defined if you do not have valid binary image files.
 * It should be usually be defined anyway to allow easy "updating".
 */
#define ALLOW_TEMPLATES

/*
 * OPTION: Allow repeating of last command.
 */
#define ALLOW_REPEAT

/*
 * OPTION: Handle signals
 */
#define HANDLE_SIGNALS

/*
 * OPTION: Allow "Wizards" to yield "high scores"
 */
#define SCORE_WIZARDS

/*
 * OPTION: Allow "automata" to yield "high scores"
 */
#define SCORE_AUTOMATON

/*
 * OPTION: Allow "Cheaters" to yield "high scores"
 */
/* #define SCORE_CHEATERS */

/*
 * OPTION: Support multiple "player" grids in "map_info()"
 */
/* #define MAP_INFO_MULTIPLE_PLAYERS */

/*
 * OPTION: Use the "complex" wall illumination code
 */
/* #define UPDATE_VIEW_COMPLEX_WALL_ILLUMINATION */

/*
 * OPTION: Gamma correct colours (with X11)
 */
#define SUPPORT_GAMMA

/*
 * OPTION: Check the modification time of *_info.raw files
 */
#define CHECK_MODIFICATION_TIME

/*
 * OPTION: Allow the use of "sound" in various places.
 */
#define USE_SOUND

/*
 * OPTION: Allow the use of "graphics" in various places
 */
#define USE_GRAPHICS

/*
 * Hack -- Macintosh stuff
 */
#ifdef MACINTOSH

/* Do not handle signals */
#undef HANDLE_SIGNALS

#endif

/*
 * Hack -- Windows stuff
 */
#ifdef WINDOWS

/* Do not handle signals */
#undef HANDLE_SIGNALS

#endif

/*
 * Hack -- EMX stuff
 */
#ifdef USE_EMX

/* Do not handle signals */
#undef HANDLE_SIGNALS

#endif

/*
 * OPTION: Set the "default" path to the angband "lib" directory.
 *
 * See "main.c" for usage, and note that this value is only used on
 * certain machines, primarily Unix machines.
 *
 * The configure script overrides this value.  Check the "--prefix=<dir>"
 * option of the configure script.
 *
 * This value will be over-ridden by the "ANGBAND_PATH" environment
 * variable, if that variable is defined and accessable.  The final
 * "slash" is required if the value supplied is in fact a directory.
 *
 * Using the value "./lib/" below tells Sil that, by default,
 * the user will run "sil" from the same directory that contains
 * the "lib" directory.  This is a reasonable (but imperfect) default.
 *
 * If at all possible, you should change this value to refer to the
 * actual location of the "lib" folder, for example, "/tmp/sil/lib/"
 * or "/usr/games/lib/sil/", or "/pkg/sil/lib".
 */
#ifndef DEFAULT_PATH
#define DEFAULT_PATH "./lib/"
#endif /* DEFAULT_PATH */

/*
 * OPTION: Create and use a hidden directory in the users home directory
 * for storing pref-files and character-dumps.
 */
#if defined(USE_PRIVATE_SAVE_PATH) && !defined(PRIVATE_USER_PATH)
#define PRIVATE_USER_PATH "~/.sil"
#endif

/*
 * On multiuser systems, add the "uid" to savefile names
 */
#ifdef SET_UID
#define SAVEFILE_USE_UID
#endif /* SET_UID */

/*
 * OPTION: Check the "time" against "lib/file/hours.txt"
 */
/* #define CHECK_TIME */

/*
 * OPTION: Prevent usage of the "ANGBAND_PATH" environment variable and
 * the '-d<what>=<path>' command line option (except for '-du=<path>').
 *
 * This prevents cheating in multi-user installs as well as possible
 * security problems when running setgid.
 */
#ifdef SET_UID
#define FIXED_PATHS
#endif /* SET_UID */

/*
 * OPTION: Capitalize the "user_name" (for "default" player name)
 * This option is only relevant on SET_UID machines.
 */
#define CAPITALIZE_USER_NAME

/*
 * OPTION: Person to bother if something goes wrong.
 */
#define MAINTAINER "sil@amirrorclear.net"

/*
 * OPTION: Default font (when using X11).
 */
#define DEFAULT_X11_FONT "9x15"

/*
 * OPTION: Default fonts (when using X11)
 */
#define DEFAULT_X11_FONT_0 "10x20"
#define DEFAULT_X11_FONT_1 "9x15"
#define DEFAULT_X11_FONT_2 "9x15"
#define DEFAULT_X11_FONT_3 "5x8"
#define DEFAULT_X11_FONT_4 "5x8"
#define DEFAULT_X11_FONT_5 "5x8"
#define DEFAULT_X11_FONT_6 "5x8"
#define DEFAULT_X11_FONT_7 "5x8"

/*
 * Hack -- Mach-O (native binary format of OS X) is basically a Un*x
 * but has Mac OS/Windows-like user interface
 */
#ifdef MACH_O_CARBON
#ifdef SAVEFILE_USE_UID
#undef SAVEFILE_USE_UID
#endif
#endif

/*
 * Hack -- Special "ancient machine" versions
 */
#if defined(USE_286) || defined(ANGBAND_LITE_MAC)
#ifndef ANGBAND_LITE
#define ANGBAND_LITE
#endif
#endif

/*
 * OPTION: Attempt to minimize the size of the game
 */
#ifndef ANGBAND_LITE
/* #define ANGBAND_LITE */
#endif

/*
 * Hack -- React to the "ANGBAND_LITE" flag
 */
#ifdef ANGBAND_LITE
#undef ALLOW_COLORS
#undef ALLOW_VISUALS
#undef ALLOW_MACROS
#undef ALLOW_TERROR
#undef ALLOW_DEBUG
#undef ALLOW_SPOILERS
#undef ALLOW_TEMPLATES
#endif

/*
 * OPTION: Attempt to prevent all "cheating"
 */
/* #define VERIFY_HONOR */

/*
 * React to the "VERIFY_HONOR" flag
 */
#ifdef VERIFY_HONOR
#define VERIFY_SAVEFILE
#define VERIFY_CHECKSUMS
#define VERIFY_TIMESTAMP
#endif
