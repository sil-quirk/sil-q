#ifndef INCLUDED_MELEE_MOVEMENT_INTERNAL_H
#define INCLUDED_MELEE_MOVEMENT_INTERNAL_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

bool get_move_retreat(monster_type* m_ptr, int* ty, int* tx);
void get_move_advance(monster_type* m_ptr, int* ty, int* tx);
int calc_vulnerability(int fy, int fx);
bool get_route_to_target(monster_type* m_ptr, int* ty, int* tx);
bool push_aside(monster_type* m_ptr, monster_type* n_ptr);

#endif /* INCLUDED_MELEE_MOVEMENT_INTERNAL_H */
