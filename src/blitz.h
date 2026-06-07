#ifndef INCLUDED_BLITZ_H
#define INCLUDED_BLITZ_H

#include "angband.h"

typedef enum run_mode {
    RUN_MODE_STORY = 0,
    RUN_MODE_BLITZ = 1,
} run_mode;

typedef enum blitz_character_mode {
    BLITZ_CHARACTER_RANDOM = 0,
    BLITZ_CHARACTER_RANDOM_STATS = 1,
    BLITZ_CHARACTER_SELECTED = 2,
} blitz_character_mode;

typedef enum blitz_effect_mode {
    BLITZ_EFFECT_RANDOM = 0,
    BLITZ_EFFECT_SELECTED = 1,
    BLITZ_EFFECT_SELECTED_DESCR = 2,
} blitz_effect_mode;

typedef struct blitz_setup {
    byte character_mode;
    bool oaths_enabled;
    byte blessing_count;
    byte curse_count;
    byte effect_mode;
} blitz_setup;

void run_mode_reset(void);
void run_mode_set_pending(run_mode mode);
run_mode run_mode_pending(void);
void run_mode_activate_pending(void);
void run_mode_set_current(run_mode mode);
run_mode run_mode_current(void);
bool run_mode_is_blitz(void);

/*
 * Cross-session request to (re)launch into Blitz.  Set when the player starts
 * Blitz from the in-game menu; it survives run_mode_reset() and the
 * between-games re-initialisation so initial_menu() can route the next
 * play_game() straight into Blitz mode.
 */
void blitz_request_launch(void);
bool blitz_launch_requested(void);
void blitz_launch_clear(void);

const char* active_score_filename(void);
bool build_active_score_path(char* buf, size_t len);
void build_active_savefile_stem(const char* base_name, char* out, size_t len);

void blitz_setup_reset(void);
blitz_setup* blitz_current_setup_mutable(void);
const blitz_setup* blitz_current_setup(void);
bool blitz_oaths_enabled(void);
bool blitz_auto_allocates_stats(void);

void blitz_runtime_reset(void);
int8_t* blitz_runtime_curse_stacks(void);
u64b* blitz_runtime_curses_seen(void);
void blitz_runtime_restore(const int8_t* stacks, u64b seen);
void blitz_show_end_summary(byte sil_count);

#endif /* INCLUDED_BLITZ_H */
