/* File: z-form.h */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

#ifndef INCLUDED_Z_FORM_H
#define INCLUDED_Z_FORM_H

#include "h-basic.h"

/*
 * This file provides low-level formatting functions with custom format
 * sequences (like "%^" for capitalizing). Higher-level API is in format.h
 */

/**** Available Functions ****/

/* Format arguments into given bounded-length buffer */
extern size_t vstrnfmt(char* buf, size_t max, cptr fmt, va_list vp);

/* Simple interface to "vstrnfmt()" */
extern size_t strnfmt(char* buf, size_t max, cptr fmt, ...);

/* Append a formatted string to another string */
extern void strnfcat(char* str, size_t max, size_t* end, cptr fmt, ...);

#endif /* INCLUDED_Z_FORM_H */
