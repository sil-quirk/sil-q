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
#include "log/fatal.h"
#include "mem/alloc.h"
#include "format.h"
#include "rng.h"
#include "z-term.h"

/*
 * Include the high-level includes.
 */
#include "config.h"
#include "defines.h"
#include "types.h"
#include "supplies.h"

#include <SDL3/SDL.h>

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

/* ------------------------------------------------------------------------ */
/* Ego affix helpers                                                        */
/* ------------------------------------------------------------------------ */

/*
 * Ego items now support a prefix and a suffix.
 *
 * Storage:
 * - Suffix ego index is stored in object_type.name2 (legacy ego field).
 * - Prefix ego index is stored in object_type.unused2 (reserved savefile field).
 *
 * Do not access these fields directly outside low-level helpers; use the
 * accessors below to keep semantics consistent across the codebase.
 */
static inline byte object_ego_prefix(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;
    if (o_ptr->unused2 <= 0)
        return 0;
    if (o_ptr->unused2 > 255)
        return 0;
    return (byte)o_ptr->unused2;
}

static inline void object_set_ego_prefix(object_type* o_ptr, int e_idx)
{
    if (!o_ptr)
        return;
    if (e_idx <= 0)
    {
        o_ptr->unused2 = 0;
        return;
    }
    o_ptr->unused2 = (s32b)(byte)e_idx;
}

static inline byte object_ego_suffix(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;
    return o_ptr->name2;
}

static inline void object_set_ego_suffix(object_type* o_ptr, int e_idx)
{
    if (!o_ptr)
        return;
    if (e_idx <= 0)
    {
        o_ptr->name2 = 0;
        return;
    }
    o_ptr->name2 = (byte)e_idx;
}

static inline bool object_has_ego(const object_type* o_ptr)
{
    return object_ego_prefix(o_ptr) || object_ego_suffix(o_ptr);
}

static inline bool object_has_ego_idx(const object_type* o_ptr, int e_idx)
{
    if (!o_ptr || e_idx <= 0 || e_idx > 255)
        return false;
    return object_ego_prefix(o_ptr) == (byte)e_idx
        || object_ego_suffix(o_ptr) == (byte)e_idx;
}

static inline s32b object_runtime_state(const object_type* o_ptr)
{
    if (!o_ptr)
        return OBJECT_RUNTIME_STATE_NONE;
    return o_ptr->unused3;
}

static inline void object_set_runtime_state(object_type* o_ptr, s32b state)
{
    if (!o_ptr)
        return;
    o_ptr->unused3 = state;
}

static inline s32b object_runtime_payload(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;
    return o_ptr->unused4;
}

static inline void object_set_runtime_payload(object_type* o_ptr, s32b payload)
{
    if (!o_ptr)
        return;
    o_ptr->unused4 = payload;
}

static inline bool ego_name_is_prefix(const char* name)
{
    if (!name || !name[0])
        return false;
    size_t len = strlen(name);
    return (len >= 2 && name[0] == '(' && name[len - 1] == ')');
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

/*
 * String duplication and free helpers (SDL3-based)
 * These replace the legacy string_make/string_free from z-virt.c
 */

/* Duplicate a string using SDL memory allocation */
static inline char* str_dup(const char* str) {
    if (!str) return NULL;
    return SDL_strdup(str);
}

/* Free a string allocated with str_dup and return NULL */
static inline void* str_free(const char* str) {
    if (str) SDL_free((void*)str);
    return NULL;
}

#endif
