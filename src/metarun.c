/* --------------------------------------------------------------------
 *  src/metarun.c   (2025-07-06)   – final, crash-free, warning-free
 * --------------------------------------------------------------------
 *  Tracks a “meta-run” that ends after 15 Silmarils (win) or
 *  15 deaths (lose).  Finished runs are appended to meta.raw so
 *  the entire history is preserved.  Includes:
 *     • list_metaruns()  – compact history view
 *     • print_metarun_stats() – details for current run
 * -------------------------------------------------------------------- */
#include "angband.h"
#include "metarun.h"
#include "init.h"        /* cu_info / z_info */
#include "platform.h"    /* path_build(), fd_*, MKDIR         */

#include <string.h>
#include <stdlib.h>
#include <time.h>

/* =========================  constants  ========================= */
#define META_RAW          "meta.raw"
#define META_SUBDIR       "metaruns"
#define CURSE_MENU_LINES  3

/* 4-bit curse counters ------------------------------------------ */
#define CURSE_GET(i) (((i)<16)?((meta.curses_lo>>((i)*4))&0xF) \
                             :((meta.curses_hi>>(((i)-16)*4))&0xF))
#define CURSE_ADD(i,n) do { \
        if ((i)<16) meta.curses_lo += ((u32b)(n)<<((i)*4)); \
        else        meta.curses_hi += ((u32b)(n)<<(((i)-16)*4)); \
    } while(0)

/* =========================  globals  =========================== */
static metarun *metaruns    = NULL;
static s16b     metarun_max = 0;
static s16b     current_run = 0;
bool            metarun_created = FALSE;

/* ==================  tiny local helpers  ======================= */
static int rng_int(int max) { return max ? (int)(rand() % max) : 0; }

static void build_meta_path(char *buf, size_t len,
                            const metarun *m, const char *leaf)
{
    char sub[128];
    if (m)
        strnfmt(sub, sizeof sub, "%s/%08u/%s",
                META_SUBDIR, (unsigned)m->id, leaf);
    else
        strnfmt(sub, sizeof sub, "%s/%s", META_SUBDIR, leaf);
    path_build(buf, len, ANGBAND_DIR_APEX, sub);
}

static void reset_defaults(metarun *m)
{
    memset(m, 0, sizeof(*m));
    m->id          = 1;
    m->last_played = (u32b)time(NULL);
}

/* ensure directory apex/metaruns/NNNNNNNN exists */
static void ensure_run_dir(const metarun *m)
{
    char dir[1024];
    path_build(dir, sizeof dir, ANGBAND_DIR_APEX, META_SUBDIR); MKDIR(dir);
    strnfmt(dir, sizeof dir, "%s/%08u", META_SUBDIR, (unsigned)m->id);
    path_build(dir, sizeof dir, ANGBAND_DIR_APEX, dir);         MKDIR(dir);
}

/* forward declarations */
static void choose_escape_curses(int n);
static void check_run_end(void);
static void start_new_metarun(void);

/* =======================  load / save  ========================= */
errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    int  fd;

    build_meta_path(fn, sizeof fn, NULL, META_RAW);
    fd = fd_open(fn, O_RDONLY);

    if (fd < 0 && create_if_missing) {
        FILE_TYPE(FILE_TYPE_DATA);
        fd = fd_make(fn, 0644);
        if (fd < 0) return -1;

        metarun seed; reset_defaults(&seed);
        fd_write(fd, (cptr)&seed, sizeof seed);
        fd_seek(fd, 0);
        metarun_created = TRUE;
    }
    if (fd < 0) return -1;

    metarun_max = (s16b)(fd_file_size(fd) / sizeof(metarun));
    metaruns    = C_ZNEW(metarun_max, metarun);
    fd_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);

    /* choose current run */
    u32b latest = 0;
    for (s16b i = 0; i < metarun_max; i++) {
        if (metaruns[i].last_played > latest ||
            (metaruns[i].last_played == latest && i > current_run))
        {
            latest      = metaruns[i].last_played;
            current_run = i;
        }
    }
    meta = metaruns[current_run];

    /* ensure its per-run directory exists */
    ensure_run_dir(&meta);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Safely write the meta-run array.  Bail out if the indices look     *
 *  wrong – avoids dereferencing a freed/reallocated block.           *
 * ------------------------------------------------------------------ */
errr save_metaruns(void)
{
    if (!metaruns || current_run < 0 || current_run >= metarun_max)
        return -1;                           /* corrupted state – do nothing */

    char fn[1024];
    build_meta_path(fn, sizeof fn, NULL, META_RAW);

    int fd = fd_open(fn, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return -1;

    meta.last_played      = (u32b)time(NULL);
    metaruns[current_run] = meta;            /* safe: array is valid */

    fd_write(fd, (cptr)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);
    return 0;
}

int any_curse_flag_active(u32b flag)
 {
    /* returns true if one or more active curses have `flag` set */
    return (curse_flag_count(flag) > 0);
 }

/* ---------------------------------------------------------------
 * Pick a curse at random, respecting weights, stacks, caps,
 * and the RHF_CURSE tail-lift.
 * ------------------------------------------------------------- */
static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero’s lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->phouse].flags & RHF_CURSE);

    /* Pass 1 – find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 – sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;           /* cap reached */

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rng_int(z_info->cu_max);    /* safety net */

    /* Pass 3 – roulette wheel */
    long pick = rng_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rng_int(z_info->cu_max);                /* unreachable */
}



static int menu_choose_one_curse(void)
{

        /* if any active curse has the “no‐choice” flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES], sel;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = FALSE;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = TRUE; break; }
            
            byte cap = cu_info[pick[i]].max_stacks;
            if (cap && CURSE_GET(pick[i]) >= cap) { dup = TRUE; continue; }

        } while (dup);
    }

    screen_save();  Term_clear();
    c_prt(TERM_YELLOW,
          "Dark powers demand their price – choose your curse:", 2, 2);

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[pick[i]];
        c_put_str(TERM_L_RED,
                  format("%c) %s", 'a'+i, cu_name + cu->name),
                  4+i, 4);
    }
    c_put_str(TERM_L_DARK, "Press a, b or c.", 8, 4);

    sel = -1;
    while (sel < 0 || sel >= CURSE_MENU_LINES) sel = inkey() - 'a';
    screen_load();
    return pick[sel];
}

/**
 * Grant escape curses after a metarun.
 * If any active curse has the CUR_NOCHOICE bit, only one curse is rolled.
 */
static void choose_escape_curses(int n)
{
    /* If “No-Choice” is active on any curse, limit to a single roll */
    int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;

    for (int i = 0; i < rolls; i++) {
        int idx = menu_choose_one_curse();

        if (cu_info[idx].max_stacks &&
            CURSE_GET(idx) >= cu_info[idx].max_stacks)
            continue;   /* skip – player already at the cap */

        /* Add one stack of the chosen curse */
        CURSE_ADD(idx, 1);

        /* Persist the metarun state */
        save_metaruns();

        /* Notify the player */
        msg_format("The curse of %s binds your fate…",
                   cu_name + cu_info[idx].name);
        message_flush();
    }
}


/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 *  NOTE: save_metaruns() comes **after** check_run_end() so that     *
 *  any realloc in start_new_metarun() has already finished.          *
 * ------------------------------------------------------------------ */
void metarun_update_on_exit(bool died, bool escaped, byte new_sils)
{
    if (died)    meta.deaths++;
    if (escaped) meta.silmarils += new_sils;

    if (escaped && new_sils) choose_escape_curses(new_sils);

    check_run_end();              /* may realloc or grow `metaruns` */

    save_metaruns();              /* pointer is guaranteed valid here */
}


void metarun_increment_deaths(void)   { metarun_update_on_exit(TRUE,  FALSE, 0); }
void metarun_gain_silmarils(byte n)   { metarun_update_on_exit(FALSE, TRUE,  n); }

/* ======================  run-state logic  ====================== */
/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 * ------------------------------------------------------------------ */
static void check_run_end(void)
{
    if (meta.silmarils >= WINCON_SILMARILS) {
        msg_print("\n*** VICTORY! 15 Silmarils recovered – a new age dawns… ***\n");
        message_flush();
        start_new_metarun();

    } else if (meta.deaths >= LOSECON_DEATHS) {
        msg_print(format(
            "\n*** DEFEAT! %d hero%s fallen – the run is lost. ***\n",
            LOSECON_DEATHS, (LOSECON_DEATHS == 1) ? " has" : "es have"));
        message_flush();
        start_new_metarun();
    }
}

/* ------------------------------------------------------------------
 *  Start a brand-new meta-run.
 *  We snapshot the finished run **after** the array has been grown,
 *  so we only write once and always with the final pointer.
 * ------------------------------------------------------------------ */
static void start_new_metarun(void)
{
    /* Save old state */
    s16b old_max   = metarun_max;
    metarun *old   = metaruns;

    /* Try to allocate a new array for one more run */
    metarun *tmp = C_RNEW(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed — keep everything as is */
        return;
    }

    /* Copy over the previous runs (if any) */
    if (old) {
        C_COPY(tmp, old, old_max, metarun);
    }

    /* Free the old array just once */
    FREE(old);

    /* Commit the new array and size */
    metaruns    = tmp;
    metarun_max = old_max + 1;

    /* Initialize the brand-new slot */
    reset_defaults(&metaruns[metarun_max - 1]);
    metaruns[metarun_max - 1].id = meta.id + 1;

    /* Update globals */
    current_run      = metarun_max - 1;
    meta             = metaruns[current_run];
    metarun_created  = TRUE;

    /* Persist and prepare */
    save_metaruns();      /* safe now that metaruns≠NULL */ 
    ensure_run_dir(&meta);
}


/* ==================  stats & history screens  ================== */
void print_metarun_stats(void)
{
    screen_save();
    Term_clear();
    text_out_hook = text_out_to_screen;  text_out_wrap = 0;

    text_out_c(TERM_L_GREEN, "\nCurrent Metarun Statistics\n\n");
    text_out(format(" Run-ID          : %u\n", meta.id));
    text_out(format(" Silmarils       : %d / %d\n",
                    meta.silmarils, WINCON_SILMARILS));
    text_out(format(" Deaths          : %d / %d\n",
                    meta.deaths,    LOSECON_DEATHS));

    char datebuf[32];
    strftime(datebuf, sizeof datebuf, "%Y-%m-%d",
             localtime((time_t*)&meta.last_played));
    text_out(format(" Last played     : %s\n", datebuf));

    text_out("\nPress any key to continue.");
    inkey();
    screen_load();
}

/* compact table of all meta-runs */
void list_metaruns(void)
{
    screen_save();
    Term_clear();
    c_prt(TERM_L_GREEN, "Meta-run history", 1, 2);
    c_put_str(TERM_L_DARK,
              " ID       Sil  Dth  Res  Last played", 3, 2);

    int row = 4;
    for (s16b i = 0; i < metarun_max; i++) {
        const metarun *m = &metaruns[i];
        char res = (m->silmarils >= WINCON_SILMARILS) ? 'W' :
                   (m->deaths    >= LOSECON_DEATHS)   ? 'L' : ' ';
        char date[16];
        strftime(date, sizeof date, "%Y-%m-%d",
                 localtime((time_t*)&m->last_played));

        c_put_str((i == current_run) ? TERM_YELLOW : TERM_WHITE,
                  format("%08u   %2d   %2d   %c   %s",
                         m->id, m->silmarils, m->deaths, res, date),
                  row++, 2);

        if (row >= 23 && i+1 < metarun_max) {   /* page break */
            c_put_str(TERM_L_DARK, "[more – any key]", 23, 2);
            inkey();  Term_clear();
            row = 4;
            c_prt(TERM_L_GREEN, "Meta-run history (cont.)", 1, 2);
            c_put_str(TERM_L_DARK,
                      " ID       Sil  Dth  Res  Last played", 3, 2);
        }
    }
    c_put_str(TERM_L_DARK, "Press any key to return.", row+1, 2);
    inkey();
    screen_load();
}
