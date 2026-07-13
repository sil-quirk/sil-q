#ifndef INCLUDED_METARUN_FILES_H
#define INCLUDED_METARUN_FILES_H

#include "h-basic.h"

bool autoload_alive_from_scores(void);
bool autoload_recovery_quit_requested(void);
bool switch_story_scorefile_between_tales(u32b outgoing_id,
    u32b incoming_id, bool create_empty_incoming,
    bool allow_empty_outgoing);
bool begin_story_scorefile_rollover(u32b outgoing_id, u32b incoming_id,
    bool allow_empty_outgoing);
bool finish_story_scorefile_switch(void);
bool story_scorefile_switch_recovery_required(void);
bool restore_story_scorefile_for_tale(u32b tale_id);
bool recover_pending_story_scorefile_switch(u32b selected_tale_id);
bool clear_scorefile(void);
void metarun_finalize_scores_and_saves(void);
void backup_and_clear_saves(void);

#endif /* INCLUDED_METARUN_FILES_H */
