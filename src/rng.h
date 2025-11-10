/* File: rng.h */

/*
 * Modern RNG module backed by SDL3
 * 
 * This replaces the legacy z-rand.c/z-rand.h with SDL3-backed random
 * number generation while maintaining API compatibility for gameplay.
 * 
 * Uses SDL_RandomContext for proper cross-platform random generation
 * with deterministic seeding for save/load compatibility.
 */

#ifndef INCLUDED_RNG_H
#define INCLUDED_RNG_H

#include "h-basic.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

/**** RNG State ****/

/* Use the "simple" LCRNG for quick operations */
extern bool Rand_quick;

/* Current "value" of the "simple" RNG */
extern u32b Rand_value;

/* Current "index" for the "complex" RNG */
extern u16b Rand_place;

/* Current "state" table for the "complex" RNG (63 entries) */
#define RAND_DEG 63
extern u32b Rand_state[RAND_DEG];

/**** Core RNG Functions ****/

/**
 * Initialize the complex RNG with a seed value.
 * This must be called during game startup and when loading saves.
 * 
 * @param seed The seed value for deterministic generation
 */
extern void Rand_state_init(u32b seed);

/**
 * Generate a random number from 0 to m-1.
 * Uses either the simple or complex RNG depending on Rand_quick flag.
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
extern u32b Rand_simple(u32b m);

/**
 * Divide with rounding (up for positive, down for negative).
 * 
 * @param n Numerator
 * @param d Denominator (must not be 0)
 * @return Rounded division result
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
