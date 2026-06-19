#ifndef SIL_PLAYER_UPKEEP_INTERNAL_H
#define SIL_PLAYER_UPKEEP_INTERNAL_H

#include "angband.h"

void calc_hitpoints(void);
void calc_bonuses(void);
void calc_stats(void);
void update_lore(u32b update_flags);
int bow_bonus(const object_type* o_ptr);
int oath_special_ability_from_oath_num(int oath_num);
bool player_has_equipped_flag3(u32b flag3);
bool player_has_inventory_flag3(u32b flag3);

#endif
