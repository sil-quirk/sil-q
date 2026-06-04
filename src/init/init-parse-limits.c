#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "init-parse-internal.h"
#include "init-object-bonuses.h"
#include <ctype.h>

#ifdef ALLOW_TEMPLATES
errr parse_z_info(char* buf, header* head)
{
    maxima* z_info = head->info_ptr;

    /* Hack - Verify 'M:x:' format */
    if (buf[0] != 'M')
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    if (!buf[2])
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    if (buf[3] != ':')
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);

    /* Process 'F' for "Maximum f_info[] index" */
    if (buf[2] == 'F')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->f_max = max;
    }

    /* Process 'K' for "Maximum k_info[] index" */
    else if (buf[2] == 'K')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->k_max = max;
    }

    /* Process 'B' for "Maximum b_info[] index" */
    else if (buf[2] == 'B')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->b_max = max;
    }

    /* Process 'A' for "Maximum a_info[] index" */
    else if (buf[2] == 'A')
    {
        int art_special_max, art_normal_max, art_random_max, art_self_made_max;

        /* Scan for the value */
        if (4
            != sscanf(buf + 4, "%d:%d:%d:%d", &art_special_max, &art_normal_max,
                &art_random_max, &art_self_made_max))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        z_info->art_spec_max = art_special_max;
        z_info->art_norm_max = art_normal_max + art_special_max;
        z_info->art_rand_max = z_info->art_norm_max + art_random_max;
        z_info->art_self_made_max = z_info->art_rand_max + art_self_made_max;

        /* Total artefacts */
        z_info->art_max = art_special_max + art_normal_max + art_random_max
            + art_self_made_max;
    }

    /* Process 'E' for "Maximum e_info[] index" */
    else if (buf[2] == 'E')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->e_max = max;
    }

    /* Process 'G' for "Maximum e_info[] index" */
    else if (buf[2] == 'G')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->ghost_other_max = max;
    }

    /* Process 'R' for "Maximum r_info[] index" */
    else if (buf[2] == 'R')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->r_max = max;
    }

    /* Process 'V' for "Maximum v_info[] index" */
    else if (buf[2] == 'V')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->v_max = max;
    }

    /* Process 'P' for "Maximum p_info[] index" */
    else if (buf[2] == 'P')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->p_max = max;
    }

    /* Process 'C' for "Maximum c_info[] index" */
    else if (buf[2] == 'C')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->c_max = max;
    }

    /* Process 'H' for "Maximum h_info[] index" */
    else if (buf[2] == 'H')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->h_max = max;
    }

    /* Process 'S' for "Maximum st_info[] index" */

    else if (buf[2] == 'S')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->st_max = max;
    }

    /* Process 'U' for "Maximum cu_info[] index" */

    else if (buf[2] == 'U')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->cu_max = max;
    }

    /* Process 'J' for "Maximum blessing info index" */
    else if (buf[2] == 'J')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        z_info->mb_max = max;
    }

    /* Process 'Q' for "Maximum q_info[] index" */
    else if (buf[2] == 'Q')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->quest_max = max;
    }

    /* Process 'W' for "Maximum oath_info[] subindex" */
    else if (buf[2] == 'W')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->oath_max = max;
    }

    /* Process 'L' for "Maximum flavor_info[] subindex" */
    else if (buf[2] == 'L')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->flavor_max = max;
    }

    /* Process 'O' for "Maximum o_list[] index" */
    else if (buf[2] == 'O')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->o_max = max;
    }

    /* Process 'N' for "Fake name size" */
    else if (buf[2] == 'N')
    {
        long max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%ld", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->fake_name_size = max;
    }

    /* Process 'T' for "Fake text size" */
    else if (buf[2] == 'T')
    {
        long max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%ld", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->fake_text_size = max;
    }
    /* M:R:<number_of_runtypes> ----------------------------------------- */
    else if (buf[2] == 'Y')
    {
        z_info->rt_max = (u16b)atoi(buf + 4);
    }
    /* M:Z:<number_of_styles> */
    else if (buf[2] == 'Z')
    {
        z_info->style_max = (u16b)atoi(buf + 4);
    }
    else if (buf[2] == 'X')
    {
        z_info->skeleton_note_max = (u16b)atoi(buf + 4);
        log_debug("Parsed skeleton_note_max (M:X): %d", z_info->skeleton_note_max);
    }
    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

#endif /* ALLOW_TEMPLATES */