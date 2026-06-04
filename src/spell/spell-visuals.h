/* File: spell/spell-visuals.h */

#ifndef INCLUDED_SPELL_VISUALS_H
#define INCLUDED_SPELL_VISUALS_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

void stun_monster(monster_type* m_ptr, int stun);
u16b bolt_pict(int y, int x, int ny, int nx, int typ);

#endif /* INCLUDED_SPELL_VISUALS_H */
