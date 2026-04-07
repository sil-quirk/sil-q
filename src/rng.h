/* File: rng.h */

#ifndef INCLUDED_RNG_H
#define INCLUDED_RNG_H

#include "h-basic.h"
#include <stdbool.h>

/**** Core RNG Functions ****/

/**
 * Initialize the complex RNG with a seed value.
 * This must be called during game startup and when loading saves.
 * 
 * @param seed The seed value for deterministic generation
 */
extern void Rand_state_init(u64b seed);

/**
 * Export/import helpers so callers (savefiles, deterministic helpers) can
 * capture and restore the exact RNG state.
 * 
 * State Format:
 * - 64-bit unsigned integer representing the internal PRNG state
 * - Zero values are automatically sanitized to prevent degenerate sequences
 * - Export/import are symmetrical: import(export()) preserves state
 */
extern u64b Rand_state_export(void);
extern void Rand_state_import(u64b state);

/**
 * Push/pop helpers for temporary RNG state changes.
 * Useful for deterministic preview calculations without affecting gameplay RNG.
 * 
 * Example:
 *   u64b saved = Rand_state_push(fixed_seed);  // Switch to deterministic mode
 *   ... perform calculations ...
 *   Rand_state_pop(saved);                      // Restore original state
 * 
 * @param new_state Seed value to switch to (push), or saved state to restore (pop)
 * @return Previous state value (push only)
 */
extern u64b Rand_state_push(u64b new_state);
extern void Rand_state_pop(u64b saved_state);

/**
 * Generate a random number from 0 to m-1.
 * 
 * @param m Upper bound (exclusive), must be > 0
 * @return Random value in range [0, m-1]
 */
extern u32b Rand_div(u32b m);

/**
 * Generate a random number with normal distribution.
 * 
 * @param mean Center of the distribution
 * @param stand Standard deviation (must be >= 1)
 * @return Random value with normal distribution
 */
extern s16b Rand_normal(int mean, int stand);

/**
 * Generate a random number using a separate RNG state.
 * Used for UI/external operations that shouldn't affect gameplay RNG.
 * 
 * @param m Upper bound (exclusive)
 * @return Random value in range [0, m-1]
 */
extern s32b div_round(s32b n, s32b d);

/**** Convenience Macros ****/

/* Generate random integer X where 0 <= X < M */
#define rand_int(M) ((s32b)(Rand_div(M)))

/* Generate random integer X where 1 <= X <= M */
#define dieroll(M) (rand_int(M) + 1)
#define rand_die(M) (rand_int(M) + 1)

/* Generate random integer X where A <= X <= B */
#define rand_range(A, B) ((A) + (rand_int(1 + (B) - (A))))

/* Generate random integer X where A-D <= X <= A+D */
#define rand_spread(A, D) ((A) + (rand_int(1 + (D) + (D))) - (D))

/* True with 1 in X chance (safe for X < 1) */
#define one_in_(X) (rand_int(X > 0 ? X : 1) == 0)

/* True with X percent chance */
#define percent_chance(X) (rand_int(100) < X)

#endif /* INCLUDED_RNG_H */
