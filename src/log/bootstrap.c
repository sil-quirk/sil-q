/* log/bootstrap.c - Logger initialization */

#include "../angband.h"
#include "bootstrap.h"
#include "../fs/path.h"
#include "log.h"
#include <time.h>

/*
 * Initialises logger. Opens `log.txt` file and sets log level for stdout and
 * file from `SIL_LOG_LEVEL` environment variable. The `quiet` argument disables
 * stdout when set to true (essential for terminal modes like ncurses where
 * screen output would be garbled otherwise).
 */
void init_logger(bool quiet, const char* exe_path)
{
    const char* log_level_str = getenv("SIL_LOG_LEVEL");
    int level = LOG_TRACE; /* Default to DEBUG level */
    char log_path[1024];
    
    if (log_level_str && strlen(log_level_str) > 0)
    {
        for (level = LOG_TRACE; level <= LOG_FATAL; level++)
        {
            if (strcmp(log_level_str, log_level_string(level)) == 0)
            {
                break;
            }
        }
        if (level > LOG_FATAL)
        {
            level = LOG_INFO;
            log_warn("Unknown log level %s, log level will be set to INFO",
                log_level_str);
        }
    }

    /* Build log file path in same directory as executable */
    if (exe_path)
    {
        int i;
        strcpy(log_path, exe_path);
        
        /* Find the last directory separator */
        for (i = strlen(log_path) - 1; i >= 0; i--)
        {
            if (log_path[i] == '\\' || log_path[i] == '/')
            {
#if LOG_MODE_TIMESTAMP
                /* Create timestamped log filename */
                time_t now = time(NULL);
                struct tm *timeinfo = localtime(&now);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
                sprintf(log_path + i + 1, "log_%s.txt", timestamp);
#else
                /* Replace everything after the separator with "log.txt" */
                strcpy(log_path + i + 1, "log.txt");
#endif
                break;
            }
        }
        
        /* If no separator found, use appropriate filename in current directory */
        if (i < 0)
        {
#if LOG_MODE_TIMESTAMP
            time_t now = time(NULL);
            struct tm *timeinfo = localtime(&now);
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
            sprintf(log_path, "log_%s.txt", timestamp);
#else
            strcpy(log_path, "log.txt");
#endif
        }
    }
    else
    {
        /* Fallback to current directory if exe_path not available */
#if LOG_MODE_TIMESTAMP
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
        sprintf(log_path, "log_%s.txt", timestamp);
#else
        strcpy(log_path, "log.txt");
#endif
    }

    /* Note: The log library uses standard FILE* for logging */
    char parsed_path[1024];
    if (!path_parse(parsed_path, sizeof(parsed_path), log_path))
        quit("could not parse log path");
    
    FILE* log_file = fopen(parsed_path, "w");
    if (!log_file)
        quit("could not open log.txt for writing");
    log_add_fp(log_file, level);
    log_set_level(level);

    if (quiet)
    {
        log_set_quiet(true);
    }

    log_info("logger initialised with level %d", level);
    atexit(log_close_files);
}
