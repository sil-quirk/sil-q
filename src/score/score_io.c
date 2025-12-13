#include "score/score_io.h"

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "score/score_logic.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

/* Helper to build score file path correctly for both portable and normal builds */
static bool build_score_path(char* buf, size_t len, const char* filename)
{
#ifdef SIL_USE_LOCAL_DATA
    /* Portable build: scores in apex directory */
    return path_build(buf, len, ANGBAND_DIR_APEX, filename);
#else
    /* Normal build: scores in meta directory (parent of metaruns) */
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep) *last_sep = '\0';
        return path_build(buf, len, meta_dir, filename);
    } else {
        return path_build(buf, len, ANGBAND_DIR_APEX, filename);
    }
#endif
}

#define highscore_fd (score_file_active_ctx()->fd)
#define scores_file_entry_count (score_file_active_ctx()->entry_count)
#define scores_file_version_major (score_file_active_ctx()->version_major)
#define scores_file_version_minor (score_file_active_ctx()->version_minor)
#define scores_file_version_patch (score_file_active_ctx()->version_patch)
#define scores_file_version_extra (score_file_active_ctx()->version_extra)

static score_file_ctx global_score_ctx;
static score_file_ctx* active_score_ctx = &global_score_ctx;

static const char* file_mode_from_flags(int mode);
static bool score_file_upgrade_to_curses(score_file_ctx* ctx, const char *filepath);

score_file_ctx* score_file_set_active_ctx(score_file_ctx* ctx)
{
    score_file_ctx* previous = active_score_ctx;
    active_score_ctx = ctx ? ctx : &global_score_ctx;
    return previous;
}

score_file_ctx* score_file_active_ctx(void)
{
    return active_score_ctx;
}

score_file_ctx* score_file_global_ctx(void)
{
    return &global_score_ctx;
}

bool scores_version_has_curses(const score_file_ctx* ctx)
{
    if (!ctx)
        return false;

    /* Compare version tuple: major.minor.patch.extra */
    if (ctx->version_major > 0)
        return true;

    if (ctx->version_minor > 9)
        return true;
    if (ctx->version_minor < 9)
        return false;

    if (ctx->version_patch > 0)
        return true;

    return (ctx->version_extra >= 6);
}

void score_file_reset_ctx(score_file_ctx* ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

bool score_file_load_header(score_file_ctx* ctx, const char *filepath)
{
    if (!ctx || !filepath)
        return false;

    FILE* file = fopen(filepath, "rb");
    if (!file)
        return false;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < (long)sizeof(score_file_header)) {
        fclose(file);
        log_error("Score file too small to contain header");
        return false;
    }

    score_file_header header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        log_error("Failed to read score file header");
        return false;
    }
    fclose(file);

    if (header.version_major > 127 || header.version_minor > 127 ||
        header.version_patch > 127) {
        log_error("Invalid version in score file header");
        return false;
    }

    long payload = file_size - (long)sizeof(score_file_header);
    if (payload < 0 || (payload % (long)sizeof(high_score)) != 0) {
        log_error("Score file size not aligned with high_score entries");
        return false;
    }
    u32b actual_entries = (u32b)(payload / (long)sizeof(high_score));

    ctx->version_major = header.version_major;
    ctx->version_minor = header.version_minor;
    ctx->version_patch = header.version_patch;
    ctx->version_extra = header.version_extra;
    ctx->entry_count = header.entry_count;

    log_trace("score_file_load_header: cached version %d.%d.%d.%d count=%u (physical=%u)",
              ctx->version_major, ctx->version_minor,
              ctx->version_patch, ctx->version_extra,
              ctx->entry_count, actual_entries);

    if (header.entry_count != actual_entries) {
        log_debug("score_file_load_header: header entry_count=%u but file has %u entries",
                  header.entry_count, actual_entries);
    }

    return true;
}

static bool score_file_upgrade_to_curses(score_file_ctx* ctx, const char *filepath)
{
    if (!ctx || !filepath)
        return false;

    if (scores_version_has_curses(ctx)) {
        return true;
    }

    log_info("Upgrading scores file from v%d.%d.%d.%d to v0.9.0.6 (enabling curse support)",
             ctx->version_major, ctx->version_minor,
             ctx->version_patch, ctx->version_extra);

    FILE* file = fopen(filepath, "r+b");
    if (!file) {
        log_error("Cannot open scores file for upgrade: %s", filepath);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(file_size);
    if (!buffer) {
        fclose(file);
        log_error("Cannot allocate memory for upgrade");
        return false;
    }

    if (fread(buffer, 1, file_size, file) != (size_t)file_size) {
        mem_free_null(buffer);
        fclose(file);
        log_error("Failed to read file during upgrade");
        return false;
    }

    score_file_header* header = (score_file_header*)buffer;
    header->version_major = 0;
    header->version_minor = 9;
    header->version_patch = 0;
    header->version_extra = 6;

    long entry_count = (file_size - sizeof(score_file_header)) / sizeof(high_score);
    for (long i = 0; i < entry_count; i++) {
        long entry_offset = sizeof(score_file_header) + i * sizeof(high_score);
        long pts_offset = entry_offset + 8;
        memset(buffer + pts_offset, ' ', 4);
        buffer[pts_offset + 4] = '\0';
    }

    log_info("Cleared pts field (old score) in %ld entries", entry_count);

    fseek(file, 0, SEEK_SET);
    if (fwrite(buffer, 1, file_size, file) != (size_t)file_size) {
        mem_free_null(buffer);
        fclose(file);
        log_error("Failed to write upgraded file");
        return false;
    }

    mem_free_null(buffer);
    fclose(file);

    ctx->version_major = 0;
    ctx->version_minor = 9;
    ctx->version_patch = 0;
    ctx->version_extra = 6;

    log_info("Successfully upgraded scores file to v0.9.0.6");
    return true;
}

SDL_IOStream* score_file_open(const char *filepath, int mode)
{
    score_file_ctx* ctx = score_file_active_ctx();
    score_file_reset_ctx(ctx);
    bool exists = score_file_load_header(ctx, filepath);

    /* If file doesn't exist and caller allows creation, bootstrap header */
    if (!exists && (mode & O_CREAT)) {
        score_file_header header;
        header.version_major = SCORE_FILE_VERSION_MAJOR;
        header.version_minor = SCORE_FILE_VERSION_MINOR;
        header.version_patch = SCORE_FILE_VERSION_PATCH;
        header.version_extra = SCORE_FILE_VERSION_EXTRA;
        header.entry_count  = 0;

        log_debug("score_file_open: file doesn't exist, creating with header at '%s'", filepath);
        FILE* new_file = fopen(filepath, "wb");
        if (new_file) {
            fwrite(&header, sizeof(header), 1, new_file);
            fclose(new_file);
            exists = score_file_load_header(ctx, filepath);
            log_info("score_file_open: initialized new scores file header at %s", filepath);
        } else {
            log_error("score_file_open: failed to create scores file header at %s (errno=%d: %s)", 
                      filepath, errno, strerror(errno));
        }
    }

    if (exists && (mode & (O_RDWR | O_WRONLY))) {
        score_file_upgrade_to_curses(ctx, filepath);
        score_file_load_header(ctx, filepath);
    }

    SDL_IOStream* file = NULL;
    const char* mode_str = file_mode_from_flags(mode);

    if (mode_str == NULL) {
        file = SDL_IOFromFile(filepath, "r+b");
        if (!file) {
            file = SDL_IOFromFile(filepath, "w+b");
        }
    } else {
        file = SDL_IOFromFile(filepath, mode_str);
    }

    if (file && exists && mode != O_RDONLY) {
        Sint64 file_size = SDL_GetIOSize(file);

        if (file_size >= (Sint64)sizeof(score_file_header)) {
            SDL_SeekIO(file, 0, SDL_IO_SEEK_SET);
            score_file_header header;
            if (SDL_ReadIO(file, &header, sizeof(header)) == sizeof(header)) {
                Sint64 payload = file_size - (Sint64)sizeof(score_file_header);
                if (payload >= 0 && (payload % (Sint64)sizeof(high_score)) == 0) {
                    u32b actual_entries = (u32b)(payload / (Sint64)sizeof(high_score));
                    if (header.entry_count != actual_entries) {
                        log_info("Reconciling scores header: entry_count %u -> %u",
                                 header.entry_count, actual_entries);
                        header.entry_count = actual_entries;
                        SDL_SeekIO(file, 0, SDL_IO_SEEK_SET);
                        SDL_WriteIO(file, &header, sizeof(header));
                        ctx->entry_count = actual_entries;
                    }
                }
            }
        }
    }

    return file;
}

static int collect_scores_load(SDL_IOStream* file, high_score* entries, int limit)
{
    if (!file || !entries || limit <= 0)
        return 0;

    int count = 0;
    while (count < limit) {
        high_score temp;
        if (SDL_ReadIO(file, &temp, sizeof(temp)) != sizeof(temp))
            break;
        if (temp.who[0] == '\0')
            break;
        entries[count++] = temp;
    }
    return count;
}

static int collect_scores_unique(high_score* entries, int count)
{
    if (count <= 1)
        return count;

    high_score unique[MAX_HISCORES + 1];
    int unique_scores[MAX_HISCORES + 1];
    int unique_count = 0;

    for (int i = 0; i < count; i++) {
        int pts = score_points(&entries[i]);
        bool merged = false;
        for (int j = 0; j < unique_count; j++) {
            if (!streq(entries[i].who, unique[j].who))
                continue;

            if (pts > unique_scores[j] ||
                (pts == unique_scores[j] && strcmp(entries[i].day, unique[j].day) > 0) ||
                (pts == unique_scores[j] && streq(entries[i].day, unique[j].day) &&
                 strcmp(entries[i].how, unique[j].how) > 0)) {
                unique[j] = entries[i];
                unique_scores[j] = pts;
            }
            merged = true;
            break;
        }

        if (!merged) {
            unique[unique_count] = entries[i];
            unique_scores[unique_count] = pts;
            unique_count++;
        }
    }

    for (int i = 0; i < unique_count; i++)
        entries[i] = unique[i];

    return unique_count;
}

static int compare_scores_qsort(const void* va, const void* vb)
{
    const high_score* a = (const high_score*)va;
    const high_score* b = (const high_score*)vb;
    return score_compare(a, b);
}

static long score_day_key(const high_score* entry)
{
    if (!entry)
        return LONG_MIN;

    if (streq(entry->how, "(alive and well)"))
        return LONG_MAX;

    if (entry->day[0] != '@')
        return LONG_MIN + 1;

    char buf[32];
    SDL_strlcpy(buf, entry->day + 1, sizeof(buf));
    char* end = NULL;
    long value = strtol(buf, &end, 10);
    if (value <= 0 || !end || *end != '\0')
        return LONG_MIN + 1;

    return value;
}

static int compare_scores_chronological(const void* va, const void* vb)
{
    const high_score* a = (const high_score*)va;
    const high_score* b = (const high_score*)vb;

    long day_a = score_day_key(a);
    long day_b = score_day_key(b);
    if (day_a != day_b)
        return (day_a > day_b) ? -1 : 1;

    int cmp = strcmp(a->who, b->who);
    if (cmp != 0)
        return cmp;

    cmp = strcmp(a->how, b->how);
    if (cmp != 0)
        return cmp;

    return score_compare(a, b);
}

int collect_high_scores(high_score* out, int capacity, bool sort_by_score)
{
    if (!out || capacity <= 0)
        return 0;

    char score_path[1024];
    build_score_path(score_path, sizeof(score_path), "scores.raw");

    score_file_ctx* ctx = score_file_active_ctx();
    SDL_IOStream* file = ctx ? ctx->fd : NULL;
    bool opened_new = false;
    Sint64 restore_pos = 0;

    if (!file) {
        safe_setuid_grab();
        file = score_file_open(score_path, O_RDONLY);
        safe_setuid_drop();
        if (!file)
            return 0;
        opened_new = true;
    } else {
        restore_pos = SDL_TellIO(file);
    }

    if (SDL_SeekIO(file, sizeof(score_file_header), SDL_IO_SEEK_SET) < 0) {
        if (opened_new)
            SDL_CloseIO(file);
        else
            SDL_SeekIO(file, restore_pos, SDL_IO_SEEK_SET);
        return 0;
    }

    int limit = (capacity > MAX_HISCORES) ? MAX_HISCORES : capacity;
    int count = collect_scores_load(file, out, limit);

    if (opened_new)
        SDL_CloseIO(file);
    else
        SDL_SeekIO(file, restore_pos, SDL_IO_SEEK_SET);

    if (count <= 0)
        return 0;

    if (sort_by_score)
        qsort(out, count, sizeof(high_score), compare_scores_qsort);
    else
        qsort(out, count, sizeof(high_score), compare_scores_chronological);

    count = collect_scores_unique(out, count);
    if (count > MAX_HISCORES)
        count = MAX_HISCORES;

    return count;
}

static int highscore_seek_versioned(int i)
{
    long offset = sizeof(score_file_header) + i * sizeof(high_score);

    Sint64 result = SDL_SeekIO(highscore_fd, offset, SDL_IO_SEEK_SET);
    if (result < 0 || result != offset) {
        log_warn("Failed to seek to offset %ld (result=%lld)", offset, (long long)result);
        return -1;
    }
    return 0;
}

static void update_scores_file_header_count(void)
{
    u32b count = 0;
    high_score temp_score;

    highscore_seek_versioned(0);
    while (count < MAX_HISCORES && highscore_read(&temp_score) == 0) {
        if (temp_score.who[0] != '\0') {
            count++;
        } else {
            break;
        }
    }

    if (scores_file_entry_count != count) {
        score_file_header header;
        if (SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET) < 0)
            return;
        size_t read_items = SDL_ReadIO(highscore_fd, &header, sizeof(header));
        if (read_items != sizeof(header))
            return;

        header.entry_count = count;
        if (SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET) < 0)
            return;
        size_t written_items = SDL_WriteIO(highscore_fd, &header, sizeof(header));
        if (written_items != sizeof(header))
            return;
        scores_file_entry_count = count;
        log_debug("Updated scores file header count to %u", count);
    }
}

int highscore_seek(int i)
{
    return highscore_seek_versioned(i);
}

errr highscore_read(high_score* score)
{
    if (!score || !highscore_fd)
        return 1;

    Sint64 current_pos = SDL_TellIO(highscore_fd);
    Sint64 file_size = SDL_GetIOSize(highscore_fd);

    if (current_pos + (Sint64)sizeof(high_score) > file_size)
        return 1;

    size_t bytes_read = SDL_ReadIO(highscore_fd, score, sizeof(high_score));
    if (bytes_read != sizeof(high_score))
        return 1;

    return 0;
}

static bool is_blank_score(const high_score *s)
{
    static const high_score blank_ref;
    return (memcmp(s, &blank_ref, sizeof(high_score)) == 0);
}

int highscore_write(const high_score* score)
{
    if (!score || !highscore_fd)
        return 1;

    if (is_blank_score(score)) {
        log_warn("Refusing to write blank highscore record (ignored)");
        return 0;
    }

    size_t bytes_written = SDL_WriteIO(highscore_fd, score, sizeof(high_score));
    if (bytes_written != sizeof(high_score))
        return 1;

    SDL_FlushIO(highscore_fd);
    update_scores_file_header_count();

    return 0;
}

errr backup_scores_file(const char *filepath)
{
    SDL_IOStream* fd_src = sdl_fopen(filepath, "rb");
    if (!fd_src)
        return 0;

    Sint64 file_size_64 = sdl_size(fd_src);
    int file_size = (file_size_64 > 0) ? (int)file_size_64 : 0;
    if (file_size <= 0) {
        sdl_fclose(fd_src);
        return 0;
    }

    char *buffer = mem_alloc_array(file_size, char);
    if (!buffer) {
        sdl_fclose(fd_src);
        return -1;
    }

    if (sdl_read(fd_src, buffer, file_size) != 0) {
        mem_free_null(buffer);
        sdl_fclose(fd_src);
        return -1;
    }
    sdl_fclose(fd_src);

    char backup_path1[1024], backup_path2[1024], backup_path3[1024];
    strnfmt(backup_path1, sizeof(backup_path1), "%s.bak1", filepath);
    strnfmt(backup_path2, sizeof(backup_path2), "%s.bak2", filepath);
    strnfmt(backup_path3, sizeof(backup_path3), "%s.bak3", filepath);

    fd_kill(backup_path3);

    SDL_IOStream* fd_test2 = sdl_fopen(backup_path2, "rb");
    if (fd_test2) {
        sdl_fclose(fd_test2);
        if (fd_move(backup_path2, backup_path3) != 0) {
            log_error("backup_scores_file: failed to move bak2 to bak3");
        }
    }

    SDL_IOStream* fd_test1 = sdl_fopen(backup_path1, "rb");
    if (fd_test1) {
        sdl_fclose(fd_test1);
        if (fd_move(backup_path1, backup_path2) != 0) {
            log_error("backup_scores_file: failed to move bak1 to bak2");
        }
    }

    SDL_IOStream* fd_dst = sdl_fmake(backup_path1, 0644);
    if (!fd_dst) {
        mem_free_null(buffer);
        return -1;
    }

    errr result = sdl_write(fd_dst, buffer, file_size);
    sdl_fclose(fd_dst);
    mem_free_null(buffer);

    if (result == 0) {
        log_info("Created scores backup: %s (rotated 3 backups)", backup_path1);
    }

    return result;
}

int score_count_alive_entries(void)
{
    char score_path[1024];
    build_score_path(score_path, sizeof(score_path), "scores.raw");

    SDL_IOStream* saved_fd = highscore_fd;
    byte saved_major = scores_file_version_major;
    byte saved_minor = scores_file_version_minor;
    byte saved_patch = scores_file_version_patch;
    byte saved_extra = scores_file_version_extra;
    u32b saved_entry_count = scores_file_entry_count;

    safe_setuid_grab();
    SDL_IOStream* scan_fd = score_file_open(score_path, O_RDONLY);
    safe_setuid_drop();
    if (!scan_fd) {
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return 0;
    }

    highscore_fd = scan_fd;

    int alive = 0;
    if (highscore_seek(0) == 0) {
        high_score entry;
        while (highscore_read(&entry) == 0) {
            if (strcmp(entry.how, "(alive and well)") == 0) {
                alive++;
            }
        }
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = saved_fd;
    scores_file_version_major = saved_major;
    scores_file_version_minor = saved_minor;
    scores_file_version_patch = saved_patch;
    scores_file_version_extra = saved_extra;
    scores_file_entry_count = saved_entry_count;

    return alive;
}

u32b score_sum_dead_points(void)
{
    char score_path[1024];
    build_score_path(score_path, sizeof(score_path), "scores.raw");

    SDL_IOStream* saved_fd = highscore_fd;
    byte saved_major = scores_file_version_major;
    byte saved_minor = scores_file_version_minor;
    byte saved_patch = scores_file_version_patch;
    byte saved_extra = scores_file_version_extra;
    u32b saved_entry_count = scores_file_entry_count;

    safe_setuid_grab();
    SDL_IOStream* scan_fd = score_file_open(score_path, O_RDONLY);
    safe_setuid_drop();
    if (!scan_fd) {
        highscore_fd = saved_fd;
        scores_file_version_major = saved_major;
        scores_file_version_minor = saved_minor;
        scores_file_version_patch = saved_patch;
        scores_file_version_extra = saved_extra;
        scores_file_entry_count = saved_entry_count;
        return 0;
    }

    highscore_fd = scan_fd;

    u32b total = 0;
    if (highscore_seek(0) == 0) {
        high_score entry;
        while (highscore_read(&entry) == 0) {
            char how_buf[sizeof(entry.how) + 1];
            char who_buf[sizeof(entry.who) + 1];
            parse_score_string(entry.how, sizeof(entry.how), how_buf, sizeof(how_buf));
            parse_score_string(entry.who, sizeof(entry.who), who_buf, sizeof(who_buf));

            bool alive_marker = streq(how_buf, "(alive and well)");
            bool escaped_marker = (entry.escaped[0] == 't');

            int points = score_points(&entry);
            if (points < 0) points = 0;

            if (alive_marker || escaped_marker) {
                continue;
            }

            u32b contribution = (u32b)points;
            if (entry.morgoth_slain[0] == 't')
            {
                if (contribution > 0x7FFFFFFFU)
                    contribution = 0xFFFFFFFFU;
                else
                    contribution *= 2;
            }

            if (contribution > 0xFFFFFFFFU - total)
                total = 0xFFFFFFFFU;
            else
                total += contribution;
        }
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = saved_fd;
    scores_file_version_major = saved_major;
    scores_file_version_minor = saved_minor;
    scores_file_version_patch = saved_patch;
    scores_file_version_extra = saved_extra;
    scores_file_entry_count = saved_entry_count;

    return total;
}

static int load_scores_into_array(high_score* entries, int capacity)
{
    if (!highscore_fd || capacity <= 0)
        return 0;

    if (highscore_seek(0))
        return 0;

    int count = 0;
    while (count < capacity)
    {
        high_score temp;
        if (highscore_read(&temp))
            break;
        if (temp.who[0] == '\0')
            break;
        entries[count++] = temp;
    }

    highscore_seek(0);

    return count;
}

static int deduplicate_scores_by_name(high_score* entries, int count)
{
    if (count <= 1)
        return count;

    high_score unique[MAX_HISCORES + 1];
    int unique_scores[MAX_HISCORES + 1];
    int unique_count = 0;

    for (int i = 0; i < count; i++)
    {
        int pts = score_points(&entries[i]);
        bool merged = false;

        for (int j = 0; j < unique_count; j++)
        {
            if (streq(entries[i].who, unique[j].who))
            {
                if (pts > unique_scores[j]
                    || (pts == unique_scores[j] && strcmp(entries[i].day, unique[j].day) > 0)
                    || (pts == unique_scores[j] && streq(entries[i].day, unique[j].day)
                        && strcmp(entries[i].how, unique[j].how) > 0))
                {
                    unique[j] = entries[i];
                    unique_scores[j] = pts;
                }
                merged = true;
                break;
            }
        }

        if (!merged)
        {
            unique[unique_count] = entries[i];
            unique_scores[unique_count] = pts;
            unique_count++;
        }
    }

    for (int i = 0; i < unique_count; i++)
    {
        entries[i] = unique[i];
    }

    return unique_count;
}

int highscore_add(high_score* score)
{
    if (!score || !highscore_fd)
        return -1;

    high_score entries[MAX_HISCORES + 1];
    int count = load_scores_into_array(entries, MAX_HISCORES);
    bool replaced = false;

    for (int i = 0; i < count; i++)
    {
        if (streq(entries[i].who, score->who))
        {
            entries[i] = (*score);
            replaced = true;
            break;
        }
    }

    if (!replaced)
    {
        entries[count++] = (*score);
    }

    qsort(entries, count, sizeof(high_score), compare_scores_qsort);

    count = deduplicate_scores_by_name(entries, count);

    if (count > MAX_HISCORES)
        count = MAX_HISCORES;

    int slot = -1;
    for (int i = 0; i < count; i++)
    {
        if (memcmp(&entries[i], score, sizeof(high_score)) == 0)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        for (int i = 0; i < count; i++)
        {
            if (streq(entries[i].who, score->who)
                && streq(entries[i].day, score->day)
                && streq(entries[i].how, score->how))
            {
                slot = i;
                break;
            }
        }
    }

    if (highscore_seek(0))
    {
        log_error("Failed to seek before rewriting high score table");
        return slot;
    }

    for (int i = 0; i < count; i++)
    {
        if (SDL_WriteIO(highscore_fd, &entries[i], sizeof(high_score)) != sizeof(high_score))
        {
            log_error("Failed to rewrite high score table entry %d", i);
            return -1;
        }
    }

    if (SDL_FlushIO(highscore_fd) != 0)
    {
        log_error("Failed to flush high score file: %s", SDL_GetError());
    }

    score_file_header header;
    if (SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET) >= 0
        && SDL_ReadIO(highscore_fd, &header, sizeof(header)) == sizeof(header))
    {
        header.entry_count = count;
        SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET);
        SDL_WriteIO(highscore_fd, &header, sizeof(header));
        SDL_FlushIO(highscore_fd);
        scores_file_entry_count = count;
    }
    else
    {
        log_warn("Unable to refresh high score header after rewrite");
    }

    highscore_seek(0);

    if (slot < 0 && !replaced)
    {
        log_warn("Score for player '%s' did not reach the published high score table", score->who);
    }

    return slot;
}

void upsert_live_score_on_save(void)
{
    char score_path[1024];
    build_score_path(score_path, sizeof(score_path), "scores.raw");
    log_info("upsert_live_score_on_save: Score path: %s", score_path);

    safe_setuid_grab();
    SDL_IOStream* live_fd = score_file_open(score_path, O_RDWR | O_CREAT);
    safe_setuid_drop();
    if (!live_fd) {
        log_warn("Could not open scores.raw to upsert live save entry");
        return;
    }

    SDL_IOStream* prev_fd = highscore_fd;
    byte prev_major = scores_file_version_major;
    byte prev_minor = scores_file_version_minor;
    byte prev_patch = scores_file_version_patch;
    byte prev_extra = scores_file_version_extra;
    u32b prev_count = scores_file_entry_count;
    highscore_fd = live_fd;

    if (!score_file_load_header(score_file_global_ctx(), score_path)) {
        score_file_header header;
        header.version_major = SCORE_FILE_VERSION_MAJOR;
        header.version_minor = SCORE_FILE_VERSION_MINOR;
        header.version_patch = SCORE_FILE_VERSION_PATCH;
        header.version_extra = SCORE_FILE_VERSION_EXTRA;
        header.entry_count = 0;
        header.reserved[0] = 0;
        header.reserved[1] = 0;
        SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET);
        SDL_WriteIO(highscore_fd, &header, sizeof(header));
        scores_file_version_major = SCORE_FILE_VERSION_MAJOR;
        scores_file_version_minor = SCORE_FILE_VERSION_MINOR;
        scores_file_version_patch = SCORE_FILE_VERSION_PATCH;
        scores_file_version_extra = SCORE_FILE_VERSION_EXTRA;
        scores_file_entry_count = 0;
    }

    char saved_how[sizeof(p_ptr->died_from)];
    SDL_strlcpy(saved_how, p_ptr->died_from, sizeof(saved_how));
    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
    high_score live_score;
    create_score(&live_score);
    SDL_strlcpy(p_ptr->died_from, saved_how, sizeof(p_ptr->died_from));

    if (highscore_seek(0) == 0) {
        high_score tmp; bool found=false; int idx;
        for (idx=0; idx < MAX_HISCORES; idx++) {
            if (highscore_read(&tmp)) break;
            if (streq(tmp.who, live_score.who) && streq(tmp.how, "(alive and well)")) { found=true; break; }
        }
        if (found) {
            highscore_seek(idx);
            highscore_write(&live_score);
        } else {
            highscore_add(&live_score);
        }
    }

    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_END);
    long phys_size = SDL_TellIO(highscore_fd);
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET);

    score_file_header hdrchk;
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_SET);

    if (SDL_ReadIO(highscore_fd, &hdrchk, sizeof(hdrchk)) == sizeof(hdrchk))
    {
        long payload = phys_size - (long)sizeof(score_file_header);
        long logical = (payload >= 0) ? (payload / (long)sizeof(high_score)) : -1;
        log_debug("scores.raw post-save header.entry_count=%u physical_entries=%ld file_size=%ld", hdrchk.entry_count, logical, phys_size);
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = prev_fd;
    scores_file_version_major = prev_major;
    scores_file_version_minor = prev_minor;
    scores_file_version_patch = prev_patch;
    scores_file_version_extra = prev_extra;
    scores_file_entry_count = prev_count;
}

int highscore_dead(char* name)
{
    if (!name)
        return 0;

    bool opened_here = false;

    if (!highscore_fd) {
        char buf[1024];
        build_score_path(buf, sizeof(buf), "scores.raw");
        highscore_fd = score_file_open(buf, O_RDONLY);
        if (!highscore_fd) return 0;
        opened_here = true;
    }

    if (highscore_seek(0)) {
        if (opened_here) { SDL_CloseIO(highscore_fd); highscore_fd = NULL; }
        return 0;
    }

    if (scores_file_entry_count == 0) {
        if (opened_here) { SDL_CloseIO(highscore_fd); highscore_fd = NULL; }
        return 0;
    }

    for (int i = 0; i < MAX_HISCORES; i++) {
        high_score the_score;
        if (highscore_read(&the_score)) break;
        if (strcmp(name, the_score.who) == 0) {
            int dead = (strcmp(the_score.how, "(alive and well)") != 0);
            if (opened_here) { SDL_CloseIO(highscore_fd); highscore_fd = NULL; }
            return dead;
        }
    }

    if (opened_here) { SDL_CloseIO(highscore_fd); highscore_fd = NULL; }
    return 0;
}

static const char* file_mode_from_flags(int mode)
{
    switch (mode) {
    case O_RDONLY: return "rb";
    case O_WRONLY: return "wb";
    case O_RDWR:   return "r+b";
    case (O_WRONLY | O_CREAT): return "wb";
    case (O_RDWR | O_CREAT): return NULL; /* handled specially */
    default:
        return NULL;
    }
}
