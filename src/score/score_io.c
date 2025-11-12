#include "score/score_io.h"

#include "angband.h"
#include "log/log.h"

#include <string.h>

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
    if (ctx->version_major < 0)
        return false;

    if (ctx->version_minor > 9)
        return true;
    if (ctx->version_minor < 9)
        return false;

    if (ctx->version_patch > 0)
        return true;
    if (ctx->version_patch < 0)
        return false;

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
    score_file_ctx* ctx = score_file_global_ctx();
    bool exists = score_file_load_header(ctx, filepath);

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
