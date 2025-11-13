#ifndef INCLUDED_SCORE_RUNS_H
#define INCLUDED_SCORE_RUNS_H

#include "h-basic.h"
#include "score/score_format.h"

#include <time.h>

struct high_score;

bool score_runs_record_current_run(const struct high_score* legacy_score,
                                   time_t timestamp,
                                   score_record_status status);

#endif /* INCLUDED_SCORE_RUNS_H */
