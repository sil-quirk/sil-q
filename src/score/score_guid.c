#include "score/score_guid.h"

#include "rng.h"

guid64 score_guid_from_u64(u64b value)
{
    guid64 guid;
    guid.hi = (u32b)(value >> 32);
    guid.lo = (u32b)(value & 0xFFFFFFFFu);
    return guid;
}

guid64 score_guid_from_string(const char* text, u32b salt)
{
    const u64b offset = UINT64_C(1469598103934665603);
    const u64b prime = UINT64_C(1099511628211);
    u64b hash = offset ^ salt;

    if (text) {
        while (*text) {
            hash ^= (unsigned char)*text++;
            hash *= prime;
        }
    }

    return score_guid_from_u64(hash);
}

static u32b score_guid_rand32(void)
{
    u32b hi = Rand_div(0x10000);
    u32b lo = Rand_div(0x10000);
    return (hi << 16) | lo;
}

guid64 score_guid_random(void)
{
    guid64 guid;
    guid.hi = score_guid_rand32();
    guid.lo = score_guid_rand32();
    if (guid.hi == 0 && guid.lo == 0) {
        guid.lo = 1;
    }
    return guid;
}

bool score_guid_is_zero(const guid64* guid)
{
    return !guid || (guid->hi == 0 && guid->lo == 0);
}

void score_guid_clear(guid64* guid)
{
    if (!guid)
        return;
    guid->hi = 0;
    guid->lo = 0;
}
