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

/* Summary log (append across sessions) */
static FILE *gen_summary_file = NULL;
static char gen_summary_path[1024] = {0};

/* Per-level tracking */
static int current_depth = 0;
static int current_attempt = 0;
static time_t level_start_time = 0;
static int last_depth = -1;
static int current_map_hgt = 0;
static int current_map_wid = 0;
static char last_failure_reason[1024] = {0};

/* Store exe path for delayed initialization */
static char stored_exe_path[1024] = {0};

static bool gen_log_build_path(char *out, size_t out_sz, const char *exe_path, const char *filename)
{
    const char *last_back;
    const char *last_fwd;
    const char *last_sep;

    if (!out || out_sz == 0) return false;

    if (!exe_path || !exe_path[0])
    {
        strnfmt(out, out_sz, "%s", filename);
        return true;
    }

    last_back = strrchr(exe_path, '\\');
    last_fwd  = strrchr(exe_path, '/');
    last_sep  = last_back;
    if (!last_sep || (last_fwd && last_fwd > last_sep)) last_sep = last_fwd;

    if (!last_sep)
    {
        strnfmt(out, out_sz, "%s", filename);
        return true;
    }

    strnfmt(out, out_sz, "%.*s%s", (int)(last_sep - exe_path + 1), exe_path, filename);
    return true;
}

static void gen_log_close_internal(void)
{
    if (gen_log_file)
    {
        fprintf(gen_log_file, "\n=== GENERATION LOG CLOSED ===\n");
        fflush(gen_log_file);
        fclose(gen_log_file);
        gen_log_file = NULL;
    }
    if (gen_summary_file)
    {
        fprintf(gen_summary_file, "=== GENERATION SUMMARY LOG CLOSED ===\n");
        fflush(gen_summary_file);
        fclose(gen_summary_file);
        gen_summary_file = NULL;
    }
    gen_log_initialized = false;
}

void gen_log_init(const char *exe_path)
{
    char log_path[1024];
    char parsed_path[1024];
    char summary_path[1024];
    char parsed_summary_path[1024];
    char base_log_path[1024];
    char base_summary_path[1024];
    time_t now;
    struct tm *timeinfo;
    const char *sdl_base = NULL;
    bool use_sdl_base = false;
    
    if (gen_log_initialized) return;
    
    /* Store exe_path for reference */
    if (exe_path)
    {
        strncpy(stored_exe_path, exe_path, sizeof(stored_exe_path) - 1);
        stored_exe_path[sizeof(stored_exe_path) - 1] = '\0';
    }
    
    /* Build log file paths in the executable directory (prefer SDL base path). */
    sdl_base = SDL_GetBasePath();
    if (sdl_base && sdl_base[0])
    {
        use_sdl_base = path_build(base_log_path, sizeof(base_log_path), sdl_base, "generation.txt")
            && path_build(base_summary_path, sizeof(base_summary_path), sdl_base, "generation-summary.txt");
        if (use_sdl_base)
        {
            (void)strnfmt(log_path, sizeof(log_path), "%s", base_log_path);
            (void)strnfmt(summary_path, sizeof(summary_path), "%s", base_summary_path);
        }
    }
    if (!use_sdl_base)
    {
        (void)gen_log_build_path(log_path, sizeof(log_path), exe_path, "generation.txt");
        (void)gen_log_build_path(summary_path, sizeof(summary_path), exe_path, "generation-summary.txt");
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

    /* Open persistent summary log (append) */
    if (!path_parse(parsed_summary_path, sizeof(parsed_summary_path), summary_path))
    {
        strcpy(parsed_summary_path, summary_path);
    }
    gen_summary_file = fopen(parsed_summary_path, "a");
    if (gen_summary_file)
    {
        strncpy(gen_summary_path, parsed_summary_path, sizeof(gen_summary_path) - 1);
        gen_summary_path[sizeof(gen_summary_path) - 1] = '\0';
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

    /* Write summary header (append) */
    if (gen_summary_file)
    {
        fprintf(gen_summary_file, "\n");
        fprintf(gen_summary_file, "================================================================================\n");
        fprintf(gen_summary_file, "SIL-MORE GENERATION SUMMARY LOG (persistent)\n");
        fprintf(gen_summary_file, "Session started: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        fprintf(gen_summary_file, "Summary path: %s\n", gen_summary_path[0] ? gen_summary_path : parsed_summary_path);
        fprintf(gen_summary_file, "================================================================================\n");
        fflush(gen_summary_file);
    }
    
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
    char msg[2048];
    
    if (!gen_log_file) return;
    
    now = time(NULL);
    timeinfo = localtime(&now);

    va_start(args, fmt);
    (void)vstrnfmt(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    /* Timestamp + category */
    fprintf(gen_log_file, "%02d:%02d:%02d [%-9s] ",
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
            category);
    
    /* Message */
    fprintf(gen_log_file, "%s", msg);
    
    fprintf(gen_log_file, "\n");

    /* Track last failure reason for summary logging */
    if (category)
    {
        if (strcmp(category, "FAIL") == 0)
        {
            strncpy(last_failure_reason, msg, sizeof(last_failure_reason) - 1);
            last_failure_reason[sizeof(last_failure_reason) - 1] = '\0';
        }
        else if (strcmp(category, "QUEST") == 0)
        {
            if (strstr(msg, "SPAWN FAILED") || strstr(msg, "forcing regeneration"))
            {
                strncpy(last_failure_reason, msg, sizeof(last_failure_reason) - 1);
                last_failure_reason[sizeof(last_failure_reason) - 1] = '\0';
            }
        }
    }
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
    current_map_hgt = map_hgt;
    current_map_wid = map_wid;
    last_failure_reason[0] = '\0';

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
    struct tm *timeinfo;
    
    if (!gen_log_file) return;
    (void)attempts;
    
    now = time(NULL);
    timeinfo = localtime(&now);
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

    /* Persistent per-attempt summary */
    if (gen_summary_file && timeinfo)
    {
        const char *result = success ? "SUCCESS" : "FAIL";
        const char *reason = success ? "" : (last_failure_reason[0] ? last_failure_reason : "UNKNOWN");

        fprintf(gen_summary_file,
                "%04d-%02d-%02d %02d:%02d:%02d\tdepth=%d\tattempt=%d\tmap=%dx%d\trooms=%d\tresult=%s\telapsed=%.1fs\treason=%s\n",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                current_depth, attempt_count, current_map_hgt, current_map_wid, rooms,
                result, elapsed, reason);
        fflush(gen_summary_file);
    }
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
