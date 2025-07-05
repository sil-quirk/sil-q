#ifndef METARUN_H
#define METARUN_H

#include "angband.h"

/* -------------------------- data layout --------------------------- */

typedef struct metarun
{
    u32b id;           /* directory / score-file id (00000001 …)  */
    byte type;         /* index into runtypes.raw                 */
    byte deaths;       /* dead characters in this run             */
    byte silmarils;    /* Silmarils recovered so far              */
    byte pad;          /* alignment / future use                  */
    u32b curses_lo;    /* 16 × 4-bit digits -> curses 0-15        */
    u32b curses_hi;    /* 16 × 4-bit digits -> curses 16-31       */
    u32b last_played;  /* time() stamp                            */
} metarun;

/* global handle (current run in memory) */
extern metarun   meta;
extern int       meta_fd;
extern bool      metarun_created; 

/* public API ------------------------------------------------------- */
errr  load_metaruns(bool create_if_missing);   /* init2.c calls this once */
errr  save_metaruns(void);                     /* called by save.c, death */
void  metarun_update_on_exit(bool character_died, bool escaped_with_sils,
                             byte new_silmarils);

extern void print_metarun_stats(void);

void  start_metarun(void);  /* wrapper that chooses or creates a char */

#endif /* METARUN_H */
