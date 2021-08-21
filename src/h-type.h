/* File: h-type.h */

#include <stdint.h>

#ifndef INCLUDED_H_TYPE_H
#define INCLUDED_H_TYPE_H

/*
 * Basic "types".
 *
 * A char/byte takes exactly 1 byte
 * A s16b/u16b takes exactly 2 bytes
 * A s32b/u32b takes exactly 4 bytes
 */

/*** Special 4 letter names for some standard types ***/

/* A string pointer */
typedef const char* cptr;

/* An error code */
typedef int errr;

/*
 * Hack -- prevent problems with non-MACINTOSH
 */
#undef uint
#define uint uint_hack

/*
 * Hack -- prevent problems with AMIGA
 */
#undef byte
#define byte byte_hack

/*
 * Hack -- prevent problems with C++
 */
#undef bool
#define bool bool_hack

/* Note that unsigned values can cause math problems */
/* An unsigned byte of memory */
typedef unsigned char byte;

/* Note that a bool is smaller than a full "int" */
/* Simple True/False type */
typedef char bool;

/* An unsigned, "standard" integer (often pre-defined) */
typedef unsigned int uint;

/* Signed/Unsigned 16 bit value */
typedef int16_t s16b;
typedef uint16_t u16b;

typedef int32_t s32b;
typedef uint32_t u32b;

#endif /* INCLUDED_H_TYPE_H */
