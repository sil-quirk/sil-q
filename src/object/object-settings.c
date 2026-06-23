#include "angband.h"

#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "object/object-settings.h"

#include <ctype.h>
#include <stdlib.h>

static errr object_settings_parse_command(char *buf)
{
    char *tokens[4];
    int count;

    if (!buf[0] || isspace((unsigned char)buf[0]) || buf[0] == '#')
        return 0;
    if (buf[1] != ':')
        return 1;

    if (buf[0] == 'B')
    {
        long kind;

        if (tokenize(buf + 2, 2, tokens) != 2)
            return 1;

        kind = strtol(tokens[0], NULL, 0);
        if (kind <= 0 || kind >= z_info->k_max)
            return 1;

        add_autoinscription((s16b)kind, tokens[1]);
        return 0;
    }

    if (buf[0] == 'Q')
    {
        count = tokenize(buf + 2, 4, tokens);
        if (count == 2)
        {
            long index = strtol(tokens[0], NULL, 0);
            long level = strtol(tokens[1], NULL, 0);

            if (index < 0 || index >= SQUELCH_BYTES)
                return 1;

            squelch_level[index] = (byte)level;
            return 0;
        }

        if (count == 4)
        {
            long index = strtol(tokens[0], NULL, 0);
            long tval = strtol(tokens[1], NULL, 0);
            long sval = strtol(tokens[2], NULL, 0);
            long squelch = strtol(tokens[3], NULL, 0);

            if (index > 0 && index < z_info->k_max
                && k_info[index].tval == tval && k_info[index].sval == sval)
            {
                k_info[index].squelch = (byte)squelch;
                return 0;
            }

            for (int i = 1; i < z_info->k_max; i++)
            {
                if (k_info[i].tval == tval && k_info[i].sval == sval)
                {
                    k_info[i].squelch = (byte)squelch;
                    return 0;
                }
            }
        }
    }

    return 1;
}

errr object_settings_load(cptr name)
{
    SDL_IOStream *file;
    char path[1024];
    char buf[1024];
    int line = 0;
    errr err = 0;

    if (!name || !name[0]
        || !path_build(path, sizeof(path), ANGBAND_DIR_USER, name))
    {
        return -1;
    }

    file = sdl_fopen(path, "r");
    if (!file)
        return -1;

    while (sdl_fgets(file, buf, sizeof(buf)) == 0)
    {
        line++;
        err = object_settings_parse_command(buf);
        if (err)
        {
            log_warn("Object settings parse error in '%s' at line %d: %s",
                path, line, buf);
            break;
        }
    }

    sdl_fclose(file);
    return err;
}
