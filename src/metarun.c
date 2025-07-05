/* src/metarun.c  –  portable metarun support */

#include "angband.h"
#include "metarun.h"
#include "init.h"        /* cu_info / z_info */
#include "platform.h"    /* MKDIR / fd_file_size */

#include <stdlib.h>      /* rand() */

/* --------------------------------------------------- globals */
bool    metarun_created = FALSE;
// metarun meta;                                   /* current run */

/* ------------------------------------------------- constants */
#define META_RAW          "meta.raw"
#define META_SUBDIR       "metaruns"
#define CURSE_MENU_LINES  3                      /* show 3 picks */

/* ---------- 4-bit per-curse counters (32 total) ------------ */
#define CURSE_GET(i)   (((i) < 16) ? ((meta.curses_lo >> ((i)*4)) & 0xF) \
                                    : ((meta.curses_hi >> (((i)-16)*4)) & 0xF))

#define CURSE_ADD(i,n) do { \
        if ((i) < 16) meta.curses_lo += ((u32b)(n) << ((i)*4)); \
        else          meta.curses_hi += ((u32b)(n) << (((i)-16)*4)); \
    } while (0)

/* ------------------------------------------------ table in RAM */
static metarun *metaruns        = NULL;
static s16b     metarun_max     = 0;
s16b            current_metarun = 0;

extern runtype_type *runtype_info;

/* ------------------------------------------------ helpers */
static int rng0(int m)          { return m ? rand() % m : 0; }

static void reset_defaults(metarun *m)
{
    WIPE(m, metarun);
    m->id = 1;  m->type = 0;  m->last_played = 0;
}

static void build_meta_path(char *dst, size_t n,
                            const metarun *m, cptr leaf)
{
    path_build(dst, n, ANGBAND_DIR_APEX, META_SUBDIR);
    path_build(dst, n, dst, format("%08u", m->id));
    MKDIR(dst);
    path_build(dst, n, dst, leaf);
}

/* ------------------------------------------------ load / save */
errr load_metaruns(bool create_if_missing)
{
    char fn[1024];  int fd;
    path_build(fn, sizeof fn, ANGBAND_DIR_APEX, META_RAW);
    fd = fd_open(fn, O_RDONLY);

    if (fd < 0 && create_if_missing)
    {
        FILE_TYPE(FILE_TYPE_DATA);
        fd = fd_make(fn, 0644);  if (fd < 0) return -1;
        metarun def;  reset_defaults(&def);
        fd_write(fd, (cptr)&def, sizeof def);
        fd_seek(fd, 0);
        metarun_created = TRUE;
    }
    if (fd < 0) return -1;

    metarun_max = (s16b)(fd_file_size(fd) / sizeof(metarun));
    metaruns    = C_ZNEW(metarun_max, metarun);
    fd_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);

    u32b latest = 0;
    for (s16b i = 0; i < metarun_max; i++)
        if (metaruns[i].last_played > latest)
        { latest = metaruns[i].last_played; current_metarun = i; }

    meta = metaruns[current_metarun];
    return 0;
}

errr save_metaruns(void)
{
    char fn[1024];  int fd;
    path_build(fn, sizeof fn, ANGBAND_DIR_APEX, META_RAW);
    fd = fd_open(fn, O_RDWR);  if (fd < 0) return -1;

    meta.last_played = (u32b)time(NULL);
    metaruns[current_metarun] = meta;

    fd_seek(fd, 0);
    fd_write(fd, (cptr)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);
    return 0;
}

/* ------------------------------------------------ curse menu */
static int random_curse(void) { return rng0(z_info->cu_max); }

static int menu_choose_one_curse(void)
{
    int choice[CURSE_MENU_LINES];
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool again;
        do {
            again = FALSE;
            choice[i] = random_curse();
            for (int j = 0; j < i; j++) if (choice[i] == choice[j]) { again = TRUE; break; }
        } while (again);
    }

    screen_save();  Term_clear();
    c_prt(TERM_YELLOW, "Dark powers demand their price ... choose your curse:", 2, 2);

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[choice[i]];
        c_put_str(TERM_L_RED,
                  format("%c) %s", 'a'+i, cu_name + cu->name),
                  4+i, 4);
    }
    c_put_str(TERM_L_DARK, "Press a, b or c.", 8, 4);

    int sel = -1;
    while (sel < 0) {
        int ch = inkey();
        if (ch == 'a' || ch == 'b' || ch == 'c') sel = ch - 'a';
    }
    screen_load();
    return choice[sel];
}

static void choose_escape_curses(int n)
{
    for (int i = 0; i < n; i++) {
        int idx = menu_choose_one_curse();
        CURSE_ADD(idx, 1);
        save_metaruns();
        msg_format("The curse of %s binds your fate ...",
                   cu_name + cu_info[idx].name);
        message_flush();
    }
}

/* ------------------------------------------------ public API */
void metarun_update_on_exit(bool dead, bool escaped, byte new_sils)
{
    if (dead)    meta.deaths++;
    if (escaped) meta.silmarils += new_sils;
    save_metaruns();
    if (escaped && new_sils) choose_escape_curses(new_sils);
}

/* alive-character detector */
static bool any_alive_character(const metarun *m)
{
    char path[1024]; build_meta_path(path, sizeof path, m, "scores.raw");
    int fd = fd_open(path, O_RDONLY);  if (fd < 0) return FALSE;

    high_score hs; bool live = FALSE;
    while (!fd_read(fd, (char*)&hs, sizeof hs))
        if (hs.turns[0] && streq(hs.how, "(alive and well)"))
        { live = TRUE; break; }
    fd_close(fd);
    return live;
}

/* launch / resume */
void start_metarun(void)
{
    if (any_alive_character(&meta)) load_player();
    else                            character_wipe();

    save_metaruns();
    play_game(TRUE);
}

/* ============================================================ */
/*              printable statistics helper                     */
/* ============================================================ */
void print_metarun_stats(void)
{
    screen_save();  Term_clear();
    text_out_hook = text_out_to_screen;  text_out_wrap = 0;

    text_out_c(TERM_L_GREEN, "\nCurrent Metarun Statistics\n\n");

    text_out(format(" Run-ID          : %u\n", meta.id));
    text_out(format(" Hero type       : %s\n",
                    runtype_info[meta.type].name));
    text_out(format(" Silmarils       : %d\n", meta.silmarils));
    text_out(format(" Deaths          : %d\n", meta.deaths));

    text_out("\n Curses selected so far:\n");
    for (int i = 0; i < (int)z_info->cu_max; i++) {
        int cnt = CURSE_GET(i);
        if (cnt)
            text_out(format("   %-24s × %d\n",
                            cu_name + cu_info[i].name, cnt));
    }

    char datebuf[32];
    strftime(datebuf, sizeof datebuf, "%Y-%m-%d",
             localtime((time_t *)&meta.last_played));
    text_out(format("\n Last played     : %s\n", datebuf));

    text_out("\nPress any key to continue.");
    inkey();
    screen_load();
}
