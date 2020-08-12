/* File: init2.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

#include "init.h"

#ifdef RUNTIME_PRIVATE_USER_PATH
/*
 * This is a hook so the main program can set a runtime value for the private
 * user path rather than the compile-time one in PRIVATE_USER_PATH.  It is
 * used by the OS X front end.  An alternative would be to have init_paths()
 * take at least two arguments, one specifying the read-only paths and another
 * for the ones where stuff is written.  See version 4 of Angband for one
 * implementation (with three arguments) of that.  To use this hook, also
 * set PRIVATE_USER_PATH (the contents don't matter), and, if desired,
 * USE_PRIVATE_SAVE_PATH.
 */
extern const char *get_runtime_user_path(void);
#endif

/*
 * This file is used to initialize various variables and arrays for the
 * Sil game.  Note the use of "fd_read()" and "fd_write()" to bypass
 * the common limitation of "read()" and "write()" to only 32767 bytes
 * at a time.
 *
 * Several of the arrays for Sil are built from "template" files in
 * the "lib/file" directory, from which quick-load binary "image" files
 * are constructed whenever they are not present in the "lib/data"
 * directory, or if those files become obsolete, if we are allowed.
 *
 * Warning -- the "ascii" file parsers use a minor hack to collect the
 * name and text information in a single pass.  Thus, the game will not
 * be able to load any template file with more than 20K of names or 60K
 * of text, even though technically, up to 64K should be legal.
 *
 * The "init1.c" file is used only to parse the ascii template files,
 * to create the binary image files.  If you include the binary image
 * files instead of the ascii template files, then you can undefine
 * "ALLOW_TEMPLATES", saving about 20K by removing "init1.c".  Note
 * that the binary image files are extremely system dependant.
 */

/*
 * Find the default paths to all of our important sub-directories.
 *
 * The purpose of each sub-directory is described in "variable.c".
 *
 * All of the sub-directories should, by default, be located inside
 * the main "lib" directory, whose location is very system dependant.
 *
 * This function takes a writable buffer, initially containing the
 * "path" to the "lib" directory, for example, "/pkg/lib/sil/",
 * or a system dependant string, for example, ":lib:".  The buffer
 * must be large enough to contain at least 32 more characters.
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "info" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * Mega-Hack -- support fat raw files under NEXTSTEP, using special
 * "suffixed" directories for the "ANGBAND_DIR_DATA" directory, but
 * requiring the directories to be created by hand by the user.
 *
 * Hack -- first we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(char* path)
{
    char* tail;

#ifdef PRIVATE_USER_PATH
    char buf[1024];
#endif /* PRIVATE_USER_PATH */

    /*** Free everything ***/

    /* Free the main path */
    string_free(ANGBAND_DIR);

    /* Free the sub-paths */
    string_free(ANGBAND_DIR_APEX);
    string_free(ANGBAND_DIR_BONE);
    string_free(ANGBAND_DIR_DATA);
    string_free(ANGBAND_DIR_EDIT);
    string_free(ANGBAND_DIR_FILE);
    string_free(ANGBAND_DIR_HELP);
    string_free(ANGBAND_DIR_INFO);
    string_free(ANGBAND_DIR_SAVE);
    string_free(ANGBAND_DIR_PREF);
    string_free(ANGBAND_DIR_USER);
    string_free(ANGBAND_DIR_XTRA);
    string_free(ANGBAND_DIR_SCRIPT);

    /*** Prepare the "path" ***/

    /* Hack -- save the main directory */
    ANGBAND_DIR = string_make(path);

    /* Prepare to append to the Base Path */
    tail = path + strlen(path);

#ifdef VM

    /*** Use "flat" paths with VM/ESA ***/

    /* Use "blank" path names */
    ANGBAND_DIR_APEX = string_make("");
    ANGBAND_DIR_BONE = string_make("");
    ANGBAND_DIR_DATA = string_make("");
    ANGBAND_DIR_EDIT = string_make("");
    ANGBAND_DIR_FILE = string_make("");
    ANGBAND_DIR_HELP = string_make("");
    ANGBAND_DIR_INFO = string_make("");
    ANGBAND_DIR_SAVE = string_make("");
    ANGBAND_DIR_PREF = string_make("");
    ANGBAND_DIR_USER = string_make("");
    ANGBAND_DIR_XTRA = string_make("");
    ANGBAND_DIR_SCRIPT = string_make("");

#else /* VM */

    /*** Build the sub-directory names ***/

    /* Build a path name */
    strcpy(tail, "edit");
    ANGBAND_DIR_EDIT = string_make(path);

    /* Build a path name */
    strcpy(tail, "pref");
    ANGBAND_DIR_PREF = string_make(path);

#ifdef PRIVATE_USER_PATH

    /* Build the path to the user specific directory */
#ifdef RUNTIME_PRIVATE_USER_PATH
    path_build(buf, sizeof(buf), get_runtime_user_path(), VERSION_NAME);
#else
    path_build(buf, sizeof(buf), PRIVATE_USER_PATH, VERSION_NAME);
#endif

    /* Build a relative path name */
    ANGBAND_DIR_USER = string_make(buf);

#else /* PRIVATE_USER_PATH */

    /* Build a path name */
    strcpy(tail, "user");
    ANGBAND_DIR_USER = string_make(path);

#endif /* PRIVATE_USER_PATH */

#ifdef USE_PRIVATE_SAVE_PATH
    /* Build the path to the user specific sub-directory */
    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, "data");

    /* Build a relative path name */
    ANGBAND_DIR_DATA = string_make(buf);

    /* Build the path to the user specific sub-directory */
    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, "scores");

    /* Build a relative path name */
    ANGBAND_DIR_APEX = string_make(buf);

    /* Build the path to the user specific sub-directory */
    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, "save");

    /* Build a relative path name */
    ANGBAND_DIR_SAVE = string_make(buf);

#else /* USE_PRIVATE_SAVE_PATH */

    /* Build a path name */
    strcpy(tail, "data");
    ANGBAND_DIR_DATA = string_make(path);

    /* Build a path name */
    strcpy(tail, "apex");
    ANGBAND_DIR_APEX = string_make(path);

    /* Build a path name */
    strcpy(tail, "save");
    ANGBAND_DIR_SAVE = string_make(path);

#endif /* USE_PRIVATE_SAVE_PATH */

    /* Build a path name */
    strcpy(tail, "xtra");
    ANGBAND_DIR_XTRA = string_make(path);

    /* Build a path name */
    strcpy(tail, "script");
    ANGBAND_DIR_SCRIPT = string_make(path);

#endif /* VM */

#ifdef NeXT

    /* Allow "fat binary" usage with NeXT */
    if (TRUE)
    {
        cptr next = NULL;

#if defined(m68k)
        next = "m68k";
#endif

#if defined(i386)
        next = "i386";
#endif

#if defined(sparc)
        next = "sparc";
#endif

#if defined(hppa)
        next = "hppa";
#endif

        /* Use special directory */
        if (next)
        {
            /* Forget the old path name */
            string_free(ANGBAND_DIR_DATA);

            /* Build a new path name */
            sprintf(tail, "data-%s", next);
            ANGBAND_DIR_DATA = string_make(path);
        }
    }

#endif /* NeXT */
}

#ifdef ALLOW_TEMPLATES

/*
 * Hack -- help give useful error messages
 */
int error_idx;
int error_line;

/*
 * Standard error message text
 */
static cptr err_str[PARSE_ERROR_MAX] = {
    NULL,
    "parse error",
    "obsolete file",
    "missing record header",
    "non-sequential records",
    "invalid flag specification",
    "undefined directive",
    "out of memory",
    "value out of bounds",
    "too few arguments",
    "too many arguments",
    "too many allocation entries",
    "invalid spell frequency",
    "invalid number of items (0-99)",
    "too many entries",
    "vault too big",
    "vault not rectangular (check spaces at end of line?)",
    NULL,
};

#endif /* ALLOW_TEMPLATES */

/*
 * File headers
 */
header z_head;
header v_head;
header f_head;
header k_head;
header b_head;
header a_head;
header e_head;
header r_head;
header p_head;
header c_head;
header h_head;
header b_head;
header g_head;
header flavor_head;
header q_head;
header n_head;

/*** Initialize from binary image files ***/

/*
 * Initialize a "*_info" array, by parsing a binary "image" file
 */
static errr init_info_raw(int fd, header* head)
{
    header test;

    /* Read and verify the header */
    if (fd_read(fd, (char*)(&test), sizeof(header))
        || (test.v_major != head->v_major) || (test.v_minor != head->v_minor)
        || (test.v_patch != head->v_patch) || (test.v_extra != head->v_extra)
        || (test.info_num != head->info_num)
        || (test.info_len != head->info_len)
        || (test.head_size != head->head_size)
        || (test.info_size != head->info_size))
    {
        /* Error */
        return (-1);
    }

    /* Accept the header */
    COPY(head, &test, header);

    /* Allocate the "*_info" array */
    C_MAKE(head->info_ptr, head->info_size, char);

    /* Read the "*_info" array */
    fd_read(fd, head->info_ptr, head->info_size);

    if (head->name_size)
    {
        /* Allocate the "*_name" array */
        C_MAKE(head->name_ptr, head->name_size, char);

        /* Read the "*_name" array */
        fd_read(fd, head->name_ptr, head->name_size);
    }

    if (head->text_size)
    {
        /* Allocate the "*_text" array */
        C_MAKE(head->text_ptr, head->text_size, char);

        /* Read the "*_text" array */
        fd_read(fd, head->text_ptr, head->text_size);
    }

    /* Success */
    return (0);
}

/*
 * Initialize the header of an *_info.raw file.
 */
static void init_header(header* head, int num, int len)
{
    /* Save the "version" */
    head->v_major = VERSION_MAJOR;
    head->v_minor = VERSION_MINOR;
    head->v_patch = VERSION_PATCH;
    head->v_extra = VERSION_EXTRA;

    /* Save the "record" information */
    head->info_num = num;
    head->info_len = len;

    /* Save the size of "*_head" and "*_info" */
    head->head_size = sizeof(header);
    head->info_size = head->info_num * head->info_len;
}

#ifdef ALLOW_TEMPLATES

/*
 * Display a parser error message.
 */
static void display_parse_error(cptr filename, errr err, cptr buf)
{
    cptr oops;

    /* Error string */
    oops = (((err > 0) && (err < PARSE_ERROR_MAX)) ? err_str[err] : "unknown");

    /* Oops */
    msg_format("Error at line %d of '%s.txt'.", error_line, filename);
    msg_format("Record %d contains a '%s' error.", error_idx, oops);
    msg_format("Parsing '%s'.", buf);
    message_flush();

    /* Quit */
    quit_fmt("Error in '%s.txt' file.", filename);
}

#endif /* ALLOW_TEMPLATES */

/*
 * Initialize a "*_info" array
 *
 * Note that we let each entry have a unique "name" and "text" string,
 * even if the string happens to be empty (everyone has a unique '\0').
 */
static errr init_info(cptr filename, header* head)
{
    int fd;

    errr err = 1;

    FILE* fp;

    /* General buffer */
    char buf[1024];

#ifdef ALLOW_TEMPLATES

    /*** Load the binary image file ***/

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

    /* Attempt to open the "raw" file */
    fd = fd_open(buf, O_RDONLY);

    /* Process existing "raw" file */
    if (fd >= 0)
    {
#ifdef CHECK_MODIFICATION_TIME

        err = check_modification_date(fd, format("%s.txt", filename));

#endif /* CHECK_MODIFICATION_TIME */

        /* Attempt to parse the "raw" file */
        if (!err)
            err = init_info_raw(fd, head);

        /* Close it */
        fd_close(fd);
    }

    /* Do we have to parse the *.txt file? */
    if (err)
    {
        /*** Make the fake arrays ***/

        /* Allocate the "*_info" array */
        C_MAKE(head->info_ptr, head->info_size, char);

        /* MegaHack -- make "fake" arrays */
        if (z_info)
        {
            C_MAKE(head->name_ptr, z_info->fake_name_size, char);
            C_MAKE(head->text_ptr, z_info->fake_text_size, char);
        }

        /*** Load the ascii template file ***/

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_EDIT, format("%s.txt", filename));

        /* Open the file */
        fp = my_fopen(buf, "r");

        /* Parse it */
        if (!fp)
            quit(format("Cannot open '%s.txt' file.", filename));

        /* Parse the file */
        err = init_info_txt(fp, buf, head, head->parse_info_txt);

        /* Close it */
        my_fclose(fp);

        /* Errors */
        if (err)
            display_parse_error(filename, err, buf);

        /*** Dump the binary image file ***/

        /* File type is "DATA" */
        FILE_TYPE(FILE_TYPE_DATA);

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the file */
        fd = fd_open(buf, O_RDONLY);

        /* Failure */
        if (fd < 0)
        {
            int mode = 0644;

            /* Grab permissions */
            safe_setuid_grab();

            /* Create a new file */
            fd = fd_make(buf, mode);

            /* Drop permissions */
            safe_setuid_drop();

            /* Failure */
            if (fd < 0)
            {
                /* Complain */
                plog_fmt("Cannot create the '%s' file!", buf);

                /* Continue */
                return (0);
            }
        }

        /* Close it */
        fd_close(fd);

        /* Grab permissions */
        safe_setuid_grab();

        /* Attempt to create the raw file */
        fd = fd_open(buf, O_WRONLY);

        /* Drop permissions */
        safe_setuid_drop();

        /* Failure */
        if (fd < 0)
        {
            /* Complain */
            plog_fmt("Cannot write the '%s' file!", buf);

            /* Continue */
            return (0);
        }

        /* Dump to the file */
        if (fd >= 0)
        {
            /* Dump it */
            fd_write(fd, (cptr)head, head->head_size);

            /* Dump the "*_info" array */
            fd_write(fd, head->info_ptr, head->info_size);

            /* Dump the "*_name" array */
            fd_write(fd, head->name_ptr, head->name_size);

            /* Dump the "*_text" array */
            fd_write(fd, head->text_ptr, head->text_size);

            /* Close */
            fd_close(fd);
        }

        /*** Kill the fake arrays ***/

        /* Free the "*_info" array */
        KILL(head->info_ptr);

        /* MegaHack -- Free the "fake" arrays */
        if (z_info)
        {
            KILL(head->name_ptr);
            KILL(head->text_ptr);
        }

#endif /* ALLOW_TEMPLATES */

        /*** Load the binary image file ***/

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the "raw" file */
        fd = fd_open(buf, O_RDONLY);

        /* Process existing "raw" file */
        if (fd < 0)
            quit(format("Cannot load '%s.raw' file.", filename));

        /* Attempt to parse the "raw" file */
        err = init_info_raw(fd, head);

        /* Close it */
        fd_close(fd);

        /* Error */
        if (err)
            quit(format("Cannot parse '%s.raw' file.", filename));

#ifdef ALLOW_TEMPLATES
    }
#endif /* ALLOW_TEMPLATES */

    /* Success */
    return (0);
}

/*
 * Free the allocated memory for the info-, name-, and text- arrays.
 */
static errr free_info(header* head)
{
    if (head->info_size)
        FREE(head->info_ptr);

    if (head->name_size)
        FREE(head->name_ptr);

    if (head->text_size)
        FREE(head->text_ptr);

    /* Success */
    return (0);
}

/*
 * Initialize the "z_info" array
 */
static errr init_z_info(void)
{
    errr err;

    /* Init the header */
    init_header(&z_head, 1, sizeof(maxima));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    z_head.parse_info_txt = parse_z_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("limits", &z_head);

    /* Set the global variables */
    z_info = z_head.info_ptr;

    return (err);
}

/*
 * Initialize the "f_info" array
 */
static errr init_f_info(void)
{
    errr err;

    /* Init the header */
    init_header(&f_head, z_info->f_max, sizeof(feature_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    f_head.parse_info_txt = parse_f_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("terrain", &f_head);

    /* Set the global variables */
    f_info = f_head.info_ptr;
    f_name = f_head.name_ptr;
    f_text = f_head.text_ptr;

    return (err);
}

/*
 * Initialize the "k_info" array
 */
static errr init_k_info(void)
{
    errr err;

    /* Init the header */
    init_header(&k_head, z_info->k_max, sizeof(object_kind));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    k_head.parse_info_txt = parse_k_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("object", &k_head);

    /* Set the global variables */
    k_info = k_head.info_ptr;
    k_name = k_head.name_ptr;
    k_text = k_head.text_ptr;

    return (err);
}

/*
 * Initialize the "b_info" array
 */
static errr init_b_info(void)
{
    errr err;

    /* Init the header */
    init_header(&b_head, z_info->b_max, sizeof(ability_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    b_head.parse_info_txt = parse_b_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("ability", &b_head);

    /* Set the global variables */
    b_info = b_head.info_ptr;
    b_name = b_head.name_ptr;
    b_text = b_head.text_ptr;

    return (err);
}

/*
 * Initialize the "a_info" array
 */
static errr init_a_info(void)
{
    errr err;

    /* Init the header */
    init_header(&a_head, z_info->art_max, sizeof(artefact_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    a_head.parse_info_txt = parse_a_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("artefact", &a_head);

    /* Set the global variables */
    a_info = a_head.info_ptr;

    a_text = a_head.text_ptr;

    return (err);
}

/*
 * Initialize the "e_info" array
 */
static errr init_e_info(void)
{
    errr err;

    /* Init the header */
    init_header(&e_head, z_info->e_max, sizeof(ego_item_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    e_head.parse_info_txt = parse_e_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("special", &e_head);

    /* Set the global variables */
    e_info = e_head.info_ptr;
    e_name = e_head.name_ptr;
    e_text = e_head.text_ptr;

    return (err);
}

/*
 * Initialize the "r_info" array
 */
static errr init_r_info(void)
{
    errr err;

    /* Init the header */
    init_header(&r_head, z_info->r_max, sizeof(monster_race));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    r_head.parse_info_txt = parse_r_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("monster", &r_head);

    /* Set the global variables */
    r_info = r_head.info_ptr;
    r_name = r_head.name_ptr;
    r_text = r_head.text_ptr;

    return (err);
}

/*
 * Initialize the "v_info" array
 */
static errr init_v_info(void)
{
    errr err;

    /* Init the header */
    init_header(&v_head, z_info->v_max, sizeof(vault_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    v_head.parse_info_txt = parse_v_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("vault", &v_head);

    /* Set the global variables */
    v_info = v_head.info_ptr;
    v_name = v_head.name_ptr;
    v_text = v_head.text_ptr;

    return (err);
}

/*
 * Initialize the "p_info" array
 */
static errr init_p_info(void)
{
    errr err;

    /* Init the header */
    init_header(&p_head, z_info->p_max, sizeof(player_race));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    p_head.parse_info_txt = parse_p_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("race", &p_head);

    /* Set the global variables */
    p_info = p_head.info_ptr;
    p_name = p_head.name_ptr;
    p_text = p_head.text_ptr;

    return (err);
}

/*
 * Initialize the "c_info" array
 */
static errr init_c_info(void)
{
    errr err;

    /* Init the header */
    init_header(&c_head, z_info->c_max, sizeof(player_house));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    c_head.parse_info_txt = parse_c_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("house", &c_head);

    /* Set the global variables */
    c_info = c_head.info_ptr;
    c_name = c_head.name_ptr;
    c_text = c_head.text_ptr;

    return (err);
}

/*
 * Initialize the "h_info" array
 */
static errr init_h_info(void)
{
    errr err;

    /* Init the header */
    init_header(&h_head, z_info->h_max, sizeof(hist_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    h_head.parse_info_txt = parse_h_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("history", &h_head);

    /* Set the global variables */
    h_info = h_head.info_ptr;
    h_text = h_head.text_ptr;

    return (err);
}

/*
 * Initialize the "n_info" structure
 */
static errr init_n_info(void)
{
    errr err;

    /* Init the header */
    init_header(&n_head, 1, sizeof(names_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    n_head.parse_info_txt = parse_n_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("names", &n_head);

    n_info = n_head.info_ptr;

    return (err);
}

/*
 * Initialize the "flavor_info" array
 */
static errr init_flavor_info(void)
{
    errr err;

    /* Init the header */
    init_header(&flavor_head, z_info->flavor_max, sizeof(flavor_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    flavor_head.parse_info_txt = parse_flavor_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("flavor", &flavor_head);

    /* Set the global variables */
    flavor_info = flavor_head.info_ptr;
    flavor_name = flavor_head.name_ptr;
    flavor_text = flavor_head.text_ptr;

    return (err);
}

extern void autoinscribe_clean(void)
{
    if (inscriptions)
    {
        FREE(inscriptions);
    }

    inscriptions = 0;
    inscriptionsCount = 0;
}

extern void autoinscribe_init(void)
{
    /* Paranoia */
    autoinscribe_clean();

    C_MAKE(inscriptions, AUTOINSCRIPTIONS_MAX, autoinscription);
}

/*
 * Reinitialize some things between games
 *
 * Needed because rerunning the whole of init_angband() causes crashes.
 */
extern void re_init_some_things(void)
{
    int i;

    Rand_quick = FALSE;

    // wipe the whole player structure
    (void)WIPE(p_ptr, player_type);

    // clear some additional things
    savefile[0] = '\0';
    playerturn = 0;
    op_ptr->full_name[0] = '\0';

    // clear the terms
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term* old = Term;

        /* Dead window */
        if (!angband_term[i])
            continue;

        /* Activate */
        Term_activate(angband_term[i]);

        /* Erase */
        Term_clear();

        /* Refresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }

    // Reset the autoinscriptions
    autoinscribe_clean();
    autoinscribe_init();

    // display the introduction message again
    display_introduction();

    /* Array of grids */
    FREE(view_g);
    C_MAKE(view_g, VIEW_MAX, u16b);

    /* Array of grids */
    FREE(temp_g);
    C_MAKE(temp_g, TEMP_MAX, u16b);

    /* has_lite patch causes both temp_g and temp_x/y to be used
    in targetting mode: can't use the same memory any more. */
    FREE(temp_y);
    FREE(temp_x);
    C_MAKE(temp_y, TEMP_MAX, byte);
    C_MAKE(temp_x, TEMP_MAX, byte);

    /*** Prepare dungeon arrays ***/

    /* Padded into array */
    FREE(cave_info);
    C_MAKE(cave_info, MAX_DUNGEON_HGT, u16b_256);

    /* Feature array */
    FREE(cave_feat);
    C_MAKE(cave_feat, MAX_DUNGEON_HGT, byte_wid);

    /* Light array */
    FREE(cave_light);
    C_MAKE(cave_light, MAX_DUNGEON_HGT, s16b_wid);

    /* Entity arrays */
    FREE(cave_o_idx);
    FREE(cave_m_idx);
    C_MAKE(cave_o_idx, MAX_DUNGEON_HGT, s16b_wid);
    C_MAKE(cave_m_idx, MAX_DUNGEON_HGT, s16b_wid);

    /* Flow arrays */
    FREE(cave_when);
    C_MAKE(cave_when, MAX_DUNGEON_HGT, byte_wid);

    /*** Prepare "vinfo" array ***/

    /* Used by "update_view()" */
    (void)vinfo_init();

    /*** Prepare entity arrays ***/

    /* Objects */
    FREE(o_list);
    C_MAKE(o_list, z_info->o_max, object_type);

    /* Monsters */
    FREE(mon_list);
    C_MAKE(mon_list, MAX_MONSTERS, monster_type);

    /*** Prepare lore array ***/

    /* Lore */
    FREE(l_list);
    C_MAKE(l_list, z_info->r_max, monster_lore);

    /*** Prepare the inventory ***/

    /* Allocate it */
    FREE(inventory);
    C_MAKE(inventory, INVEN_TOTAL, object_type);

    /*** Prepare the options ***/

    /* Initialize the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        /* Default value */
        op_ptr->opt[i] = option_norm[i];
    }

    /* Initialize the window flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Assume no flags */
        op_ptr->window_flag[i] = 0L;
    }

    // Set some sensible defaults
    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);

    // re-initialize the objects and flavors
    if (init_k_info())
        quit("Cannot initialize objects");
    if (init_flavor_info())
        quit("Cannot initialize flavors");
    if (init_e_info())
        quit("Cannot initialize special items");
}

/*
 * Initialize some other arrays
 */
static errr init_other(void)
{
    int i;

    /*** Prepare the various "bizarre" arrays ***/

    /* Initialize the "macro" package */
    (void)macro_init();

    /* Initialize the "quark" package */
    (void)quarks_init();

    /* Initialize autoinscriptions */
    (void)autoinscribe_init();

    /* Initialize the "message" package */
    (void)messages_init();

    /*** Prepare grid arrays ***/

    /* Array of grids */
    C_MAKE(view_g, VIEW_MAX, u16b);

    /* Array of grids */
    C_MAKE(temp_g, TEMP_MAX, u16b);

    /* has_lite patch causes both temp_g and temp_x/y to be used
    in targetting mode: can't use the same memory any more. */
    C_MAKE(temp_y, TEMP_MAX, byte);
    C_MAKE(temp_x, TEMP_MAX, byte);

    /*** Prepare dungeon arrays ***/

    /* Padded into array */
    C_MAKE(cave_info, MAX_DUNGEON_HGT, u16b_256);

    /* Feature array */
    C_MAKE(cave_feat, MAX_DUNGEON_HGT, byte_wid);

    /* Light array */
    C_MAKE(cave_light, MAX_DUNGEON_HGT, s16b_wid);

    /* Entity arrays */
    C_MAKE(cave_o_idx, MAX_DUNGEON_HGT, s16b_wid);
    C_MAKE(cave_m_idx, MAX_DUNGEON_HGT, s16b_wid);

    /* Flow arrays */
    C_MAKE(cave_when, MAX_DUNGEON_HGT, byte_wid);

    /*** Prepare "vinfo" array ***/

    /* Used by "update_view()" */
    (void)vinfo_init();

    /*** Prepare entity arrays ***/

    /* Objects */
    C_MAKE(o_list, z_info->o_max, object_type);

    /* Monsters */
    C_MAKE(mon_list, MAX_MONSTERS, monster_type);

    /*** Prepare lore array ***/

    /* Lore */
    C_MAKE(l_list, z_info->r_max, monster_lore);

    /*** Prepare the inventory ***/

    /* Allocate it */
    C_MAKE(inventory, INVEN_TOTAL, object_type);

    /*** Prepare the options ***/

    /* Initialize the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        /* Default value */
        op_ptr->opt[i] = option_norm[i];
    }

    /* Initialize the window flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Assume no flags */
        op_ptr->window_flag[i] = 0L;
    }

    // Set some sensible defaults
    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);

    /*** Pre-allocate space for the "format()" buffer ***/

    /* Hack -- Just call the "format()" function */
    (void)format("%s", MAINTAINER);

    /* Success */
    return (0);
}

/*
 * Initialize some other arrays
 */
static errr init_alloc(void)
{
    int i, j;

    object_kind* k_ptr;

    monster_race* r_ptr;

    ego_item_type* e_ptr;

    alloc_entry* table;

    s16b num[MAX_DEPTH];

    s16b aux[MAX_DEPTH];

    /*** Analyze object allocation info ***/

    /* Clear the "aux" array */
    (void)C_WIPE(aux, MAX_DEPTH, s16b);

    /* Clear the "num" array */
    (void)C_WIPE(num, MAX_DEPTH, s16b);

    /* Size of "alloc_kind_table" */
    alloc_kind_size = 0;

    /* Scan the objects */
    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /* Count the "legal" entries */
            if (k_ptr->chance[j])
            {
                /* Count the entries */
                alloc_kind_size++;

                /* Group by level */
                num[k_ptr->locale[j]]++;
            }
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Paranoia */
    // if (!num[0]) quit("No surface objects!");

    /*** Initialize object allocation info ***/

    /* Allocate the alloc_kind_table */
    C_MAKE(alloc_kind_table, alloc_kind_size, alloc_entry);

    /* Get the table entry */
    table = alloc_kind_table;

    /* Scan the objects */
    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /* Count the "legal" entries */
            if (k_ptr->chance[j])
            {
                int p, x, y, z;

                /* Extract the base level */
                x = k_ptr->locale[j];

                /* Extract the base probability */
                p = (100 / k_ptr->chance[j]);

                /* Skip entries preceding our locale */
                y = (x > 0) ? num[x - 1] : 0;

                /* Skip previous entries at this locale */
                z = y + aux[x];

                /* Load the entry */
                table[z].index = i;
                table[z].level = x;
                table[z].prob1 = p;
                table[z].prob2 = p;
                table[z].prob3 = p;

                /* Another entry complete for this locale */
                aux[x]++;
            }
        }
    }

    /*** Analyze monster allocation info ***/

    /* Clear the "aux" array */
    (void)C_WIPE(aux, MAX_DEPTH, s16b);

    /* Clear the "num" array */
    (void)C_WIPE(num, MAX_DEPTH, s16b);

    /* Size of "alloc_race_table" */
    alloc_race_size = 0;

    /* Scan the monsters*/
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Get the i'th race */
        r_ptr = &r_info[i];

        /* Legal monsters */
        if (r_ptr->rarity)
        {
            /* Count the entries */
            alloc_race_size++;

            /* Group by level */
            num[r_ptr->level]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Paranoia */
    // if (!num[0]) quit("No surface monsters!");

    /*** Initialize monster allocation info ***/

    /* Allocate the alloc_race_table */
    C_MAKE(alloc_race_table, alloc_race_size, alloc_entry);

    /* Get the table entry */
    table = alloc_race_table;

    /* Scan the monsters*/
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Get the i'th race */
        r_ptr = &r_info[i];

        /* Count valid pairs */
        if (r_ptr->rarity)
        {
            int p, x, y, z;

            /* Extract the base level */
            x = r_ptr->level;

            /* Extract the base probability */
            p = (100 / r_ptr->rarity);

            /* Skip entries preceding our locale */
            y = (x > 0) ? num[x - 1] : 0;

            /* Skip previous entries at this locale */
            z = y + aux[x];

            /* Load the entry */
            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            /* Another entry complete for this locale */
            aux[x]++;
        }
    }

    /*** Analyze ego_item allocation info ***/

    /* Clear the "aux" array */
    (void)C_WIPE(aux, MAX_DEPTH, s16b);

    /* Clear the "num" array */
    (void)C_WIPE(num, MAX_DEPTH, s16b);

    /* Size of "alloc_ego_table" */
    alloc_ego_size = 0;

    /* Scan the ego items */
    for (i = 1; i < z_info->e_max; i++)
    {
        /* Get the i'th ego item */
        e_ptr = &e_info[i];

        /* Legal items */
        if (e_ptr->rarity)
        {
            /* Count the entries */
            alloc_ego_size++;

            /* Group by level */
            num[e_ptr->level]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /*** Initialize special item allocation info ***/

    /* Allocate the alloc_ego_table */
    C_MAKE(alloc_ego_table, alloc_ego_size, alloc_entry);

    /* Get the table entry */
    table = alloc_ego_table;

    /* Scan the special items */
    for (i = 1; i < z_info->e_max; i++)
    {
        /* Get the i'th ego item */
        e_ptr = &e_info[i];

        /* Count valid pairs */
        if (e_ptr->rarity)
        {
            int p, x, y, z;

            /* Extract the base level */
            x = e_ptr->level;

            /* Extract the base probability */
            p = (100 / e_ptr->rarity);

            /* Skip entries preceding our locale */
            y = (x > 0) ? num[x - 1] : 0;

            /* Skip previous entries at this locale */
            z = y + aux[x];

            /* Load the entry */
            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            /* Another entry complete for this locale */
            aux[x]++;
        }
    }

    /* Success */
    return (0);
}

/*
 * Hack -- take notes on line 23
 */
static void note(cptr str)
{
    Term_erase(0, 23, 255);
    Term_putstr(20, 23, -1, TERM_SLATE, str);
    Term_fresh();
}

/*
 * Hack -- Explain a broken "lib" folder and quit (see below).
 */
static void init_angband_aux(cptr why)
{
    quit_fmt("%s\n\n%s", why,
        "The 'lib' directory is probably missing or broken.\n"
        "Perhaps the archive was not extracted correctly.\n"
        "See the manual for more information.");
}

extern void display_introduction(void)
{
    /* Clear screen */
    Term_clear();

    Term_putstr(14, 3, -1, TERM_L_BLUE,
        "  The world was young, the mountains green,            ");
    Term_putstr(14, 4, -1, TERM_L_BLUE,
        "     No stain yet on the moon was seen...              ");

    Term_putstr(14, 7, -1, TERM_WHITE,
        "Welcome to Sil, a game of adventure set                ");
    Term_putstr(14, 8, -1, TERM_WHITE,
        "  in the First Age of Middle-earth,                    ");
    Term_putstr(14, 9, -1, TERM_WHITE,
        "    when the world still rang with elven song          ");
    Term_putstr(14, 10, -1, TERM_WHITE,
        "      and gleamed with dwarven mail.                   ");

    Term_putstr(14, 11, -1, TERM_WHITE,
        "Walk the dark halls of Angband.                        ");
    Term_putstr(14, 12, -1, TERM_WHITE,
        "  Slay creatures black and fell.                       ");
    Term_putstr(14, 13, -1, TERM_WHITE,
        "    Wrest a shining Silmaril from Morgoth's iron crown.");

    /* Flush it */
    Term_fresh();
}

/*
 * Hack -- main Sil initialization entry point
 *
 * Verify some files, create
 * the high score file, initialize all internal arrays, and
 * load the basic "user pref files".
 *
 * Be very careful to keep track of the order in which things
 * are initialized, in particular, the only thing *known* to
 * be available when this function is called is the "z-term.c"
 * package, and that may not be fully initialized until the
 * end of this function, when the default "user pref files"
 * are loaded and "Term_xtra(TERM_XTRA_REACT,0)" is called.
 *
 * Note that this function attempts to verify the "news" file,
 * and the game aborts (cleanly) on failure, since without the
 * "news" file, it is likely that the "lib" folder has not been
 * correctly located.  Otherwise, the news file is displayed for
 * the user.
 *
 * Note that this function attempts to verify (or create) the
 * "high score" file, and the game aborts (cleanly) on failure,
 * since one of the most common "extraction" failures involves
 * failing to extract all sub-directories (even empty ones), such
 * as by failing to use the "-d" option of "pkunzip", or failing
 * to use the "save empty directories" option with "Compact Pro".
 * This error will often be caught by the "high score" creation
 * code below, since the "lib/apex" directory, being empty in the
 * standard distributions, is most likely to be "lost", making it
 * impossible to create the high score file.
 *
 * Note that various things are initialized by this function,
 * including everything that was once done by "init_some_arrays".
 *
 * This initialization involves the parsing of special files
 * in the "lib/data" and sometimes the "lib/edit" directories.
 *
 * Note that the "template" files are initialized first, since they
 * often contain errors.  This means that macros and message recall
 * and things like that are not available until after they are done.
 *
 * We load the default "user pref files" here in case any "color"
 * changes are needed before character creation.
 *
 * Note that the "graf-xxx.prf" file must be loaded separately,
 * if needed, in the first (?) pass through "TERM_XTRA_REACT".
 */
void init_angband(void)
{
    int fd;

    int mode = 0644;

    char buf[1024];

    /*** Display the introduction ***/

    display_introduction();

    /*** Verify (or create) the "high score" file ***/

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");

    /* Attempt to open the high score file */
    fd = fd_open(buf, O_RDONLY);

    /* Failure */
    if (fd < 0)
    {
        /* File type is "DATA" */
        FILE_TYPE(FILE_TYPE_DATA);

        /* Grab permissions */
        safe_setuid_grab();

        /* Create a new high score file */
        fd = fd_make(buf, mode);

        /* Drop permissions */
        safe_setuid_drop();

        /* Failure */
        if (fd < 0)
        {
            char why[1024];

            /* Message */
            strnfmt(why, sizeof(why), "Cannot create the '%s' file!", buf);

            /* Crash and burn */
            init_angband_aux(why);
        }
    }

    /* Close it */
    fd_close(fd);

    /*** Initialize some arrays ***/

    /* Initialize size info */
    note("[Initializing array sizes...]");
    if (init_z_info())
        quit("Cannot initialize sizes");

    /* Initialize feature info */
    note("[Initializing arrays... (features)]");
    if (init_f_info())
        quit("Cannot initialize features");

    /* Initialize object info */
    note("[Initializing arrays... (objects)]");
    if (init_k_info())
        quit("Cannot initialize objects");

    /* Initialize ability info */
    note("[Initializing arrays... (abilities)]");
    if (init_b_info())
        quit("Cannot initialize abilities");

    /* Initialize artefact info */
    note("[Initializing arrays... (artefacts)]");
    if (init_a_info())
        quit("Cannot initialize artefacts");

    /* Initialize special item info */
    note("[Initializing arrays... (special items)]");
    if (init_e_info())
        quit("Cannot initialize special items");

    /* Initialize monster info */
    note("[Initializing arrays... (monsters)]");
    if (init_r_info())
        quit("Cannot initialize monsters");

    /* Initialize feature info */
    note("[Initializing arrays... (vaults)]");
    if (init_v_info())
        quit("Cannot initialize vaults");

    /* Initialize history info */
    note("[Initializing arrays... (histories)]");
    if (init_h_info())
        quit("Cannot initialize histories");

    /* Initialize race info */
    note("[Initializing arrays... (races)]");
    if (init_p_info())
        quit("Cannot initialize races");

    /* Initialize house info */
    note("[Initializing arrays... (houses)]");
    if (init_c_info())
        quit("Cannot initialize houses");

    /* Initialize flavor info */
    note("[Initializing arrays... (flavors)]");
    if (init_flavor_info())
        quit("Cannot initialize flavors");

    /* Initialize some other arrays */
    note("[Initializing arrays... (other)]");
    if (init_other())
        quit("Cannot initialize other stuff");

    /* Initialize some other arrays */
    note("[Initializing arrays... (alloc)]");
    if (init_alloc())
        quit("Cannot initialize alloc stuff");

    /*** Load default user pref files ***/

    /* Initialize feature info */
    note("[Loading basic user pref file...]");

    /* Process that file */
    (void)process_pref_file("pref.prf");

    /* Initialize feature info */
    note("[Initializing Random Artefact Tables...]");

    /* Initialize the random name table */
    if (init_n_info())
        quit("Cannot initialize random name generator stuff");

    /*Build the randart probability tables based on the standard Artefact Set*/
    build_randart_tables();

    /* Done */
    note("                                              ");
}

extern int initial_menu(int* highlight)
{
    int ch;

    if (arg_wizard)
    {
        Term_putstr(15, 15, 80, TERM_BLUE,
            "Resurrecting a character is a form of cheating.");
        Term_putstr(15, 16, 80, TERM_BLUE,
            "You cannot get a high score with this character.");
    }
    else
    {
        Term_putstr(15, 15, 80, TERM_BLUE,
            "                                                ");
        Term_putstr(15, 16, 80, TERM_BLUE,
            "                                                ");
    }

    Term_putstr(
        15, 17, 60, TERM_L_DARK, "________________________________________");
    Term_putstr(20, 19, 25, (*highlight == 1) ? TERM_L_BLUE : TERM_WHITE,
        "a) Tutorial");
    Term_putstr(20, 20, 25, (*highlight == 2) ? TERM_L_BLUE : TERM_WHITE,
        "b) New character");
    Term_putstr(20, 21, 25, (*highlight == 3) ? TERM_L_BLUE : TERM_WHITE,
        "c) Open saved character");
    Term_putstr(
        20, 22, 25, (*highlight == 4) ? TERM_L_BLUE : TERM_WHITE, "d) Quit");

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(10, 18 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = TRUE;
    ch = inkey();
    hide_cursor = FALSE;

    /* Tutorial */
    if ((ch == 'a') || (ch == 'T') || (ch == 't'))
    {
        *highlight = 1;
        return (1);
    }

    /* New */
    if ((ch == 'b') || (ch == 'N') || (ch == 'n'))
    {
        *highlight = 2;
        return (2);
    }

    /* Open */
    if ((ch == 'c') || (ch == 'O') || (ch == 'o'))
    {
        *highlight = 3;
        return (3);
    }

    /* Quit  */
    if ((ch == 'd') || (ch == 'Q') || (ch == 'q'))
    {
        return (4);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' '))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < 4)
            (*highlight)++;
    }

    /* Set wizard flag for resurrecting dead characters */
    if (ch == KTRL('W'))
    {
        arg_wizard = !arg_wizard;
    }

    return (0);
}

void cleanup_angband(void)
{
    /* Free the macros */
    macro_free();

    /* Free the macro triggers */
    macro_trigger_free();

    /* Free the allocation tables */
    FREE(alloc_ego_table);
    FREE(alloc_race_table);
    FREE(alloc_kind_table);

    /* Free the player inventory */
    FREE(inventory);

    /*Clean the Autoinscribe*/
    autoinscribe_clean();

    /* Free the lore, monster, and object lists */
    FREE(l_list);
    FREE(mon_list);
    FREE(o_list);

    /* Flow arrays */
    FREE(cave_when);

    /* Free the cave */
    FREE(cave_o_idx);
    FREE(cave_m_idx);
    FREE(cave_feat);
    FREE(cave_info);
    FREE(cave_light);

    /* Free the "update_view()" array */
    FREE(view_g);

    /* Free the temp array */
    FREE(temp_g);

    /* Free the messages */
    messages_free();

    /* Free the "quarks" */
    quarks_free();

    /*free the randart arrays*/
    free_randart_tables();

    /* Free the info, name, and text arrays */
    free_info(&flavor_head);
    free_info(&g_head);
    free_info(&b_head);
    free_info(&c_head);
    free_info(&p_head);
    free_info(&h_head);
    free_info(&v_head);
    free_info(&r_head);
    free_info(&e_head);
    free_info(&a_head);
    free_info(&k_head);
    free_info(&f_head);
    free_info(&z_head);
    free_info(&n_head);

    /* Free the format() buffer */
    vformat_kill();

    /* Free the directories */
    string_free(ANGBAND_DIR);
    string_free(ANGBAND_DIR_APEX);
    string_free(ANGBAND_DIR_BONE);
    string_free(ANGBAND_DIR_DATA);
    string_free(ANGBAND_DIR_EDIT);
    string_free(ANGBAND_DIR_FILE);
    string_free(ANGBAND_DIR_HELP);
    string_free(ANGBAND_DIR_INFO);
    string_free(ANGBAND_DIR_SAVE);
    string_free(ANGBAND_DIR_PREF);
    string_free(ANGBAND_DIR_USER);
    string_free(ANGBAND_DIR_XTRA);
    string_free(ANGBAND_DIR_SCRIPT);
}
