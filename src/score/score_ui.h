#ifndef INCLUDED_SCORE_UI_H
#define INCLUDED_SCORE_UI_H

#include "h-basic.h"

struct high_score;

void display_scores(void);
bool build_live_preview_score(struct high_score* out);
void show_scores_interactive(void);
void show_scores_interactive_highlight(const struct high_score* entry);
void show_scores_interactive_highlight_from_file(const char* filepath,
                                                 const struct high_score* entry);
void do_cmd_run_history(void);

#endif /* INCLUDED_SCORE_UI_H */
