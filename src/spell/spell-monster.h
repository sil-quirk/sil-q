/* File: spell/spell-monster.h */

#ifndef INCLUDED_SPELL_MONSTER_H
#define INCLUDED_SPELL_MONSTER_H

#include "../h-basic.h"

bool slow_monsters(int power);
bool sleep_monsters(int power);
void wake_all_monsters(int who);
bool make_aggressive(void);
bool banishment(void);
bool mass_banishment(void);

#endif /* INCLUDED_SPELL_MONSTER_H */
