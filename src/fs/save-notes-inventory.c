/* File: fs/save-notes-inventory.c -- carved from save.c (shares state via fs/save-internal.h) */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "fs/save-internal.h"
#include <stdio.h>

/*
 * Write the notes into the savefile. Every savefile has at least NOTES_MARK.
 */
void wr_notes(void)
{
    char end_note[80];
    char tmpstr[100];
    char ch;
    bool done = false;

    int i = 0;
    int j = 0;

    // Sil: I've had to re-do this with the removal of the notes file
    //      The code below is pretty verbose and surely there was a better way!
    while (!done)
    {
        j = 0;

        while (true)
        {
            ch = notes_buffer[i];

            tmpstr[j] = ch;

            i++;
            j++;

            if (ch == '\n')
            {
                tmpstr[j - 1] = '\0';

                wr_string(tmpstr);
                break;
            }

            if (ch == '\0')
            {
                done = true;
                break;
            }
        }
    }

    // copy the special notes marker into a string
    SDL_strlcpy(end_note, NOTES_MARK, sizeof(end_note));

    /* Always write NOTES_MARK */
    wr_string(end_note);
}

