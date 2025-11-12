#ifndef INCLUDED_SCORE_FORMAT_H
#define INCLUDED_SCORE_FORMAT_H

#include "h-basic.h"

/*
 * Binary score database header (Phase 1 scaffold).
 * The header is intentionally versioned independently from the core game
 * version so we can evolve the layout without forcing immediate upgrades.
 */
#define SCORE_DB_MAGIC "SRDB"

typedef struct score_db_header {
    char magic[4];         /* "SRDB" */
    u32b version;          /* format version (0x00010000 for v1) */
    u32b record_count;     /* total run records present */
    u32b character_count;  /* entries in the character database */
    u32b monster_count;    /* entries in the monster statistics table */
    u32b index_table_off;  /* optional offset to per-metarun index */
    u32b reserved[3];      /* room for checksum/flags/future offsets */
} score_db_header;

typedef struct score_guid64 {
    u32b hi;
    u32b lo;
} score_guid64;

typedef enum score_record_status {
    SCORE_RECORD_ALIVE = 0,
    SCORE_RECORD_DEAD = 1,
    SCORE_RECORD_ESCAPED = 2,
} score_record_status;

typedef enum score_killer_kind {
    SCORE_KILLER_MONSTER = 0,
    SCORE_KILLER_TRAP,
    SCORE_KILLER_FALL,
    SCORE_KILLER_SELF,
    SCORE_KILLER_OTHER,
} score_killer_kind;

typedef enum score_run_flag {
    SCORE_RUN_FLAG_MORGOTH_SLAIN = 0x01,
    SCORE_RUN_FLAG_ANGBAND_ESCAPED = 0x02,
    SCORE_RUN_FLAG_NOSCORE = 0x04,
    SCORE_RUN_FLAG_CHEAT = 0x08,
} score_run_flag;

/*
 * Phase 1 run-statistics record. These fields cover the full snapshot of a
 * single dungeon run so downstream systems (scoreboard UI, metarun summaries,
 * analytics) never have to rehydrate data from save files.
 */
typedef struct score_record_v1 {
    u32b record_id;           /* monotonically increasing ID */
    u32b metarun_id;          /* owning metarun (matches meta.raw entry) */
    u32b character_id;        /* foreign key into character database */
    u32b chronological_idx;   /* order of creation within metarun */
    u32b created_utc;         /* when the run started */
    u32b completed_utc;       /* when the run ended */

    score_record_status status;  /* alive/dead/escaped */
    byte run_flags;              /* SCORE_RUN_FLAG_* bits */
    byte race_id;                /* race index at run end */
    byte house_id;               /* house index at run end */

    u16b max_depth;          /* deepest dungeon level reached */
    u16b exit_depth;         /* depth where the run ended */
    u16b silmarils;          /* silmarils carried when exiting/dying */
    u16b uniques_killed;     /* unique monsters defeated */
    u16b quests_completed;   /* quest counter for the run */
    u16b skills_learned;     /* total skills purchased */
    u16b abilities_learned;  /* total abilities purchased */
    s16b net_curses;         /* curses minus blessings */
    s16b house_power;        /* power rating at time of death */

    u32b turns_spent;        /* game turns */
    u32b xp_earned;          /* total XP gained */
    u32b kills_total;        /* total monster kills */
    u32b kills_seen;         /* monsters seen */

    score_guid64 killer_guid;   /* stable GUID from data files */
    score_killer_kind killer_kind;
    u16b killer_race_index;     /* fallback race index */
    u16b cause_code;            /* optional internal identifiers */

    char killer_name[48];       /* final rendered killer string */
    char cause_of_death[64];    /* narrative summary */
    char savefile_hint[32];     /* savefile stem for recovery */
    char reserved[32];          /* padding for future fields */
} score_record_v1;

/*
 * Character database entry: cumulative data for a hero across all runs.
 */
typedef struct score_character_record_v1 {
    u32b character_id;       /* primary key */
    score_guid64 guid;       /* persistent GUID (hash of name + seed) */
    char canonical_name[32]; /* latest spelling of the player name */
    char ancestry[32];       /* race/house string for UI */
    u32b runs_started;
    u32b runs_completed;
    u32b total_score;
    u32b best_score;
    u32b total_turns;
    u16b highest_depth;
    u16b silmarils_recovered;
    u16b deaths;
    u16b escapes;
    byte last_race;
    byte last_house;
    byte reserved[46];
} score_character_record_v1;

/*
 * Monster statistics rollup: tracks relationships between the player base and
 * each monster archetype for richer analytics and lore-driven unlocks.
 */
typedef struct score_monster_stats_v1 {
    score_guid64 guid;       /* stable GUID from monster.txt */
    u16b r_idx;              /* current race index for quick lookups */
    u16b name_offset;        /* optional offset into a string table */
    u32b seen_count;         /* heroes that have seen this monster */
    u32b killed_count;       /* heroes that have killed it */
    u32b death_count;        /* heroes slain by it */
    u32b banished_count;     /* times banished (for uniques) */
    u32b reserved[4];
} score_monster_stats_v1;

#endif /* INCLUDED_SCORE_FORMAT_H */
