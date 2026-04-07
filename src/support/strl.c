#include <SDL3/SDL_stdinc.h>
#include <string.h>

size_t SDL_strlcpy(char* buf, const char* src, size_t bufsize)
{
    size_t len = strlen(src);
    size_t ret = len;

    if (bufsize == 0)
        return ret;

    if (len >= bufsize)
        len = bufsize - 1;

    memcpy(buf, src, len);
    buf[len] = '\0';

    return ret;
}

size_t SDL_strlcat(char* buf, const char* src, size_t bufsize)
{
    size_t dlen = strlen(buf);

    if (dlen < bufsize - 1)
    {
        return dlen + SDL_strlcpy(buf + dlen, src, bufsize - dlen);
    }

    return dlen + strlen(src);
}
