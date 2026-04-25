/* File: h-config.h */

#ifndef INCLUDED_H_CONFIG_H
#define INCLUDED_H_CONFIG_H

/*
 * Choose the hardware, operating system, and compiler.
 * Also, choose various "system level" compilation options.
 * A lot of these definitions take effect in "h-system.h"
 *
 * Note that most of these "options" are defined by the compiler,
 * the "Makefile", the "project file", or something similar, and
 * should not be defined by the user.
 */

/*
 * iOS detection: must come before other platform checks because iOS also
 * defines __APPLE__.  TargetConditionals.h is available on all Apple
 * toolchains and provides TARGET_OS_IPHONE.
 */
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#ifndef SIL_IOS
#define SIL_IOS
#endif
#endif
#endif

/*
 * Convenience: SIL_MOBILE is defined on both Android and iOS so that
 * code shared across mobile platforms can use a single guard.
 */
#if defined(__ANDROID__) || defined(SIL_IOS)
#ifndef SIL_MOBILE
#define SIL_MOBILE
#endif
#endif

/*
 * OPTION: Compile on a Windows machine
 */
#ifndef WINDOWS
/* #define WINDOWS */
#endif

/*
 * OPTION: Compile on a SYS V version of UNIX
 */
#ifndef SYS_V
/* #define SYS_V */
#endif

/*
 * OPTION: Compile on a Solaris machine
 */
#ifndef SOLARIS
/* #define SOLARIS */
#endif

/*
 * Extract the "WINDOWS" flag from the compiler
 */
#if defined(_Windows) || defined(__WINDOWS__) || defined(__WIN32__)            \
    || defined(WIN32) || defined(__WINNT__) || defined(__NT__)                 \
    || defined(_WIN32)
#ifndef WINDOWS
#define WINDOWS
#endif
#endif

/*
 * OPTION: set "SET_UID" if the machine is a "multi-user" machine.
 * This option is used to verify the use of "uids" and "gids" for
 * various "Unix" calls, and of "pids" for getting a random seed,
 * and of the "umask()" call for various reasons, and to guess if
 * the "kill()" function is available, and for permission to use
 * functions to extract user names and expand "tildes" in filenames.
 * It is also used for "locking" and "unlocking" the score file.
 * Basically, SET_UID should *only* be set for "Unix" machines.
 */

#if !defined(WINDOWS) && !defined(__ANDROID__) && !defined(SIL_IOS)
#define SET_UID
#endif

/*
 * OPTION: Set "USG" for "System V" versions of Unix
 * This is used to choose a "lock()" function, and to choose
 * which header files ("string.h" vs "strings.h") to include.
 * It is also used to allow certain other options, such as options
 * involving userid's, or multiple users on a single machine, etc.
 */
#ifdef SET_UID
#if defined(SYS_V) || defined(SOLARIS)
#ifndef USG
#define USG
#endif
#endif
#endif

/*
 * Every system seems to use its own symbol as a path separator.
 * Default to the standard Unix slash, but attempt to change this
 * for various other systems.
 */
#undef PATH_SEP
#define PATH_SEP "/"
#if defined(WINDOWS) || defined(WINNT)
#undef PATH_SEP
#define PATH_SEP "\\"
#endif

/*
 * File type definitions (no longer platform-specific)
 */
#ifndef FILE_TYPE_TEXT
#define FILE_TYPE_TEXT
#define FILE_TYPE_DATA
#define FILE_TYPE_SAVE
#define FILE_TYPE(X)
#endif

/*
 * OPTION: Define "HAVE_USLEEP" only if "usleep()" exists.
 *
 * Note that this is only relevant for "SET_UID" machines.
 *
 * (Set in autoconf.h when HAVE_CONFIG_H -- i.e. when configure is used.)
 */
#if defined(SET_UID) && !defined(HAVE_CONFIG_H)
#define HAVE_USLEEP
#endif

#endif
/* ------------------------------------------------------------------
 * Turn *on* the meta-run curse playground in dbg_show_active_flags().
 * Comment this line out for a clean release build.
 * ------------------------------------------------------------------ */
// #define DEBUG_CURSES  //debug curses functionality
// #define SHOW_DEBUG_OPTIONS_MENU  // show the options-screen debug menu entry
// #define DEBUG  //debug messaging
