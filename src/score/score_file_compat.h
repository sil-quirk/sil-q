#ifndef INCLUDED_SCORE_FILE_COMPAT_H
#define INCLUDED_SCORE_FILE_COMPAT_H

#include "score/score_io.h"

#define highscore_fd (score_file_active_ctx()->fd)
#define scores_file_entry_count (score_file_active_ctx()->entry_count)
#define scores_file_version_major (score_file_active_ctx()->version_major)
#define scores_file_version_minor (score_file_active_ctx()->version_minor)
#define scores_file_version_patch (score_file_active_ctx()->version_patch)
#define scores_file_version_extra (score_file_active_ctx()->version_extra)

#endif /* INCLUDED_SCORE_FILE_COMPAT_H */