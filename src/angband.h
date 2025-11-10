/* File: angband.h */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#ifndef INCLUDED_ANGBAND_H
#define INCLUDED_ANGBAND_H

/*
 * Include the low-level includes.
 */
#include "h-basic.h"

/*
 * Include the mid-level includes.
 */
#include "z-util.h"
#include "z-virt.h"
#include "format.h"
#include "rng.h"
#include "z-term.h"
#include "log/log.h"

/*
 * Include the high-level includes.
 */
#include "config.h"
#include "defines.h"
#include "types.h"
#include "supplies.h"

#include <SDL3/SDL.h>

#include "externs.h"

/***** Some older copyright messages follow below *****/

/*
 * Note that these copyright messages apply to an ancient version
 * of Angband, as in, from pre-2.4.frog-knows days, and thus the
 * references to version numbers may be rather misleading...
 */

/*
 * UNIX ANGBAND Version 5.0
 */

/* Original copyright message follows. */

/*
 * ANGBAND Version 4.8	COPYRIGHT (c) Robert Alan Koeneke
 *
 *	 I lovingly dedicate this game to hackers and adventurers
 *	 everywhere...
 *
 *	 Designer and Programmer:
 *		Robert Alan Koeneke
 *		University of Oklahoma
 *
 *	 Assistant Programmer:
 *		Jimmey Wayne Todd
 *		University of Oklahoma
 *
 *	 Assistant Programmer:
 *		Gary D. McAdoo
 *		University of Oklahoma
 *
 *	 UNIX Port:
 *		James E. Wilson
 *		UC Berkeley
 *		wilson@ernie.Berkeley.EDU
 *		ucbvax!ucbernie!wilson
 */

/*
 *	 ANGBAND may be copied and modified freely as long as the above
 *	 credits are retained.	No one who-so-ever may sell or market
 *	 this software in any form without the expressed written consent
 *	 of the author Robert Alan Koeneke.
 */

/*
 * Inline string helper functions (replacing z-util.c implementations)
 * These provide simple wrappers for common string operations.
 */
#include <string.h>

/* String equality check */
static inline bool streq(const char* a, const char* b) {
    return (strcmp(a, b) == 0);
}

/* Check if string t is a prefix of string s */
static inline bool prefix(const char* s, const char* t) {
    while (*t) {
        if (*t++ != *s++) return false;
    }
    return true;
}

/* Check if string t is a suffix of string s */
static inline bool suffix(const char* s, const char* t) {
    size_t slen = strlen(s);
    size_t tlen = strlen(t);
    if (tlen > slen) return false;
    return (strcmp(s + slen - tlen, t) == 0);
}

#endif
