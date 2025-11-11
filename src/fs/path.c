#include "angband.h"
#include "fs/path.h"
#include "log/log.h"

#define FS_PREF_ORG "Sil-QH"

static bool is_separator(char ch)
{
    return ch == '/' || ch == '\\';
}

static bool safe_copy(char* dst, size_t max, const char* src)
{
    if (!dst || !src || max == 0)
        return false;

    return SDL_strlcpy(dst, src, max) < max;
}

static bool safe_append(char* dst, size_t max, const char* src)
{
    if (!dst || !src || *src == '\0')
        return true;

    return SDL_strlcat(dst, src, max) < max;
}

static void normalize_separators(char* path)
{
    if (!path)
        return;

#ifdef WINDOWS
    for (char* cursor = path; *cursor; ++cursor)
    {
        if (*cursor == '/')
            *cursor = '\\';
    }
#else
    for (char* cursor = path; *cursor; ++cursor)
    {
        if (*cursor == '\\')
            *cursor = '/';
    }
#endif
}

static bool ensure_trailing_sep(char* buf, size_t max)
{
    if (!buf)
        return false;

    size_t len = strlen(buf);
    if (len == 0)
        return true;

    if (is_separator(buf[len - 1]))
        return true;

    return safe_append(buf, max, PATH_SEP);
}

static bool append_component(char* buf, size_t max, const char* component)
{
    if (!buf || !component)
        return false;

    const char* trimmed = component;
    while (*trimmed && is_separator(*trimmed))
    {
        trimmed++;
    }

    if (*trimmed == '\0')
        return true;

    if (buf[0] != '\0' && !ensure_trailing_sep(buf, max))
        return false;

    return safe_append(buf, max, trimmed);
}

static const char* resolve_home(void)
{
    const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (home)
        return home;

    home = SDL_getenv("HOME");
    if (home && *home)
        return home;

    home = SDL_getenv("USERPROFILE");
    if (home && *home)
        return home;

    return NULL;
}

static bool expand_home_path(char* buf, size_t max, cptr file)
{
    char* pref_path = NULL;
    const char* home = resolve_home();
    if (!home)
    {
        pref_path = SDL_GetPrefPath(FS_PREF_ORG, VERSION_NAME);
        if (!pref_path)
        {
            log_error(
                "path_parse: SDL_GetPrefPath() failed for '%s': %s",
                file, SDL_GetError());
            return false;
        }
        home = pref_path;
    }

    if (!safe_copy(buf, max, home))
    {
        log_error("path_parse: home path '%s' exceeds buffer", home);
        SDL_free(pref_path);
        return false;
    }

    if (!ensure_trailing_sep(buf, max))
    {
        log_error("path_parse: unable to append separator to '%s'", buf);
        SDL_free(pref_path);
        return false;
    }

    const char* tail = file + 1;
    while (*tail && is_separator(*tail))
        tail++;

    if (!safe_append(buf, max, tail))
    {
        log_error("path_parse: expanded path '%s%s' truncated", home, tail);
        SDL_free(pref_path);
        return false;
    }

    normalize_separators(buf);
    SDL_free(pref_path);
    return true;
}

bool path_parse(char* buf, size_t max, cptr file)
{
    if (!buf || max == 0 || !file || *file == '\0')
        return false;

    buf[0] = '\0';

    if (file[0] == '~')
        return expand_home_path(buf, max, file);

    if (!safe_copy(buf, max, file))
        return false;

    normalize_separators(buf);
    return true;
}

bool path_build(char* buf, size_t max, cptr path, cptr file)
{
    if (!buf || max == 0 || !file)
        return false;

    buf[0] = '\0';

    if (file[0] == '~')
        return path_parse(buf, max, file);

    if (prefix(file, PATH_SEP) && !streq(PATH_SEP, ""))
        return safe_copy(buf, max, file);

    if (!path || path[0] == '\0')
        return safe_copy(buf, max, file);

    if (!safe_copy(buf, max, path))
        return false;

    if (!append_component(buf, max, file))
        return false;

    normalize_separators(buf);
    return true;
}

static bool generate_temp_filename(char* out, size_t max)
{
    Uint64 ticks = SDL_GetTicks();
    Uint64 counter = SDL_GetPerformanceCounter();
    Uint64 entropy = (ticks << 32) ^ counter;

    char random_component[64];
    strnfmt(random_component, sizeof(random_component),
        "tmp-%08x-%08x.tmp", (unsigned)(entropy & 0xFFFFFFFFu),
        (unsigned)(entropy >> 32));

    return safe_append(out, max, random_component);
}

bool path_temp(char* buf, size_t max)
{
    if (!buf || max == 0)
        return false;

    char* pref_path = SDL_GetPrefPath(FS_PREF_ORG, VERSION_NAME);
    if (!pref_path)
    {
        log_error("path_temp: SDL_GetPrefPath() failed: %s", SDL_GetError());
        return false;
    }

    bool success = false;
    for (int attempt = 0; attempt < 32 && !success; ++attempt)
    {
        buf[0] = '\0';

        if (!safe_copy(buf, max, pref_path))
            break;

        if (!ensure_trailing_sep(buf, max))
            break;

        if (!generate_temp_filename(buf, max))
        {
            log_error("path_temp: temp filename overflow in '%s'", buf);
            break;
        }

        if (!SDL_GetPathInfo(buf, NULL))
        {
            SDL_ClearError();
            success = true;
        }
    }

    SDL_free(pref_path);

    if (!success)
        log_error("path_temp: unable to allocate unique path after retries");

    return success;
}

bool fd_kill(cptr file)
{
    char parsed[1024];

    if (!path_parse(parsed, sizeof(parsed), file))
        return false;

    if (!SDL_RemovePath(parsed))
    {
        log_error("fd_kill: remove '%s' failed: %s", parsed, SDL_GetError());
        return false;
    }

    return true;
}

bool fd_move(cptr file, cptr what)
{
    char parsed_src[1024];
    char parsed_dst[1024];

    if (!path_parse(parsed_src, sizeof(parsed_src), file))
        return false;

    if (!path_parse(parsed_dst, sizeof(parsed_dst), what))
        return false;

    if (!SDL_RenamePath(parsed_src, parsed_dst))
    {
        log_error("fd_move: '%s' -> '%s' failed: %s", parsed_src,
            parsed_dst, SDL_GetError());
        return false;
    }

    return true;
}

bool fd_copy(cptr file, cptr what)
{
    char parsed_src[1024];
    char parsed_dst[1024];

    if (!path_parse(parsed_src, sizeof(parsed_src), file))
        return false;

    if (!path_parse(parsed_dst, sizeof(parsed_dst), what))
        return false;

    if (!SDL_CopyFile(parsed_src, parsed_dst))
    {
        log_error("fd_copy: '%s' -> '%s' failed: %s", parsed_src,
            parsed_dst, SDL_GetError());
        return false;
    }

    return true;
}
