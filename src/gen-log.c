/* gen-log.c - Implementation of dedicated generation logging */

#include "angband.h"
#include "gen-log.h"
#include "fs/path.h"
#include <time.h>
#include <string.h>

#if GENERATION_LOG_ENABLED

FILE *gen_log_file = NULL;
bool gen_log_initialized = false;
int gen_log_level_count = 0;

/* Per-level tracking */
static int current_depth = 0;
static int current_attempt = 0;
static time_t level_start_time = 0;
static int last_depth = -1;

/* Store exe path for delayed initialization */
static char stored_exe_path[1024] = {0};

static void gen_log_close_internal(void)
{
    if (gen_log_file)
    {
        fprintf(gen_log_file, "\n=== GENERATION LOG CLOSED ===\n");
        fflush(gen_log_file);
        fclose(gen_log_file);
        gen_log_file = NULL;
    }
    gen_log_initialized = false;
}

void gen_log_init(const char *exe_path)
{
    char log_path[1024];
    char parsed_path[1024];
    time_t now;
    struct tm *timeinfo;
    
    if (gen_log_initialized) return;
    
    /* Store exe_path for reference */
    if (exe_path)
    {
        strncpy(stored_exe_path, exe_path, sizeof(stored_exe_path) - 1);
        stored_exe_path[sizeof(stored_exe_path) - 1] = '\0';
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
                strcpy(log_path + i + 1, "generation.txt");
                break;
            }
        }
        
        /* If no separator found, use current directory */
        if (i < 0)
        {
            strcpy(log_path, "generation.txt");
        }
    }
    else
    {
        strcpy(log_path, "generation.txt");
    }
    
    /* Parse path and open file */
    if (!path_parse(parsed_path, sizeof(parsed_path), log_path))
    {
        /* Fallback to direct path if parse fails */
        strcpy(parsed_path, log_path);
    }
    
    gen_log_file = fopen(parsed_path, "w");
    if (!gen_log_file)
    {
        /* Silent failure - don't crash the game */
        return;
    }
    
    gen_log_initialized = true;
    gen_log_level_count = 0;
    
    /* Write header */
    now = time(NULL);
    timeinfo = localtime(&now);
    
    fprintf(gen_log_file, "================================================================================\n");
    fprintf(gen_log_file, "                    SIL-MORE GENERATION DEBUG LOG\n");
    fprintf(gen_log_file, "================================================================================\n");
    fprintf(gen_log_file, "Session started: %04d-%02d-%02d %02d:%02d:%02d\n",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    fprintf(gen_log_file, "Log path: %s\n", parsed_path);
    fprintf(gen_log_file, "================================================================================\n\n");
    
    fprintf(gen_log_file, "Legend:\n");
    fprintf(gen_log_file, "  [PARTITION] - Partition grid, modes (ROOMY/CAVEY/RUINED/LABYRINTH/CHASM/BIG_CAVE)\n");
    fprintf(gen_log_file, "  [ROOM]      - Room placement attempts and results\n");
    fprintf(gen_log_file, "  [ANCHOR]    - CA blobs, BSP slices, labyrinth carving, chasm platforms\n");
    fprintf(gen_log_file, "  [CONNECT]   - Tunnel building, partition connectivity\n");
    fprintf(gen_log_file, "  [QUEST]     - Quest lottery, spawning decisions, vault placement\n");
    fprintf(gen_log_file, "  [STAIRS]    - Stair placement and distance calculations\n");
    fprintf(gen_log_file, "  [FAIL]      - Regeneration triggers and failure reasons\n");
    fprintf(gen_log_file, "  [SUMMARY]   - Per-level generation summary\n");
    fprintf(gen_log_file, "  [MONSTER]   - Monster placement during generation\n");
    fprintf(gen_log_file, "\n");
    
    fflush(gen_log_file);
    
    /* Register cleanup */
    atexit(gen_log_close_internal);
}

void gen_log_close(void)
{
    gen_log_close_internal();
}

void gen_log_write(const char *category, const char *fmt, ...)
{
    va_list args;
    time_t now;
    struct tm *timeinfo;
    
    if (!gen_log_file) return;
    
    now = time(NULL);
    timeinfo = localtime(&now);
    
    /* Timestamp + category */
    fprintf(gen_log_file, "%02d:%02d:%02d [%-9s] ",
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
            category);
    
    /* Message */
    va_start(args, fmt);
    vfprintf(gen_log_file, fmt, args);
    va_end(args);
    
    fprintf(gen_log_file, "\n");
}

void gen_log_flush(void)
{
    if (gen_log_file)
    {
        fflush(gen_log_file);
    }
}

void gen_log_level_start(int depth, int map_hgt, int map_wid)
{
    time_t now;
    struct tm *timeinfo;
    int blocks_h = 0;
    int blocks_w = 0;
    
    if (!gen_log_file) return;
    
    /* Detect regeneration: same depth means another attempt */
    if (depth != last_depth)
    {
        gen_log_level_count++;
        current_attempt = 1;
    }
    else
    {
        current_attempt++;
    }
    current_depth = depth;
    last_depth = depth;

    /* Derive block counts for better readability in the log */
    if (PANEL_HGT > 0) blocks_h = map_hgt / PANEL_HGT;
    if (PANEL_HGT > 0) blocks_w = map_wid / PANEL_HGT; /* square maps use PANEL_HGT for both axes */
    if (blocks_h <= 0) blocks_h = 1;
    if (blocks_w <= 0) blocks_w = 1;

    level_start_time = time(NULL);
    now = level_start_time;
    timeinfo = localtime(&now);
    
    fprintf(gen_log_file, "\n");
    fprintf(gen_log_file, "################################################################################\n");
    fprintf(gen_log_file, "### LEVEL %d GENERATION START (level #%d this session, attempt #%d)\n",
            depth, gen_log_level_count, current_attempt);
    fprintf(gen_log_file, "### Time: %02d:%02d:%02d | Map size: %d x %d | Blocks: %d x %d (area %d)\n",
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
            map_hgt, map_wid, blocks_h, blocks_w, blocks_h * blocks_w);
    fprintf(gen_log_file, "################################################################################\n");
    fflush(gen_log_file);
}

void gen_log_level_end(bool success, int rooms, int attempts)
{
    time_t now;
    double elapsed;
    int attempt_count;
    
    if (!gen_log_file) return;
    (void)attempts;
    
    now = time(NULL);
    elapsed = difftime(now, level_start_time);
    attempt_count = current_attempt;
    
    fprintf(gen_log_file, "--------------------------------------------------------------------------------\n");
    if (success)
    {
        fprintf(gen_log_file, "### LEVEL %d COMPLETE: %d rooms, attempt #%d, %.1f seconds\n",
                current_depth, rooms, attempt_count, elapsed);
    }
    else
    {
        fprintf(gen_log_file, "### LEVEL %d FAILED on attempt #%d, %.1f seconds - REGENERATING\n",
                current_depth, attempt_count, elapsed);
    }
    fprintf(gen_log_file, "--------------------------------------------------------------------------------\n\n");
    fflush(gen_log_file);
}

#else /* GENERATION_LOG_ENABLED == 0 */

/* Stub implementations when logging is disabled */
FILE *gen_log_file = NULL;
bool gen_log_initialized = false;
int gen_log_level_count = 0;

void gen_log_init(const char *exe_path) { (void)exe_path; }
void gen_log_close(void) {}
void gen_log_write(const char *category, const char *fmt, ...) { (void)category; (void)fmt; }
void gen_log_flush(void) {}
void gen_log_level_start(int depth, int map_hgt, int map_wid) { (void)depth; (void)map_hgt; (void)map_wid; }
void gen_log_level_end(bool success, int rooms, int attempts) { (void)success; (void)rooms; (void)attempts; }

#endif /* GENERATION_LOG_ENABLED */
