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

/*
 * Legacy z-virt compatibility macros.
 * These provide backward compatibility while using SDL3 functions internally.
 * New code should use mem_alloc/mem_free or SDL functions directly.
 */

/* Size calculations */
#define C_SIZE(N, T) ((N) * (sizeof(T)))
#define SIZE(T) (sizeof(T))

/* Memory operations using standard C functions */
#define C_WIPE(P, N, T) (memset((P), 0, C_SIZE(N, T)))
#define WIPE(P, T) (memset((P), 0, SIZE(T)))
#define C_COPY(P1, P2, N, T) (memcpy((P1), (P2), C_SIZE(N, T)))
#define COPY(P1, P2, T) (memcpy((P1), (P2), SIZE(T)))
#define C_BSET(P, V, N, T) (memset((P), (V), C_SIZE(N, T)))

/* Allocation using SDL3 */
#define C_RNEW(N, T) ((T*)SDL_calloc((N), sizeof(T)))
#define RNEW(T) ((T*)SDL_calloc(1, sizeof(T)))
#define C_ZNEW(N, T) ((T*)SDL_calloc((N), sizeof(T)))
#define ZNEW(T) ((T*)SDL_calloc(1, sizeof(T)))
#define C_MAKE(P, N, T) ((P) = C_ZNEW(N, T))
#define MAKE(P, T) ((P) = ZNEW(T))

/* Deallocation using SDL3 */
#define FREE(P) (SDL_free(P), NULL)
#define KILL(P) ((P) = FREE(P))

#endif /* INCLUDED_MEM_ALLOC_H */
