/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "blitz.h"
#include "fs/savefile-name.h"
#include "fs/path.h"
#include "externs.h"
#include "log/log.h"
#include <ctype.h>
void process_player_name(bool sf)
{
    size_t src = 0;
    size_t dst = 0;

    /* Process the player name */
    while (op_ptr->full_name[src] && dst + 1 < sizeof(op_ptr->base_name))
    {
        unsigned char c = (unsigned char)op_ptr->full_name[src];
        int len = utf8_sequence_len(op_ptr->full_name + src);

        /* No control characters */
        if (iscntrl(c))
        {
            /* Illegal characters */
            quit(format("Illegal control char (0x%02X) in player name", c));
        }

        if (len <= 0)
            break;

        /* Convert illegal file system characters but preserve some readability */
        if (c < 0x80 && (iscntrl(c) || c == '/' || c == '\\' || c == ':' ||
            c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'))
        {
            /* Convert illegal characters to underscore */
            op_ptr->base_name[dst++] = '_';
            src++;
        }
        else if (c == ' ')
        {
            /* Convert spaces to underscores for file system compatibility */
            op_ptr->base_name[dst++] = '_';
            src++;
        }
        else
        {
            /* Keep all other characters, preserving complete UTF-8 sequences. */
            if (dst + (size_t)len >= sizeof(op_ptr->base_name))
                break;
            for (int j = 0; j < len; j++)
                op_ptr->base_name[dst++] = op_ptr->full_name[src + (size_t)j];
            src += (size_t)len;
        }
    }

    /* Terminate */
    op_ptr->base_name[dst] = '\0';

    /* Require a "base" name */
    if (!op_ptr->base_name[0])
    {
        log_debug("No base name provided, using 'nameless'");
        SDL_strlcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));
    }

    /* Pick savefile name if needed */
    if (sf)
    {
        char temp[128];

        /* Rename the savefile, using the mode-specific base name. */
        build_active_savefile_stem(op_ptr->base_name, temp, sizeof(temp));

        /* Build the filename */
        path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, temp);
        log_info("Generated savefile path: %s", savefile);
    }
}

/*
 * Gets a name for the character, reacting to name changes.
 */
