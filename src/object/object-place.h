/* File: object/object-place.h */

#ifndef INCLUDED_OBJECT_PLACE_H
#define INCLUDED_OBJECT_PLACE_H

#include "angband.h"
#include "object-make.h"

s16b floor_carry(int y, int x, object_type* j_ptr);
s16b drop_near(object_type* j_ptr, int chance, int y, int x);
void acquirement(int y1, int x1, int num, drop_quality quality);
void place_object(int y, int x, drop_quality quality, int droptype,
    bool allow_artefacts);
void place_trap(int y, int x);
void reveal_trap(int y, int x);
void place_secret_door(int y, int x);
void place_closed_door(int y, int x);
void place_random_door(int y, int x);
void place_forge(int y, int x);

#endif /* INCLUDED_OBJECT_PLACE_H */
