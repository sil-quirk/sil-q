#ifndef INCLUDED_MELEE_PROCESS_H
#define INCLUDED_MELEE_PROCESS_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

/* Terrified monsters turn to fight inside this range if they cannot outrun the player. */
#define TURN_RANGE 3

extern int challenge_check(monster_type* m_ptr);
extern void find_range(monster_type* m_ptr);
extern void wander(monster_type* m_ptr);
extern int get_chance_of_ranged_attack(monster_type* m_ptr);
extern void produce_cloud(monster_type* m_ptr);
extern int morale_from_friends(monster_type* m_ptr);
extern void calc_morale(monster_type* m_ptr);
extern void calc_stance(monster_type* m_ptr);
extern void process_monsters(s16b minimum_energy);
extern void monster_perception(
    bool player_centered, bool main_roll, int difficulty);

#endif /* INCLUDED_MELEE_PROCESS_H */
