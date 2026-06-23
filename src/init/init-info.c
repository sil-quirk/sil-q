#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "item_set.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "sdl-sound.h"
#include "init2-internal.h"
#include "init-lifecycle.h"
#include <SDL3/SDL_filesystem.h>
#include <stdio.h>
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
header rt_head;
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
header st_head;
header cu_head;
header mb_head;
header g_head;
header flavor_head;
header quest_head;
header oath_head;
header n_head;
header style_head;
header skeleton_note_head;

/*** Initialize from binary image files ***/

/*
 * Initialize a "*_info" array, by parsing a binary "image" file
 */
static errr init_info_raw(SDL_IOStream* fd, header* head)
{
    header test;

    /* Read and verify the header */
    if (sdl_read(fd, (char*)(&test), sizeof(header))
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
    memcpy(head, &test, sizeof(header));

    /* Allocate the "*_info" array */
    head->info_ptr = mem_alloc_array(head->info_size, char);

    /* Read the "*_info" array */
    sdl_read(fd, head->info_ptr, head->info_size);

    if (head->name_size)
    {
        /* Allocate the "*_name" array */
        head->name_ptr = mem_alloc_array(head->name_size, char);

        /* Read the "*_name" array */
        sdl_read(fd, head->name_ptr, head->name_size);
    }

    if (head->text_size)
    {
        /* Allocate the "*_text" array */
        head->text_ptr = mem_alloc_array(head->text_size, char);

        /* Read the "*_text" array */
        sdl_read(fd, head->text_ptr, head->text_size);
    }

    /* Success */
    return (0);
}

#ifdef __ANDROID__
static bool build_template_signature_path(char* buf, size_t len, cptr raw_path)
{
    if (!buf || len == 0 || !raw_path)
        return false;

    if (SDL_strlcpy(buf, raw_path, len) >= len)
        return false;

    return SDL_strlcat(buf, ".sig", len) < len;
}

static bool compute_template_signature(cptr txt_path, Uint64* out_signature)
{
    SDL_IOStream* fd;
    Uint64 signature = 1469598103934665603ULL;
    Uint64 total_bytes = 0;
    Uint8 buffer[4096];

    if (!txt_path || !out_signature)
        return false;

    fd = sdl_fopen(txt_path, "rb");
    if (!fd)
    {
        log_warn("template signature: unable to open '%s'", txt_path);
        return false;
    }

    for (;;)
    {
        size_t bytes_read = SDL_ReadIO(fd, buffer, sizeof(buffer));
        if (bytes_read == 0)
            break;

        total_bytes += bytes_read;

        for (size_t i = 0; i < bytes_read; ++i)
        {
            signature ^= buffer[i];
            signature *= 1099511628211ULL;
        }
    }

    if (sdl_fclose(fd) != 0)
    {
        log_warn("template signature: failed to close '%s'", txt_path);
        return false;
    }

    signature ^= total_bytes;
    signature *= 1099511628211ULL;

    *out_signature = signature;
    return true;
}

static bool load_template_signature(cptr raw_path, Uint64* out_signature)
{
    char sig_path[1024];
    SDL_IOStream* fd;
    Uint64 signature;

    if (!out_signature || !build_template_signature_path(sig_path, sizeof(sig_path), raw_path))
        return false;

    fd = sdl_fopen(sig_path, "rb");
    if (!fd)
        return false;

    if (SDL_ReadIO(fd, &signature, sizeof(signature)) != sizeof(signature))
    {
        sdl_fclose(fd);
        return false;
    }

    if (sdl_fclose(fd) != 0)
        return false;

    *out_signature = signature;
    return true;
}

static void save_template_signature(cptr raw_path, cptr txt_path)
{
    char sig_path[1024];
    SDL_IOStream* fd;
    Uint64 signature;

    if (!build_template_signature_path(sig_path, sizeof(sig_path), raw_path))
        return;

    if (!compute_template_signature(txt_path, &signature))
        return;

    fd = sdl_fopen(sig_path, "wb");
    if (!fd)
    {
        log_warn("template signature: unable to write '%s'", sig_path);
        return;
    }

    if (SDL_WriteIO(fd, &signature, sizeof(signature)) != sizeof(signature))
    {
        log_warn("template signature: failed to write '%s'", sig_path);
    }

    if (sdl_fclose(fd) != 0)
    {
        log_warn("template signature: failed to close '%s'", sig_path);
    }
}

static errr check_template_signature(cptr raw_path, cptr txt_path)
{
    Uint64 current_signature;
    Uint64 stored_signature;
    SDL_PathInfo raw_info;

    if (!raw_path || !txt_path)
        return 0;

    if (!SDL_GetPathInfo(raw_path, &raw_info) || raw_info.type != SDL_PATHTYPE_FILE)
        return -1;

    if (!compute_template_signature(txt_path, &current_signature))
        return 0;

    if (!load_template_signature(raw_path, &stored_signature))
    {
        log_info("template signature: missing or unreadable sidecar for '%s' - regenerating", raw_path);
        return -1;
    }

    if (stored_signature != current_signature)
    {
        log_info("template signature: detected updated template '%s' - regenerating '%s'",
            txt_path, raw_path);
        return -1;
    }

    log_debug("template signature: '%s' matches cached raw '%s'", txt_path, raw_path);
    return 0;
}
#endif

/*
 * Initialize the header of an *_info.raw file.
 */
void init_header(header* head, int num, int len)
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
void display_parse_error(cptr filename, errr err, cptr buf)
{
    cptr oops;

    /* Error string */
    oops = (((err > 0) && (err < PARSE_ERROR_MAX)) ? err_str[err] : "unknown");

    /* Oops */
    msg_format("Error at line %d of '%s.txt'.", error_line, filename);
    msg_format("Record %d contains a '%s' error.", error_idx, oops);
    msg_format("Parsing '%s'.", buf);
    
    /* Explicitly log the error to log.txt as requested (one line) */
    log_error("CRITICAL PARSE ERROR: %s in %s.txt at line %d (record %d). Entry: '%s'", oops, filename, error_line, error_idx, buf);
    
    message_flush();

    /* Quit */
    quit(format("Error in '%s.txt' file.", filename));
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
    SDL_IOStream* fd;

    errr err = 1;

    SDL_IOStream* fp;

    /* General buffer */
    char buf[1024];
#ifdef ALLOW_TEMPLATES
    char txt_path[1024];
#endif

#ifdef ALLOW_TEMPLATES

    /*** Load the binary image file ***/

    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, format("%s.txt", filename));

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

    /* Attempt to open the "raw" file */
    fd = sdl_fopen(buf, "rb");

    /* Process existing "raw" file */
    if (fd)
    {
#ifdef CHECK_MODIFICATION_TIME
        /* Check if text file is newer than raw file */
        log_debug("Checking modification times: raw='%s' vs txt='%s'", buf, txt_path);
#ifdef __ANDROID__
        err = check_template_signature(buf, txt_path);
#else
        err = check_modification_date_sdl(buf, txt_path);
#endif
        if (err)
        {
            /* Template changed - close raw and regenerate */
            log_info("Template '%s.txt' changed or is newer than raw file - regenerating", filename);
            sdl_fclose(fd);
            fd = NULL;
        }
        else
        {
            log_debug("Raw file '%s.raw' is up to date", filename);
        }
#endif /* CHECK_MODIFICATION_TIME */

        /* Attempt to parse the "raw" file */
        if (fd && !err)
            err = init_info_raw(fd, head);

        /* Close it */
        sdl_fclose(fd);
    }

    /* Do we have to parse the *.txt file? */
    if (err)
    {
        /*** Make the fake arrays ***/

        /* Allocate the "*_info" array */
        head->info_ptr = mem_alloc_array(head->info_size, char);

        /* MegaHack -- make "fake" arrays */
        if (z_info)
        {
            head->name_ptr = mem_alloc_array(z_info->fake_name_size, char);
            head->text_ptr = mem_alloc_array(z_info->fake_text_size, char);
        }

        /*** Load the ascii template file ***/

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_EDIT, format("%s.txt", filename));

        /* Open the file */
        fp = sdl_fopen(buf, "r");

        /* Parse it */
        if (!fp)
            quit(format("Cannot open '%s.txt' file.", filename));

        /* Parse the file */
        err = init_info_txt(fp, buf, head, head->parse_info_txt);

        /* Close it */
        sdl_fclose(fp);

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
        fd = sdl_fopen(buf, "rb");

        /* Failure */
        if (!fd)
        {
            int mode = 0644;

            /* Grab permissions */
            safe_setuid_grab();

            /* Create a new file */
            fd = sdl_fmake(buf, mode);

            /* Drop permissions */
            safe_setuid_drop();

            /* Failure */
            if (!fd)
            {
                /* Complain */
                plog(format("Cannot create the '%s' file!", buf));

                /* Continue */
                return (0);
            }
        }

        /* Close it */
        sdl_fclose(fd);

        /* Grab permissions */
        safe_setuid_grab();

        /* Attempt to create the raw file */
        fd = sdl_fopen(buf, "wb");

        /* Drop permissions */
        safe_setuid_drop();

        /* Failure */
        if (!fd)
        {
            /* Complain */
            plog(format("Cannot write the '%s' file!", buf));

            /* Continue */
            return (0);
        }

        /* Dump to the file */
        if (fd)
        {
            /* Dump it */
            sdl_write(fd, (cptr)head, head->head_size);

            /* Dump the "*_info" array */
            sdl_write(fd, head->info_ptr, head->info_size);

            /* Dump the "*_name" array */
            sdl_write(fd, head->name_ptr, head->name_size);

            /* Dump the "*_text" array */
            sdl_write(fd, head->text_ptr, head->text_size);

            /* Close */
            sdl_fclose(fd);

#ifdef __ANDROID__
            save_template_signature(buf, txt_path);
#endif
        }

        /*** Kill the fake arrays ***/

        /* Free the "*_info" array */
        mem_free_null(head->info_ptr);

        /* MegaHack -- Free the "fake" arrays */
        if (z_info)
        {
            mem_free_null(head->name_ptr);
            mem_free_null(head->text_ptr);
        }

#endif /* ALLOW_TEMPLATES */

        /*** Load the binary image file ***/

        /* Build the filename */
        path_build(
            buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the "raw" file */
        fd = sdl_fopen(buf, "rb");

        /* Process existing "raw" file */
        if (!fd)
            quit(format("Cannot load '%s.raw' file.", filename));

        /* Attempt to parse the "raw" file */
        err = init_info_raw(fd, head);

        /* Close it */
        sdl_fclose(fd);

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
errr free_info(header* head)
{
    if (head->info_size)
        mem_free_null(head->info_ptr);

    if (head->name_size)
        mem_free_null(head->name_ptr);

    if (head->text_size)
        mem_free_null(head->text_ptr);

    /* Success */
    return (0);
}

/*
 * Initialize the "z_info" array
 */
errr init_z_info(void)
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
errr init_f_info(void)
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
 * Initialize the "style_info" array
 */
errr init_style_info(void)
{
    errr err;
    /* Default to zero if not specified yet; will be set by limits.txt */
    init_header(&style_head, z_info->style_max, sizeof(style_type));
    style_head.parse_info_txt = parse_style_info;
    err = init_info("style", &style_head);
    if (err) return err;
    /* Ensure M: banner strings are loaded even if RAW cache was used. */
    styles_reload_messages_from_text();
    /* Load level/vault rules from separate file (always parse text for side-effects).
     * We bypass the RAW cache here so manual edits to style-levels.txt take effect
     * even when ALLOW_TEMPLATES is not defined. */
    {
        SDL_IOStream* fp;
        char buf[1024];
        header levels_head;
        init_header(&levels_head, 1, 1);
        /* Build full path to lib/edit/style-levels.txt */
        path_build(buf, sizeof(buf), ANGBAND_DIR_EDIT, format("%s.txt", "style-levels"));
        fp = sdl_fopen(buf, "r");
        if (!fp) quit("Cannot open 'style-levels.txt' file.");
        /* Parse the file using the style-levels parser (populates global rule tables) */
        {
            char linebuf[1024];
            err = init_info_txt(fp, linebuf, &levels_head, parse_style_levels);
        }
        sdl_fclose(fp);
        if (err)
        {
            /* Report a parse error with helpful context */
            display_parse_error("style-levels", err, "style-levels");
            return err;
        }
    }

    /* No separate pass for D: depth banners; per requirements, banners come from per-style M: only. */
    return 0;
}

errr init_partition_info(void)
{
    errr err;
    SDL_IOStream* fp;
    char path[1024];
    char linebuf[1024];
    header part_head;

    init_header(&part_head, 1, 1);
    partition_config_reset();

    path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "partition"));
    fp = sdl_fopen(path, "r");
    if (!fp)
        quit("Cannot open 'partition.txt' file.");

    err = init_info_txt(fp, linebuf, &part_head, parse_partition_info);
    sdl_fclose(fp);

    if (err)
    {
        display_parse_error("partition", err, linebuf);
        return err;
    }

    return 0;
}

/*
 * Initialize the "k_info" array
 */
errr init_k_info(void)
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
errr init_b_info(void)
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
errr init_a_info(void)
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

void ensure_artifact_guids(void)
{
    if (!a_info || !z_info)
        return;

    for (int i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        if (!a_ptr)
            continue;

        if (!score_guid_is_zero(&a_ptr->guid))
            continue;

        const char* name = a_ptr->name[0] ? a_ptr->name : "unknown-artifact";
        a_ptr->guid = score_guid_from_string(name, (u32b)i);
    }
}

void ensure_artifact_spawn_numbers(void)
{
    if (!a_info || !z_info)
        return;

    for (int i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        if (!a_ptr)
            continue;
        if (a_ptr->spawn_num == 0)
            a_ptr->spawn_num = 1;
    }
}

/*
 * Initialize the "e_info" array
 */
errr init_e_info(void)
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
errr init_r_info(void)
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
errr init_v_info(void)
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

errr init_rt_info(void)
{
    errr err;
    init_header(&rt_head, z_info->rt_max, sizeof(runtype_type));     /* (1) */
#ifdef ALLOW_TEMPLATES
    rt_head.parse_info_txt = parse_rt_info;                          /* (2) */
#endif
    err = init_info("runtypes", &rt_head);                           /* (3) */

    runtype_info = rt_head.info_ptr;                                 /* (4) global */
    return err;
}


/*
 * Initialize the "p_info" array
 */
errr init_p_info(void)
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
errr init_c_info(void)
{
    errr err;

    /* Init the header */
    init_header(&c_head, z_info->c_max, sizeof(character_profile));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    c_head.parse_info_txt = parse_c_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("character", &c_head);

    /* Set the global variables */
    c_info = c_head.info_ptr;
    c_name = c_head.name_ptr;
    c_text = c_head.text_ptr;

    return (err);
}

/*
 * Initialize the "h_info" array
 */
errr init_h_info(void)
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
 * Initialize the "st_info" array
 */
errr init_st_info(void)
{
    errr err;

    /* Init the header */
    init_header(&st_head, z_info->st_max, sizeof(story_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    st_head.parse_info_txt = parse_st_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("story", &st_head);

    /* Set the global variables */
    st_info = st_head.info_ptr;
    st_text = st_head.text_ptr;
    st_name = st_head.name_ptr;

    return (err);
}

/*
 * Initialize the "cu_info" array
 */
errr init_cu_info(void)
{
    errr err;

    /* Init the header */
    init_header(&cu_head, z_info->cu_max, sizeof(curse_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    cu_head.parse_info_txt = parse_cu_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("curses", &cu_head);

    /* Set the global variables */
    cu_info = cu_head.info_ptr;
    cu_text = cu_head.text_ptr;
    cu_name = cu_head.name_ptr;

    return (err);
}

/*
 * Initialize the blessing definitions array
 */
errr init_mb_info(void)
{
    errr err;

    init_header(&mb_head, z_info->mb_max, sizeof(major_blessing_type));

#ifdef ALLOW_TEMPLATES
    mb_head.parse_info_txt = parse_mb_info;
#endif /* ALLOW_TEMPLATES */

    err = init_info("blessing", &mb_head);

    mb_info = mb_head.info_ptr;
    mb_text = mb_head.text_ptr;
    mb_name = mb_head.name_ptr;

    return err;
}

/*
 * Initialize the "n_info" structure
 */
errr init_n_info(void)
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
errr init_flavor_info(void)
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

/*
 * Initialize the special effect graphics (misc_to_attr, misc_to_char)
 */
static header effect_head;

static void effect_visuals_apply(bool ascii_mode)
{
    const effect_glyph* glyphs = (const effect_glyph*)effect_head.info_ptr;

    if (!glyphs)
        return;

    for (int i = 0; i < 256; i++)
    {
        byte attr = ascii_mode ? glyphs[i].d_attr : glyphs[i].x_attr;
        byte ch = ascii_mode ? glyphs[i].d_char : glyphs[i].x_char;

        misc_to_attr[i] = attr;
        misc_to_char[i] = (char)ch;
    }
}

void refresh_effect_visuals_for_graphics_mode(void)
{
    effect_visuals_apply(graphics_are_ascii());
}

errr init_effect_info(void)
{
    errr err;

    /* Init the header - 256 entries map directly onto misc_to_attr/char */
    init_header(&effect_head, 256, sizeof(effect_glyph));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    effect_head.parse_info_txt = parse_effect_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("effect", &effect_head);

    if (!err)
        refresh_effect_visuals_for_graphics_mode();

    return (err);
}

/*
 * Initialize skeleton note templates
 */
errr init_skeleton_note_info(void)
{
    errr err;

    if (z_info && z_info->skeleton_note_max <= 0)
    {
        log_warn("skeleton_note_max not set in limits.txt (or 0), defaulting to 420");
        z_info->skeleton_note_max = 420;
    }
    else
    {
        log_debug("skeleton_note_max initialized to %d", z_info->skeleton_note_max);
    }

    init_header(
        &skeleton_note_head, z_info->skeleton_note_max,
        sizeof(skeleton_note_template));

#ifdef ALLOW_TEMPLATES
    skeleton_note_head.parse_info_txt = parse_skeleton_note_info;
#endif /* ALLOW_TEMPLATES */

    err = init_info("skeleton_note", &skeleton_note_head);

    skeleton_note_info = (skeleton_note_template*)skeleton_note_head.info_ptr;
    skeleton_note_text = skeleton_note_head.text_ptr;

    return (err);
}

/*
 * Initialize the "quest_info" array
 */
errr init_quest_info(void)
{
    errr err;

    /* Init the header */
    init_header(&quest_head, z_info->quest_max, sizeof(quest_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    quest_head.parse_info_txt = parse_quest_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("quest", &quest_head);

    /* Set the global variables */
    quest_info = quest_head.info_ptr;
    quest_name_text = quest_head.name_ptr;
    quest_desc_text = quest_head.text_ptr;
    q_text = quest_head.text_ptr;

    return (err);
}

/*
 * Initialize the "oath_info" array
 */
errr init_oath_info(void)
{
    errr err;

    /* Init the header */
    init_header(&oath_head, z_info->oath_max, sizeof(oath_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    oath_head.parse_info_txt = parse_oath_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("oath", &oath_head);

    /* Set the global variables */
    oath_info = oath_head.info_ptr;
    oath_name_text = oath_head.name_ptr;
    oath_desc_text = oath_head.text_ptr;

    return (err);
}
