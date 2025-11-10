/* File: rng.c */

#include "rng.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>
#include <time.h>

static Uint64 rng_state = 0;

static Uint64 sanitize_seed(Uint64 seed)
{
    if (seed == 0)
    {
        seed = ((Uint64)time(NULL) << 32) ^ SDL_GetPerformanceCounter() ^ 0x9E3779B97F4A7C15ULL;
    }
    return seed;
}

void Rand_state_init(u64b seed) { rng_state = sanitize_seed(seed); }

u64b Rand_state_export(void) { return rng_state; }

void Rand_state_import(u64b state) { rng_state = sanitize_seed(state); }

static Uint32 rng_random_bits(void) { return SDL_rand_bits_r(&rng_state); }

u32b Rand_div(u32b m)
{
    if (m <= 1)
        return 0;

    Uint32 threshold = UINT32_MAX - (UINT32_MAX % m);
    Uint32 value;

    do
    {
        value = rng_random_bits();
    } while (value >= threshold);

    return value % m;
}

/*
 * The number of entries in the "Rand_normal_table"
 */
#define RANDNOR_NUM 256

/*
 * The standard deviation of the "Rand_normal_table"
 */
#define RANDNOR_STD 64

/*
 * The normal distribution table for the "Rand_normal()" function (below)
 */
static s16b Rand_normal_table[RANDNOR_NUM] = {
    206,   613,   1022,  1430,  1838,  2245,  2652,  3058,  3463,  3867,
    4271,  4673,  5075,  5475,  5874,  6271,  6667,  7061,  7454,  7845,
    8234,  8621,  9006,  9389,  9770,  10148, 10524, 10898, 11269, 11638,
    12004, 12367, 12727, 13085, 13440, 13792, 14140, 14486, 14828, 15168,
    15504, 15836, 16166, 16492, 16814, 17133, 17449, 17761, 18069, 18374,
    18675, 18972, 19266, 19556, 19842, 20124, 20403, 20678, 20949, 21216,
    21479, 21738, 21994, 22245, 22493, 22737, 22977, 23213, 23446, 23674,
    23899, 24120, 24336, 24550, 24759, 24965, 25166, 25365, 25559, 25750,
    25937, 26120, 26300, 26476, 26649, 26818, 26983, 27146, 27304, 27460,
    27612, 27760, 27906, 28048, 28187, 28323, 28455, 28585, 28711, 28835,
    28955, 29073, 29188, 29299, 29409, 29515, 29619, 29720, 29818, 29914,
    30007, 30098, 30186, 30272, 30356, 30437, 30516, 30593, 30668, 30740,
    30810, 30879, 30945, 31010, 31072, 31133, 31192, 31249, 31304, 31358,
    31410, 31460, 31509, 31556, 31601, 31646, 31688, 31730, 31770, 31808,
    31846, 31882, 31917, 31950, 31983, 32014, 32044, 32074, 32102, 32129,
    32155, 32180, 32205, 32228, 32251, 32273, 32294, 32314, 32333, 32352,
    32370, 32387, 32404, 32420, 32435, 32450, 32464, 32477, 32490, 32503,
    32515, 32526, 32537, 32548, 32558, 32568, 32577, 32586, 32595, 32603,
    32611, 32618, 32625, 32632, 32639, 32645, 32651, 32657, 32662, 32667,
    32672, 32677, 32682, 32686, 32690, 32694, 32698, 32702, 32705, 32708,
    32711, 32714, 32717, 32720, 32722, 32725, 32727, 32729, 32731, 32733,
    32735, 32737, 32739, 32740, 32742, 32743, 32745, 32746, 32747, 32748,
    32749, 32750, 32751, 32752, 32753, 32754, 32755, 32756, 32757, 32757,
    32758, 32758, 32759, 32760, 32760, 32761, 32761, 32761, 32762, 32762,
    32763, 32763, 32763, 32764, 32764, 32764, 32764, 32765, 32765, 32765,
    32765, 32766, 32766, 32766, 32766, 32767,
};

s16b Rand_normal(int mean, int stand)
{
    s16b tmp;
    s16b offset;

    s16b low = 0;
    s16b high = RANDNOR_NUM;

    /* Paranoia */
    if (stand < 1)
        return mean;

    /* Roll for probability */
    tmp = (s16b)rand_int(32768);

    /* Binary Search */
    while (low < high)
    {
        int mid = (low + high) >> 1;

        if (Rand_normal_table[mid] < tmp)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    offset = (long)stand * (long)low / RANDNOR_STD;

    if (one_in_(2))
        return (mean - offset);

    return (mean + offset);
}

s32b div_round(s32b n, s32b d)
{
    s32b tmp;

    if (!d)
        return n;

    tmp = n / d;

    if ((ABS(n) % ABS(d)) * 2 >= d)
    {
        if (n * d > 0L)
            tmp += 1L;
        else
            tmp -= 1L;
    }

    return tmp;
}
