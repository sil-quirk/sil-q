/* File: init2.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <stdio.h>
#include "metarun.h"
#include "score/score_guid.h"

#include "h-define.h"
#include "init.h"
#include <SDL3/SDL_filesystem.h>

#define SIL_USER_ROOT "sil-more"
#define SIL_USER_DATA_DIR "data"
#define SIL_USER_SAVE_DIR "save"
#define SIL_USER_META_DIR "meta"
#define SIL_USER_META_RUNS "metaruns"

static bool is_path_separator(char ch)
{
#ifdef WINDOWS
    return (ch == '\\') || (ch == '/');
#else
    return (ch == '/');
#endif
}

static void strip_trailing_separators(char* path)
{
    size_t len = strlen(path);
    while (len > 0 && is_path_separator(path[len - 1]))
    {
        path[--len] = '\0';
    }
}

static bool build_user_root_path(char* buf, size_t len)
{
    const char* base = SDL_GetUserFolder(SDL_FOLDER_SAVEDGAMES);
    char temp[1024];

    if ((!base || !*base))
        base = SDL_GetUserFolder(SDL_FOLDER_HOME);

    if (base && *base)
    {
        if (SDL_strlcpy(temp, base, sizeof(temp)) >= sizeof(temp))
            return false;
        strip_trailing_separators(temp);
        return path_build(buf, len, temp, SIL_USER_ROOT);
    }

    char* pref = SDL_GetPrefPath("Sil-QH", SIL_USER_ROOT);
    if (!pref)
        return false;

    bool ok = false;
    if (SDL_strlcpy(buf, pref, len) < len)
    {
        strip_trailing_separators(buf);
        ok = true;
    }
    SDL_free(pref);
    return ok;
}

static void ensure_directory_exists(const char* path, const char* label)
{
    if (!path || !*path)
        return;

    if (!SDL_CreateDirectory(path))
    {
        log_warn("init_file_paths: unable to create %s directory '%s': %s",
            label ? label : "user", path, SDL_GetError());
    }
}

static void seed_user_data_from_install(const char* user_data_dir)
{
    if (!user_data_dir || !*user_data_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    char install_data_dir[1024];
    if (!path_build(install_data_dir, sizeof(install_data_dir), ANGBAND_DIR, "data"))
        return;

    SDL_PathInfo install_info;
    if (!SDL_GetPathInfo(install_data_dir, &install_info)
        || install_info.type != SDL_PATHTYPE_DIRECTORY)
        return;

    int entry_count = 0;
    char** entries = SDL_GlobDirectory(install_data_dir, "*.raw", 0, &entry_count);
    if (!entries)
        return;

    for (int i = 0; entries[i]; ++i)
    {
        char source[1024];
        char destination[1024];

        if (!path_build(source, sizeof(source), install_data_dir, entries[i]))
            continue;
        if (!path_build(destination, sizeof(destination), user_data_dir, entries[i]))
            continue;

        if (SDL_GetPathInfo(destination, NULL))
            continue;

        if (!SDL_CopyFile(source, destination))
        {
            log_warn("init_file_paths: unable to copy '%s' to '%s': %s",
                source, destination, SDL_GetError());
        }
    }

    SDL_free(entries);
}

typedef bool (*version_check_fn)(const char* path);

static void migrate_legacy_metarun_layout(const char* meta_root, const char* metarun_dir);

static bool copy_leaf_if_needed(const char* src_dir, const char* dst_dir, const char* leaf,
    const char* label, version_check_fn needs_refresh)
{
    char src[1024];
    char dst[1024];
    SDL_PathInfo info;

    if (!src_dir || !dst_dir || !leaf)
        return false;

    if (!path_build(src, sizeof(src), src_dir, leaf))
        return false;
    if (!SDL_GetPathInfo(src, &info) || info.type != SDL_PATHTYPE_FILE)
        return false;

    if (!path_build(dst, sizeof(dst), dst_dir, leaf))
        return false;

    bool dest_exists = SDL_GetPathInfo(dst, NULL);
    bool should_copy = !dest_exists;
    if (!should_copy && needs_refresh)
        should_copy = needs_refresh(dst);

    if (!should_copy)
        return false;

    if (!SDL_CopyFile(src, dst))
    {
        log_warn("init_file_paths: unable to seed %s file '%s' from '%s': %s",
            label ? label : "user", dst, src, SDL_GetError());
        return false;
    }
    else
    {
        log_info("init_file_paths: seeded %s file at '%s'", label ? label : leaf, dst);
    }
    return true;
}

static bool has_valid_metarun_data(const char* meta_dir)
{
    if (!meta_dir || !*meta_dir)
        return false;

    char meta_path[1024];
    if (!path_build(meta_path, sizeof(meta_path), meta_dir, META_RAW))
        return false;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(meta_path, &info) || info.type != SDL_PATHTYPE_FILE)
        return false;

    /* File exists and is readable */
    SDL_IOStream* fd = sdl_fopen(meta_path, "rb");
    if (!fd)
        return false;

    /* Check if it has valid header */
    meta_file_header header;
    bool valid = (SDL_ReadIO(fd, &header, sizeof(header)) == sizeof(header))
        && header.entry_count > 0;

    sdl_fclose(fd);
    return valid;
}

static void seed_user_meta_from_install(const char* user_meta_dir, const char* user_metarun_dir)
{
    if (!user_meta_dir || !*user_meta_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    /* Check if user directory already has valid metarun data (from previous migration) */
    bool has_existing_data = has_valid_metarun_data(user_meta_dir);
    if (has_existing_data)
    {
        log_info("init_file_paths: user meta directory already contains valid data, skipping migration");
        return;
    }

    log_debug("init_file_paths: ANGBAND_DIR = '%s'", ANGBAND_DIR);

    /* First, check for legacy location: lib/apex/metaruns/meta.raw (old structure) */
    char install_apex_dir[1024];
    char install_metaruns_dir[1024];
    char legacy_meta_path[1024];
    bool found_legacy = false;

    if (path_build(install_apex_dir, sizeof(install_apex_dir), ANGBAND_DIR, "apex"))
    {
        log_debug("init_file_paths: apex dir = '%s'", install_apex_dir);
        if (path_build(install_metaruns_dir, sizeof(install_metaruns_dir), install_apex_dir, "metaruns"))
        {
            log_debug("init_file_paths: metaruns dir = '%s'", install_metaruns_dir);
                if (path_build(legacy_meta_path, sizeof(legacy_meta_path), install_metaruns_dir, META_RAW))
                {
                log_debug("init_file_paths: checking for legacy meta.raw at '%s'", legacy_meta_path);
                SDL_PathInfo info;
                if (SDL_GetPathInfo(legacy_meta_path, &info) && info.type == SDL_PATHTYPE_FILE)
                {
                    found_legacy = true;
                    log_info("init_file_paths: found legacy meta.raw in lib/apex/metaruns/");
                }
                else
                {
                    log_debug("init_file_paths: legacy meta.raw not found or not a file");
                }
            }
        }
    }

    /* Determine source directory for scores.raw and possibly meta.raw */
    char install_meta_dir[1024];
    if (!path_build(install_meta_dir, sizeof(install_meta_dir), ANGBAND_DIR, "apex"))
        return;

    log_info("init_file_paths: migrating metarun data from install directory");

    /* Copy scores.raw from lib/apex/ if present */
    char score_path[1024];
    if (path_build(score_path, sizeof(score_path), user_meta_dir, "scores.raw"))
    {
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(score_path, &info) || info.type != SDL_PATHTYPE_FILE)
        {
            if (copy_leaf_if_needed(install_meta_dir, user_meta_dir, "scores.raw", "scores", NULL))
            {
                log_info("init_file_paths: migrated scores.raw from lib/apex/");
            }
        }
    }

    /* Copy meta.raw from the appropriate source */
    char user_meta_path[1024];
    if (path_build(user_meta_path, sizeof(user_meta_path), user_meta_dir, META_RAW))
    {
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(user_meta_path, &info) || info.type != SDL_PATHTYPE_FILE)
        {
            if (found_legacy)
            {
                /* Copy from lib/apex/metaruns/meta.raw (old location) */
                log_info("init_file_paths: attempting to copy legacy meta.raw from '%s' to '%s'",
                    legacy_meta_path, user_meta_path);
                if (SDL_CopyFile(legacy_meta_path, user_meta_path))
                {
                    log_info("init_file_paths: migrated meta.raw from lib/apex/metaruns/ (legacy location)");
                }
                else
                {
                    log_warn("init_file_paths: failed to copy legacy meta.raw: %s", SDL_GetError());
                }
            }
            else
            {
                log_debug("init_file_paths: no legacy meta.raw found, trying lib/apex/meta.raw");
                /* Try lib/apex/meta.raw (if it exists) */
                if (copy_leaf_if_needed(install_meta_dir, user_meta_dir, META_RAW, "meta", NULL))
                {
                    log_info("init_file_paths: migrated meta.raw from lib/apex/");
                }
                else
                {
                    log_debug("init_file_paths: no meta.raw found in lib/apex/");
                }
            }
        }
    }
}

static void seed_user_saves_from_install(const char* user_save_dir)
{
    if (!user_save_dir || !*user_save_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    char install_save_dir[1024];
    if (!path_build(install_save_dir, sizeof(install_save_dir), ANGBAND_DIR, "save"))
        return;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(install_save_dir, &info) || info.type != SDL_PATHTYPE_DIRECTORY)
        return;

    /* Check if user directory already has save files */
    int user_entry_count = 0;
    char** user_entries = SDL_GlobDirectory(user_save_dir, NULL, 0, &user_entry_count);
    bool has_existing_saves = false;

    if (user_entries)
    {
        for (int i = 0; user_entries[i]; ++i)
        {
            if (!streq(user_entries[i], ".") && !streq(user_entries[i], ".."))
            {
                has_existing_saves = true;
                break;
            }
        }
        SDL_free(user_entries);
    }

    if (has_existing_saves)
    {
        log_info("init_file_paths: user save directory already contains files, skipping migration");
        return;
    }

    /* Only migrate if user directory is empty */
    log_info("init_file_paths: migrating save files from install directory");

    int entry_count = 0;
    char** entries = SDL_GlobDirectory(install_save_dir, NULL, 0, &entry_count);
    if (!entries)
        return;

    for (int i = 0; entries[i]; ++i)
    {
        if (streq(entries[i], ".") || streq(entries[i], ".."))
            continue;

        /* Skip git files and backup folders */
        if (streq(entries[i], ".gitignore") || strstr(entries[i], "saves_metarun_"))
            continue;

        char src_path[1024];
        char dst_path[1024];
        if (!path_build(src_path, sizeof(src_path), install_save_dir, entries[i]))
            continue;
        if (!path_build(dst_path, sizeof(dst_path), user_save_dir, entries[i]))
            continue;

        if (!SDL_GetPathInfo(src_path, &info) || info.type != SDL_PATHTYPE_FILE)
            continue;
        if (SDL_GetPathInfo(dst_path, NULL))
            continue;

        if (!SDL_CopyFile(src_path, dst_path))
        {
            log_warn("init_file_paths: unable to seed save '%s' -> '%s': %s",
                src_path, dst_path, SDL_GetError());
        }
        else
        {
            log_info("init_file_paths: copied legacy save '%s' -> '%s'", src_path, dst_path);
        }
    }

    SDL_free(entries);
}

static void seed_sound_config(const char* user_dir)
{
    if (!user_dir || !*user_dir || !ANGBAND_DIR || !*ANGBAND_DIR)
        return;

    char user_sound_path[1024];
    if (!path_build(user_sound_path, sizeof(user_sound_path), user_dir, "sound.json"))
        return;

    /* Check if sound.json already exists in user folder */
    if (SDL_GetPathInfo(user_sound_path, NULL))
    {
        log_debug("init_file_paths: sound.json already exists in user folder");
        return;
    }

    /* Copy sound.json from lib/pref to user folder */
    char pref_sound_path[1024];
    
    /* ANGBAND_DIR_PREF already points to lib/pref */
    if (!ANGBAND_DIR_PREF || !*ANGBAND_DIR_PREF)
    {
        log_warn("init_file_paths: ANGBAND_DIR_PREF not set, cannot seed sound.json");
        return;
    }
    
    /* Build path to lib/pref/sound.json */
    if (!path_build(pref_sound_path, sizeof(pref_sound_path), ANGBAND_DIR_PREF, "sound.json"))
        return;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(pref_sound_path, &info) || info.type != SDL_PATHTYPE_FILE)
    {
        log_warn("init_file_paths: sound.json not found at '%s'", pref_sound_path);
        return;
    }

    if (SDL_CopyFile(pref_sound_path, user_sound_path))
    {
        log_info("init_file_paths: copied sound.json from lib/pref to user folder");
    }
    else
    {
        log_warn("init_file_paths: failed to copy sound.json: %s", SDL_GetError());
    }
}

static void migrate_legacy_metarun_layout(const char* meta_root, const char* metarun_dir)
{
    if (!meta_root || !*meta_root || !metarun_dir || !*metarun_dir)
        return;

    char legacy[1024];
    if (!path_build(legacy, sizeof(legacy), metarun_dir, META_RAW))
        return;

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(legacy, &info) || info.type != SDL_PATHTYPE_FILE)
    {
        /* No legacy file to migrate - check if the metaruns/ folder is empty and remove it */
        int entry_count = 0;
        char** entries = SDL_GlobDirectory(metarun_dir, NULL, 0, &entry_count);
        bool is_empty = true;

        if (entries)
        {
            for (int i = 0; entries[i]; ++i)
            {
                if (!streq(entries[i], ".") && !streq(entries[i], ".."))
                {
                    is_empty = false;
                    break;
                }
            }
            SDL_free(entries);
        }

        if (is_empty)
        {
            if (SDL_RemovePath(metarun_dir))
            {
                log_info("init_file_paths: removed empty legacy metaruns directory '%s'", metarun_dir);
            }
        }
        return;
    }

    char target[1024];
    if (!path_build(target, sizeof(target), meta_root, META_RAW))
        return;

    SDL_PathInfo target_info;
    bool target_exists = SDL_GetPathInfo(target, &target_info)
        && target_info.type == SDL_PATHTYPE_FILE;

    if (target_exists)
    {
        if (!SDL_RemovePath(legacy))
        {
            log_warn("init_file_paths: failed to remove legacy metarun file '%s': %s",
                legacy, SDL_GetError());
        }
        else
        {
            log_info("init_file_paths: removed duplicate legacy metarun file '%s'", legacy);

            /* Try to remove the now-empty metaruns directory */
            if (SDL_RemovePath(metarun_dir))
            {
                log_info("init_file_paths: removed empty legacy metaruns directory '%s'", metarun_dir);
            }
        }
        return;
    }

    if (!SDL_RenamePath(legacy, target))
    {
        log_warn("init_file_paths: failed to migrate legacy metarun file '%s' -> '%s': %s",
            legacy, target, SDL_GetError());
    }
    else
    {
        log_info("init_file_paths: migrated legacy metarun file to '%s'", target);

        /* Try to remove the now-empty metaruns directory */
        if (SDL_RemovePath(metarun_dir))
        {
            log_info("init_file_paths: removed empty legacy metaruns directory '%s'", metarun_dir);
        }
    }
}

/*
 * This file is used to initialize various variables and arrays for the
 * Sil game.  Note the use of "sdl_read()" and "sdl_write()" to bypass
 * the common limitation of "read()" and "write()" to only 32767 bytes
 * at a time.
 *
 * Several of the arrays for Sil are built from "template" files in
 * the "lib/file" directory, from which quick-load binary "image" files
 * are constructed whenever they are not present in the "lib/data"
 * directory, or if those files become obsolete, if we are allowed.
 *
 * Warning -- the "ascii" file parsers use a minor hack to collect the
 * name and text information in a single pass.  Thus, the game will not
 * be able to load any template file with more than 20K of names or 60K
 * of text, even though technically, up to 64K should be legal.
 *
 * The "init1.c" file is used only to parse the ascii template files,
 * to create the binary image files.  If you include the binary image
 * files instead of the ascii template files, then you can undefine
 * "ALLOW_TEMPLATES", saving about 20K by removing "init1.c".  Note
 * that the binary image files are extremely system dependant.
 */

/*
 * Find the default paths to all of our important sub-directories.
 *
 * The purpose of each sub-directory is described in "variable.c".
 *
 * All of the sub-directories should, by default, be located inside
 * the main "lib" directory, whose location is very system dependant.
 *
 * This function takes a writable buffer, initially containing the
 * "path" to the "lib" directory, for example, "/pkg/lib/sil/",
 * or a system dependant string, for example, ":lib:".  The buffer
 * must be large enough to contain at least 32 more characters.
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "info" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * Mega-Hack -- support fat raw files under NEXTSTEP, using special
 * "suffixed" directories for the "ANGBAND_DIR_DATA" directory, but
 * requiring the directories to be created by hand by the user.
 *
 * Hack -- first we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(char* path)
{
    char* tail;
    char buf[1024];

    /*** Free everything ***/

    /* Free the main path */
    str_free(ANGBAND_DIR);

    /* Free the sub-paths */
    str_free(ANGBAND_DIR_APEX);
    str_free(ANGBAND_DIR_METARUN);
    str_free(ANGBAND_DIR_BONE);
    str_free(ANGBAND_DIR_DATA);
    str_free(ANGBAND_DIR_EDIT);
    str_free(ANGBAND_DIR_FILE);
    str_free(ANGBAND_DIR_HELP);
    str_free(ANGBAND_DIR_INFO);
    str_free(ANGBAND_DIR_SAVE);
    str_free(ANGBAND_DIR_PREF);
    str_free(ANGBAND_DIR_USER);
    str_free(ANGBAND_DIR_XTRA);
    str_free(ANGBAND_DIR_SCRIPT);

    /*** Prepare the "path" ***/

    /* Hack -- save the main directory */
    ANGBAND_DIR = str_dup(path);

    /* Prepare to append to the Base Path */
    tail = path + strlen(path);

#ifdef VM

    /*** Use "flat" paths with VM/ESA ***/

    /* Use "blank" path names */
    ANGBAND_DIR_APEX = str_dup("");
    ANGBAND_DIR_BONE = str_dup("");
    ANGBAND_DIR_DATA = str_dup("");
    ANGBAND_DIR_EDIT = str_dup("");
    ANGBAND_DIR_FILE = str_dup("");
    ANGBAND_DIR_HELP = str_dup("");
    ANGBAND_DIR_INFO = str_dup("");
    ANGBAND_DIR_SAVE = str_dup("");
    ANGBAND_DIR_PREF = str_dup("");
    ANGBAND_DIR_USER = str_dup("");
    ANGBAND_DIR_XTRA = str_dup("");
    ANGBAND_DIR_SCRIPT = str_dup("");

#else /* VM */

    /*** Build the sub-directory names ***/

    strcpy(tail, "edit");
    ANGBAND_DIR_EDIT = str_dup(path);
    strcpy(tail, "pref");
    ANGBAND_DIR_PREF = str_dup(path);

    strcpy(tail, "pref");
    ANGBAND_DIR_PREF = str_dup(path);

#ifdef SIL_USE_LOCAL_DATA
    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "user"))
        ANGBAND_DIR_USER = str_dup(buf);
    else
        ANGBAND_DIR_USER = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "data"))
        ANGBAND_DIR_DATA = str_dup(buf);
    else
        ANGBAND_DIR_DATA = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "save"))
        ANGBAND_DIR_SAVE = str_dup(buf);
    else
        ANGBAND_DIR_SAVE = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "apex"))
        ANGBAND_DIR_APEX = str_dup(buf);
    else
        ANGBAND_DIR_APEX = str_dup(ANGBAND_DIR);

    if (path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, SIL_USER_META_RUNS))
        ANGBAND_DIR_METARUN = str_dup(buf);
    else
        ANGBAND_DIR_METARUN = str_dup(ANGBAND_DIR_APEX);
#else
    char user_root[1024];
    if (!build_user_root_path(user_root, sizeof(user_root)))
    {
        SDL_strlcpy(user_root, SIL_USER_ROOT, sizeof(user_root));
        log_warn("init_file_paths: defaulting user path to '%s' relative to the working directory",
            user_root);
    }

    ensure_directory_exists(user_root, "user root");
    ANGBAND_DIR_USER = str_dup(user_root);

    if (path_build(buf, sizeof(buf), user_root, SIL_USER_DATA_DIR))
    {
        ensure_directory_exists(buf, "data");
        ANGBAND_DIR_DATA = str_dup(buf);
    }
    else
    {
        ANGBAND_DIR_DATA = str_dup(user_root);
    }

    if (path_build(buf, sizeof(buf), user_root, SIL_USER_SAVE_DIR))
    {
        ensure_directory_exists(buf, "save");
        ANGBAND_DIR_SAVE = str_dup(buf);
    }
    else
    {
        ANGBAND_DIR_SAVE = str_dup(user_root);
    }

    /* Set ANGBAND_DIR_APEX to the actual apex directory in game folder */
    if (path_build(buf, sizeof(buf), ANGBAND_DIR, "apex"))
        ANGBAND_DIR_APEX = str_dup(buf);
    else
        ANGBAND_DIR_APEX = str_dup(ANGBAND_DIR);

    /* Set up meta directory for scores.raw and metarun data */
    char meta_root[1024];
    if (path_build(meta_root, sizeof(meta_root), user_root, SIL_USER_META_DIR))
    {
        ensure_directory_exists(meta_root, "meta");

        char metarun_dir[1024];
        if (path_build(metarun_dir, sizeof(metarun_dir), meta_root, SIL_USER_META_RUNS))
        {
            ensure_directory_exists(metarun_dir, "metarun");
            ANGBAND_DIR_METARUN = str_dup(metarun_dir);
        }
        else
        {
            ANGBAND_DIR_METARUN = str_dup(meta_root);
        }
    }
    else
    {
        ANGBAND_DIR_METARUN = str_dup(user_root);
    }

    migrate_legacy_metarun_layout(meta_root, ANGBAND_DIR_METARUN);
    seed_user_data_from_install(ANGBAND_DIR_DATA);
    seed_user_meta_from_install(meta_root, ANGBAND_DIR_METARUN);
    seed_user_saves_from_install(ANGBAND_DIR_SAVE);
    seed_sound_config(user_root);
#endif /* SIL_USE_LOCAL_DATA */

    strcpy(tail, "xtra");
    ANGBAND_DIR_XTRA = str_dup(path);

    strcpy(tail, "script");
    ANGBAND_DIR_SCRIPT = str_dup(path);

#endif /* VM */

#ifdef NeXT

    /* Allow "fat binary" usage with NeXT */
    if (true)
    {
        cptr next = NULL;

#if defined(m68k)
        next = "m68k";
#endif

#if defined(i386)
        next = "i386";
#endif

#if defined(sparc)
        next = "sparc";
#endif

#if defined(hppa)
        next = "hppa";
#endif

        /* Use special directory */
        if (next)
        {
            /* Forget the old path name */
            str_free(ANGBAND_DIR_DATA);

            /* Build a new path name */
            sprintf(tail, "data-%s", next);
            ANGBAND_DIR_DATA = str_dup(path);
        }
    }

#endif /* NeXT */
}

#ifdef ALLOW_TEMPLATES

/*
 * Hack -- help give useful error messages
 */
int error_idx;
int error_line;

/*
 * Standard error message text
 */
static cptr err_str[PARSE_ERROR_MAX] = {
    NULL,
    "parse error",
    "obsolete file",
    "missing record header",
    "non-sequential records",
    "invalid flag specification",
    "undefined directive",
    "out of memory",
    "value out of bounds",
    "too few arguments",
    "too many arguments",
    "too many allocation entries",
    "invalid spell frequency",
    "invalid number of items (0-99)",
    "too many entries",
    "vault too big",
    "vault not rectangular (check spaces at end of line?)",
    NULL,
};

#endif /* ALLOW_TEMPLATES */

/*
 * File headers
 */
header z_head;
header v_head;
header f_head;
header k_head;
header b_head;
header a_head;
header e_head;
header r_head;
header p_head;
header c_head;
header h_head;
header st_head;
header cu_head;
header mb_head;
header b_head;
header g_head;
header flavor_head;
header quest_head;
header oath_head;
header n_head;
header style_head;
header skeleton_note_head;

/*** Initialize from binary image files ***/

/*
 * Initialize a "*_info" array, by parsing a binary "image" file
 */
static errr init_info_raw(SDL_IOStream* fd, header* head)
{
    header test;

    /* Read and verify the header */
    if (sdl_read(fd, (char*)(&test), sizeof(header))
        || (test.v_major != head->v_major) || (test.v_minor != head->v_minor)
        || (test.v_patch != head->v_patch) || (test.v_extra != head->v_extra)
        || (test.info_num != head->info_num)
        || (test.info_len != head->info_len)
        || (test.head_size != head->head_size)
        || (test.info_size != head->info_size))
    {
        /* Error */
        return (-1);
    }

    /* Accept the header */
    memcpy(head, &test, sizeof(header));

    /* Allocate the "*_info" array */
    head->info_ptr = mem_alloc_array(head->info_size, char);

    /* Read the "*_info" array */
    sdl_read(fd, head->info_ptr, head->info_size);

    if (head->name_size)
    {
        /* Allocate the "*_name" array */
        head->name_ptr = mem_alloc_array(head->name_size, char);

        /* Read the "*_name" array */
        sdl_read(fd, head->name_ptr, head->name_size);
    }

    if (head->text_size)
    {
        /* Allocate the "*_text" array */
        head->text_ptr = mem_alloc_array(head->text_size, char);

        /* Read the "*_text" array */
        sdl_read(fd, head->text_ptr, head->text_size);
    }

    /* Success */
    return (0);
}

/* local forward */
static errr init_rt_info(void);
static errr init_style_info(void);
static errr init_skeleton_note_info(void);
/* From init1.c */

/*
 * Initialize the header of an *_info.raw file.
 */
static void init_header(header* head, int num, int len)
{
    /* Save the "version" */
    head->v_major = VERSION_MAJOR;
    head->v_minor = VERSION_MINOR;
    head->v_patch = VERSION_PATCH;
    head->v_extra = VERSION_EXTRA;

    /* Save the "record" information */
    head->info_num = num;
    head->info_len = len;

    /* Save the size of "*_head" and "*_info" */
    head->head_size = sizeof(header);
    head->info_size = head->info_num * head->info_len;
}

#ifdef ALLOW_TEMPLATES

/*
 * Display a parser error message.
 */
static void display_parse_error(cptr filename, errr err, cptr buf)
{
    cptr oops;

    /* Error string */
    oops = (((err > 0) && (err < PARSE_ERROR_MAX)) ? err_str[err] : "unknown");

    /* Oops */
    msg_format("Error at line %d of '%s.txt'.", error_line, filename);
    msg_format("Record %d contains a '%s' error.", error_idx, oops);
    msg_format("Parsing '%s'.", buf);
    
    /* Explicitly log the error to log.txt as requested (one line) */
    log_error("CRITICAL PARSE ERROR: %s in %s.txt at line %d (record %d). Entry: '%s'", oops, filename, error_line, error_idx, buf);
    
    message_flush();

    /* Quit */
    quit(format("Error in '%s.txt' file.", filename));
}

#endif /* ALLOW_TEMPLATES */

/*
 * Initialize a "*_info" array
 *
 * Note that we let each entry have a unique "name" and "text" string,
 * even if the string happens to be empty (everyone has a unique '\0').
 */
static errr init_info(cptr filename, header* head)
{
    SDL_IOStream* fd;

    errr err = 1;

    SDL_IOStream* fp;

    /* General buffer */
    char buf[1024];

#ifdef ALLOW_TEMPLATES

    /*** Load the binary image file ***/

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

    /* Attempt to open the "raw" file */
    fd = sdl_fopen(buf, "rb");

    /* Process existing "raw" file */
    if (fd)
    {
#ifdef CHECK_MODIFICATION_TIME
        /* Check if text file is newer than raw file */
        char txt_path[1024];
        path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, format("%s.txt", filename));
        log_debug("Checking modification times: raw='%s' vs txt='%s'", buf, txt_path);
        err = check_modification_date_sdl(buf, txt_path);
        if (err)
        {
            /* Text file is newer - close raw and regenerate */
            log_info("Text file '%s.txt' is newer than raw file - regenerating", filename);
            sdl_fclose(fd);
            fd = NULL;
        }
        else
        {
            log_debug("Raw file '%s.raw' is up to date", filename);
        }
#endif /* CHECK_MODIFICATION_TIME */

        /* Attempt to parse the "raw" file */
        if (fd && !err)
            err = init_info_raw(fd, head);

        /* Close it */
        sdl_fclose(fd);
    }

    /* Do we have to parse the *.txt file? */
    if (err)
    {
        /*** Make the fake arrays ***/

        /* Allocate the "*_info" array */
        head->info_ptr = mem_alloc_array(head->info_size, char);

        /* MegaHack -- make "fake" arrays */
        if (z_info)
        {
            head->name_ptr = mem_alloc_array(z_info->fake_name_size, char);
            head->text_ptr = mem_alloc_array(z_info->fake_text_size, char);
        }

        /*** Load the ascii template file ***/

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_EDIT, format("%s.txt", filename));

        /* Open the file */
        fp = sdl_fopen(buf, "r");

        /* Parse it */
        if (!fp)
            quit(format("Cannot open '%s.txt' file.", filename));

        /* Parse the file */
        err = init_info_txt(fp, buf, head, head->parse_info_txt);

        /* Close it */
        sdl_fclose(fp);

        /* Errors */
        if (err)
            display_parse_error(filename, err, buf);

        /*** Dump the binary image file ***/

        /* File type is "DATA" */
        FILE_TYPE(FILE_TYPE_DATA);

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the file */
        fd = sdl_fopen(buf, "rb");

        /* Failure */
        if (!fd)
        {
            int mode = 0644;

            /* Grab permissions */
            safe_setuid_grab();

            /* Create a new file */
            fd = sdl_fmake(buf, mode);

            /* Drop permissions */
            safe_setuid_drop();

            /* Failure */
            if (!fd)
            {
                /* Complain */
                plog(format("Cannot create the '%s' file!", buf));

                /* Continue */
                return (0);
            }
        }

        /* Close it */
        sdl_fclose(fd);

        /* Grab permissions */
        safe_setuid_grab();

        /* Attempt to create the raw file */
        fd = sdl_fopen(buf, "wb");

        /* Drop permissions */
        safe_setuid_drop();

        /* Failure */
        if (!fd)
        {
            /* Complain */
            plog(format("Cannot write the '%s' file!", buf));

            /* Continue */
            return (0);
        }

        /* Dump to the file */
        if (fd)
        {
            /* Dump it */
            sdl_write(fd, (cptr)head, head->head_size);

            /* Dump the "*_info" array */
            sdl_write(fd, head->info_ptr, head->info_size);

            /* Dump the "*_name" array */
            sdl_write(fd, head->name_ptr, head->name_size);

            /* Dump the "*_text" array */
            sdl_write(fd, head->text_ptr, head->text_size);

            /* Close */
            sdl_fclose(fd);
        }

        /*** Kill the fake arrays ***/

        /* Free the "*_info" array */
        mem_free_null(head->info_ptr);

        /* MegaHack -- Free the "fake" arrays */
        if (z_info)
        {
            mem_free_null(head->name_ptr);
            mem_free_null(head->text_ptr);
        }

#endif /* ALLOW_TEMPLATES */

        /*** Load the binary image file ***/

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the "raw" file */
        fd = sdl_fopen(buf, "rb");

        /* Process existing "raw" file */
        if (fd < 0)
            quit(format("Cannot load '%s.raw' file.", filename));

        /* Attempt to parse the "raw" file */
        err = init_info_raw(fd, head);

        /* Close it */
        sdl_fclose(fd);

        /* Error */
        if (err)
            quit(format("Cannot parse '%s.raw' file.", filename));

#ifdef ALLOW_TEMPLATES
    }
#endif /* ALLOW_TEMPLATES */

    /* Success */
    return (0);
}

/*
 * Free the allocated memory for the info-, name-, and text- arrays.
 */
static errr free_info(header* head)
{
    if (head->info_size)
        mem_free_null(head->info_ptr);

    if (head->name_size)
        mem_free_null(head->name_ptr);

    if (head->text_size)
        mem_free_null(head->text_ptr);

    /* Success */
    return (0);
}

/*
 * Initialize the "z_info" array
 */
static errr init_z_info(void)
{
    errr err;

    /* Init the header */
    init_header(&z_head, 1, sizeof(maxima));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    z_head.parse_info_txt = parse_z_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("limits", &z_head);

    /* Set the global variables */
    z_info = z_head.info_ptr;

    return (err);
}

/*
 * Initialize the "f_info" array
 */
static errr init_f_info(void)
{
    errr err;

    /* Init the header */
    init_header(&f_head, z_info->f_max, sizeof(feature_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    f_head.parse_info_txt = parse_f_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("terrain", &f_head);

    /* Set the global variables */
    f_info = f_head.info_ptr;
    f_name = f_head.name_ptr;
    f_text = f_head.text_ptr;

    return (err);
}

/*
 * Initialize the "style_info" array
 */
static errr init_style_info(void)
{
    errr err;
    /* Default to zero if not specified yet; will be set by limits.txt */
    init_header(&style_head, z_info->style_max, sizeof(style_type));
    style_head.parse_info_txt = parse_style_info;
    err = init_info("style", &style_head);
    if (err) return err;
    /* Ensure M: banner strings are loaded even if RAW cache was used. */
    styles_reload_messages_from_text();
    /* Load level/vault rules from separate file (always parse text for side-effects).
     * We bypass the RAW cache here so manual edits to style-levels.txt take effect
     * even when ALLOW_TEMPLATES is not defined. */
    {
        SDL_IOStream* fp;
        char buf[1024];
        header levels_head;
        init_header(&levels_head, 1, 1);
        /* Build full path to lib/edit/style-levels.txt */
        path_build(buf, sizeof(buf), ANGBAND_DIR_EDIT, format("%s.txt", "style-levels"));
        fp = sdl_fopen(buf, "r");
        if (!fp) quit("Cannot open 'style-levels.txt' file.");
        /* Parse the file using the style-levels parser (populates global rule tables) */
        {
            char linebuf[1024];
            err = init_info_txt(fp, linebuf, &levels_head, parse_style_levels);
        }
        sdl_fclose(fp);
        if (err)
        {
            /* Report a parse error with helpful context */
            display_parse_error("style-levels", err, "style-levels");
            return err;
        }
    }

    /* No separate pass for D: depth banners; per requirements, banners come from per-style M: only. */
    return 0;
}

/*
 * Initialize the "k_info" array
 */
static errr init_k_info(void)
{
    errr err;

    /* Init the header */
    init_header(&k_head, z_info->k_max, sizeof(object_kind));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    k_head.parse_info_txt = parse_k_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("object", &k_head);

    /* Set the global variables */
    k_info = k_head.info_ptr;
    k_name = k_head.name_ptr;
    k_text = k_head.text_ptr;

    return (err);
}

/*
 * Initialize the "b_info" array
 */
static errr init_b_info(void)
{
    errr err;

    /* Init the header */
    init_header(&b_head, z_info->b_max, sizeof(ability_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    b_head.parse_info_txt = parse_b_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("ability", &b_head);

    /* Set the global variables */
    b_info = b_head.info_ptr;
    b_name = b_head.name_ptr;
    b_text = b_head.text_ptr;

    return (err);
}

/*
 * Initialize the "a_info" array
 */
static errr init_a_info(void)
{
    errr err;

    /* Init the header */
    init_header(&a_head, z_info->art_max, sizeof(artefact_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    a_head.parse_info_txt = parse_a_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("artefact", &a_head);

    /* Set the global variables */
    a_info = a_head.info_ptr;

    a_text = a_head.text_ptr;

    return (err);
}

static void ensure_artifact_guids(void)
{
    if (!a_info || !z_info)
        return;

    for (int i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        if (!a_ptr)
            continue;

        if (!score_guid_is_zero(&a_ptr->guid))
            continue;

        const char* name = a_ptr->name[0] ? a_ptr->name : "unknown-artifact";
        a_ptr->guid = score_guid_from_string(name, (u32b)i);
    }
}

/*
 * Initialize the "e_info" array
 */
static errr init_e_info(void)
{
    errr err;

    /* Init the header */
    init_header(&e_head, z_info->e_max, sizeof(ego_item_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    e_head.parse_info_txt = parse_e_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("special", &e_head);

    /* Set the global variables */
    e_info = e_head.info_ptr;
    e_name = e_head.name_ptr;
    e_text = e_head.text_ptr;

    return (err);
}

/*
 * Initialize the "r_info" array
 */
static errr init_r_info(void)
{
    errr err;

    /* Init the header */
    init_header(&r_head, z_info->r_max, sizeof(monster_race));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    r_head.parse_info_txt = parse_r_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("monster", &r_head);

    /* Set the global variables */
    r_info = r_head.info_ptr;
    r_name = r_head.name_ptr;
    r_text = r_head.text_ptr;

    return (err);
}

/*
 * Initialize the "v_info" array
 */
static errr init_v_info(void)
{
    errr err;

    /* Init the header */
    init_header(&v_head, z_info->v_max, sizeof(vault_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    v_head.parse_info_txt = parse_v_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("vault", &v_head);

    /* Set the global variables */
    v_info = v_head.info_ptr;
    v_name = v_head.name_ptr;
    v_text = v_head.text_ptr;

    return (err);
}

static errr init_rt_info(void)
{
    errr err;
    init_header(&rt_head, z_info->rt_max, sizeof(runtype_type));     /* ① */
#ifdef ALLOW_TEMPLATES
    rt_head.parse_info_txt = parse_rt_info;                          /* ② */
#endif
    err = init_info("runtypes", &rt_head);                           /* ③ */

    runtype_info = rt_head.info_ptr;                                 /* ④ global */
    return err;
}


/*
 * Initialize the "p_info" array
 */
static errr init_p_info(void)
{
    errr err;

    /* Init the header */
    init_header(&p_head, z_info->p_max, sizeof(player_race));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    p_head.parse_info_txt = parse_p_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("race", &p_head);

    /* Set the global variables */
    p_info = p_head.info_ptr;
    p_name = p_head.name_ptr;
    p_text = p_head.text_ptr;

    return (err);
}

/*
 * Initialize the "c_info" array
 */
static errr init_c_info(void)
{
    errr err;

    /* Init the header */
    init_header(&c_head, z_info->c_max, sizeof(character_profile));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    c_head.parse_info_txt = parse_c_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("character", &c_head);

    /* Set the global variables */
    c_info = c_head.info_ptr;
    c_name = c_head.name_ptr;
    c_text = c_head.text_ptr;

    return (err);
}

/*
 * Initialize the "h_info" array
 */
static errr init_h_info(void)
{
    errr err;

    /* Init the header */
    init_header(&h_head, z_info->h_max, sizeof(hist_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    h_head.parse_info_txt = parse_h_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("history", &h_head);

    /* Set the global variables */
    h_info = h_head.info_ptr;
    h_text = h_head.text_ptr;

    return (err);
}

/*
 * Initialize the "st_info" array
 */
static errr init_st_info(void)
{
    errr err;

    /* Init the header */
    init_header(&st_head, z_info->st_max, sizeof(story_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    st_head.parse_info_txt = parse_st_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("story", &st_head);

    /* Set the global variables */
    st_info = st_head.info_ptr;
    st_text = st_head.text_ptr;
    st_name = st_head.name_ptr;

    return (err);
}

/*
 * Initialize the "cu_info" array
 */
static errr init_cu_info(void)
{
    errr err;

    /* Init the header */
    init_header(&cu_head, z_info->cu_max, sizeof(curse_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    cu_head.parse_info_txt = parse_cu_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("curses", &cu_head);

    /* Set the global variables */
    cu_info = cu_head.info_ptr;
    cu_text = cu_head.text_ptr;
    cu_name = cu_head.name_ptr;

    return (err);
}

/*
 * Initialize the blessing definitions array
 */
static errr init_mb_info(void)
{
    errr err;

    init_header(&mb_head, z_info->mb_max, sizeof(major_blessing_type));

#ifdef ALLOW_TEMPLATES
    mb_head.parse_info_txt = parse_mb_info;
#endif /* ALLOW_TEMPLATES */

    err = init_info("blessing", &mb_head);

    mb_info = mb_head.info_ptr;
    mb_text = mb_head.text_ptr;
    mb_name = mb_head.name_ptr;

    return err;
}

/*
 * Initialize the "n_info" structure
 */
static errr init_n_info(void)
{
    errr err;

    /* Init the header */
    init_header(&n_head, 1, sizeof(names_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    n_head.parse_info_txt = parse_n_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("names", &n_head);

    n_info = n_head.info_ptr;

    return (err);
}

/*
 * Initialize the "flavor_info" array
 */
static errr init_flavor_info(void)
{
    errr err;

    /* Init the header */
    init_header(&flavor_head, z_info->flavor_max, sizeof(flavor_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    flavor_head.parse_info_txt = parse_flavor_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("flavor", &flavor_head);

    /* Set the global variables */
    flavor_info = flavor_head.info_ptr;
    flavor_name = flavor_head.name_ptr;
    flavor_text = flavor_head.text_ptr;

    return (err);
}

/*
 * Initialize skeleton note templates
 */
static errr init_skeleton_note_info(void)
{
    errr err;

    if (z_info && z_info->skeleton_note_max <= 0)
    {
        log_warn("skeleton_note_max not set in limits.txt (or 0), defaulting to 160");
        z_info->skeleton_note_max = 160;
    }
    else
    {
        log_debug("skeleton_note_max initialized to %d", z_info->skeleton_note_max);
    }

    init_header(
        &skeleton_note_head, z_info->skeleton_note_max,
        sizeof(skeleton_note_template));

#ifdef ALLOW_TEMPLATES
    skeleton_note_head.parse_info_txt = parse_skeleton_note_info;
#endif /* ALLOW_TEMPLATES */

    err = init_info("skeleton_note", &skeleton_note_head);

    skeleton_note_info = (skeleton_note_template*)skeleton_note_head.info_ptr;
    skeleton_note_text = skeleton_note_head.text_ptr;

    return (err);
}

/*
 * Initialize the "quest_info" array
 */
static errr init_quest_info(void)
{
    errr err;

    /* Init the header */
    init_header(&quest_head, z_info->quest_max, sizeof(quest_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    quest_head.parse_info_txt = parse_quest_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("quest", &quest_head);

    /* Set the global variables */
    quest_info = quest_head.info_ptr;
    quest_name_text = quest_head.name_ptr;
    quest_desc_text = quest_head.text_ptr;
    q_text = quest_head.text_ptr;

    return (err);
}

/*
 * Initialize the "oath_info" array
 */
static errr init_oath_info(void)
{
    errr err;

    /* Init the header */
    init_header(&oath_head, z_info->oath_max, sizeof(oath_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    oath_head.parse_info_txt = parse_oath_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("oath", &oath_head);

    /* Set the global variables */
    oath_info = oath_head.info_ptr;
    oath_name_text = oath_head.name_ptr;
    oath_desc_text = oath_head.text_ptr;

    return (err);
}

extern void autoinscribe_clean(void)
{
    if (inscriptions)
    {
        mem_free_null(inscriptions);
    }

    inscriptions = 0;
    inscriptionsCount = 0;
}

extern void autoinscribe_init(void)
{
    /* Paranoia */
    autoinscribe_clean();

    inscriptions = mem_alloc_array(AUTOINSCRIPTIONS_MAX, autoinscription);
}

/*
 * Reinitialize some things between games
 *
 * Needed because rerunning the whole of init_angband() causes crashes.
 */
extern void re_init_some_things(void)
{
    int i;

    // wipe the whole player structure
    memset(p_ptr, 0, sizeof(player_type));

    // clear some additional things
    savefile[0] = '\0';
    playerturn = 0;
    min_depth_counter = 0;
    op_ptr->full_name[0] = '\0';

    // clear the terms
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term* old = Term;

        /* Dead window */
        if (!angband_term[i])
            continue;

        /* Activate */
        Term_activate(angband_term[i]);

        /* Erase */
        Term_clear();

        /* Refresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }

    // Reset the autoinscriptions
    autoinscribe_clean();
    autoinscribe_init();

    // display the introduction message again
    sdl_story_font_enable();
    display_introduction();
    sdl_story_font_reset();

    /* Array of grids */
    mem_free_null(view_g);
    view_g = mem_alloc_array(VIEW_MAX, u16b);

    /* Array of grids */
    mem_free_null(temp_g);
    temp_g = mem_alloc_array(TEMP_MAX, u16b);

    /* has_lite patch causes both temp_g and temp_x/y to be used
    in targetting mode: can't use the same memory any more. */
    mem_free_null(temp_y);
    mem_free_null(temp_x);
    temp_y = mem_alloc_array(TEMP_MAX, byte);
    temp_x = mem_alloc_array(TEMP_MAX, byte);

    /*** Prepare dungeon arrays ***/

    /* Padded into array */
    mem_free_null(cave_info);
    cave_info = mem_alloc_array(MAX_DUNGEON_HGT, u16b_256);

    /* Feature array */
    mem_free_null(cave_feat);
    cave_feat = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Color array */
    mem_free_null(cave_color);
    cave_color = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Light array */
    mem_free_null(cave_light);
    cave_light = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Entity arrays */
    mem_free_null(cave_o_idx);
    mem_free_null(cave_m_idx);
    cave_o_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_m_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Flow arrays */
    mem_free_null(cave_when);
    cave_when = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /*** Prepare "vinfo" array ***/

    /* Used by "update_view()" */
    (void)vinfo_init();

    /*** Prepare entity arrays ***/

    /* Objects */
    mem_free_null(o_list);
    o_list = mem_alloc_array(z_info->o_max, object_type);

    /* Monsters */
    mem_free_null(mon_list);
    mon_list = mem_alloc_array(MAX_MONSTERS, monster_type);

    /*** Prepare lore array ***/

    /* Lore */
    mem_free_null(l_list);
    l_list = mem_alloc_array(z_info->r_max, monster_lore);

    /*** Prepare the inventory ***/

    /* Allocate it */
    mem_free_null(inventory);
    inventory = mem_alloc_array(INVEN_TOTAL, object_type);

    /*** Prepare the options ***/

    /* Initialize the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        /* Default value */
        op_ptr->opt[i] = option_norm[i];
    }

    /* Initialize the window flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Assume no flags */
        op_ptr->window_flag[i] = 0L;
    }

    // Set some sensible defaults
    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);

    // re-initialize the objects and flavors
    if (init_k_info())
        quit("Cannot initialize objects");
    if (init_flavor_info())
        quit("Cannot initialize flavors");
    if (init_skeleton_note_info())
        quit("Cannot initialize skeleton notes");
    if (init_e_info())
        quit("Cannot initialize special items");
}

/*
 * Initialize some other arrays
 */
static errr init_other(void)
{
    int i;

    /*** Prepare the various "bizarre" arrays ***/

    /* Initialize the "macro" package */
    (void)macro_init();

    /* Initialize the "quark" package */
    (void)quarks_init();

    /* Initialize autoinscriptions */
    (void)autoinscribe_init();

    /* Initialize the "message" package */
    (void)messages_init();

    /*** Prepare grid arrays ***/

    /* Array of grids */
    view_g = mem_alloc_array(VIEW_MAX, u16b);

    /* Array of grids */
    temp_g = mem_alloc_array(TEMP_MAX, u16b);

    /* has_lite patch causes both temp_g and temp_x/y to be used
    in targetting mode: can't use the same memory any more. */
    temp_y = mem_alloc_array(TEMP_MAX, byte);
    temp_x = mem_alloc_array(TEMP_MAX, byte);

    /*** Prepare dungeon arrays ***/

    /* Padded into array */
    cave_info = mem_alloc_array(MAX_DUNGEON_HGT, u16b_256);

    /* Feature array */
    cave_feat = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Color array */
    cave_color = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Light array */
    cave_light = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Entity arrays */
    cave_o_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_m_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Flow arrays */
    cave_when = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /*** Prepare "vinfo" array ***/

    /* Used by "update_view()" */
    (void)vinfo_init();

    /*** Prepare entity arrays ***/

    /* Objects */
    o_list = mem_alloc_array(z_info->o_max, object_type);

    /* Monsters */
    mon_list = mem_alloc_array(MAX_MONSTERS, monster_type);

    /*** Prepare lore array ***/

    /* Lore */
    l_list = mem_alloc_array(z_info->r_max, monster_lore);

    /*** Prepare the inventory ***/

    /* Allocate it */
    inventory = mem_alloc_array(INVEN_TOTAL, object_type);

    /*** Prepare the options ***/

    /* Initialize the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        /* Default value */
        op_ptr->opt[i] = option_norm[i];
    }

    /* Initialize the window flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Assume no flags */
        op_ptr->window_flag[i] = 0L;
    }

    // Set some sensible defaults
    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);

    /*** Pre-allocate space for the "format()" buffer ***/

    /* Hack -- Just call the "format()" function */
    (void)format("%s", MAINTAINER);

    /* Success */
    return (0);
}

/*
 * Initialize some other arrays
 */
static errr init_alloc(void)
{
    int i, j;

    object_kind* k_ptr;

    monster_race* r_ptr;

    ego_item_type* e_ptr;

    alloc_entry* table;

    s16b num[MAX_DEPTH];

    s16b aux[MAX_DEPTH];

    /*** Analyze object allocation info ***/

    /* Clear the "aux" array */
    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);

    /* Clear the "num" array */
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    /* Size of "alloc_kind_table" */
    alloc_kind_size = 0;

    /* Scan the objects */
    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /* Count the "legal" entries */
            if (k_ptr->chance[j])
            {
                /* Count the entries */
                alloc_kind_size++;

                /* Group by level */
                num[k_ptr->locale[j]]++;
            }
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Paranoia */
    // if (!num[0]) quit("No surface objects!");

    /*** Initialize object allocation info ***/

    /* Allocate the alloc_kind_table */
    alloc_kind_table = mem_alloc_array(alloc_kind_size, alloc_entry);

    /* Get the table entry */
    table = alloc_kind_table;

    /* Scan the objects */
    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /* Count the "legal" entries */
            if (k_ptr->chance[j])
            {
                int p, x, y, z;

                /* Extract the base level */
                x = k_ptr->locale[j];

                /* Extract the base probability */
                p = (100 / k_ptr->chance[j]);

                /* Skip entries preceding our locale */
                y = (x > 0) ? num[x - 1] : 0;

                /* Skip previous entries at this locale */
                z = y + aux[x];

                /* Load the entry */
                table[z].index = i;
                table[z].level = x;
                table[z].prob1 = p;
                table[z].prob2 = p;
                table[z].prob3 = p;

                /* Another entry complete for this locale */
                aux[x]++;
            }
        }
    }

    /*** Analyze monster allocation info ***/

    /* Clear the "aux" array */
    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);

    /* Clear the "num" array */
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    /* Size of "alloc_race_table" */
    alloc_race_size = 0;

    /* Scan the monsters*/
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Get the i'th race */
        r_ptr = &r_info[i];

        /* Legal monsters */
        if (r_ptr->rarity)
        {
            /* Count the entries */
            alloc_race_size++;

            /* Group by level */
            num[r_ptr->level]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Paranoia */
    // if (!num[0]) quit("No surface monsters!");

    /*** Initialize monster allocation info ***/

    /* Allocate the alloc_race_table */
    alloc_race_table = mem_alloc_array(alloc_race_size, alloc_entry);

    /* Get the table entry */
    table = alloc_race_table;

    /* Scan the monsters*/
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Get the i'th race */
        r_ptr = &r_info[i];

        /* Count valid pairs */
        if (r_ptr->rarity)
        {
            int p, x, y, z;

            /* Extract the base level */
            x = r_ptr->level;

            /* Extract the base probability */
            p = (100 / r_ptr->rarity);

            /* Skip entries preceding our locale */
            y = (x > 0) ? num[x - 1] : 0;

            /* Skip previous entries at this locale */
            z = y + aux[x];

            /* Load the entry */
            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            /* Another entry complete for this locale */
            aux[x]++;
        }
    }

    /*** Analyze ego_item allocation info ***/

    /* Clear the "aux" array */
    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);

    /* Clear the "num" array */
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    /* Size of "alloc_ego_table" */
    alloc_ego_size = 0;

    /* Scan the ego items */
    for (i = 1; i < z_info->e_max; i++)
    {
        /* Get the i'th ego item */
        e_ptr = &e_info[i];

        /* Legal items */
        if (e_ptr->rarity)
        {
            /* Count the entries */
            alloc_ego_size++;

            /* Group by level */
            num[e_ptr->level]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /*** Initialize special item allocation info ***/

    /* Allocate the alloc_ego_table */
    alloc_ego_table = mem_alloc_array(alloc_ego_size, alloc_entry);

    /* Get the table entry */
    table = alloc_ego_table;

    /* Scan the special items */
    for (i = 1; i < z_info->e_max; i++)
    {
        /* Get the i'th ego item */
        e_ptr = &e_info[i];

        /* Count valid pairs */
        if (e_ptr->rarity)
        {
            int p, x, y, z;

            /* Extract the base level */
            x = e_ptr->level;

            /* Extract the base probability */
            p = (100 / e_ptr->rarity);

            /* Skip entries preceding our locale */
            y = (x > 0) ? num[x - 1] : 0;

            /* Skip previous entries at this locale */
            z = y + aux[x];

            /* Load the entry */
            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            /* Another entry complete for this locale */
            aux[x]++;
        }
    }

    /* Success */
    return (0);
}

/*
 * Hack -- take notes on line 23
 */
static void note(cptr str)
{
    Term_erase(0, 23, 255);
    Term_putstr(20, 23, -1, TERM_SLATE, str);
    Term_fresh();
}

/*
 * Hack -- Explain a broken "lib" folder and quit (see below).
 */
static void init_angband_aux(cptr why)
{
    quit(format("%s\n\n%s", why,
        "The 'lib' directory is probably missing or broken.\n"
        "Perhaps the archive was not extracted correctly.\n"
        "See the manual for more information."));
}

extern void display_introduction(void)
{
    /* Clear screen */
    Term_clear();

     /* Hide the cursor for the intro screen while rendering. Do NOT
         toggle the global hide_cursor here — callers (menus) should set
         hide_cursor around any following input waits. */
     bool _saved_cursor_state = false;
     (void)Term_get_cursor(&_saved_cursor_state);
     (void)Term_set_cursor(false);

    Term_putstr(12, 1, -1, TERM_L_BLUE,
        "    The world was young, the mountains green,            ");
    Term_putstr(12, 2, -1, TERM_L_BLUE,
        "       No stain yet on the moon was seen...              ");

    Term_putstr(12, 5, -1, TERM_WHITE,
        "Welcome to Sil-More, Shining Darkness                ");
    Term_putstr(12, 6, -1, TERM_WHITE,
        "  An adventure set in Middle-earth's mythic past,                    ");
    Term_putstr(12, 7, -1, TERM_WHITE,
        "    when the world still rang with elven song          ");
    Term_putstr(12, 8, -1, TERM_WHITE,
        "      and gleamed with dwarven mail.                   ");

    Term_putstr(12, 10, -1, TERM_YELLOW,
        " A reimagining of the classic Sil experience,       ");
    Term_putstr(12, 11, -1, TERM_YELLOW,
        "   enriched by modern roguelike mechanics.                        ");

    Term_putstr(12, 13, -1, TERM_WHITE,
        "Walk the dark halls of Angband and slay creatures black and fell.");
    Term_putstr(12, 14, -1, TERM_WHITE,
        "  Wrest a shining Silmaril from Morgoth's iron crown.");
    Term_putstr(12, 15, -1, TERM_WHITE,
        "    Endure the curses of evil, guided by the wisdom of the Valar. ");
    Term_putstr(12, 16, -1, TERM_WHITE,
        "      And prove your right to live in the lands of Valinor.");

    /* Flush it */
    Term_fresh();

    /* Restore cursor visibility */
    (void)Term_set_cursor(_saved_cursor_state);
}

/*
 * Hack -- main Sil initialization entry point
 *
 * Verify some files, create
 * the high score file, initialize all internal arrays, and
 * load the basic "user pref files".
 *
 * Be very careful to keep track of the order in which things
 * are initialized, in particular, the only thing *known* to
 * be available when this function is called is the "z-term.c"
 * package, and that may not be fully initialized until the
 * end of this function, when the default "user pref files"
 * are loaded and "Term_xtra(TERM_XTRA_REACT,0)" is called.
 *
 * Note that this function attempts to verify the "news" file,
 * and the game aborts (cleanly) on failure, since without the
 * "news" file, it is likely that the "lib" folder has not been
 * correctly located.  Otherwise, the news file is displayed for
 * the user.
 *
 * Note that this function attempts to verify (or create) the
 * "high score" file, and the game aborts (cleanly) on failure,
 * since one of the most common "extraction" failures involves
 * failing to extract all sub-directories (even empty ones), such
 * as by failing to use the "-d" option of "pkunzip", or failing
 * to use the "save empty directories" option with "Compact Pro".
 * This error will often be caught by the "high score" creation
 * code below, since the "lib/apex" directory, being empty in the
 * standard distributions, is most likely to be "lost", making it
 * impossible to create the high score file.
 *
 * Note that various things are initialized by this function,
 * including everything that was once done by "init_some_arrays".
 *
 * This initialization involves the parsing of special files
 * in the "lib/data" and sometimes the "lib/edit" directories.
 *
 * Note that the "template" files are initialized first, since they
 * often contain errors.  This means that macros and message recall
 * and things like that are not available until after they are done.
 *
 * We load the default "user pref files" here in case any "color"
 * changes are needed before character creation.
 *
 * Note that the "graf-xxx.prf" file must be loaded separately,
 * if needed, in the first (?) pass through "TERM_XTRA_REACT".
 */
void init_angband(void)
{
    SDL_IOStream* fd;

    int mode = 0644;

    char buf[1024];
    int i;

    /*** Display the introduction ***/

    sdl_story_font_enable();
    display_introduction();
    sdl_story_font_reset();

    /*** Verify (or create) the "high score" file ***/

    /* Build the filename */
#ifdef SIL_USE_LOCAL_DATA
    path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");
#else
    /* Normal build: scores.raw in meta directory */
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep) *last_sep = '\0';
        path_build(buf, sizeof(buf), meta_dir, "scores.raw");
    } else {
        path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");
    }
#endif

    /* Attempt to open the high score file */
    fd = sdl_fopen(buf, "rb");

    /* Failure */
    if (fd < 0)
    {
        /* File type is "DATA" */
        FILE_TYPE(FILE_TYPE_DATA);

        /* Grab permissions */
        safe_setuid_grab();

        /* Create a new high score file */
        fd = sdl_fmake(buf, mode);

        /* Drop permissions */
        safe_setuid_drop();

        /* Failure */
        if (!fd)
        {
            char why[1024];

            /* Message */
            strnfmt(why, sizeof(why), "Cannot create the '%s' file!", buf);

            /* Crash and burn */
            init_angband_aux(why);
        }
        else
        {
            /* Write version header to new scores file */
        score_file_header header;
        header.version_major = SCORE_FILE_VERSION_MAJOR;
        header.version_minor = SCORE_FILE_VERSION_MINOR;
        header.version_patch = SCORE_FILE_VERSION_PATCH;
        header.version_extra = SCORE_FILE_VERSION_EXTRA;
            header.entry_count = 0;
            header.reserved[0] = 0;
            header.reserved[1] = 0;
            
            sdl_write(fd, (cptr)&header, sizeof(header));
        }
    }

    /* Close it */
    sdl_fclose(fd);

    log_info("Loading metarun...");
    // Load metarun
    if (load_metaruns(1) != 0) {
        init_angband_aux("Cannot load or create metarun file!");
    }


    /*** Initialize some arrays ***/

    /* Initialize size info */
    note("[Initializing array sizes...]");
    if (init_z_info())
        quit("Cannot initialize sizes");

    /* runtypes.raw ------------------------------------------------------ */
    note("[Initializing arrays. (runtypes)]");
    if (init_rt_info()) quit("Cannot initialise run types");


    /* Initialize feature info */
    note("[Initializing arrays... (features)]");
    if (init_f_info())
        quit("Cannot initialize features");

    /* Initialize object info */
    note("[Initializing arrays... (objects)]");
    if (init_k_info())
        quit("Cannot initialize objects");

    /* Initialize ability info */
    note("[Initializing arrays... (abilities)]");
    if (init_b_info())
        quit("Cannot initialize abilities");

    /* Initialize artefact info */
    note("[Initializing arrays... (artefacts)]");
    if (init_a_info())
        quit("Cannot initialize artefacts");
    ensure_artifact_guids();

    /* Initialize special item info */
    note("[Initializing arrays... (special items)]");
    if (init_e_info())
        quit("Cannot initialize special items");

    /* Initialize monster info */
    note("[Initializing arrays... (monsters)]");
    if (init_r_info())
        quit("Cannot initialize monsters");

    /* Snapshot monster base stats for runtime overrides */
    if (!r_base)
    {
        r_base = mem_alloc_array(z_info->r_max, monster_race);
    }
    for (i = 0; i < z_info->r_max; i++)
    {
        r_base[i] = r_info[i];
    }

    /* Initialize feature info */
    note("[Initializing arrays... (vaults)]");
    if (init_v_info())
        quit("Cannot initialize vaults");

    /* Initialize history info */
    note("[Initializing arrays... (histories)]");
    if (init_h_info())
        quit("Cannot initialize histories");

    /* Initialize story info */
    note("[Initializing arrays... (stories)]");
    if (init_st_info())
        quit("Cannot initialize stories");

    /* Initialize style info (visual styles) */
    note("[Initializing arrays... (styles)]");
    if (init_style_info())
        quit("Cannot initialize styles");
    style_info = (style_type*)style_head.info_ptr;
    style_name = style_head.name_ptr;

    /* Initialize curses info */
    note("[Initializing arrays... (curses)]");
    if (init_cu_info())
        quit("Cannot initialize curses");

    /* Initialize major blessing info */
    note("[Initializing arrays... (blessings)]");
    if (init_mb_info())
        quit("Cannot initialize major blessings");

    /* Initialize race info */
    note("[Initializing arrays... (races)]");
    if (init_p_info())
        quit("Cannot initialize races");

    /* Initialize character info */
    note("[Initializing arrays... (characters)]");
    if (init_c_info())
        quit("Cannot initialize characters");

    /* Initialize flavor info */
    note("[Initializing arrays... (flavors)]");
    if (init_flavor_info())
        quit("Cannot initialize flavors");

    /* Initialize skeleton note templates */
    note("[Initializing arrays... (skeleton notes)]");
    if (init_skeleton_note_info())
        quit("Cannot initialize skeleton notes");

    /* Initialize quest info */
    note("[Initializing arrays... (quests)]");
    if (init_quest_info())
        quit("Cannot initialize quests");

    /* Initialize oath info */
    note("[Initializing arrays... (oaths)]");
    if (init_oath_info())
        quit("Cannot initialize oaths");

    /* Initialize some other arrays */
    note("[Initializing arrays... (other)]");
    if (init_other())
        quit("Cannot initialize other stuff");

    /* Initialize some other arrays */
    note("[Initializing arrays... (alloc)]");
    if (init_alloc())
        quit("Cannot initialize alloc stuff");

    /*** Load default user pref files ***/

    /* Initialize feature info */
    note("[Loading basic user pref file...]");

    /* Process that file */
    (void)process_pref_file("pref.prf");

    /* Initialize feature info */
    note("[Initializing Random Artefact Tables...]");

    /* Initialize the random name table */
    if (init_n_info())
        quit("Cannot initialize random name generator stuff");

    /*Build the randart probability tables based on the standard Artefact Set*/
    build_randart_tables();

    /* Clean up old files if this is a fresh start (no existing metarun) */
    if (metarun_created) {
        cleanup_old_game_files();
    }

    /* Done */
    note("                                              ");
}


/* --- UPDATED MAIN-MENU HANDLER ---------------------------------------- */
extern NavResult initial_menu(bool *start_new)
{
    log_info("initial_menu: ENTERED - showing main menu");
    int ch;
    NavResult result = NAV_BACK;
    bool intro_story_font = true;
    sdl_story_font_enable();

    display_introduction();

    /* wizard-mode resurrection warning or blank it out */
    if (arg_wizard)
        Term_putstr(15, 17, 80, TERM_BLUE,
            "Resurrecting a character is a form of cheating.");
    else
        Term_putstr(15, 17, 80, TERM_BLUE,
            "                                                ");

    /* frame */
    Term_putstr(12, 17, 60, TERM_L_DARK,
        "______________________________________________________");

    /* menu lines (new order) */
    if (metarun_created == true)
    Term_putstr(16, 19, 50, TERM_L_BLUE,
        "Start your story (press space to continue)");
    else
    Term_putstr(16, 19, 50,TERM_L_BLUE,
        "Continue your story (press space to continue)");

    Term_putstr(25, 22, 30,TERM_WHITE,
        "press q or ESC to exit");

    Term_fresh();

    /* Prevent inkey() from showing the cursor while waiting on the menu */
    bool _saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    ch = inkey();
    hide_cursor = _saved_hide_cursor;

    /* direct key choices ------------------------------------------------*/

    /* enter : CONTINUE  */
    if (ch == '\n' || ch == '\r' || ch == ' ')
    {
        log_info("initial_menu: User pressed space/enter - starting game");
        *start_new = true;
        result = NAV_OK;   /* start new game */
        goto menu_done;
    }


    /* q : EXIT      */
    if (ch == 'q' || ch == ESCAPE)
    {
        result = NAV_QUIT;                /* handled as quit in main-win.c */
        goto menu_done;
    }

menu_done:
    log_info("initial_menu: EXITING with result=%d", result);
    if (intro_story_font)
        sdl_story_font_reset();
    return result;
}
/* ---------------------------------------------------------------------- */


void cleanup_angband(void)
{
    /* Free the macros */
    macro_free();

    /* Free the macro triggers */
    macro_trigger_free();

    /* Free the allocation tables */
    mem_free_null(alloc_ego_table);
    mem_free_null(alloc_race_table);
    mem_free_null(alloc_kind_table);

    /* Free the player inventory */
    mem_free_null(inventory);

    /*Clean the Autoinscribe*/
    autoinscribe_clean();

    /* Free the lore, monster, and object lists */
    mem_free_null(l_list);
    mem_free_null(mon_list);
    mem_free_null(o_list);

    /* Flow arrays */
    mem_free_null(cave_when);

    /* Free the cave */
    mem_free_null(cave_o_idx);
    mem_free_null(cave_m_idx);
    mem_free_null(cave_feat);
    mem_free_null(cave_color);
    mem_free_null(cave_info);
    mem_free_null(cave_light);

    /* Free the "update_view()" array */
    mem_free_null(view_g);

    /* Free the temp arrays */
    mem_free_null(temp_g);
    mem_free_null(temp_y);
    mem_free_null(temp_x);

    /* Free the messages */
    messages_free();

    /* Free the "quarks" */
    quarks_free();

    /*free the randart arrays*/
    free_randart_tables();

    /* Free the info, name, and text arrays */
    free_info(&flavor_head);
    free_info(&g_head);
    free_info(&b_head);
    free_info(&c_head);
    free_info(&p_head);
    free_info(&h_head);
    free_info(&v_head);
    free_info(&r_head);
    free_info(&e_head);
    free_info(&a_head);
    free_info(&k_head);
    free_info(&f_head);
    free_info(&z_head);
    free_info(&n_head);
    free_info(&style_head);
    free_info(&skeleton_note_head);

    /* Note: format() now uses a static buffer, no cleanup needed */

    /* Free the directories */
    str_free(ANGBAND_DIR);
    str_free(ANGBAND_DIR_APEX);
    str_free(ANGBAND_DIR_METARUN);
    str_free(ANGBAND_DIR_BONE);
    str_free(ANGBAND_DIR_DATA);
    str_free(ANGBAND_DIR_EDIT);
    str_free(ANGBAND_DIR_FILE);
    str_free(ANGBAND_DIR_HELP);
    str_free(ANGBAND_DIR_INFO);
    str_free(ANGBAND_DIR_SAVE);
    str_free(ANGBAND_DIR_PREF);
    str_free(ANGBAND_DIR_USER);
    str_free(ANGBAND_DIR_XTRA);
    str_free(ANGBAND_DIR_SCRIPT);
}
















