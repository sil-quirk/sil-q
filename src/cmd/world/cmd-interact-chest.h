#ifndef INCLUDED_CMD_INTERACT_CHEST_H
#define INCLUDED_CMD_INTERACT_CHEST_H

#include "angband.h"

s16b chest_check(int y, int x);
bool chest_trap_presence_known(const object_type* o_ptr);
bool chest_trap_fully_known(const object_type* o_ptr);
bool chest_minigame_retry_target(int* y, int* x);
void chest_minigame_clear_retry(void);
void do_cmd_search_skeleton(int y, int x, s16b o_idx);
bool do_cmd_open_chest(int y, int x, s16b o_idx);
bool do_cmd_disarm_chest(int y, int x, s16b o_idx);

#endif /* INCLUDED_CMD_INTERACT_CHEST_H */
