#ifndef INCLUDED_GAME_LIFECYCLE_H
#define INCLUDED_GAME_LIFECYCLE_H

#include "h-basic.h"

void do_cmd_escape(int silmarils);
void do_cmd_morgoth_victory(void);
void do_cmd_suicide(void);
void do_cmd_save_game(void);
void close_game(void);
void exit_game_panic(void);

#endif /* INCLUDED_GAME_LIFECYCLE_H */