#ifndef INCLUDED_SCORE_ENTRY_H
#define INCLUDED_SCORE_ENTRY_H

#include "h-basic.h"
#include <time.h>

struct high_score;

bool highscore_is_empty(void);
void comma_number(char* output, int number);
void atomonth(int number, char* output);
int silmarils_possessed(void);
int has_iron_crown(void);
errr create_score(struct high_score* the_score);
errr score_entry_enter(struct high_score* the_score);
bool score_entry_has_committed_current(void);
void score_entry_set_death_time(time_t death_time);
time_t score_entry_death_time(void);
bool build_live_preview_score(struct high_score* out);
bool mobile_autosave_game(cptr reason);
const char* kinslayer_try_kill(uint8_t n_sils, bool do_roll);

#endif /* INCLUDED_SCORE_ENTRY_H */