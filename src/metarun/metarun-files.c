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
#include "metarun/metarun-internal.h"
#include "platform.h"
#include "score/score_file_compat.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_paths.h"
#include "score/score_postmortem.h"
#include "score/score_runs.h"
#include "scorefile.h"
#include "ui/question.h"
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
typedef enum autoload_recovery_action {
    AUTOLOAD_RECOVERY_MARK_DEAD = 0,
    AUTOLOAD_RECOVERY_KEEP_AND_QUIT,
    AUTOLOAD_RECOVERY_REMOVE,
    AUTOLOAD_RECOVERY_PROCEED
} autoload_recovery_action;

static bool autoload_quit_requested = false;

bool autoload_recovery_quit_requested(void)
{
    return autoload_quit_requested;
}

static autoload_recovery_action autoload_prompt_unavailable_save(
    const char* who)
{
    char desc[768];
    ui_question_option options[] = {
        { 'd', "Mark the character dead and continue", TERM_L_RED, false },
        { 'q', "Keep everything unchanged and quit", TERM_WHITE, false },
        { 'r', "Remove the character from scores and continue", TERM_ORANGE,
            false },
    };

    strnfmt(desc, sizeof(desc),
        "The score ledger says %s is alive, but the character's save is "
        "missing or unreadable. This can be caused by a game bug or by "
        "save-file tampering. Sil-More can detect common save manipulation "
        "patterns; please do not replace, rename, or alter save files. "
        "Choose Quit if you want to try repairing the save first.",
        (who && who[0]) ? who : "this character");

    int choice = ui_question_ask_overlay("Character save unavailable", desc,
        options, N_ELEMENTS(options), UI_QUESTION_GLOBAL,
        UI_QUESTION_GLOBAL, AUTOLOAD_RECOVERY_KEEP_AND_QUIT);
    if (choice < 0)
        return AUTOLOAD_RECOVERY_KEEP_AND_QUIT;
    return (autoload_recovery_action)choice;
}

static autoload_recovery_action autoload_prompt_older_save(const char* who,
    u32b loaded_turns, u32b recorded_turns)
{
    char desc[768];
    ui_question_option options[] = {
        { 'd', "Mark the character dead", TERM_L_RED, false },
        { 'p', "Proceed with the older save", TERM_ORANGE, false },
    };

    strnfmt(desc, sizeof(desc),
        "The save for %s is older than the last recorded run state "
        "(save turn %lu; recorded turn %lu). Replacing a current save with "
        "a backup is a detectable cheating pattern. Please do not swap or "
        "roll back save files. A game bug can also cause this mismatch.",
        (who && who[0]) ? who : "this character",
        (unsigned long)loaded_turns, (unsigned long)recorded_turns);

    int choice = ui_question_ask_overlay("Older save detected", desc,
        options, N_ELEMENTS(options), UI_QUESTION_GLOBAL,
        UI_QUESTION_GLOBAL, 1);
    if (choice == 0)
        return AUTOLOAD_RECOVERY_MARK_DEAD;
    return AUTOLOAD_RECOVERY_PROCEED;
}

static void autoload_report_resolution_failure(const char* who)
{
    char desc[512];
    ui_question_option options[] = {
        { 'q', "Quit without making further changes", TERM_WHITE, false },
    };

    strnfmt(desc, sizeof(desc),
        "Sil-More could not safely update the recovery records for %s. "
        "The game will quit so the score and save files are not changed "
        "again. Check log.txt before retrying.",
        (who && who[0]) ? who : "this character");
    (void)ui_question_ask_overlay("Recovery could not be completed", desc,
        options, N_ELEMENTS(options), UI_QUESTION_GLOBAL,
        UI_QUESTION_GLOBAL, 0);
    autoload_quit_requested = true;
}

static bool autoload_save_loaded_character_as_dead(void)
{
    if (!character_loaded || !p_ptr)
        return false;

    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    SDL_strlcpy(p_ptr->died_from, "their own hand",
        sizeof(p_ptr->died_from));

    bool saved = save_player();
    character_loaded = false;
    character_loaded_dead = false;
    p_ptr->restoring = false;
    return saved;
}

static bool autoload_resolve_live_entry(const char* score_path,
    const high_score* entry, autoload_recovery_action action,
    bool loaded_character)
{
    bool resolved;

    if (!score_path || !entry)
        return false;

    if (action == AUTOLOAD_RECOVERY_MARK_DEAD) {
        if (loaded_character && !autoload_save_loaded_character_as_dead()) {
            log_error("autoload: failed to save rollback character as dead");
            return false;
        }
        resolved = score_mark_alive_entry_dead_at_path(score_path, entry,
            "their own hand");
        if (!resolved)
            return false;
        if (!score_runs_resolve_legacy_entry(entry, SCORE_RECORD_DEAD,
                "their own hand"))
        {
            log_warn("autoload: linked runs.db row could not be marked dead");
        }
        if (!run_mode_is_blitz()) {
            metarun_increment_deaths();
            if (!save_metaruns())
                log_warn("autoload: failed to persist Tale death recovery");
        }
        return true;
    }

    if (action == AUTOLOAD_RECOVERY_REMOVE) {
        resolved = score_remove_alive_entry_at_path(score_path, entry);
        if (!resolved)
            return false;
        if (!score_runs_resolve_legacy_entry(entry, SCORE_RECORD_REMOVED,
                "removed during save recovery"))
        {
            log_warn("autoload: linked runs.db row could not be tombstoned");
        }
        return true;
    }

    return false;
}

static u32b autoload_recorded_turns(const high_score* entry)
{
    int raw_turns = parse_score_int(entry->turns, sizeof(entry->turns), 0);
    u32b recorded = (raw_turns > 0) ? (u32b)raw_turns : 0;
    u32b runs_turns = 0;

    if (score_runs_get_recorded_turns(entry, &runs_turns)
        && runs_turns > recorded)
        recorded = runs_turns;
    return recorded;
}

static bool autoload_accept_loaded_entry(const char* score_path,
    const high_score* entry, const char* who, u32b recorded_turns)
{
    u32b loaded_turns = (playerturn > 0) ? (u32b)playerturn : 0;

    if (loaded_turns >= recorded_turns)
        return true;

    log_warn("autoload: older save detected for '%s' (loaded=%lu, recorded=%lu)",
        who, (unsigned long)loaded_turns, (unsigned long)recorded_turns);
    autoload_recovery_action action = autoload_prompt_older_save(who,
        loaded_turns, recorded_turns);
    if (action == AUTOLOAD_RECOVERY_PROCEED) {
        log_warn("autoload: player chose to proceed with older save '%s'", who);
        return true;
    }

    if (!autoload_resolve_live_entry(score_path, entry,
            AUTOLOAD_RECOVERY_MARK_DEAD, true))
    {
        autoload_report_resolution_failure(who);
    }
    return false;
}

bool autoload_alive_from_scores(void)
{
    char score_path[1024];
    high_score entries[MAX_HISCORES];
    int checked_alive = 0;
    int n_recs = 0;

    log_info("===== autoload_alive_from_scores: FUNCTION CALLED =====");
    autoload_quit_requested = false;
    build_current_score_path(score_path, sizeof(score_path));

    /* Validate first, then copy the small ledger into memory and close it.
     * Recovery uses atomic rename, which requires no open handle on Windows. */
    if (!score_count_alive_entries_at_path_checked(score_path,
            &checked_alive))
    {
        if (SDL_GetPathInfo(score_path, NULL))
            log_error("autoload: score ledger is unreadable: %s", score_path);
        else
            SDL_ClearError();
        return false;
    }
    if (checked_alive <= 0)
        return false;

    SDL_IOStream* saved_fd = highscore_fd;
    byte saved_major = scores_file_version_major;
    byte saved_minor = scores_file_version_minor;
    byte saved_patch = scores_file_version_patch;
    byte saved_extra = scores_file_version_extra;
    u32b saved_entry_count = scores_file_entry_count;

    highscore_fd = score_file_open(score_path, O_RDONLY);
    if (!highscore_fd || scores_file_entry_count > N_ELEMENTS(entries)) {
        log_error("autoload: could not copy validated score ledger: %s",
            score_path);
        if (highscore_fd)
            SDL_CloseIO(highscore_fd);
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return false;
    }

    n_recs = (int)scores_file_entry_count;
    if (n_recs > 0 && highscore_seek(0) == 0) {
        for (int i = 0; i < n_recs; i++) {
            if (highscore_read(&entries[i]) != 0) {
                n_recs = 0;
                break;
            }
        }
    } else {
        n_recs = 0;
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = saved_fd;
    scores_file_version_major = saved_major;
    scores_file_version_minor = saved_minor;
    scores_file_version_patch = saved_patch;
    scores_file_version_extra = saved_extra;
    scores_file_entry_count = saved_entry_count;
    if (n_recs <= 0)
        return false;

    for (int i = 0; i < n_recs; ++i) {
        high_score* entry = &entries[i];
        char how_buf[sizeof(entry->how) + 1];
        char who_buf[sizeof(entry->who) + 1];

        parse_score_string(entry->how, sizeof(entry->how), how_buf,
            sizeof(how_buf));
        if (!streq(how_buf, "(alive and well)"))
            continue;
        parse_score_string(entry->who, sizeof(entry->who), who_buf,
            sizeof(who_buf));
        if (!who_buf[0]) {
            log_warn("autoload: alive entry at index %d has an empty name", i);
            continue;
        }

        u32b recorded_turns = autoload_recorded_turns(entry);
        log_info("autoload: found alive entry '%s' (index %d, turn %lu)",
            who_buf, i, (unsigned long)recorded_turns);

        SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
        process_player_name(true);
        log_info("autoload: savefile path generated: '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (normalized)",
                who_buf);
            if (autoload_accept_loaded_entry(score_path, entry, who_buf,
                    recorded_turns))
                return true;
            if (autoload_quit_requested)
                return false;
            continue;
        }

        /* Some older builds retained spaces in the filename stem. */
        char savefile_backup[1024];
        char alt_temp[128];
        char alt_path[1024];
        SDL_strlcpy(savefile_backup, savefile, sizeof(savefile_backup));
        build_active_savefile_stem(who_buf, alt_temp, sizeof(alt_temp));
        path_build(alt_path, sizeof(alt_path), ANGBAND_DIR_SAVE, alt_temp);
        SDL_strlcpy(savefile, alt_path, sizeof(savefile));
        log_info("autoload: retrying legacy filename '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (legacy filename)",
                who_buf);
            SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
            process_player_name(true);
            SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));
            if (autoload_accept_loaded_entry(score_path, entry, who_buf,
                    recorded_turns))
                return true;
            if (autoload_quit_requested)
                return false;
            continue;
        }
        SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));

        log_warn("autoload: savefile unavailable for '%s'", who_buf);
        autoload_recovery_action action =
            autoload_prompt_unavailable_save(who_buf);
        if (action == AUTOLOAD_RECOVERY_KEEP_AND_QUIT) {
            log_info("autoload: player kept '%s' unchanged and requested quit",
                who_buf);
            autoload_quit_requested = true;
            return false;
        }
        if (!autoload_resolve_live_entry(score_path, entry, action, false)) {
            autoload_report_resolution_failure(who_buf);
            return false;
        }
    }

    return false;
}

static bool build_tale_score_path(char* path, size_t len, u32b tale_id)
{
    metarun key;

    memset(&key, 0, sizeof(key));
    key.id = tale_id;
    return build_meta_path(path, len, &key, "scores.raw");
}

#define TALE_SWITCH_JOURNAL_MAGIC 0x54414C45U /* "TALE" */

typedef struct tale_switch_journal {
    u32b magic;
    u32b outgoing_id;
    u32b incoming_id;
} tale_switch_journal;

static bool tale_switch_recovery_required = false;

static bool build_tale_switch_journal_path(char* path, size_t len)
{
    return score_build_meta_path(path, len, "tale-switch.pending");
}

static bool score_path_exists(const char* path)
{
    if (!path)
        return false;

    /* Existence and readability are deliberately separate checks.  Opening
     * an unreadable ledger would report it as absent and let callers replace
     * data that is still present on disk. */
    return SDL_GetPathInfo(path, NULL);
}

static bool score_path_has_valid_header(const char* path)
{
    score_file_ctx ctx;

    score_file_reset_ctx(&ctx);
    return path && score_file_load_header(&ctx, path);
}

static bool score_path_is_valid_ledger(const char* path)
{
    int alive = 0;

    return score_path_has_valid_header(path)
        && score_count_alive_entries_at_path_checked(path, &alive);
}

static bool create_empty_scorefile(const char* path)
{
    score_file_ctx local_ctx;
    score_file_ctx* previous_ctx;
    SDL_IOStream* file;
    bool closed = false;
    bool empty = false;
    SDL_PathInfo path_info;
    int exclusive_fd;

    if (!path)
        return false;
    if (SDL_GetPathInfo(path, &path_info)) {
        if (path_info.type != SDL_PATHTYPE_FILE || !fd_kill(path))
            return false;
    } else {
        SDL_ClearError();
    }

    /* Claim the pathname exclusively before score_file_open() bootstraps its
     * header.  This prevents a metadata-check failure from ever turning
     * O_CREAT into truncation of an existing but unreadable ledger. */
    safe_setuid_grab();
    exclusive_fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    safe_setuid_drop();
    if (exclusive_fd < 0)
        return false;
    if (close(exclusive_fd) != 0) {
        (void)fd_kill(path);
        return false;
    }
    score_file_reset_ctx(&local_ctx);
    previous_ctx = score_file_set_active_ctx(&local_ctx);
    file = score_file_open(path, O_RDWR | O_CREAT);
    if (file) {
        empty = local_ctx.entry_count == 0
            && SDL_GetIOSize(file) == (Sint64)sizeof(score_file_header);
        closed = SDL_CloseIO(file);
    }
    score_file_set_active_ctx(previous_ctx);
    if (!file || !empty || !closed || !score_path_is_valid_ledger(path)) {
        (void)fd_kill(path);
        return false;
    }
    return true;
}

/* SDL_CopyFile() is not atomic.  Copy beside the destination and then use
 * SDL_RenamePath(), which replaces the destination atomically. */
static bool copy_file_atomically(const char* source, const char* destination)
{
    char temporary[1024];

    if (!source || !destination)
        return false;
    strnfmt(temporary, sizeof(temporary), "%s.tale-copy", destination);
    (void)fd_kill(temporary);
    if (!fd_copy(source, temporary))
        return false;
    if (!fd_move(temporary, destination)) {
        (void)fd_kill(temporary);
        return false;
    }
    return true;
}

static bool snapshot_active_story_scorefile(u32b tale_id,
    bool allow_empty)
{
    char active[1024];
    char snapshot[1024];
    metarun key;

    if (!run_mode_is_blitz() && highscore_fd
        && !SDL_FlushIO(highscore_fd))
    {
        log_error("Unable to flush the active score ledger for Tale %u",
            (unsigned)tale_id);
        return false;
    }
    if (!score_build_meta_path(active, sizeof(active), "scores.raw"))
        return false;
    if (!score_path_is_valid_ledger(active)) {
        if (score_path_exists(active)) {
            log_error("Active score ledger for Tale %u is invalid",
                (unsigned)tale_id);
            return false;
        }
        if (!allow_empty) {
            log_error("Active score ledger for Tale %u is missing",
                (unsigned)tale_id);
            return false;
        }
        if (!create_empty_scorefile(active)) {
            log_error("Unable to initialize the empty score ledger for Tale %u",
                (unsigned)tale_id);
            return false;
        }
    }

    memset(&key, 0, sizeof(key));
    key.id = tale_id;
    ensure_run_dir(&key);
    if (!build_tale_score_path(snapshot, sizeof(snapshot), tale_id))
        return false;
    if (!copy_file_atomically(active, snapshot)) {
        log_error("Unable to snapshot score ledger for Tale %u",
            (unsigned)tale_id);
        return false;
    }
    log_info("Saved active score ledger for Tale %u to %s",
        (unsigned)tale_id, snapshot);
    return true;
}

static bool tale_archive_name_matches(const char* name, u32b tale_id)
{
    char suffix[32];
    size_t name_len;
    size_t suffix_len;

    if (!name || strncmp(name, "scores-", 7) != 0)
        return false;
    strnfmt(suffix, sizeof(suffix), "-%08u.raw", (unsigned)tale_id);
    name_len = strlen(name);
    suffix_len = strlen(suffix);
    return name_len > suffix_len
        && SDL_strcasecmp(name + name_len - suffix_len, suffix) == 0;
}

static bool find_latest_tale_archive_in(const char* directory, u32b tale_id,
    char* result, size_t len)
{
    char best[256] = "";

    if (!directory || !directory[0])
        return false;
#ifdef WINDOWS
    WIN32_FIND_DATA find_data;
    char pattern[1024];
    if (!path_build(pattern, sizeof(pattern), directory, "scores-*.raw"))
        return false;
    HANDLE find = FindFirstFile(pattern, &find_data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            char candidate[1024];

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            if (tale_archive_name_matches(find_data.cFileName, tale_id)
                && path_build(candidate, sizeof(candidate), directory,
                    find_data.cFileName)
                && score_path_is_valid_ledger(candidate)
                && (!best[0] || strcmp(find_data.cFileName, best) > 0))
            {
                SDL_strlcpy(best, find_data.cFileName, sizeof(best));
            }
        } while (FindNextFile(find, &find_data));
        FindClose(find);
    }
#else
    DIR* dir = opendir(directory);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            char candidate[1024];

            if (tale_archive_name_matches(entry->d_name, tale_id)
                && path_build(candidate, sizeof(candidate), directory,
                    entry->d_name)
                && score_path_is_valid_ledger(candidate)
                && (!best[0] || strcmp(entry->d_name, best) > 0))
            {
                SDL_strlcpy(best, entry->d_name, sizeof(best));
            }
        }
        closedir(dir);
    }
#endif
    return best[0] && path_build(result, len, directory, best);
}

static void path_parent_directory(char* path)
{
    char* slash;
    char* backslash;
    char* separator;

    if (!path)
        return;
    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    separator = slash;
    if (backslash && (!separator || backslash > separator))
        separator = backslash;
    if (separator)
        *separator = '\0';
    else
        path[0] = '\0';
}

static bool find_legacy_tale_archive(u32b tale_id, char* result, size_t len)
{
    char active[1024];
    char meta_directory[1024] = "";

    if (score_build_meta_path(active, sizeof(active), "scores.raw")) {
        SDL_strlcpy(meta_directory, active, sizeof(meta_directory));
        path_parent_directory(meta_directory);
        if (find_latest_tale_archive_in(meta_directory, tale_id, result, len))
            return true;
    }

    /* Older standard builds accidentally archived under the install apex
     * rather than the per-user meta directory.  Search there for migration. */
    if (ANGBAND_DIR_APEX && ANGBAND_DIR_APEX[0]
        && (!meta_directory[0]
            || SDL_strcasecmp(meta_directory, ANGBAND_DIR_APEX) != 0))
    {
        return find_latest_tale_archive_in(ANGBAND_DIR_APEX, tale_id,
            result, len);
    }
    return false;
}

static bool write_tale_switch_journal(u32b outgoing_id, u32b incoming_id)
{
    char journal_path[1024];
    char temporary[1024];
    tale_switch_journal journal;
    SDL_IOStream* file;
    bool ok;

    if (!build_tale_switch_journal_path(journal_path, sizeof(journal_path)))
        return false;
    if (score_path_exists(journal_path)) {
        tale_switch_recovery_required = true;
        log_error("A previous Tale switch is still pending recovery");
        return false;
    }

    journal.magic = TALE_SWITCH_JOURNAL_MAGIC;
    journal.outgoing_id = outgoing_id;
    journal.incoming_id = incoming_id;
    strnfmt(temporary, sizeof(temporary), "%s.tmp", journal_path);
    (void)fd_kill(temporary);
    file = sdl_fmake(temporary, 0644);
    if (!file)
        return false;
    ok = SDL_WriteIO(file, &journal, sizeof(journal)) == sizeof(journal)
        && SDL_FlushIO(file);
    if (sdl_fclose(file) != 0)
        ok = false;
    if (!ok || !fd_move(temporary, journal_path)) {
        (void)fd_kill(temporary);
        return false;
    }
    tale_switch_recovery_required = true;
    return true;
}

static bool read_tale_switch_journal(tale_switch_journal* journal)
{
    char path[1024];
    SDL_IOStream* file;
    bool ok;

    if (!journal
        || !build_tale_switch_journal_path(path, sizeof(path)))
    {
        return false;
    }
    file = sdl_fopen(path, "rb");
    if (!file)
        return false;
    ok = SDL_GetIOSize(file) == (Sint64)sizeof(*journal)
        && SDL_ReadIO(file, journal, sizeof(*journal)) == sizeof(*journal)
        && journal->magic == TALE_SWITCH_JOURNAL_MAGIC;
    if (sdl_fclose(file) != 0)
        ok = false;
    return ok;
}

bool finish_story_scorefile_switch(void)
{
    char path[1024];

    if (!build_tale_switch_journal_path(path, sizeof(path)))
        return false;
    if (!score_path_exists(path)) {
        tale_switch_recovery_required = false;
        return true;
    }
    if (!fd_kill(path)) {
        tale_switch_recovery_required = true;
        log_error("Unable to finish the pending Tale switch transaction");
        return false;
    }
    tale_switch_recovery_required = false;
    return true;
}

bool story_scorefile_switch_recovery_required(void)
{
    return tale_switch_recovery_required;
}

static bool reopen_active_story_scorefile(void)
{
    char active[1024];

    if (!score_build_meta_path(active, sizeof(active), "scores.raw"))
        return false;
    safe_setuid_grab();
    highscore_fd = score_file_open(active, O_RDWR);
    safe_setuid_drop();
    return highscore_fd != NULL;
}

static bool replace_active_story_scorefile(const char* source)
{
    char active[1024];
    bool reopen_story;
    bool ok;

    if (!source
        || !score_build_meta_path(active, sizeof(active), "scores.raw"))
    {
        return false;
    }
    reopen_story = !run_mode_is_blitz() && highscore_fd != NULL;
    if (reopen_story) {
        if (!SDL_FlushIO(highscore_fd)) {
            log_error("Unable to flush the active Story score ledger");
            return false;
        }
        if (!SDL_CloseIO(highscore_fd)) {
            highscore_fd = NULL;
            log_error("Unable to close the active Story score ledger");
            return false;
        }
        highscore_fd = NULL;
    }

    ok = copy_file_atomically(source, active);
    if (reopen_story) {
        if (!reopen_active_story_scorefile())
            ok = false;
    }
    return ok;
}

bool restore_story_scorefile_for_tale(u32b tale_id)
{
    char archived[1024];

    if (!build_tale_score_path(archived, sizeof(archived), tale_id)
        || !score_path_is_valid_ledger(archived))
    {
        log_error("Cannot restore the score ledger for Tale %u",
            (unsigned)tale_id);
        return false;
    }
    if (!replace_active_story_scorefile(archived)) {
        log_error("Unable to restore the active ledger for Tale %u",
            (unsigned)tale_id);
        return false;
    }
    return true;
}

bool recover_pending_story_scorefile_switch(u32b selected_tale_id)
{
    char path[1024];
    tale_switch_journal journal;

    if (!build_tale_switch_journal_path(path, sizeof(path)))
        return false;
    if (!score_path_exists(path)) {
        tale_switch_recovery_required = false;
        return true;
    }
    tale_switch_recovery_required = true;
    if (!read_tale_switch_journal(&journal)) {
        log_error("The pending Tale switch journal is corrupt");
        return false;
    }
    if (selected_tale_id != journal.outgoing_id
        && selected_tale_id != journal.incoming_id)
    {
        log_error("Pending Tale switch references Tales %u and %u, but "
            "metadata selected Tale %u", (unsigned)journal.outgoing_id,
            (unsigned)journal.incoming_id, (unsigned)selected_tale_id);
        return false;
    }
    log_warn("Recovering an interrupted Tale switch; restoring Tale %u",
        (unsigned)selected_tale_id);
    return restore_story_scorefile_for_tale(selected_tale_id)
        && finish_story_scorefile_switch();
}

bool switch_story_scorefile_between_tales(u32b outgoing_id,
    u32b incoming_id, bool create_empty_incoming,
    bool allow_empty_outgoing)
{
    char incoming[1024];
    char legacy[1024];
    metarun incoming_key;
    int incoming_alive = 0;
    bool story_was_open = !run_mode_is_blitz() && highscore_fd != NULL;

    if (!snapshot_active_story_scorefile(outgoing_id,
            allow_empty_outgoing))
    {
        return false;
    }

    memset(&incoming_key, 0, sizeof(incoming_key));
    incoming_key.id = incoming_id;
    ensure_run_dir(&incoming_key);
    if (!build_tale_score_path(incoming, sizeof(incoming), incoming_id))
        return false;

    if (create_empty_incoming) {
        if (!create_empty_scorefile(incoming)) {
            log_error("Unable to initialize a score ledger for new Tale %u",
                (unsigned)incoming_id);
            return false;
        }
    } else if (!score_path_is_valid_ledger(incoming)
        && find_legacy_tale_archive(incoming_id, legacy, sizeof(legacy))
        && score_path_is_valid_ledger(legacy))
    {
        if (!copy_file_atomically(legacy, incoming)) {
            log_error("Unable to migrate legacy score ledger for Tale %u",
                (unsigned)incoming_id);
            return false;
        }
        log_info("Migrated legacy score ledger for Tale %u from %s",
            (unsigned)incoming_id, legacy);
    }

    if (!score_path_is_valid_ledger(incoming)) {
        log_error("No score ledger is available for Tale %u",
            (unsigned)incoming_id);
        return false;
    }
    if (!score_count_alive_entries_at_path_checked(incoming,
            &incoming_alive))
    {
        log_error("Score ledger for Tale %u could not be validated",
            (unsigned)incoming_id);
        return false;
    }
    if (incoming_alive > 0) {
        log_error("Refusing to load Tale %u because its archived ledger still "
            "contains a running character", (unsigned)incoming_id);
        return false;
    }

    if (!write_tale_switch_journal(outgoing_id, incoming_id))
        return false;
    if (!replace_active_story_scorefile(incoming)) {
        bool restored = restore_story_scorefile_for_tale(outgoing_id);

        if (restored && story_was_open && !highscore_fd)
            restored = reopen_active_story_scorefile();
        if (restored)
            (void)finish_story_scorefile_switch();
        return false;
    }

    /* An ordinary death does not create a timestamped postmortem archive.
     * Once Final Look changes the active Tale, retain its outgoing canonical
     * snapshot so Halls of Mandos can still find the just-finished hero. */
    if (death_spectator_active() && !score_postmortem_path()[0]) {
        char outgoing[1024];

        if (build_tale_score_path(outgoing, sizeof(outgoing), outgoing_id))
            score_postmortem_set_path(outgoing);
    }

    log_info("Activated score ledger for Tale %u", (unsigned)incoming_id);
    return true;
}

static bool archive_active_story_scorefile(u32b tale_id, char* archive,
    size_t archive_len)
{
    char active[1024];
    char archive_leaf[128];
    char stamp[32];
    time_t now;
    struct tm* lt;

    if (!archive || archive_len == 0)
        return false;
    archive[0] = '\0';
    if (!run_mode_is_blitz() && highscore_fd
        && !SDL_FlushIO(highscore_fd))
    {
        log_error("Unable to flush Tale %u before archiving its score ledger",
            (unsigned)tale_id);
        return false;
    }
    if (!score_build_meta_path(active, sizeof(active), "scores.raw")
        || !score_path_is_valid_ledger(active))
    {
        log_error("Cannot archive the invalid score ledger for Tale %u",
            (unsigned)tale_id);
        return false;
    }

    now = time(NULL);
    lt = localtime(&now);
    if (lt)
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", lt);
    else
        SDL_strlcpy(stamp, "unknown", sizeof(stamp));
    strnfmt(archive_leaf, sizeof(archive_leaf), "scores-%s-%08u.raw",
        stamp, (unsigned)tale_id);
    if (!score_build_meta_path(archive, archive_len, archive_leaf)
        || !copy_file_atomically(active, archive))
    {
        archive[0] = '\0';
        log_error("Unable to create the timestamped score archive for Tale %u",
            (unsigned)tale_id);
        return false;
    }
    return true;
}

bool begin_story_scorefile_rollover(u32b outgoing_id, u32b incoming_id,
    bool allow_empty_outgoing)
{
    char archive[1024];

    if (run_mode_is_blitz())
        return false;
    if (!snapshot_active_story_scorefile(outgoing_id,
            allow_empty_outgoing))
    {
        return false;
    }
    if (!archive_active_story_scorefile(outgoing_id, archive,
            sizeof(archive)))
    {
        return false;
    }
    if (!switch_story_scorefile_between_tales(outgoing_id, incoming_id,
            true, allow_empty_outgoing))
    {
        return false;
    }
    score_postmortem_set_path(archive);
    return true;
}

/*
 * Delete the current high-score file and immediately recreate an empty
 * placeholder so subsequent sdl_fopen() calls succeed without special cases.
 */
bool clear_scorefile(void)
{
    char cur_path[1024];
    char arch_path[1024];
    char replacement[1024];
    bool was_open = (highscore_fd != NULL);

    if (run_mode_is_blitz()) {
        log_warn("Refusing to clear the Story score ledger during Blitz");
        return false;
    }

    /* Keep a deterministic per-tale copy so Tale Statistics can reactivate
     * this tale later.  The timestamped archive below remains as a backup. */
    if (!snapshot_active_story_scorefile(metar.id, true)) {
        log_error("Score ledger clear aborted because Tale %u could not be "
            "snapshotted", (unsigned)metar.id);
        return false;
    }

    /* Full path to "scores.raw" */
    if (!score_build_meta_path(cur_path, sizeof(cur_path), "scores.raw"))
        return false;

    if (!archive_active_story_scorefile(metar.id, arch_path,
            sizeof(arch_path))) {
        log_error("Score ledger clear aborted because the timestamped archive "
            "could not be created");
        return false;
    }

    strnfmt(replacement, sizeof(replacement), "%s.tale-clear", cur_path);
    if (!create_empty_scorefile(replacement)) {
        log_error("Unable to prepare an empty story score ledger");
        return false;
    }

    /* Close existing descriptor if open */
    if (was_open) {
        if (!SDL_CloseIO(highscore_fd)) {
            highscore_fd = NULL;
            (void)reopen_active_story_scorefile();
            log_error("Unable to close the Story score ledger before clearing");
            return false;
        }
        highscore_fd = NULL;
    }

    if (!fd_move(replacement, cur_path)) {
        (void)fd_kill(replacement);
        if (was_open)
            (void)reopen_active_story_scorefile();
        log_error("Unable to activate the empty story score ledger");
        return false;
    }
    score_postmortem_set_path(arch_path);

    /* If the file was previously open, reopen it for read/write */
    if (was_open) {
        if (!reopen_active_story_scorefile()) {
            bool restored = restore_story_scorefile_for_tale(metar.id);

            if (restored)
                (void)reopen_active_story_scorefile();
            log_error("The cleared story score ledger could not be reopened");
            return false;
        }
    }
    return true;
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
    if (!run_mode_is_blitz() && p_ptr
        && (p_ptr->wizard || (p_ptr->noscore & 0x0008))
        && (p_ptr->noscore & 0x000F)) {
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
            if (SDL_strncasecmp(filename, "blitz_", 6) == 0) continue;

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
            if (SDL_strncasecmp(filename, "blitz_", 6) == 0) continue;

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
