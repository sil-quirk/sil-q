/* File: object/object-flags.h */

#ifndef INCLUDED_OBJECT_FLAGS_H
#define INCLUDED_OBJECT_FLAGS_H

#include "angband.h"

void object_flags(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
void object_flags4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3,
    u32b* f4);
void object_flags_known(const object_type* o_ptr, u32b* f1, u32b* f2,
    u32b* f3);
void object_flags_known4(const object_type* o_ptr, u32b* f1, u32b* f2,
    u32b* f3, u32b* f4);
bool object_grants_ability(const object_type* o_ptr, int skilltype,
    int abilitynum);

#endif /* INCLUDED_OBJECT_FLAGS_H */
