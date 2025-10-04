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
#include "h-define.h"
#include "log.h"
#include "platform.h"    /* path_build(), fd_*, MKDIR         */
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define METARUN_V5_SIZE (sizeof(metarun) - (2 * sizeof(u32b)))

#ifdef WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif  

/* --------------------------------------------------------------- */
/*  metarun.c : quick-and-dirty logger                             */
/* --------------------------------------------------------------- */

/* =========================  constants  ========================= */
#define META_RAW          "meta.raw"
#define META_SUBDIR       "metaruns"
#define CURSE_MENU_LINES  3

/* =========================  globals  =========================== */
static metarun *metaruns    = NULL;
static s16b     metarun_max = 0;
static s16b     current_run = 0;
bool            metarun_created = false;

/* ==================  tiny local helpers  ======================= */
static int rng_int(int max) { return max ? (int)(rand() % max) : 0; }

static int popcount32(u32b value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);
        count++;
    }
    return count;
}

static u32b compute_metarun_score(const metarun *m)
{
    if (!m) return 0;

    s32b total = (s32b)m->best_run_score;
    total += (s32b)m->silmarils * 120;
    total -= (s32b)m->deaths * 60;
    total += (s32b)60 * popcount32(m->completed_quests);
    total -= (s32b)100 * popcount32(m->banned_oaths);

    if (total < 0) total = 0;
    return (u32b)total;
}

static void refresh_current_metar_score(void)
{
    if (!metaruns) return;
    if (current_run < 0 || current_run >= metarun_max) return;

    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
    metaruns[current_run].best_run_score = metar.best_run_score;
}

static int compare_metarun_indices(const void *a, const void *b)
{
    const s16b ia = *(const s16b *)a;
    const s16b ib = *(const s16b *)b;

    if (!metaruns) return 0;

    const metarun *ma = &metaruns[ia];
    const metarun *mb = &metaruns[ib];

    if (ma->score != mb->score)
        return (ma->score < mb->score) ? 1 : -1;
    if (ma->last_played != mb->last_played)
        return (ma->last_played < mb->last_played) ? 1 : -1;
    if (ma->id < mb->id) return -1;
    if (ma->id > mb->id) return 1;
    return 0;
}

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
    log_info("Initializing new metarun with default values");
    memset(m, 0, sizeof(*m));
    m->id          = 1;
    m->last_played = (u32b)time(NULL);
    m->curses_lo   = 0;
    m->curses_hi   = 0;
    m->curses_seen = 0;   
    m->deaths      = 0;
    m->silmarils   = 0;
    
    /* Initialize persistent settings with defaults */
    for (int i = 0; i < 8; i++) {
        m->persistent_options[i] = 0;
    }
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        m->persistent_window_flags[i] = 0;
    }
    m->persistent_delay_factor = 5;      /* Default delay factor */
    m->persistent_hitpoint_warn = 3;     /* Default hitpoint warning */
    m->persistent_options_initialized = 0; /* Mark as not initialized yet */
    
    /* Initialize quest tracking */
    m->completed_quests = 0;             /* No quests completed initially */
    
    /* Initialize oath system tracking */
    m->unlocked_oaths = 0;               /* No oaths unlocked initially */
    m->banned_oaths = 0;                 /* No oaths banned initially */
    m->max_difficulty_reached = 0;       /* Start with easiest difficulty */
    
    for (int i = 0; i < 12; i++) {       /* Updated to 12 due to new field */
        m->quest_reserved[i] = 0;
    }

    m->score = compute_metarun_score(m);

    log_debug("After init: curses_seen = 0x%08X", m->curses_seen);
}

static bool ensure_default_metarun_slot(const char *reason)
{
    if (metarun_max > 0 && metaruns) return false;

    if (metaruns) {
        FREE(metaruns);
        metaruns = NULL;
    }

    if (reason && *reason)
        log_warn("Metarun recovery triggered (%s); creating default entry", reason);
    else
        log_warn("Metarun recovery triggered; creating default entry");

    metarun_max = 1;
    metaruns = C_ZNEW(metarun_max, metarun);
    reset_defaults(&metaruns[0]);
    metarun_created = true;

    return true;
}

/* Apply initial curses based on difficulty level (runtype) */
static void apply_difficulty_curses(metarun *m)
{
    if (!runtype_info) return; /* runtype data not loaded yet */
    if (m->type >= z_info->rt_max) return; /* invalid runtype */

    runtype_type *rt = &runtype_info[m->type];
    
    log_info("Applying curses for runtype %d (%s)", m->type, rt->name);
    
    /* Apply curses based on runtype configuration */
    if (rt->start_curses)
    {
        for (int curse_id = 0; curse_id < 32; curse_id++)
        {
            if (rt->start_curses & (1UL << curse_id))
            {
                byte stacks = rt->curse_stacks[curse_id];
                if (stacks > 0)
                {
                    CURSE_SET(curse_id, stacks);
                    CURSE_SEEN_SET(curse_id);
                    log_debug("Applied %d stacks of curse %d from runtype", stacks, curse_id);
                }
            }
        }
    }
}

/* ensure directory apex/metaruns/NNNNNNNN exists */
static void ensure_run_dir(const metarun *m)
{
    char dir[1024];
    path_build(dir, sizeof dir, ANGBAND_DIR_APEX, META_SUBDIR); MKDIR(dir);
    strnfmt(dir, sizeof dir, "%s/%08u", META_SUBDIR, (unsigned)m->id);
    path_build(dir, sizeof dir, ANGBAND_DIR_APEX, dir);         MKDIR(dir);
}

static bool sync_current_metarun_slot(bool stamp_time)
{
    if (!metaruns || current_run < 0 || current_run >= metarun_max) {
        return false;
    }

    if (stamp_time) {
        metar.last_played = (u32b)time(NULL);
    }

    metaruns[current_run] = metar;
    return true;
}

/* forward declarations */
static void start_new_metarun(void);
static void choose_difficulty_menu(void);
static void print_heading_fade(cptr title, byte final_attr);
static bool print_paragraph_fade(cptr txt, byte final_attr, int row);

/* ----------------------------------------------------------------
 * Flush the live 2-bit counters into the on-disk words
 * ---------------------------------------------------------------- */
static void curses_pack_words(void)
{
    u32b lo = 0, hi = 0;

    for (int id = 0; id < 32; id++) {
        u32b cnt = CURSE_GET(id) & 0x3;        /* 0–3 stacks */
        if (id < 16)
            lo |= cnt << (id * 2);             /* bits 0,2,4 … 30 */
        else
            hi |= cnt << ((id - 16) * 2);
    }

    metar.curses_lo = lo;
    metar.curses_hi = hi;
}

/* ----------------------------------------------------------------
 * Expand the on-disk words into the live 2-bit counters
 * (call straight after reading the struct)
 * ---------------------------------------------------------------- */
static void curses_unpack_words(void)
{
    log_trace("curses_unpack_words: before - curses_seen=0x%08X", metar.curses_seen);
    
    u32b lo = metar.curses_lo;          
    u32b hi = metar.curses_hi;

    for (int id = 0; id < 32; id++) {
        u32b cnt = (id < 16)
                 ? (lo >> (id * 2)) & 0x3    
                 : (hi >> ((id - 16) * 2)) & 0x3;

        CURSE_SET(id, (byte)cnt);            
    }
    
    log_trace("curses_unpack_words: after - curses_seen=0x%08X", metar.curses_seen);
}


/* =======================  load / save  ========================= */

/* Check if a file is in the new versioned format */
static bool is_versioned_meta_file(int fd, int file_size)
{
    if (file_size < sizeof(meta_file_header)) return false;

    meta_file_header header;
    fd_seek(fd, 0);
    if (fd_read(fd, (char*)&header, sizeof(header)) != 0) return false;

    /* Check for reasonable version numbers (0-255) and entry count */
    if (header.version_major > 255 || header.version_minor > 255 ||
        header.version_patch > 255 || header.version_extra > 255) return false;

    /* Check if the entry count makes sense with file size */
    size_t payload = file_size - sizeof(meta_file_header);
    if (header.entry_count == 0) {
        if (payload != 0) return false;
    } else {
        if ((payload % header.entry_count) != 0) return false;
        size_t entry_size = payload / header.entry_count;
        if (entry_size != sizeof(metarun) && entry_size != METARUN_V5_SIZE)
            return false;
    }

    log_info("Detected versioned meta file: v%d.%d.%d, %u entries",
             header.version_major, header.version_minor, header.version_patch, header.entry_count);
    return true;
}

/*
 * Clean up old save and score files when starting fresh (no meta.raw exists)
 */
void cleanup_old_game_files(void)
{
    log_info("*** FRESH STARTUP CLEANUP STARTING ***");
    
    /* Use the correct save directory - ANGBAND_DIR_SAVE points to lib/save */
    char save_dir[1024];
    strnfmt(save_dir, sizeof(save_dir), "%s", ANGBAND_DIR_SAVE);
    
    log_trace("Fresh startup: checking save directory: %s", save_dir);
    
    /* Platform-agnostic approach: scan directory for ANY files (except .gitignore and archives) */
    bool has_save_files = false;
    
    #ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile for directory scanning */
    WIN32_FIND_DATA findData;
    char search_path[1024];
    path_build(search_path, sizeof(search_path), save_dir, "*");
    
    HANDLE hFind = FindFirstFile(search_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /* Skip directories and special entries */
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            char* filename = findData.cFileName;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Found a save file! */
            has_save_files = true;
            log_trace("Fresh startup: found save file: %s", filename);
            break;
            
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
    #else
    /* Unix/Linux/macOS: Use POSIX opendir/readdir */
    DIR *dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Found a save file! */
            has_save_files = true;
            log_trace("Fresh startup: found save file: %s", filename);
            break;
        }
        closedir(dir);
    }
    #endif
    
    /* ULTRA FAST EXIT if no save files detected */
    if (!has_save_files) {
        log_info("*** NO SAVE FILES DETECTED - INSTANT FRESH START ***");
        
        /* Quick score file check and removal */
        char score_file[1024];
        path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw");
        
        int score_fd = fd_open(score_file, O_RDONLY);
        if (score_fd >= 0) {
            fd_close(score_fd);
            log_info("*** REMOVING SCORE FILE FOR FRESH START ***");
            
            /* Platform-agnostic file removal using standard C */
            remove(score_file);
        } else {
            log_trace("Fresh startup: no score file found");
        }
        
        log_info("*** INSTANT FRESH STARTUP COMPLETED ***");
        return;  /* INSTANT EXIT - no shell commands needed */
    }
    
    /* Comprehensive cleanup: delete ALL files except .gitignore and archive files using ONLY standard C */
    log_info("*** FOUND SAVE FILES - DELETING ALL NON-ARCHIVE FILES ***");
    
    /* Use ONLY standard C functions - no shell commands for better portability */
    int files_deleted = 0;
    
    #ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile to enumerate and delete */
    WIN32_FIND_DATA cleanupFindData;
    char cleanup_search_path[1024];
    path_build(cleanup_search_path, sizeof(cleanup_search_path), save_dir, "*");
    
    HANDLE hCleanupFind = FindFirstFile(cleanup_search_path, &cleanupFindData);
    if (hCleanupFind != INVALID_HANDLE_VALUE) {
        do {
            /* Skip directories and special entries */
            if (cleanupFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            char* filename = cleanupFindData.cFileName;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Delete this file using standard C */
            char file_path[1024];
            path_build(file_path, sizeof(file_path), save_dir, filename);
            
            if (remove(file_path) == 0) {
                files_deleted++;
                log_trace("Fresh startup: deleted file: %s", filename);
            } else {
                log_trace("Fresh startup: failed to delete: %s", filename);
            }
            
        } while (FindNextFile(hCleanupFind, &cleanupFindData));
        FindClose(hCleanupFind);
    }
    #else
    /* Unix/Linux/macOS: Use opendir/readdir to enumerate and delete */
    dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Delete this file using standard C */
            char file_path[1024];
            path_build(file_path, sizeof(file_path), save_dir, filename);
            
            if (remove(file_path) == 0) {
                files_deleted++;
                log_trace("Fresh startup: deleted file: %s", filename);
            } else {
                log_trace("Fresh startup: failed to delete: %s", filename);
            }
        }
        closedir(dir);
    }
    #endif
    
    if (files_deleted > 0) {
        log_info("*** FRESH STARTUP DELETED %d FILES USING STANDARD C ***", files_deleted);
    } else {
        log_info("*** NO FILES FOUND TO DELETE ***");
    }
    
    /* Score file cleanup */
    char score_file[1024];
    path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw");
    
    int score_fd = fd_open(score_file, O_RDONLY);
    if (score_fd >= 0) {
        fd_close(score_fd);
        log_info("*** REMOVING SCORE FILE FOR FRESH START ***");
        
        /* Platform-agnostic file removal using standard C */
        remove(score_file);
    }
    
    log_info("*** FRESH STARTUP CLEANUP COMPLETED ***");
}

errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    int  fd;

    build_meta_path(fn, sizeof fn, NULL, META_RAW);
    fd = fd_open(fn, O_RDONLY);

    if (fd < 0 && create_if_missing) {
        log_info("Creating new versioned metarun file: %s", fn);
        FILE_TYPE(FILE_TYPE_DATA);
        fd = fd_make(fn, 0644);
        if (fd < 0) return -1;

        /* Write versioned header */
        meta_file_header header;
        header.version_major = METARUN_FILE_VERSION_MAJOR;
        header.version_minor = METARUN_FILE_VERSION_MINOR;
        header.version_patch = METARUN_FILE_VERSION_PATCH;
        header.version_extra = METARUN_FILE_VERSION_EXTRA;
        header.entry_count = 1;

        fd_write(fd, (cptr)&header, sizeof(header));

        metarun seed;
        reset_defaults(&seed);
        seed.score = compute_metarun_score(&seed);
        fd_write(fd, (cptr)&seed, sizeof seed);
        fd_close(fd);
        fd = fd_open(fn, O_RDONLY);
        metarun_created = true;
    }
    else log_info("Loading existing metarun file: %s", fn);
    if (fd < 0) return -1;

    /* Check if this is a versioned file */
    int file_size = fd_file_size(fd);
    bool is_versioned = is_versioned_meta_file(fd, file_size);
    
    const char *recovery_reason = NULL;

    if (is_versioned) {
        /* Load versioned format */
        meta_file_header header;
        fd_seek(fd, 0);
        fd_read(fd, (char*)&header, sizeof(header));

        log_info("Loading versioned meta file v%d.%d.%d with %u entries",
                 header.version_major, header.version_minor, header.version_patch, header.entry_count);

        metarun_max = header.entry_count;
        size_t payload = (file_size >= (int)sizeof(meta_file_header))
                       ? (size_t)file_size - sizeof(meta_file_header)
                       : 0;
        size_t entry_size = (metarun_max > 0)
                          ? (payload / (size_t)metarun_max)
                          : 0;

        if (metarun_max > 0) {
            metaruns = C_ZNEW(metarun_max, metarun);

            if (entry_size == sizeof(metarun)) {
                fd_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
            } else if (entry_size == METARUN_V5_SIZE) {
                typedef struct metarun_v5 {
                    u32b id;
                    byte type;
                    byte deaths;
                    byte silmarils;
                    u32b last_played;
                    u32b curses_lo;
                    u32b curses_hi;
                    u32b curses_seen;
                    u32b persistent_options[8];
                    byte persistent_delay_factor;
                    byte persistent_hitpoint_warn;
                    u32b persistent_window_flags[ANGBAND_TERM_MAX];
                    byte persistent_options_initialized;
                    u32b completed_quests;
                    byte unlocked_oaths;
                    byte banned_oaths;
                    byte max_difficulty_reached;
                    byte quest_reserved[12];
                } metarun_v5;

                metarun_v5 *legacy = C_ZNEW(metarun_max, metarun_v5);
                fd_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v5));

                for (s16b i = 0; i < metarun_max; i++) {
                    metaruns[i].id = legacy[i].id;
                    metaruns[i].type = legacy[i].type;
                    metaruns[i].deaths = legacy[i].deaths;
                    metaruns[i].silmarils = legacy[i].silmarils;
                    metaruns[i].last_played = legacy[i].last_played;
                    metaruns[i].curses_lo = legacy[i].curses_lo;
                    metaruns[i].curses_hi = legacy[i].curses_hi;
                    metaruns[i].curses_seen = legacy[i].curses_seen;
                    C_COPY(metaruns[i].persistent_options, legacy[i].persistent_options, 8, u32b);
                    metaruns[i].persistent_delay_factor = legacy[i].persistent_delay_factor;
                    metaruns[i].persistent_hitpoint_warn = legacy[i].persistent_hitpoint_warn;
                    C_COPY(metaruns[i].persistent_window_flags, legacy[i].persistent_window_flags, ANGBAND_TERM_MAX, u32b);
                    metaruns[i].persistent_options_initialized = legacy[i].persistent_options_initialized;
                    metaruns[i].completed_quests = legacy[i].completed_quests;
                    metaruns[i].unlocked_oaths = legacy[i].unlocked_oaths;
                    metaruns[i].banned_oaths = legacy[i].banned_oaths;
                    metaruns[i].max_difficulty_reached = legacy[i].max_difficulty_reached;
                    C_COPY(metaruns[i].quest_reserved, legacy[i].quest_reserved, 12, byte);
                    metaruns[i].best_run_score = 0;
                }

                FREE(legacy);
            } else {
                recovery_reason = "versioned meta.raw had unexpected entry size";
                log_warn("Versioned meta file entry size %zu did not match known formats", entry_size);
                FREE(metaruns);
                metaruns = NULL;
                metarun_max = 0;
            }

            if (metaruns) {
                for (s16b i = 0; i < metarun_max; i++) {
                    metaruns[i].score = compute_metarun_score(&metaruns[i]);
                }
            }
        } else {
            recovery_reason = "versioned meta.raw reported zero entries";
            log_warn("Versioned meta file had no entries; scheduling default metarun creation");
        }
    }
    else {
        /* Legacy format detection and conversion */
        s16b old_count = (s16b)(file_size / sizeof(metarun_old));
        s16b new_count = (s16b)(file_size / sizeof(metarun));
        
        bool is_old_format = false;
        
        log_info("File size: %d bytes, old struct: %d bytes, new struct: %d bytes", 
                 file_size, (int)sizeof(metarun_old), (int)sizeof(metarun));
        log_info("Old count would be: %d, new count would be: %d", old_count, new_count);
        
        /* If the file is exactly divisible by new format size, it's new format */
        if ((file_size % sizeof(metarun)) == 0) {
            is_old_format = false;
            metarun_max = new_count;
            log_info("Loading legacy new format metarun file with %d entries", new_count);
        } else if ((file_size % sizeof(metarun_old)) == 0) {
            /* Only consider old format if new format doesn't fit exactly */
            is_old_format = true;
            metarun_max = old_count;
            log_info("Detected legacy old format metarun file, converting %d entries", old_count);
        } else {
            /* File size doesn't match either format - default to new format with truncation */
            is_old_format = false;
            metarun_max = new_count;  /* This will truncate partial entries */
            log_info("File size doesn't match known formats exactly, assuming new format with %d complete entries", new_count);
        }

        if (metarun_max > 0)
            metaruns = C_ZNEW(metarun_max, metarun);

        if (is_old_format && metarun_max > 0) {
            /* Load old format and convert to new format */
            metarun_old *old_data = C_ZNEW(metarun_max, metarun_old);
            fd_seek(fd, 0);
            fd_read(fd, (char*)old_data, metarun_max * sizeof(metarun_old));

            /* Convert each old entry to new format */
                for (s16b i = 0; i < metarun_max; i++) {
                    /* Copy old fields */
                    metaruns[i].id = old_data[i].id;
                    metaruns[i].type = old_data[i].type;
                    metaruns[i].deaths = old_data[i].deaths;
                    metaruns[i].silmarils = old_data[i].silmarils;
                    metaruns[i].last_played = old_data[i].last_played;
                    metaruns[i].curses_lo = old_data[i].curses_lo;
                    metaruns[i].curses_hi = old_data[i].curses_hi;
                    metaruns[i].curses_seen = old_data[i].curses_seen;
                
                /* Initialize new persistent settings fields with defaults */
                for (int j = 0; j < 8; j++) {
                    metaruns[i].persistent_options[j] = 0;
                }
                for (int j = 0; j < ANGBAND_TERM_MAX; j++) {
                    metaruns[i].persistent_window_flags[j] = 0;
                }
                metaruns[i].persistent_delay_factor = 5;
                metaruns[i].persistent_hitpoint_warn = 3;
                metaruns[i].persistent_options_initialized = 0;
                
                    /* Initialize quest tracking fields for old format */
                    metaruns[i].completed_quests = 0;
                    for (int j = 0; j < (int)N_ELEMENTS(metaruns[i].quest_reserved); j++) {
                        metaruns[i].quest_reserved[j] = 0;
                    }

                    metaruns[i].best_run_score = 0;
                    metaruns[i].score = compute_metarun_score(&metaruns[i]);
                }

                FREE(old_data);
                log_info("Successfully converted legacy old format to new format");
        } else if (!is_old_format && metarun_max > 0) {
            /* Load new format directly */
            fd_seek(fd, 0);
            fd_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
            log_info("Loaded legacy new format entries");

            for (s16b i = 0; i < metarun_max; i++) {
                metaruns[i].score = compute_metarun_score(&metaruns[i]);
            }
        } else {
            recovery_reason = "legacy meta.raw truncated without any complete entries";
            log_warn("Legacy meta file did not contain any complete entries; scheduling default metarun creation");
        }
    }

    fd_close(fd);

    bool seeded_default = false;
    if (metarun_max <= 0 || !metaruns) {
        seeded_default = ensure_default_metarun_slot(recovery_reason);
    }

    /* choose current run */
    u32b latest = 0;
    current_run = -1;  /* Initialize to invalid value so any valid entry will be selected */

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            log_debug("Metarun %d: id=%u, last_played=%u, deaths=%u, silmarils=%u",
                      i, metaruns[i].id, metaruns[i].last_played, metaruns[i].deaths, metaruns[i].silmarils);

            if (metaruns[i].last_played > latest ||
                (metaruns[i].last_played == latest && i > current_run))
            {
                latest      = metaruns[i].last_played;
                current_run = i;
                log_debug("Selected metarun %d as current (last_played=%u)", i, latest);
            }
        }
    }

    if (current_run < 0 || current_run >= metarun_max) {
        if (ensure_default_metarun_slot("no valid metarun could be selected")) {
            seeded_default = true;
        }
        log_info("No valid metarun found, defaulting to entry 0");
        current_run = 0;
    }

    if (metarun_max <= 0 || !metaruns) {
        if (ensure_default_metarun_slot("metarun array unavailable before final selection")) {
            seeded_default = true;
        }
    }

    if (seeded_default) {
        log_info("Metarun loader seeded a default entry to recover from a corrupt or empty meta.raw");
    }

    metar = metaruns[current_run];
    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
    metaruns[current_run].best_run_score = metar.best_run_score;
    log_debug("Final current_run=%d, metar: id=%u, deaths=%u, silmarils=%u",
              current_run, metar.id, metar.deaths, metar.silmarils);

    /* ensure its per-run directory exists */
    ensure_run_dir(&metar);
    curses_unpack_words();    /* NEW: expand words into live table */
    
    /* Apply difficulty curses only if this is a newly created metarun */
    if (metarun_created)
    {
        apply_difficulty_curses(&metar);
        save_metaruns(); /* persist the changes */
    }
    
    log_debug("Loaded metarun %d with %d silmarils, %d deaths", metar.id, metar.silmarils, metar.deaths);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Safely write the meta-run array.  Bail out if the indices look     *
 *  wrong – avoids dereferencing a freed/reallocated block.           *
 * ------------------------------------------------------------------ */
static errr backup_file(const char *filepath)
{
    static u32b last_backup_time = 0;
    static char last_backed_up_file[1024] = "";
    u32b current_time = (u32b)time(NULL);
    
    /* Throttle backups: only create backup if 
     * 1. This is a different file than last time, OR
     * 2. More than 300 seconds (5 minutes) have passed since last backup of this file
     */
    if (my_stricmp(last_backed_up_file, filepath) != 0) {
        /* Different file - always backup */
        log_info("backup_file: backing up different file: %s", filepath);
    } else if (current_time - last_backup_time >= 300) {
        /* Same file but enough time has passed (5 minutes instead of 1 minute) */
        log_info("backup_file: backing up %s after %u seconds", filepath, current_time - last_backup_time);
    } else {
        /* Same file, recent backup - skip */
        log_trace("backup_file: skipping backup of %s (last backup %u seconds ago)", 
                  filepath, current_time - last_backup_time);
        return 0;
    }
    
    /* Check if original file exists */
    int fd_src = fd_open(filepath, O_RDONLY);
    if (fd_src < 0) {
        /* Original file doesn't exist, no backup needed */
        log_info("backup_file: original file %s doesn't exist, no backup needed", filepath);
        return 0;
    }
    
    /* Get file size */
    int file_size = fd_file_size(fd_src);
    if (file_size <= 0) {
        log_info("backup_file: original file %s is empty, no backup needed", filepath);
        fd_close(fd_src);
        return 0;
    }
    
    log_info("backup_file: creating backup for %s (size: %d bytes)", filepath, file_size);
    
    /* Read original file */
    char *buffer = C_ZNEW(file_size, char);
    if (!buffer) {
        fd_close(fd_src);
        return -1;
    }
    
    if (fd_read(fd_src, buffer, file_size) != 0) {
        FREE(buffer);
        fd_close(fd_src);
        return -1;
    }
    fd_close(fd_src);
    
    /* Optimize backup rotation: Only do full rotation once per session/day
     * For frequent saves, just overwrite .bak1 */
    char backup_path1[1024], backup_path2[1024], backup_path3[1024];
    strnfmt(backup_path1, sizeof(backup_path1), "%s.bak1", filepath);
    strnfmt(backup_path2, sizeof(backup_path2), "%s.bak2", filepath);
    strnfmt(backup_path3, sizeof(backup_path3), "%s.bak3", filepath);
    
    /* Check if this is the first backup of the day (roughly) */
    bool should_rotate = false;
    int fd_test1 = fd_open(backup_path1, O_RDONLY);
    if (fd_test1 >= 0) {
        /* Check if bak1 is old enough to warrant rotation (use simple time check) */
        /* If we created a backup within the last hour, don't rotate */
        if (current_time - last_backup_time >= 3600) {  /* 1 hour */
            should_rotate = true;
            log_info("backup_file: enough time passed since last backup, will rotate backups");
        }
        fd_close(fd_test1);
    } else {
        /* No bak1 exists, create fresh backup */
        should_rotate = false;
        log_info("backup_file: no existing backup, creating fresh bak1");
    }
    
    if (should_rotate) {
        log_info("backup_file: rotating backups for %s", filepath);
        
        /* Rotate: bak2 -> bak3, bak1 -> bak2, current -> bak1 */
        fd_kill(backup_path3);                    /* Remove oldest */
        log_debug("backup_file: removed old bak3");
        
        /* Move bak2 to bak3 (if bak2 exists) */
        int fd_test2 = fd_open(backup_path2, O_RDONLY);
        if (fd_test2 >= 0) {
            fd_close(fd_test2);
            log_debug("backup_file: moving bak2 to bak3");
            if (fd_move(backup_path2, backup_path3) != 0) {
                log_debug("backup_file: failed to move bak2 to bak3");
            }
        }
        
        /* Move bak1 to bak2 (if bak1 exists) */
        fd_test1 = fd_open(backup_path1, O_RDONLY);
        if (fd_test1 >= 0) {
            fd_close(fd_test1);
            log_debug("backup_file: moving bak1 to bak2");
            if (fd_move(backup_path1, backup_path2) != 0) {
                log_debug("backup_file: failed to move bak1 to bak2");
            }
        }
    } else {
        /* Just overwrite bak1 for frequent saves */
        log_debug("backup_file: overwriting existing bak1 (frequent save)");
        fd_kill(backup_path1);
    }
    
    /* Create new bak1 from current file */
    log_info("backup_file: creating new bak1 from current file (size: %d)", file_size);
    int fd_dst = fd_make(backup_path1, 0644);
    if (fd_dst < 0) {
        FREE(buffer);
        return -1;
    }
    
    errr result = fd_write(fd_dst, buffer, file_size);
    fd_close(fd_dst);
    FREE(buffer);
    
    if (result == 0) {
        log_info("backup_file: successfully created backup for %s", filepath);
        /* Update throttling variables only on successful backup */
        last_backup_time = current_time;
        my_strcpy(last_backed_up_file, filepath, sizeof(last_backed_up_file));
    } else {
        log_error("backup_file: failed to write bak1 for %s", filepath);
    }
    
    return result;
}

errr save_metaruns(void)
{
    static u32b last_save_time = 0;
    u32b current_time = (u32b)time(NULL);
    
    /* Log save frequency tracking */
    if (last_save_time > 0) {
        u32b time_since_last = current_time - last_save_time;
        log_info("save_metaruns() called again after %u seconds", time_since_last);
    } else {
        log_info("save_metaruns() called for the first time this session");
    }
    last_save_time = current_time;

    curses_pack_words();      /* NEW: ensure words hold 2-bit data */
    refresh_current_metar_score();

    char fn[1024];
    build_meta_path(fn, sizeof fn, NULL, META_RAW);

    /* Create backup before saving */
    backup_file(fn);

    log_debug("Before save: current_run=%d, metar: id=%u, deaths=%u, silmarils=%u", 
              current_run, metar.id, metar.deaths, metar.silmarils);
              
    metar.last_played      = current_time;
    metaruns[current_run] = metar;            /* safe: array is valid */
    
    log_debug("After updating array: metaruns[%d]: id=%u, deaths=%u, silmarils=%u", 
              current_run, metaruns[current_run].id, metaruns[current_run].deaths, metaruns[current_run].silmarils);

    /* After backup is created in backup_file(), remove the original so fd_make can succeed */
    fd_kill(fn);
    
    /* Write using the new versioned format */
    int fd = fd_make(fn, 0644);
    if (fd < 0) {
        log_info("Failed to create metarun file for writing");
        return -1;
    }

    /* Write version header first */
    meta_file_header header;
    header.version_major = METARUN_FILE_VERSION_MAJOR;
    header.version_minor = METARUN_FILE_VERSION_MINOR;
    header.version_patch = METARUN_FILE_VERSION_PATCH;
    header.version_extra = METARUN_FILE_VERSION_EXTRA;
    header.entry_count = metarun_max;
    
    errr result = fd_write(fd, (cptr)&header, sizeof(header));
    if (result != 0) {
        fd_close(fd);
        log_info("Failed to write metarun header to file");
        return -1;
    }

    /* Write metarun data */
    int bytes_to_write = metarun_max * sizeof(metarun);
    result = fd_write(fd, (cptr)metaruns, bytes_to_write);
    fd_close(fd);
    
    if (result != 0) {
        log_info("Failed to write metarun data to file");
        return -1;
    }
    
    log_info("Metarun data saved successfully (%d bytes, %d entries)", bytes_to_write, metarun_max);

    return 0;
}

int any_curse_flag_active(u32b flag)
{
    /* Intended for CUR flags such as CUR_NOCHOICE. */
    return (curse_flag_count_cur(flag) > 0);
}

/* ---------------------------------------------------------------
 * Simple counters used by other modules (no UI side-effects)
 * ------------------------------------------------------------- */
void metarun_increment_deaths(void)
{
    /* Clamp to byte range; defer saving/UI to caller */
    if (metar.deaths >= 255) return;

    metar.deaths++;

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_increment_deaths: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
}

void metarun_gain_silmarils(byte n)
{
    if (!n) return;
    int total = (int)metar.silmarils + (int)n;
    if (total > 255) total = 255;
    if (total < 0) total = 0;
    metar.silmarils = (byte)total;
    refresh_current_metar_score();

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_gain_silmarils: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
}

/* ---------------------------------------------------------------
 * Persistent Settings Management
 * ------------------------------------------------------------- */

/*
 * Save current game options to the metarun persistent settings
 */
void metarun_save_persistent_settings(void)
{
    log_info("Saving persistent settings to metarun");
    
    /* Save options */
    for (int i = 0; i < 8; i++) {
        metar.persistent_options[i] = 0;
    }
    
    /* Pack options into the persistent storage */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;
        
        if (word_idx < 8 && option_text[i] && op_ptr->opt[i]) {
            metar.persistent_options[word_idx] |= (1UL << bit_idx);
        }
    }
    
    /* Save special settings */
    metar.persistent_delay_factor = op_ptr->delay_factor;
    metar.persistent_hitpoint_warn = op_ptr->hitpoint_warn;
    
    /* Save window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        metar.persistent_window_flags[i] = op_ptr->window_flag[i];
    }
    
    /* Mark as initialized */
    metar.persistent_options_initialized = 1;
    
    /* Save the metarun data */
    save_metaruns();
    
    log_info("Persistent settings saved successfully");
}

/*
 * Load metarun persistent settings to current game options
 */
void metarun_load_persistent_settings(void)
{
    /* Only load if settings have been previously saved */
    if (!metar.persistent_options_initialized) {
        log_info("No persistent settings found, using defaults");
        return;
    }
    
    log_info("Loading persistent settings from metarun");
    
    /* Load options */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;
        
        if (word_idx < 8 && option_text[i]) {
            op_ptr->opt[i] = (metar.persistent_options[word_idx] & (1UL << bit_idx)) != 0;
        }
    }
    
    /* Load special settings */
    op_ptr->delay_factor = metar.persistent_delay_factor;
    op_ptr->hitpoint_warn = metar.persistent_hitpoint_warn;
    
    /* Load window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        op_ptr->window_flag[i] = metar.persistent_window_flags[i];
    }
    
    log_info("Persistent settings loaded successfully");
}

/* ---------------------------------------------------------------
 * Pick a curse at random, respecting weights, stacks, caps,
 * and the RHF_CURSE tail-lift and exclusion of most weighted curses.
 * ------------------------------------------------------------- */
static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->phouse].flags & RHF_CURSE);

    /* Pass 1 – find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 – sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;           /* cap reached */

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

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
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rng_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (cu_info[idx].max_stacks &&
        CURSE_GET(idx) >= cu_info[idx].max_stacks)
    {
        log_debug("Curse %d (%s) already at max stacks", idx, cu_name + cu_info[idx].name);
        return;
    }

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

int menu_choose_one_curse(int n)
{
    /* if any active curse has the "no‐choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES], sel;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }
            
            byte cap = cu_info[pick[i]].max_stacks;
            if (cap && CURSE_GET(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    screen_save();  Term_clear();
    
    /* Fade in the title */
    char str[60];
    const char* seq[] = { "a", "the second", "the third" };
    strnfmt(str, sizeof(str), "Dark powers demand their price - choose %s curse:", seq[n]);
    print_heading_fade(str, TERM_YELLOW);

    /* dynamic vertical layout – ask util.c to count wrapped lines   */
    int row = 4;                                     /* first free row */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 2;                   /* full width     */

    /* Show each curse one by one with fade-in effect */
    bool fast_forward = false;
    
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[pick[i]];
        char name_buf[128];
        strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);
        
        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);
        
#ifdef DEBUG_CURSES
        const char *pow = cu_text + cu->power;
        int need_pow_lines = 0;
        if (*pow) {
            need_pow_lines = count_wrapped_lines(pow, text_out_wrap, 4);
        }
#endif

        /* Fade in all text for this curse simultaneously */
        const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE };
        const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

        for (int s = 0; s < steps && !fast_forward; s++)
        {
            /* Check for ESC key to skip fade */
            char ch;
            if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
            {
                fast_forward = true;
                break;
            }

            /* Name line */
            c_put_str(s == steps - 1 ? TERM_L_RED : fade_cols[s], name_buf, row, 2);
            
            /* Poem text */
            Term_gotoxy(4, row + 2);
            text_out_c(s == steps - 1 ? TERM_SLATE : fade_cols[s], txt);

#ifdef DEBUG_CURSES
            /* Power text if present */
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(s == steps - 1 ? TERM_L_RED : fade_cols[s], pow);
            }
#endif
            
            Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, 200);
        }

        /* If fade was interrupted, show final state immediately */
        if (fast_forward) {
            c_put_str(TERM_L_RED, name_buf, row, 2);
            Term_gotoxy(4, row + 2);
            text_out_c(TERM_SLATE, txt);

#ifdef DEBUG_CURSES
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(TERM_L_RED, pow);
            }
#endif
        }

        /* Move to next curse position */
#ifdef DEBUG_CURSES
        if (*pow)
            row += need_lines + need_pow_lines + 3;
        else
#endif
            row += need_lines + 3;

        /* 1 second delay between curses (except for the last one) */
        if (i < CURSE_MENU_LINES - 1) {
            Term_xtra(TERM_XTRA_DELAY, 1000);
        }
    }

    /* Show the prompt immediately without fade */
    c_put_str(TERM_L_DARK, "Arrows to navigate     Space/Enter Accept     a/b/c Select", row + 1, 2);
    
    /* Menu navigation variables */
    int highlight = 0;  /* Currently highlighted option (0, 1, 2) */
    bool menu_done = false;
    int option_rows[CURSE_MENU_LINES];  /* Store the row for each option */
    
    /* Calculate row positions for each option */
    int calc_row = 4;
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        option_rows[i] = calc_row;
        curse_type *cu = &cu_info[pick[i]];
        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);
        calc_row += need_lines + 3;
    }
    
    while (!menu_done) {
        /* Ensure text output settings are consistent */
        text_out_hook = text_out_to_screen;
        text_out_wrap = Term->wid - 2;
        
        /* Update highlight display for each option */
        for (int i = 0; i < CURSE_MENU_LINES; i++) {
            curse_type *cu = &cu_info[pick[i]];
            char name_buf[128];
            strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);
            
            /* Clear the line first to remove any previous highlighting */
            Term_erase(2, option_rows[i], strlen(name_buf));
            
            /* Display the option with highlighting */
            if (i == highlight) {
                c_put_str(TERM_RED, name_buf, option_rows[i], 2);     /* Highlighted - red */
            } else {
                c_put_str(TERM_L_RED, name_buf, option_rows[i], 2);   /* Normal - light red */
            }
        }
        
        /* Position cursor at the end of the highlighted option text */
        curse_type *highlighted_cu = &cu_info[pick[highlight]];
        char highlighted_name_buf[128];
        strnfmt(highlighted_name_buf, sizeof highlighted_name_buf, "%c) %s", 'a'+highlight, cu_name + highlighted_cu->name);
        int cursor_col = 2 + strlen(highlighted_name_buf);
        Term_gotoxy(cursor_col, option_rows[highlight]);
        Term_fresh();
        char key = inkey();
        
        /* Handle input */
        if (key >= 'a' && key < 'a' + CURSE_MENU_LINES) {
            /* Letter shortcuts */
            sel = key - 'a';
            menu_done = true;
        }
        else if (key >= 'A' && key < 'A' + CURSE_MENU_LINES) {
            /* Capital letter shortcuts */
            sel = key - 'A';
            menu_done = true;
        }
        else if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            /* Enter, Space, or numpad 6 - select current highlight */
            sel = highlight;
            menu_done = true;
        }
        else if (key == '8' || key == 'k') {
            /* Up navigation */
            highlight = (highlight + CURSE_MENU_LINES - 1) % CURSE_MENU_LINES;
        }
        else if (key == '2' || key == 'j') {
            /* Down navigation */
            highlight = (highlight + 1) % CURSE_MENU_LINES;
        }
        else if (key == ESCAPE) {
            /* Escape - default to first option */
            sel = 0;
            menu_done = true;
        }
    }
    screen_load();
    return pick[sel];
}


/* ------------------------------------------------------------------ *
 *  Debug helper – wipe every active curse for the current meta-run.  *
 * ------------------------------------------------------------------ */
void metarun_clear_all_curses(void)
{
    log_info("Clearing all curses for current metarun");
    metar.curses_lo = 0;
    metar.curses_hi = 0;
    metar.curses_seen = 0;        
    save_metaruns();
}

/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 *  NOTE: save_metaruns() comes **after** check_run_end() so that     *
 *  any realloc in start_new_metarun() has already finished.          *
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 * ------------------------------------------------------------------ */
/*
 * Metarun narrative & exit logic - refactor **v4** (30 Jul 2025)
 * ------------------------------------------------------------------
 *  ✧ Re‑orders the sequence so NOTHING is overwritten:
 *      0. Escape‑curse chooser (UI)  → clears screen once finished.
 *      1. Chosen‑curse line(s).
 *      2. Victory banner & Silmaril count paragraph.
 *      3. Temptation of Treachery (escalating 1‑3 lines).
 *      4. Story Fragment (depends on Silmarils & Treachery flag).
 *      5. Echoes of Kinslaying (escalating 1‑3 lines)
 *      6. Final pause, then deferred side‑effects.
 *
 *  ✧ `choose_escape_curses_ui()` now **returns** the indices chosen and
 *    does NOT leave the menu clutter on screen. We re‑render the
 *    “The curse of X binds your fate.” lines after a clean clear.
 *
 *  ✧ Adds `print_story_fragment()` – a short narrative bridge keyed off
 *    Silmaril count (1‑3) and whether treachery was overcome.
 *
 *  ✧ Tested matrix: {treachery flag × kinslayer flag × silmarils (1‑3)}
 *    All show in the intended order with no garbled overlaps.
 */

/********************  Enhanced UI helpers with fade-in effects  ***************************/

static void print_heading_fade(cptr title, byte final_attr)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    int w, h; 
    Term_get_size(&w, &h);
    
    // Center the heading
    int title_len = strlen(title);
    int start_col = (w - title_len) / 2;
    if (start_col < 1) start_col = 1;
    
    for (int s = 0; s < steps; s++)
    {
        c_prt(fade_cols[s], title, 2, start_col);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 150);
    }
    Term_xtra(TERM_XTRA_DELAY, 500); // Extra pause after heading
}

static bool print_paragraph_fade(cptr txt, byte final_attr, int row)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    
    text_out_hook   = text_out_to_screen;
    text_out_indent = 2;
    text_out_wrap   = Term->wid - 4;

    for (int s = 0; s < steps; s++)
    {
        // Check for ESC key to skip fade
        char ch;
        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            // Show final state immediately and return interrupted status
            Term_gotoxy(2, row);
            text_out_c(final_attr, txt);
            text_out("\n");
            Term_fresh();
            return false;
        }
        
        Term_gotoxy(2, row);
        text_out_c(fade_cols[s], txt);
        text_out("\n");
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }
    
    Term_xtra(TERM_XTRA_DELAY, 1000); // Pause after paragraph
    return true;
}

static void print_paragraph(cptr txt, byte attr)
{
    text_out_hook   = text_out_to_screen;
    text_out_indent = 1;
    text_out_wrap   = Term->wid - 2;

    Term_addstr(0, attr, "");
    text_out_c(attr, txt);
    text_out("\n");
}

static void wait_for_keypress_with_prompt(cptr prompt)
{
    int w, h;
    Term_get_size(&w, &h);
    
    // Clear bottom line and show prompt
    Term_erase(0, h - 1, w);
    c_prt(TERM_L_WHITE, prompt ? prompt : "[Press any key to continue]", h - 1, 2);
    Term_fresh();
    
    (void)inkey();
    
    // Clear the prompt line
    Term_erase(0, h - 1, w);
}

static cptr curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;
    if (strncmp(raw, "Curse of ", 8) == 0) raw += 8;
    return raw;
}

/****************  Escape‑curse chooser (clean version) ************/

/*
 * Presents the menu *n* times (or once if CUR_NOCHOICE). Returns the
 * number of curses actually chosen and fills `out` with their indices.
 * The display is cleared afterwards so we can start narrative fresh.
 */
int choose_escape_curses_ui(int n, int out[3])
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;
    bool fast_forward = false;

    /* Display intro with fade-in effect */
    screen_save();
    Term_clear();
    
    print_heading_fade("The Valar's Judgment", TERM_L_BLUE);
    
    char intro_text[512];
    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : "three",
            (n == 1) ? "" : "s");
    
    if (!print_paragraph_fade(intro_text, TERM_L_WHITE, 4))
        fast_forward = true;
    
    wait_for_keypress_with_prompt("[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay side‑effect */
        if (taken < 3) out[taken++] = idx;
    }

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();
    
    /* Restore screen state to fix character_icky imbalance */
    screen_load();
    
    /* Avoid unused variable warning */
    (void)fast_forward;
    
    return taken;
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific curse message with fade-in, waits 3 seconds,
 * then shows the permanent consequence message and curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    bool fast_forward = false;
    
    /* Display curse message with fade-in effect */
    screen_save();
    Term_clear();
    
    /* Add Tolkien-style heading */
    print_heading_fade("The Sundering of Sacred Vows", TERM_L_RED);
    
    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);
    
    /* Add empty line before E: text */
    Term_putstr(2, 4, -1, TERM_SLATE, "");
    
    /* Show only the permanent message (E: field) with fade */
    if (perm_msg && perm_msg[0]) {
        if (!print_paragraph_fade(perm_msg, TERM_L_RED, 5))
            fast_forward = true;
    } else {
        if (!print_paragraph_fade("Your oath is forever broken in this age.", TERM_L_RED, 5))
            fast_forward = true;
    }
    
    /* Hold the message for 3 seconds if not fast-forwarded */
    if (!fast_forward) {
        Term_xtra(TERM_XTRA_DELAY, 3000);
    }
    
    /* Add empty line before attention text */
    Term_putstr(2, 8, -1, TERM_SLATE, "");
    
    /* Show Morgoth's attention text with fade in red */
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");
    
    if (!print_paragraph_fade(intro_text, TERM_RED, 9))
        fast_forward = true;
    
    wait_for_keypress_with_prompt("[Press any key to face your judgment]");
    Term_clear();

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    log_debug("Player selected curse %d for oath breaking", idx);
    
    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();
    
    /* Restore screen state */
    screen_load();
    
    /* Avoid unused variable warning */
    (void)fast_forward;
    
    return idx;
}

/* ------------------------------------------------------------------ */
/*  Standard “Press any key…” prompts – use enum, not raw strings     */
/* ------------------------------------------------------------------ */
typedef enum {
    PROMPT_CONTINUE_TALE,
    PROMPT_FACE_TEMPTATION,
    PROMPT_CONTINUE_GENERIC,
    PROMPT_FACE_ECHOES,
    PROMPT_CONCLUDE_TALE,
    PROMPT_WITNESS_CONSEQUENCES,
    PROMPT_RETURN_MIDDLE_EARTH
} prompt_t;

static const char *prompt_text[] = {
    "[Press any key to continue your tale]",
    "[Press any key to face temptation]",
    "[Press any key to continue]",
    "[Press any key to face the echoes]",
    "[Press any key to conclude your tale]",
    "[Press any key to witness the consequences]",
    "[Press any key to return to Middle-earth]"
};

static void wait_prompt(prompt_t id) {         /* tiny wrapper */
    wait_for_keypress_with_prompt(prompt_text[id]);
}

/* ------------------------------------------------------------------
 * metarun_update_on_exit() – v5, 30 Jul 2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? gift‑of‑Eru?)
 *   1.  Escape‑curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls – stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1‑3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause → apply deferred effects
 *   7.  Persist silmaril/death counters, check run end, save
 *
 *  All narrative helpers (print_heading(), print_paragraph(),
 *  choose_escape_curses_ui(), kinslayer_try_kill(), etc.) are reused.
 * ------------------------------------------------------------------ */
void metarun_update_on_exit(bool died, bool escaped, byte sil_count)
{
    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d", 
             died ? "true" : "false", escaped ? "true" : "false", sil_count);
             
    /* -------- Lineage flags -------------------------------------- */
    u32b f_house = c_info[p_ptr->phouse].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (f_house | f_race) & RHF_GIFTERU;
    bool allow_treachery = (f_house | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (f_house | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool fast_forward = false; // Track if user wants to skip fade effects

    high_score temp_score;
    int current_score = 0;
    errr score_err = create_score(&temp_score);
    if (score_err == 0) {
        current_score = score_points(&temp_score);
        if (current_score < 0) current_score = 0;
        if ((u32b)current_score > metar.best_run_score) {
            log_debug("Updating best_run_score from %u to %d", (unsigned)metar.best_run_score, current_score);
            metar.best_run_score = (u32b)current_score;
        }
    } else {
        log_warn("create_score() failed during metarun update (err=%d)", score_err);
    }

        /* Treat as a death unless Eru intervenes */
        if (died && !has_gift_eru)
            metarun_increment_deaths();

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    – any path that reaches here counts as a "run end" event  */
    /* ------------------------------------------------------------- */
    if (died)
    {
        log_info("Player died - displaying death narrative");
        /*****  NEW DEATH-NARRATIVE *****/
        screen_save();
        Term_clear();

        /* Pick correct sequence number: 0 when Gift-of-Eru fires,
         * otherwise 1-based death counter that was just incremented. */
        byte target_order = has_gift_eru ? 0 : metar.deaths;

        /* Build a pool of candidate story entries.                    */
        int *pool = C_ZNEW(z_info->st_max, int);
        int pool_sz = 0;
        if (!pool) {
            screen_load();                 /* restore game view            */
            check_run_end();
            save_metaruns();
            FREE(pool);
            return;
        }
        for (int i = 0; i < z_info->st_max && pool; i++) {
            story_type *st = &st_info[i];
            if (!st->name)            continue;                /* unused slot   */
            if (st->st_type != 1)     continue;                /* not “death”   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback – allow any order-0 message if nothing matched.   */
        if (!pool_sz && target_order) {
            for (int i = 0; i < z_info->st_max && pool; i++) {
                story_type *st = &st_info[i];
                if (!st->name || st->st_type != 1) continue;
                if (st->order != 0)   continue;
                if (st->runtypes &&
                   !(st->runtypes & (1u << metar.type))) continue;
                pool[pool_sz++] = i;
            }
        }

        /* Display the chosen fragment with the usual fade-in style.  */
        if (pool_sz) {
            story_type *pick = &st_info[ pool[rng_int(pool_sz)] ];
            cptr title = st_name + pick->name;
            cptr text  = st_text + pick->text;

            print_heading_fade(title, TERM_RED);
            print_paragraph_fade(text, TERM_WHITE, 4);

            char transition_text[256];
            strnfmt(transition_text, sizeof(transition_text),
                    "The hero whose mantle you took has fallen, their tale ends in shadow. "
                    "Yet your spirit returns, for the Valar's trial is not yet complete.");

            if (!fast_forward && !print_paragraph_fade(transition_text, TERM_L_BLUE, 8))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(transition_text, TERM_L_BLUE);
            wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
        }

        screen_load();                 /* restore game view            */
        FREE(pool);
        refresh_current_metar_score();
        check_run_end();
        save_metaruns();
        return;
    }
    else if (!escaped_with_sils) {
        log_debug("Player escaped without Silmarils - no narrative needed");
        refresh_current_metar_score();
        save_metaruns();
        return;                        /* no further narrative needed  */
    }

    /* ------------------------------------------------------------- */
    /*        Enhanced Narrative Path – escaped with ≥1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative", sil_count);
    screen_save();

    /* ============================================================= */
    /* SCENE 1: Escape Curse Selection                              */
    /* ============================================================= */
    int chosen[3] = { -1, -1, -1 };
    int chosen_cnt = choose_escape_curses_ui(sil_count, chosen);

    /* ============================================================= */
    /* SCENE 2: The Binding of Fate                                 */
    /* ============================================================= */
    if (chosen_cnt > 0)
    {
        print_heading_fade("The Binding of Fate", TERM_L_RED);
        
        for (int i = 0; i < chosen_cnt; ++i)
        {
            char buf[128];
            strnfmt(buf, sizeof buf,
                    "The curse of %s binds your fate.",
                    curse_display_name(chosen[i]));
            
            if (!fast_forward && print_paragraph_fade(buf, TERM_RED, 4 + i * 2))
            {
                // Continue with fade effects
            }
            else
            {
                fast_forward = true;
                print_paragraph(buf, TERM_RED);
            }
        }
        
        wait_prompt(PROMPT_CONTINUE_TALE);
        Term_clear();
    }

    /* ============================================================= */
    /* SCENE 3: Victory Declaration                                  */
    /* ============================================================= */
    print_heading_fade("Victory Amid Shadow", TERM_YELLOW);
    
    const char *victory_text;
    switch (sil_count)
    {
        case 1:
            victory_text = "You emerge victorious from darkness, one holy jewel blazing in your grasp. Morgoth's crown is diminished, yet hope is rekindled, though shadow lingers.";
            break;
        case 2:
            victory_text = "You escape triumphant, two Silmarils blazing fiercely in your hands. Morgoth roars in wrath; his pride is wounded deeply. Your spirit exults, yet your heart begins to feel their burning weight.";
            break;
        case 3:
            victory_text = "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Feanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
            break;
        default:
            victory_text = "You have achieved the impossible, claiming more Silmarils than should exist. Reality itself bends before your triumph.";
            break;
    }
    
    if (!fast_forward && !print_paragraph_fade(victory_text, TERM_WHITE, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(victory_text, TERM_WHITE);
    
    if (allow_treachery)
        wait_prompt(PROMPT_FACE_TEMPTATION);
    else
        wait_prompt(PROMPT_CONTINUE_GENERIC);
    Term_clear();

    /* ============================================================= */
    /* SCENE 4: Temptation of Treachery (Enhanced Messages)        */
    /* ============================================================= */
    byte stolen = 0;
    if (allow_treachery)
    {
        static const int pct[3] = { 20, 50, 95 };
        
        /* Enhanced escalating treachery messages */
        static const char *success_msgs[3] = {
            "The first jewel shines brightly, its pure light uncorrupted. You master desire, choosing honor.",
            "The second jewel blazes defiant, temptation growing strong-but once more, you cling to honor.",
            "The third Silmaril's holy flame burns fiercely. Yet against all odds, your will resists corruption."
        };
        
        static const char *failure_msgs[3] = {
            "Greed whispers softly, and you listen. Secretly you withhold the jewel's light, betraying even yourself.",
            "Desire gnaws deeper; you falter, concealing its brilliance in shame, light darkened by your betrayal.",
            "Consumed by lust for its beauty, you claim it secretly, sealing its radiance from all others-a betrayal of all trust."
        };
        
        print_heading_fade("Temptation of Treachery", TERM_L_UMBER);
        
        int current_row = 4;

        for (int i = 0; i < sil_count; ++i)
        {
            bool fail = (rand_int(100) < pct[i]);
            if (fail) stolen++;
            
            const char *tempt_text = fail ? failure_msgs[i] : success_msgs[i];
            
            if (!fast_forward && !print_paragraph_fade(tempt_text, fail ? TERM_RED : TERM_WHITE, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(tempt_text, fail ? TERM_RED : TERM_WHITE);
            
            current_row += 3; // Space for next paragraph
        }

        if (stolen)
        {
            const char *shadow_text = "In shadows your deeds are recorded-tainted victory shall diminish the jewel's blessing.";
            if (!fast_forward && !print_paragraph_fade(shadow_text, TERM_L_DARK, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(shadow_text, TERM_L_DARK);
        }
        
        wait_prompt(PROMPT_CONTINUE_GENERIC);
        Term_clear();
    }

    byte final_sils = sil_count - stolen;
    bool treachery_occurred = (stolen > 0);

    /* ============================================================= */
    /* SCENE 5: The Weight of Victory                               */
    /* ============================================================= */
    print_heading_fade("The Weight of Victory", TERM_L_BLUE);
    
    const char *weight_text;
    if (!treachery_occurred)
    {
        const char *pure_frag[3] = {
            "A single star reclaimed, hope rekindled faintly in Middle-earth. Yet Morgoth laughs still, for two remain bound in shadow.",
            "Two jewels shine again beneath sky; Morgoth's power falters greatly. Yet you feel their brilliance burning; temptation ever near.",
            "All three jewels, radiant and pure, blaze again beneath stars. Morgoth's power breaks. Triumph is absolute, your soul soaring."
        };
        weight_text = pure_frag[final_sils-1];
    }
    else
    {
        const char *tainted_frag[3] = {
            "Though victory is yours, its memory darkens. Trust is fragile, and your spirit heavy beneath secret betrayal.",
            "Your heart trembles: Morgoth sees clearly your treachery-he smiles grimly, knowing darkness still dwells in you.",
            "Greatest triumph now mingled with darkest shame. Morgoth's laughter echoes bitterly-he senses your fall."
        };
        weight_text = tainted_frag[sil_count-1];
    }
    
    if (!fast_forward && !print_paragraph_fade(weight_text, treachery_occurred ? TERM_RED : TERM_L_WHITE, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(weight_text, treachery_occurred ? TERM_RED : TERM_L_WHITE);
    
    if (allow_kinslay)
        wait_prompt(PROMPT_FACE_ECHOES);
    else
        wait_prompt(PROMPT_CONCLUDE_TALE);
    Term_clear();

    /* ============================================================= */
    /* SCENE 6: Echoes of Kinslaying (Enhanced Notifications)      */
    /* ============================================================= */
    bool deferred_kill[3] = { false, false, false };
    int kinslaying_victims = 0;
    if (allow_kinslay)
    {
        print_heading_fade("Echoes of Kinslaying", TERM_L_RED);
        
        static const int kin_pct[3] = { 20, 50, 95 };
        int current_row = 4;

        for (int k = 0; k < sil_count; ++k)
        {
            /* One roll only – use kin_pct[] here and *skip* the roll
             * inside kinslayer_try_kill() later.                        */
            /* one-shot probability (keep a local alias for UI)        */
            bool fail = (rand_int(100) < kin_pct[k]);
            deferred_kill[k] = fail;
            if (fail) kinslaying_victims++;

            const char *echo_text = NULL;
            switch (k)
            {
                case 0: echo_text = fail ?
                    "\"Alqualonde's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualonde-first grief, first guilt." :
                    "The sorrow of Alqualonde passes over you-your spirit holds fast, blood unstained.";
                    break;
                case 1: echo_text = fail ?
                    "\"Ruin of Doriath\"\nAgain your hand recalls tragedy-fallen halls of Menegroth, Dior's blood shed beneath stolen starlight." :
                    "Memory of Doriath rises briefly, but your blade remains clean, honour upheld.";
                    break;
                case 2: echo_text = fail ?
                    "\"Tragedy at Sirion\"\nEchoes rise from Sirion-Elwing's flight, blood and betrayal. Once more your blade draws innocent blood, sealing doom anew." :
                    "You resist dark whispers recalling Sirion-your sword is stayed, mercy unbroken.";
                    break;
            }
            
            if (!fast_forward && !print_paragraph_fade(echo_text, fail ? TERM_RED : TERM_L_WHITE, current_row))
                fast_forward = true;
            else if (fast_forward)  print_paragraph(echo_text, fail ? TERM_RED : TERM_L_WHITE);
            
            current_row += 4; // Space for next echo
            
            /* Stop at first failure */
            if (fail) break;
        }

        if (kinslaying_victims > 0)
        {
            const char *doom_text = "Blood now stains your triumph, your fate forever woven with grief and shame.";
            if (!fast_forward && !print_paragraph_fade(doom_text, TERM_L_DARK, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(doom_text, TERM_L_DARK);
        }
        
        wait_prompt(PROMPT_CONCLUDE_TALE);
        Term_clear();
    }

    /* ============================================================= */
    /* SCENE 7: Final Summary                                       */
    /* ============================================================= */
    print_heading_fade("The Tale Concludes", TERM_YELLOW);
    
    char summary[256];
    strnfmt(summary, sizeof summary,
            "Your legend is written: %d Silmaril%s claimed, %s, %s.",
            final_sils,
            (final_sils == 1) ? "" : "s",
            treachery_occurred ? "tainted by treachery" : "pure of heart",
            (kinslaying_victims > 0) ? "stained by kinslaying" : "with honour intact");
    
    if (!fast_forward && !print_paragraph_fade(summary, TERM_L_GREEN, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(summary, TERM_L_GREEN);

    Term_xtra(TERM_XTRA_DELAY, 3000);
    Term_clear();

    /* ============================================================= */
    /* SCENE 8: Kinslaying Execution & Notifications               */
    /* ============================================================= */
    if (allow_kinslay && kinslaying_victims > 0)
    {
        /* Show kinslaying notifications BEFORE screen_load() */
        print_heading_fade("The Price of Blood", TERM_RED);
        
        char kill_msg[128];
        strnfmt(kill_msg, sizeof kill_msg,
                "Your kinslaying echoes through time. %d innocent%s will fall by your hand...",
                kinslaying_victims, (kinslaying_victims == 1) ? "" : "s");
        
        if (!fast_forward && !print_paragraph_fade(kill_msg, TERM_RED, 4))
            fast_forward = true;
        else if (fast_forward)
            print_paragraph(kill_msg, TERM_RED);
        
        wait_prompt(PROMPT_WITNESS_CONSEQUENCES);
    }

    /* ------------------------------------------------------------- */
    /*  SCENE 8-bis: actual executions with cinematic feedback       */
    /* ------------------------------------------------------------- */
    if (allow_kinslay && kinslaying_victims > 0) {
        Term_clear();
        print_heading_fade("Blood Is Demanded", TERM_RED);

        int row = 4;
        for (int k = 0; k < 3; k++) {
            if (!deferred_kill[k]) continue;

            const char *house =
                kinslayer_try_kill(k + 1, /*do_roll=*/false);
            if (!house) continue;               /* should not happen */

            metarun_increment_deaths();
            log_info("Metarun: kinslaying victim counted as death (%u total)", (unsigned)metar.deaths);

            char buf[96];
            strnfmt(buf, sizeof buf,
                    "A hero %s has fallen beneath your blade.", house);

            if (!fast_forward && !print_paragraph_fade(buf, TERM_RED, row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(buf, TERM_RED);

            row += 3;
        }

        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    } else {
        /* no kinslaying scene – still give one clean exit prompt   */
        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    }

    metarun_gain_silmarils(final_sils);
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils, metar.silmarils);
    refresh_current_metar_score();
    print_story(3, true);

    /* Restore the saved play-screen only after every narrative beat */
    screen_load();

    check_run_end();
    /* Save persistent settings when exiting */
    metarun_save_persistent_settings();
    
    /* Save metarun data (deaths, silmarils, etc.) */
    save_metaruns();
}


/* ======================  run-state logic  ====================== */
/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 *  Loss condition takes precedence over win condition.               *
 * ------------------------------------------------------------------ */
void check_run_end(void)
{
    /* Get dynamic win/loss conditions from runtype */
    int win_goal = WINCON_SILMARILS;   /* fallback */
    int death_base = LOSECON_DEATHS;   /* fallback */
    
    if (runtype_info && metar.type < z_info->rt_max)
    {
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
        death_base = runtype_info[metar.type].lose_con ? runtype_info[metar.type].lose_con : LOSECON_DEATHS;
    }
    
    int max_deaths = MAX(1, death_base - 3 * curse_flag_count(CUR_DEATH));

    /* Check loss condition first - if both win and loss are satisfied, loss takes precedence */
    if (metar.deaths >= max_deaths) {
        log_info("Metarun DEFEAT: %d deaths reached (limit: %d)", metar.deaths, max_deaths);
        screen_save();
        Term_clear();
        
        print_heading_fade("The Trial's End", TERM_RED);
        
        char defeat_text[256];
        strnfmt(defeat_text, sizeof(defeat_text),
                "%d hero%s fallen; the halls of Mandos echo with grief. "
                "This trial ends in shadow—the run is lost. "
                "Begin anew to reclaim lost hope.",
                max_deaths, (max_deaths == 1) ? " has" : "es have");
        
        print_paragraph_fade(defeat_text, TERM_WHITE, 4);
        
        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();
        
        start_new_metarun();
        return; /* Important: return after handling defeat */

    } else if (metar.silmarils >= win_goal) {
        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, win_goal);
        screen_save();
        Term_clear();
        
        print_heading_fade("The Trial's End", TERM_YELLOW);
        
        char victory_text[256];
        strnfmt(victory_text, sizeof(victory_text),
                "%d Silmarils reclaimed from Morgoth's crown! "
                "Hope kindles anew; your long trial approaches its end. "
                "Yet one final ordeal awaits—your ultimate destiny, "
                "as your true self faces the Last Trial.",
                win_goal);
        
        print_paragraph_fade(victory_text, TERM_L_GREEN, 4);
        
        const char *implementation_note = "(This final trial is yet to be implemented.)";
        print_paragraph_fade(implementation_note, TERM_L_DARK, 8);
        
        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();
        
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
    log_info("Starting new metarun (previous run ID: %d)", metar.id);
    log_debug("metarun: pre-finalize state (wizard=%d, noscore=0x%04X, savefile='%s')",
              p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
              p_ptr ? (unsigned)p_ptr->noscore : 0,
              savefile);

    u32b previous_id = metar.id;
    if (!sync_current_metarun_slot(true)) {
        log_warn("metarun: unable to snapshot current run before rollover (idx=%d, max=%d)",
                 current_run, metarun_max);
    }

     /* Before wiping scores for the next run, backup and clear save files */
     backup_and_clear_saves();
     
     /* Before wiping scores for the next run, finalize current ones:
         - mark all alive entries as dead by their own hand
         - save any corresponding savefiles as dead
         Then archive/clear the score file so the next run starts clean. */
     metarun_finalize_scores_and_saves();
     clear_scorefile();

    /* Hard purge the current savefile if this was a noscore wizard/debug run */
    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008)) && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            int rc;
            safe_setuid_grab();
            rc = fd_kill(savefile);
            safe_setuid_drop();
            if (rc == 0) {
                log_info("metarun: deleted noscore savefile '%s'", savefile);
            } else {
                log_warn("metarun: failed to delete noscore savefile '%s'", savefile);
            }
        }
    } else {
        log_info("metarun: purge skipped (wizard=%d, noscore=0x%04X, savefile='%s')",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0,
                 savefile);
    }
    /* Save old state */
    s16b old_max   = metarun_max;
    metarun *old   = metaruns;

    /* Try to allocate a new array for one more run */
    metarun *tmp = C_RNEW(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed - keep everything as is */
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
    metaruns[metarun_max - 1].id = previous_id + 1;
    metaruns[metarun_max - 1].type = 0; /* Default to type 0 (Normal) for new metaruns */

    /* Update globals */
    current_run      = metarun_max - 1;
    metar             = metaruns[current_run];
    metarun_created  = true;  /* Set flag to show story intro for new metarun */

    /* Apply difficulty curses based on the runtype */
    apply_difficulty_curses(&metar);

    /* Persist and prepare */
    save_metaruns();      /* safe now that metaruns≠NULL */ 
    ensure_run_dir(&metar);
    log_info("New metarun %d created and initialized", metar.id);
}

/* Show all active curses in a dedicated screen */
static void show_all_active_curses(void)
{
    int term_height, term_width;
    screen_save();
    Term_clear();
    
    /* Get actual terminal dimensions */
    Term_get_size(&term_width, &term_height);
    
    /* Title */
    Term_putstr(2, 1, -1, TERM_YELLOW, "=== All Active Curses ===");
    
    int row = 3;
    char buf[128];
    
    /* Count active curses */
    int active_count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_GET(id)) active_count++;
    }
    
    if (active_count == 0) {
        Term_putstr(2, row, -1, TERM_L_DARK, "No active curses");
    } else {
        snprintf(buf, sizeof buf, "%d active curse%s:", 
                 active_count, (active_count == 1) ? "" : "s");
        Term_putstr(2, row++, -1, TERM_WHITE, buf);
        
#ifdef DEBUG_CURSES
        Term_putstr(2, row++, -1, TERM_L_DARK, "(showing D:stacks and P:effect)");
#endif
        
        for (int id = 0; id < z_info->cu_max; id++) {
            byte cnt = CURSE_GET(id);
            if (!cnt) continue;
            
            /* Build line: id, name, D:count, optional P:text */
            cptr name = cu_name + cu_info[id].name;
#ifdef DEBUG_CURSES
            cptr pow = cu_text + cu_info[id].power;
            snprintf(buf, sizeof buf, " %2d: %-20s D:%d P:%s", id, name, cnt, pow);
#else
            snprintf(buf, sizeof buf, " %2d: %-20s D:%d", id, name, cnt);
#endif
            Term_putstr(4, row++, -1, TERM_WHITE, buf);
            
            /* Handle page breaks for very long lists using actual terminal height */
            if (row >= term_height - 2) {
                Term_putstr(2, row, -1, TERM_L_DARK, "[Press any key for more]");
                inkey();
                Term_clear();
                Term_putstr(2, 1, -1, TERM_YELLOW, "=== All Active Curses (continued) ===");
                row = 3;
            }
        }
    }
    
    Term_putstr(2, row + 1, -1, TERM_L_DARK, "Press any key to return.");
    inkey();
    screen_load();
}

/*
 * Enhanced print_metarun_stats():
 * - Draws bracketed progress bars for Silmarils & Deaths with colored stars
 * - Displays numeric counts next to each bar
 * - Aligns labels & values for a cleaner layout
 * - Lists active curses with D: and (optionally) P: details
 */
/* Updated print_metarun_stats(): prettier layout, star & death bars, curses list */
void print_metarun_stats(void)
{
    int row = 1;
    int col = 2;
    char buf[128];
    int x;
    int term_height, term_width;

    /* Safety check: ensure metarun system is initialized */
    if (current_run < 0 || current_run >= metarun_max) {
        screen_save();
        Term_clear();
        Term_putstr(2, 5, -1, TERM_RED, "Error: No metarun data available.");
        Term_putstr(2, 6, -1, TERM_L_WHITE, "Please start a new game first.");
        Term_putstr(2, 8, -1, TERM_L_DARK, "Press any key to return.");
        inkey();
        screen_load();
        return;
    }

    /* Save & clear screen */
    screen_save();
    Term_clear();
    
    /* Get actual terminal dimensions */
    Term_get_size(&term_width, &term_height);

    /* Title */
    Term_putstr(col, row++, -1, TERM_YELLOW, "=== Current Story Statistics ===");

    /* Run ID */
    snprintf(buf, sizeof buf, "Run-ID     : %u", metar.id);
    Term_putstr(col, row++, -1, TERM_WHITE, buf);
    
    /* Difficulty Level - use dynamic name from runtype */
    const char *diff_name = "Unknown";
    int win_goal = WINCON_SILMARILS;  /* fallback */
    int death_limit = LOSECON_DEATHS; /* fallback */
    
    if (runtype_info && metar.type < z_info->rt_max && runtype_info[metar.type].name[0])
    {
        diff_name = runtype_info[metar.type].name;
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
        death_limit = runtype_info[metar.type].lose_con ? runtype_info[metar.type].lose_con : LOSECON_DEATHS;
    }
    
    snprintf(buf, sizeof buf, "Difficulty : %s", diff_name);
    Term_putstr(col, row++, -1, TERM_L_BLUE, buf);

    snprintf(buf, sizeof buf, "Meta Score : %lu", (unsigned long)metar.score);
    Term_putstr(col, row++, -1, TERM_WHITE, buf);

    snprintf(buf, sizeof buf, "Best Run   : %lu", (unsigned long)metar.best_run_score);
    Term_putstr(col, row++, -1, TERM_WHITE, buf);

    /* Silmarils bar - use dynamic win goal */
    snprintf(buf, sizeof buf, "Silmarils  : ");
    Term_putstr(col, row, -1, TERM_WHITE, buf);
    x = col + strlen(buf);
    for (int i = 0; i < win_goal; i++) {
        byte attr = (i < metar.silmarils) ? TERM_L_GREEN : TERM_L_WHITE;
        Term_putch(x++, row, attr, '*');
    }
    snprintf(buf, sizeof buf, "  (%d/%d)", metar.silmarils, win_goal);
    Term_putstr(x + 1, row++, -1, TERM_WHITE, buf);

    /* Deaths bar - calculate actual death limit based on difficulty and curses */
    /* Safe access: Use metarun curse data directly instead of curse_flag_count which may access uninitialized player data */
    int death_curse_stacks = 0;
    if (z_info && z_info->cu_max > CUR_DEATH) {
        death_curse_stacks = CURSE_GET(CUR_DEATH);
    }
    int max_deaths = MAX(1, death_limit - 3 * death_curse_stacks);
    snprintf(buf, sizeof buf, "Deaths     : ");
    Term_putstr(col, row, -1, TERM_WHITE, buf);
    x = col + strlen(buf);
    for (int i = 0; i < max_deaths; i++) {
        byte attr = (i < metar.deaths) ? TERM_RED : TERM_L_WHITE;
        Term_putch(x++, row, attr, 'x');
    }
    snprintf(buf, sizeof buf, "  (%d/%d)", metar.deaths, max_deaths);
    Term_putstr(x + 1, row++, -1, TERM_WHITE, buf);

    /* Active curses list - with pagination to fit screen */
    int curse_start_row = row + 1; /* Add minimal spacing before curses */
    int available_lines = term_height - curse_start_row - 2; /* Reserve 2 lines for prompt only */
    bool curses_truncated = false; /* Track if we had to truncate the curse list */
    
    /* Count active curses first */
    int active_curse_count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_GET(id)) active_curse_count++;
    }
    
    if (active_curse_count > 0) {
        Term_putstr(col, curse_start_row++, -1, TERM_YELLOW, "Active Curses:");
#ifdef DEBUG_CURSES
        Term_putstr(col, curse_start_row++, -1, TERM_L_DARK, "(showing D:stacks and P:effect)");
        available_lines--; /* Account for debug line */
#endif
        
        /* Calculate how many curses we can show - be more aggressive with space usage */
        int max_curses_to_show = available_lines;
        if (active_curse_count > max_curses_to_show) {
            max_curses_to_show--; /* Reserve 1 line for "more..." message */
        }
        
        int curses_shown = 0;
        
        for (int id = 0; id < z_info->cu_max && curses_shown < max_curses_to_show; id++) {
            byte cnt = CURSE_GET(id);
            if (!cnt) continue;
            
            /* Build line: id, name, D:count, optional P:text */
            cptr name = cu_name + cu_info[id].name;
#ifdef DEBUG_CURSES
            cptr pow = cu_text + cu_info[id].power;
            snprintf(buf, sizeof buf, " %2d: %-20s D:%d P:%s", id, name, cnt, pow);
#else
            snprintf(buf, sizeof buf, " %2d: %-20s D:%d", id, name, cnt);
#endif
            Term_putstr(col + 2, curse_start_row++, -1, TERM_WHITE, buf);
            curses_shown++;
        }
        
        /* Check if there are more curses that couldn't be shown */
        if (curses_shown < active_curse_count) {
            curses_truncated = true;
            int remaining = active_curse_count - curses_shown;
            snprintf(buf, sizeof buf, "     ... and %d more curse%s (press 's' to see all)", 
                     remaining, (remaining == 1) ? "" : "s");
            Term_putstr(col + 2, curse_start_row++, -1, TERM_L_DARK, buf);
        }
        
        /* If curses were truncated, mention the 's' option more prominently */
        if (curses_truncated) {
            snprintf(buf, sizeof buf, "[c] Change difficulty  [s] Show history & full curse list  [any other key] Continue");
        } else {
            snprintf(buf, sizeof buf, "[c] Change difficulty  [s] Show history  [any other key] Continue");
        }
    } else {
        Term_putstr(col, curse_start_row++, -1, TERM_L_DARK, "No active curses");
        snprintf(buf, sizeof buf, "[c] Change difficulty  [s] Show history  [any other key] Continue");
    }

    /* Enhanced prompt - position it at the bottom of screen */
    Term_putstr(col, term_height - 1, -1, TERM_L_DARK, buf);
    
    char key = inkey();
    if (key == 'c' || key == 'C')
    {
        screen_load();
        choose_difficulty_menu();
        return;
    }
    else if (key == 's' || key == 'S')
    {
        screen_load();
        list_metaruns();
        
        /* Also show all curses if they were truncated in the main display */
        if (curses_truncated) {
            show_all_active_curses();
        }
        
        print_metarun_stats(); /* Return to stats after showing history */
        return;
    }
    
    screen_load();
    
    /* Check if metarun has ended after user chooses to continue */
    check_run_end();
}

/* Generate curse description for a runtype */
static void get_curse_description(int runtype_id, char *buf, size_t buf_size)
{
    if (!runtype_info || runtype_id >= z_info->rt_max || buf_size < 64)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    runtype_type *rt = &runtype_info[runtype_id];
    
    if (!rt->start_curses)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    /* Count curses and determine stack ranges */
    int curse_count = 0;
    int min_stacks = 255, max_stacks = 0;
    
    for (int curse_id = 0; curse_id < 32; curse_id++)
    {
        if (rt->start_curses & (1UL << curse_id))
        {
            curse_count++;
            int stacks = rt->curse_stacks[curse_id];
            if (stacks < min_stacks) min_stacks = stacks;
            if (stacks > max_stacks) max_stacks = stacks;
        }
    }
    
    if (curse_count == 0)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    /* Format the description */
    if (min_stacks == max_stacks)
    {
        if (min_stacks == 1)
            snprintf(buf, buf_size, "Curses: %d x %d stack", curse_count, min_stacks);
        else
            snprintf(buf, buf_size, "Curses: %d x %d stacks", curse_count, min_stacks);
    }
    else
    {
        snprintf(buf, buf_size, "Curses: %d (%d-%d stacks)", curse_count, min_stacks, max_stacks);
    }
}

/* Difficulty selection menu */
static void choose_difficulty_menu(void)
{
    int choice = metar.type;  /* Start with current difficulty */
    int max_difficulty = (runtype_info && z_info->rt_max > 0) ? z_info->rt_max - 1 : 0;
    
    screen_save();
    
    while (true)
    {
        Term_clear();

        /* Title */
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== Select Difficulty Level ===");
        
        int row = 3;
        for (int i = 0; i <= max_difficulty; i++)
        {
            byte name_color, desc_color;
            byte runtype_color = TERM_WHITE; /* default color */
            bool is_locked = (i < metar.max_difficulty_reached); /* Lock easier difficulties */
            
            /* Get runtype color from U: field */
            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0])
            {
                runtype_color = runtype_info[i].colour;
            }
            else
            {
                runtype_color = TERM_WHITE; /* fallback if runtype not loaded */
            }
            
            if (is_locked) {
                /* Locked (easier) difficulties - greyed out */
                name_color = TERM_L_DARK;
                desc_color = TERM_L_DARK;
                Term_putstr(2, row, -1, TERM_L_DARK, "-");
            }
            else if (i == choice) {
                /* Highlight selected difficulty - use runtype color but brighter */
                name_color = runtype_color;
                desc_color = TERM_L_WHITE;
                Term_putstr(2, row, -1, runtype_color, ">");
            } else if (i == metar.type) {
                /* Show current difficulty in its runtype color but dimmed */
                name_color = runtype_color;
                desc_color = TERM_SLATE;
                Term_putstr(2, row, -1, TERM_L_DARK, " ");
            } else {
                /* Normal difficulty in its runtype color */
                name_color = runtype_color;
                desc_color = TERM_L_DARK;
                Term_putstr(2, row, -1, TERM_L_DARK, " ");
            }
            
            /* Get dynamic name and stats from runtype */
            const char *rt_name = "Unknown";
            int win_goal = WINCON_SILMARILS;
            int death_limit = LOSECON_DEATHS;
            
            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0])
            {
                rt_name = runtype_info[i].name;
                win_goal = runtype_info[i].win_con ? runtype_info[i].win_con : WINCON_SILMARILS;
                death_limit = runtype_info[i].lose_con ? runtype_info[i].lose_con : LOSECON_DEATHS;
            }
            
            char desc_buf[128];
            char curse_buf[64];
            get_curse_description(i, curse_buf, sizeof(curse_buf));
            
            if (is_locked) {
                snprintf(desc_buf, sizeof(desc_buf), "[LOCKED] Win: %d Silmarils, Lose: %d deaths, %s", 
                         win_goal, death_limit, curse_buf);
            } else {
                snprintf(desc_buf, sizeof(desc_buf), "Win: %d Silmarils, Lose: %d deaths, %s", 
                         win_goal, death_limit, curse_buf);
            }
            
            char name_buf[128];
            if (is_locked) {
                snprintf(name_buf, sizeof(name_buf), "%c) %s [LOCKED]", 'a'+i, rt_name);
            } else {
                snprintf(name_buf, sizeof(name_buf), "%c) %s", 'a'+i, rt_name);
            }
            
            Term_putstr(4, row++, -1, name_color, name_buf);
            Term_putstr(7, row++, -1, desc_color, desc_buf);
            
            /* Add extra spacing between options */
            row++;
        }
        
        /* Instructions */
        Term_putstr(2, row + 1, -1, TERM_L_WHITE, "Arrows to navigate     Space/Enter Accept     Esc Cancel");
        
        /* Get input */
        char key = inkey();
        
        /* Handle input */
        if (key == ESCAPE) 
        {
            screen_load();
            return;
        }
        else if (key == '\r' || key == '\n' || key == ' ' || key == '6')  /* Enter/Space/6 key */
        {
            /* Check if trying to select a locked difficulty */
            if (choice < metar.max_difficulty_reached) {
                /* Show warning and stay in menu */
                Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, 2000);
                continue;
            }
            break;  /* Confirm selection */
        }
        else if (key == '8' || key == 'k' || key == '-')  /* Up */
        {
            /* Navigate up but skip locked difficulties */
            int new_choice = choice - 1;
            while (new_choice >= 0 && new_choice < metar.max_difficulty_reached) {
                new_choice--;
            }
            if (new_choice >= 0) choice = new_choice;
        }
        else if (key == '2' || key == 'j' || key == '+')  /* Down */
        {
            /* Navigate down normally */
            if (choice < max_difficulty) choice++;
        }
        else if (key >= 'a' && key <= 'z')  /* Letter selection */
        {
            int new_choice = key - 'a';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    /* Show warning for locked difficulty */
                    Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                    Term_fresh();
                    Term_xtra(TERM_XTRA_DELAY, 2000);
                } else {
                    choice = new_choice;
                }
            }
        }
        else if (key >= 'A' && key <= 'Z')  /* Capital letter selection */
        {
            int new_choice = key - 'A';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    /* Show warning for locked difficulty */
                    Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                    Term_fresh();
                    Term_xtra(TERM_XTRA_DELAY, 2000);
                } else {
                    choice = new_choice;
                }
            }
        }
    }
    
    /* Apply the new difficulty */
    if (choice != metar.type)
    {
        /* Warn if increasing difficulty */
        if (choice > metar.type) {
            screen_save();
            Term_clear();
            Term_putstr(2, 5, -1, TERM_YELLOW, "WARNING: Increasing Difficulty");
            Term_putstr(2, 7, -1, TERM_WHITE, "If you increase the difficulty level, you will NOT be able to");
            Term_putstr(2, 8, -1, TERM_WHITE, "go back to an easier level for the rest of this story run.");
            Term_putstr(2, 10, -1, TERM_L_RED, "This change is PERMANENT for this meta-run!");
            Term_putstr(2, 12, -1, TERM_L_WHITE, "Do you want to continue? (y/n)");
            
            char confirm = inkey();
            screen_load();
            
            if (confirm != 'y' && confirm != 'Y') {
                return; /* Cancel the change */
            }
        }
        
        log_info("Changing difficulty from %d to %d", metar.type, choice);
        
        /* Store current curses to preserve them */
        u32b preserved_curses_lo = metar.curses_lo;
        u32b preserved_curses_hi = metar.curses_hi;
        u32b preserved_curses_seen = metar.curses_seen;
        
        /* Temporarily clear curses to get base difficulty curses */
        metarun_clear_all_curses();
        
        /* Set new type and apply its base curses */
        metar.type = (byte)choice;
        apply_difficulty_curses(&metar);
        
        /* Merge preserved curses with new difficulty curses (ADD stacks, don't just take max) */
        for (int curse_id = 0; curse_id < 32; curse_id++) {
            byte preserved_stacks = (curse_id < 16) ? 
                ((preserved_curses_lo >> (curse_id * 2)) & 3) :
                ((preserved_curses_hi >> ((curse_id - 16) * 2)) & 3);
            
            byte current_stacks = CURSE_GET(curse_id);
            byte total_stacks = preserved_stacks + current_stacks;
            
            /* Respect per-curse maximum stacks */
            byte max_allowed = cu_info[curse_id].max_stacks;
            if (max_allowed > 0 && total_stacks > max_allowed) {
                total_stacks = max_allowed;
            }
            
            if (total_stacks > 0) {
                CURSE_SET(curse_id, total_stacks);
            }
        }
        
        /* Update maximum difficulty reached */
        if (choice > metar.max_difficulty_reached) {
            metar.max_difficulty_reached = (byte)choice;
        }
        
        /* Restore seen flags */
        metar.curses_seen |= preserved_curses_seen;
        
        /* Save changes */
        save_metaruns();
        
        const char *new_name = "Unknown";
        if (runtype_info && choice < z_info->rt_max && runtype_info[choice].name[0])
            new_name = runtype_info[choice].name;
        
        msg_print(format("Difficulty changed to: %s", new_name));
    }
    
    screen_load();
    
    /* Return to metarun stats to show updated information */
    print_metarun_stats();
}

/* compact table of all meta-runs */
void list_metaruns(void)
{
    screen_save();
    Term_clear();
    c_prt(TERM_L_GREEN, "Meta-run history", 1, 2);
    c_put_str(TERM_L_DARK,
              " *ID      Score     Sil  Dth  Res  Last played", 3, 2);

    refresh_current_metar_score();

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    s16b *order = NULL;
    if (metarun_max > 0 && metaruns) {
        order = C_ZNEW(metarun_max, s16b);
        for (s16b i = 0; i < metarun_max; i++) order[i] = i;
        qsort(order, metarun_max, sizeof(s16b), compare_metarun_indices);
    }

    int row = 4;
    for (s16b i = 0; i < metarun_max; i++) {
        s16b idx = order ? order[i] : i;
        const metarun *m = &metaruns[idx];

        /* Get dynamic win/loss conditions for this metarun type */
        int win_goal = WINCON_SILMARILS;
        int death_limit = LOSECON_DEATHS;

        if (runtype_info && m->type < z_info->rt_max)
        {
            win_goal = runtype_info[m->type].win_con ? runtype_info[m->type].win_con : WINCON_SILMARILS;
            death_limit = runtype_info[m->type].lose_con ? runtype_info[m->type].lose_con : LOSECON_DEATHS;
        }

        char res = (m->silmarils >= win_goal) ? 'W' :
                   (m->deaths >= death_limit) ? 'L' : ' ';
        char date[16];
        strftime(date, sizeof date, "%Y-%m-%d",
                 localtime((time_t*)&m->last_played));

        byte attr = (idx == current_run) ? TERM_YELLOW : TERM_WHITE;
        char marker = (idx == current_run) ? '*' : ' ';

        c_put_str(attr,
                  format("%c%08u %8lu   %2d   %2d   %c   %s",
                         marker,
                         (unsigned)m->id,
                         (unsigned long)m->score,
                         m->silmarils, m->deaths, res, date),
                  row++, 2);

        if (row >= 23 && i+1 < metarun_max) {   /* page break */
            c_put_str(TERM_L_DARK, "[more – any key]", 23, 2);
            inkey();  Term_clear();
            row = 4;
            c_prt(TERM_L_GREEN, "Meta-run history (cont.)", 1, 2);
            c_put_str(TERM_L_DARK,
                      " *ID      Score     Sil  Dth  Res  Last played", 3, 2);
        }
    }

    FREE(order);
    c_put_str(TERM_L_DARK, "Press any key to return.", row+1, 2);
    inkey();
    screen_load();
}

void show_known_curses_menu(void)
{
    int shown = 0;
    int row = 2;
    int id;

    /* Collect and count first */
    for (id = 0; id < (int)z_info->cu_max; id++)
        if (CURSE_SEEN(id)) {
                shown++;
            }
    if (!shown) {
        log_debug("No curses have been seen yet");
        msg_print("You have not identified any curses yet.");
        return;
    }

    log_info("Displaying %d known curses", shown);

    screen_save();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curses:");

    row = 2;

    /* Enable wrapped text helper */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 4;   /* generous rhs margin */

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (!CURSE_SEEN(id)) continue;

        curse_type *c = &cu_info[id];
        cptr cname  = cu_name + c->name;
        cptr cdesc  = cu_text + c->text;
        cptr cpower = cu_text + c->power;

        /* Name */
        c_put_str(TERM_L_RED, cname, row, 1);
        row++;

        /* Description (wrapped) */
        Term_gotoxy(3, row);
        text_out_c(TERM_WHITE, cdesc);
        row += count_wrapped_lines(cdesc, text_out_wrap, 3);

        /* Optional power line */
        if (*cpower)
        {
            Term_gotoxy(3, row);
            text_out_c(TERM_L_DARK, cpower);
            row += count_wrapped_lines(cpower, text_out_wrap, 3);
        }

        /* Page wrap (match self_knowledge style) */
        if (row >= 21)
        {
            Term_putstr(1, row, -1, TERM_L_WHITE, "(press any key)");
            (void)inkey();
            Term_clear();
            Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curses:");
            row = 2;
        }
    }

    Term_putstr(1, row+1, -1, TERM_L_WHITE, "(press any key)");
    (void)inkey();
    screen_load();
}

/* Public wrapper for difficulty selection menu */
void choose_difficulty_level(void)
{
    choose_difficulty_menu();
}

/* ================================================================== */
/*  Quest completion tracking functions                               */
/* ================================================================== */

/* Check if a specific quest is completed in the CURRENT metarun */
bool metarun_is_quest_completed(u32b quest_flag)
{
    /* Only check the current metarun, not all metaruns */
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Metarun quest check: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return false;
    }
    
    if (metaruns[current_run].completed_quests & quest_flag) {
        log_trace("Metarun quest check: Found quest 0x%x completed in current metarun[%d] (id=%d)", 
                  quest_flag, current_run, metaruns[current_run].id);
        return true;
    }
    
    log_trace("Metarun quest check: Quest 0x%x not completed in current metarun[%d] (id=%d)", 
              quest_flag, current_run, metaruns[current_run].id);
    return false;
}

/* Mark a quest as completed in the current metarun */
void metarun_mark_quest_completed(u32b quest_flag)
{
    if (current_run < 0 || current_run >= metarun_max) return;
    /* IMPORTANT: modify the live 'metar' copy first, THEN persist.
     * Previous code wrote directly to metaruns[current_run] and was
     * immediately overwritten inside save_metaruns() when that
     * function copied the stale 'metar' struct back into the array.
     * (metaruns[current_run] = metar;). This caused lost quest flags.
     */
    if (!(metar.completed_quests & quest_flag)) {
        metar.completed_quests |= quest_flag;                  /* update live */
        metaruns[current_run].completed_quests = metar.completed_quests; /* keep array in sync early (optional) */
        log_trace("Metarun: Quest flag 0x%x added (completed_quests=0x%08X)", quest_flag, metar.completed_quests);
        refresh_current_metar_score();
        save_metaruns();
    } else {
        log_trace("Metarun: Quest flag 0x%x already set (completed_quests=0x%08X) - no save needed", quest_flag, metar.completed_quests);
    }
}

/* Check and update quest completion status based on player state */
void metarun_check_and_update_quests(void)
{
    log_trace("Metarun quest check: Entry - current_run=%d, metarun_max=%d", current_run, metarun_max);
    
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Metarun quest check: Early return - current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    
    log_trace("Metarun quest check: current_run=%d, tulkas=%d, aule=%d, mandos=%d", 
              current_run, p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);
    
    /* Check Tulkas quest completion - only mark as metarun-complete when REWARDED */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED) {
        if (!metarun_is_quest_completed(METARUN_QUEST_TULKAS)) {
            log_trace("Metarun: Marking Tulkas quest as completed (rewarded, was %d)", p_ptr->tulkas_quest);
            metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
        } else {
            log_trace("Metarun: Tulkas quest already marked as completed");
        }
    }
    
    /* Check Aule quest completion - only mark as metarun-complete when REWARDED */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED) {
        if (!metarun_is_quest_completed(METARUN_QUEST_AULE)) {
            log_trace("Metarun: Marking Aule quest as completed (rewarded)");
            metarun_mark_quest_completed(METARUN_QUEST_AULE);
        } else {
            log_trace("Metarun: Aule quest already marked as completed");
        }
    }

    /* Check Mandos quest completion - only mark as metarun-complete when REWARDED */
    if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED) {
        if (!metarun_is_quest_completed(METARUN_QUEST_MANDOS)) {
            log_trace("Metarun: Marking Mandos quest as completed (rewarded)");
            metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
        } else {
            log_trace("Metarun: Mandos quest already marked as completed");
        }
    }
}

/* Restore quest states from metarun data after character loading */
void metarun_restore_quest_states(void)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Metarun restore: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    
    u32b completed = metaruns[current_run].completed_quests;
    log_trace("Metarun restore: Restoring quest states from metarun[%d], completed_quests=0x%08X", 
              current_run, completed);
    
    /* Restore Tulkas quest state */
    if (completed & METARUN_QUEST_TULKAS) {
        if (p_ptr->tulkas_quest < TULKAS_QUEST_REWARDED) {
            p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
            log_trace("Metarun restore: Tulkas quest set to REWARDED (%d)", TULKAS_QUEST_REWARDED);
        }
    }
    
    /* Restore Aule quest state */
    if (completed & METARUN_QUEST_AULE) {
        if (p_ptr->aule_quest < AULE_QUEST_REWARDED) {
            p_ptr->aule_quest = AULE_QUEST_REWARDED;
            log_trace("Metarun restore: Aule quest set to REWARDED (%d)", AULE_QUEST_REWARDED);
        }
    }
    
    /* Restore Mandos quest state */
    if (completed & METARUN_QUEST_MANDOS) {
        if (p_ptr->mandos_quest < MANDOS_QUEST_REWARDED) {
            p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
            log_trace("Metarun restore: Mandos quest set to REWARDED (%d)", MANDOS_QUEST_REWARDED);
        }
    }
    
    /* Restore Niena quest state */
    if (completed & METARUN_QUEST_NIENA) {
        if (p_ptr->niena_quest < NIENA_QUEST_REWARDED) {
            p_ptr->niena_quest = NIENA_QUEST_REWARDED;
            p_ptr->niena_level = 0; /* Clear depth for previous run attribution */
            log_trace("Metarun restore: Niena quest set to REWARDED (%d)", NIENA_QUEST_REWARDED);
        }
    }
    
    /* Restore Orome quest state */
    if (completed & METARUN_QUEST_OROME) {
        if (p_ptr->orome_quest < OROME_QUEST_REWARDED) {
            p_ptr->orome_quest = OROME_QUEST_REWARDED;
            log_trace("Metarun restore: Orome quest set to REWARDED (%d)", OROME_QUEST_REWARDED);
        }
    }
    
    log_trace("Metarun restore: Final quest states - Tulkas: %d, Aule: %d, Mandos: %d, Niena: %d, Orome: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest);
}

/* ------------------------------------------------------------------ */
/*  Oath system tracking                                              */
/* ------------------------------------------------------------------ */

/*
 * Check if an oath is unlocked in the current metarun
 */
bool oath_unlocked(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].unlocked_oaths & oath_bit) != 0;
}

/*
 * Check if an oath is banned in the current metarun
 */
bool oath_banned(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].banned_oaths & oath_bit) != 0;
}

/*
 * Unlock an oath in the current metarun
 */
void metarun_unlock_oath(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath unlock: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath unlock: Invalid oath_id=%d", oath_id);
        return;
    }
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-4 to bits 1,2,4,8 */
    
    /* Update both the global metar and the metaruns array */
    metar.unlocked_oaths |= oath_bit;
    metaruns[current_run].unlocked_oaths |= oath_bit;
    
    log_trace("Oath unlock: Unlocked oath %d (bit %d) in metarun[%d], unlocked_oaths=0x%02X", 
              oath_id, oath_bit, current_run, metaruns[current_run].unlocked_oaths);
    
    /* Save immediately to persist the change */
    save_metaruns();
}

/*
 * Ban an oath in the current metarun (when broken)
 */
void metarun_ban_oath(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath ban: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath ban: Invalid oath_id=%d", oath_id);
        return;
    }
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    
    /* Update both the global metar and the metaruns array */
    metar.banned_oaths |= oath_bit;
    metaruns[current_run].banned_oaths |= oath_bit;

    log_trace("Oath ban: Banned oath %d (bit %d) in metarun[%d], banned_oaths=0x%02X",
              oath_id, oath_bit, current_run, metaruns[current_run].banned_oaths);

    /* Save immediately to persist the change */
    refresh_current_metar_score();
    save_metaruns();
}

/*
 * Get bitmask of oaths available for selection (unlocked but not banned)
 */
int get_available_oaths_mask(void)
{
    if (current_run < 0 || current_run >= metarun_max) return 0;
    
    byte unlocked = metaruns[current_run].unlocked_oaths;
    byte banned = metaruns[current_run].banned_oaths;
    byte available = unlocked & ~banned;
    
    log_trace("Oath availability: unlocked=0x%02X, banned=0x%02X, available=0x%02X", 
              unlocked, banned, available);
    
    return available;
}
