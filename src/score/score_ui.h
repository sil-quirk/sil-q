#ifndef INCLUDED_SCORE_UI_H
#define INCLUDED_SCORE_UI_H

#include "h-basic.h"

struct high_score;

void display_single_score(byte attr, int row, int col, int place, int fake,
                          struct high_score* the_score);
void display_scores(int from, int to);
void display_scores_short(int from, int to);
bool build_live_preview_score(struct high_score* out);
void show_scores(bool longscore);
void show_scores_interactive(bool longscore);
void show_scores_interactive_highlight(bool longscore,
                                       const struct high_score* entry);
void do_cmd_run_history(void);

#endif /* INCLUDED_SCORE_UI_H */
