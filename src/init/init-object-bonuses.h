#ifndef INCLUDED_INIT_OBJECT_BONUSES_H
#define INCLUDED_INIT_OBJECT_BONUSES_H

#include "angband.h"

void apply_default_pval_bonuses(u32b flags1, s16b pval,
    s16b stat_bonus[A_MAX], const bool stat_bonus_set[A_MAX],
    s16b skill_bonus[S_MAX], const bool skill_bonus_set[S_MAX]);
bool apply_obj_bonus_token(const char* token, int value,
    u32b* flags1,
    s16b stat_bonus[A_MAX], bool stat_bonus_set[A_MAX],
    s16b skill_bonus[S_MAX], bool skill_bonus_set[S_MAX]);
bool parse_bonus_value_range(char* text, int* min_value, int* max_value);
bool apply_ego_bonus_token_range(const char* token, int min_value, int max_value,
    u32b* flags1,
    s16b stat_bonus_min[A_MAX], s16b stat_bonus[A_MAX], bool stat_bonus_set[A_MAX],
    s16b skill_bonus_min[S_MAX], s16b skill_bonus[S_MAX], bool skill_bonus_set[S_MAX]);

#endif /* INCLUDED_INIT_OBJECT_BONUSES_H */