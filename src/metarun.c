#include "angband.h"
#include "metarun.h"

/* compile-time constants */
#define META_RAW      "meta.raw"
#define META_SUBDIR   "metaruns"   /* lib/apex/metaruns/<id>/ */

/* in-memory table -------------------------------------------------- */
static metarun *metaruns   = NULL;
static s16b     metarun_max = 0;
s16b            current_metarun = 0;

extern runtype_type *runtype_info;   /* now in the header; keep for C++ */


/* local helpers ---------------------------------------------------- */
static void reset_defaults(metarun *m)
{
    WIPE(m, metarun);
    m->id          = 1;
    m->type        = 0;
    m->last_played = 0;
}

static void build_meta_path(char *dst, size_t n, const metarun *m, cptr leaf)
{
    char sub[16];
    path_build(dst, n, ANGBAND_DIR_APEX, META_SUBDIR);
    path_build(dst, n, dst, format("%08u", m->id));
    validate_dir(dst); /* create if missing   */
    path_build(dst, n, dst, leaf);
}

/* ------------- load / save whole table (lib/apex/meta.raw) -------- */

errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    int  fd;
    path_build(fn, sizeof(fn), ANGBAND_DIR_APEX, META_RAW);

    fd = fd_open(fn, O_RDONLY);
    if (fd < 0 && create_if_missing)
    {
        FILE_TYPE(FILE_TYPE_DATA);
        fd = fd_make(fn, 0644);
        if (fd < 0) return -1;

        metarun def; reset_defaults(&def);
        (void)fd_write(fd, (cptr)&def, sizeof(def));
        fd_seek(fd, 0);
    }
    if (fd < 0) return -1;

    metarun_max = (s16b)(fd_file_size(fd) / sizeof(metarun));
    metaruns    = C_ZNEW(metarun_max, metarun);
    fd_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);

    /* pick most-recent */
    u32b latest = 0;
    for (s16b i = 0; i < metarun_max; i++)
        if (metaruns[i].last_played > latest)
        { latest = metaruns[i].last_played; current_metarun = i; }

    meta = metaruns[current_metarun];
    return 0;
}

errr save_metaruns(void)
{
    char fn[1024];
    int  fd;
    path_build(fn, sizeof(fn), ANGBAND_DIR_APEX, META_RAW);

    fd = fd_open(fn, O_RDWR);
    if (fd < 0) return -1;

    meta.last_played = (u32b)time(NULL);          /* stamp */
    metaruns[current_metarun] = meta;

    fd_seek(fd, 0);
    fd_write(fd,(cptr)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);
    return 0;
}

/* ------------- public helpers called from game code -------------- */

void metarun_update_on_exit(bool dead, bool escaped, byte new_sils)
{
    if (dead)             meta.deaths++;
    if (escaped)          meta.silmarils += new_sils;
    save_metaruns();
}

/* simplistic character selector ----------------------------------- */

static bool any_alive_character(const metarun *m)
{
    char path[1024];
    build_meta_path(path, sizeof(path), m, "scores.raw");
    int fd = fd_open(path, O_RDONLY);
    if (fd < 0) return FALSE;

    high_score hs;            /* struct from files.c */
    bool alive = FALSE;
    while (!fd_read(fd, (char*)&hs, sizeof(hs)))
        if (hs.turns[0] && !hs.death[0]) { alive = TRUE; break; }
    fd_close(fd);
    return alive;
}

void start_metarun(void)
{
    /* show menu (omitted here for brevity — you can reuse the old death menu) */
    metarun *m = &meta; /* currently active */

    /* decide whether to load or create */
    if (any_alive_character(m))
        load_player();              /* existing save in that folder */
    else
        character_wipe();           /* birth.c handles new char */

    /* record touch */
    save_metaruns();
    play_game();
}
