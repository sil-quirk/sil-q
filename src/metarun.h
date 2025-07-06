#ifndef METARUN_H
#define METARUN_H
#include "angband.h"
#define WINCON_SILMARILS 15
#define LOSECON_DEATHS    2   /* test value */
typedef struct metarun {
    u32b id; byte type, deaths, silmarils; u32b last_played;
    u32b curses_lo, curses_hi;
} metarun;
extern metarun meta;
errr load_metaruns(bool create_if_missing);
errr save_metaruns(void);
void metarun_update_on_exit(bool died,bool esc,byte new_sils);
void metarun_increment_deaths(void);
void metarun_gain_silmarils(byte);
void print_metarun_stats(void);
void list_metaruns(void);
#endif
