/* File: object/object-util.h */

#ifndef INCLUDED_OBJECT_UTIL_H
#define INCLUDED_OBJECT_UTIL_H

#include "angband.h"

s16b lookup_kind(int tval, int sval);
void object_wipe(object_type* o_ptr);
void object_copy(object_type* o_ptr, const object_type* j_ptr);
byte object_chest_trap_flags(const object_type* o_ptr);
int random_k_idx(void);
void object_prep(object_type* o_ptr, int k_idx);

#endif /* INCLUDED_OBJECT_UTIL_H */
