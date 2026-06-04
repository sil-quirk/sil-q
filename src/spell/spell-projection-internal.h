/* File: spell/spell-projection-internal.h */

#ifndef INCLUDED_SPELL_PROJECTION_INTERNAL_H
#define INCLUDED_SPELL_PROJECTION_INTERNAL_H

#include "../h-basic.h"

typedef struct object_type object_type;
typedef struct monster_type monster_type;

extern int project_m_n;
extern int project_m_x;
extern int project_m_y;
extern int death_count;

bool project_f(
    int who, int y, int x, int dist, int dd, int ds, int dif, int typ);
bool project_o(int who, int y, int x, int dd, int ds, int dif, int typ);
bool project_m(
    int who, int y, int x, int dd, int ds, int dif, int typ, u32b flg);
bool project_p(int who, int y, int x, int dd, int ds, int dif, int typ);
void sound_dam(int raw_dam, int min_raw, int max_raw, int hp_dam);

#endif /* INCLUDED_SPELL_PROJECTION_INTERNAL_H */
