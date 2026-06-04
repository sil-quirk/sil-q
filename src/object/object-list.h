/* File: object/object-list.h */

#ifndef INCLUDED_OBJECT_LIST_H
#define INCLUDED_OBJECT_LIST_H

#include "angband.h"

void excise_object_idx(int o_idx);
void delete_object_idx(int o_idx);
void delete_object(int y, int x);
void compact_objects(int size);
void wipe_o_list(void);
s16b o_pop(void);
object_type* get_first_object(int y, int x);
object_type* get_next_object(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_LIST_H */
