/* File: main.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

#include "angband.h"
#include "externs.h"
#include "log/bootstrap.h"
#include "gen-log.h"

/*
 * Some machines have a "main()" function in their "main-xxx.c" file,
 * all the others use this file for their "main()" function.
 */

#include "main.h"
#include "log/log.h"
#include "sdl-sound.h"

/*
 * Sil-y: game in progress
 */
bool game_in_progress = false;

/*
 * List of the available modules in the order they are tried.
 */
static const struct module modules[] = {
    { "sdl", help_sdl, init_sdl },
};

/*
 * A hook for "quit()".
 *
 * Close down, then fall back into "quit()".
 */
static void quit_hook(cptr s)
{
    int j;

    /* Unused parameter */
    (void)s;

    /* Scan windows */
    for (j = ANGBAND_TERM_MAX - 1; j >= 0; j--)
    {
        /* Unused */
        if (!angband_term[j])
            continue;

        /* Nuke it */
        term_nuke(angband_term[j]);
    }
}

/*
 * Initialize and verify the file paths, and the score file.
 *
 * Use the ANGBAND_PATH environment var if possible, else use
 * DEFAULT_PATH, and in either case, branch off appropriately.
 *
 * First, we'll look for the ANGBAND_PATH environment variable,
 * and then look for the files in there.  If that doesn't work,
 * we'll try the DEFAULT_PATH constant.  So be sure that one of
 * these two things works...
 *
 * We must ensure that the path ends with "PATH_SEP" if needed,
 * since the "init_file_paths()" function will simply append the
 * relevant "sub-directory names" to the given path.
 *
 * Make sure that the path doesn't overflow the buffer.  We have
 * to leave enough space for the path separator, directory, and
 * filenames.
 */
static void init_stuff(void)
{
    char path[1024];

    cptr tail = NULL;

#ifndef FIXED_PATHS

    /* Get the environment variable */
    tail = getenv("ANGBAND_PATH");

#endif /* FIXED_PATHS */

    /* Use the angband_path, or a default */
    SDL_strlcpy(path, tail ? tail : DEFAULT_PATH, sizeof(path));

    /* Make sure it's terminated */
    path[511] = '\0';

    /* Hack -- Add a path separator (only if needed) */
    if (!suffix(path, PATH_SEP))
        SDL_strlcat(path, PATH_SEP, sizeof(path));

    /* Initialize */
    init_file_paths(path);
}

/*
 * Handle a "-d<what>=<path>" option
 *
 * The "<what>" can be any string starting with the same letter as the
 * name of a subdirectory of the "lib" folder (i.e. "i" or "info").
 *
 * The "<path>" can be any legal path for the given system, and should
 * not end in any special path separator (i.e. "/tmp" or "~/.ang-info").
 */
static void change_path(cptr info)
{
    cptr s;

    /* Find equal sign */
    s = strchr(info, '=');

    /* Verify equal sign */
    if (!s)
        quit(format("Try '-d<what>=<path>' not '-d%s'", info));

    /* Analyze */
    switch (tolower((unsigned char)info[0]))
    {
#ifndef FIXED_PATHS
    case 'a':
    {
        str_free(ANGBAND_DIR_APEX);
        ANGBAND_DIR_APEX = str_dup(s + 1);
        break;
    }

    case 'f':
    {
        // str_free(ANGBAND_DIR_FILE);
        // ANGBAND_DIR_FILE = str_dup(s+1);
        break;
    }

    case 'h':
    {
        // str_free(ANGBAND_DIR_HELP);
        // ANGBAND_DIR_HELP = str_dup(s+1);
        break;
    }

    case 'i':
    {
        // str_free(ANGBAND_DIR_INFO);
        // ANGBAND_DIR_INFO = str_dup(s+1);
        break;
    }

    case 'x':
    {
        str_free(ANGBAND_DIR_XTRA);
        ANGBAND_DIR_XTRA = str_dup(s + 1);
        break;
    }

#ifdef VERIFY_SAVEFILE

    case 'b':
    case 'd':
    case 'e':
    case 's':
    {
        quit(format("Restricted option '-d%s'", info));
    }

#else /* VERIFY_SAVEFILE */

    case 'b':
    {
        // str_free(ANGBAND_DIR_BONE);
        // ANGBAND_DIR_BONE = str_dup(s+1);
        break;
    }

    case 'd':
    {
        str_free(ANGBAND_DIR_DATA);
        ANGBAND_DIR_DATA = str_dup(s + 1);
        break;
    }

    case 'e':
    {
        str_free(ANGBAND_DIR_EDIT);
        ANGBAND_DIR_EDIT = str_dup(s + 1);
        break;
    }

    case 's':
    {
        str_free(ANGBAND_DIR_SAVE);
        ANGBAND_DIR_SAVE = str_dup(s + 1);
        break;
    }

#endif /* VERIFY_SAVEFILE */

#endif /* FIXED_PATHS */

    case 'u':
    {
        str_free(ANGBAND_DIR_USER);
        ANGBAND_DIR_USER = str_dup(s + 1);
        break;
    }

    default:
    {
        quit(format("Bad semantics in '-d%s'", info));
    }
    }
}

/*
 * Simple "main" function for multiple platforms.
 *
 * Note the special "--" option which terminates the processing of
 * standard options.  All non-standard options (if any) are passed
 * directly to the "init_xxx()" function.
 */
int main(int argc, char* argv[])
{
    int i;

    bool done = false;

    int show_score = 0;

    cptr mstr = NULL;

    bool args = true;
    // Initialise logger in 'quiet' mode (don't write to stdout).
    init_logger(true, argv[0]);
    
    // Initialize dedicated generation log (generation.txt)
    gen_log_init(argv[0]);

    /* Initialize character_icky to ensure it starts at 0 */
    character_icky = 0;
    log_debug("main: character_icky initialized to %d", character_icky);

#ifdef SET_UID

    /* Default permissions on files */
    (void)umask(022);

#endif /* SET_UID */

    /* Get the file paths */
    init_stuff();


#ifdef SET_UID

    /* Get the user id (?) */
    player_uid = getuid();

#ifdef SAFE_SETUID

#if defined(HAVE_SETEGID) || defined(SAFE_SETUID_POSIX)

    /* Save some info for later */
    player_euid = geteuid();
    player_egid = getegid();

#endif /* defined(HAVE_SETEGID) || defined(SAFE_SETUID_POSIX) */

#endif /* SAFE_SETUID */

#endif /* SET_UID */

    /* Drop permissions */
    safe_setuid_drop();

#ifdef SET_UID

    /* Initialize the "time" checker */
    if (check_time_init() || check_time())
    {
        quit("The gates to Angband are closed (bad time).");
    }

    /* Get the "user name" as a default player name */
    user_name(op_ptr->full_name, sizeof(op_ptr->full_name), player_uid);

#endif /* SET_UID */

    /* Process the command line arguments */
    for (i = 1; args && (i < argc); i++)
    {
        cptr arg = argv[i];

        /* Require proper options */
        if (*arg++ != '-')
            goto usage;

        /* Analyze option */
        switch (*arg++)
        {
        case 'N':
        case 'n':
        {
            // Sil-y:
            game_in_progress = true;
            break;
        }

        case 'F':
        case 'f':
        {
            arg_fiddle = true;
            break;
        }

        case 'W':
        case 'w':
        {
            arg_wizard = true;
            break;
        }

        case 'V':
        case 'v':
        {
            arg_sound = true;
            break;
        }

        case 'G':
        case 'g':
        {
            /* Default graphics tile */
            arg_graphics = GRAPHICS_MICROCHASM;
            break;
        }

        case 'R':
        case 'r':
        {
            arg_force_roguelike = true;
            break;
        }

        case 'O':
        case 'o':
        {
            arg_force_original = true;
            break;
        }

        case 'S':
        case 's':
        {
            show_score = atoi(arg);
            if (show_score <= 0)
                show_score = 10;
            continue;
        }

        case 'u':
        case 'U':
        {
            if (!*arg)
                goto usage;

            /* Get the savefile name */
            SDL_strlcpy(op_ptr->full_name, arg, sizeof(op_ptr->full_name));

            // Sil-y:
            game_in_progress = true;
            continue;
        }

        case 'm':
        case 'M':
        {
            if (!*arg)
                goto usage;
            mstr = arg;
            continue;
        }

        case 'd':
        case 'D':
        {
            change_path(arg);
            continue;
        }

        case '-':
        {
            argv[i] = argv[0];
            argc = argc - i;
            argv = argv + i;
            args = false;
            break;
        }

        default:
        usage:
        {
            /* Dump usage information */
            puts("Usage: sil [options] [-- subopts]");
            puts("  -n       Start a new character");
            puts("  -f       Request fiddle (verbose) mode");
            puts("  -w       Request wizard mode");
            puts("  -v       Request sound mode");
            puts("  -g       Request graphics mode");
            puts("  -o       Request original keyset (default)");
            puts("  -r       Request rogue-like keyset");
            puts("  -s<num>  Show <num> high scores (default: 10)");
            puts("  -u<who>  Use your <who> savefile");
            puts("  -d<def>  Define a 'lib' dir sub-path");
            puts("  -m<sys>  use Module <sys>, where <sys> can be:");

            /* Print the name and help for each available module */
            for (i = 0; i < (int)N_ELEMENTS(modules); i++)
            {
                printf("     %s   %s\n", modules[i].name, modules[i].help);
            }

            /* Actually abort the process */
            quit(NULL);
        }
        }
        if (*arg)
            goto usage;
    }

    /* Hack -- Forget standard args */
    if (args)
    {
        argc = 1;
        argv[1] = NULL;
    }

    /* Note: process_player_name() is NOT called here anymore.
     * It will be called later when we actually know which character we're playing:
     * - By autoload_alive_from_scores() when loading from scorefile
     * - By character creation when creating a new character
     * - After load_player() succeeds
     */

    /* Install "quit" hook */
    log_register_quit_hook(quit_hook);

    /* Try the modules in the order specified by modules[] */
    for (i = 0; i < (int)N_ELEMENTS(modules); i++)
    {
        /* User requested a specific module? */
        if (!mstr || (streq(mstr, modules[i].name)))
        {
            if (0 == modules[i].init(argc, argv))
            {
                ANGBAND_SYS = modules[i].name;
                done = true;
                break;
            }
        }
    }

    /* Make sure we have a display! */
    if (!done)
        quit("Unable to prepare the SDL display module!");

    /* Catch nasty signals */
    signals_init();

    /* Initialize */
    init_angband();

    /* Initialize sound system (requires ANGBAND_DIR_XTRA to be set) */
    sdl_init_sounds();
    sdl_music_play_main();

    /* Hack -- If requested, display scores and quit */
    if (show_score > 0)
        display_scores(0, show_score);

    /* Wait for response */
    // pause_line(Term->hgt - 1);

    /* Play the game */
    // play_game(new_game);

    // Sil-y: There is now a text menu that can play repeated games
    while (1)
    {
        /* Let the player choose a savefile or start a new game */
        if (!game_in_progress) {
            bool      start_new = false;
            NavResult mn;

            /* loop until the player chooses a valid action */
            while (!game_in_progress) {
                mn = initial_menu(&start_new);
                if (mn == NAV_QUIT) quit(NULL);          /* immediate exit   */
                if (mn == NAV_OK) {                      /* play or load     */
                    game_in_progress = true;
                }
                /* NAV_BACK ⇒ redraw + loop again */
            }
        }

        /* Handle pending events (most notably update) and flush input */
        Term_flush();

        /* Play a game */
        PlayResult pr = play_game();   /* play and capture result */

        // rerun the first initialization routine
        init_stuff();

        // do some more between-games initialization
        re_init_some_things();

        // game no longer in progress
        game_in_progress = false;
        if (pr == PLAY_QUIT) quit(NULL);       /* honour in-game quit     */
    }

    /* Free resources */
    cleanup_angband();

    /* Quit */
    quit(NULL);

    /* Exit */
    return (0);
}
