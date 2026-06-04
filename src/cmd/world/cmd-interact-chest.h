#ifndef INCLUDED_CMD_INTERACT_CHEST_H
#define INCLUDED_CMD_INTERACT_CHEST_H

#include "angband.h"

s16b chest_check(int y, int x);
void do_cmd_search_skeleton(int y, int x, s16b o_idx);
bool do_cmd_open_chest(int y, int x, s16b o_idx);
bool do_cmd_disarm_chest(int y, int x, s16b o_idx);

#endif /* INCLUDED_CMD_INTERACT_CHEST_H */
