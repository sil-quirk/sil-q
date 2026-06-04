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
errr parse_oath_info(char* buf, header* head)
{
    int i;
    char *s;

    /* Current entry */
    static oath_type* oath_ptr = NULL;

    /* Process 'N' for "New/Number/Name" or 'O' for "Oath" */
    if (buf[0] == 'N' || buf[0] == 'O')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        oath_ptr = (oath_type*)head->info_ptr + i;

        /* Initialize the new fields */
    oath_ptr->oath_num = i;
        oath_ptr->stat_bonuses[0] = 0;
        oath_ptr->stat_bonuses[1] = 0;
        oath_ptr->stat_bonuses[2] = 0;
        oath_ptr->stat_bonuses[3] = 0;
        oath_ptr->skill_type = 0;
        oath_ptr->skill_bonus = 0;

        /* Store the name */
        if (!(oath_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Type info" or "Title text" */
    else if (buf[0] == 'T')
    {
        int oath_num, difficulty;

        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Try to scan for numeric values first (Type info) */
        if (2 == sscanf(buf + 2, "%d:%d", &oath_num, &difficulty))
        {
            /* Save the values */
            oath_ptr->oath_num = oath_num;
            oath_ptr->difficulty = difficulty;
        }
        else
        {
            /* Ignore title text for now - not stored in oath structure */
        }
    }

    /* Process 'R' for "Reward description" */
    else if (buf[0] == 'R')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store reward text using add_name for name buffer */
        if (!(oath_ptr->reward_text = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'A' for "Ability reward" */
    else if (buf[0] == 'A')
    {
        int reward_type, reward_value;

        /* There better be a current o_ptr */
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &reward_type, &reward_value))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        oath_ptr->reward_type = reward_type;
        oath_ptr->reward_value = reward_value;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the text */
        if (!add_text(&(oath_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'P' for "Pledge text" */
    else if (buf[0] == 'P')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store pledge text using add_name for name buffer */
        if (!(oath_ptr->pledge_text = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'F' for "Forbidden action text" */
    else if (buf[0] == 'F')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store forbidden action text using add_name for name buffer */
        if (!(oath_ptr->forbidden_text = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stat bonuses" */
    else if (buf[0] == 'S')
    {
        int str, dex, con, gra;

        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse stat bonuses and store them */
        /* Format: S:str:dex:con:gra */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &str, &dex, &con, &gra))
        {
            return (PARSE_ERROR_GENERIC);
        }

        /* Store the stat bonuses */
        oath_ptr->stat_bonuses[0] = str;
        oath_ptr->stat_bonuses[1] = dex;
        oath_ptr->stat_bonuses[2] = con;
        oath_ptr->stat_bonuses[3] = gra;
    }

    /* Process 'K' for "sKill bonuses" */
    else if (buf[0] == 'K')
    {
        int skill_type, skill_bonus;

        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse skill bonuses and store them */
        /* Format: K:skill:bonus */
        if (2 != sscanf(buf + 2, "%d:%d", &skill_type, &skill_bonus))
        {
            return (PARSE_ERROR_GENERIC);
        }

        /* Store the skill bonuses */
        oath_ptr->skill_type = skill_type;
        oath_ptr->skill_bonus = skill_bonus;
    }

    /* Process 'B' for "Behavioral restrictions" */
    else if (buf[0] == 'B')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Ignore behavioral restriction text for now - not essential for oath selection */
    }

    /* Process 'U' for "Unlock conditions" */
    else if (buf[0] == 'U')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Ignore unlock condition text for now - not essential for oath selection */
    }

    /* Process 'C' for "Confirmation prompt" */
    else if (buf[0] == 'C')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store confirmation prompt text using add_name for name buffer */
        if (!(oath_ptr->confirmation_prompt = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'M' for "Curse Message" */
    else if (buf[0] == 'M')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store curse message text using add_name for name buffer */
        if (!(oath_ptr->curse_message = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'E' for "pErmanent consequence message" */
    else if (buf[0] == 'E')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store permanent consequence message text using add_name for name buffer */
        if (!(oath_ptr->permanent_message = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Q' for "Death/escape message" */
    else if (buf[0] == 'Q')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store death/escape message text using add_name for name buffer */
        if (!(oath_ptr->death_message = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Z' for "Banned text (birth screen)" */
    else if (buf[0] == 'Z')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store banned text using add_text to allow multiple lines */
        if (!add_text(&(oath_ptr->banned_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
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