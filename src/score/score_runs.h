#ifndef INCLUDED_SCORE_RUNS_H
#define INCLUDED_SCORE_RUNS_H

#include "h-basic.h"
#include "score/score_format.h"

#include <time.h>

struct high_score;
typedef struct SDL_IOStream SDL_IOStream;

#define SCORE_RUNS_DB_FILENAME "runs.db"

typedef struct score_run_detail_block {
    score_run_detail_header_v1 header;
    score_run_stat_v1* stats;
    score_run_skill_v1* skills;
    score_run_ability_v1* abilities;
    score_run_milestone_v1* milestones;
    score_run_artefact_v1* artefacts;
    score_run_monster_v1* monsters;
    u16b stats_count;
    u16b skills_count;
    u16b ability_count;
    u16b milestone_count;
} score_run_detail_block;

#define SCORE_RUNS_METARUN_UNKNOWN 0xFFFFFFFFu

bool score_runs_record_current_run(const struct high_score* legacy_score,
                                   time_t timestamp,
                                   score_record_status status);
bool score_runs_record_current_run_with_id(const struct high_score* legacy_score,
                                           time_t timestamp,
                                           score_record_status status,
                                           u32b* out_record_id);
void score_runs_set_legacy_link(struct high_score* legacy_score,
                                u32b record_id);
bool score_runs_get_legacy_link(const struct high_score* legacy_score,
                                u32b* out_record_id);
bool score_runs_load_details(s64b detail_offset, score_run_detail_block* out);
void score_runs_free_details(score_run_detail_block* details);
bool score_runs_snapshot_details(score_run_detail_block* out);
bool score_runs_skip_detail_payload(SDL_IOStream* file,
                                    const score_run_detail_header_v1* header);

#endif /* INCLUDED_SCORE_RUNS_H */
