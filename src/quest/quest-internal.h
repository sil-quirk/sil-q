#ifndef SIL_QUEST_INTERNAL_H
#define SIL_QUEST_INTERNAL_H

#include "angband.h"

int select_tulkas_quest_target(void);
int select_tulkas_quest_prize(int target_level);
u32b get_metarun_quest_flag(int quest_idx);
int get_quest_oath_id(int quest_idx);
cptr get_oath_name_from_id(byte oath_id);
cptr* prepend_repeat_context(
    int quest_idx, cptr* texts, int* count, bool is_completion);
char* my_strstr(const char* haystack, const char* needle);
void display_wrapped_text(
    int col, int* row, cptr text, byte color, int max_width);
void remove_quest_giver_silent(int quest_giver_r_idx);
bool trigger_adjacent_quest_giver_interaction(
    int quest_giver_r_idx, cptr quest_giver_name,
    void (*interaction)(void));
bool ensure_reward_quest_giver_near_player(
    int quest_giver_r_idx, int radius, cptr quest_giver_name,
    cptr arrival_message, int* spawn_y, int* spawn_x);

#endif
