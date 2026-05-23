#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"

#define TAB_COLUMNS 8

static bool sanitize_path(char* out, size_t out_len, cptr file)
{
    if (!path_parse(out, out_len, file))
    {
        log_debug("sdl_fopen: unable to parse path '%s'", file);
        return false;
    }
    return true;
}

SDL_IOStream* sdl_fopen(cptr file, cptr mode)
{
    char buf[1024];
    if (!sanitize_path(buf, sizeof(buf), file))
        return NULL;

    SDL_IOStream* stream = SDL_IOFromFile(buf, mode);
    if (!stream)
    {
        log_debug("sdl_fopen: failed to open '%s' mode '%s': %s", buf, mode, SDL_GetError());
    }

    return stream;
}

errr sdl_fclose(SDL_IOStream* stream)
{
    if (!stream)
        return -1;

    if (!SDL_CloseIO(stream))
    {
        log_debug("sdl_fclose: Failed to close stream: %s", SDL_GetError());
        return 1;
    }

    return 0;
}

SDL_IOStream* sdl_fopen_temp(char* buf, size_t max)
{
    if (!path_temp(buf, max))
        return NULL;

    return sdl_fopen(buf, "w");
}

SDL_IOStream* sdl_fmake(cptr file, int mode)
{
    char buf[1024];
    (void)mode;

    if (!sanitize_path(buf, sizeof(buf), file))
        return NULL;

    SDL_IOStream* test = SDL_IOFromFile(buf, "rb");
    if (test)
    {
        SDL_CloseIO(test);
        return NULL;
    }

    return SDL_IOFromFile(buf, "wb");
}

errr sdl_fgets(SDL_IOStream* stream, char* buf, size_t n)
{
    u16b i = 0;
    int len;

    if (n <= 0)
        return 1;

    if (n > 1024)
        n = 1024;

    len = n - 1;

    while (i < len)
    {
        unsigned char c;
        size_t bytes_read = SDL_ReadIO(stream, &c, 1);

        if (bytes_read == 0)
        {
            if (i == 0)
                return 1;
            break;
        }

        if (c == '\n')
            break;

        if (c == '\r')
            continue;

        if (c == '\t')
        {
            do
            {
                if (i == len)
                    break;
                buf[i++] = ' ';
            } while (0x01 & i);
            continue;
        }

        if (c < 32)
            c = ' ';

        buf[i++] = c;

        if (c == '\0')
            break;
    }

    buf[i] = '\0';
    if (i >= 3 && (unsigned char)buf[0] == 0xEF
        && (unsigned char)buf[1] == 0xBB
        && (unsigned char)buf[2] == 0xBF)
    {
        memmove(buf, buf + 3, (size_t)i - 2);
    }
    return 0;
}

errr sdl_fputs(SDL_IOStream* stream, cptr buf, size_t n)
{
    (void)n;
    size_t result = SDL_IOprintf(stream, "%s\n", buf);
    if (result == 0)
    {
        log_debug("sdl_fputs: Failed to write: %s", SDL_GetError());
        return 1;
    }

    return 0;
}

errr sdl_read(SDL_IOStream* stream, char* buf, size_t n)
{
    size_t bytes_read = SDL_ReadIO(stream, buf, n);
    if (bytes_read != n)
    {
        log_debug("sdl_read: Expected %zu bytes, got %zu: %s", n, bytes_read, SDL_GetError());
        return 1;
    }

    return 0;
}

errr sdl_write(SDL_IOStream* stream, cptr buf, size_t n)
{
    size_t bytes_written = SDL_WriteIO(stream, buf, n);
    if (bytes_written != n)
    {
        log_debug("sdl_write: Expected to write %zu bytes, wrote %zu: %s", n, bytes_written, SDL_GetError());
        return 1;
    }

    return 0;
}

errr sdl_seek(SDL_IOStream* stream, Sint64 offset)
{
    if (!stream)
        return 1;

    Sint64 result = SDL_SeekIO(stream, offset, SDL_IO_SEEK_SET);
    if (result < 0)
    {
        log_debug("sdl_seek: Failed to seek to %lld: %s", (long long)offset, SDL_GetError());
        return 1;
    }

    if (result != offset)
    {
        log_debug("sdl_seek: Sought to %lld but ended at %lld", (long long)offset, (long long)result);
        return 1;
    }

    return 0;
}

Sint64 sdl_tell(SDL_IOStream* stream)
{
    Sint64 pos = SDL_TellIO(stream);
    if (pos < 0)
    {
        log_debug("sdl_tell: Failed: %s", SDL_GetError());
    }
    return pos;
}

Sint64 sdl_size(SDL_IOStream* stream)
{
    Sint64 size = SDL_GetIOSize(stream);
    if (size < 0)
    {
        log_debug("sdl_size: Failed: %s", SDL_GetError());
    }
    return size;
}

