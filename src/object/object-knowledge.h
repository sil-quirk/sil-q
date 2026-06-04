/* File: object/object-knowledge.h */

#ifndef INCLUDED_OBJECT_KNOWLEDGE_H
#define INCLUDED_OBJECT_KNOWLEDGE_H

#include "angband.h"

void object_known(object_type* o_ptr);
void object_aware(object_type* o_ptr);
void object_tried(object_type* o_ptr);
bool object_has_ego_flag4(const object_type* o_ptr, u32b flag);
s32b object_value(const object_type* o_ptr);
bool object_similar(const object_type* o_ptr, const object_type* j_ptr);
void object_absorb(object_type* o_ptr, object_type* j_ptr);
void object_refresh_weight(object_type* o_ptr);

#endif /* INCLUDED_OBJECT_KNOWLEDGE_H */
