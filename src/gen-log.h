/* gen-log.h - Dedicated generation logging for debugging dungeon generation
 *
 * This creates a separate generation.txt log file with maximum detail
 * about level generation, partition modes, room placement, connectivity,
 * and quest spawning. Enable/disable with GENERATION_LOG_ENABLED.
 */

#ifndef GEN_LOG_H
#define GEN_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

/*
 * Master switch for generation logging.
 * Set to 1 to enable detailed generation.txt output.
 * Set to 0 to disable all generation logging (no performance impact).
 */
#define GENERATION_LOG_ENABLED 1

/*
 * Sub-category switches for fine-grained control.
 * Only effective when GENERATION_LOG_ENABLED is 1.
 */
#define GENLOG_PARTITIONS   1   /* Partition grid, modes, densities */
#define GENLOG_ROOMS        1   /* Room placement attempts and results */
#define GENLOG_ANCHORS      1   /* CA blobs, BSP slices, labyrinths, chasms */
#define GENLOG_CONNECTIVITY 1   /* Tunnel building, connectivity checks */
#define GENLOG_QUESTS       1   /* Quest lottery, spawning, vault placement */
#define GENLOG_STAIRS       1   /* Stair placement and distance calculations */
#define GENLOG_FAILURES     1   /* Regeneration triggers and failure reasons */
#define GENLOG_SUMMARY      1   /* Per-level generation summary */
#define GENLOG_MONSTERS     1   /* Monster placement during generation */

/* File handle for generation log (defined in gen-log.c) */
extern FILE *gen_log_file;
extern bool gen_log_initialized;
extern int gen_log_level_count;  /* Track how many levels generated this session */

/* Initialize generation log file. Call once at startup after exe_path is known. */
void gen_log_init(const char *exe_path);

/* Close generation log file. Called automatically via atexit. */
void gen_log_close(void);

/* Core logging function - writes to generation.txt */
void gen_log_write(const char *category, const char *fmt, ...);

/* Flush the log file (call after each level generation completes) */
void gen_log_flush(void);

/* Log level generation start - resets per-level counters */
void gen_log_level_start(int depth, int map_hgt, int map_wid);

/* Log level generation end with summary */
void gen_log_level_end(bool success, int rooms, int attempts);

/*
 * Convenience macros for categorized logging.
 * Each macro checks both master switch and category switch.
 */

#if GENERATION_LOG_ENABLED && GENLOG_PARTITIONS
#define genlog_partition(...) gen_log_write("PARTITION", __VA_ARGS__)
#else
#define genlog_partition(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_ROOMS
#define genlog_room(...) gen_log_write("ROOM", __VA_ARGS__)
#else
#define genlog_room(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_ANCHORS
#define genlog_anchor(...) gen_log_write("ANCHOR", __VA_ARGS__)
#else
#define genlog_anchor(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_CONNECTIVITY
#define genlog_connect(...) gen_log_write("CONNECT", __VA_ARGS__)
#else
#define genlog_connect(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_QUESTS
#define genlog_quest(...) gen_log_write("QUEST", __VA_ARGS__)
#else
#define genlog_quest(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_STAIRS
#define genlog_stairs(...) gen_log_write("STAIRS", __VA_ARGS__)
#else
#define genlog_stairs(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_FAILURES
#define genlog_fail(...) gen_log_write("FAIL", __VA_ARGS__)
#else
#define genlog_fail(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_SUMMARY
#define genlog_summary(...) gen_log_write("SUMMARY", __VA_ARGS__)
#else
#define genlog_summary(...) ((void)0)
#endif

#if GENERATION_LOG_ENABLED && GENLOG_MONSTERS
#define genlog_monster(...) gen_log_write("MONSTER", __VA_ARGS__)
#else
#define genlog_monster(...) ((void)0)
#endif

/* Generic log for uncategorized messages */
#if GENERATION_LOG_ENABLED
#define genlog(...) gen_log_write("GEN", __VA_ARGS__)
#else
#define genlog(...) ((void)0)
#endif

#endif /* GEN_LOG_H */
