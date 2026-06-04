/* File: object/object-desc.h */

#ifndef INCLUDED_OBJECT_DESC_H
#define INCLUDED_OBJECT_DESC_H

#include "angband.h"

void strip_name(char* buf, int k_idx);
void object_desc(char* buf, size_t max, const object_type* o_ptr, int pref,
    int mode);
void object_desc_floor(char* buf, size_t max, const object_type* o_ptr,
    int pref, int mode);
void object_desc_spoil(char* buf, size_t max, const object_type* o_ptr,
    int pref, int mode);
void identify_random_gen(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_DESC_H */
