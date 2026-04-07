#ifndef INCLUDED_SCORE_GUID_H
#define INCLUDED_SCORE_GUID_H

#include "h-basic.h"

guid64 score_guid_from_u64(u64b value);
guid64 score_guid_from_string(const char* text, u32b salt);
guid64 score_guid_random(void);
bool score_guid_is_zero(const guid64* guid);
void score_guid_clear(guid64* guid);

#endif /* INCLUDED_SCORE_GUID_H */
