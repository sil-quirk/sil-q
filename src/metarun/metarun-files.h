#ifndef INCLUDED_METARUN_FILES_H
#define INCLUDED_METARUN_FILES_H

#include "h-basic.h"

bool autoload_alive_from_scores(void);
void clear_scorefile(void);
void metarun_finalize_scores_and_saves(void);
void backup_and_clear_saves(void);

#endif /* INCLUDED_METARUN_FILES_H */