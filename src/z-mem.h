/* File: z-mem.h */

/*
 * Modern memory management interface for Sil-QH.
 * 
 * This replaces the legacy z-virt.h macro system with a clean C17 interface.
 * All allocation still goes through ralloc/rnfree for memory tracking.
 */

#ifndef INCLUDED_Z_MEM_H
#define INCLUDED_Z_MEM_H

#include "h-basic.h"

/**** Core allocation functions ****/

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

/**** Replacement hooks (optional) ****/

/* Replacement hook for "rnfree()" */
extern void* (*rnfree_aux)(void*);

/* Replacement hook for "rpanic()" */
extern void* (*rpanic_aux)(size_t);

/* Replacement hook for "ralloc()" */
extern void* (*ralloc_aux)(size_t);

/**** Modern C17 memory interface ****/

/**
 * Allocate and zero-initialize an array of count elements of the given type.
 * 
 * @param count Number of elements to allocate
 * @param type Type of each element
 * @return Pointer to zeroed memory, or calls rpanic on failure
 * 
 * Example: int *array = mem_alloc_array(100, int);
 */
#define mem_alloc_array(count, type) \
    ((type*)memset(ralloc((count) * sizeof(type)), 0, (count) * sizeof(type)))

/**
 * Allocate and zero-initialize a single instance of the given type.
 * 
 * @param type Type to allocate
 * @return Pointer to zeroed memory, or calls rpanic on failure
 * 
 * Example: struct foo *ptr = mem_alloc(struct foo);
 */
#define mem_alloc(type) \
    ((type*)memset(ralloc(sizeof(type)), 0, sizeof(type)))

/**
 * Free memory and return NULL (safe to assign back to pointer).
 * 
 * @param ptr Pointer to free
 * @return Always returns NULL
 * 
 * This is designed to be used in assignments: ptr = mem_free(ptr);
 */
static inline void* mem_free(void* ptr)
{
    return rnfree(ptr);
}

/**
 * Free memory and NULL the pointer in one expression.
 * 
 * @param ptr Pointer variable to free and NULL
 * 
 * Example: mem_free_null(my_array);
 */
#define mem_free_null(ptr) ((ptr) = mem_free(ptr))

#endif /* INCLUDED_Z_MEM_H */
