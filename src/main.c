/* File: main.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

#include "angband.h"

/*
 * Some machines have a "main()" function in their "main-*.c" file,
 * all the others use this file for their "main()" function.
 */

#if !defined(WINDOWS)

#include "main.h"

/*
 * Sil-y: game in progress
 */
bool game_in_progress = FALSE;

/*
 * List of the available modules in the order they are tried.
 */
static const struct module modules[] = {
#ifdef USE_X11
    { "x11", help_x11, init_x11 },
#endif /* USE_X11 */

#ifdef USE_GCU
    { "gcu", help_gcu, init_gcu },
#endif /* USE_GCU */
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

#ifdef PRIVATE_USER_PATH

/*
 * Create an ".sil/" directory in the users home directory.
 *
 * ToDo: Add error handling.
 * ToDo: Only create the directories when actually writing files.
 */
static void create_user_dir(void)
{
    char dirpath[1024];
    char subdirpath[1024];

    /* Get an absolute path from the filename */
    path_parse(dirpath, sizeof(dirpath), PRIVATE_USER_PATH);

    /* Create the ~/.sil/ directory */
    mkdir(dirpath, 0700);

    /* Build the path to the variant-specific sub-directory */
    path_build(subdirpath, sizeof(subdirpath), dirpath, VERSION_NAME);

    /* Create the directory */
    mkdir(subdirpath, 0700);

#ifdef USE_PRIVATE_SAVE_PATH
    /* Build the path to the scores sub-directory */
    path_build(dirpath, sizeof(dirpath), subdirpath, "data");

    /* Create the directory */
    mkdir(dirpath, 0700);

    /* Build the path to the scores sub-directory */
    path_build(dirpath, sizeof(dirpath), subdirpath, "scores");

    /* Create the directory */
    mkdir(dirpath, 0700);

    /* Build the path to the savefile sub-directory */
    path_build(dirpath, sizeof(dirpath), subdirpath, "save");

    /* Create the directory */
    mkdir(dirpath, 0700);
#endif /* USE_PRIVATE_SAVE_PATH */
}

#endif /* PRIVATE_USER_PATH */

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
    my_strcpy(path, tail ? tail : DEFAULT_PATH, sizeof(path));

    /* Make sure it's terminated */
    path[511] = '\0';

    /* Hack -- Add a path separator (only if needed) */
    if (!suffix(path, PATH_SEP))
        my_strcat(path, PATH_SEP, sizeof(path));

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
        quit_fmt("Try '-d<what>=<path>' not '-d%s'", info);

    /* Analyze */
    switch (tolower((unsigned char)info[0]))
    {
#ifndef FIXED_PATHS
    case 'a':
    {
        string_free(ANGBAND_DIR_APEX);
        ANGBAND_DIR_APEX = string_make(s + 1);
        break;
    }

    case 'f':
    {
        // string_free(ANGBAND_DIR_FILE);
        // ANGBAND_DIR_FILE = string_make(s+1);
        break;
    }

    case 'h':
    {
        // string_free(ANGBAND_DIR_HELP);
        // ANGBAND_DIR_HELP = string_make(s+1);
        break;
    }

    case 'i':
    {
        // string_free(ANGBAND_DIR_INFO);
        // ANGBAND_DIR_INFO = string_make(s+1);
        break;
    }

    case 'x':
    {
        string_free(ANGBAND_DIR_XTRA);
        ANGBAND_DIR_XTRA = string_make(s + 1);
        break;
    }

#ifdef VERIFY_SAVEFILE

    case 'b':
    case 'd':
    case 'e':
    case 's':
    {
        quit_fmt("Restricted option '-d%s'", info);
    }

#else /* VERIFY_SAVEFILE */

    case 'b':
    {
        // string_free(ANGBAND_DIR_BONE);
        // ANGBAND_DIR_BONE = string_make(s+1);
        break;
    }

    case 'd':
    {
        string_free(ANGBAND_DIR_DATA);
        ANGBAND_DIR_DATA = string_make(s + 1);
        break;
    }

    case 'e':
    {
        string_free(ANGBAND_DIR_EDIT);
        ANGBAND_DIR_EDIT = string_make(s + 1);
        break;
    }

    case 's':
    {
        string_free(ANGBAND_DIR_SAVE);
        ANGBAND_DIR_SAVE = string_make(s + 1);
        break;
    }

#endif /* VERIFY_SAVEFILE */

#endif /* FIXED_PATHS */

    case 'u':
    {
        string_free(ANGBAND_DIR_USER);
        ANGBAND_DIR_USER = string_make(s + 1);
        break;
    }

    default:
    {
        quit_fmt("Bad semantics in '-d%s'", info);
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

    bool done = FALSE;

    bool new_game = FALSE;

    int show_score = 0;

    cptr mstr = NULL;

    bool args = TRUE;

    /* Save the "program name" XXX XXX XXX */
    argv0 = argv[0];

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

#if defined(HAVE_SETEGID)

    /* Save some info for later */
    player_euid = geteuid();
    player_egid = getegid();

#endif /* defined(HAVE_SETEGID) */

#if 0 /* XXX XXX XXX */

	/* Redundant setting necessary in case root is running the game */
	/* If not root or game not setuid the following two calls do nothing */

	if (setgid(getegid()) != 0)
	{
		quit("setgid(): cannot set permissions correctly!");
	}

	if (setuid(geteuid()) != 0)
	{
		quit("setuid(): cannot set permissions correctly!");
	}

#endif /* 0 */

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

#ifdef PRIVATE_USER_PATH

    /* Create a directory for the users files. */
    create_user_dir();

#endif /* PRIVATE_USER_PATH */

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
            new_game = TRUE;

            // Sil-y:
            game_in_progress = TRUE;
            break;
        }

        case 'F':
        case 'f':
        {
            arg_fiddle = TRUE;
            break;
        }

        case 'W':
        case 'w':
        {
            arg_wizard = TRUE;
            break;
        }

        case 'A':
        case 'a':
        {
            // VERSION_NAME is defined apparently
            arg_sound = TRUE;
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
            arg_force_roguelike = TRUE;
            break;
        }

        case 'O':
        case 'o':
        {
            arg_force_original = TRUE;
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
            my_strcpy(op_ptr->full_name, arg, sizeof(op_ptr->full_name));

            // Sil-y:
            game_in_progress = TRUE;
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

        case 'V':
        case 'v':
        {
            printf("%s version %s\n", VERSION_NAME, VERSION_STRING);
            quit(NULL);
            break;
        }

        case '-':
        {
            argv[i] = argv[0];
            argc = argc - i;
            argv = argv + i;
            args = FALSE;
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
            puts("  -a       Request audio/sound mode");
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
            puts("  -v       Print Sil-Q version");

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

    /* Process the player name */
    process_player_name(TRUE);

    /* Install "quit" hook */
    quit_aux = quit_hook;

    /* Try the modules in the order specified by modules[] */
    for (i = 0; i < (int)N_ELEMENTS(modules); i++)
    {
        /* User requested a specific module? */
        if (!mstr || (streq(mstr, modules[i].name)))
        {
            if (0 == modules[i].init(argc, argv))
            {
                ANGBAND_SYS = modules[i].name;
                done = TRUE;
                break;
            }
        }
    }

    /* Make sure we have a display! */
    if (!done)
        quit(
            "Unable to prepare any 'display module' (such as 'x11' or 'gcu')!");

    /* Catch nasty signals */
    signals_init();

    /* Initialize */
    init_angband();

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
        if (!game_in_progress)
        {
            int choice = 0;
            int highlight = 1;
            char buf[80];

            if (p_ptr->is_dead)
                highlight = 4;

            /* Process Events until "new" or "open" is selected */
            while (!game_in_progress)
            {
                choice = initial_menu(&highlight);

                switch (choice)
                {
                case 1:
                    path_build(
                        savefile, sizeof(buf), ANGBAND_DIR_XTRA, "tutorial");
                    game_in_progress = TRUE;
                    new_game = FALSE;
                    break;
                case 2:
                    game_in_progress = TRUE;
                    new_game = TRUE;
                    break;
                case 3:
                    game_in_progress = TRUE;
                    new_game = FALSE;

                    /* Prompt for a new name */
                    if (1)
                    {
                        char tmp[14];
                        bool name_selected = FALSE;

                        // Default name
                        my_strcpy(tmp, "<name>", sizeof(tmp));

                        Term_gotoxy(50, 21);

                        while (!name_selected)
                        {
                            if (askfor_aux(tmp, sizeof(tmp)))
                            {
                                my_strcpy(op_ptr->full_name, tmp,
                                    sizeof(op_ptr->full_name));
                            }

                            if (tmp[0] != '\0')
                                name_selected = TRUE;
                            else
                                bell("You must choose a name.");
                        }
                    }
                    process_player_name(TRUE);
                    break;
                case 4:
                    /* Free resources */
                    cleanup_angband();
                    /* Quit */
                    quit(NULL);
                    break;
                }
            }
        }

        /* Handle pending events (most notably update) and flush input */
        Term_flush();

        /*
         * Play a game -- "new_game" is set by "new", "open" or the open
         * document even handler as appropriate
         */
        play_game(new_game);

        // rerun the first initialization routine
        init_stuff();

        // do some more between-games initialization
        re_init_some_things();

        // game no longer in progress
        game_in_progress = FALSE;
    }
}

#endif /* !defined(WINDOWS) */
