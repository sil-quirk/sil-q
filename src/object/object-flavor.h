/* File: object/object-flavor.h */

#ifndef INCLUDED_OBJECT_FLAVOR_H
#define INCLUDED_OBJECT_FLAVOR_H

#include "angband.h"

bool easter_time(void);
void flavor_init(void);
void reset_visuals(bool prefs);
byte object_display_color(const object_type* o_ptr, byte base_color);

#endif /* INCLUDED_OBJECT_FLAVOR_H */
