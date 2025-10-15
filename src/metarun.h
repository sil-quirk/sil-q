/*
 *  metarun.h – public API for the “meta-run” subsystem
 *  ---------------------------------------------------
 *  A *meta-run* is a long-term campaign that spans many individual
 *  characters.  It ends either in victory (15 Silmarils recovered) or
 *  defeat (too many deaths).  This header exposes:
 *
 *      • the metarun data-structure and global instance   (meta)
 *      • load / save helpers and high-level book-keeping
 *      • nibble-packed curse-stack accessors
 *      • gameplay / debug helpers that manipulate curses
 *
 *  All helpers are i386-safe and save-file compatible.
 */
#ifndef METARUN_H
#define METARUN_H

#include "angband.h"          /* basic types (u32b, byte, errr …)      */

/* ------------------------------------------------------------------ */
/*  Win / lose conditions                                             */
/* ------------------------------------------------------------------ */
#define WINCON_SILMARILS 15   /* Recover 15 Silmarils → victory        */
#define LOSECON_DEATHS    15   /* Two deaths end the run  (test value)  */

/* ------------------------------------------------------------------ */
/*  Quest completion tracking                                         */
/* ------------------------------------------------------------------ */
#define METARUN_QUEST_TULKAS   (1UL << 0)   /* Tulkas quest completed */
#define METARUN_QUEST_AULE     (1UL << 1)   /* Aule quest completed   */
#define METARUN_QUEST_MANDOS   (1UL << 2)   /* Mandos quest completed */
#define METARUN_QUEST_NIENA    (1UL << 3)   /* Niena quest completed  */
#define METARUN_QUEST_OROME    (1UL << 4)   /* Oromë quest completed  */
/* Additional quests can be added as (1UL << 5), (1UL << 6), etc.   */

/* ------------------------------------------------------------------ */
/*  Meta-run save-record                                              */
/* ------------------------------------------------------------------ */

/* Version header for meta.raw file */
#define METARUN_FILE_VERSION_MAJOR 0
#define METARUN_FILE_VERSION_MINOR 9
#define METARUN_FILE_VERSION_PATCH 0
#define METARUN_FILE_VERSION_EXTRA 1  /* +1 for persistent blessing choices */

/* Blessing / reward economy */
#define METARUN_BLESSING_POINT_THRESHOLD 300   /* Score required per blessing point */

typedef enum {
    METARUN_MAJOR_EFFECT_NONE = 0,
    METARUN_MAJOR_EFFECT_SUPPLY_LIMIT,
    METARUN_MAJOR_EFFECT_START_ARTIFACT,
} metarun_major_effect;

typedef struct meta_file_header
{
    byte version_major;  /* Major version (0) */
    byte version_minor;  /* Minor version (9) */
    byte version_patch;  /* Patch version (0) */
    byte version_extra;  /* Extra version (0) */
    u32b entry_count;    /* Number of metarun entries in file */
} meta_file_header;

typedef struct metarun
{
    /* ----- per-run counters --------------------------------------- */
    u32b id;            /* monotonic 0-based index                    */
    byte type;          /* reserved for future run-type support       */
    byte deaths;        /* how many characters have died so far       */
    byte silmarils;     /* Silmarils recovered so far                 */
    u32b last_played;   /* time() of the most recent character        */

    u32b score;         /* aggregate campaign score                   */
    u32b best_run_score;/* best individual run score                  */

    int8_t curse_stacks[32]; /* signed stacks: >0 curses, <0 blessings */
    u32b curses_seen;      /* bit i == 1  → curse i is known/revealed */

    /* ----- persistent settings ----------------------------------- */
    u32b persistent_options[8];  /* Persistent options across the metarun */
    byte persistent_delay_factor; /* Persistent delay factor */
    byte persistent_hitpoint_warn; /* Persistent hitpoint warning */
    u32b persistent_window_flags[ANGBAND_TERM_MAX]; /* Persistent window flags */
    byte persistent_options_initialized; /* Flag to track if persistent options are set */

    /* ----- quest completion tracking --------------------------- */
    u32b completed_quests;      /* Bitmask of completed quests (bit 0=Tulkas, bit 1=Aule, etc.) */
    
    /* ----- oath system tracking -------------------------------- */
    byte unlocked_oaths;        /* Bitmask of oaths unlocked this metarun (1=Mercy, 2=Silence, 4=Iron) */
    byte banned_oaths;          /* Bitmask of oaths broken/banned this metarun (cannot select again) */
    byte max_difficulty_reached; /* Maximum difficulty level reached this metarun (cannot go back) */
    
    byte quest_reserved[12];    /* Reserved for future expansion                    */

    /* ----- blessing economy (runtime cached totals) ---------------- */
    u32b fallen_score_total;    /* Total score contributed by fallen heroes        */
    u32b fallen_score_pool;     /* Remainder toward next blessing threshold        */
    s16b blessing_points;       /* Total blessing credits earned (floor division)  */
    u16b blessing_points_spent; /* Credits already spent on blessings              */
    u16b major_blessings;       /* Bitmask of unlocked major blessings             */
    byte alive_characters;      /* Cached count of living heroes in scorefile      */
    
    /* ----- persistent blessing choices (no re-rolling) ------------- */
    byte pending_blessing_choices[3]; /* Currently offered blessing IDs (0-31, 255=empty) */
    byte pending_blessing_count;      /* How many choices are currently pending (0-3)     */
    
    byte reserved_runtime[1];   /* Future use / padding for alignment              */

} metarun;

/* The *current* meta-run – defined once in metarun.c */
extern metarun metar;

/* ------------------------------------------------------------------ */
/*  Disk I/O                                                          */
/* ------------------------------------------------------------------ */
errr load_metaruns(bool create_if_missing);      /* read meta.raw  */
errr save_metaruns(void);                        /* write meta.raw */

/* ------------------------------------------------------------------ */
/*  Book-keeping helpers                                              */
/* ------------------------------------------------------------------ */
void metarun_update_on_exit(bool died,
                            bool escaped,
                            byte new_silmarils,
                            s32b final_score);
/* Call exactly once when a character leaves the dungeon.  Decides if
 * the run ends and persists everything.                              */

void check_run_end(void);                        /* Check win/loss conditions */
void metarun_increment_deaths(void);             /* Shortcut: +1 death      */
void metarun_gain_silmarils(byte n);             /* Shortcut: +n Silmarils  */

void print_metarun_stats(void);                  /* Pretty single-run view  */
void list_metaruns(void);                        /* Full meta-run history   */

/* ------------------------------------------------------------------ */
/*  Quest completion tracking                                         */
/* ------------------------------------------------------------------ */
bool metarun_is_quest_completed(u32b quest_flag);   /* Check if quest is completed */
void metarun_mark_quest_completed(u32b quest_flag); /* Mark quest as completed */
void metarun_check_and_update_quests(void);         /* Check current character quests and update metarun */
void metarun_restore_quest_states(void);            /* Restore quest states from metarun after character load */

/* ------------------------------------------------------------------ */
/*  Oath system tracking                                              */
/* ------------------------------------------------------------------ */
bool oath_unlocked(int oath_id);                    /* Check if oath is unlocked in current metarun */
bool oath_banned(int oath_id);                      /* Check if oath is banned in current metarun */
void metarun_unlock_oath(int oath_id);               /* Unlock oath in current metarun */
void metarun_ban_oath(int oath_id);                  /* Ban oath in current metarun */
int get_available_oaths_mask(void);                 /* Get bitmask of oaths available for selection */

/* ------------------------------------------------------------------ */
/*  Persistent Settings                                               */
/* ------------------------------------------------------------------ */
void metarun_save_persistent_settings(void);     /* Save current options to metarun */
void metarun_load_persistent_settings(void);     /* Load metarun options to current */

void metarun_apply_runtime_effects(void);        /* Sync blessing effects into runtime systems */
bool metarun_has_major_blessing_effect(metarun_major_effect effect);
bool metarun_has_major_blessing_index(int idx);
int  metarun_major_blessing_count(void);
int  metarun_alive_count_cached(void);
u32b compute_blessing_pool(void);               /* Recalculate pool totals (returns total score) */
int  blessing_points_available(void);           /* Unspent blessing credits */

static inline int CURSE_GET(int id)
{
    if (id < 0 || id >= 32) return 0;  /* bounds check */
    return metar.curse_stacks[id];
}

static inline void CURSE_SET(int id, int val)
{
    if (id < 0 || id >= 32) return;    /* bounds check */
    if (val > 127) val = 127;
    if (val < -127) val = -127;
    metar.curse_stacks[id] = (int8_t)val;
}

static inline bool CURSE_SEEN(int id)
{
    if (id < 0 || id >= 32) return false;  // Add bounds check
    return (metar.curses_seen & (1UL << (id & 31))) != 0;
}

static inline void CURSE_SEEN_SET(int id)
{
    if (id < 0 || id >= 32) return;        // Add bounds check
    metar.curses_seen |= (1UL << (id & 31));
}

#define CURSE_ADD(id, d)  CURSE_SET((id), CURSE_GET(id) + (d))

static inline int CURSE_CURSE_STACK(int id)
{
    int v = CURSE_GET(id);
    return (v > 0) ? v : 0;
}

static inline int CURSE_BLESSING_STACK(int id)
{
    int v = CURSE_GET(id);
    return (v < 0) ? -v : 0;
}

/* ------------------------------------------------------------------ */
/*  Public helpers implemented in metarun.c                           */
/* ------------------------------------------------------------------ */
extern bool metarun_created;           /* Flag set when new metarun file created */
void cleanup_old_game_files(void);     /* Clean save/score files on fresh start */
int  menu_choose_one_curse(int n);      /* weighted picker / poem menu  */
int  choose_escape_curses_ui(int n, int out[3]); /* interactive curse selection */
int  choose_oath_breaking_curse_ui(int oath_id); /* oath-specific curse selection with fade */
void metarun_clear_all_curses(void);   /* zero every curse counter     */
void add_curse_stack(int idx);         /* +1 stack respecting caps     */
/* NEW: show a menu of all *known* curses (those with CURSE_SEEN). */
void show_known_curses_menu(void);
void choose_difficulty_level(void);   /* Difficulty selection menu    */

/* Flag-query utilities used throughout the code-base                 */
u32b curse_flag_mask(void);            /* bitmask of active flags      */
int  curse_flag_count_rhf(u32b rhf_flag);  /* #curses with RHF bit  */
int  curse_flag_count_cur(u32b cur_flag);  /* #curses with CUR bit  */
int  any_curse_flag_active(u32b flag);     /* CUR-only helper      */

#endif /* METARUN_H */

