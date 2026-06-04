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
errr parse_quest_info(char* buf, header* head)
{
    int i;
    char *s;

    /* Current entry */
    static quest_type* quest_ptr = NULL;

    /* Process 'N' for "New/Number/Name" or 'Q' for "Quest" */
    if (buf[0] == 'N' || buf[0] == 'Q')
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
        quest_ptr = (quest_type*)head->info_ptr + i;

        /* Store the name */
        if (!(quest_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Title text" */
    else if (buf[0] == 'T')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store title text in dedicated field */
        if (!add_text(&(quest_ptr->title_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'C' for "Challenge text" */
    else if (buf[0] == 'C')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store challenge text in dedicated field */
        if (!add_text(&(quest_ptr->challenge_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Y' for "quest tYpe" */
    else if (buf[0] == 'Y')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse quest type */
        quest_ptr->quest_type = atoi(buf + 2);
    }

    /* Process 'P' for "Parametric formula" */
    else if (buf[0] == 'P')
    {
        char formula_name[32];
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse formula: P:FORMULA_TYPE:param1:param2:param3:param4:depth_min:depth_max */
        /* Example: P:LINEAR_DECAY:27:0:0:0:6:19 */
        /*          P:SCALED_RANGE:0.125:14:5:0:14:25 */
        
        /* Initialize to hardcoded by default */
        quest_ptr->formula_type = FORMULA_HARDCODED;
        quest_ptr->formula_params[0] = 0.0f;
        quest_ptr->formula_params[1] = 0.0f;
        quest_ptr->formula_params[2] = 0.0f;
        quest_ptr->formula_params[3] = 0.0f;
        quest_ptr->depth_min = 0;
        quest_ptr->depth_max = 25;
        
        /* Parse formula name */
        if (sscanf(buf + 2, "%31[^:]", formula_name) == 1) {
            char *rest = strchr(buf + 2, ':');
            if (rest) {
                rest++; /* Skip the ':' */
                
                /* Determine formula type */
                if (SDL_strcasecmp(formula_name, "LINEAR_DECAY") == 0) {
                    quest_ptr->formula_type = FORMULA_LINEAR_DECAY;
                    /* Parse: base:unused:unused:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                } else if (SDL_strcasecmp(formula_name, "SCALED_RANGE") == 0) {
                    quest_ptr->formula_type = FORMULA_SCALED_RANGE;
                    /* Parse: max_prob:start_depth:range:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                } else if (SDL_strcasecmp(formula_name, "LINEAR_INTERPOLATE") == 0) {
                    quest_ptr->formula_type = FORMULA_LINEAR_INTERPOLATE;
                    /* Parse: min_prob:max_prob:unused:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                } else if (SDL_strcasecmp(formula_name, "FIXED_PERCENT") == 0) {
                    quest_ptr->formula_type = FORMULA_FIXED_PERCENT;
                    /* Parse: percentage:unused:unused:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                }
                /* Add more formula types here as needed */
            }
        }
    }

    /* Process 'O' for "Oath" */
    else if (buf[0] == 'O')
    {
        int oath_id;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse oath ID directly as integer */
        oath_id = atoi(buf + 2);
        
        /* Validate oath ID range dynamically using z_info->oath_max */
        if (oath_id < 0 || !z_info || oath_id >= z_info->oath_max) {
            oath_id = 0; /* Default to no oath */
        }
        
        /* Store the oath ID */
        quest_ptr->oath_id = oath_id;
    }

    /* Process 'E' for "Eligibility requirements" */
    else if (buf[0] == 'E')
    {
        char requirement_type[32];
        int value1, value2, value3;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        log_trace("QUEST PARSE: Processing E: field for quest %d: '%s'", quest_ptr->quest_num, buf);
        log_trace("QUEST PARSE: buf+2 = '%s'", buf + 2);

        /* Initialize eligibility fields */
        quest_ptr->eligibility_type = 0; /* Default: no requirements */
        quest_ptr->eligibility_skill = 0;
        quest_ptr->eligibility_value = 0;
        quest_ptr->eligibility_depth_min = 0;
        quest_ptr->eligibility_depth_max = 0;

        /* Parse eligibility requirements */
        /* Try parsing with skill name first: E:SKILL_MIN:SMT:10 */
        char skill_name[32];
        
        /* Debug: test the sscanf pattern */
        char test_input[] = "SKILL_MIN:SMT:10";
        char test_skill[32];
        int test_value;
        int test_result = sscanf(test_input, "SKILL_MIN:%31[^:]:%d", test_skill, &test_value);
        log_trace("QUEST PARSE: sscanf test on '%s' returned %d, skill='%s', value=%d", 
                 test_input, test_result, test_skill, test_value);
        
        if (2 == sscanf(buf + 2, "SKILL_MIN:%31[^:]:%d", skill_name, &value1))
        {
            quest_ptr->eligibility_type = 1; /* skill_min */
            
            log_trace("QUEST PARSE: Successfully parsed SKILL_MIN: skill='%s', value=%d", skill_name, value1);
            
            /* Map skill names to skill types */
            if (SDL_strcasecmp(skill_name, "MEL") == 0) {
                quest_ptr->eligibility_skill = S_MEL;
            } else if (SDL_strcasecmp(skill_name, "ARC") == 0) {
                quest_ptr->eligibility_skill = S_ARC;
            } else if (SDL_strcasecmp(skill_name, "EVN") == 0) {
                quest_ptr->eligibility_skill = S_EVN;
            } else if (SDL_strcasecmp(skill_name, "STL") == 0) {
                quest_ptr->eligibility_skill = S_STL;
            } else if (SDL_strcasecmp(skill_name, "PER") == 0) {
                quest_ptr->eligibility_skill = S_PER;
            } else if (SDL_strcasecmp(skill_name, "WIL") == 0) {
                quest_ptr->eligibility_skill = S_WIL;
            } else if (SDL_strcasecmp(skill_name, "SMT") == 0) {
                quest_ptr->eligibility_skill = S_SMT;
            } else if (SDL_strcasecmp(skill_name, "SNG") == 0) {
                quest_ptr->eligibility_skill = S_SNG;
            } else {
                quest_ptr->eligibility_skill = S_MEL; /* Default to Melee */
            }
            
            quest_ptr->eligibility_value = value1; /* minimum value */
            
            log_trace("QUEST PARSE: Set eligibility_type=1, eligibility_skill=%d, eligibility_value=%d", 
                     quest_ptr->eligibility_skill, quest_ptr->eligibility_value);
        }
        /* Fall back to numeric parsing */
        else if (3 == sscanf(buf + 2, "%31[^:]:%d:%d", requirement_type, &value1, &value2))
        {
            log_trace("QUEST PARSE: Trying numeric parsing: type='%s', value1=%d, value2=%d", requirement_type, value1, value2);
            
            /* Format: E:requirement_type:value1:value2 */
            if (streq(requirement_type, "SKILL_MIN"))
            {
                quest_ptr->eligibility_type = 1; /* skill_min */
                quest_ptr->eligibility_skill = value1; /* skill type (S_SMT, etc.) */
                quest_ptr->eligibility_value = value2; /* minimum value */
                
                log_trace("QUEST PARSE: Numeric SKILL_MIN - eligibility_type=1, eligibility_skill=%d, eligibility_value=%d", 
                         quest_ptr->eligibility_skill, quest_ptr->eligibility_value);
            }
            else if (streq(requirement_type, "DEPTH_RANGE"))
            {
                quest_ptr->eligibility_type = 3; /* depth_range */
                quest_ptr->eligibility_depth_min = value1; /* minimum depth */
                quest_ptr->eligibility_depth_max = value2; /* maximum depth */
                /* Also set for P: field formula calculations */
                quest_ptr->depth_min = value1;
                quest_ptr->depth_max = value2;
            }
        }
        else if (4 == sscanf(buf + 2, "%31[^:]:%d:%d:%d", requirement_type, &value1, &value2, &value3))
        {
            /* Format: E:requirement_type:value1:value2:value3 */
            if (streq(requirement_type, "SKILL_RANGE"))
            {
                quest_ptr->eligibility_type = 2; /* skill_range */
                quest_ptr->eligibility_skill = value1; /* skill type */
                quest_ptr->eligibility_depth_min = value2; /* min depth */
                quest_ptr->eligibility_depth_max = value3; /* max depth */
                /* Also set for P: field formula calculations */
                quest_ptr->depth_min = value2;
                quest_ptr->depth_max = value3;
            }
        }
        else
        {
            log_trace("QUEST PARSE: Failed to parse E: field '%s' - leaving as NO_REQUIREMENTS", buf);
        }
    }

    /* Process 'Y' for "Yarn/story" */
    else if (buf[0] == 'Y')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Add story text to description */
        if (!add_text(&(quest_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'A' for "Ability reward" */
    else if (buf[0] == 'A')
    {
        int ability_type, ability_id;

        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &ability_type, &ability_id))
            return (PARSE_ERROR_GENERIC);

        /* Save the values in the new ability fields */
        quest_ptr->ability_type = ability_type;
        quest_ptr->ability_id = ability_id;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the text */
        if (!add_text(&(quest_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stat bonuses" */
    else if (buf[0] == 'S')
    {
        int str_bonus, dex_bonus, con_bonus, gra_bonus;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse stat bonuses: S:str:dex:con:gra */
        if (4 == sscanf(buf + 2, "%d:%d:%d:%d", &str_bonus, &dex_bonus, &con_bonus, &gra_bonus))
        {
            quest_ptr->stat_bonuses[0] = str_bonus;
            quest_ptr->stat_bonuses[1] = dex_bonus;
            quest_ptr->stat_bonuses[2] = con_bonus;
            quest_ptr->stat_bonuses[3] = gra_bonus;
        }
    }

    /* Process 'K' for "sKill bonuses" */
    else if (buf[0] == 'K')
    {
        char skill_name[32];
        int skill_bonus;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse skill bonuses: K:skill:bonus */
        if (2 == sscanf(buf + 2, "%31[^:]:%d", skill_name, &skill_bonus))
        {
            /* Map skill names to skill types using proper constants */
            if (SDL_strcasecmp(skill_name, "MEL") == 0) {
                quest_ptr->skill_type = S_MEL; /* Melee (0) */
            } else if (SDL_strcasecmp(skill_name, "ARC") == 0) {
                quest_ptr->skill_type = S_ARC; /* Archery (1) */
            } else if (SDL_strcasecmp(skill_name, "EVN") == 0) {
                quest_ptr->skill_type = S_EVN; /* Evasion (2) */
            } else if (SDL_strcasecmp(skill_name, "STL") == 0) {
                quest_ptr->skill_type = S_STL; /* Stealth (3) */
            } else if (SDL_strcasecmp(skill_name, "PER") == 0) {
                quest_ptr->skill_type = S_PER; /* Perception (4) */
            } else if (SDL_strcasecmp(skill_name, "WIL") == 0) {
                quest_ptr->skill_type = S_WIL; /* Will (5) */
            } else if (SDL_strcasecmp(skill_name, "SMT") == 0) {
                quest_ptr->skill_type = S_SMT; /* Smithing (6) */
            } else if (SDL_strcasecmp(skill_name, "SNG") == 0) {
                quest_ptr->skill_type = S_SNG; /* Song (7) */
            } else {
                quest_ptr->skill_type = 0; /* Default to Melee if unknown */
            }
            quest_ptr->skill_bonus = skill_bonus;
        }
    }

    /* Process 'I' for "Initialization text" */
    else if (buf[0] == 'I')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the initialization text in dedicated field with newline separator */
        if (quest_ptr->init_text != 0) {
            /* Add newline separator if not first line */
            if (!add_text(&(quest_ptr->init_text), head, "\n"))
                return (PARSE_ERROR_OUT_OF_MEMORY);
        }
        
        /* Store the text (buf + 2 points after "I:") */
        if (!add_text(&(quest_ptr->init_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'W' for "Win/completion text" */
    else if (buf[0] == 'W')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the completion text in dedicated field with newline separator */
        if (quest_ptr->completion_text != 0) {
            /* Add newline separator if not first line */
            if (!add_text(&(quest_ptr->completion_text), head, "\n"))
                return (PARSE_ERROR_OUT_OF_MEMORY);
        }
        
        /* Store the completion text (buf + 2 points after "W:") */
        if (!add_text(&(quest_ptr->completion_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'R' for "vaRiable name" (quest state mapping) */
    else if (buf[0] == 'R')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the quest state variable name */
        if (!(quest_ptr->quest_state_var = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'M' for "Metarun quest ID" */
    else if (buf[0] == 'M')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the metarun quest ID string */
        if (!(quest_ptr->metarun_quest_id = add_name(head, buf + 2)))
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