/* File: z-virt.h */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

#ifndef INCLUDED_Z_VIRT_H
#define INCLUDED_Z_VIRT_H

#include "h-basic.h"

/*
 * Memory management routines.
 *
 * Modern C17 interface: Use mem_alloc_array(), mem_alloc(), and mem_free_null()
 * defined at the bottom of this file for all new code. These provide better type
 * safety and debuggability than the legacy macro interface.
 *
 * Legacy macro interface: The original z-virt macros (C_MAKE, MAKE, FREE, KILL, etc.)
 * are deprecated as of 2025. All codebase usage has been migrated to the modern
 * interface. The macros remain defined for reference only.
 *
 * Set ralloc_aux to modify the memory allocation routine.
 * Set rnfree_aux to modify the memory de-allocation routine.
 * Set rpanic_aux to let the program react to memory failures.
 *
 * These routines work best as a *replacement* for malloc/free.
 *
 * The string_make() and string_free() routines handle dynamic strings.
 * A dynamic string is a string allocated at run-time, which should not
 * be modified once it has been created.
 */

/**** Available macros ****/

/* Size of 'N' things of type 'T' */
#define C_SIZE(N, T) ((N) * (sizeof(T)))

/* Size of one thing of type 'T' */
#define SIZE(T) (sizeof(T))

/* Compare two arrays of type T[N], at locations P1 and P2 */
#define C_DIFF(P1, P2, N, T) (memcmp((P1), (P2), C_SIZE(N, T)))

/* Compare two things of type T, at locations P1 and P2 */
#define DIFF(P1, P2, T) (memcmp((P1), (P2), SIZE(T)))

/* Set every byte in an array of type T[N], at location P, to V, and return P */
#define C_BSET(P, V, N, T) (memset((P), (V), C_SIZE(N, T)))

/* Set every byte in a thing of type T, at location P, to V, and return P */
#define BSET(P, V, T) (memset((P), (V), SIZE(T)))

/*
 * DEPRECATED MACROS - Use modern alternatives below
 * 
 * These macros are kept for reference only. All code has been migrated
 * to use the modern mem_* functions defined below. Do not use these in
 * new code or when modifying existing code.
 * 
 * Migration guide:
 *   C_MAKE(ptr, N, type) → ptr = mem_alloc_array(N, type)
 *   MAKE(ptr, type)      → ptr = mem_alloc(type)
 *   FREE(ptr)            → mem_free_null(ptr)
 *   KILL(ptr)            → mem_free_null(ptr)
 *   C_WIPE(ptr, N, type) → memset(ptr, 0, N * sizeof(type))
 *   WIPE(ptr, type)      → memset(ptr, 0, sizeof(type))
 *   C_COPY(d, s, N, T)   → memcpy(d, s, N * sizeof(T))
 *   COPY(dst, src, type) → memcpy(dst, src, sizeof(type))
 */

/* Wipe an array of type T[N], at location P, and return P */
#define C_WIPE(P, N, T) (memset((P), 0, C_SIZE(N, T)))

/* Wipe a thing of type T, at location P, and return P */
#define WIPE(P, T) (memset((P), 0, SIZE(T)))

/* Load an array of type T[N], at location P1, from another, at location P2 */
#define C_COPY(P1, P2, N, T) (memcpy((P1), (P2), C_SIZE(N, T)))

/* Load a thing of type T, at location P1, from another, at location P2 */
#define COPY(P1, P2, T) (memcpy((P1), (P2), SIZE(T)))

/* DEPRECATED: Allocate, and return, an array of type T[N] - Use mem_alloc_array() */
#define C_RNEW(N, T) (ralloc(C_SIZE(N, T)))

/* DEPRECATED: Allocate, and return, a thing of type T - Use mem_alloc() */
#define RNEW(T) (ralloc(SIZE(T)))

/* DEPRECATED: Allocate, wipe, and return an array of type T[N] - Use mem_alloc_array() */
#define C_ZNEW(N, T) (C_WIPE(C_RNEW(N, T), N, T))

/* DEPRECATED: Allocate, wipe, and return a thing of type T - Use mem_alloc() */
#define ZNEW(T) (WIPE(RNEW(T), T))

/* DEPRECATED: Allocate a wiped array of type T[N], assign to pointer P - Use ptr = mem_alloc_array(N, T) */
#define C_MAKE(P, N, T) ((P) = C_ZNEW(N, T))

/* DEPRECATED: Allocate a wiped thing of type T, assign to pointer P - Use ptr = mem_alloc(T) */
#define MAKE(P, T) ((P) = ZNEW(T))

/* DEPRECATED: Free one thing at P, return NULL - Use mem_free_null() */
#define FREE(P) (rnfree(P))

/* DEPRECATED: Free a thing at location P and set P to NULL - Use mem_free_null() */
#define KILL(P) ((P) = FREE(P))

/**** Available variables ****/

/* Replacement hook for "rnfree()" */
extern void* (*rnfree_aux)(void*);

/* Replacement hook for "rpanic()" */
extern void* (*rpanic_aux)(size_t);

/* Replacement hook for "ralloc()" */
extern void* (*ralloc_aux)(size_t);

/**** Available functions ****/

/* De-allocate memory */
extern void* rnfree(void* p);

/* Panic, attempt to allocate 'len' bytes */
extern void* rpanic(size_t len);

/* Allocate (and return) 'len', or dump core */
extern void* ralloc(size_t len);

/* Create a "dynamic string" */
extern cptr string_make(cptr str);

/* Free a string allocated with "string_make()" */
extern errr string_free(cptr str);

/*
 * Modern C17 typed inline helpers for memory allocation.
 * These provide better type safety than the macro versions while
 * maintaining the same semantics. Use these for new code.
 * 
 * Benefits over macros:
 * - Type safety: Compiler checks pointer types match
 * - Debuggability: Appear in stack traces, can be stepped through
 * - Explicit: Clear return value semantics
 * 
 * Example migration:
 *   Old: C_MAKE(array, count, int);
 *   New: array = mem_alloc_array(count, int);
 */

/**
 * Allocate and zero-initialize an array of count elements of the given type.
 * Returns NULL on failure (calls rpanic which may not return).
 * 
 * @param count Number of elements to allocate
 * @return Pointer to zeroed memory, or NULL on failure
 */
#define mem_alloc_array(count, type) \
    ((type*)C_ZNEW(count, type))

/**
 * Allocate and zero-initialize a single instance of the given type.
 * Returns NULL on failure (calls rpanic which may not return).
 * 
 * @return Pointer to zeroed memory, or NULL on failure
 */
#define mem_alloc(type) \
    ((type*)ZNEW(type))

/**
 * Free memory and return NULL (safe to assign back to pointer).
 * 
 * @param ptr Pointer to free
 * @return Always returns NULL
 */
static inline void* mem_free(void* ptr)
{
    return rnfree(ptr);
}

/**
 * Helper for freeing and NULLing a pointer in one expression.
 * Typical usage: ptr = mem_free_null(ptr);
 * 
 * @param ptr Pointer to free
 * @return Always returns NULL
 */
#define mem_free_null(ptr) ((ptr) = mem_free(ptr))

#endif /* INCLUDED_Z_VIRT_H */
