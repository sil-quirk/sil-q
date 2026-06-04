/* File: spell/spell-detection.h */

#ifndef INCLUDED_SPELL_DETECTION_H
#define INCLUDED_SPELL_DETECTION_H

#include "../h-basic.h"

void detect_all_doors_traps(void);
bool detect_traps(void);
bool detect_doors(void);
bool detect_stairs(void);
bool detect_objects_normal(int radius);
bool detect_objects_magic(void);
bool detect_monsters(int radius);
bool detect_monsters_invis(void);
bool detect_all(void);

#endif /* INCLUDED_SPELL_DETECTION_H */
