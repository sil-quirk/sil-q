/* Legacy metarun format conversions */
#include "metarun_legacy.h"
#include <string.h>

void metarun_from_v10(metarun *dst, const metarun_v10 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    metarun_clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = src->score;
    dst->best_run_score = src->best_run_score;

    size_t stack_copy = MIN(sizeof(dst->curse_stacks), sizeof(src->curse_stacks));
    memcpy(dst->curse_stacks, src->curse_stacks, stack_copy);
    if (stack_copy < sizeof(dst->curse_stacks)) {
        memset(dst->curse_stacks + stack_copy, 0, sizeof(dst->curse_stacks) - stack_copy);
    }
    dst->curses_seen = src->curses_seen;

    memcpy(dst->persistent_options, src->persistent_options, 8 * sizeof(u32b));
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    memcpy(dst->persistent_window_flags, src->persistent_window_flags,
        sizeof(dst->persistent_window_flags));
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    metarun_seed_quest_counts_from_mask(dst, src->completed_quests);
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;

    memcpy(dst->quest_reserved, src->quest_reserved, 12 * sizeof(byte));

    dst->fallen_score_total = src->fallen_score_total;
    dst->fallen_score_pool = src->fallen_score_pool;
    dst->blessing_points = src->blessing_points;
    dst->blessing_points_spent = src->blessing_points_spent;
    dst->major_blessings = src->major_blessings;
    dst->alive_characters = src->alive_characters;

    memcpy(dst->pending_blessing_choices, src->pending_blessing_choices, 3 * sizeof(byte));
    dst->pending_blessing_count = src->pending_blessing_count;
    dst->blessing_threshold_mode = src->blessing_threshold_mode;

    size_t runtime_copy = MIN(sizeof(dst->reserved_runtime), sizeof(src->reserved_runtime));
    if (runtime_copy > 0) {
        memcpy(dst->reserved_runtime, src->reserved_runtime, runtime_copy);
    }
    if (runtime_copy < sizeof(dst->reserved_runtime)) {
        memset(dst->reserved_runtime + runtime_copy, 0, sizeof(dst->reserved_runtime) - runtime_copy);
    }

    metarun_clamp_and_sync_quests(dst);
    metarun_sanitize_blessing_economy(dst);
}

void metarun_from_v9(metarun *dst, const metarun_v9 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    metarun_clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = src->score;
    dst->best_run_score = src->best_run_score;

    size_t stack_copy = MIN(sizeof(dst->curse_stacks), sizeof(src->curse_stacks));
    memcpy(dst->curse_stacks, src->curse_stacks, stack_copy);
    if (stack_copy < sizeof(dst->curse_stacks)) {
        memset(dst->curse_stacks + stack_copy, 0, sizeof(dst->curse_stacks) - stack_copy);
    }
    dst->curses_seen = (u64b)src->curses_seen;

    memcpy(dst->persistent_options, src->persistent_options, 8 * sizeof(u32b));
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    memcpy(dst->persistent_window_flags, src->persistent_window_flags,
        sizeof(dst->persistent_window_flags));
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    metarun_seed_quest_counts_from_mask(dst, src->completed_quests);
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;

    memcpy(dst->quest_reserved, src->quest_reserved, 12 * sizeof(byte));

    dst->fallen_score_total = src->fallen_score_total;
    dst->fallen_score_pool = src->fallen_score_pool;
    dst->blessing_points = src->blessing_points;
    dst->blessing_points_spent = src->blessing_points_spent;
    dst->major_blessings = src->major_blessings;
    dst->alive_characters = src->alive_characters;

    memcpy(dst->pending_blessing_choices, src->pending_blessing_choices, 3 * sizeof(byte));
    dst->pending_blessing_count = src->pending_blessing_count;
    dst->blessing_threshold_mode = src->blessing_threshold_mode;

    size_t runtime_copy = MIN(sizeof(dst->reserved_runtime), sizeof(src->reserved_runtime));
    if (runtime_copy > 0) {
        memcpy(dst->reserved_runtime, src->reserved_runtime, runtime_copy);
    }
    if (runtime_copy < sizeof(dst->reserved_runtime)) {
        memset(dst->reserved_runtime + runtime_copy, 0, sizeof(dst->reserved_runtime) - runtime_copy);
    }

    metarun_clamp_and_sync_quests(dst);
    metarun_sanitize_blessing_economy(dst);
}

void metarun_from_v8(metarun *dst, const metarun_v8 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    metarun_clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = 0;                 /* score not stored in v8 */
    dst->best_run_score = 0;        /* best_run_score not stored in v8 */

    size_t stack_copy = MIN(sizeof(dst->curse_stacks), sizeof(src->curse_stacks));
    memcpy(dst->curse_stacks, src->curse_stacks, stack_copy);
    if (stack_copy < sizeof(dst->curse_stacks)) {
        memset(dst->curse_stacks + stack_copy, 0, sizeof(dst->curse_stacks) - stack_copy);
    }
    dst->curses_seen = (u64b)src->curses_seen;

    memcpy(dst->persistent_options, src->persistent_options, 8 * sizeof(u32b));
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    memcpy(dst->persistent_window_flags, src->persistent_window_flags,
        sizeof(dst->persistent_window_flags));
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    metarun_seed_quest_counts_from_mask(dst, src->completed_quests);
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;

    /* Copy all quest_reserved bytes */
    memcpy(dst->quest_reserved, src->quest_reserved, 12 * sizeof(byte));

    dst->fallen_score_total = 0;
    dst->fallen_score_pool = 0;
    dst->blessing_points = 0;
    dst->blessing_points_spent = 0;
    dst->major_blessings = 0;
    dst->alive_characters = 0;

    /* No persistent blessing choices in v8 */
    for (int i = 0; i < 3; i++) dst->pending_blessing_choices[i] = 255;
    dst->pending_blessing_count = 0;
    dst->blessing_threshold_mode = METARUN_BLESSING_THRESHOLD_NORMAL;

    memset(dst->reserved_runtime, 0, sizeof(dst->reserved_runtime));

    metarun_clamp_and_sync_quests(dst);
    metarun_sanitize_blessing_economy(dst);
}
