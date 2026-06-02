/*
 * Legacy metarun format conversions (v0.9.0.0+)
 */
#ifndef METARUN_LEGACY_H
#define METARUN_LEGACY_H

#include "metarun.h"

/* Version structures for backward compatibility
 * Current (0.9.1.2) uses struct metarun in metarun.h
 *  - quest completion counts per quest (capped) stored explicitly
 * Legacy layouts:
 *  v10 = 0.9.0.1 (expanded curse capacity, 64-bit known mask)
 *  v9  = 0.9.0.1 (persistent blessing choices, reserved_runtime[1])
 *  v8  = 0.9.0.0 (initial versioned, no blessing persistence)
 */
typedef struct metarun_v10 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    int8_t curse_stacks[METAR_CURSE_SLOTS];
    u64b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[SAVE_WINDOW_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
    u32b fallen_score_total;
    u32b fallen_score_pool;
    s16b blessing_points;
    u16b blessing_points_spent;
    u16b major_blessings;
    byte alive_characters;
    byte pending_blessing_choices[3];
    byte pending_blessing_count;
    byte blessing_threshold_mode;
    byte reserved_runtime[31];
} metarun_v10;

typedef struct metarun_v9 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    int8_t curse_stacks[32];
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[SAVE_WINDOW_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
    u32b fallen_score_total;
    u32b fallen_score_pool;
    s16b blessing_points;
    u16b blessing_points_spent;
    u16b major_blessings;
    byte alive_characters;
    byte pending_blessing_choices[3];
    byte pending_blessing_count;
    byte blessing_threshold_mode;
    byte reserved_runtime[31];
} metarun_v9;

typedef struct metarun_v8 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    int8_t curse_stacks[32];
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[SAVE_WINDOW_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
    u32b fallen_score_total;
    u32b fallen_score_pool;
    s16b blessing_points;
    u16b blessing_points_spent;
    u16b major_blessings;
    byte alive_characters;
    byte pending_blessing_choices[3];
    byte pending_blessing_count;
    byte reserved_runtime[1];
} metarun_v8;

#define METARUN_V10_SIZE (sizeof(metarun_v10))
#define METARUN_V9_SIZE (sizeof(metarun_v9))
#define METARUN_V8_SIZE (sizeof(metarun_v8))

void metarun_from_v10(metarun *dst, const metarun_v10 *src);
void metarun_from_v9(metarun *dst, const metarun_v9 *src);
void metarun_from_v8(metarun *dst, const metarun_v8 *src);

#endif /* METARUN_LEGACY_H */
