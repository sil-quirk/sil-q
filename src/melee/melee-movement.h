#ifndef INCLUDED_MELEE_MOVEMENT_H
#define INCLUDED_MELEE_MOVEMENT_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

extern bool monster_can_smell(monster_type* m_ptr);
extern bool get_move_wander(monster_type* m_ptr, int* ty, int* tx);
extern bool get_move(
    monster_type* m_ptr, int* ty, int* tx, bool* fear, bool must_use_target);
extern bool make_move(
    monster_type* m_ptr, int* ty, int* tx, bool fear, bool* bash);
extern void process_move(monster_type* m_ptr, int ty, int tx, bool bash);
extern void warning_message(monster_type* m_ptr);
extern void monster_exchange_places(monster_type* m_ptr);
extern int calc_hesitance(monster_type* m_ptr);

#endif /* INCLUDED_MELEE_MOVEMENT_H */
