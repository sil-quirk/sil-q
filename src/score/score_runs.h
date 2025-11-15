#ifndef INCLUDED_SCORE_RUNS_H
#define INCLUDED_SCORE_RUNS_H

#include "h-basic.h"
#include "score/score_format.h"

#include <time.h>

struct high_score;

#define SCORE_RUNS_DB_FILENAME "runs.db"

typedef struct score_run_detail_block {
    score_run_detail_header_v1 header;
    score_run_artefact_v1* artefacts;
    score_run_monster_v1* monsters;
} score_run_detail_block;

bool score_runs_record_current_run(const struct high_score* legacy_score,
                                   time_t timestamp,
                                   score_record_status status);
bool score_runs_load_details(s64b detail_offset, score_run_detail_block* out);
void score_runs_free_details(score_run_detail_block* details);
bool score_runs_snapshot_details(score_run_detail_block* out);

#endif /* INCLUDED_SCORE_RUNS_H */
