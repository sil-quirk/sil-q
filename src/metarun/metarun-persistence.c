#include "angband.h"
#include "metarun-internal.h"
#include "metarun-files.h"

#include <limits.h>

static errr backup_file(const char *filepath);

bool build_meta_path(char *buf, size_t len,
    const metarun *m, const char *leaf)
{
    const char* name = leaf ? leaf : "";

    if (!m)
    {
#ifdef SIL_USE_LOCAL_DATA
        /* Portable build: use ANGBAND_DIR_APEX */
        if (!path_build(buf, len, ANGBAND_DIR_APEX, name))
#else
        /* Normal build: use parent of ANGBAND_DIR_METARUN (the meta directory) */
        char meta_dir[1024];
        if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
            SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
            char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
            if (last_sep) *last_sep = '\0';
        } else {
            SDL_strlcpy(meta_dir, ANGBAND_DIR_APEX, sizeof(meta_dir));
        }
        if (!path_build(buf, len, meta_dir, name))
#endif
        {
            log_error("build_meta_path: failed for apex/%s", name);
            return false;
        }
        return true;
    }

    char sub[128];
#ifdef SIL_USE_LOCAL_DATA
    if (name[0])
        strnfmt(sub, sizeof sub, "%s/%08u/%s",
            META_SUBDIR, (unsigned)m->id, name);
    else
        strnfmt(sub, sizeof sub, "%s/%08u",
            META_SUBDIR, (unsigned)m->id);
    if (!path_build(buf, len, ANGBAND_DIR_APEX, sub))
#else
    if (name[0])
        strnfmt(sub, sizeof sub, "%08u/%s", (unsigned)m->id, name);
    else
        strnfmt(sub, sizeof sub, "%08u", (unsigned)m->id);
    if (!path_build(buf, len, ANGBAND_DIR_METARUN, sub))
#endif
    {
        log_error("build_meta_path: failed for %s", sub);
        return false;
    }
    return true;
}


void reset_defaults(metarun *m)
{
    log_info("Initializing new metarun with default values");
    memset(m, 0, sizeof(*m));
    metarun_clear_blessing_runtime_fields(m);
    m->id          = 1;
    m->last_played = (u32b)time(NULL);
    memset(m->curse_stacks, 0, sizeof(m->curse_stacks));
    m->curses_seen = 0;
    m->deaths      = 0;
    m->silmarils   = 0;

    /* Initialize persistent settings with defaults */
    for (int i = 0; i < 8; i++) {
        m->persistent_options[i] = 0;
    }
    for (int i = 0; i < SAVE_WINDOW_TERM_MAX; i++) {
        m->persistent_window_flags[i] = 0;
    }
    m->persistent_delay_factor = 5;      /* Default delay factor */
    m->persistent_hitpoint_warn = 3;     /* Default hitpoint warning */
    m->persistent_options_initialized = 0; /* Mark as not initialized yet */

    /* Initialize quest tracking */
    m->completed_quests = 0;             /* No quests completed initially */
    for (int i = 0; i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = 0;
    }
    metarun_clamp_and_sync_quests(m);

    /* Initialize oath system tracking */
    m->unlocked_oaths = 0;               /* No oaths unlocked initially */
    m->banned_oaths = 0;                 /* No oaths banned initially */
    m->max_difficulty_reached = 0;       /* Start with easiest difficulty */

    /* Clear quest_reserved array */
    for (int i = 0; i < 12; i++) {
        m->quest_reserved[i] = 0;
    }

    m->score = compute_metarun_score(m);
    update_blessing_ledger(m);

    log_debug("After init: curses_seen = 0x%016llX", (unsigned long long)m->curses_seen);
}

static bool metarun_header_before(const meta_file_header* header,
    byte major, byte minor, byte patch, byte extra)
{
    if (!header)
        return false;
    if (header->version_major != major)
        return header->version_major < major;
    if (header->version_minor != minor)
        return header->version_minor < minor;
    if (header->version_patch != patch)
        return header->version_patch < patch;
    return header->version_extra < extra;
}

static void metarun_clear_obsolete_interface_options_097(metarun* m)
{
    if (!m)
        return;

#define CLEAR_OBSOLETE_METARUN_OPTION(slot) \
    m->persistent_options[(slot) / 32] &= ~(1UL << ((slot) % 32));
    SIL_OBSOLETE_OPTION_097_SLOTS(CLEAR_OBSOLETE_METARUN_OPTION)
#undef CLEAR_OBSOLETE_METARUN_OPTION
}

static bool ensure_default_metarun_slot(const char *reason)
{
    if (metarun_max > 0 && metaruns) return false;

    if (metaruns) {
        mem_free_null(metaruns);
        metaruns = NULL;
    }

    if (reason && *reason)
        log_warn("Metarun recovery triggered (%s); creating default entry", reason);
    else
        log_warn("Metarun recovery triggered; creating default entry");

    metarun_max = 1;
    metaruns = mem_alloc_array(metarun_max, metarun);
    reset_defaults(&metaruns[0]);
    metarun_created = true;

    return true;
}

/* Apply initial curses based on difficulty level (runtype) */
void apply_difficulty_curses(metarun *m)
{
    if (!runtype_info) return; /* runtype data not loaded yet */
    if (m->type >= z_info->rt_max) return; /* invalid runtype */

    runtype_type *rt = &runtype_info[m->type];

    log_info("Applying curses for runtype %d (%s)", m->type, rt->name);

    /* Apply curses based on runtype configuration */
    if (rt->start_curses)
    {
        int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
        for (int curse_id = 0; curse_id < limit; curse_id++)
        {
            if (rt->start_curses & (1ULL << curse_id))
            {
                byte stacks = rt->curse_stacks[curse_id];
                if (stacks > 0)
                {
                    m->curse_stacks[curse_id] = (int8_t)stacks;
                    m->curses_seen |= (1ULL << curse_id);
                    log_debug("Applied %d stacks of curse %d from runtype", stacks, curse_id);
                }
            }
        }
    }
}

/* ensure the per-run metarun directory exists */
void ensure_run_dir(const metarun *m)
{
    char base[1024];
    char dir[1024];
    char leaf[32];

    if (!m)
        return;

#ifdef SIL_USE_LOCAL_DATA
    if (!path_build(base, sizeof base, ANGBAND_DIR_APEX, META_SUBDIR))
    {
        log_error("ensure_run_dir: failed to build base metarun directory");
        return;
    }
#else
    if (!ANGBAND_DIR_METARUN || !*ANGBAND_DIR_METARUN)
    {
        log_error("ensure_run_dir: metarun directory is not configured");
        return;
    }
    SDL_strlcpy(base, ANGBAND_DIR_METARUN, sizeof(base));
#endif

    MKDIR(base);
    strnfmt(leaf, sizeof leaf, "%08u", (unsigned)m->id);
    if (!path_build(dir, sizeof dir, base, leaf))
    {
        log_error("ensure_run_dir: failed to build run directory for id=%u",
            (unsigned)m->id);
        return;
    }
    MKDIR(dir);
}

void cleanup_old_game_files(void)
{
#ifndef METARUN_CLEANUP_OLD_FILES
    log_info("*** FRESH STARTUP CLEANUP DISABLED (METARUN_CLEANUP_OLD_FILES not defined) ***");
    return;
#else
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
    if (!path_build(search_path, sizeof(search_path), save_dir, "*"))
    {
        log_error("cleanup_old_game_files: failed to build save directory search path");
        return;
    }

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
        if (path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw"))
        {
            SDL_IOStream* score_fd = sdl_fopen(score_file, "rb");
            if (score_fd) {
                sdl_fclose(score_fd);
                log_info("*** REMOVING SCORE FILE FOR FRESH START ***");

                /* Platform-agnostic file removal using standard C */
                remove(score_file);
            } else {
                log_trace("Fresh startup: no score file found");
            }
        }
        else
        {
            log_error("cleanup_old_game_files: failed to build score file path");
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
    if (path_build(cleanup_search_path, sizeof(cleanup_search_path), save_dir, "*"))
    {
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
                if (!path_build(file_path, sizeof(file_path), save_dir, filename))
                {
                    log_error("cleanup_old_game_files: failed to build deletion path for '%s'", filename);
                    continue;
                }

                if (remove(file_path) == 0) {
                    files_deleted++;
                    log_trace("Fresh startup: deleted file: %s", filename);
                } else {
                    log_trace("Fresh startup: failed to delete: %s", filename);
                }

            } while (FindNextFile(hCleanupFind, &cleanupFindData));
            FindClose(hCleanupFind);
        }
    }
    else
    {
        log_error("cleanup_old_game_files: failed to build cleanup search path");
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
            if (!path_build(file_path, sizeof(file_path), save_dir, filename))
            {
                log_error("cleanup_old_game_files: failed to build deletion path for '%s'", filename);
                continue;
            }

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
    if (!path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw"))
    {
        log_error("cleanup_old_game_files: failed to build score file path during cleanup");
        return;
    }

    SDL_IOStream* score_fd = sdl_fopen(score_file, "rb");
    if (score_fd) {
        sdl_fclose(score_fd);
        log_info("*** REMOVING SCORE FILE FOR FRESH START ***");

        /* Platform-agnostic file removal using standard C */
        remove(score_file);
    }

    log_info("*** FRESH STARTUP CLEANUP COMPLETED ***");
#endif /* METARUN_CLEANUP_OLD_FILES */
}

errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    SDL_IOStream* fd;
    bool found_existing_data = false;

    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;
    fd = sdl_fopen(fn, "rb");

#ifdef SIL_USE_LOCAL_DATA
    if (!fd) {
        char legacy_dir[1024];
        char legacy[1024];
        if (path_build(legacy_dir, sizeof legacy_dir, ANGBAND_DIR_APEX, META_SUBDIR)
            && path_build(legacy, sizeof legacy, legacy_dir, META_RAW))
        {
            fd = sdl_fopen(legacy, "rb");
            if (fd) {
                log_info("Loading legacy portable metarun file: %s", legacy);
                found_existing_data = true;
            }
        }
    }
#else
    if (!fd && ANGBAND_DIR_METARUN && ANGBAND_DIR_METARUN[0]) {
        char legacy[1024];
        if (path_build(legacy, sizeof legacy, ANGBAND_DIR_METARUN, META_RAW)) {
            fd = sdl_fopen(legacy, "rb");
            if (fd) {
                log_info("Loading legacy metarun file: %s", legacy);
                found_existing_data = true;
            }
        }
        else
        {
            log_error("load_metarun_data: failed to build legacy path");
        }
    }
#endif

    if (fd) {
        found_existing_data = true;
    }

    if (!fd && create_if_missing) {
        log_info("Creating new versioned metarun file: %s", fn);
        FILE_TYPE(FILE_TYPE_DATA);
        fd = sdl_fmake(fn, 0644);
        if (!fd) return -1;

        /* Write versioned header */
        meta_file_header header;
        header.version_major = METARUN_FILE_VERSION_MAJOR;
        header.version_minor = METARUN_FILE_VERSION_MINOR;
        header.version_patch = METARUN_FILE_VERSION_PATCH;
        header.version_extra = METARUN_FILE_VERSION_EXTRA;
        header.entry_count = 1;

        sdl_write(fd, (cptr)&header, sizeof(header));

        metarun seed;
        reset_defaults(&seed);
        seed.score = compute_metarun_score(&seed);
        sdl_write(fd, (cptr)&seed, sizeof seed);
        sdl_fclose(fd);
        fd = sdl_fopen(fn, "rb");
        /* Only set metarun_created if we truly created a NEW file, not migrating existing data */
        if (!found_existing_data) {
            metarun_created = true;
            log_info("Created brand new metarun - will show story intro");
        } else {
            log_info("Seeded new metarun file from existing data - skipping intro");
        }
    }
    else log_info("Loading existing metarun file: %s", fn);
    if (!fd) return -1;

    /* All metarun files are versioned (v0.9.0+) */
    Sint64 file_size_64 = sdl_size(fd);
    if (file_size_64 < 0 || file_size_64 > INT_MAX) {
        log_error("Invalid metarun file size: %lld",
            (long long)file_size_64);
        sdl_fclose(fd);
        return -1;
    }
    int file_size = (int)file_size_64;
    const char *recovery_reason = NULL;

    meta_file_header header;
    bool interface_settings_migrated = false;
    sdl_seek(fd, 0);
    if (sdl_read(fd, (char*)&header, sizeof(header)) != 0) {
        log_error("Failed to read metarun header");
        sdl_fclose(fd);
        return -1;
    }

    log_info("Loading versioned meta file v%d.%d.%d.%d (%u entries)",
             header.version_major, header.version_minor,
             header.version_patch, header.version_extra, header.entry_count);
    /* Keep the original 0.9.7 interface migration on every platform.  Only
     * Android and iOS need the newer orientation-profile migration. */
#if defined(__ANDROID__) || defined(SIL_IOS)
    interface_settings_migrated = metarun_header_before(&header, 0, 9, 7, 3);
#else
    interface_settings_migrated = metarun_header_before(&header, 0, 9, 7, 0);
#endif

    bool header_matches_current = (header.version_major == METARUN_FILE_VERSION_MAJOR &&
                                   header.version_minor == METARUN_FILE_VERSION_MINOR &&
                                   header.version_patch == METARUN_FILE_VERSION_PATCH &&
                                   header.version_extra == METARUN_FILE_VERSION_EXTRA);
    if (!header_matches_current) {
        log_warn("metarun: file version v%d.%d.%d.%d differs from game version v%d.%d.%d.%d",
                 header.version_major, header.version_minor, header.version_patch, header.version_extra,
                 METARUN_FILE_VERSION_MAJOR, METARUN_FILE_VERSION_MINOR, METARUN_FILE_VERSION_PATCH, METARUN_FILE_VERSION_EXTRA);
    }

    metarun_max = header.entry_count;
    size_t payload = (file_size >= (int)sizeof(meta_file_header))
                   ? (size_t)file_size - sizeof(meta_file_header)
                  : 0;
    size_t entry_size = (metarun_max > 0)
                      ? (payload / (size_t)metarun_max)
                      : 0;

    if (metarun_max > 0 && entry_size > 0) {
        metaruns = mem_alloc_array(metarun_max, metarun);
        sdl_seek(fd, sizeof(meta_file_header));

        if (entry_size == sizeof(metarun)) {
            sdl_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
            for (s16b i = 0; i < metarun_max; i++) {
                if (header.version_major == 0 && header.version_minor < 9) {
                    metaruns[i].blessing_points_spent = 0;
                }
                /* Initialize pending blessing choices for pre-0.9.0.1 saves
                 * (fields were part of reserved_runtime and may contain garbage) */
                if (header.version_major == 0 && header.version_minor == 9 &&
                    header.version_patch == 0 && header.version_extra == 0) {
                    /* Clear pending choices - will be regenerated on first menu open */
                    metaruns[i].pending_blessing_count = 0;
                    for (int j = 0; j < 3; j++) {
                        metaruns[i].pending_blessing_choices[j] = 255;
                    }
                    log_debug("Cleared pending blessing choices for metarun %d (loaded from v0.9.0.0)", i);
                }
                metarun_clamp_and_sync_quests(&metaruns[i]);
                metarun_sanitize_blessing_economy(&metaruns[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
        } else if (entry_size == METARUN_V10_SIZE) {
            metarun_v10 *legacy = mem_alloc_array(metarun_max, metarun_v10);
            sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v10));
            for (s16b i = 0; i < metarun_max; i++) {
                metarun_from_v10(&metaruns[i], &legacy[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
            legacy = mem_free(legacy);
        } else if (entry_size == METARUN_V9_SIZE) {
            metarun_v9 *legacy = mem_alloc_array(metarun_max, metarun_v9);
            sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v9));
            for (s16b i = 0; i < metarun_max; i++) {
                metarun_from_v9(&metaruns[i], &legacy[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
            legacy = mem_free(legacy);
        } else if (entry_size == METARUN_V8_SIZE) {
            metarun_v8 *legacy = mem_alloc_array(metarun_max, metarun_v8);
            sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v8));
            for (s16b i = 0; i < metarun_max; i++) {
                metarun_from_v8(&metaruns[i], &legacy[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
            legacy = mem_free(legacy);
        } else {
            recovery_reason = "versioned meta.raw had unsupported entry size (requires v0.9.0+)";
            log_warn("Unsupported metarun entry size %zu in versioned file; dropping pre-0.9.0 legacy support", entry_size);
            mem_free_null(metaruns);
            metaruns = NULL;
            metarun_max = 0;
        }
    } else if (metarun_max == 0) {
        recovery_reason = "versioned meta.raw reported zero entries";
        log_warn("Versioned meta file contains zero entries");
    } else {
        recovery_reason = "versioned meta.raw had invalid payload size";
        log_warn("Versioned meta file payload %zu does not align with %d entries",
                 payload, metarun_max);
        mem_free_null(metaruns);
        metaruns = NULL;
        metarun_max = 0;
    }

    if (metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            if (interface_settings_migrated)
                metarun_clear_obsolete_interface_options_097(&metaruns[i]);
        }
    }

    if (interface_settings_migrated)
        sdl_reset_interface_settings_to_defaults_for_migration();

    sdl_fclose(fd);

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
    if (!recover_pending_story_scorefile_switch(metar.id)) {
        log_error("Unable to recover the interrupted Tale switch for Tale %u",
            (unsigned)metar.id);
        return -1;
    }
    metarun_clamp_and_sync_quests(&metar);
    metaruns[current_run].completed_quests = metar.completed_quests;
    memcpy(metaruns[current_run].quest_completion_counts,
           metar.quest_completion_counts,
           sizeof(metar.quest_completion_counts));
    metarun_sanitize_blessing_economy(&metar);
    metaruns[current_run].fallen_score_pool = metar.fallen_score_pool;
    metaruns[current_run].blessing_points = metar.blessing_points;
    metaruns[current_run].blessing_points_spent = metar.blessing_points_spent;
    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
    metarun_apply_runtime_effects();
    log_debug("Final current_run=%d, metar: id=%u, deaths=%u, silmarils=%u",
              current_run, metar.id, metar.deaths, metar.silmarils);

    /* ensure its per-run directory exists */
    ensure_run_dir(&metar);

    /* Apply difficulty curses only if this is a newly created metarun */
    if (metarun_created)
    {
        apply_difficulty_curses(&metar);
        save_metaruns(); /* persist the changes */
    }
    else if (interface_settings_migrated)
    {
        save_metaruns(); /* rewrite header and cleared obsolete option bits */
    }

    log_debug("Loaded metarun %d with %d silmarils, %d deaths", metar.id, metar.silmarils, metar.deaths);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Safely write the meta-run array.  Bail out if the indices look     *
 *  wrong - avoids dereferencing a freed/reallocated block.           *
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
    if (SDL_strcasecmp(last_backed_up_file, filepath) != 0) {
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
    SDL_IOStream* fd_src = sdl_fopen(filepath, "rb");
    if (!fd_src) {
        /* Original file doesn't exist, no backup needed */
        log_info("backup_file: original file %s doesn't exist, no backup needed", filepath);
        return 0;
    }

    /* Get file size */
    Sint64 file_size_64 = sdl_size(fd_src);
    if (file_size_64 <= 0) {
        log_info("backup_file: original file %s is empty, no backup needed", filepath);
        sdl_fclose(fd_src);
        return 0;
    }
    if (file_size_64 > INT_MAX) {
        log_error("backup_file: file too large to back up: %lld bytes",
                  (long long)file_size_64);
        sdl_fclose(fd_src);
        return -1;
    }
    int file_size = (int)file_size_64;

    log_info("backup_file: creating backup for %s (size: %d bytes)", filepath, file_size);

    /* Read original file */
    char *buffer = mem_alloc_array(file_size, char);
    if (!buffer) {
        sdl_fclose(fd_src);
        return -1;
    }

    if (sdl_read(fd_src, buffer, file_size) != 0) {
        buffer = mem_free(buffer);
        sdl_fclose(fd_src);
        return -1;
    }
    sdl_fclose(fd_src);

    /* Optimize backup rotation: Only do full rotation once per session/day
     * For frequent saves, just overwrite .bak1 */
    char backup_path1[1024], backup_path2[1024], backup_path3[1024];
    strnfmt(backup_path1, sizeof(backup_path1), "%s.bak1", filepath);
    strnfmt(backup_path2, sizeof(backup_path2), "%s.bak2", filepath);
    strnfmt(backup_path3, sizeof(backup_path3), "%s.bak3", filepath);

    /* Check if this is the first backup of the day (roughly) */
    bool should_rotate = false;
    SDL_IOStream* fd_test1 = sdl_fopen(backup_path1, "rb");
    if (fd_test1) {
        /* Check if bak1 is old enough to warrant rotation (use simple time check) */
        /* If we created a backup within the last hour, don't rotate */
        if (current_time - last_backup_time >= 3600) {  /* 1 hour */
            should_rotate = true;
            log_info("backup_file: enough time passed since last backup, will rotate backups");
        }
        sdl_fclose(fd_test1);
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
        SDL_IOStream* fd_test2 = sdl_fopen(backup_path2, "rb");
        if (fd_test2) {
            sdl_fclose(fd_test2);
            log_debug("backup_file: moving bak2 to bak3");
            if (!fd_move(backup_path2, backup_path3)) {
                log_debug("backup_file: failed to move bak2 to bak3");
            }
        }

        /* Move bak1 to bak2 (if bak1 exists) */
        fd_test1 = sdl_fopen(backup_path1, "rb");
        if (fd_test1) {
            sdl_fclose(fd_test1);
            log_debug("backup_file: moving bak1 to bak2");
            if (!fd_move(backup_path1, backup_path2)) {
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
    SDL_IOStream* fd_dst = sdl_fmake(backup_path1, 0644);
    if (!fd_dst) {
        buffer = mem_free(buffer);
        return -1;
    }

    errr result = sdl_write(fd_dst, buffer, file_size);
    sdl_fclose(fd_dst);
    buffer = mem_free(buffer);

    if (result == 0) {
        log_info("backup_file: successfully created backup for %s", filepath);
        /* Update throttling variables only on successful backup */
        last_backup_time = current_time;
        SDL_strlcpy(last_backed_up_file, filepath, sizeof(last_backed_up_file));
    } else {
        log_error("backup_file: failed to write bak1 for %s", filepath);
    }

    return result;
}

errr save_metaruns(void)
{
    static u32b last_save_time = 0;
    u32b current_time = (u32b)time(NULL);
    char temporary[1024];

    /* Tale activation may deliberately advance last_played beyond the wall
     * clock to make an older array entry unambiguously current. */
    if (current_time < metar.last_played)
        current_time = metar.last_played;

    /* Log save frequency tracking */
    if (last_save_time > 0) {
        u32b time_since_last = current_time - last_save_time;
        log_info("save_metaruns() called again after %u seconds", time_since_last);
    } else {
        log_info("save_metaruns() called for the first time this session");
    }
    last_save_time = current_time;

    refresh_current_metar_score();

    char fn[1024];
    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;
    strnfmt(temporary, sizeof(temporary), "%s.tale-save", fn);

    /* Create backup before saving */
    backup_file(fn);

    log_debug("Before save: current_run=%d, metar: id=%u, deaths=%u, silmarils=%u, score=%u",
              current_run, metar.id, metar.deaths, metar.silmarils, metar.score);

    metarun_clamp_and_sync_quests(&metar);
    metar.last_played      = current_time;
    metaruns[current_run] = metar;            /* safe: array is valid */

    log_debug("After updating array: metaruns[%d]: id=%u, deaths=%u, silmarils=%u, score=%u",
              current_run, metaruns[current_run].id, metaruns[current_run].deaths, metaruns[current_run].silmarils,
              metaruns[current_run].score);

    /* Write beside the live file, then atomically replace it. */
    (void)fd_kill(temporary);
    SDL_IOStream* fd = sdl_fmake(temporary, 0644);
    if (!fd) {
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

    errr result = sdl_write(fd, (cptr)&header, sizeof(header));
    if (result != 0) {
        sdl_fclose(fd);
        (void)fd_kill(temporary);
        log_info("Failed to write metarun header to file");
        return -1;
    }

    /* Write metarun data */
    int bytes_to_write = metarun_max * sizeof(metarun);
    result = sdl_write(fd, (cptr)metaruns, bytes_to_write);
    bool flushed = (result == 0) && SDL_FlushIO(fd);
    errr close_result = sdl_fclose(fd);

    if (result != 0 || !flushed || close_result != 0) {
        (void)fd_kill(temporary);
        log_info("Failed to write or flush metarun data to file");
        return -1;
    }
    if (!fd_move(temporary, fn)) {
        (void)fd_kill(temporary);
        log_error("Failed to atomically replace metarun file");
        return -1;
    }

    log_info("Metarun data saved successfully (%d bytes, %d entries)", bytes_to_write, metarun_max);

    return 0;
}
