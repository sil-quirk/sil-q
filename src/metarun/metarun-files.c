/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#ifndef WINDOWS
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#endif

#include "angband.h"
#include "blitz.h"
#include "metarun/metarun-files.h"
#include "fs/file.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "fs/savefile-name.h"
#include "log/log.h"
#include "metarun.h"
#include "platform.h"
#include "score/score_file_compat.h"
#include "score/score_io.h"
#include "score/score_paths.h"
#include "score/score_postmortem.h"
#include "scorefile.h"
#include "externs.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef WINDOWS
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
bool autoload_alive_from_scores(void)
{
    log_info("===== autoload_alive_from_scores: FUNCTION CALLED =====");
    char score_path[1024];
    build_current_score_path(score_path, sizeof(score_path));

    /* Preserve global scorefile state */
    SDL_IOStream* saved_fd = highscore_fd;
    byte saved_major = scores_file_version_major;
    byte saved_minor = scores_file_version_minor;
    byte saved_patch = scores_file_version_patch;
    byte saved_extra = scores_file_version_extra;
    u32b saved_entry_count = scores_file_entry_count;

    /* Open with version detection (read/write so we can patch entries) */
    highscore_fd = score_file_open(score_path, O_RDWR | O_CREAT);
    if (!highscore_fd) {
        log_warn("autoload: could not open scorefile: %s", score_path);
        /* restore */
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return false;
    }

    /* Determine number of records */
    int n_recs;
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_END);
    long file_size = SDL_TellIO(highscore_fd);
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET);

    long payload = file_size - (long)sizeof(score_file_header);
    if (payload < 0) payload = 0;
    n_recs = payload / (long)sizeof(high_score);
    /* Prefer header entry count if sane */
    if (scores_file_entry_count > 0 && (int)scores_file_entry_count <= n_recs)
        n_recs = (int)scores_file_entry_count;
    log_trace("autoload: scorefile n_recs=%d header_count=%u", n_recs, scores_file_entry_count);

    if (n_recs <= 0) {
        SDL_CloseIO(highscore_fd);
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return false;
    }

    /* Iterate alive entries */
    for (int i = 0; i < n_recs; ++i) {
        if (highscore_seek(i)) break;
        high_score entry;
        if (highscore_read(&entry)) break; /* EOF */
        if (strcmp(entry.how, "(alive and well)") != 0) continue;

        char who_buf[sizeof entry.who + 1];
        memset(who_buf, 0, sizeof who_buf);
        SDL_strlcpy(who_buf, entry.who, sizeof(who_buf));
        /* Trim trailing spaces */
        for (int t = (int)strlen(who_buf) - 1; t >= 0; --t) {
            if (who_buf[t] == ' ' || who_buf[t] == '\t') who_buf[t] = '\0'; else break;
        }
        if (!who_buf[0]) {
            log_warn("autoload: alive entry at index %d has empty name, skipping", i);
            continue;
        }
        log_info("autoload: found alive entry '%s' (index %d) - attempting load", who_buf, i);

        SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
        process_player_name(true);

        log_info("autoload: savefile path generated: '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (normalized)", who_buf);
            SDL_CloseIO(highscore_fd);
            highscore_fd = saved_fd;
            scores_file_version_major = saved_major;
            scores_file_version_minor = saved_minor;
            scores_file_version_patch = saved_patch;
            scores_file_version_extra = saved_extra;
            scores_file_entry_count = saved_entry_count;
            return true;
        }

        /* Legacy spaced filename attempt */
        char savefile_backup[1024];
        char alt_temp[128];
        char alt_path[1024];
        SDL_strlcpy(savefile_backup, savefile, sizeof(savefile_backup));
        build_active_savefile_stem(who_buf, alt_temp, sizeof(alt_temp));
        path_build(alt_path, sizeof(alt_path), ANGBAND_DIR_SAVE, alt_temp);
        SDL_strlcpy(savefile, alt_path, sizeof(savefile));
        log_info("autoload: retrying with legacy spaced filename '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (legacy spaced)", who_buf);
            /* Restore canonical name */
            SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
            process_player_name(true);
            SDL_CloseIO(highscore_fd);
            highscore_fd = saved_fd;
            scores_file_version_major = saved_major;
            scores_file_version_minor = saved_minor;
            scores_file_version_patch = saved_patch;
            scores_file_version_extra = saved_extra;
            scores_file_entry_count = saved_entry_count;
            SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));
            return true;
        }
        SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));

        /* Mark as dead and continue */
#if ANTICHEAT
        log_warn("autoload: savefile missing/corrupt for '%s' - marking dead", who_buf);
        strnfmt(entry.how, sizeof entry.how, "%-.49s", "their own hand");
        if (highscore_seek(i) == 0) {
            highscore_write(&entry);
        }
        if (!run_mode_is_blitz()) {
            metarun_increment_deaths();
            (void)save_metaruns();
        }
        msg_format("Warning: Alive entry '%s' had no valid savefile. Marked as dead.", who_buf);
        msg_print("Please do not tamper with savefiles.");
        message_flush();
#else
        log_warn("autoload: savefile missing/corrupt for '%s' - skipping (ANTICHEAT disabled)", who_buf);
        /* Continue to next entry without marking as dead */
#endif
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = saved_fd;
    scores_file_version_major = saved_major;
    scores_file_version_minor = saved_minor;
    scores_file_version_patch = saved_patch;
    scores_file_version_extra = saved_extra;
    scores_file_entry_count = saved_entry_count;
    return false;
}

/*
 * Delete the current high-score file and immediately recreate an empty
 * placeholder so subsequent sdl_fopen() calls succeed without special cases.
 */
void clear_scorefile(void)
{
    char cur_path[1024];
    bool was_open = (highscore_fd != NULL);

    score_postmortem_clear();

    /* Full path to "scores.raw" */
    score_build_meta_path(cur_path, sizeof(cur_path), "scores.raw");

    /* Close existing descriptor if open */
    if (was_open) {
        SDL_CloseIO(highscore_fd);
        highscore_fd = NULL;
    }

    /* If the file exists and is non-empty, archive it with timestamp */
    {
        /* Peek size */
        safe_setuid_grab();
        int fd_probe = open(cur_path, O_RDONLY);
        off_t sz = -1;
        if (fd_probe >= 0) {
            sz = lseek(fd_probe, 0, SEEK_END);
            close(fd_probe);
        }
        safe_setuid_drop();

        if (sz > 0) {
            /* Build archive filename: scores-YYYYMMDD-HHMMSS-<run>.raw */
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            char stamp[32];
            if (lt) strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", lt);
            else SDL_strlcpy(stamp, "unknown", sizeof stamp);

            /* Include run id if available (metar declared in metarun.h) */
            extern metarun metar; /* declared in metarun.h */
            char arch_leaf[128];
            strnfmt(arch_leaf, sizeof arch_leaf, "scores-%s-%08u.raw",
                    stamp, (unsigned)metar.id);

            char arch_path[1024];
            path_build(arch_path, sizeof arch_path, ANGBAND_DIR_APEX, arch_leaf);

            /* Try to rename; if it fails, fall back to delete */
            safe_setuid_grab();
            int rn = rename(cur_path, arch_path);
            safe_setuid_drop();
            if (rn == 0) {
                score_postmortem_set_path(arch_path);
            } else {
                score_postmortem_clear();
                (void)fd_kill(cur_path); /* fallback */
            }
        }
        else {
            /* Nothing useful to archive; just remove it */
            score_postmortem_clear();
            (void)fd_kill(cur_path);
        }
    }

    /* Re-create a zero-length file properly */
    safe_setuid_grab();
    SDL_IOStream* fd_new = sdl_fmake(cur_path, 0644);
    if (fd_new) sdl_fclose(fd_new);
    safe_setuid_drop();

    /* If the file was previously open, reopen it for read/write */
    if (was_open) {
        safe_setuid_grab();
        highscore_fd = score_file_open(cur_path, O_RDWR);
        safe_setuid_drop();
    }
}

/*
 * Metarun finalizer: iterate all "alive" entries in scores.raw.
 * For each entry, attempt to load the savefile by name; if load succeeds,
 * flag the character as dead by their own hand and save back. In either
 * case, patch the score entry's how field to "their own hand".
 */
void metarun_finalize_scores_and_saves(void)
{
    log_info("finalize: entry (wizard=%d, noscore=0x%04X, savefile='%s')",
             p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
             p_ptr ? (unsigned)p_ptr->noscore : 0,
             savefile);
    char score_path[1024];
    score_build_meta_path(score_path, sizeof(score_path), "scores.raw");

    /* Open for read/write so we can patch entries */
    int fd_local;
    safe_setuid_grab();
    fd_local = open(score_path, O_RDWR | O_CREAT, 0644);
    safe_setuid_drop();
    if (fd_local < 0) {
    log_warn("finalize: could not open scorefile: %s", score_path);
        return;
    }

    off_t file_end = lseek(fd_local, 0, SEEK_END);
    off_t payload2 = file_end - (off_t)sizeof(score_file_header);  /* All scores files are versioned */
    int n_recs = (int)(payload2 / (off_t)sizeof(high_score));
    if (n_recs <= 0) {
        safe_setuid_grab();
        close(fd_local);
        safe_setuid_drop();
        return;
    }

    int patched = 0;
    for (int i = 0; i < n_recs; i++) {
        high_score entry;
        off_t entry_offset = (off_t)sizeof(score_file_header)
            + (off_t)i * (off_t)sizeof entry;

        if (lseek(fd_local, entry_offset, SEEK_SET) < 0)
            break;
        ssize_t got = read(fd_local, &entry, sizeof entry);
        if (got != sizeof entry) break;

        /* Only touch entries marked as alive */
        if (strcmp(entry.how, "(alive and well)") != 0) continue;

        /* Patch score entry regardless of save success */
        strnfmt(entry.how, sizeof entry.how, "%-.49s", "their own hand");
        if (lseek(fd_local, entry_offset, SEEK_SET) >= 0) {
            (void)write(fd_local, &entry, sizeof entry);
        }
        patched++;
    }

    safe_setuid_grab();
    close(fd_local);
    safe_setuid_drop();
    log_info("finalize: patched %d alive entries to 'their own hand'", patched);

    /*
     * If the current character is a noscore wizard/debug run, purge their
     * savefile entirely as part of metarun cleanup, so it can't be resumed.
     *
     * Harmonized with start_new_metarun(): allow either wizard OR debug
     * (0x0008) in combination with any noscore bit (0x000F).
     */
    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008)) && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            int rc;
            safe_setuid_grab();
            rc = fd_kill(savefile);
            safe_setuid_drop();
            if (rc == 0) {
                log_info("finalize: deleted noscore wizard/debug savefile '%s'", savefile);
            } else {
                log_warn("finalize: failed to delete noscore wizard/debug savefile '%s'", savefile);
            }
        }
    } else {
        log_info("finalize: no direct purge in finalize (wizard=%d, noscore=0x%04X)",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0);
    }
}

/*
 * Backup all save files to a timestamped ZIP archive and delete originals
 * Called when starting a new metarun to preserve old saves
 */
void backup_and_clear_saves(void)
{
    char save_dir[1024];

    /* Use the correct save directory - ANGBAND_DIR_SAVE points to lib/save */
    strnfmt(save_dir, sizeof(save_dir), "%s", ANGBAND_DIR_SAVE);

    log_info("Checking for save files to backup in: %s", save_dir);

    /* Fast check: Try to open a few common save file patterns to see if anything exists */
    bool has_files = false;
    char test_patterns[][32] = {"*.sav", "*.dat", "*.txt", "character.sav", "save.dat", "Fëanor", "player"};

    for (int i = 0; i < 7 && !has_files; i++) {
        char test_path[1024];
        path_build(test_path, sizeof(test_path), save_dir, test_patterns[i]);

        log_trace("Checking for save file pattern: %s", test_path);

        /* Quick test using sdl_fopen - much faster than popen */
        SDL_IOStream* test_fd = sdl_fopen(test_path, "rb");
        if (test_fd) {
            sdl_fclose(test_fd);
            has_files = true;
            log_trace("Found save file: %s", test_path);
            break;
        } else {
            log_trace("File not found: %s", test_path);
        }
    }

    /* Also try to detect ANY file in the directory using a directory listing approach */
    if (!has_files) {
        log_trace("No specific patterns found, checking directory contents...");

        /* Try some common character names and generic file patterns */
        char common_patterns[][32] = {"save", "char", "game", "*"};

        for (int i = 0; i < 4 && !has_files; i++) {
            char test_path[1024];
            path_build(test_path, sizeof(test_path), save_dir, common_patterns[i]);

            log_trace("Checking directory pattern: %s", test_path);

            SDL_IOStream* test_fd = sdl_fopen(test_path, "rb");
            if (test_fd) {
                sdl_fclose(test_fd);
                has_files = true;
                log_trace("Found file with pattern: %s", test_path);
                break;
            }
        }
    }

    /* Super fast exit if no save files exist */
    if (!has_files) {
        log_info("No save files found - skipping backup/clear process");
        log_trace("Backup skipped because no save files were detected");
        return;  /* Exit immediately, no UI messages needed */
    }

    /* Create timestamped backup folder */
    char backup_folder[1024];
    char timestamp[64];
    time_t now;
    struct tm *timeinfo;

    /* Display progress message to user */
    prt("[Creating save file backup folder...]", 0, 0);
    Term_fresh();

    log_info("Found save files to backup and clear");
    log_trace("Starting folder-based backup process for save files");

    /* Get current timestamp for backup folder name */
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

    /* Create backup folder with timestamp */
    path_build(backup_folder, sizeof(backup_folder), save_dir, format("saves_metarun_%s", timestamp));

    log_info("Creating backup folder: %s", backup_folder);
    log_trace("Full backup folder path: %s", backup_folder);

    /* Create the backup directory */
    #ifdef WINDOWS
    if (_mkdir(backup_folder) != 0) {
        log_warn("Failed to create backup folder: %s", backup_folder);
        return;
    }
    #else
    if (mkdir(backup_folder, 0755) != 0) {
        log_warn("Failed to create backup folder: %s", backup_folder);
        return;
    }
    #endif

    /* Move ALL files to backup folder (except .gitignore and existing backup folders) */
    int files_moved = 0;


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

            /* Skip .gitignore and backup folders */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, "saves_metarun_")) continue; /* Skip existing backup folders */

            /* Move this file to backup folder */
            char old_path[1024], new_path[1024];
            path_build(old_path, sizeof(old_path), save_dir, filename);
            path_build(new_path, sizeof(new_path), backup_folder, filename);

            /* Use rename() to move the file (atomic operation) */
            if (rename(old_path, new_path) == 0) {
                files_moved++;
                log_trace("Moved file to backup: %s", filename);
            } else {
                log_trace("Failed to move file: %s", filename);
            }

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

            /* Skip .gitignore and backup folders */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, "saves_metarun_")) continue; /* Skip existing backup folders */

            /* Move this file to backup folder */
            char old_path[1024], new_path[1024];
            path_build(old_path, sizeof(old_path), save_dir, filename);
            path_build(new_path, sizeof(new_path), backup_folder, filename);

            /* Use rename() to move the file (atomic operation) */
            if (rename(old_path, new_path) == 0) {
                files_moved++;
                log_trace("Moved file to backup: %s", filename);
            } else {
                log_trace("Failed to move file: %s", filename);
            }
        }
        closedir(dir);
    }
    #endif

    if (files_moved > 0) {
        log_info("Save backup completed successfully: %s (%d files moved)", backup_folder, files_moved);
        prt("[Save files moved to backup folder]", 0, 0);
        Term_fresh();
    } else {
        log_info("No files found to move to backup");
        /* Remove empty backup folder if no files were moved */
        #ifdef WINDOWS
        _rmdir(backup_folder);
        #else
        rmdir(backup_folder);
        #endif
    }

    log_trace("Folder-based backup process completed");
}

























