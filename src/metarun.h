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
typedef struct meta_file_header
{
    byte version_major;  /* Major version (0) */
    byte version_minor;  /* Minor version (8) */
    byte version_patch;  /* Patch version (5) */
    byte version_extra;  /* Extra version (0) */
    u32b entry_count;    /* Number of metarun entries in file */
} meta_file_header;

/* Old metarun structure for backwards compatibility */
typedef struct metarun_old
{
    /* ----- per-run counters --------------------------------------- */
    u32b id;            /* monotonic 0-based index                    */
    byte type;          /* reserved for future run-type support       */
    byte deaths;        /* how many characters have died so far       */
    byte silmarils;     /* Silmarils recovered so far                 */
    u32b last_played;   /* time() of the most recent character        */

    u32b curses_lo;     /* curse IDs  0–15  – 2 bits each (max 4) */
    u32b curses_hi;     /* curse IDs 16–31  – 2 bits each (max 4) */
    u32b curses_seen;   /* bit i == 1  → curse i is known/revealed    */
} metarun_old;

typedef struct metarun
{
    /* ----- per-run counters --------------------------------------- */
    u32b id;            /* monotonic 0-based index                    */
    byte type;          /* reserved for future run-type support       */
    byte deaths;        /* how many characters have died so far       */
    byte silmarils;     /* Silmarils recovered so far                 */
    u32b last_played;   /* time() of the most recent character        */

    u32b curses_lo;     /* curse IDs  0–15  – 2 bits each (max 4) */
    u32b curses_hi;     /* curse IDs 16–31  – 2 bits each (max 4) */
    u32b curses_seen;   /* bit i == 1  → curse i is known/revealed    */

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
    
    byte quest_reserved[13];    /* Reserved for future quest expansion (reduced from 15 to accommodate oath fields) */

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
                            byte new_silmarils);
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

static inline byte CURSE_GET(int id)
{
    if (id < 0 || id >= 32) return 0;  // Add bounds check
    u32b word = (id < 16) ? metar.curses_lo : metar.curses_hi;
    int  sh   = (id & 15) * 2;          
    return (word >> sh) & 0x3;          
}

static inline void CURSE_SET(int id, byte val)
{
    if (id < 0 || id >= 32) return;    // Add bounds check
    u32b *p   = (id < 16) ? &metar.curses_lo : &metar.curses_hi;
    int   sh  = (id & 15) * 2;
    *p = (*p & ~(0x3UL << sh))          
       | ((u32b)(val & 0x3) << sh);     
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

#define CURSE_ADD(id, d)  CURSE_SET((id), (byte)(CURSE_GET(id) + (d)))

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
int  curse_flag_count(u32b flag);          /* legacy: RHF+CUR */
int  any_curse_flag_active(u32b flag);     /* CUR-only helper      */

#endif /* METARUN_H */
