#include "angband.h"
#include "support/utf8.h"

static bool utf8_is_continuation(unsigned char c)
{
    return ((c & 0xC0) == 0x80);
}

int utf8_sequence_len_n(cptr str, int len)
{
    unsigned char c0;

    if (!str || len <= 0 || !str[0])
        return 0;

    c0 = (unsigned char)str[0];
    if (c0 < 0x80)
        return 1;

    if (c0 >= 0xC2 && c0 <= 0xDF)
    {
        if (len >= 2 && utf8_is_continuation((unsigned char)str[1]))
            return 2;
        return 1;
    }

    if (c0 == 0xE0)
    {
        if (len >= 3 && ((unsigned char)str[1] >= 0xA0)
            && ((unsigned char)str[1] <= 0xBF)
            && utf8_is_continuation((unsigned char)str[2]))
            return 3;
        return 1;
    }

    if ((c0 >= 0xE1 && c0 <= 0xEC) || (c0 >= 0xEE && c0 <= 0xEF))
    {
        if (len >= 3 && utf8_is_continuation((unsigned char)str[1])
            && utf8_is_continuation((unsigned char)str[2]))
            return 3;
        return 1;
    }

    if (c0 == 0xED)
    {
        if (len >= 3 && ((unsigned char)str[1] >= 0x80)
            && ((unsigned char)str[1] <= 0x9F)
            && utf8_is_continuation((unsigned char)str[2]))
            return 3;
        return 1;
    }

    if (c0 == 0xF0)
    {
        if (len >= 4 && ((unsigned char)str[1] >= 0x90)
            && ((unsigned char)str[1] <= 0xBF)
            && utf8_is_continuation((unsigned char)str[2])
            && utf8_is_continuation((unsigned char)str[3]))
            return 4;
        return 1;
    }

    if (c0 >= 0xF1 && c0 <= 0xF3)
    {
        if (len >= 4 && utf8_is_continuation((unsigned char)str[1])
            && utf8_is_continuation((unsigned char)str[2])
            && utf8_is_continuation((unsigned char)str[3]))
            return 4;
        return 1;
    }

    if (c0 == 0xF4)
    {
        if (len >= 4 && ((unsigned char)str[1] >= 0x80)
            && ((unsigned char)str[1] <= 0x8F)
            && utf8_is_continuation((unsigned char)str[2])
            && utf8_is_continuation((unsigned char)str[3]))
            return 4;
        return 1;
    }

    return 1;
}

int utf8_sequence_len(cptr str)
{
    int len = 0;

    if (!str)
        return 0;

    while (len < 4 && str[len])
        len++;

    return utf8_sequence_len_n(str, len);
}

static u32b utf8_decode_codepoint_n(cptr str, int len, int* out_len)
{
    unsigned char c0;
    int n;

    if (!str || len <= 0 || !str[0])
    {
        if (out_len)
            *out_len = 0;
        return 0;
    }

    c0 = (unsigned char)str[0];
    n = utf8_sequence_len_n(str, len);
    if (out_len)
        *out_len = n;

    if (n == 1)
        return c0;
    if (n == 2)
        return (u32b)(((c0 & 0x1F) << 6) | ((unsigned char)str[1] & 0x3F));
    if (n == 3)
        return (u32b)(((c0 & 0x0F) << 12)
            | (((unsigned char)str[1] & 0x3F) << 6)
            | ((unsigned char)str[2] & 0x3F));
    if (n == 4)
        return (u32b)(((c0 & 0x07) << 18)
            | (((unsigned char)str[1] & 0x3F) << 12)
            | (((unsigned char)str[2] & 0x3F) << 6)
            | ((unsigned char)str[3] & 0x3F));

    return c0;
}

static bool utf8_codepoint_is_combining(u32b cp)
{
    return ((cp >= 0x0300 && cp <= 0x036F)
        || (cp >= 0x1AB0 && cp <= 0x1AFF)
        || (cp >= 0x1DC0 && cp <= 0x1DFF)
        || (cp >= 0x20D0 && cp <= 0x20FF)
        || (cp >= 0xFE20 && cp <= 0xFE2F));
}

bool utf8_has_non_ascii(cptr str)
{
    if (!str)
        return false;

    while (*str)
    {
        if ((unsigned char)*str >= 0x80)
            return true;
        str++;
    }

    return false;
}

bool utf8_has_non_ascii_n(cptr str, int len)
{
    if (!str || len <= 0)
        return false;

    for (int i = 0; i < len && str[i]; i++)
    {
        if ((unsigned char)str[i] >= 0x80)
            return true;
    }

    return false;
}

int utf8_display_width_n(cptr str, int len)
{
    int width = 0;
    int i = 0;

    if (!str || len <= 0)
        return 0;

    while (i < len && str[i])
    {
        int char_len = 0;
        u32b cp = utf8_decode_codepoint_n(str + i, len - i, &char_len);

        if (char_len <= 0)
            break;

        if (cp == '\n')
        {
            i += char_len;
            continue;
        }

        if (!utf8_codepoint_is_combining(cp))
            width++;

        i += char_len;
    }

    return width;
}

int utf8_safe_prefix_len(cptr str, int len)
{
    int safe = 0;
    int i = 0;

    if (!str || len <= 0)
        return 0;

    while (i < len && str[i])
    {
        int char_len = utf8_sequence_len_n(str + i, len - i);

        if (char_len <= 0 || i + char_len > len)
            break;

        safe = i + char_len;
        i += char_len;
    }

    return safe;
}
