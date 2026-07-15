#ifndef INCLUDED_DUNGEON_INTERNAL_H
#define INCLUDED_DUNGEON_INTERNAL_H

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "score/score_runs.h"
#include "score/score_ui.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include "z-term.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Profiling is deliberately zero-cost in normal builds.  Some wrapped scopes
 * wait for player input, so timing them as active turn work produces false
 * warnings and synchronous log flushes during ordinary play. */
#define TIME_PHASE(label, stmt) do { (void)(label); stmt; } while (0)

extern int last_player_y;
extern int last_player_x;
extern bool was_in_morgoth_vault;
extern bool morgoth_entry_preconfirmed;
extern int last_music_depth;
extern bool first_entry_to_dungeon;
extern bool death_spectator_mode;
extern bool death_spectator_exit_requested;
extern char g_active_partition_banner_text[1024];
extern bool g_active_partition_banner_consumes_input;
extern bool g_active_partition_banner_skip_next_decay;
extern char greater_vault_xp_name[80];
extern bool greater_vault_xp_awarded;

void play_game_pop_startup_icky(bool* startup_icky_active, cptr reason);
bool restore_player_position_after_denied_move(int y, int x);
void snapshot_run_history(const char* reason);
void reset_level_entry_tracking(void);
void display_partition_narrative_banner(int old_sidx, int new_sidx,
    level_partition_kind kind, bool line_delay);
void describe_greater_vault_entry(cptr vault_name);
void morgoth_prompt_controller_label(int binding, const char* fallback,
    char* buf, size_t buflen);
bool confirm_enter_morgoth_hall(void);
void update_labyrinth_view_state(bool handle_now);
void handle_partition_entry(bool force_message, int narrative_mode);
void regenhp(int regen_multiplier);
void regenmana(int regen_multiplier);
void scan_artifacts_near_player(void);
void process_world(void);
bool enter_wizard_mode(void);
void process_command(void);
bool death_spectator_command_allowed(int command);
void process_player(void);
void dungeon(void);
void print_story_intro(void);

#endif /* INCLUDED_DUNGEON_INTERNAL_H */
