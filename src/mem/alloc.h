/* mem/alloc.h - Modern memory allocation interface */

#ifndef INCLUDED_MEM_ALLOC_H
#define INCLUDED_MEM_ALLOC_H

#include "../h-basic.h"
#include <SDL3/SDL.h>
#include <string.h>

/*
 * Modern type-safe memory allocation wrappers.
 * These replace the legacy z-virt.h macro-based system with:
 * - Better type safety (compiler checks types)
 * - Clearer semantics (explicit return values)
 * - Standard behavior (uses SDL_calloc/SDL_free)
 * - Better debugging (inline functions appear in stack traces)
 */

/**
 * Allocate and zero-initialize an array of elements.
 * 
 * @param count Number of elements
 * @param type Element type
 * @return Pointer to zeroed memory, or NULL on failure
 * 
 * Example:
 *   int* array = mem_alloc_array(100, int);
 *   if (!array) { handle_error(); }
 */
#define mem_alloc_array(count, type) \
    ((type*)SDL_calloc((count), sizeof(type)))

/**
 * Allocate and zero-initialize a single object.
 * 
 * @param type Object type
 * @return Pointer to zeroed memory, or NULL on failure
 * 
 * Example:
 *   monster_type* mon = mem_alloc(monster_type);
 *   if (!mon) { handle_error(); }
 */
#define mem_alloc(type) \
    ((type*)SDL_calloc(1, sizeof(type)))

/**
 * Free memory and return NULL.
 * Safe to call on NULL pointers.
 * 
 * @param ptr Pointer to free
 * @return Always returns NULL
 * 
 * Example:
 *   ptr = mem_free(ptr);  // Frees and NULLs in one step
 */
static inline void* mem_free(void* ptr)
{
    if (ptr) SDL_free(ptr);
    return NULL;
}

/**
 * Macro helper for freeing and NULLing a pointer.
 * Equivalent to: ptr = mem_free(ptr);
 * 
 * Example:
 *   mem_free_null(array);  // array is now NULL
 */
#define mem_free_null(ptr) ((ptr) = mem_free(ptr))

#endif /* INCLUDED_MEM_ALLOC_H */
