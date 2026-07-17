/*
 *  metarun.h - public API for the "meta-run" subsystem
 *  ---------------------------------------------------
 *  A *meta-run* is a long-term campaign that spans many individual
 *  characters.  It ends either in victory (15 Silmarils recovered) or
 *  defeat (too many deaths).  This header exposes:
 *
 *      - the metarun data-structure and global instance   (meta)
 *      - load / save helpers and high-level book-keeping
 *      - nibble-packed curse-stack accessors
 *      - gameplay / debug helpers that manipulate curses
 *
 *  All helpers are i386-safe and save-file compatible.
 */
#ifndef METARUN_H
#define METARUN_H

#include "angband.h"          /* basic types (u32b, byte, errr ...)      */

extern curse_type* cu_info;

/* ------------------------------------------------------------------ */
/*  Win / lose conditions                                             */
/* ------------------------------------------------------------------ */
#define WINCON_SILMARILS 15   /* Recover 15 Silmarils -> victory        */
#define LOSECON_DEATHS    15   /* Two deaths end the run  (test value)  */

/* ------------------------------------------------------------------ */
/*  Quest completion tracking                                         */
/* ------------------------------------------------------------------ */
#define METARUN_QUEST_TULKAS   (1UL << 0)   /* Tulkas quest completed */
#define METARUN_QUEST_AULE     (1UL << 1)   /* Aulë quest completed   */
#define METARUN_QUEST_MANDOS   (1UL << 2)   /* Mandos quest completed */
#define METARUN_QUEST_NIENA    (1UL << 3)   /* Nienna quest completed  */
#define METARUN_QUEST_OROME    (1UL << 4)   /* Oromë quest completed  */
#define METARUN_QUEST_VARDA    (1UL << 5)   /* Varda quest completed  */
#define METARUN_QUEST_SLOT_MAX 8            /* Max quest slots tracked in metarun */
#define METARUN_QUEST_COMPLETION_CAP 7      /* Max times a quest counts per metarun */
/* Additional quests can be added as (1UL << 5), (1UL << 6), etc.   */

/* ------------------------------------------------------------------ */
/*  Meta-run save-record                                              */
/* ------------------------------------------------------------------ */

/* Version history (tied to game VERSION_* from defines.h):
 *   0.9.0.0 - Initial versioned format (quest support)
 *   0.9.0.1 - Persistent blessing choices added
 *   0.9.0.2 - Progressive scoring system, increased reserved_runtime[1->32]
 */

/* Metarun file format always tracks the core game version. Update the release
 * numbers below when compatibility changes.
 *   0.9.0.0 - Initial versioned format (quest support)
 *   0.9.0.1 - Persistent blessing choices added
 *   0.9.0.2 - Per-quest completion counters (capped) stored alongside bitmask
 *   0.9.1.3 - Current meta-file version (matches game release)
 */
#define METARUN_FILE_VERSION_MAJOR VERSION_MAJOR
#define METARUN_FILE_VERSION_MINOR VERSION_MINOR
#define METARUN_FILE_VERSION_PATCH VERSION_PATCH
#define METARUN_FILE_VERSION_EXTRA VERSION_EXTRA

#define META_SUBDIR "metaruns"
#define META_RAW "meta.raw"

/* Blessing / reward economy */
#define METARUN_BLESSING_POINT_THRESHOLD 300   /* Fallback threshold if runtype doesn't specify (data-driven via L: in runtypes.txt) */

typedef enum {
    METARUN_BLESSING_THRESHOLD_NORMAL = RUNTYPE_BLESSING_MODE_NORMAL,
    METARUN_BLESSING_THRESHOLD_EASIER = RUNTYPE_BLESSING_MODE_EASIER,
    METARUN_BLESSING_THRESHOLD_HARDER = RUNTYPE_BLESSING_MODE_HARDER,
    METARUN_BLESSING_THRESHOLD_MODE_MAX = RUNTYPE_BLESSING_MODE_COUNT
} metarun_blessing_threshold_mode;

typedef enum {
    METARUN_MAJOR_EFFECT_NONE = 0,
    METARUN_MAJOR_EFFECT_SUPPLY_LIMIT,
    METARUN_MAJOR_EFFECT_START_ARTIFACT,
} metarun_major_effect;

typedef struct meta_file_header
{
    byte version_major;  /* Major version (0) */
    byte version_minor;  /* Minor version (9) */
    byte version_patch;  /* Patch version (1) */
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

    int8_t curse_stacks[METAR_CURSE_SLOTS]; /* signed stacks: >0 curses, <0 blessings */
    u64b curses_seen;      /* bit i == 1  -> curse i is known/revealed */

    /* ----- persistent settings ----------------------------------- */
    u32b persistent_options[8];  /* Persistent options across the metarun */
    byte persistent_delay_factor; /* Persistent delay factor */
    byte persistent_hitpoint_warn; /* Persistent hitpoint warning */
    u32b persistent_window_flags[SAVE_WINDOW_TERM_MAX]; /* Persistent window flags */
    byte persistent_options_initialized; /* Flag to track if persistent options are set */

    /* ----- quest completion tracking --------------------------- */
    u32b completed_quests;      /* Bitmask of completed quests (bit 0=Tulkas, bit 1=Aulë, etc.) */
    byte quest_completion_counts[METARUN_QUEST_SLOT_MAX]; /* Times each quest has been completed this metarun (capped) */
    
    /* ----- oath system tracking -------------------------------- */
    byte unlocked_oaths;        /* Bitmask of oaths unlocked this metarun (OATH_*_FLAG bits: Mercy, Silence, Iron, Smith, Valorous, Light) */
    byte banned_oaths;          /* Bitmask of oaths broken/banned this metarun (cannot select again) */
    byte max_difficulty_reached; /* Maximum difficulty level reached this metarun (cannot go back) */
    
    byte quest_reserved[12];    /* Reserved for future quest expansion */

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
    
    byte blessing_threshold_mode;     /* 0=normal (default), 1=easier, 2=harder          */
    byte reserved_runtime[31];        /* Remaining runtime expansion space               */

} metarun;

/* The *current* meta-run - defined once in variable.c */
extern metarun metar;

int8_t* active_curse_stacks(void);
u64b* active_curses_seen_ptr(void);

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
void metarun_show_poetry_scene(cptr title, byte title_attr, cptr body,
                               byte body_attr, cptr transition,
                               byte transition_attr, cptr prompt);
void metarun_show_poetry_blocks(cptr title, byte title_attr,
                                cptr blocks[], const byte block_attrs[],
                                const bool block_outcome_reveals[],
                                int block_count, cptr prompt, int hold_ms,
                                bool wait_for_key, bool immediate,
                                bool *fast_forward);
void metarun_debug_show_run_result(bool victory, int silmarils, int alive,
                                   int required_survivors);
void metarun_debug_show_run_summary(int silmarils, int stolen_silmarils,
                                    bool show_treachery,
                                    int kinslaying_attempt);
int metarun_debug_preview_curse_menu(void);
int metarun_debug_choose_escape_curses(int n, int out[4]);
void metarun_increment_deaths(void);             /* Shortcut: +1 death      */
void metarun_gain_silmarils(byte n);             /* Shortcut: +n Silmarils  */

void print_metarun_stats(void);                  /* Pretty single-run view  */
void list_metaruns(void);                        /* Full meta-run history   */
void refresh_current_metar_score(void);          /* Recompute cached score for active run */
const metarun *metarun_current(void);            /* Read-only pointer to current metarun */
metarun *metarun_current_mutable(void);          /* Mutable pointer to current metarun */
const metarun *metarun_entry_const(s16b idx);    /* Bounds-checked read-only access */
metarun *metarun_entry_mutable(s16b idx);        /* Bounds-checked mutable access */
s16b metarun_current_index(void);                /* Current metarun index or -1 */
s16b metarun_entry_count(void);                  /* Total metarun entries loaded */
int metarun_completed_count(void);              /* Count finished metaruns */
bool metarun_tale_management_available(void);   /* No active character/story save */
bool metarun_tale_recovery_required(void);      /* Story ledger needs restart recovery */
bool metarun_activate_tale(s16b idx);            /* Make a recorded tale current */
bool metarun_create_tale(void);                  /* Create/select a fresh tale */

/* ------------------------------------------------------------------ */
/*  Quest completion tracking                                         */
/* ------------------------------------------------------------------ */
bool metarun_is_quest_completed(u32b quest_flag);   /* Check if quest is completed */
int metarun_quest_completion_count(u32b quest_flag); /* How many times quest completed (capped) */
void metarun_mark_quest_completed(u32b quest_flag); /* Mark quest as completed */
void metarun_check_and_update_quests(void);         /* Check current character quests and update metarun */
void metarun_restore_quest_states(void);            /* Restore quest states from metarun after character load */
void metarun_seed_quest_counts_from_mask(metarun *m, u32b mask); /* Expand quest mask into counters */
void metarun_clamp_and_sync_quests(metarun *m);     /* Clamp counters and sync mask */
int metarun_total_quest_completions(const metarun *m); /* Aggregate quest completion total */
int metarun_quests_completed_at_least(int minimum_count); /* Count quests completed at least N times */
bool metarun_repeat_tier_unlocked(int prior_completion_count); /* Is another repeat tier available? */
int quest_initiated_count_this_run(void);           /* Quest encounters spawned/initiated this character */
int quest_accepted_count_this_run(void);            /* Active accepted quests this character */
bool quest_can_initiate_more(void);                 /* Below QUEST_MAX_INITIATED_PER_RUN */
bool quest_can_accept_more(void);                   /* Below QUEST_MAX_ACCEPTED_PER_RUN */
void quest_note_initiated(int quest_idx);           /* Record a quest encounter for per-run cap */

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
void metarun_apply_artefact_memory(void);        /* Apply remembered artefact lore */
void metarun_seed_artefact_memory_from_current_state_if_missing(void);
bool metarun_record_artefact_identification(int a_idx);
bool metarun_record_artefact_revealed(int a_idx);
bool metarun_try_identify_remembered_artefact(object_type *o_ptr);

void metarun_apply_runtime_effects(void);        /* Sync blessing effects into runtime systems */
bool metarun_has_major_blessing_effect(metarun_major_effect effect);
bool metarun_has_major_blessing_index(int idx);
int  metarun_major_blessing_count(void);
int  metarun_alive_count_cached(void);
u32b compute_blessing_pool(void);               /* Recalculate pool totals (returns total score) */
int  blessing_points_available(void);           /* Unspent blessing credits */

static inline metarun_blessing_threshold_mode metarun_get_threshold_mode(const metarun *m)
{
    if (!m) return METARUN_BLESSING_THRESHOLD_NORMAL;
    byte mode = m->blessing_threshold_mode;
    if (mode >= METARUN_BLESSING_THRESHOLD_MODE_MAX) return METARUN_BLESSING_THRESHOLD_NORMAL;
    return (metarun_blessing_threshold_mode)mode;
}

static inline void metarun_set_threshold_mode(metarun *m, metarun_blessing_threshold_mode mode)
{
    if (!m) return;
    if (mode >= METARUN_BLESSING_THRESHOLD_MODE_MAX) mode = METARUN_BLESSING_THRESHOLD_NORMAL;
    m->blessing_threshold_mode = (byte)mode;
}

static inline int CURSE_GET(int id)
{
    int8_t* stacks = active_curse_stacks();
    if (id < 0 || id >= METAR_CURSE_SLOTS) return 0;  /* bounds check */
    return stacks ? stacks[id] : 0;
}

static inline void CURSE_SET(int id, int val)
{
    int8_t* stacks = active_curse_stacks();
    if (id < 0 || id >= METAR_CURSE_SLOTS) return;    /* bounds check */
    if (!stacks) return;
    if (val > 127) val = 127;
    if (val < -127) val = -127;
    stacks[id] = (int8_t)val;
}

static inline bool CURSE_SEEN(int id)
{
    u64b* seen = active_curses_seen_ptr();
    if (id < 0 || id >= METAR_CURSE_SLOTS) return false;  // Add bounds check
    if (!seen) return false;
    return ((*seen) & (1ULL << id)) != 0;
}

static inline void CURSE_SEEN_SET(int id)
{
    u64b* seen = active_curses_seen_ptr();
    if (id < 0 || id >= METAR_CURSE_SLOTS) return;        // Add bounds check
    if (!seen) return;
    (*seen) |= (1ULL << id);
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

static inline int CURSE_CURSE_CAP(int id)
{
    if (id < 0 || id >= METAR_CURSE_SLOTS || !cu_info) return 0;
    return cu_info[id].max_stacks;
}

static inline int CURSE_BLESSING_CAP(int id)
{
    if (id < 0 || id >= METAR_CURSE_SLOTS || !cu_info) return 0;
    if (cu_info[id].max_blessing_stacks > 0)
        return cu_info[id].max_blessing_stacks;
    return cu_info[id].max_stacks;
}

/* ------------------------------------------------------------------ */
/*  Public helpers implemented by the metarun subsystem                */
/* ------------------------------------------------------------------ */
extern bool metarun_created;           /* Flag set when new metarun file created */
void cleanup_old_game_files(void);     /* Clean save/score files on fresh start */
int  menu_choose_one_curse(int n);      /* weighted picker / poem menu  */
int  choose_escape_curses_ui(int n, int out[4]); /* interactive curse selection */
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
int  curse_flag_delta_cur(u32b cur_flag);  /* signed CUR curse/blessing delta */
int  any_curse_flag_active(u32b flag);     /* CUR-only helper      */
void metarun_clear_blessing_runtime_fields(metarun *m); /* Reset runtime blessing fields */
void metarun_sanitize_blessing_economy(metarun *m);     /* Clamp blessing totals */
void metarun_sanitize_major_blessing_bits(metarun *m);  /* Trim major blessing mask */

#endif /* METARUN_H */
