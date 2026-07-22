/* File: object/object-use.h */

#ifndef INCLUDED_OBJECT_USE_H
#define INCLUDED_OBJECT_USE_H

#include "angband.h"

int get_paired_artefact(int art_idx);
bool player_can_treat_as_throwing_flags(const object_type* o_ptr, u32b f3);
bool weapon_is_impale_eligible(const object_type* o_ptr);
bool player_can_treat_as_throwing(const object_type* o_ptr);
bool potion_has_thrown_effect(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_USE_H */
