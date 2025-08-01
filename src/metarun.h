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
/*  Meta-run save-record                                              */
/* ------------------------------------------------------------------ */
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

void metarun_increment_deaths(void);             /* Shortcut: +1 death      */
void metarun_gain_silmarils(byte n);             /* Shortcut: +n Silmarils  */

void print_metarun_stats(void);                  /* Pretty single-run view  */
void list_metaruns(void);                        /* Full meta-run history   */

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
int  menu_choose_one_curse(int n);      /* weighted picker / poem menu  */
void metarun_clear_all_curses(void);   /* zero every curse counter     */
void add_curse_stack(int idx);         /* +1 stack respecting caps     */
/* NEW: show a menu of all *known* curses (those with CURSE_SEEN). */
void show_known_curses_menu(void);

/* Flag-query utilities used throughout the code-base                 */
u32b curse_flag_mask(void);            /* bitmask of active flags      */
int  curse_flag_count_rhf(u32b rhf_flag);  /* #curses with RHF bit  */
int  curse_flag_count_cur(u32b cur_flag);  /* #curses with CUR bit  */
int  curse_flag_count(u32b flag);          /* legacy: RHF+CUR */
int  any_curse_flag_active(u32b flag);     /* CUR-only helper      */

#endif /* METARUN_H */
