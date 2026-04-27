/* --------------------------------------------------------------------
 *  src/metarun.c   (2025-07-06)   - final, crash-free, warning-free
 * --------------------------------------------------------------------
 *  Tracks a "meta-run" that ends after 15 Silmarils (win) or
 *  15 deaths (lose).  Finished runs are appended to meta.raw so
 *  the entire history is preserved.  Includes:
 *     - list_metaruns()  - compact history view
 *     - print_metarun_stats() - details for current run
 * -------------------------------------------------------------------- */

#ifndef WINDOWS
#define _DEFAULT_SOURCE  /* For DT_DIR and other POSIX extensions */
#define _BSD_SOURCE      /* For setregid on older systems */
#endif

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "metarun_legacy.h"
#include "sdl-sound.h"
#include "h-define.h"
#include "platform.h"    /* MKDIR helper                      */
#include "supplies.h"
#include <SDL3/SDL.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#ifdef WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif  

/* --------------------------------------------------------------- */
/*  metarun.c : quick-and-dirty logger                             */
/* --------------------------------------------------------------- */

/* Enable this to delete old save/score files on fresh metarun start.
 * Currently disabled to prevent accidental data loss during debugging. */
/* #define METARUN_CLEANUP_OLD_FILES */

/* =========================  constants  ========================= */
#define CURSE_MENU_LINES  3

/* =========================  globals  =========================== */
static metarun *metaruns    = NULL;
static s16b     metarun_max = 0;
static s16b     current_run = 0;
bool            metarun_created = false;

static int popcount32(u32b value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);
        count++;
    }
    return count;
}

static void metarun_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

/* ----------------------- accessors --------------------------- */
const metarun *metarun_current(void)
{
    if (!metaruns) return NULL;
    if (current_run < 0 || current_run >= metarun_max) return NULL;
    return &metaruns[current_run];
}

metarun *metarun_current_mutable(void)
{
    if (!metaruns) return NULL;
    if (current_run < 0 || current_run >= metarun_max) return NULL;
    return &metaruns[current_run];
}

const metarun *metarun_entry_const(s16b idx)
{
    if (!metaruns) return NULL;
    if (idx < 0 || idx >= metarun_max) return NULL;
    return &metaruns[idx];
}

metarun *metarun_entry_mutable(s16b idx)
{
    if (!metaruns) return NULL;
    if (idx < 0 || idx >= metarun_max) return NULL;
    return &metaruns[idx];
}

s16b metarun_current_index(void)
{
    if (!metaruns) return -1;
    if (current_run < 0 || current_run >= metarun_max) return -1;
    return current_run;
}

s16b metarun_entry_count(void)
{
    return metarun_max;
}

int metarun_completed_count(void)
{
    int completed = 0;

    if (!metaruns)
        return 0;

    for (s16b i = 0; i < metarun_max; i++) {
        const metarun *m = &metaruns[i];
        int win_goal = WINCON_SILMARILS;

        if (runtype_info && z_info && m->type < z_info->rt_max) {
            win_goal = runtype_info[m->type].win_con
                ? runtype_info[m->type].win_con
                : WINCON_SILMARILS;
        }

        if (m->silmarils >= win_goal)
            completed++;
    }

    return completed;
}

static int parse_character_file(SDL_IOStream *fp)
{
    int count = 0;
    char line[1024];

    while (sdl_fgets(fp, line, sizeof(line)) == 0) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') continue;
        if ((p[0] == 'N') && (p[1] == ':')) count++;
    }

    return count;
}

static int count_character_txt_entries(void)
{
    static int cached_total = -1;
    if (cached_total >= 0) return cached_total;

    const struct {
        cptr dir;
        cptr filename;
    } candidates[] = {
        { ANGBAND_DIR_SAVE, "character.txt" },
        { ANGBAND_DIR_USER, "character.txt" },
        { ANGBAND_DIR_APEX, "character.txt" },
        { ANGBAND_DIR_DATA, "character.txt" },
        { ANGBAND_DIR_EDIT, "character.txt" },
        { NULL, NULL }
    };

    char path[1024];
    SDL_IOStream *fp = NULL;

    for (size_t i = 0; candidates[i].dir; i++) {
        if (!candidates[i].dir || !*candidates[i].dir) continue;
        if (!path_build(path, sizeof(path), candidates[i].dir, candidates[i].filename))
        {
            log_error("count_character_txt_entries: failed to build path for %s/%s",
                candidates[i].dir ? candidates[i].dir : "(null)",
                candidates[i].filename ? candidates[i].filename : "(null)");
            continue;
        }
        log_debug("count_character_txt_entries: trying %s", path);
        fp = sdl_fopen(path, "r");
        if (fp) {
            cached_total = parse_character_file(fp);
            sdl_fclose(fp);
            log_debug("count_character_txt_entries: loaded %d entries from %s", cached_total, path);
            break;
        }
    }

    if (fp == NULL) {
        log_debug("count_character_txt_entries: no character.txt found in known locations");
        cached_total = 0;
    }

    return cached_total;
}

static u32b runtype_threshold_for_mode(int runtype_id, metarun_blessing_threshold_mode mode);
static u32b metarun_threshold_value(const metarun *m);
static const char *threshold_mode_name(metarun_blessing_threshold_mode mode);

/* Clamp blessing economy values after (re)computing the ledger */
static void update_blessing_ledger(metarun *m)
{
    if (!m) return;

    /* Get blessing point threshold from runtype data */
    u32b threshold = metarun_threshold_value(m);
    if (threshold == 0) threshold = 1;

    u32b total = m->fallen_score_total;
    u32b earned = total / threshold;
    u32b remainder = total % threshold;

    if (earned > (u32b)SHRT_MAX) {
        earned = (u32b)SHRT_MAX;
    }

    m->blessing_points = (s16b)earned;
    m->fallen_score_pool = remainder;
}

/* Rebuild blessing_points from fallen_score_total.
 * Does NOT clamp blessing_points_spent - that value must be preserved from save.
 * Clamping of spent vs earned happens only at spend-time, not at load-time. */
void metarun_sanitize_blessing_economy(metarun *m)
{
    if (!m) return;

    /* Rebuild blessing_points from fallen_score_total */
    update_blessing_ledger(m);

    /* blessing_points CAN be negative - that's valid
     * blessing_points_spent is preserved exactly as loaded */
}

void metarun_clear_blessing_runtime_fields(metarun *m)
{
    if (!m) return;

    m->fallen_score_total = 0;
    m->fallen_score_pool = 0;
    m->blessing_points = 0;
    m->blessing_points_spent = 0;
    m->major_blessings = 0;
    m->alive_characters = 0;
    
    /* Clear pending blessing choices */
    m->pending_blessing_count = 0;
    for (int i = 0; i < 3; i++) {
        m->pending_blessing_choices[i] = 255;
    }
    
    metarun_set_threshold_mode(m, METARUN_BLESSING_THRESHOLD_NORMAL);
    memset(m->reserved_runtime, 0, sizeof(m->reserved_runtime));
}

static int major_blessing_capacity(void)
{
    if (!z_info) return 0;
    int cap = (int)z_info->mb_max;
    if (cap < 0) cap = 0;
    if (cap > 16) cap = 16; /* stored in u16 bitmask */
    return cap;
}

static u16b major_blessing_mask(void)
{
    int cap = major_blessing_capacity();
    if (cap <= 0) return 0;
    if (cap >= 16) return 0xFFFFu;
    return (u16b)((1u << cap) - 1u);
}

void metarun_sanitize_major_blessing_bits(metarun *m)
{
    if (!m || !z_info || !mb_info) return;

    u16b mask = major_blessing_mask();
    if (mask == 0) {
        m->major_blessings = 0;
        return;
    }

    u16b defined_mask = 0;
    int cap = major_blessing_capacity();
    for (int i = 0; i < cap; i++) {
        const major_blessing_type *def = &mb_info[i];
        if (def->name)
            defined_mask |= (1U << i);
    }

    if (defined_mask == 0) {
        m->major_blessings = 0;
        return;
    }

    m->major_blessings &= (mask & defined_mask);
}

static const major_blessing_type *major_blessing_def(int idx)
{
    if (!mb_info || !z_info) return NULL;
    if (idx < 0 || idx >= (int)z_info->mb_max) return NULL;
    const major_blessing_type *def = &mb_info[idx];
    if (!def->name) return NULL;
    return def;
}

static cptr major_blessing_name_str(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_name || !def->name) return "(unknown)";
    return mb_name + def->name;
}

static cptr major_blessing_short_desc(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->short_desc) return NULL;
    return mb_text + def->short_desc;
}

static cptr major_blessing_detail_desc(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->detail_desc) return NULL;
    return mb_text + def->detail_desc;
}

static cptr major_blessing_unlock_msg(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->unlock_msg) return NULL;
    return mb_text + def->unlock_msg;
}

static int major_blessing_cost(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def) return 0;
    if (def->cost == 0) return 3;
    return def->cost;
}

static metarun_major_effect major_blessing_effect(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def) return METARUN_MAJOR_EFFECT_NONE;
    return (metarun_major_effect)def->effect;
}

static void build_symbol_bar(char *out, size_t out_len, int current, int maximum, char filled)
{
    if (!out || out_len == 0) return;
    if (maximum <= 0) {
        strnfmt(out, out_len, "[]");
        return;
    }

    const int MAX_BAR_SLOTS = 20;
    int slots = maximum;
    if (slots > MAX_BAR_SLOTS) slots = MAX_BAR_SLOTS;
    if (slots < 1) slots = 1;

    char buffer[MAX_BAR_SLOTS + 1];
    for (int i = 0; i < slots; i++) {
        buffer[i] = (i < current) ? filled : '.';
    }
    buffer[slots] = '\0';

    strnfmt(out, out_len, "[%s]", buffer);
}

static void build_death_marks(char *out, size_t out_len, int deaths)
{
    if (!out || out_len == 0) return;
    if (deaths <= 0) {
        strnfmt(out, out_len, "none");
        return;
    }

    int max_marks = (int)out_len - 1;
    if (max_marks <= 0) {
        if (out_len > 0) out[0] = '\0';
        return;
    }

    if (deaths <= max_marks) {
        for (int i = 0; i < deaths; i++) out[i] = 'x';
        out[deaths] = '\0';
    } else {
        int marks = max_marks - 1;
        if (marks < 0) marks = 0;
        for (int i = 0; i < marks; i++) out[i] = 'x';
        out[marks] = '+';
        out[marks + 1] = '\0';
    }
}

static void refresh_alive_cache(void)
{
    int alive_scores = score_count_alive_entries();
    if (alive_scores < 0) alive_scores = 0;

    int roster_total = count_character_txt_entries();
    int alive_from_roster = roster_total - (int)metar.deaths;
    if (alive_from_roster < 0) {
        log_warn("refresh_alive_cache: metar.deaths=%d exceeds roster_total=%d", metar.deaths, roster_total);
        alive_from_roster = 0;
    }

    int alive = MAX(alive_scores, alive_from_roster);

    if (character_generated && p_ptr && !p_ptr->is_dead) {
        if (alive < 1) alive = 1;
    }

    if (alive > 255) alive = 255;
    metar.alive_characters = (byte)alive;

    log_debug("refresh_alive_cache: roster=%d deaths=%d scoreboard=%d final=%d",
              roster_total, metar.deaths, alive_scores, alive);
}

static u32b runtype_threshold_for_mode(int runtype_id, metarun_blessing_threshold_mode mode)
{
    u32b fallback = METARUN_BLESSING_POINT_THRESHOLD;

    if (!runtype_info || !z_info) return fallback;
    if (runtype_id < 0 || runtype_id >= z_info->rt_max) return fallback;

    runtype_type *rt = &runtype_info[runtype_id];

    int idx = (int)mode;
    if (idx < 0 || idx >= RUNTYPE_BLESSING_MODE_COUNT) idx = RUNTYPE_BLESSING_MODE_NORMAL;

    u16b val = rt->blessing_threshold_modes[idx];
    if (!val && idx != RUNTYPE_BLESSING_MODE_NORMAL) {
        val = rt->blessing_threshold_modes[RUNTYPE_BLESSING_MODE_NORMAL];
    }
    if (!val) val = METARUN_BLESSING_POINT_THRESHOLD;

    return (u32b)val;
}

static u32b metarun_threshold_value(const metarun *m)
{
    if (!m) return METARUN_BLESSING_POINT_THRESHOLD;
    return runtype_threshold_for_mode(m->type, metarun_get_threshold_mode(m));
}

static const char *threshold_mode_name(metarun_blessing_threshold_mode mode)
{
    switch (mode) {
        case METARUN_BLESSING_THRESHOLD_EASIER: return "Easier";
        case METARUN_BLESSING_THRESHOLD_HARDER: return "Harder";
        default: return "Normal";
    }
}

static u32b get_best_run_score_from_highscores(void)
{
    #define MAX_SCORES 100
    high_score scores[MAX_SCORES];
    int count = collect_high_scores(scores, MAX_SCORES, true);
    u32b best = 0;
    
    for (int i = 0; i < count; i++) {
        int pts = score_points(&scores[i]);
        if (pts > 0 && (u32b)pts > best) {
            best = (u32b)pts;
        }
    }
    
    #undef MAX_SCORES
    return best;
}

/* Calculate progressive diminishing score across all character runs
 * Formula: best/1 + second/2 + third/4 + fourth/8 + fifth/16 + ...
 * Rewards consistency while still heavily weighting best performance.
 * Caps at top 16 runs to prevent overflow and keep calculation fast.
 * Returns aggregate score contribution from character performance. */
static u32b compute_progressive_character_score(void)
{
    #define MAX_SCORES 100
    high_score scores[MAX_SCORES];
    int count = collect_high_scores(scores, MAX_SCORES, true); /* sorted by score descending */
    
    unsigned long long total = 0;
    unsigned long long divisor = 1;
    
    /* Process top 16 runs with progressive halving */
    for (int i = 0; i < count && i < 16; i++) {
        int pts = score_points(&scores[i]);
        if (pts > 0) {
            total += (unsigned long long)pts / divisor;
            divisor *= 2;  /* Each subsequent run worth half the previous */
        }
    }
    
    /* Clamp to u32b range */
    if (total > UINT32_MAX) return UINT32_MAX;
    
    log_debug("compute_progressive_character_score: processed %d runs, total=%u", 
              (count < 16 ? count : 16), (u32b)total);
    
    #undef MAX_SCORES
    return (u32b)total;
}

static u32b compute_metarun_score(const metarun *m)
{
    if (!m) return 0;

    /* Use progressive scoring across all character runs (v0.9.0.2+) */
    u32b progressive_score = compute_progressive_character_score();
    
    int quest_count = metarun_total_quest_completions(m);
    
    s32b total = (s32b)progressive_score;
    total += (s32b)m->silmarils * 120;
    total -= (s32b)m->deaths * 60;
    total += (s32b)60 * quest_count;
    total -= (s32b)100 * popcount32(m->banned_oaths);

    log_debug("compute_metarun_score: progressive=%u, sils=%d, deaths=%d, quest_count=%d (0x%08X), banned=%d => total=%d",
              progressive_score, m->silmarils, m->deaths, quest_count, m->completed_quests, 
              popcount32(m->banned_oaths), total);

    if (total < 0) total = 0;
    return (u32b)total;
}

void refresh_current_metar_score(void)
{
    if (!metaruns) return;
    if (current_run < 0 || current_run >= metarun_max) return;

    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
}

static int compare_metarun_indices(const void *a, const void *b)
{
    const s16b ia = *(const s16b *)a;
    const s16b ib = *(const s16b *)b;

    if (!metaruns) return 0;

    const metarun *ma = &metaruns[ia];
    const metarun *mb = &metaruns[ib];

    if (ma->score != mb->score)
        return (ma->score < mb->score) ? 1 : -1;
    if (ma->last_played != mb->last_played)
        return (ma->last_played < mb->last_played) ? 1 : -1;
    if (ma->id < mb->id) return -1;
    if (ma->id > mb->id) return 1;
    return 0;
}

static bool build_meta_path(char *buf, size_t len,
    const metarun *m, const char *leaf)
{
    const char* name = leaf ? leaf : "";

    if (!m)
    {
#ifdef SIL_USE_LOCAL_DATA
        /* Portable build: use ANGBAND_DIR_APEX */
        if (!path_build(buf, len, ANGBAND_DIR_APEX, name))
#else
        /* Normal build: use parent of ANGBAND_DIR_METARUN (the meta directory) */
        char meta_dir[1024];
        if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
            SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
            char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
            if (last_sep) *last_sep = '\0';
        } else {
            SDL_strlcpy(meta_dir, ANGBAND_DIR_APEX, sizeof(meta_dir));
        }
        if (!path_build(buf, len, meta_dir, name))
#endif
        {
            log_error("build_meta_path: failed for apex/%s", name);
            return false;
        }
        return true;
    }

    char sub[128];
    if (name[0])
        strnfmt(sub, sizeof sub, "%s/%08u/%s",
            META_SUBDIR, (unsigned)m->id, name);
    else
        strnfmt(sub, sizeof sub, "%s/%08u",
            META_SUBDIR, (unsigned)m->id);
#ifdef SIL_USE_LOCAL_DATA
    if (!path_build(buf, len, ANGBAND_DIR_APEX, sub))
#else
    /* For metarun subdirectories, use ANGBAND_DIR_METARUN */
    if (!path_build(buf, len, ANGBAND_DIR_METARUN, sub))
#endif
    {
        log_error("build_meta_path: failed for %s", sub);
        return false;
    }
    return true;
}

static void reset_defaults(metarun *m)
{
    log_info("Initializing new metarun with default values");
    memset(m, 0, sizeof(*m));
    metarun_clear_blessing_runtime_fields(m);
    m->id          = 1;
    m->last_played = (u32b)time(NULL);
    memset(m->curse_stacks, 0, sizeof(m->curse_stacks));
    m->curses_seen = 0;
    m->deaths      = 0;
    m->silmarils   = 0;
    
    /* Initialize persistent settings with defaults */
    for (int i = 0; i < 8; i++) {
        m->persistent_options[i] = 0;
    }
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        m->persistent_window_flags[i] = 0;
    }
    m->persistent_delay_factor = 5;      /* Default delay factor */
    m->persistent_hitpoint_warn = 3;     /* Default hitpoint warning */
    m->persistent_options_initialized = 0; /* Mark as not initialized yet */
    
    /* Initialize quest tracking */
    m->completed_quests = 0;             /* No quests completed initially */
    for (int i = 0; i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = 0;
    }
    metarun_clamp_and_sync_quests(m);
    
    /* Initialize oath system tracking */
    m->unlocked_oaths = 0;               /* No oaths unlocked initially */
    m->banned_oaths = 0;                 /* No oaths banned initially */
    m->max_difficulty_reached = 0;       /* Start with easiest difficulty */
    
    /* Clear quest_reserved array */
    for (int i = 0; i < 12; i++) {
        m->quest_reserved[i] = 0;
    }

    m->score = compute_metarun_score(m);
    update_blessing_ledger(m);

    log_debug("After init: curses_seen = 0x%016llX", (unsigned long long)m->curses_seen);
}

static bool ensure_default_metarun_slot(const char *reason)
{
    if (metarun_max > 0 && metaruns) return false;

    if (metaruns) {
        mem_free_null(metaruns);
        metaruns = NULL;
    }

    if (reason && *reason)
        log_warn("Metarun recovery triggered (%s); creating default entry", reason);
    else
        log_warn("Metarun recovery triggered; creating default entry");

    metarun_max = 1;
    metaruns = mem_alloc_array(metarun_max, metarun);
    reset_defaults(&metaruns[0]);
    metarun_created = true;

    return true;
}

/* Apply initial curses based on difficulty level (runtype) */
static void apply_difficulty_curses(metarun *m)
{
    if (!runtype_info) return; /* runtype data not loaded yet */
    if (m->type >= z_info->rt_max) return; /* invalid runtype */

    runtype_type *rt = &runtype_info[m->type];
    
    log_info("Applying curses for runtype %d (%s)", m->type, rt->name);
    
    /* Apply curses based on runtype configuration */
    if (rt->start_curses)
    {
        int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
        for (int curse_id = 0; curse_id < limit; curse_id++)
        {
            if (rt->start_curses & (1ULL << curse_id))
            {
                byte stacks = rt->curse_stacks[curse_id];
                if (stacks > 0)
                {
                    CURSE_SET(curse_id, stacks);
                    CURSE_SEEN_SET(curse_id);
                    log_debug("Applied %d stacks of curse %d from runtype", stacks, curse_id);
                }
            }
        }
    }
}

/* ensure directory apex/metaruns/NNNNNNNN exists */
static void ensure_run_dir(const metarun *m)
{
    char dir[1024];
    if (!path_build(dir, sizeof dir, ANGBAND_DIR_APEX, META_SUBDIR))
    {
        log_error("ensure_run_dir: failed to build base metarun directory");
        return;
    }
    MKDIR(dir);
    strnfmt(dir, sizeof dir, "%s/%08u", META_SUBDIR, (unsigned)m->id);
    if (!path_build(dir, sizeof dir, ANGBAND_DIR_APEX, dir))
    {
        log_error("ensure_run_dir: failed to build run directory for id=%u",
            (unsigned)m->id);
        return;
    }
    MKDIR(dir);
}

static bool sync_current_metarun_slot(bool stamp_time)
{
    if (!metaruns || current_run < 0 || current_run >= metarun_max) {
        return false;
    }

    if (stamp_time) {
        metar.last_played = (u32b)time(NULL);
    }

    metarun_clamp_and_sync_quests(&metar);
    metaruns[current_run] = metar;
    return true;
}

/* forward declarations */
static void start_new_metarun(void);
static void choose_difficulty_menu(void);
static void print_heading_fade(cptr title, byte final_attr);
static bool print_paragraph_fade(cptr txt, byte final_attr, int row);
static void adjust_blessing_threshold_menu(void);
/* =======================  load / save  ========================= */

/*
 * Clean up old save and score files when starting fresh (no meta.raw exists)
 */
void cleanup_old_game_files(void)
{
#ifndef METARUN_CLEANUP_OLD_FILES
    log_info("*** FRESH STARTUP CLEANUP DISABLED (METARUN_CLEANUP_OLD_FILES not defined) ***");
    return;
#else
    log_info("*** FRESH STARTUP CLEANUP STARTING ***");
    
    /* Use the correct save directory - ANGBAND_DIR_SAVE points to lib/save */
    char save_dir[1024];
    strnfmt(save_dir, sizeof(save_dir), "%s", ANGBAND_DIR_SAVE);
    
    log_trace("Fresh startup: checking save directory: %s", save_dir);
    
    /* Platform-agnostic approach: scan directory for ANY files (except .gitignore and archives) */
    bool has_save_files = false;
    
    #ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile for directory scanning */
    WIN32_FIND_DATA findData;
    char search_path[1024];
    if (!path_build(search_path, sizeof(search_path), save_dir, "*"))
    {
        log_error("cleanup_old_game_files: failed to build save directory search path");
        return;
    }
    
    HANDLE hFind = FindFirstFile(search_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /* Skip directories and special entries */
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            
            char* filename = findData.cFileName;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Found a save file! */
            has_save_files = true;
            log_trace("Fresh startup: found save file: %s", filename);
            break;
            
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
    #else
    /* Unix/Linux/macOS: Use POSIX opendir/readdir */
    DIR *dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Found a save file! */
            has_save_files = true;
            log_trace("Fresh startup: found save file: %s", filename);
            break;
        }
        closedir(dir);
    }
    #endif
    
    /* ULTRA FAST EXIT if no save files detected */
    if (!has_save_files) {
        log_info("*** NO SAVE FILES DETECTED - INSTANT FRESH START ***");
        
        /* Quick score file check and removal */
        char score_file[1024];
        if (path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw"))
        {
            SDL_IOStream* score_fd = sdl_fopen(score_file, "rb");
            if (score_fd) {
                sdl_fclose(score_fd);
                log_info("*** REMOVING SCORE FILE FOR FRESH START ***");

                /* Platform-agnostic file removal using standard C */
                remove(score_file);
            } else {
                log_trace("Fresh startup: no score file found");
            }
        }
        else
        {
            log_error("cleanup_old_game_files: failed to build score file path");
        }
        
        log_info("*** INSTANT FRESH STARTUP COMPLETED ***");
        return;  /* INSTANT EXIT - no shell commands needed */
    }
    
    /* Comprehensive cleanup: delete ALL files except .gitignore and archive files using ONLY standard C */
    log_info("*** FOUND SAVE FILES - DELETING ALL NON-ARCHIVE FILES ***");
    
    /* Use ONLY standard C functions - no shell commands for better portability */
    int files_deleted = 0;
    
#ifdef WINDOWS
    /* Windows: Use FindFirstFile/FindNextFile to enumerate and delete */
    WIN32_FIND_DATA cleanupFindData;
    char cleanup_search_path[1024];
    if (path_build(cleanup_search_path, sizeof(cleanup_search_path), save_dir, "*"))
    {
        HANDLE hCleanupFind = FindFirstFile(cleanup_search_path, &cleanupFindData);
        if (hCleanupFind != INVALID_HANDLE_VALUE) {
            do {
                /* Skip directories and special entries */
                if (cleanupFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

                char* filename = cleanupFindData.cFileName;

                /* Skip .gitignore and archive files */
                if (strcmp(filename, ".gitignore") == 0) continue;
                if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;

                /* Delete this file using standard C */
                char file_path[1024];
                if (!path_build(file_path, sizeof(file_path), save_dir, filename))
                {
                    log_error("cleanup_old_game_files: failed to build deletion path for '%s'", filename);
                    continue;
                }

                if (remove(file_path) == 0) {
                    files_deleted++;
                    log_trace("Fresh startup: deleted file: %s", filename);
                } else {
                    log_trace("Fresh startup: failed to delete: %s", filename);
                }

            } while (FindNextFile(hCleanupFind, &cleanupFindData));
            FindClose(hCleanupFind);
        }
    }
    else
    {
        log_error("cleanup_old_game_files: failed to build cleanup search path");
    }
#else
    /* Unix/Linux/macOS: Use opendir/readdir to enumerate and delete */
    dir = opendir(save_dir);
    if (dir) {
        struct dirent *entry;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip directories and special entries */
            if (entry->d_type == DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
            char* filename = entry->d_name;
            
            /* Skip .gitignore and archive files */
            if (strcmp(filename, ".gitignore") == 0) continue;
            if (strstr(filename, ".tar") || strstr(filename, ".zip") || strstr(filename, ".gz")) continue;
            
            /* Delete this file using standard C */
            char file_path[1024];
            if (!path_build(file_path, sizeof(file_path), save_dir, filename))
            {
                log_error("cleanup_old_game_files: failed to build deletion path for '%s'", filename);
                continue;
            }
            
            if (remove(file_path) == 0) {
                files_deleted++;
                log_trace("Fresh startup: deleted file: %s", filename);
            } else {
                log_trace("Fresh startup: failed to delete: %s", filename);
            }
        }
        closedir(dir);
    }
    #endif
    
    if (files_deleted > 0) {
        log_info("*** FRESH STARTUP DELETED %d FILES USING STANDARD C ***", files_deleted);
    } else {
        log_info("*** NO FILES FOUND TO DELETE ***");
    }
    
    /* Score file cleanup */
    char score_file[1024];
    if (!path_build(score_file, sizeof(score_file), ANGBAND_DIR_APEX, "scores.raw"))
    {
        log_error("cleanup_old_game_files: failed to build score file path during cleanup");
        return;
    }
    
    SDL_IOStream* score_fd = sdl_fopen(score_file, "rb");
    if (score_fd) {
        sdl_fclose(score_fd);
        log_info("*** REMOVING SCORE FILE FOR FRESH START ***");
        
        /* Platform-agnostic file removal using standard C */
        remove(score_file);
    }
    
    log_info("*** FRESH STARTUP CLEANUP COMPLETED ***");
#endif /* METARUN_CLEANUP_OLD_FILES */
}

errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    SDL_IOStream* fd;
    bool found_existing_data = false;

    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;
    fd = sdl_fopen(fn, "rb");

#ifdef SIL_USE_LOCAL_DATA
    if (!fd) {
        char legacy_dir[1024];
        char legacy[1024];
        if (path_build(legacy_dir, sizeof legacy_dir, ANGBAND_DIR_APEX, META_SUBDIR)
            && path_build(legacy, sizeof legacy, legacy_dir, META_RAW))
        {
            fd = sdl_fopen(legacy, "rb");
            if (fd) {
                log_info("Loading legacy portable metarun file: %s", legacy);
                found_existing_data = true;
            }
        }
    }
#else
    if (!fd && ANGBAND_DIR_METARUN && ANGBAND_DIR_METARUN[0]) {
        char legacy[1024];
        if (path_build(legacy, sizeof legacy, ANGBAND_DIR_METARUN, META_RAW)) {
            fd = sdl_fopen(legacy, "rb");
            if (fd) {
                log_info("Loading legacy metarun file: %s", legacy);
                found_existing_data = true;
            }
        }
        else
        {
            log_error("load_metarun_data: failed to build legacy path");
        }
    }
#endif

    if (fd) {
        found_existing_data = true;
    }

    if (!fd && create_if_missing) {
        log_info("Creating new versioned metarun file: %s", fn);
        FILE_TYPE(FILE_TYPE_DATA);
        fd = sdl_fmake(fn, 0644);
        if (!fd) return -1;

        /* Write versioned header */
        meta_file_header header;
        header.version_major = METARUN_FILE_VERSION_MAJOR;
        header.version_minor = METARUN_FILE_VERSION_MINOR;
        header.version_patch = METARUN_FILE_VERSION_PATCH;
        header.version_extra = METARUN_FILE_VERSION_EXTRA;
        header.entry_count = 1;

        sdl_write(fd, (cptr)&header, sizeof(header));

        metarun seed;
        reset_defaults(&seed);
        seed.score = compute_metarun_score(&seed);
        sdl_write(fd, (cptr)&seed, sizeof seed);
        sdl_fclose(fd);
        fd = sdl_fopen(fn, "rb");
        /* Only set metarun_created if we truly created a NEW file, not migrating existing data */
        if (!found_existing_data) {
            metarun_created = true;
            log_info("Created brand new metarun - will show story intro");
        } else {
            log_info("Seeded new metarun file from existing data - skipping intro");
        }
    }
    else log_info("Loading existing metarun file: %s", fn);
    if (!fd) return -1;

    /* All metarun files are versioned (v0.9.0+) */
    Sint64 file_size_64 = sdl_size(fd);
    int file_size = (file_size_64 > 0) ? (int)file_size_64 : 0;
    const char *recovery_reason = NULL;

    meta_file_header header;
    sdl_seek(fd, 0);
    if (sdl_read(fd, (char*)&header, sizeof(header)) != 0) {
        log_error("Failed to read metarun header");
        sdl_fclose(fd);
        return -1;
    }

    log_info("Loading versioned meta file v%d.%d.%d.%d (%u entries)",
             header.version_major, header.version_minor,
             header.version_patch, header.version_extra, header.entry_count);

    bool header_matches_current = (header.version_major == METARUN_FILE_VERSION_MAJOR &&
                                   header.version_minor == METARUN_FILE_VERSION_MINOR &&
                                   header.version_patch == METARUN_FILE_VERSION_PATCH &&
                                   header.version_extra == METARUN_FILE_VERSION_EXTRA);
    if (!header_matches_current) {
        log_warn("metarun: file version v%d.%d.%d.%d differs from game version v%d.%d.%d.%d",
                 header.version_major, header.version_minor, header.version_patch, header.version_extra,
                 METARUN_FILE_VERSION_MAJOR, METARUN_FILE_VERSION_MINOR, METARUN_FILE_VERSION_PATCH, METARUN_FILE_VERSION_EXTRA);
    }

    metarun_max = header.entry_count;
    size_t payload = (file_size >= (int)sizeof(meta_file_header))
                   ? (size_t)file_size - sizeof(meta_file_header)
                  : 0;
    size_t entry_size = (metarun_max > 0)
                      ? (payload / (size_t)metarun_max)
                      : 0;

    if (metarun_max > 0 && entry_size > 0) {
        metaruns = mem_alloc_array(metarun_max, metarun);
        sdl_seek(fd, sizeof(meta_file_header));

        if (entry_size == sizeof(metarun)) {
            sdl_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
            for (s16b i = 0; i < metarun_max; i++) {
                if (header.version_major == 0 && header.version_minor < 9) {
                    metaruns[i].blessing_points_spent = 0;
                }
                /* Initialize pending blessing choices for pre-0.9.0.1 saves
                 * (fields were part of reserved_runtime and may contain garbage) */
                if (header.version_major == 0 && header.version_minor == 9 &&
                    header.version_patch == 0 && header.version_extra == 0) {
                    /* Clear pending choices - will be regenerated on first menu open */
                    metaruns[i].pending_blessing_count = 0;
                    for (int j = 0; j < 3; j++) {
                        metaruns[i].pending_blessing_choices[j] = 255;
                    }
                    log_debug("Cleared pending blessing choices for metarun %d (loaded from v0.9.0.0)", i);
                }
                metarun_clamp_and_sync_quests(&metaruns[i]);
                metarun_sanitize_blessing_economy(&metaruns[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
        } else if (entry_size == METARUN_V10_SIZE) {
            metarun_v10 *legacy = mem_alloc_array(metarun_max, metarun_v10);
            sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v10));
            for (s16b i = 0; i < metarun_max; i++) {
                metarun_from_v10(&metaruns[i], &legacy[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
            legacy = mem_free(legacy);
        } else if (entry_size == METARUN_V9_SIZE) {
            metarun_v9 *legacy = mem_alloc_array(metarun_max, metarun_v9);
            sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v9));
            for (s16b i = 0; i < metarun_max; i++) {
                metarun_from_v9(&metaruns[i], &legacy[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
            legacy = mem_free(legacy);
        } else if (entry_size == METARUN_V8_SIZE) {
            metarun_v8 *legacy = mem_alloc_array(metarun_max, metarun_v8);
            sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v8));
            for (s16b i = 0; i < metarun_max; i++) {
                metarun_from_v8(&metaruns[i], &legacy[i]);
                metarun_sanitize_major_blessing_bits(&metaruns[i]);
            }
            legacy = mem_free(legacy);
        } else {
            recovery_reason = "versioned meta.raw had unsupported entry size (requires v0.9.0+)";
            log_warn("Unsupported metarun entry size %zu in versioned file; dropping pre-0.9.0 legacy support", entry_size);
            mem_free_null(metaruns);
            metaruns = NULL;
            metarun_max = 0;
        }
    } else if (metarun_max == 0) {
        recovery_reason = "versioned meta.raw reported zero entries";
        log_warn("Versioned meta file contains zero entries");
    } else {
        recovery_reason = "versioned meta.raw had invalid payload size";
        log_warn("Versioned meta file payload %zu does not align with %d entries",
                 payload, metarun_max);
        mem_free_null(metaruns);
        metaruns = NULL;
        metarun_max = 0;
    }

    if (metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    sdl_fclose(fd);

    bool seeded_default = false;
    if (metarun_max <= 0 || !metaruns) {
        seeded_default = ensure_default_metarun_slot(recovery_reason);
    }

    /* choose current run */
    u32b latest = 0;
    current_run = -1;  /* Initialize to invalid value so any valid entry will be selected */

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            log_debug("Metarun %d: id=%u, last_played=%u, deaths=%u, silmarils=%u",
                      i, metaruns[i].id, metaruns[i].last_played, metaruns[i].deaths, metaruns[i].silmarils);

            if (metaruns[i].last_played > latest ||
                (metaruns[i].last_played == latest && i > current_run))
            {
                latest      = metaruns[i].last_played;
                current_run = i;
                log_debug("Selected metarun %d as current (last_played=%u)", i, latest);
            }
        }
    }

    if (current_run < 0 || current_run >= metarun_max) {
        if (ensure_default_metarun_slot("no valid metarun could be selected")) {
            seeded_default = true;
        }
        log_info("No valid metarun found, defaulting to entry 0");
        current_run = 0;
    }

    if (metarun_max <= 0 || !metaruns) {
        if (ensure_default_metarun_slot("metarun array unavailable before final selection")) {
            seeded_default = true;
        }
    }

    if (seeded_default) {
        log_info("Metarun loader seeded a default entry to recover from a corrupt or empty meta.raw");
    }

    metar = metaruns[current_run];
    metarun_clamp_and_sync_quests(&metar);
    metaruns[current_run].completed_quests = metar.completed_quests;
    memcpy(metaruns[current_run].quest_completion_counts,
           metar.quest_completion_counts,
           sizeof(metar.quest_completion_counts));
    metarun_sanitize_blessing_economy(&metar);
    metaruns[current_run].fallen_score_pool = metar.fallen_score_pool;
    metaruns[current_run].blessing_points = metar.blessing_points;
    metaruns[current_run].blessing_points_spent = metar.blessing_points_spent;
    metar.score = compute_metarun_score(&metar);
    metaruns[current_run].score = metar.score;
    metarun_apply_runtime_effects();
    log_debug("Final current_run=%d, metar: id=%u, deaths=%u, silmarils=%u",
              current_run, metar.id, metar.deaths, metar.silmarils);

    /* ensure its per-run directory exists */
    ensure_run_dir(&metar);
    
    /* Apply difficulty curses only if this is a newly created metarun */
    if (metarun_created)
    {
        apply_difficulty_curses(&metar);
        save_metaruns(); /* persist the changes */
    }
    
    log_debug("Loaded metarun %d with %d silmarils, %d deaths", metar.id, metar.silmarils, metar.deaths);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Safely write the meta-run array.  Bail out if the indices look     *
 *  wrong - avoids dereferencing a freed/reallocated block.           *
 * ------------------------------------------------------------------ */
static errr backup_file(const char *filepath)
{
    static u32b last_backup_time = 0;
    static char last_backed_up_file[1024] = "";
    u32b current_time = (u32b)time(NULL);
    
    /* Throttle backups: only create backup if 
     * 1. This is a different file than last time, OR
     * 2. More than 300 seconds (5 minutes) have passed since last backup of this file
     */
    if (SDL_strcasecmp(last_backed_up_file, filepath) != 0) {
        /* Different file - always backup */
        log_info("backup_file: backing up different file: %s", filepath);
    } else if (current_time - last_backup_time >= 300) {
        /* Same file but enough time has passed (5 minutes instead of 1 minute) */
        log_info("backup_file: backing up %s after %u seconds", filepath, current_time - last_backup_time);
    } else {
        /* Same file, recent backup - skip */
        log_trace("backup_file: skipping backup of %s (last backup %u seconds ago)", 
                  filepath, current_time - last_backup_time);
        return 0;
    }
    
    /* Check if original file exists */
    SDL_IOStream* fd_src = sdl_fopen(filepath, "rb");
    if (!fd_src) {
        /* Original file doesn't exist, no backup needed */
        log_info("backup_file: original file %s doesn't exist, no backup needed", filepath);
        return 0;
    }
    
    /* Get file size */
    Sint64 file_size_64 = sdl_size(fd_src);
    int file_size = (file_size_64 > 0) ? (int)file_size_64 : 0;
    if (file_size <= 0) {
        log_info("backup_file: original file %s is empty, no backup needed", filepath);
        sdl_fclose(fd_src);
        return 0;
    }
    
    log_info("backup_file: creating backup for %s (size: %d bytes)", filepath, file_size);
    
    /* Read original file */
    char *buffer = mem_alloc_array(file_size, char);
    if (!buffer) {
        sdl_fclose(fd_src);
        return -1;
    }
    
    if (sdl_read(fd_src, buffer, file_size) != 0) {
        buffer = mem_free(buffer);
        sdl_fclose(fd_src);
        return -1;
    }
    sdl_fclose(fd_src);
    
    /* Optimize backup rotation: Only do full rotation once per session/day
     * For frequent saves, just overwrite .bak1 */
    char backup_path1[1024], backup_path2[1024], backup_path3[1024];
    strnfmt(backup_path1, sizeof(backup_path1), "%s.bak1", filepath);
    strnfmt(backup_path2, sizeof(backup_path2), "%s.bak2", filepath);
    strnfmt(backup_path3, sizeof(backup_path3), "%s.bak3", filepath);
    
    /* Check if this is the first backup of the day (roughly) */
    bool should_rotate = false;
    SDL_IOStream* fd_test1 = sdl_fopen(backup_path1, "rb");
    if (fd_test1) {
        /* Check if bak1 is old enough to warrant rotation (use simple time check) */
        /* If we created a backup within the last hour, don't rotate */
        if (current_time - last_backup_time >= 3600) {  /* 1 hour */
            should_rotate = true;
            log_info("backup_file: enough time passed since last backup, will rotate backups");
        }
        sdl_fclose(fd_test1);
    } else {
        /* No bak1 exists, create fresh backup */
        should_rotate = false;
        log_info("backup_file: no existing backup, creating fresh bak1");
    }
    
    if (should_rotate) {
        log_info("backup_file: rotating backups for %s", filepath);
        
        /* Rotate: bak2 -> bak3, bak1 -> bak2, current -> bak1 */
        fd_kill(backup_path3);                    /* Remove oldest */
        log_debug("backup_file: removed old bak3");
        
        /* Move bak2 to bak3 (if bak2 exists) */
        SDL_IOStream* fd_test2 = sdl_fopen(backup_path2, "rb");
        if (fd_test2) {
            sdl_fclose(fd_test2);
            log_debug("backup_file: moving bak2 to bak3");
            if (!fd_move(backup_path2, backup_path3)) {
                log_debug("backup_file: failed to move bak2 to bak3");
            }
        }
        
        /* Move bak1 to bak2 (if bak1 exists) */
        fd_test1 = sdl_fopen(backup_path1, "rb");
        if (fd_test1) {
            sdl_fclose(fd_test1);
            log_debug("backup_file: moving bak1 to bak2");
            if (!fd_move(backup_path1, backup_path2)) {
                log_debug("backup_file: failed to move bak1 to bak2");
            }
        }
    } else {
        /* Just overwrite bak1 for frequent saves */
        log_debug("backup_file: overwriting existing bak1 (frequent save)");
        fd_kill(backup_path1);
    }
    
    /* Create new bak1 from current file */
    log_info("backup_file: creating new bak1 from current file (size: %d)", file_size);
    SDL_IOStream* fd_dst = sdl_fmake(backup_path1, 0644);
    if (!fd_dst) {
        buffer = mem_free(buffer);
        return -1;
    }
    
    errr result = sdl_write(fd_dst, buffer, file_size);
    sdl_fclose(fd_dst);
    buffer = mem_free(buffer);
    
    if (result == 0) {
        log_info("backup_file: successfully created backup for %s", filepath);
        /* Update throttling variables only on successful backup */
        last_backup_time = current_time;
        SDL_strlcpy(last_backed_up_file, filepath, sizeof(last_backed_up_file));
    } else {
        log_error("backup_file: failed to write bak1 for %s", filepath);
    }
    
    return result;
}

errr save_metaruns(void)
{
    static u32b last_save_time = 0;
    u32b current_time = (u32b)time(NULL);
    
    /* Log save frequency tracking */
    if (last_save_time > 0) {
        u32b time_since_last = current_time - last_save_time;
        log_info("save_metaruns() called again after %u seconds", time_since_last);
    } else {
        log_info("save_metaruns() called for the first time this session");
    }
    last_save_time = current_time;

    refresh_current_metar_score();

    char fn[1024];
    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;

    /* Create backup before saving */
    backup_file(fn);

    log_debug("Before save: current_run=%d, metar: id=%u, deaths=%u, silmarils=%u, score=%u", 
              current_run, metar.id, metar.deaths, metar.silmarils, metar.score);
              
    metarun_clamp_and_sync_quests(&metar);
    metar.last_played      = current_time;
    metaruns[current_run] = metar;            /* safe: array is valid */
    
    log_debug("After updating array: metaruns[%d]: id=%u, deaths=%u, silmarils=%u, score=%u", 
              current_run, metaruns[current_run].id, metaruns[current_run].deaths, metaruns[current_run].silmarils,
              metaruns[current_run].score);

    /* After backup is created in backup_file(), remove the original so sdl_fmake can succeed */
    fd_kill(fn);
    
    /* Write using the new versioned format */
    SDL_IOStream* fd = sdl_fmake(fn, 0644);
    if (!fd) {
        log_info("Failed to create metarun file for writing");
        return -1;
    }

    /* Write version header first */
    meta_file_header header;
    header.version_major = METARUN_FILE_VERSION_MAJOR;
    header.version_minor = METARUN_FILE_VERSION_MINOR;
    header.version_patch = METARUN_FILE_VERSION_PATCH;
    header.version_extra = METARUN_FILE_VERSION_EXTRA;
    header.entry_count = metarun_max;
    
    errr result = sdl_write(fd, (cptr)&header, sizeof(header));
    if (result != 0) {
        sdl_fclose(fd);
        log_info("Failed to write metarun header to file");
        return -1;
    }

    /* Write metarun data */
    int bytes_to_write = metarun_max * sizeof(metarun);
    result = sdl_write(fd, (cptr)metaruns, bytes_to_write);
    sdl_fclose(fd);
    
    if (result != 0) {
        log_info("Failed to write metarun data to file");
        return -1;
    }
    
    log_info("Metarun data saved successfully (%d bytes, %d entries)", bytes_to_write, metarun_max);

    return 0;
}

u32b compute_blessing_pool(void)
{
    u32b total = score_sum_dead_points();
    metar.fallen_score_total = total;
    update_blessing_ledger(&metar);
    metarun_sanitize_major_blessing_bits(&metar);
    refresh_alive_cache();

    if (!sync_current_metarun_slot(false)) {
        log_warn("compute_blessing_pool: unable to sync current slot");
    }
    return total;
}

int blessing_points_available(void)
{
    u32b total = compute_blessing_pool();
    (void)total; /* Up-to-date total already stored in metar */

    int available = (int)metar.blessing_points - (int)metar.blessing_points_spent;
    if (available < 0) available = 0;
    return available;
}

int metarun_alive_count_cached(void)
{
    refresh_alive_cache();
    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_alive_count_cached: unable to sync current slot");
    }
    return metar.alive_characters;
}

int any_curse_flag_active(u32b flag)
{
    /* Intended for CUR flags such as CUR_NOCHOICE (curse-only, not blessings). */
    if (!z_info || !cu_info) return 0;
    int count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        int stacks = CURSE_GET(id);
        if (stacks > 0 && (cu_info[id].flags_u & flag)) count += stacks;
    }
    return count;
}

/* ---------------------------------------------------------------
 * Simple counters used by other modules (no UI side-effects)
 * ------------------------------------------------------------- */
void metarun_increment_deaths(void)
{
    /* Clamp to byte range; defer saving/UI to caller */
    if (metar.deaths >= 255) return;

    metar.deaths++;

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_increment_deaths: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
}

void metarun_gain_silmarils(byte n)
{
    if (!n) return;
    int total = (int)metar.silmarils + (int)n;
    if (total > 255) total = 255;
    if (total < 0) total = 0;
    metar.silmarils = (byte)total;
    refresh_current_metar_score();

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_gain_silmarils: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
}

void metarun_apply_runtime_effects(void)
{
    metarun_sanitize_major_blessing_bits(&metar);

    int weight_cap = SUPPLIES_MAX_WEIGHT_DEFAULT;
    if (metarun_has_major_blessing_effect(METARUN_MAJOR_EFFECT_SUPPLY_LIMIT)) {
        weight_cap = SUPPLIES_MAX_WEIGHT_BLESSING;
    }
    supplies_set_max_weight_cap(weight_cap);
}

int metarun_major_blessing_count(void)
{
    return major_blessing_capacity();
}

bool metarun_has_major_blessing_index(int idx)
{
    metarun_sanitize_major_blessing_bits(&metar);
    if (idx < 0) return false;
    int cap = major_blessing_capacity();
    if (idx >= cap) return false;
    if (!major_blessing_def(idx)) return false;
    return (metar.major_blessings & (1U << idx)) != 0;
}

bool metarun_has_major_blessing_effect(metarun_major_effect effect)
{
    if (effect == METARUN_MAJOR_EFFECT_NONE) return false;
    metarun_sanitize_major_blessing_bits(&metar);
    int cap = major_blessing_capacity();
    for (int i = 0; i < cap; i++) {
        if (!metarun_has_major_blessing_index(i)) continue;
        if (major_blessing_effect(i) == effect) return true;
    }
    return false;
}

/* ---------------------------------------------------------------
 * Persistent Settings Management
 * ------------------------------------------------------------- */

/*
 * Save current game options to the metarun persistent settings
 */
void metarun_save_persistent_settings(void)
{
    log_info("Saving persistent settings to metarun");
    
    /* Save options */
    for (int i = 0; i < 8; i++) {
        metar.persistent_options[i] = 0;
    }
    
    /* Pack options into the persistent storage */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;
        
        if (word_idx < 8 && option_text[i] && !option_is_app_persistent(i)
            && op_ptr->opt[i]) {
            metar.persistent_options[word_idx] |= (1UL << bit_idx);
        }
    }

    /* Save window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        metar.persistent_window_flags[i] = op_ptr->window_flag[i];
    }
    
    /* Mark as initialized */
    metar.persistent_options_initialized = 1;
    
    /* Save the metarun data */
    save_metaruns();
    
    log_info("Persistent settings saved successfully");
}

/*
 * Load metarun persistent settings to current game options
 */
void metarun_load_persistent_settings(void)
{
    /* Only load if settings have been previously saved */
    if (!metar.persistent_options_initialized) {
        log_info("No persistent settings found, using defaults");
        return;
    }
    
    log_info("Loading persistent settings from metarun");
    
    /* Load options */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;
        
        if (word_idx < 8 && option_text[i] && !option_is_app_persistent(i)) {
            op_ptr->opt[i] = (metar.persistent_options[word_idx] & (1UL << bit_idx)) != 0;
        }
    }

    /* Load window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
        op_ptr->window_flag[i] = metar.persistent_window_flags[i];
    }
    
    log_info("Persistent settings loaded successfully");
}

/* ---------------------------------------------------------------
 * Pick a curse at random, respecting weights, stacks, caps,
 * and the RHF_CURSE tail-lift and exclusion of most weighted curses.
 * ------------------------------------------------------------- */
static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    /* Pass 1 - find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 - sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;           /* cap reached */

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rand_int(z_info->cu_max);    /* safety net */

    /* Pass 3 - roulette wheel */
    long pick = rand_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rand_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (CURSE_CURSE_CAP(idx) &&
        CURSE_CURSE_STACK(idx) >= CURSE_CURSE_CAP(idx))
    {
        log_debug("Curse %d (%s) already at max stacks", idx, cu_name + cu_info[idx].name);
        return;
    }

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

int menu_choose_one_curse(int n)
{
    /* if any active curse has the "no-choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES], sel;
    bool steamdeck = steamdeck_controls_active();

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }
            
            byte cap = (byte)CURSE_CURSE_CAP(pick[i]);
            if (cap && CURSE_CURSE_STACK(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    screen_save();  Term_clear();
    
    /* Fade in the title */
    char str[60];
    const char* seq[] = { "a", "the second", "the third" };
    strnfmt(str, sizeof(str), "Dark powers demand their price - choose %s curse:", seq[n]);
    print_heading_fade(str, TERM_YELLOW);

    /* dynamic vertical layout - ask util.c to count wrapped lines   */
    int row = 4;                                     /* first free row */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 2;                   /* full width     */

    /* Show each curse one by one with fade-in effect */
    bool fast_forward = false;
    
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[pick[i]];
        char name_buf[128];
        if (steamdeck)
            strnfmt(name_buf, sizeof name_buf, "   %s", cu_name + cu->name);
        else
            strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);
        
        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);
        
#ifdef DEBUG_CURSES
        const char *pow = cu_text + cu->power;
        int need_pow_lines = 0;
        if (*pow) {
            need_pow_lines = count_wrapped_lines(pow, text_out_wrap, 4);
        }
#endif

        /* Fade in all text for this curse simultaneously */
        const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE };
        const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

        for (int s = 0; s < steps && !fast_forward; s++)
        {
            /* Check for ESC key to skip fade */
            char ch;
            if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
            {
                fast_forward = true;
                break;
            }

            /* Name line */
            c_put_str(s == steps - 1 ? TERM_L_RED : fade_cols[s], name_buf, row, 2);
            
            /* Poem text */
            Term_gotoxy(4, row + 2);
            text_out_c(s == steps - 1 ? TERM_SLATE : fade_cols[s], txt);

#ifdef DEBUG_CURSES
            /* Power text if present */
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(s == steps - 1 ? TERM_L_RED : fade_cols[s], pow);
            }
#endif
            
            Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, 200);
        }

        /* If fade was interrupted, show final state immediately */
        if (fast_forward) {
            c_put_str(TERM_L_RED, name_buf, row, 2);
            Term_gotoxy(4, row + 2);
            text_out_c(TERM_SLATE, txt);

#ifdef DEBUG_CURSES
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(TERM_L_RED, pow);
            }
#endif
        }

        /* Move to next curse position */
#ifdef DEBUG_CURSES
        if (*pow)
            row += need_lines + need_pow_lines + 3;
        else
#endif
            row += need_lines + 3;

        /* 1 second delay between curses (except for the last one) */
        if (i < CURSE_MENU_LINES - 1) {
            Term_xtra(TERM_XTRA_DELAY, 1000);
        }
    }

    /* Show the prompt immediately without fade */
    if (steamdeck)
    {
        char accept_label[16];
        char back_label[16];
        char hint_buf[96];

        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(hint_buf, sizeof(hint_buf),
            "D-pad to navigate     [%s] accept     [%s] cancel",
            accept_label, back_label);
        c_put_str(TERM_L_DARK, hint_buf, row + 1, 2);
    }
    else
    {
        c_put_str(TERM_L_DARK,
            "Arrows to navigate     Space/Enter Accept     a/b/c Select",
            row + 1, 2);
    }
    
    /* Menu navigation variables */
    int highlight = 0;  /* Currently highlighted option (0, 1, 2) */
    bool menu_done = false;
    int option_rows[CURSE_MENU_LINES];  /* Store the row for each option */
    
    /* Calculate row positions for each option */
    int calc_row = 4;
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        option_rows[i] = calc_row;
        curse_type *cu = &cu_info[pick[i]];
        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);
        calc_row += need_lines + 3;
    }
    
    while (!menu_done) {
        /* Ensure text output settings are consistent */
        text_out_hook = text_out_to_screen;
        text_out_wrap = Term->wid - 2;
        
        /* Update highlight display for each option */
        for (int i = 0; i < CURSE_MENU_LINES; i++) {
            curse_type *cu = &cu_info[pick[i]];
            char name_buf[128];
            if (steamdeck)
                strnfmt(name_buf, sizeof name_buf, "   %s", cu_name + cu->name);
            else
                strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);
            
            /* Clear the line first to remove any previous highlighting */
            Term_erase(2, option_rows[i], strlen(name_buf));
            
            /* Display the option with highlighting */
            if (i == highlight) {
                c_put_str(TERM_RED, name_buf, option_rows[i], 2);     /* Highlighted - red */
            } else {
                c_put_str(TERM_L_RED, name_buf, option_rows[i], 2);   /* Normal - light red */
            }
        }
        
        /* Position cursor at the end of the highlighted option text */
        curse_type *highlighted_cu = &cu_info[pick[highlight]];
        char highlighted_name_buf[128];
        if (steamdeck)
            strnfmt(highlighted_name_buf, sizeof highlighted_name_buf, "   %s",
                cu_name + highlighted_cu->name);
        else
            strnfmt(highlighted_name_buf, sizeof highlighted_name_buf, "%c) %s",
                'a'+highlight, cu_name + highlighted_cu->name);
        int cursor_col = 2 + strlen(highlighted_name_buf);
        Term_gotoxy(cursor_col, option_rows[highlight]);
        Term_fresh();
        char key = inkey();
        
        /* Handle input */
        if (!steamdeck && key >= 'a' && key < 'a' + CURSE_MENU_LINES) {
            /* Letter shortcuts */
            sel = key - 'a';
            menu_done = true;
        }
        else if (!steamdeck && key >= 'A' && key < 'A' + CURSE_MENU_LINES) {
            /* Capital letter shortcuts */
            sel = key - 'A';
            menu_done = true;
        }
        else if (key == '\r' || key == '\n' || key == ' ' || key == '6'
            || (steamdeck && key == steamdeck_confirm_key())) {
            /* Enter, Space, or numpad 6 - select current highlight */
            sel = highlight;
            menu_done = true;
        }
        else if (key == '8' || key == 'k') {
            /* Up navigation */
            highlight = (highlight + CURSE_MENU_LINES - 1) % CURSE_MENU_LINES;
        }
        else if (key == '2' || key == 'j') {
            /* Down navigation */
            highlight = (highlight + 1) % CURSE_MENU_LINES;
        }
        else if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())) {
            /* Escape - default to first option */
            sel = 0;
            menu_done = true;
        }
    }
    screen_load();
    return pick[sel];
}


/* ------------------------------------------------------------------ *
 *  Debug helper - wipe every active curse for the current meta-run.  *
 * ------------------------------------------------------------------ */
void metarun_clear_all_curses(void)
{
    log_info("Clearing all curses for current metarun");
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    metar.curses_seen = 0;
    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_clear_all_curses: unable to sync current slot");
    }
    save_metaruns();
}

/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 *  NOTE: save_metaruns() comes **after** check_run_end() so that     *
 *  any realloc in start_new_metarun() has already finished.          *
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 * ------------------------------------------------------------------ */
/*
 * Metarun narrative & exit logic - refactor **v4** (30 Jul 2025)
 * ------------------------------------------------------------------
 *  * Re-orders the sequence so NOTHING is overwritten:
 *      0. Escape-curse chooser (UI)  -> clears screen once finished.
 *      1. Chosen-curse line(s).
 *      2. Victory banner & Silmaril count paragraph.
 *      3. Temptation of Treachery (escalating 1-3 lines).
 *      4. Story Fragment (depends on Silmarils & Treachery flag).
 *      5. Echoes of Kinslaying (escalating 1-3 lines)
 *      6. Final pause, then deferred side-effects.
 *
 *  * `choose_escape_curses_ui()` now **returns** the indices chosen and
 *    does NOT leave the menu clutter on screen. We re-render the
 *    "The curse of X binds your fate." lines after a clean clear.
 *
 *  * Adds `print_story_fragment()` - a short narrative bridge keyed off
 *    Silmaril count (1-3) and whether treachery was overcome.
 *
 *  * Tested matrix: {treachery flag x kinslayer flag x silmarils (1-3)}
 *    All show in the intended order with no garbled overlaps.
 */

/********************  Enhanced UI helpers with fade-in effects  ***************************/

static void print_heading_fade(cptr title, byte final_attr)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    int w, h; 
    Term_get_size(&w, &h);
    
    // Center the heading
    int title_len = strlen(title);
    int start_col = (w - title_len) / 2;
    if (start_col < 1) start_col = 1;
    
    sdl_story_font_enable();
    
    for (int s = 0; s < steps; s++)
    {
        c_prt(fade_cols[s], title, 2, start_col);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 150);
    }
    Term_xtra(TERM_XTRA_DELAY, 500); // Extra pause after heading
    
    sdl_story_font_disable();
}

static bool print_paragraph_fade(cptr txt, byte final_attr, int row)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    
    text_out_hook   = text_out_to_screen;
    text_out_indent = 2;
    text_out_wrap   = Term->wid - 4;

    sdl_story_font_enable();

    for (int s = 0; s < steps; s++)
    {
        // Check for ESC key to skip fade
        char ch;
        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            // Show final state immediately and return interrupted status
            Term_gotoxy(2, row);
            text_out_c(final_attr, txt);
            text_out("\n");
            Term_fresh();
            sdl_story_font_disable();
            return false;
        }
        
        Term_gotoxy(2, row);
        text_out_c(fade_cols[s], txt);
        text_out("\n");
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }
    
    Term_xtra(TERM_XTRA_DELAY, 1000); // Pause after paragraph
    
    sdl_story_font_disable();
    return true;
}

static void print_paragraph(cptr txt, byte attr)
{
    text_out_hook   = text_out_to_screen;
    text_out_indent = 1;
    text_out_wrap   = Term->wid - 2;

    sdl_story_font_enable();

    Term_addstr(0, attr, "");
    text_out_c(attr, txt);
    text_out("\n");
    
    sdl_story_font_disable();
}

static void wait_for_keypress_with_prompt(cptr prompt)
{
    int w, h;
    Term_get_size(&w, &h);
    
    // Clear bottom line and show prompt
    Term_erase(0, h - 1, w);
    c_prt(TERM_L_WHITE, prompt ? prompt : "[Press any key to continue]", h - 1, 2);
    Term_fresh();
    
    (void)inkey();
    
    // Clear the prompt line
    Term_erase(0, h - 1, w);
}

static cptr curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;
    /* Strip common prefixes for cleaner display */
    if (strncmp(raw, "Curse of ", 9) == 0) raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0) raw += 8;
    return raw;
}

static cptr blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name) {
        cptr raw = cu_name + cu_info[idx].blessing_name;
        /* Strip "Blessing of " prefix for consistency */
        if (strncmp(raw, "Blessing of ", 12) == 0) raw += 12;
        return raw;
    }
    return curse_display_name(idx);
}

/****************  Escape-curse chooser (clean version) ************/

/*
 * Presents the menu *n* times (or once if CUR_NOCHOICE). Returns the
 * number of curses actually chosen and fills `out` with their indices.
 * The display is cleared afterwards so we can start narrative fresh.
 */
int choose_escape_curses_ui(int n, int out[4])
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;
    bool fast_forward = false;

    /* Display intro with fade-in effect */
    screen_save();
    Term_clear();
    
    print_heading_fade("The Valar's Judgment", TERM_L_BLUE);
    
    char intro_text[512];
    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : (n == 3) ? "three" : "four",
            (n == 1) ? "" : "s");
    
    if (!print_paragraph_fade(intro_text, TERM_L_WHITE, 4))
        fast_forward = true;
    
    wait_for_keypress_with_prompt("[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay side-effect */
        if (taken < 4) out[taken++] = idx;
    }

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();
    
    /* Restore screen state to fix character_icky imbalance */
    screen_load();
    
    /* Avoid unused variable warning */
    (void)fast_forward;
    
    return taken;
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific curse message with fade-in, waits 3 seconds,
 * then shows the permanent consequence message and curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    bool fast_forward = false;
    
    /* Display curse message with fade-in effect */
    screen_save();
    Term_clear();
    
    /* Add Tolkien-style heading */
    print_heading_fade("The Sundering of Sacred Vows", TERM_L_RED);
    
    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);
    
    /* Add empty line before E: text */
    Term_putstr(2, 4, -1, TERM_SLATE, "");
    
    /* Show only the permanent message (E: field) with fade */
    if (perm_msg && perm_msg[0]) {
        if (!print_paragraph_fade(perm_msg, TERM_L_RED, 5))
            fast_forward = true;
    } else {
        if (!print_paragraph_fade("Your oath is forever broken in this age.", TERM_L_RED, 5))
            fast_forward = true;
    }
    
    /* Hold the message for 3 seconds if not fast-forwarded */
    if (!fast_forward) {
        Term_xtra(TERM_XTRA_DELAY, 3000);
    }
    
    /* Add empty line before attention text */
    Term_putstr(2, 8, -1, TERM_SLATE, "");
    
    /* Show Morgoth's attention text with fade in red */
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");
    
    if (!print_paragraph_fade(intro_text, TERM_RED, 9))
        fast_forward = true;
    
    wait_for_keypress_with_prompt("[Press any key to face your judgment]");
    Term_clear();

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    log_debug("Player selected curse %d for oath breaking", idx);
    
    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();
    
    /* Restore screen state */
    screen_load();
    
    /* Avoid unused variable warning */
    (void)fast_forward;
    
    return idx;
}

/* ------------------------------------------------------------------ */
/*  Standard "Press any key..." prompts - use enum, not raw strings     */
/* ------------------------------------------------------------------ */
typedef enum {
    PROMPT_CONTINUE_TALE,
    PROMPT_FACE_TEMPTATION,
    PROMPT_CONTINUE_GENERIC,
    PROMPT_FACE_ECHOES,
    PROMPT_CONCLUDE_TALE,
    PROMPT_WITNESS_CONSEQUENCES,
    PROMPT_RETURN_MIDDLE_EARTH
} prompt_t;

static const char *prompt_text[] = {
    "[Press any key to continue your tale]",
    "[Press any key to face temptation]",
    "[Press any key to continue]",
    "[Press any key to face the echoes]",
    "[Press any key to conclude your tale]",
    "[Press any key to witness the consequences]",
    "[Press any key to return to Middle-earth]"
};

static void wait_prompt(prompt_t id) {         /* tiny wrapper */
    wait_for_keypress_with_prompt(prompt_text[id]);
}

/* ------------------------------------------------------------------
 * metarun_update_on_exit() - v5, 30 Jul 2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? gift-of-Eru?)
 *   1.  Escape-curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls - stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1-3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause -> apply deferred effects
 *   7.  Persist silmaril/death counters, check run end, save
 *
 *  All narrative helpers (print_heading(), print_paragraph(),
 *  choose_escape_curses_ui(), kinslayer_try_kill(), etc.) are reused.
 * ------------------------------------------------------------------ */
static void announce_blessing_gain(int previous_points)
{
    int current_points = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    if (current_points <= previous_points) return;
    int delta = current_points - previous_points;
    int available = current_points - metar.blessing_points_spent;
    if (available < 0) available = 0;
    msg_format("You gain %d blessing point%s. (%d available)",
               delta, (delta == 1) ? "" : "s", available);
    message_flush();
}

void metarun_update_on_exit(bool died, bool escaped, byte sil_count, s32b final_score)
{
    if (run_mode_is_blitz())
    {
        log_info("Suppressing metarun end-of-run processing for Blitz");
        if (escaped || (p_ptr && p_ptr->morgoth_slain && !died))
        {
            byte summary_sils = sil_count;
            if (p_ptr && p_ptr->morgoth_slain && summary_sils < 3)
                summary_sils = 3;
            blitz_show_end_summary(summary_sils);
        }
        return;
    }

    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d, final_score=%ld", 
             died ? "true" : "false", escaped ? "true" : "false", sil_count, (long)final_score);
    int blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;

    if (escaped)
    {
        sdl_music_play_main();
    }
             
    /* -------- Lineage flags -------------------------------------- */
    u32b character_flags = c_info[p_ptr->pcharacter].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (character_flags | f_race) & RHF_GIFTERU;
    bool allow_treachery = (character_flags | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (character_flags | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool fast_forward = false; // Track if user wants to skip fade effects
    bool morgoth_victory = (p_ptr->morgoth_slain && !escaped && !died);

    /* Treat as a death unless Eru intervenes */
    if (died && !has_gift_eru)
        metarun_increment_deaths();

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    - any path that reaches here counts as a "run end" event  */
    /* ------------------------------------------------------------- */
    if (morgoth_victory)
    {
        log_info("Metarun: Morgoth victory branch (sil_count=%d)", sil_count);
        screen_save();
        Term_clear();

        print_heading_fade("Beyond Fate", TERM_YELLOW);
        print_paragraph_fade(
            "The illusion of Morgoth lies shattered at your feet.",
            TERM_WHITE, 4);
        print_paragraph_fade(
            "From Valinor, the Valar proclaim your impossible triumph and pour out their blessing.",
            TERM_L_BLUE, 7);
        print_paragraph_fade(
            "Though the true Dark Enemy waits beyond this trial, three Silmarils are counted to your name.",
            TERM_L_BLUE, 10);

        wait_prompt(PROMPT_CONTINUE_TALE);

        screen_load();

        byte awarded = (sil_count < 3) ? 3 : sil_count;
        metarun_gain_silmarils(awarded);
        log_info("Metarun: Morgoth victory awarded %d Silmarils (total now %d)",
                 awarded, (int)metar.silmarils);
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        check_run_end();
        metarun_save_persistent_settings();
        save_metaruns();
        return;
    }
    else if (died)
    {
        log_info("Player died - displaying death narrative");
        /*****  NEW DEATH-NARRATIVE *****/
        screen_save();
        Term_clear();

        /* Pick correct sequence number: 0 when Gift-of-Eru fires,
         * otherwise 1-based death counter that was just incremented. */
        byte target_order = has_gift_eru ? 0 : metar.deaths;

        /* Build a pool of candidate story entries.                    */
        int *pool = mem_alloc_array(z_info->st_max, int);
        int pool_sz = 0;
        if (!pool) {
            screen_load();                 /* restore game view            */
            u32b pool_before = metar.fallen_score_total;
            refresh_current_metar_score();
            compute_blessing_pool();
            if (final_score > 0 && metar.fallen_score_total == pool_before) {
                metar.fallen_score_total += (u32b)final_score;
                update_blessing_ledger(&metar);
                (void)sync_current_metarun_slot(false);
            }
            announce_blessing_gain(blessing_points_before);
            blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
            check_run_end();
            save_metaruns();
            return;
        }
        for (int i = 0; i < z_info->st_max && pool; i++) {
            story_type *st = &st_info[i];
            if (!st->name)            continue;                /* unused slot   */
            if (st->st_type != 1)     continue;                /* not "death"   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback - allow any order-0 message if nothing matched.   */
        if (!pool_sz && target_order) {
            for (int i = 0; i < z_info->st_max && pool; i++) {
                story_type *st = &st_info[i];
                if (!st->name || st->st_type != 1) continue;
                if (st->order != 0)   continue;
                if (st->runtypes &&
                   !(st->runtypes & (1u << metar.type))) continue;
                pool[pool_sz++] = i;
            }
        }

        /* Display the chosen fragment with the usual fade-in style.  */
        if (pool_sz) {
            story_type *pick = &st_info[ pool[rand_int(pool_sz)] ];
            cptr title = st_name + pick->name;
            cptr text  = st_text + pick->text;

            print_heading_fade(title, TERM_RED);
            print_paragraph_fade(text, TERM_WHITE, 4);

            char transition_text[256];
            strnfmt(transition_text, sizeof(transition_text),
                    "The hero whose mantle you took has fallen, their tale ends in shadow. "
                    "Yet your spirit returns, for the Valar's trial is not yet complete.");

            if (!fast_forward && !print_paragraph_fade(transition_text, TERM_L_BLUE, 8))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(transition_text, TERM_L_BLUE);
            wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
        }

        screen_load();                 /* restore game view            */
        pool = mem_free(pool);
        u32b pool_before = metar.fallen_score_total;
        refresh_current_metar_score();
        compute_blessing_pool();
        if (final_score > 0 && metar.fallen_score_total == pool_before) {
            metar.fallen_score_total += (u32b)final_score;
            update_blessing_ledger(&metar);
            (void)sync_current_metarun_slot(false);
        }
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        check_run_end();
        save_metaruns();
        return;
    }
    else if (!escaped_with_sils) {
        log_debug("Player escaped without Silmarils - no narrative needed");
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        save_metaruns();
        return;                        /* no further narrative needed  */
    }

    /* ------------------------------------------------------------- */
    /*        Enhanced Narrative Path - escaped with >=1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative", sil_count);
    screen_save();

    /* ============================================================= */
    /* SCENE 1: Escape Curse Selection                              */
    /* ============================================================= */
    int curse_count = sil_count;
    int chosen[4] = { -1, -1, -1, -1 };

    if (sil_count == 3) curse_count = 4;

    int chosen_cnt = choose_escape_curses_ui(curse_count, chosen);

    /* ============================================================= */
    /* SCENE 2: The Binding of Fate                                 */
    /* ============================================================= */
    if (chosen_cnt > 0)
    {
        print_heading_fade("The Binding of Fate", TERM_L_RED);
        
        for (int i = 0; i < chosen_cnt; ++i)
        {
            char buf[128];
            strnfmt(buf, sizeof buf,
                    "The curse of %s binds your fate.",
                    curse_display_name(chosen[i]));
            
            if (!fast_forward && print_paragraph_fade(buf, TERM_RED, 4 + i * 2))
            {
                // Continue with fade effects
            }
            else
            {
                fast_forward = true;
                print_paragraph(buf, TERM_RED);
            }
        }
        
        wait_prompt(PROMPT_CONTINUE_TALE);
        Term_clear();
    }

    /* ============================================================= */
    /* SCENE 3: Victory Declaration                                  */
    /* ============================================================= */
    print_heading_fade("Victory Amid Shadow", TERM_YELLOW);
    
    const char *victory_text;
    switch (sil_count)
    {
        case 1:
            victory_text = "You emerge victorious from darkness, one holy jewel blazing in your grasp. Morgoth's crown is diminished, yet hope is rekindled, though shadow lingers.";
            break;
        case 2:
            victory_text = "You escape triumphant, two Silmarils blazing fiercely in your hands. Morgoth roars in wrath; his pride is wounded deeply. Your spirit exults, yet your heart begins to feel their burning weight.";
            break;
        case 3:
            victory_text = "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Feanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
            break;
        default:
            victory_text = "You have achieved the impossible, claiming more Silmarils than should exist. Reality itself bends before your triumph.";
            break;
    }
    
    if (!fast_forward && !print_paragraph_fade(victory_text, TERM_WHITE, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(victory_text, TERM_WHITE);
    
    if (allow_treachery)
        wait_prompt(PROMPT_FACE_TEMPTATION);
    else
        wait_prompt(PROMPT_CONTINUE_GENERIC);
    Term_clear();

    /* ============================================================= */
    /* SCENE 4: Temptation of Treachery (Enhanced Messages)        */
    /* ============================================================= */
    byte stolen = 0;
    if (allow_treachery)
    {
        static const int pct[3] = { 20, 50, 95 };
        
        /* Enhanced escalating treachery messages */
        static const char *success_msgs[3] = {
            "The first jewel shines brightly, its pure light uncorrupted. You master desire, choosing honor.",
            "The second jewel blazes defiant, temptation growing strong-but once more, you cling to honor.",
            "The third Silmaril's holy flame burns fiercely. Yet against all odds, your will resists corruption."
        };
        
        static const char *failure_msgs[3] = {
            "Greed whispers softly, and you listen. Secretly you withhold the jewel's light, betraying even yourself.",
            "Desire gnaws deeper; you falter, concealing its brilliance in shame, light darkened by your betrayal.",
            "Consumed by lust for its beauty, you claim it secretly, sealing its radiance from all others-a betrayal of all trust."
        };
        
        print_heading_fade("Temptation of Treachery", TERM_L_UMBER);
        
        int current_row = 4;

        for (int i = 0; i < sil_count; ++i)
        {
            bool fail = (rand_int(100) < pct[i]);
            if (fail) stolen++;
            
            const char *tempt_text = fail ? failure_msgs[i] : success_msgs[i];
            
            if (!fast_forward && !print_paragraph_fade(tempt_text, fail ? TERM_RED : TERM_WHITE, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(tempt_text, fail ? TERM_RED : TERM_WHITE);
            
            current_row += 3; // Space for next paragraph
        }

        if (stolen)
        {
            const char *shadow_text = "In shadows your deeds are recorded-tainted victory shall diminish the jewel's blessing.";
            if (!fast_forward && !print_paragraph_fade(shadow_text, TERM_L_DARK, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(shadow_text, TERM_L_DARK);
        }
        
        wait_prompt(PROMPT_CONTINUE_GENERIC);
        Term_clear();
    }

    byte final_sils = sil_count - stolen;
    bool treachery_occurred = (stolen > 0);

    /* ============================================================= */
    /* SCENE 5: The Weight of Victory                               */
    /* ============================================================= */
    print_heading_fade("The Weight of Victory", TERM_L_BLUE);
    
    const char *weight_text;
    if (!treachery_occurred)
    {
        const char *pure_frag[3] = {
            "A single star reclaimed, hope rekindled faintly in Middle-earth. Yet Morgoth laughs still, for two remain bound in shadow.",
            "Two jewels shine again beneath sky; Morgoth's power falters greatly. Yet you feel their brilliance burning; temptation ever near.",
            "All three jewels, radiant and pure, blaze again beneath stars. Morgoth's power breaks. Triumph is absolute, your soul soaring."
        };
        weight_text = pure_frag[final_sils-1];
    }
    else
    {
        const char *tainted_frag[3] = {
            "Though victory is yours, its memory darkens. Trust is fragile, and your spirit heavy beneath secret betrayal.",
            "Your heart trembles: Morgoth sees clearly your treachery-he smiles grimly, knowing darkness still dwells in you.",
            "Greatest triumph now mingled with darkest shame. Morgoth's laughter echoes bitterly-he senses your fall."
        };
        weight_text = tainted_frag[sil_count-1];
    }
    
    if (!fast_forward && !print_paragraph_fade(weight_text, treachery_occurred ? TERM_RED : TERM_L_WHITE, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(weight_text, treachery_occurred ? TERM_RED : TERM_L_WHITE);
    
    if (allow_kinslay)
        wait_prompt(PROMPT_FACE_ECHOES);
    else
        wait_prompt(PROMPT_CONCLUDE_TALE);
    Term_clear();

    /* ============================================================= */
    /* SCENE 6: Echoes of Kinslaying (Enhanced Notifications)      */
    /* ============================================================= */
    bool deferred_kill[3] = { false, false, false };
    int kinslaying_victims = 0;
    if (allow_kinslay)
    {
        print_heading_fade("Echoes of Kinslaying", TERM_L_RED);
        
        static const int kin_pct[3] = { 20, 50, 95 };
        int current_row = 4;

        for (int k = 0; k < sil_count; ++k)
        {
            /* One roll only - use kin_pct[] here and *skip* the roll
             * inside kinslayer_try_kill() later.                        */
            /* one-shot probability (keep a local alias for UI)        */
            bool fail = (rand_int(100) < kin_pct[k]);
            deferred_kill[k] = fail;
            if (fail) kinslaying_victims++;

            const char *echo_text = NULL;
            switch (k)
            {
                case 0: echo_text = fail ?
                    "\"Alqualonde's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualonde-first grief, first guilt." :
                    "The sorrow of Alqualonde passes over you-your spirit holds fast, blood unstained.";
                    break;
                case 1: echo_text = fail ?
                    "\"Ruin of Doriath\"\nAgain your hand recalls tragedy-fallen halls of Menegroth, Dior's blood shed beneath stolen starlight." :
                    "Memory of Doriath rises briefly, but your blade remains clean, honour upheld.";
                    break;
                case 2: echo_text = fail ?
                    "\"Tragedy at Sirion\"\nEchoes rise from Sirion-Elwing's flight, blood and betrayal. Once more your blade draws innocent blood, sealing doom anew." :
                    "You resist dark whispers recalling Sirion-your sword is stayed, mercy unbroken.";
                    break;
            }
            
            if (!fast_forward && !print_paragraph_fade(echo_text, fail ? TERM_RED : TERM_L_WHITE, current_row))
                fast_forward = true;
            else if (fast_forward)  print_paragraph(echo_text, fail ? TERM_RED : TERM_L_WHITE);
            
            current_row += 4; // Space for next echo
            
            /* Stop at first failure */
            if (fail) break;
        }

        if (kinslaying_victims > 0)
        {
            const char *doom_text = "Blood now stains your triumph, your fate forever woven with grief and shame.";
            if (!fast_forward && !print_paragraph_fade(doom_text, TERM_L_DARK, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(doom_text, TERM_L_DARK);
        }
        
        wait_prompt(PROMPT_CONCLUDE_TALE);
        Term_clear();
    }

    /* ============================================================= */
    /* SCENE 7: Final Summary                                       */
    /* ============================================================= */
    print_heading_fade("The Tale Concludes", TERM_YELLOW);
    
    char summary[256];
    strnfmt(summary, sizeof summary,
            "Your legend is written: %d Silmaril%s claimed, %s, %s.",
            final_sils,
            (final_sils == 1) ? "" : "s",
            treachery_occurred ? "tainted by treachery" : "pure of heart",
            (kinslaying_victims > 0) ? "stained by kinslaying" : "with honour intact");
    
    if (!fast_forward && !print_paragraph_fade(summary, TERM_L_GREEN, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(summary, TERM_L_GREEN);

    bool has_post_summary_scene = allow_kinslay && (kinslaying_victims > 0);

    Term_xtra(TERM_XTRA_DELAY, 3000);
    if (has_post_summary_scene)
        Term_clear();

    /* ============================================================= */
    /* SCENE 8: Kinslaying Execution & Notifications               */
    /* ============================================================= */
    if (has_post_summary_scene)
    {
        /* Show kinslaying notifications BEFORE screen_load() */
        print_heading_fade("The Price of Blood", TERM_RED);
        
        char kill_msg[128];
        strnfmt(kill_msg, sizeof kill_msg,
                "Your kinslaying echoes through time. %d innocent%s will fall by your hand...",
                kinslaying_victims, (kinslaying_victims == 1) ? "" : "s");
        
        if (!fast_forward && !print_paragraph_fade(kill_msg, TERM_RED, 4))
            fast_forward = true;
        else if (fast_forward)
            print_paragraph(kill_msg, TERM_RED);
        
        wait_prompt(PROMPT_WITNESS_CONSEQUENCES);
    }

    /* ------------------------------------------------------------- */
    /*  SCENE 8-bis: actual executions with cinematic feedback       */
    /* ------------------------------------------------------------- */
    if (has_post_summary_scene) {
        Term_clear();
        print_heading_fade("Blood Is Demanded", TERM_RED);

        int row = 4;
        for (int k = 0; k < 3; k++) {
            if (!deferred_kill[k]) continue;

            const char *character =
                kinslayer_try_kill(k + 1, /*do_roll=*/false);
            if (!character) continue;               /* should not happen */

            metarun_increment_deaths();
            log_info("Metarun: kinslaying victim counted as death (%u total)", (unsigned)metar.deaths);

            char buf[96];
            strnfmt(buf, sizeof buf,
                    "A hero %s has fallen beneath your blade.", character);

            if (!fast_forward && !print_paragraph_fade(buf, TERM_RED, row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(buf, TERM_RED);

            row += 3;
        }

        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    } else {
        /* no kinslaying scene - still give one clean exit prompt   */
        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    }

    metarun_gain_silmarils(final_sils);
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils, metar.silmarils);
    refresh_current_metar_score();
    print_story(3, true);

    /* Restore the saved play-screen only after every narrative beat */
    screen_load();

    compute_blessing_pool();
    announce_blessing_gain(blessing_points_before);
    blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    check_run_end();
    /* Save persistent settings when exiting */
    metarun_save_persistent_settings();
    
    /* Save metarun data (deaths, silmarils, etc.) */
    save_metaruns();
}


static int required_survivor_target(int win_goal)
{
    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required = 0;
    if (remaining_silmarils > 0) {
        required = (remaining_silmarils + 2) / 3;
        required += CURSE_GET(CUR_DEATH);
        if (required < 1) required = 1;
    }

    if (required < 0) required = 0;
    return required;
}


/* ======================  run-state logic  ====================== */
/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 *  Loss condition takes precedence over win condition.               *
 * ------------------------------------------------------------------ */
void check_run_end(void)
{
    int win_goal = WINCON_SILMARILS;   /* fallback */

    if (runtype_info && metar.type < z_info->rt_max)
    {
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
    }

    /* Keep blessing and survivor data aligned with the score file */
    compute_blessing_pool();
    int alive = metar.alive_characters;

    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required_survivors = required_survivor_target(win_goal);

    /* Loss takes precedence over victory */
    if (alive < required_survivors) {
        log_info("Metarun DEFEAT: alive=%d required=%d (remaining silmarils=%d)",
                 alive, required_survivors, remaining_silmarils);

        screen_save();
        Term_clear();

        print_heading_fade("The Trial's End", TERM_RED);

        char defeat_text[256];
        strnfmt(defeat_text, sizeof defeat_text,
                "Only %d hero%s remain, yet %d must endure to reclaim the remaining Silmarils. "
                "This tale falls into shadow; begin anew to kindle hope once more.",
                alive, (alive == 1) ? "" : "es",
                required_survivors);

        print_paragraph_fade(defeat_text, TERM_WHITE, 4);

        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();

        start_new_metarun();
        return;
    }

    if (metar.silmarils >= win_goal) {
        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, win_goal);
        screen_save();
        Term_clear();

        print_heading_fade("The Trial's End", TERM_YELLOW);

        char victory_text[256];
        strnfmt(victory_text, sizeof victory_text,
                "%d Silmarils reclaimed from Morgoth's crown! "
                "Hope kindles anew; your long trial approaches its end. "
                "Yet one final ordeal awaits: your ultimate destiny, "
                "as your true self faces the Last Trial.",
                win_goal);

        print_paragraph_fade(victory_text, TERM_L_GREEN, 4);

        const char *implementation_note = "(This final trial is yet to be implemented.)";
        print_paragraph_fade(implementation_note, TERM_L_DARK, 8);

        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();

        start_new_metarun();
    }
}





/* ------------------------------------------------------------------
 *  Start a brand-new meta-run.
 *  We snapshot the finished run **after** the array has been grown,
 *  so we only write once and always with the final pointer.
 * ------------------------------------------------------------------ */
static void start_new_metarun(void)
{
    log_info("Starting new metarun (previous run ID: %d)", metar.id);
    log_debug("metarun: pre-finalize state (wizard=%d, noscore=0x%04X, savefile='%s')",
              p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
              p_ptr ? (unsigned)p_ptr->noscore : 0,
              savefile);

    u32b previous_id = metar.id;
    if (!sync_current_metarun_slot(true)) {
        log_warn("metarun: unable to snapshot current run before rollover (idx=%d, max=%d)",
                 current_run, metarun_max);
    }

     /* Before wiping scores for the next run, backup and clear save files */
     backup_and_clear_saves();
     
     /* Before wiping scores for the next run, finalize current ones:
         - mark all alive entries as dead by their own hand
         - save any corresponding savefiles as dead
         Then archive/clear the score file so the next run starts clean. */
     metarun_finalize_scores_and_saves();
     clear_scorefile();

    /* Hard purge the current savefile if this was a noscore wizard/debug run */
    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008)) && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            bool deleted;
            safe_setuid_grab();
            deleted = fd_kill(savefile);
            safe_setuid_drop();
            if (deleted) {
                log_info("metarun: deleted noscore savefile '%s'", savefile);
            } else {
                log_warn("metarun: failed to delete noscore savefile '%s'", savefile);
            }
        }
    } else {
        log_info("metarun: purge skipped (wizard=%d, noscore=0x%04X, savefile='%s')",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0,
                 savefile);
    }
    /* Save old state */
    s16b old_max   = metarun_max;
    metarun *old   = metaruns;

    /* Try to allocate a new array for one more run */
    metarun *tmp = mem_alloc_array(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed - keep everything as is */
        return;
    }

    /* Copy over the previous runs (if any) */
    if (old) {
        memcpy(tmp, old, sizeof(metarun) * old_max);
    }

    /* Free the old array just once */
    old = mem_free(old);

    /* Commit the new array and size */
    metaruns    = tmp;
    metarun_max = old_max + 1;

    /* Initialize the brand-new slot */
    reset_defaults(&metaruns[metarun_max - 1]);
    metaruns[metarun_max - 1].id = previous_id + 1;
    metaruns[metarun_max - 1].type = 0; /* Default to type 0 (Normal) for new metaruns */

    /* Update globals */
    current_run      = metarun_max - 1;
    metar             = metaruns[current_run];
    metarun_created  = true;  /* Set flag to show story intro for new metarun */

    /* Apply difficulty curses based on the runtype */
    apply_difficulty_curses(&metar);

    /* Persist and prepare */
    save_metaruns();      /* safe now that metaruns!=NULL */ 
    ensure_run_dir(&metar);
    log_info("New metarun %d created and initialized", metar.id);
}

static int metarun_effect_wrap_width(int term_width)
{
    int wrap_width = term_width - 2;
    if (wrap_width < 5) wrap_width = 5;
    return wrap_width;
}

static int metarun_count_effect_lines(cptr text, int wrap_width, int indent)
{
    if (!text || !*text) return 0;

    if (sdl_is_story_font_enabled()) {
        return count_wrapped_lines_story(text, wrap_width, indent);
    }

    return count_wrapped_lines(text, wrap_width, indent);
}

static int metarun_active_effect_block_lines(int id, int term_width)
{
    const int text_col = 4;
    const int wrap_width = metarun_effect_wrap_width(term_width);
    const curse_type *cu = &cu_info[id];
    int stacks = CURSE_GET(id);
    bool is_blessing = (stacks < 0);
    bool seen = CURSE_SEEN(id);
    int lines = 1; /* Name */

    cptr desc = is_blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
        : (cu->text ? cu_text + cu->text : NULL);
    if (desc && *desc) {
        lines += metarun_count_effect_lines(desc, wrap_width, text_col);
    }

    if (seen) {
        cptr power = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);

        if (power && *power) {
            char effect_line[1024];
            strnfmt(effect_line, sizeof(effect_line), "Effect: %s", power);
            lines += metarun_count_effect_lines(effect_line, wrap_width, text_col);
        }
    } else {
        lines += 1;
    }

    lines += 1; /* Blank line between effects */
    return lines;
}

static int metarun_render_active_effect_block(int id, int row, int term_width)
{
    const int name_col = 2;
    const int text_col = 4;
    const int wrap_width = metarun_effect_wrap_width(term_width);
    int stacks = CURSE_GET(id);
    bool is_blessing = (stacks < 0);
    int magnitude = is_blessing ? -stacks : stacks;
    bool seen = CURSE_SEEN(id);

    const curse_type *cu = &cu_info[id];
    cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
    byte name_attr = is_blessing ? TERM_L_GREEN : TERM_L_RED;

    char buf[120];
    strnfmt(buf, sizeof(buf), "%s x%d", name, magnitude);
    Term_putstr(name_col, row++, -1, name_attr, buf);

    text_out_hook = text_out_to_screen;
    text_out_indent = text_col;
    text_out_wrap = wrap_width;

    cptr desc = is_blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
        : (cu->text ? cu_text + cu->text : NULL);
    if (desc && *desc) {
        Term_gotoxy(text_col, row);
        text_out_c(TERM_SLATE, desc);
        row += metarun_count_effect_lines(desc, wrap_width, text_col);
    }

    if (seen) {
        cptr power = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);

        if (power && *power) {
            char effect_line[1024];
            strnfmt(effect_line, sizeof(effect_line), "Effect: %s", power);
            Term_gotoxy(text_col, row);
            text_out_c(name_attr, effect_line);
            row += metarun_count_effect_lines(effect_line, wrap_width, text_col);
        }
    } else {
        Term_putstr(text_col, row++, -1, TERM_L_DARK, "(Effect not yet identified)");
    }

    row++;
    return row;
}

/* Show all active curses in a dedicated screen with pagination */
static void show_all_active_curses(void)
{
    int term_height, term_width;
    screen_save();
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
    }
    
    /* Get actual terminal dimensions */
    Term_get_size(&term_width, &term_height);
    
    /* Count active effects and build list */
    int active_count = 0;
    int active_ids[64];
    for (int id = 0; id < z_info->cu_max && active_count < 64; id++) {
        if (CURSE_GET(id) != 0) {
            active_ids[active_count++] = id;
        }
    }
    
    if (active_count == 0) {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== All Active Effects ===");
        Term_putstr(2, 3, -1, TERM_L_DARK, "No active curses or blessings");
        if (steamdeck) {
            char hint_buf[64];
            strnfmt(hint_buf, sizeof(hint_buf), "Press [%s] to return.", accept_label);
            Term_putstr(2, 5, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, 5, -1, TERM_L_DARK, "Press any key to return.");
        }
        inkey();
        screen_load();
        return;
    }
    
    int available_lines = term_height - 4;
    if (available_lines < 1) available_lines = 1;

    int page_starts[64];
    int total_pages = 0;
    int lines_used = 0;
    page_starts[0] = 0;

    for (int i = 0; i < active_count; i++) {
        int block_lines = metarun_active_effect_block_lines(active_ids[i], term_width);

        if (lines_used > 0 && lines_used + block_lines > available_lines) {
            total_pages++;
            page_starts[total_pages] = i;
            lines_used = 0;
        }

        lines_used += block_lines;
    }

    total_pages++;
    int current_page = 0;

    void (*old_text_out_hook)(byte, cptr) = text_out_hook;
    int old_text_out_indent = text_out_indent;
    int old_text_out_wrap = text_out_wrap;
    
    while (true) {
        Term_clear();
        
        /* Title with page info */
        char title_buf[80];
        if (total_pages > 1) {
            snprintf(title_buf, sizeof title_buf, "=== Active Effects (Page %d/%d) ===", 
                     current_page + 1, total_pages);
        } else {
            SDL_strlcpy(title_buf, "=== All Active Effects ===", sizeof title_buf);
        }
        Term_putstr(2, 1, -1, TERM_YELLOW, title_buf);
        
        int start_idx = page_starts[current_page];
        int end_idx = (current_page + 1 < total_pages) ? page_starts[current_page + 1] : active_count;
        
        int row = 3;
        for (int i = start_idx; i < end_idx; i++) {
            row = metarun_render_active_effect_block(active_ids[i], row, term_width);
        }
        
        /* Footer with navigation instructions */
        char footer_buf[100];
        char back_label[16] = "";
        if (steamdeck) {
            /* Steam Deck UI: A=ok, B=back */
            metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
            if (total_pages > 1) {
                snprintf(footer_buf, sizeof footer_buf,
                         "D-pad navigate  [%s] ok  [%s] back", accept_label, back_label);
            } else {
                snprintf(footer_buf, sizeof footer_buf,
                         "[%s] ok  [%s] back", accept_label, back_label);
            }
        } else {
            if (total_pages > 1) {
                snprintf(footer_buf, sizeof footer_buf, 
                         "Use arrows (left/right) to navigate. Any other key to return.");
            } else {
                SDL_strlcpy(footer_buf, "Press any key to return.", sizeof footer_buf);
            }
        }
        
        /* Ensure minimum 80 width for footer */
        size_t footer_len = strlen(footer_buf);
        if (footer_len < 80 && footer_len + 2 < sizeof footer_buf) {
            memset(footer_buf + footer_len, ' ', 80 - footer_len);
            footer_buf[80] = '\0';
        }
        
        Term_putstr(0, term_height - 1, -1, TERM_L_DARK, footer_buf);
        Term_fresh();
        
        /* Get input */
        char key = inkey();
        
        /* Arrow navigation: 6 = right, 4 = left (keypad directions) */
        if (total_pages > 1 && key == '6') {
            /* Next page */
            current_page = (current_page + 1) % total_pages;
        } else if (total_pages > 1 && key == '4') {
            /* Previous page */
            current_page = (current_page + total_pages - 1) % total_pages;
        } else if (steamdeck && key == steamdeck_back_key()) {
            /* B button = back in Steam Deck mode */
            break;
        } else if (steamdeck && (key == steamdeck_confirm_key() || key == '\r' || key == '\n')) {
            /* A button = confirm/close in Steam Deck mode */
            break;
        } else if (!steamdeck) {
            /* Exit on any key in non-Steam Deck mode */
            break;
        }
    }

    text_out_hook = old_text_out_hook;
    text_out_indent = old_text_out_indent;
    text_out_wrap = old_text_out_wrap;
    
    screen_load();
}

static int blessing_points_remaining(void)
{
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent;
    if (spent > earned) spent = earned;
    int available = earned - spent;
    if (available < 0) available = 0;
    return available;
}

static void blessing_spend_points(int cost)
{
    if (cost <= 0) return;
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent + cost;
    if (spent > earned) spent = earned;
    if (spent < 0) spent = 0;
    metar.blessing_points_spent = (u16b)spent;
}

static void blessing_commit_changes(bool apply_runtime)
{
    if (!sync_current_metarun_slot(false)) {
        log_warn("blessing_commit_changes: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
    if (apply_runtime) {
        metarun_apply_runtime_effects();
    }
    save_metaruns();
}

static bool blessing_remove_curse(char *result_msg, size_t msg_size, byte *result_attr)
{
    int ids[METAR_CURSE_SLOTS];
    int count = 0;

    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_CURSE_STACK(id) > 0) {
            if (count < METAR_CURSE_SLOTS) {
                ids[count++] = id;
            }
        }
    }

    if (count == 0) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "No curses cling to this saga.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    int selected = 0;
    int choice = -1;
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=cancel */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    
    /* Setup text wrapping */
    text_out_hook = text_out_to_screen;
    text_out_indent = 6;  /* Indent wrapped lines to match description column */
    int wrap_width = Term->wid - 8;  /* Leave margin for indentation */
    text_out_wrap = wrap_width;
    
    while (choice < 0) {
        screen_save();
        Term_clear();

        Term_putstr(2, 1, -1, TERM_YELLOW, "Remove a Curse (cost 1 blessing point)");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Choose which curse to lift:");

        int line = 5;
        for (int i = 0; i < count; i++) {
            int id = ids[i];
            curse_type *c = &cu_info[id];
            int stacks = CURSE_CURSE_STACK(id);
            /* Display curse name and stacks */
            char buf[128];
            if (steamdeck)
                snprintf(buf, sizeof buf, "   %-28s stacks: %d",
                         curse_display_name(id), stacks);
            else
                snprintf(buf, sizeof buf, "%c) %-28s stacks: %d",
                         'a' + i, curse_display_name(id), stacks);
            
            if (i == selected) {
                Term_putstr(2, line, -1, TERM_L_BLUE, ">");
                Term_putstr(4, line++, -1, TERM_L_RED, buf);
                
                /* Always show description (D:) with wrapping */
                if (c->text) {
                    cptr desc = cu_text + c->text;
                    Term_gotoxy(6, line);
                    text_out_c(TERM_L_WHITE, desc);
                    line += count_wrapped_lines(desc, wrap_width, 6);
                }
                
                /* Show power (P:) ONLY if curse is identified, with wrapping */
                bool is_seen = CURSE_SEEN(id);
                log_debug("blessing_remove_curse: curse %d (%s) seen=%d power=%d", 
                          id, curse_display_name(id), is_seen, (c->power != 0));
                if (is_seen && c->power) {
                    cptr power = cu_text + c->power;
                    Term_gotoxy(6, line);
                    text_out_c(TERM_SLATE, power);
                    line += count_wrapped_lines(power, wrap_width, 6);
                }
            } else {
                Term_putstr(2, line, -1, TERM_L_DARK, " ");
                Term_putstr(4, line++, -1, TERM_RED, buf);
            }
        }

        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to navigate  [%s] accept  [%s] cancel", accept_label, back_label);
            Term_putstr(2, line + 1, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, line + 1, -1, TERM_L_DARK,
                        "Arrows to navigate  Space/Enter accept  Letter select  Esc cancel");
        }
        char key = inkey();
        screen_load();

        /* Handle back/cancel - ESC or B button in Steam Deck mode */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            /* Reset text wrapping */
            text_out_wrap = 0;
            text_out_indent = 0;
            return false;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            choice = selected;
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + count - 1) % count;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % count;
            continue;
        }

        int idx = key - 'a';
        if (!steamdeck && idx >= 0 && idx < count) {
            choice = idx;
        } else if (!steamdeck && key >= 'A' && key <= 'Z') {
            idx = key - 'A';
            if (idx >= 0 && idx < count) {
                choice = idx;
            } else {
                bell("Invalid selection.");
            }
        } else {
            bell("Invalid selection.");
        }
    }

    int curse_id = ids[choice];
    int current_stacks = CURSE_CURSE_STACK(curse_id);
    
    /* Remove only one stack instead of all stacks */
    if (current_stacks > 1) {
        CURSE_SET(curse_id, current_stacks - 1);
    } else {
        CURSE_SET(curse_id, 0);
    }
    CURSE_SEEN_SET(curse_id);

    /* Reset text wrapping */
    text_out_wrap = 0;
    text_out_indent = 0;

    blessing_spend_points(1);
    
    /* Clear pending blessing choices when removing a curse */
    /* (removing a curse might make new blessings available) */
    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++) {
        metar.pending_blessing_choices[i] = 255;
    }
    
    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        if (current_stacks > 1) {
            snprintf(result_msg, msg_size, "One stack of %s is lifted. (%d remain%s)", 
                     curse_display_name(curse_id), 
                     current_stacks - 1,
                     (current_stacks - 1 == 1) ? "s" : "");
        } else {
            snprintf(result_msg, msg_size, "The curse of %s is lifted.", curse_display_name(curse_id));
        }
        if (result_attr) *result_attr = TERM_L_BLUE;
    }
    return true;
}

static bool blessing_gain_minor(char *result_msg, size_t msg_size, byte *result_attr)
{
    int options[3];
    int picks = 0;
    
    /* Check if we have pending choices that are still valid */
    bool have_valid_pending = false;
    if (metar.pending_blessing_count > 0) {
        /* Validate pending choices - make sure they're still eligible */
        for (int i = 0; i < metar.pending_blessing_count && i < 3; i++) {
            int id = metar.pending_blessing_choices[i];
            if (id == 255) continue; /* Empty slot */
            
            curse_type *c = &cu_info[id];
            if (!c->blessing_name) continue; /* No longer has blessing */
            
            int stacks = CURSE_GET(id);
            if (stacks > 0) continue; /* Currently cursed */
            
            int blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
                continue; /* At max */
            
            /* This pending choice is still valid */
            options[picks++] = id;
        }
        
        if (picks > 0) {
            have_valid_pending = true;
        }
    }
    
    /* If we don't have valid pending choices, generate new ones */
    if (!have_valid_pending) {
        int eligible[METAR_CURSE_SLOTS];
        int weights[METAR_CURSE_SLOTS];
        int count = 0;
        int total_weight = 0;

        /* Build list of eligible blessings with their weights */
        for (int id = 0; id < z_info->cu_max; id++) {
            curse_type *c = &cu_info[id];
            if (!c->blessing_name) continue;

            int stacks = CURSE_GET(id);
            if (stacks > 0) continue; /* currently cursed */

            int blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
                continue;

            if (count < METAR_CURSE_SLOTS) {
                eligible[count] = id;
                /* Apply weight with diminishing returns for existing stacks (same as curse system) */
                int base_weight = c->weight > 0 ? c->weight : 1;
                int effective_weight = base_weight / (blessing_stacks + 1);
                weights[count] = (effective_weight > 0) ? effective_weight : 1;  /* Minimum weight of 1 */
                total_weight += weights[count];
                count++;
            }
        }

        if (count == 0) {
            if (result_msg && msg_size > 0) {
                SDL_strlcpy(result_msg, "No blessings are presently available.", msg_size);
                if (result_attr) *result_attr = TERM_L_DARK;
            }
            return false;
        }

        /* Select up to 3 blessings using weighted random selection */
        picks = MIN(3, count);
        
        for (int i = 0; i < picks; i++) {
            /* Weighted random selection from remaining eligible blessings */
            int roll = rand_int(total_weight);
            int sum = 0;
            int selected = 0;
            
            for (int j = 0; j < count; j++) {
                sum += weights[j];
                if (roll < sum) {
                    selected = j;
                    break;
                }
            }
            
            options[i] = eligible[selected];
            
            /* Remove selected blessing from pool for next iteration */
            total_weight -= weights[selected];
            eligible[selected] = eligible[count - 1];
            weights[selected] = weights[count - 1];
            count--;
        }
        
        /* Store these choices as pending */
        metar.pending_blessing_count = picks;
        for (int i = 0; i < 3; i++) {
            if (i < picks) {
                metar.pending_blessing_choices[i] = options[i];
            } else {
                metar.pending_blessing_choices[i] = 255; /* Empty */
            }
        }
        save_metaruns();
    }

    int selected = 0;
    int choice = -1;
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=cancel */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    while (choice < 0) {
        screen_save();
        Term_clear();

        Term_putstr(2, 1, -1, TERM_YELLOW, "Receive a Blessing (cost 1 blessing point)");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Select a gift to accept:");

        int line = 5;
        for (int i = 0; i < picks; i++) {
            int id = options[i];
            curse_type *c = &cu_info[id];
            cptr name = blessing_display_name(id);
            char buf[160];
            if (steamdeck)
                snprintf(buf, sizeof buf, "   %-30s", name);
            else
                snprintf(buf, sizeof buf, "%c) %-30s", 'a' + i, name);
            if (i == selected) {
                Term_putstr(2, line, -1, TERM_L_BLUE, ">");
                Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
                
                /* Show both poetic description (E:) and mechanical effect (H:) for selected item */
                if (c->blessing_text) {
                    cptr desc = cu_text + c->blessing_text;
                    Term_putstr(6, line++, -1, TERM_L_WHITE, desc);
                }
                if (c->blessing_power) {
                    cptr power = cu_text + c->blessing_power;
                    Term_putstr(6, line++, -1, TERM_L_GREEN, power);
                }
            } else {
                Term_putstr(2, line, -1, TERM_L_DARK, " ");
                Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
            }
        }

        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to navigate  [%s] accept  [%s] cancel", accept_label, back_label);
            Term_putstr(2, line + 1, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, line + 1, -1, TERM_L_DARK,
                        "Arrows to navigate  Space/Enter accept  Letter select  Esc cancel");
        }
        char key = inkey();
        screen_load();

        /* Handle back/cancel - ESC or B button in Steam Deck mode */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            return false;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            choice = selected;
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + picks - 1) % picks;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % picks;
            continue;
        }

        int idx = key - 'a';
        if (!steamdeck && idx >= 0 && idx < picks) {
            choice = idx;
        } else if (!steamdeck && key >= 'A' && key <= 'Z') {
            idx = key - 'A';
            if (idx >= 0 && idx < picks) {
                choice = idx;
            } else {
                bell("Invalid selection.");
            }
        } else {
            bell("Invalid selection.");
        }
    }

    int blessing_id = options[choice];
    int stacks = CURSE_GET(blessing_id);
    int blessing_stacks = (stacks < 0) ? -stacks : 0;

    if (CURSE_BLESSING_CAP(blessing_id) > 0
        && blessing_stacks >= CURSE_BLESSING_CAP(blessing_id)) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "That blessing cannot grow any stronger.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    CURSE_ADD(blessing_id, -1);
    CURSE_SEEN_SET(blessing_id);

    blessing_spend_points(1);
    
    /* Clear pending choices after selection */
    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++) {
        metar.pending_blessing_choices[i] = 255;
    }
    
    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        snprintf(result_msg, msg_size, "You receive the %s.", blessing_display_name(blessing_id));
        if (result_attr) *result_attr = TERM_L_GREEN;
    }
    return true;
}

static bool blessing_unlock_major(char *result_msg, size_t msg_size, byte *result_attr)
{
    metarun_sanitize_major_blessing_bits(&metar);

    int cap = major_blessing_capacity();
    if (cap <= 0 || !mb_info) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "No major blessings are currently defined.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    struct {
        int idx;
        char key;
    } options[16];

    int option_count = 0;
    for (int i = 0; i < cap && option_count < 16; i++) {
        if (metarun_has_major_blessing_index(i)) continue;
        if (!major_blessing_def(i)) continue;
        options[option_count].idx = i;
        options[option_count].key = (char)('a' + option_count);
        option_count++;
    }

    if (option_count == 0) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "All major blessings are already sealed.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    int available = blessing_points_remaining();
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=cancel */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    
    /* Find first affordable option as initial selection */
    int selected = -1;
    for (int i = 0; i < option_count; i++) {
        int cost = major_blessing_cost(options[i].idx);
        if (cost <= available) {
            selected = i;
            break;
        }
    }
    
    /* If no affordable options, show message and return */
    if (selected < 0) {
        if (result_msg && msg_size > 0) {
            snprintf(result_msg, msg_size, "You need %d blessing points to unlock any major blessing.", 
                   major_blessing_cost(options[0].idx));
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }
    
    while (true) {
        screen_save();
        Term_clear();

        Term_putstr(2, 1, -1, TERM_YELLOW, "Unlock a Major Blessing");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Select which covenant to forge:");

        /* Recalculate in case something changed (shouldn't happen but safe) */
        available = blessing_points_remaining();
        
        int line = 5;
        for (int i = 0; i < option_count; i++) {
            int idx = options[i].idx;
            char key = options[i].key;
            const char *name = major_blessing_name_str(idx);
            const char *detail = major_blessing_detail_desc(idx);
            int cost = major_blessing_cost(idx);
            bool affordable = (cost <= available);

            char buf[160];
            if (steamdeck)
                snprintf(buf, sizeof buf, "   %s (cost %d)", name, cost);
            else
                snprintf(buf, sizeof buf, "%c) %s (cost %d)", key, name, cost);
            
            if (i == selected) {
                Term_putstr(2, line, -1, TERM_L_BLUE, ">");
                if (affordable) {
                    Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
                } else {
                    Term_putstr(4, line++, -1, TERM_L_DARK, buf);
                }
                
                /* Show description only for selected item */
                if (detail && *detail) {
                    byte desc_color = affordable ? TERM_L_WHITE : TERM_SLATE;
                    Term_putstr(6, line++, -1, desc_color, detail);
                }
            } else {
                Term_putstr(2, line, -1, TERM_L_DARK, " ");
                if (affordable) {
                    Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
                } else {
                    Term_putstr(4, line++, -1, TERM_L_DARK, buf);
                }
            }

            line++;
        }

        /* Show available points */
        char points_msg[80];
        snprintf(points_msg, sizeof points_msg, "Available blessing points: %d", available);
        Term_putstr(2, line++, -1, TERM_L_BLUE, points_msg);
        
        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to navigate  [%s] accept  [%s] cancel", accept_label, back_label);
            Term_putstr(2, line + 1, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, line + 1, -1, TERM_L_DARK,
                        "Arrows to navigate  Space/Enter accept  Letter select  Esc cancel");
        }

        char key = inkey();
        bool selected_from_confirm = false;
        screen_load();

        /* Handle back/cancel - ESC or B button in Steam Deck mode */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            return false;
        }

        if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            key = options[selected].key;
            selected_from_confirm = true;
        } else if (key == '8' || key == 'k' || key == '-') {
            /* Navigate up, skipping unaffordable options */
            int start = selected;
            do {
                selected = (selected + option_count - 1) % option_count;
                int cost = major_blessing_cost(options[selected].idx);
                if (cost <= available) break;
            } while (selected != start);
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            /* Navigate down, skipping unaffordable options */
            int start = selected;
            do {
                selected = (selected + 1) % option_count;
                int cost = major_blessing_cost(options[selected].idx);
                if (cost <= available) break;
            } while (selected != start);
            continue;
        }

        int choice_idx = -1;
        char lowered = tolower((unsigned char)key);
        if ((!steamdeck || selected_from_confirm) && lowered >= 'a' && lowered <= 'z') {
            for (int i = 0; i < option_count; i++) {
                if (lowered == options[i].key) {
                    int cost = major_blessing_cost(options[i].idx);
                    if (cost > available) {
                        bell("Not enough blessing points for that covenant.");
                        choice_idx = -2; /* Special marker for unaffordable */
                        break;
                    }
                    choice_idx = options[i].idx;
                    selected = i;
                    break;
                }
            }
        }

        if (choice_idx == -2) {
            /* Was unaffordable, already showed bell */
            continue;
        }
        
        if (choice_idx < 0) {
            bell("Invalid selection.");
            continue;
        }

        /* At this point choice is valid and affordable */
        int cost = major_blessing_cost(choice_idx);
        
        metar.major_blessings |= (1U << choice_idx);
        blessing_spend_points(cost);
        blessing_commit_changes(true);
        
        if (result_msg && msg_size > 0) {
            const char *msg = major_blessing_unlock_msg(choice_idx);
            if (msg && *msg) {
                SDL_strlcpy(result_msg, msg, msg_size);
            } else {
                snprintf(result_msg, msg_size, "You seal the %s.", major_blessing_name_str(choice_idx));
            }
            if (result_attr) *result_attr = TERM_YELLOW;
        }
        return true;
    }
}

static void open_blessing_exchange(void)
{
    bool done = false;
    int selected = 0;  /* Track highlighted option: 0=remove curse, 1=minor blessing, 2=major blessing */
    char status_msg[256] = "";
    byte status_attr = TERM_WHITE;
    bool clear_status_on_next_key = false;
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=back */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }

    while (!done) {
        compute_blessing_pool();
        int available = blessing_points_remaining();
        int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        int spent = metar.blessing_points_spent;

        bool major_available = false;
        int min_major_cost = INT_MAX;
        int major_cap = major_blessing_capacity();
        for (int i = 0; i < major_cap; i++) {
            if (metarun_has_major_blessing_index(i)) continue;
            if (!major_blessing_def(i)) continue;
            major_available = true;
            int cost = major_blessing_cost(i);
            if (cost < 0) cost = 0;
            if (cost < min_major_cost) min_major_cost = cost;
        }
        if (!major_available || min_major_cost == INT_MAX) {
            min_major_cost = 0;
        }
        
        /* Check if major blessing option is actually affordable */
        bool major_affordable = major_available && (min_major_cost <= available);

        int option_count = major_available ? 3 : 2;
        if (selected < 0) selected = 0;
        if (selected >= option_count) selected = option_count - 1;

        screen_save();
        Term_clear();

        Term_putstr(2, 1, -1, TERM_YELLOW, "Blessing Exchange");
        char buf[160];
        snprintf(buf, sizeof buf, "Blessing Points Available: %d (spent %d / earned %d)",
                 available, spent, earned);
        Term_putstr(2, 3, -1, TERM_L_WHITE, buf);

        /* Get blessing point threshold from runtype data */
        u32b threshold = metarun_threshold_value(&metar);
        if (threshold == 0) threshold = 1;
        
        snprintf(buf, sizeof buf, "Fallen Score Pool: %lu (progress %lu / %lu)",
                 (unsigned long)metar.fallen_score_total,
                 (unsigned long)metar.fallen_score_pool,
                 (unsigned long)threshold);
        Term_putstr(2, 4, -1, TERM_L_WHITE, buf);

        Term_putstr(2, 6, -1, TERM_L_GREEN, "Options:");
        
        /* Option 0: Remove curse */
        cptr marker0 = (selected == 0) ? ">" : " ";
        byte attr0 = (selected == 0) ? TERM_L_WHITE : TERM_WHITE;
        Term_putstr(2, 8, -1, TERM_L_BLUE, marker0);
        Term_putstr(4, 8, -1, attr0,
            steamdeck ? "Remove a curse (cost 1)"
                      : "r) Remove a curse (cost 1)");
        
        /* Option 1: Minor blessing */
        cptr marker1 = (selected == 1) ? ">" : " ";
        byte attr1 = (selected == 1) ? TERM_L_WHITE : TERM_WHITE;
        Term_putstr(2, 9, -1, TERM_L_BLUE, marker1);
        Term_putstr(4, 9, -1, attr1,
            steamdeck ? "Gain a minor blessing (cost 1)"
                      : "m) Gain a minor blessing (cost 1)");
        
        /* Option 2: Major blessing */
        if (major_available) {
            cptr marker2 = (selected == 2) ? ">" : " ";
            byte attr2;
            if (major_affordable) {
                attr2 = (selected == 2) ? TERM_L_WHITE : TERM_WHITE;
            } else {
                attr2 = TERM_L_DARK; /* Grey out if unaffordable */
            }
            snprintf(buf, sizeof buf, steamdeck
                     ? "Unlock a major blessing (cost %d)"
                     : "u) Unlock a major blessing (cost %d)",
                     min_major_cost);
            Term_putstr(2, 10, -1, TERM_L_BLUE, marker2);
            Term_putstr(4, 10, -1, attr2, buf);
        } else {
            Term_putstr(4,10, -1, TERM_L_DARK,
                steamdeck ? "Unlock a major blessing (none available)"
                          : "u) Unlock a major blessing (none available)");
        }
        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to navigate  [%s] accept  [%s] leave", accept_label, back_label);
            Term_putstr(2, 12, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, 12, -1, TERM_L_DARK,
                        "Arrows to navigate  Space/Enter accept  Letter select  ESC leave");
        }
        
        /* Display status message if present */
        if (status_msg[0] != '\0') {
            Term_putstr(2, 14, -1, status_attr, status_msg);
        }

        char key = inkey();
        bool selected_from_confirm = false;
        screen_load();
        
        /* Clear status message on navigation or if flagged */
        if (clear_status_on_next_key || key == '8' || key == 'k' || key == '-' || 
            key == '2' || key == 'j' || key == '+') {
            status_msg[0] = '\0';
            clear_status_on_next_key = false;
        }

        /* Handle navigation */
        if (key == '8' || key == 'k' || key == '-') {
            /* Navigate up, skipping unaffordable major blessing */
            int start = selected;
            do {
                selected = (selected + option_count - 1) % option_count;
                if (selected == 2 && !major_affordable) continue; /* Skip unaffordable major */
                break;
            } while (selected != start);
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            /* Navigate down, skipping unaffordable major blessing */
            int start = selected;
            do {
                selected = (selected + 1) % option_count;
                if (selected == 2 && !major_affordable) continue; /* Skip unaffordable major */
                break;
            } while (selected != start);
            continue;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            /* A button/Space/Enter activates highlighted option */
            if (selected == 0) key = 'r';
            else if (selected == 1) key = 'm';
            else if (selected == 2) key = 'u';
            selected_from_confirm = true;
        }

        /* Handle back/cancel - ESC, B button in Steam Deck mode, or 'h' key */
        if (key == ESCAPE || key == '4' || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            done = true;
            continue;
        }

        if (steamdeck && !selected_from_confirm) {
            bell("Use D-pad and confirm to select in this mode.");
            continue;
        }

        switch (key) {
        case 'r':
        case 'R':
            if (available < 1) {
                SDL_strlcpy(status_msg, "You need at least one blessing point to lift a curse.", sizeof(status_msg));
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
            } else if (blessing_remove_curse(status_msg, sizeof(status_msg), &status_attr)) {
                compute_blessing_pool();
                clear_status_on_next_key = true;
            } else {
                clear_status_on_next_key = true;
            }
            break;
        case 'm':
        case 'M':
            if (available < 1) {
                SDL_strlcpy(status_msg, "You need at least one blessing point to receive a gift.", sizeof(status_msg));
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
            } else if (blessing_gain_minor(status_msg, sizeof(status_msg), &status_attr)) {
                compute_blessing_pool();
                clear_status_on_next_key = true;
            } else {
                clear_status_on_next_key = true;
            }
            break;
        case 'u':
        case 'U':
            if (!major_available) {
                SDL_strlcpy(status_msg, "All major blessings have already been sealed.", sizeof(status_msg));
                status_attr = TERM_L_DARK;
                clear_status_on_next_key = true;
            } else if (!major_affordable) {
                snprintf(status_msg, sizeof(status_msg), "You need %d blessing points to unlock a major blessing.", min_major_cost);
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
            } else if (blessing_unlock_major(status_msg, sizeof(status_msg), &status_attr)) {
                compute_blessing_pool();
                clear_status_on_next_key = true;
            } else {
                clear_status_on_next_key = true;
            }
            break;
        default:
            bell("Unrecognised option.");
            break;
        }
    }
}

/*
 * Draw a simple blessing pool progress indicator.
 * Displays on the right side of the screen in light blue.
 */
static void draw_blessing_meter(int col, int start_row, int height, u32b current, u32b threshold)
{
    if (height < 5 || threshold == 0) return;
    
    /* Calculate fill percentage */
    int percent = (int)((current * 100) / threshold);
    if (percent > 100) percent = 100;
    
    /* Draw title */
    Term_putstr(col, start_row, -1, TERM_L_BLUE, "Blessing Pool");
    
    /* Draw top border */
    Term_putstr(col, start_row + 1, -1, TERM_L_BLUE, "+----------+");
    
    /* Draw the meter from bottom to top using simple ASCII */
    int meter_start = start_row + 2;
    int meter_end = start_row + height - 1;
    int meter_height = meter_end - meter_start;
    int filled_height = (meter_height * percent) / 100;
    
    for (int row = meter_start; row < meter_end; row++) {
        int rows_from_bottom = meter_end - row - 1;
        if (rows_from_bottom < filled_height) {
            /* Filled portion - use # for filled */
            Term_putstr(col, row, -1, TERM_L_BLUE, "|##########|");
        } else {
            /* Empty portion */
            Term_putstr(col, row, -1, TERM_L_BLUE, "|          |");
        }
    }
    
    /* Draw bottom border */
    Term_putstr(col, meter_end, -1, TERM_L_BLUE, "+----------+");
    
    /* Draw progress text below the meter */
    char progress_buf[20];
    snprintf(progress_buf, sizeof progress_buf, "%lu/%lu", 
             (unsigned long)current, (unsigned long)threshold);
    int text_col = col + (12 - (int)strlen(progress_buf)) / 2;
    if (text_col < col) text_col = col;
    Term_putstr(text_col, meter_end + 1, -1, TERM_L_BLUE, progress_buf);
}

static void metarun_truncate_for_width(char *buf, int max_width)
{
    if (!buf) return;
    if (max_width <= 0) {
        buf[0] = '\0';
        return;
    }

    int len = (int)strlen(buf);
    if (len <= max_width) return;

    if (max_width >= 4) {
        buf[max_width - 3] = '.';
        buf[max_width - 2] = '.';
        buf[max_width - 1] = '.';
        buf[max_width] = '\0';
    } else {
        buf[max_width] = '\0';
    }
}

static void metarun_put_prompt_line(int term_width, int term_height, byte attr, const char *text)
{
    if (term_width <= 0 || term_height <= 0) return;

    char line[512];
    int line_width = term_width;
    if (line_width > (int)sizeof(line) - 1) line_width = (int)sizeof(line) - 1;

    memset(line, ' ', line_width);
    line[line_width] = '\0';

    if (text && *text) {
        size_t tlen = strlen(text);
        if ((int)tlen > line_width) tlen = (size_t)line_width;
        memcpy(line, text, tlen);
    }

    Term_putstr(0, term_height - 1, -1, attr, line);
}

typedef struct {
    char variants[4][64];
    int variant_count;
    int variant_idx;
    bool enabled;
    int drop_priority;
} metarun_prompt_action;

static size_t metarun_render_action_prompt(const metarun_prompt_action *actions,
                                           int action_count,
                                           char *out,
                                           size_t out_size)
{
    if (!out || out_size == 0) return 0;

    out[0] = '\0';
    bool first = true;

    for (int i = 0; i < action_count; i++) {
        if (!actions[i].enabled) continue;
        if (actions[i].variant_idx < 0 || actions[i].variant_idx >= actions[i].variant_count) continue;

        if (!first) SDL_strlcat(out, "  ", out_size);
        SDL_strlcat(out, actions[i].variants[actions[i].variant_idx], out_size);
        first = false;
    }

    return strlen(out);
}

static void metarun_build_action_prompt(int term_width,
                                        bool steamdeck,
                                        const char *spend_label,
                                        const char *threshold_label,
                                        const char *diff_label,
                                        const char *full_label,
                                        const char *history_label,
                                        const char *blitz_label,
                                        bool blitz_enabled,
                                        char *out,
                                        size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    const char *spend = (spend_label && *spend_label) ? spend_label : "X";
    const char *thr = (threshold_label && *threshold_label) ? threshold_label : "R1";
    const char *diff = (diff_label && *diff_label) ? diff_label : "L1";
    const char *full = (full_label && *full_label) ? full_label : "Start";
    const char *hist = (history_label && *history_label) ? history_label : "Y";
    const char *blitz = (blitz_label && *blitz_label) ? blitz_label : "Back";

    metarun_prompt_action actions[6];
    memset(actions, 0, sizeof(actions));

    for (int i = 0; i < 6; i++) {
        actions[i].variant_count = 4;
        actions[i].variant_idx = 0;
        actions[i].enabled = (i < 5) ? true : blitz_enabled;
    }

    /* Lower number means dropped earlier if space is too tight. */
    actions[0].drop_priority = 5; /* Blessings */
    actions[1].drop_priority = 4; /* Threshold */
    actions[2].drop_priority = 3; /* Difficulty */
    actions[3].drop_priority = 2; /* Full list */
    actions[4].drop_priority = 1; /* History */
    actions[5].drop_priority = 4; /* Blitz */

    if (steamdeck) {
        strnfmt(actions[0].variants[0], sizeof(actions[0].variants[0]), "[%s] Spend blessings", spend);
        strnfmt(actions[0].variants[1], sizeof(actions[0].variants[1]), "[%s] Blessings", spend);
        strnfmt(actions[0].variants[2], sizeof(actions[0].variants[2]), "[%s] Bless", spend);
        strnfmt(actions[0].variants[3], sizeof(actions[0].variants[3]), "[%s]", spend);

        strnfmt(actions[1].variants[0], sizeof(actions[1].variants[0]), "[%s] Threshold", thr);
        strnfmt(actions[1].variants[1], sizeof(actions[1].variants[1]), "[%s] Thresh", thr);
        strnfmt(actions[1].variants[2], sizeof(actions[1].variants[2]), "[%s] Thr", thr);
        strnfmt(actions[1].variants[3], sizeof(actions[1].variants[3]), "[%s]", thr);

        strnfmt(actions[2].variants[0], sizeof(actions[2].variants[0]), "[%s] Difficulty", diff);
        strnfmt(actions[2].variants[1], sizeof(actions[2].variants[1]), "[%s] Diff", diff);
        strnfmt(actions[2].variants[2], sizeof(actions[2].variants[2]), "[%s] D", diff);
        strnfmt(actions[2].variants[3], sizeof(actions[2].variants[3]), "[%s]", diff);

        strnfmt(actions[3].variants[0], sizeof(actions[3].variants[0]), "[%s] Full list", full);
        strnfmt(actions[3].variants[1], sizeof(actions[3].variants[1]), "[%s] List", full);
        strnfmt(actions[3].variants[2], sizeof(actions[3].variants[2]), "[%s] L", full);
        strnfmt(actions[3].variants[3], sizeof(actions[3].variants[3]), "[%s]", full);

        strnfmt(actions[4].variants[0], sizeof(actions[4].variants[0]), "[%s] History", hist);
        strnfmt(actions[4].variants[1], sizeof(actions[4].variants[1]), "[%s] Hist", hist);
        strnfmt(actions[4].variants[2], sizeof(actions[4].variants[2]), "[%s] H", hist);
        strnfmt(actions[4].variants[3], sizeof(actions[4].variants[3]), "[%s]", hist);
 
        strnfmt(actions[5].variants[0], sizeof(actions[5].variants[0]), "[%s] Blitz", blitz);
        strnfmt(actions[5].variants[1], sizeof(actions[5].variants[1]), "[%s] Blitz", blitz);
        strnfmt(actions[5].variants[2], sizeof(actions[5].variants[2]), "[%s] Bz", blitz);
        strnfmt(actions[5].variants[3], sizeof(actions[5].variants[3]), "[%s]", blitz);

    } else {
        SDL_strlcpy(actions[0].variants[0], "[b] Spend blessings", sizeof(actions[0].variants[0]));
        SDL_strlcpy(actions[0].variants[1], "[b] Blessings", sizeof(actions[0].variants[1]));
        SDL_strlcpy(actions[0].variants[2], "[b] Bless", sizeof(actions[0].variants[2]));
        SDL_strlcpy(actions[0].variants[3], "[b]", sizeof(actions[0].variants[3]));

        SDL_strlcpy(actions[1].variants[0], "[f] Threshold", sizeof(actions[1].variants[0]));
        SDL_strlcpy(actions[1].variants[1], "[f] Thresh", sizeof(actions[1].variants[1]));
        SDL_strlcpy(actions[1].variants[2], "[f] Thr", sizeof(actions[1].variants[2]));
        SDL_strlcpy(actions[1].variants[3], "[f]", sizeof(actions[1].variants[3]));

        SDL_strlcpy(actions[2].variants[0], "[c] Difficulty", sizeof(actions[2].variants[0]));
        SDL_strlcpy(actions[2].variants[1], "[c] Diff", sizeof(actions[2].variants[1]));
        SDL_strlcpy(actions[2].variants[2], "[c] D", sizeof(actions[2].variants[2]));
        SDL_strlcpy(actions[2].variants[3], "[c]", sizeof(actions[2].variants[3]));

        SDL_strlcpy(actions[3].variants[0], "[u] Full list", sizeof(actions[3].variants[0]));
        SDL_strlcpy(actions[3].variants[1], "[u] List", sizeof(actions[3].variants[1]));
        SDL_strlcpy(actions[3].variants[2], "[u] L", sizeof(actions[3].variants[2]));
        SDL_strlcpy(actions[3].variants[3], "[u]", sizeof(actions[3].variants[3]));

        SDL_strlcpy(actions[4].variants[0], "[s] History", sizeof(actions[4].variants[0]));
        SDL_strlcpy(actions[4].variants[1], "[s] Hist", sizeof(actions[4].variants[1]));
        SDL_strlcpy(actions[4].variants[2], "[s] H", sizeof(actions[4].variants[2]));
        SDL_strlcpy(actions[4].variants[3], "[s]", sizeof(actions[4].variants[3]));

        SDL_strlcpy(actions[5].variants[0], "[x] Blitz", sizeof(actions[5].variants[0]));
        SDL_strlcpy(actions[5].variants[1], "[x] Blitz", sizeof(actions[5].variants[1]));
        SDL_strlcpy(actions[5].variants[2], "[x]", sizeof(actions[5].variants[2]));
        SDL_strlcpy(actions[5].variants[3], "[x]", sizeof(actions[5].variants[3]));
    }

    for (;;) {
        size_t len = metarun_render_action_prompt(actions, 6, out, out_size);
        if ((int)len <= term_width) break;

        int best_shrink = -1;
        int best_save = 0;
        for (int i = 0; i < 6; i++) {
            if (!actions[i].enabled) continue;
            if (actions[i].variant_idx + 1 >= actions[i].variant_count) continue;

            int cur_len = (int)strlen(actions[i].variants[actions[i].variant_idx]);
            int next_len = (int)strlen(actions[i].variants[actions[i].variant_idx + 1]);
            int save = cur_len - next_len;
            if (save > best_save) {
                best_save = save;
                best_shrink = i;
            }
        }

        if (best_shrink >= 0) {
            actions[best_shrink].variant_idx++;
            continue;
        }

        int drop_idx = -1;
        int drop_priority = INT_MAX;
        for (int i = 0; i < 6; i++) {
            if (!actions[i].enabled) continue;
            if (actions[i].drop_priority < drop_priority) {
                drop_priority = actions[i].drop_priority;
                drop_idx = i;
            }
        }

        if (drop_idx < 0) break;
        actions[drop_idx].enabled = false;
    }

    if (out[0] == '\0') {
        if (steamdeck) {
            strnfmt(out, out_size, "[%s]", spend);
        } else {
            SDL_strlcpy(out, blitz_enabled ? "[x]" : "[b]", out_size);
        }
    }
}

static void metarun_pick_best_variant(char *out,
                                      size_t out_size,
                                      int max_width,
                                      const char *v1,
                                      const char *v2,
                                      const char *v3,
                                      const char *v4)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    const char *variants[4] = { v1, v2, v3, v4 };
    const char *fallback = "";

    for (int i = 0; i < 4; i++) {
        const char *candidate = variants[i];
        if (!candidate || !*candidate) continue;
        fallback = candidate;
        if ((int)strlen(candidate) <= max_width) {
            SDL_strlcpy(out, candidate, out_size);
            return;
        }
    }

    SDL_strlcpy(out, fallback, out_size);
    metarun_truncate_for_width(out, max_width);
}

static void metarun_put_adaptive_line(int col,
                                      int *row,
                                      int term_width,
                                      byte attr,
                                      const char *v1,
                                      const char *v2,
                                      const char *v3,
                                      const char *v4)
{
    if (!row) return;
    int max_width = term_width - col - 1;
    if (max_width <= 0) return;

    char line[256];
    metarun_pick_best_variant(line, sizeof(line), max_width, v1, v2, v3, v4);
    Term_putstr(col, (*row)++, -1, attr, line);
}

/*
 * Enhanced print_metarun_stats():
 * - Draws a bracketed progress bar for Silmarils using '*'
 * - Renders deaths as a string of 'x' markers without a fixed limit
 * - Aligns labels & values for a cleaner layout
 * - Lists active curses with D: and (optionally) P: details
 * - Shows a blessing meter on the right side
 */
static void adjust_blessing_threshold_menu(void)
{
    const metarun_blessing_threshold_mode order[] = {
        METARUN_BLESSING_THRESHOLD_EASIER,
        METARUN_BLESSING_THRESHOLD_NORMAL,
        METARUN_BLESSING_THRESHOLD_HARDER
    };
    const char *labels[] = { "Easier", "Normal", "Harder" };
    const char *descs[] = {
        "If the game feels too hard, use this to earn blessings sooner.",
        "Default level.",
        "Pick this if you want fewer blessings by raising the threshold."
    };
    const int option_count = (int)N_ELEMENTS(order);

    if (current_run < 0 || current_run >= metarun_max) return;

    metarun_blessing_threshold_mode current_mode = metarun_get_threshold_mode(&metar);
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=back */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    int selection = 0;
    for (int i = 0; i < option_count; i++) {
        if (order[i] == current_mode) {
            selection = i;
            break;
        }
    }

    bool accepted = false;
    metarun_blessing_threshold_mode chosen_mode = current_mode;

    screen_save();

    while (true) {
        Term_clear();
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== Blessing Threshold ===");

        char buf[160];
        u32b current_threshold = metarun_threshold_value(&metar);
        snprintf(buf, sizeof buf, "Current: %s (%lu points per blessing)",
                 threshold_mode_name(current_mode), (unsigned long)current_threshold);
        Term_putstr(2, 3, -1, TERM_L_BLUE, buf);

        int row = 5;
        for (int i = 0; i < option_count; i++) {
            metarun_blessing_threshold_mode mode = order[i];
            u32b mode_threshold = runtype_threshold_for_mode(metar.type, mode);
            bool is_highlighted = (i == selection);
            bool is_current = (mode == current_mode);

            /* Color scheme: Green for Easier, White for Normal, Orange for Harder */
            byte base_color = (mode == METARUN_BLESSING_THRESHOLD_EASIER) ? TERM_L_GREEN :
                             (mode == METARUN_BLESSING_THRESHOLD_HARDER) ? TERM_ORANGE :
                             TERM_WHITE;

            char option_buf[80];
            if (steamdeck)
                snprintf(option_buf, sizeof option_buf, "%c  %s",
                         is_highlighted ? '>' : ' ', labels[i]);
            else
                snprintf(option_buf, sizeof option_buf, "%c%c) %s",
                         is_highlighted ? '>' : ' ', 'a' + i, labels[i]);

            byte name_attr = is_highlighted ? TERM_YELLOW : (is_current ? base_color : base_color);
            Term_putstr(2, row++, -1, name_attr, option_buf);

            snprintf(option_buf, sizeof option_buf, "    Requires %lu points per blessing",
                     (unsigned long)mode_threshold);
            byte threshold_attr = is_highlighted ? TERM_L_WHITE : TERM_L_DARK;
            Term_putstr(2, row++, -1, threshold_attr, option_buf);

            byte desc_attr = is_highlighted ? TERM_L_WHITE : TERM_SLATE;
            Term_putstr(4, row++, -1, desc_attr, descs[i]);
            row++;
        }

        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to choose  [%s] accept  [%s] cancel", accept_label, back_label);
            Term_putstr(2, row + 1, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, row + 1, -1, TERM_L_DARK,
                        "Use arrows or a/b/c to choose. Enter accepts, Esc cancels.");
        }

        char key = inkey();

        /* Handle back/cancel - ESC, B button in Steam Deck mode, or 'h' key */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            break;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            accepted = true;
            chosen_mode = order[selection];
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selection = (selection + option_count - 1) % option_count;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selection = (selection + 1) % option_count;
            continue;
        } else if (!steamdeck && key >= 'a' && key < 'a' + option_count) {
            selection = key - 'a';
            continue;
        } else if (!steamdeck && key >= 'A' && key < 'A' + option_count) {
            selection = key - 'A';
            continue;
        }
    }

    bool changed = false;
    u32b new_threshold = 0;

    if (accepted && chosen_mode != current_mode) {
        metarun_set_threshold_mode(&metar, chosen_mode);
        update_blessing_ledger(&metar);
        if (!sync_current_metarun_slot(false)) {
            log_warn("Threshold change: unable to sync metarun slot (idx=%d, max=%d)", current_run, metarun_max);
        }
        save_metaruns();
        changed = true;
        new_threshold = metarun_threshold_value(&metar);
    }

    if (accepted) {
        Term_clear();
        if (changed) {
            char msg[160];
            snprintf(msg, sizeof msg, "Blessing threshold set to %s.",
                     threshold_mode_name(chosen_mode));
            Term_putstr(2, 2, -1, TERM_L_GREEN, msg);
            snprintf(msg, sizeof msg, "New requirement: %lu points per blessing.",
                     (unsigned long)new_threshold);
            Term_putstr(2, 4, -1, TERM_WHITE, msg);
        } else {
            Term_putstr(2, 2, -1, TERM_L_DARK,
                        "Blessing threshold remains unchanged.");
        }
        if (steamdeck) {
            char hint_buf[64];
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
            strnfmt(hint_buf, sizeof(hint_buf), "Press [%s] to continue.", accept_label);
            Term_putstr(2, 6, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, 6, -1, TERM_L_DARK, "Press any key to continue.");
        }
        Term_fresh();
        (void)inkey();
    }

    screen_load();
}

/* Updated print_metarun_stats(): prettier layout, star & death bars, curses list */
void print_metarun_stats(void)
{
    int row = 1;
    int col = 2;
    char buf[160];
    int term_height, term_width;

    refresh_current_metar_score();

    if (current_run < 0 || current_run >= metarun_max) {
        screen_save();
        Term_clear();
        Term_putstr(2, 5, -1, TERM_RED, "Error: No metarun data available.");
        Term_putstr(2, 6, -1, TERM_L_WHITE, "Please start a new game first.");
        if (get_sdl_steamdeck_mode()) {
            char label[16];
            metarun_prompt_label(steamdeck_confirm_key(), "A", label, sizeof(label));
            strnfmt(buf, sizeof(buf), "Press %s to return.", label);
            Term_putstr(2, 8, -1, TERM_L_DARK, buf);
        } else {
            Term_putstr(2, 8, -1, TERM_L_DARK, "Press any key to return.");
        }
        inkey();
        screen_load();
        return;
    }

    compute_blessing_pool();
    metarun_sanitize_major_blessing_bits(&metar);

    const char *diff_name = "Unknown";
    int win_goal = WINCON_SILMARILS;

    if (runtype_info && metar.type < z_info->rt_max && runtype_info[metar.type].name[0])
    {
        diff_name = runtype_info[metar.type].name;
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
    }

    if (win_goal <= 0) win_goal = WINCON_SILMARILS;

    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    char sil_bar[32];
    build_symbol_bar(sil_bar, sizeof sil_bar, metar.silmarils, win_goal, '*');
    char death_marks[32];
    build_death_marks(death_marks, sizeof death_marks, metar.deaths);

    int required_survivors = required_survivor_target(win_goal);
    int alive = metar.alive_characters;

    u32b best_run = get_best_run_score_from_highscores();
    u32b total_pool = metar.fallen_score_total;
    u32b remainder = metar.fallen_score_pool;
    
    /* Get blessing point threshold from runtype data */
    u32b threshold = metarun_threshold_value(&metar);
    if (threshold == 0) threshold = 1;
    const char *threshold_mode = threshold_mode_name(metarun_get_threshold_mode(&metar));

    int earned_points = metar.blessing_points;
    int spent_points = metar.blessing_points_spent;
    int available_points = earned_points - spent_points;
    bool startup_scene = (!character_generated || !p_ptr || !p_ptr->playing);

    if (!startup_scene)
        screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();
    Term_get_size(&term_width, &term_height);
    bool steamdeck = get_sdl_steamdeck_mode();
    char spend_label[16] = "";
    char threshold_label[16] = "";
    char diff_label[16] = "";
    char full_label[16] = "";
    char history_label[16] = "";
    char back_label[16] = "";
    char continue_label[16] = "";
    char blitz_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=Continue, B=Back, X=Spend, Y=History,
         * L1=Diff, R1=Threshold, Start=Full list, RS Right=Blitz */
        int confirm_key = steamdeck_confirm_key();
        int back_key = steamdeck_back_key();
        int alt_key = steamdeck_alt_action_key();
        int secondary_key = steamdeck_secondary_key();
        int l1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        int r1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        int start_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_START);

        metarun_prompt_label(confirm_key, "A", continue_label, sizeof(continue_label));
        metarun_prompt_label(back_key, "B", back_label, sizeof(back_label));
        metarun_prompt_label(alt_key, "X", spend_label, sizeof(spend_label));
        metarun_prompt_label(secondary_key, "Y", history_label, sizeof(history_label));
        metarun_prompt_label(l1_key, "L1", diff_label, sizeof(diff_label));
        metarun_prompt_label(r1_key, "R1", threshold_label, sizeof(threshold_label));
        metarun_prompt_label(start_key, "Start", full_label, sizeof(full_label));
        metarun_prompt_label('x', "RS Right", blitz_label, sizeof(blitz_label));
    }

    bool blitz_enabled = (op_ptr && op_ptr->opt[OPT_unlock_blitz_mode]);

    bool full_layout = (term_width >= 80 && term_height >= 24);
    int meter_col = 0;

    /* Count major blessings once (used by both layouts) */
    int unlocked_major = 0;
    int major_total = metarun_major_blessing_count();
    for (int i = 0; i < major_total; i++) {
        if (metarun_has_major_blessing_index(i)) unlocked_major++;
    }

    if (full_layout) {
        /* Calculate blessing meter position (right side) */
        meter_col = term_width - 16;
        if (meter_col < 60) meter_col = 60; /* Keep some space for main content */
        if (meter_col > term_width - 13) meter_col = term_width - 13; /* "Blessing Pool" is 13 chars */

        int meter_height = 15;
        int max_meter_height = term_height - 7;
        if (max_meter_height < 5) max_meter_height = 5;
        if (meter_height > max_meter_height) meter_height = max_meter_height;

        /* Draw blessing meter on the right side */
        draw_blessing_meter(meter_col, 2, meter_height, remainder, threshold);

        Term_putstr(col, row++, -1, TERM_YELLOW, "=== Current Story Statistics ===");

        snprintf(buf, sizeof buf, "Run-ID         : %u", metar.id);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        snprintf(buf, sizeof buf, "Difficulty     : %s", diff_name);
        Term_putstr(col, row++, -1, TERM_L_BLUE, buf);

        snprintf(buf, sizeof buf, "Meta Score     : %lu", (unsigned long)metar.score);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        snprintf(buf, sizeof buf, "Best Run Score : %lu", (unsigned long)best_run);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        snprintf(buf, sizeof buf, "Silmarils      : %-22s %2d / %d (remaining %d)",
                 sil_bar, metar.silmarils, win_goal, remaining_silmarils);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        byte alive_attr = (alive < required_survivors) ? TERM_RED : TERM_L_GREEN;
        snprintf(buf, sizeof buf, "Living Heroes  : %d (need >= %d)", alive, required_survivors);
        Term_putstr(col, row++, -1, alive_attr, buf);

        snprintf(buf, sizeof buf, "Deaths         : %-22s (%d total)",
                 death_marks, metar.deaths);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        byte blessing_attr = (available_points > 0) ? TERM_L_GREEN : TERM_WHITE;
        snprintf(buf, sizeof buf, "Blessing Points: %d available (%d spent / %d earned)",
                 available_points, spent_points, earned_points);
        Term_putstr(col, row++, -1, blessing_attr, buf);

        if (steamdeck) {
            snprintf(buf, sizeof buf, "Blessing Pool  : %lu total (mode: %s, [%s] to change)",
                     (unsigned long)total_pool, threshold_mode, threshold_label);
        } else {
            snprintf(buf, sizeof buf, "Blessing Pool  : %lu total (mode: %s, press 'f' to change)",
                     (unsigned long)total_pool, threshold_mode);
        }
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        Term_putstr(col, row++, -1, TERM_YELLOW, "Major Blessings:");
        for (int i = 0; i < major_total; i++) {
            if (!metarun_has_major_blessing_index(i)) continue;
            const char *name = major_blessing_name_str(i);
            const char *desc = major_blessing_short_desc(i);
            char desc_buf[80];
            if (desc && *desc) {
                SDL_strlcpy(desc_buf, desc, sizeof desc_buf);
                char *nl = strchr(desc_buf, '\n');
                if (nl) *nl = '\0';
                snprintf(buf, sizeof buf, "  [X] %s (%s)", name, desc_buf);
            } else {
                snprintf(buf, sizeof buf, "  [X] %s", name);
            }
            Term_putstr(col, row++, -1, TERM_L_GREEN, buf);
        }
        if (unlocked_major == 0) {
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, "None unlocked yet");
        }

        row++; /* spacing before lists */

        Term_putstr(col, row++, -1, TERM_YELLOW, "Active Curses & Blessings:");
    } else {
        int compact_width = term_width - col - 1;
        if (compact_width < 10) compact_width = 10;
        int summary_row_limit = term_height - 4;

        metarun_put_adaptive_line(col, &row, term_width, TERM_YELLOW,
                                  "=== Story Statistics ===",
                                  "=== Story Stats ===",
                                  "== Story Stats ==",
                                  "== Stats ==");

        char line1[192], line2[192], line3[192], line4[192];

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Run-ID:%u  Difficulty:%s", metar.id, diff_name);
            strnfmt(line2, sizeof(line2), "ID:%u  Difficulty:%s", metar.id, diff_name);
            strnfmt(line3, sizeof(line3), "ID:%u  Diff:%s", metar.id, diff_name);
            strnfmt(line4, sizeof(line4), "ID:%u %s", metar.id, diff_name);
            metarun_put_adaptive_line(col, &row, term_width, TERM_L_BLUE, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Meta Score:%lu  Best Run:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            strnfmt(line2, sizeof(line2), "Meta:%lu  Best:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            strnfmt(line3, sizeof(line3), "Score:%lu  Best:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            strnfmt(line4, sizeof(line4), "M:%lu B:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            metarun_put_adaptive_line(col, &row, term_width, TERM_WHITE, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Silmarils:%d/%d (rem %d)  Alive:%d/%d", metar.silmarils, win_goal, remaining_silmarils, alive, required_survivors);
            strnfmt(line2, sizeof(line2), "Sil:%d/%d rem %d  Alive:%d/%d", metar.silmarils, win_goal, remaining_silmarils, alive, required_survivors);
            strnfmt(line3, sizeof(line3), "Sil:%d/%d  Alive:%d/%d", metar.silmarils, win_goal, alive, required_survivors);
            strnfmt(line4, sizeof(line4), "S:%d/%d A:%d/%d", metar.silmarils, win_goal, alive, required_survivors);
            byte sil_alive_attr = (alive < required_survivors) ? TERM_RED : TERM_L_GREEN;
            metarun_put_adaptive_line(col, &row, term_width, sil_alive_attr, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Deaths:%d  Blessing Points:%d (%d/%d)", metar.deaths, available_points, spent_points, earned_points);
            strnfmt(line2, sizeof(line2), "Deaths:%d  BPoints:%d (%d/%d)", metar.deaths, available_points, spent_points, earned_points);
            strnfmt(line3, sizeof(line3), "Deaths:%d  BP:%d", metar.deaths, available_points);
            strnfmt(line4, sizeof(line4), "D:%d BP:%d", metar.deaths, available_points);
            byte bp_attr = (available_points > 0) ? TERM_L_GREEN : TERM_WHITE;
            metarun_put_adaptive_line(col, &row, term_width, bp_attr, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            if (steamdeck) {
                strnfmt(line1, sizeof(line1), "Blessing Pool:%lu/%lu (%s, [%s] change)",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode, threshold_label);
                strnfmt(line2, sizeof(line2), "Pool:%lu/%lu  %s  [%s]",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode, threshold_label);
            } else {
                strnfmt(line1, sizeof(line1), "Blessing Pool:%lu/%lu (%s, press 'f')",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
                strnfmt(line2, sizeof(line2), "Pool:%lu/%lu  %s (f)",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
            }
            strnfmt(line3, sizeof(line3), "Pool:%lu/%lu  %s",
                    (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
            strnfmt(line4, sizeof(line4), "P:%lu/%lu", (unsigned long)remainder, (unsigned long)threshold);
            metarun_put_adaptive_line(col, &row, term_width, TERM_WHITE, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Major Blessings:%d", unlocked_major);
            strnfmt(line2, sizeof(line2), "Major:%d", unlocked_major);
            strnfmt(line3, sizeof(line3), "Major:%d", unlocked_major);
            strnfmt(line4, sizeof(line4), "M:%d", unlocked_major);
            metarun_put_adaptive_line(col, &row, term_width, TERM_YELLOW, line1, line2, line3, line4);
        }

        if (unlocked_major > 0 && row < summary_row_limit) {
            char majors_line[192];
            majors_line[0] = '\0';
            SDL_strlcpy(majors_line, "  ", sizeof(majors_line));
            bool first = true;
            for (int i = 0; i < major_total; i++) {
                if (!metarun_has_major_blessing_index(i)) continue;
                const char *name = major_blessing_name_str(i);
                char tmp[96];
                if (first) {
                    strnfmt(tmp, sizeof(tmp), "%s", name);
                    first = false;
                } else {
                    strnfmt(tmp, sizeof(tmp), ", %s", name);
                }
                if ((int)strlen(majors_line) + (int)strlen(tmp) > compact_width) break;
                SDL_strlcat(majors_line, tmp, sizeof(majors_line));
            }
            if (majors_line[2] != '\0') {
                metarun_truncate_for_width(majors_line, compact_width);
                Term_putstr(col, row++, -1, TERM_L_GREEN, majors_line);
            }
        }

        if (row < term_height - 2) {
            metarun_put_adaptive_line(col, &row, term_width, TERM_YELLOW,
                                      "Active Curses & Blessings:",
                                      "Curses & Blessings:",
                                      "Effects:",
                                      "Fx:");
        }
    }

    if (full_layout) {
        /* --- Full (>=80x24) layout: keep existing rendering exactly --- */

        /* Calculate max width for effect display (left side only, meter is separate) */
        int max_display_width = (meter_col > 60) ? meter_col - 4 : 56;

        int available_lines = term_height - row - 2;
        if (available_lines < 0) available_lines = 0;

        int active_count = 0;
        for (int id = 0; id < z_info->cu_max; id++) {
            if (CURSE_GET(id) != 0) active_count++;
        }

        bool curses_truncated = false;
        if (active_count == 0) {
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, "None active");
        } else if (available_lines <= 0) {
            curses_truncated = true;
            if (steamdeck) {
                snprintf(buf, sizeof buf, "List truncated - press [%s] to view all effects", full_label);
                Term_putstr(col + 2, row++, -1, TERM_L_DARK, buf);
            } else {
                Term_putstr(col + 2, row++, -1, TERM_L_DARK,
                            "List truncated - press 'u' to view all effects");
            }
        } else {
            int lines_remaining = available_lines;
            int entries_remaining = active_count;

            for (int id = 0; id < z_info->cu_max; id++) {
                int stacks = CURSE_GET(id);
                if (!stacks) continue;

                if (lines_remaining <= 0) {
                    curses_truncated = true;
                    break;
                }

                entries_remaining--;
                bool is_blessing = (stacks < 0);
                int magnitude = is_blessing ? -stacks : stacks;
                cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
                byte attr = is_blessing ? TERM_L_GREEN : TERM_RED;
                bool seen = CURSE_SEEN(id);

                const curse_type *cu = &cu_info[id];
                cptr effect = NULL;

                /* Only show H:/P: effect if identified */
                if (seen) {
                    effect = is_blessing
                        ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
                        : (cu->power ? cu_text + cu->power : NULL);
                }

                /* Format with proper alignment - shorten type labels and move closer to left */
                /* Use shortened labels: "Bless" and "Curse" instead of full words */
                if (effect && *effect) {
                    snprintf(buf, sizeof buf, "  %-28s %-5s %d - %s", name,
                             is_blessing ? "Bless" : "Curse", magnitude, effect);
                } else {
                    snprintf(buf, sizeof buf, "  %-28s %-5s %d", name,
                             is_blessing ? "Bless" : "Curse", magnitude);
                }

                /* Truncate if too long */
                if ((int)strlen(buf) > max_display_width) {
                    buf[max_display_width - 3] = '.';
                    buf[max_display_width - 2] = '.';
                    buf[max_display_width - 1] = '.';
                    buf[max_display_width] = '\0';
                }

                Term_putstr(col, row++, -1, attr, buf);
                lines_remaining--;

                if (lines_remaining <= 0 && entries_remaining > 0) {
                    curses_truncated = true;
                    break;
                }
            }

            if (curses_truncated && lines_remaining > 0) {
                if (entries_remaining > 0) {
                    if (steamdeck) {
                        snprintf(buf, sizeof buf, "... and %d more effect%s (press [%s] to view all)",
                                 entries_remaining, (entries_remaining == 1) ? "" : "s", full_label);
                    } else {
                        snprintf(buf, sizeof buf, "... and %d more effect%s (press 'u' to view all)",
                                 entries_remaining, (entries_remaining == 1) ? "" : "s");
                    }
                } else {
                    if (steamdeck) {
                        snprintf(buf, sizeof buf, "List truncated - press [%s] to view all effects", full_label);
                    } else {
                        SDL_strlcpy(buf, "List truncated - press 'u' to view all effects",
                                  sizeof buf);
                    }
                }
                Term_putstr(col, row++, -1, TERM_L_DARK, buf);
            }
        }

        /* Prompt line (full): dynamically packed to width */
        char prompt_buf[256];
        metarun_build_action_prompt(term_width, steamdeck,
                                    spend_label, threshold_label, diff_label,
                                    full_label, history_label, blitz_label,
                                    blitz_enabled,
                                    prompt_buf, sizeof(prompt_buf));
        metarun_truncate_for_width(prompt_buf, term_width);
        metarun_put_prompt_line(term_width, term_height, TERM_L_DARK, prompt_buf);
    } else {
        /* --- Compact layout --- */
        int max_display_width = term_width - col - 1;
        if (max_display_width < 10) max_display_width = 10;
        if (max_display_width > (int)sizeof(buf) - 1) max_display_width = (int)sizeof(buf) - 1;

        int available_lines = term_height - row - 2;
        if (available_lines < 0) available_lines = 0;

        int active_count = 0;
        for (int id = 0; id < z_info->cu_max; id++) {
            if (CURSE_GET(id) != 0) active_count++;
        }

        bool curses_truncated = false;
        if (active_count == 0) {
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, "None active");
        } else if (available_lines <= 0) {
            curses_truncated = true;
            if (steamdeck) {
                snprintf(buf, sizeof buf, "List truncated - press [%s] to view all effects", full_label);
                metarun_truncate_for_width(buf, term_width - col - 1);
                Term_putstr(col + 2, row++, -1, TERM_L_DARK, buf);
            } else {
                Term_putstr(col + 2, row++, -1, TERM_L_DARK,
                            "List truncated - press 'u' to view all effects");
            }
        } else {
            int lines_remaining = available_lines;
            int entries_remaining = active_count;
            bool show_effects = (max_display_width >= 36);
            int value_width = 4;
            int name_width = max_display_width - 2 - 1 - value_width; /* "  " + name + " " + value */
            if (show_effects) {
                int reserve_effect = max_display_width / 3;
                if (reserve_effect < 10) reserve_effect = 10;
                int with_effect = max_display_width - 2 - 1 - value_width - 3 - reserve_effect; /* " - " + effect */
                if (with_effect >= 8) name_width = with_effect;
            }
            if (name_width > 26) name_width = 26;
            if (name_width < 8) name_width = 8;

            for (int id = 0; id < z_info->cu_max; id++) {
                int stacks = CURSE_GET(id);
                if (!stacks) continue;

                if (lines_remaining <= 0) {
                    curses_truncated = true;
                    break;
                }

                entries_remaining--;
                bool is_blessing = (stacks < 0);
                int magnitude = is_blessing ? -stacks : stacks;
                cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
                byte attr = is_blessing ? TERM_L_GREEN : TERM_RED;
                bool seen = CURSE_SEEN(id);

                const curse_type *cu = &cu_info[id];
                cptr effect = NULL;

                if (seen && show_effects) {
                    effect = is_blessing
                        ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
                        : (cu->power ? cu_text + cu->power : NULL);
                }

                char sign = is_blessing ? '+' : '-';
                char value_buf[16];
                strnfmt(value_buf, sizeof(value_buf), "%c%d", sign, magnitude);

                if (effect && *effect && show_effects) {
                    snprintf(buf, sizeof buf, "  %-*.*s %*s - %s",
                             name_width, name_width, name,
                             value_width, value_buf, effect);
                } else {
                    snprintf(buf, sizeof buf, "  %-*.*s %*s",
                             name_width, name_width, name,
                             value_width, value_buf);
                }

                metarun_truncate_for_width(buf, max_display_width);
                Term_putstr(col, row++, -1, attr, buf);
                lines_remaining--;

                if (lines_remaining <= 0 && entries_remaining > 0) {
                    curses_truncated = true;
                    break;
                }
            }

            if (curses_truncated && lines_remaining > 0) {
                char line1[160], line2[160], line3[160], line4[160];
                if (entries_remaining > 0) {
                    if (steamdeck) {
                        snprintf(line1, sizeof line1, "... and %d more effects (press [%s] for full list)",
                                 entries_remaining, full_label);
                        snprintf(line2, sizeof line2, "... and %d more (press [%s] for list)",
                                 entries_remaining, full_label);
                        snprintf(line3, sizeof line3, "... %d more [%s]", entries_remaining, full_label);
                        snprintf(line4, sizeof line4, "... %d more", entries_remaining);
                    } else {
                        snprintf(line1, sizeof line1, "... and %d more effects (press 'u' for full list)", entries_remaining);
                        snprintf(line2, sizeof line2, "... and %d more (press 'u' for list)", entries_remaining);
                        snprintf(line3, sizeof line3, "... %d more (u)", entries_remaining);
                        snprintf(line4, sizeof line4, "... %d more", entries_remaining);
                    }
                } else {
                    if (steamdeck) {
                        snprintf(line1, sizeof line1, "List truncated - press [%s] to view all effects", full_label);
                        snprintf(line2, sizeof line2, "List truncated - press [%s]", full_label);
                        snprintf(line3, sizeof line3, "Truncated [%s]", full_label);
                        SDL_strlcpy(line4, "Truncated", sizeof(line4));
                    } else {
                        SDL_strlcpy(line1, "List truncated - press 'u' to view all effects", sizeof(line1));
                        SDL_strlcpy(line2, "List truncated - press 'u' for list", sizeof(line2));
                        SDL_strlcpy(line3, "Truncated (u)", sizeof(line3));
                        SDL_strlcpy(line4, "Truncated", sizeof(line4));
                    }
                }
                metarun_pick_best_variant(buf, sizeof(buf), term_width - col - 1,
                                          line1, line2, line3, line4);
                Term_putstr(col, row++, -1, TERM_L_DARK, buf);
            }
        }

        char prompt_buf[256];
        metarun_build_action_prompt(term_width, steamdeck,
                                    spend_label, threshold_label, diff_label,
                                    full_label, history_label, blitz_label,
                                    blitz_enabled,
                                    prompt_buf, sizeof(prompt_buf));
        metarun_truncate_for_width(prompt_buf, term_width);
        metarun_put_prompt_line(term_width, term_height, TERM_L_DARK, prompt_buf);
    }

    char key = inkey();
    if (steamdeck) {
        int back_key = steamdeck_back_key();
        int confirm_key = steamdeck_confirm_key();
        int alt_key = steamdeck_alt_action_key();
        int secondary_key = steamdeck_secondary_key();
        int l1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        int r1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        int start_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_START);
        
        if (key == back_key) {
            /* B button = exit/back */
            screen_pop_supporting_panes_hidden();
            if (!startup_scene)
                screen_load();
            return;
        } else if (key == confirm_key || key == '\r' || key == '\n') {
            /* A button = continue (exit) */
            screen_pop_supporting_panes_hidden();
            if (!startup_scene)
                screen_load();
            return;
        } else if (key == alt_key) {
            /* X button = spend blessings */
            key = 'b';
        } else if (key == secondary_key) {
            /* Y button = history */
            key = 's';
        } else if (key == l1_key) {
            /* L1 = difficulty */
            key = 'c';
        } else if (key == r1_key) {
            /* R1 = threshold */
            key = 'f';
        } else if (key == start_key) {
            /* Start = full list */
            key = 'u';
        }
    }
    if (key == 'b' || key == 'B') {
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        open_blessing_exchange();
        print_metarun_stats();
        return;
    } else if (key == 'c' || key == 'C') {
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        choose_difficulty_menu();
        return;
    } else if (key == 'f' || key == 'F') {
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        adjust_blessing_threshold_menu();
        print_metarun_stats();
        return;
    } else if (key == 'u' || key == 'U') {
        /* Show the full list of active curses/blessings separately */
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        show_all_active_curses();
        print_metarun_stats();
        return;
    } else if (key == 's' || key == 'S') {
        /* Show history only */
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        list_metaruns();
        print_metarun_stats();
        return;
    } else if ((key == 'x' || key == 'X') && blitz_enabled) {
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        run_mode_set_pending(RUN_MODE_BLITZ);
        run_mode_set_current(RUN_MODE_BLITZ);
        return;
    }

    screen_pop_supporting_panes_hidden();
    if (!startup_scene)
        screen_load();
}


/* Generate curse description for a runtype */
static void get_curse_description(int runtype_id, char *buf, size_t buf_size)
{
    if (!runtype_info || runtype_id >= z_info->rt_max || buf_size < 64)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    runtype_type *rt = &runtype_info[runtype_id];
    
    if (!rt->start_curses)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    /* Count curses and determine stack ranges */
    int curse_count = 0;
    int min_stacks = 255, max_stacks = 0;
    
    int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int curse_id = 0; curse_id < limit; curse_id++)
    {
        if (rt->start_curses & (1ULL << curse_id))
        {
            curse_count++;
            int stacks = rt->curse_stacks[curse_id];
            if (stacks < min_stacks) min_stacks = stacks;
            if (stacks > max_stacks) max_stacks = stacks;
        }
    }
    
    if (curse_count == 0)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    
    /* Format the description */
    if (min_stacks == max_stacks)
    {
        if (min_stacks == 1)
            snprintf(buf, buf_size, "Curses: %d x %d stack", curse_count, min_stacks);
        else
            snprintf(buf, buf_size, "Curses: %d x %d stacks", curse_count, min_stacks);
    }
    else
    {
        snprintf(buf, buf_size, "Curses: %d (%d-%d stacks)", curse_count, min_stacks, max_stacks);
    }
}

/* Difficulty selection menu */
static void choose_difficulty_menu(void)
{
    int choice = metar.type;  /* Start with current difficulty */
    int max_difficulty = (runtype_info && z_info->rt_max > 0) ? z_info->rt_max - 1 : 0;
    
    screen_save();
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=back */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    
    while (true)
    {
        Term_clear();

        /* Title */
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== Select Difficulty Level ===");
        
        int row = 3;
        for (int i = 0; i <= max_difficulty; i++)
        {
            byte name_color, desc_color;
            byte runtype_color = TERM_WHITE; /* default color */
            bool is_locked = (i < metar.max_difficulty_reached); /* Lock easier difficulties */
            
            /* Get runtype color from U: field */
            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0])
            {
                runtype_color = runtype_info[i].colour;
            }
            else
            {
                runtype_color = TERM_WHITE; /* fallback if runtype not loaded */
            }
            
            if (is_locked) {
                /* Locked (easier) difficulties - greyed out */
                name_color = TERM_L_DARK;
                desc_color = TERM_L_DARK;
                Term_putstr(2, row, -1, TERM_L_DARK, "-");
            }
            else if (i == choice) {
                /* Highlight selected difficulty - use runtype color but brighter */
                name_color = runtype_color;
                desc_color = TERM_L_WHITE;
                Term_putstr(2, row, -1, runtype_color, ">");
            } else if (i == metar.type) {
                /* Show current difficulty in its runtype color but dimmed */
                name_color = runtype_color;
                desc_color = TERM_SLATE;
                Term_putstr(2, row, -1, TERM_L_DARK, " ");
            } else {
                /* Normal difficulty in its runtype color */
                name_color = runtype_color;
                desc_color = TERM_L_DARK;
                Term_putstr(2, row, -1, TERM_L_DARK, " ");
            }
            
            /* Get dynamic name and stats from runtype */
            const char *rt_name = "Unknown";
            int win_goal = WINCON_SILMARILS;
            u32b blessing_thresh = runtype_threshold_for_mode(i, METARUN_BLESSING_THRESHOLD_NORMAL);
            
            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0])
            {
                rt_name = runtype_info[i].name;
                win_goal = runtype_info[i].win_con ? runtype_info[i].win_con : WINCON_SILMARILS;
            }
            
            char desc_buf[128];
            char curse_buf[64];
            get_curse_description(i, curse_buf, sizeof(curse_buf));
            
            if (is_locked) {
                snprintf(desc_buf, sizeof(desc_buf), "[LOCKED] Win: %d Silmarils, Threshold: %lu points, %s", 
                         win_goal, (unsigned long)blessing_thresh, curse_buf);
            } else {
                snprintf(desc_buf, sizeof(desc_buf), "Win: %d Silmarils, Threshold: %lu points, %s", 
                         win_goal, (unsigned long)blessing_thresh, curse_buf);
            }
            
            char name_buf[128];
            if (steamdeck) {
                if (is_locked)
                    snprintf(name_buf, sizeof(name_buf), "   %s [LOCKED]", rt_name);
                else
                    snprintf(name_buf, sizeof(name_buf), "   %s", rt_name);
            } else if (is_locked) {
                snprintf(name_buf, sizeof(name_buf), "%c) %s [LOCKED]", 'a'+i, rt_name);
            } else {
                snprintf(name_buf, sizeof(name_buf), "%c) %s", 'a'+i, rt_name);
            }
            
            Term_putstr(4, row++, -1, name_color, name_buf);
            Term_putstr(7, row++, -1, desc_color, desc_buf);
            
            /* Add extra spacing between options */
            row++;
        }
        
        /* Instructions */
        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to navigate  [%s] accept  [%s] cancel", accept_label, back_label);
            Term_putstr(2, row + 1, -1, TERM_L_WHITE, hint_buf);
        } else {
            Term_putstr(2, row + 1, -1, TERM_L_WHITE,
                        "Arrows to navigate     Space/Enter Accept     Esc Cancel");
        }
        
        /* Get input */
        char key = inkey();
        
        /* Handle back/cancel - ESC, B button in Steam Deck mode, or 'h' key */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H')))
        {
            screen_load();
            return;
        }
        else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6')  /* Enter/A button/6 key */
        {
            /* Check if trying to select a locked difficulty */
            if (choice < metar.max_difficulty_reached) {
                /* Show warning and stay in menu */
                Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, 2000);
                continue;
            }
            break;  /* Confirm selection */
        }
        else if (key == '8' || key == 'k' || key == '-')  /* Up */
        {
            /* Navigate up but skip locked difficulties */
            int new_choice = choice - 1;
            while (new_choice >= 0 && new_choice < metar.max_difficulty_reached) {
                new_choice--;
            }
            if (new_choice >= 0) choice = new_choice;
        }
        else if (key == '2' || key == 'j' || key == '+')  /* Down */
        {
            /* Navigate down normally */
            if (choice < max_difficulty) choice++;
        }
        else if (!steamdeck && key >= 'a' && key <= 'z')  /* Letter selection */
        {
            int new_choice = key - 'a';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    /* Show warning for locked difficulty */
                    Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                    Term_fresh();
                    Term_xtra(TERM_XTRA_DELAY, 2000);
                } else {
                    choice = new_choice;
                }
            }
        }
        else if (!steamdeck && key >= 'A' && key <= 'Z')  /* Capital letter selection */
        {
            int new_choice = key - 'A';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    /* Show warning for locked difficulty */
                    Term_putstr(2, row + 3, -1, TERM_RED, "Cannot select easier difficulty - locked for this story run!");
                    Term_fresh();
                    Term_xtra(TERM_XTRA_DELAY, 2000);
                } else {
                    choice = new_choice;
                }
            }
        }
    }
    
    /* Apply the new difficulty */
    if (choice != metar.type)
    {
        /* Warn if increasing difficulty */
        if (choice > metar.type) {
            int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
            bool portable = portable_controls_active();
            screen_save();
            Term_clear();
            Term_putstr(2, 5, -1, TERM_YELLOW, "WARNING: Increasing Difficulty");
            if (term_wid < 70)
            {
                Term_putstr(2, 7, term_wid - 2, TERM_WHITE,
                    "You cannot return to an easier level");
                Term_putstr(2, 8, term_wid - 2, TERM_WHITE,
                    "for the rest of this story run.");
            }
            else
            {
                Term_putstr(2, 7, -1, TERM_WHITE, "If you increase the difficulty level, you will NOT be able to");
                Term_putstr(2, 8, -1, TERM_WHITE, "go back to an easier level for the rest of this story run.");
            }
            Term_putstr(2, 10, -1, TERM_L_RED, "This change is PERMANENT for this meta-run!");
            if (steamdeck) {
                char prompt_buf[64];
                strnfmt(prompt_buf, sizeof(prompt_buf),
                        "Continue? [%s] yes  [%s] no", accept_label, back_label);
                Term_putstr(2, 12, term_wid - 2, TERM_L_WHITE, prompt_buf);
            } else {
                Term_putstr(2, 12, term_wid - 2, TERM_L_WHITE,
                    portable ? "Do you want to continue? (y/n/sp)"
                             : "Do you want to continue? (y/n)");
            }
            
            char confirm = inkey();
            screen_load();

            if (steamdeck) {
                if (confirm == steamdeck_confirm_key() || confirm == ' '
                    || confirm == '\r' || confirm == '\n')
                    confirm = 'y';
                else if (confirm == ESCAPE || confirm == steamdeck_back_key()
                    || confirm == 'h' || confirm == 'H')
                    confirm = 'n';
            } else if (portable
                && (confirm == ' ' || confirm == '\r' || confirm == '\n')) {
                confirm = 'y';
            }
            
            if (confirm != 'y' && confirm != 'Y') {
                return; /* Cancel the change */
            }
        }
        
        log_info("Changing difficulty from %d to %d", metar.type, choice);
        
        /* Preserve existing stacks and discovery state */
        int8_t preserved_stacks[METAR_CURSE_SLOTS];
        memcpy(preserved_stacks, metar.curse_stacks, sizeof(preserved_stacks));
        u64b preserved_seen = metar.curses_seen;

        /* Clear to baseline so we can reapply difficulty defaults */
        memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
        metar.curses_seen = 0;

        /* Set new type and apply its base curses */
        metar.type = (byte)choice;
        apply_difficulty_curses(&metar);

        /* Merge preserved stacks with new defaults (signed counts) */
        int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
        for (int curse_id = 0; curse_id < limit; curse_id++) {
            int preserved = preserved_stacks[curse_id];
            if (!preserved) continue;

            int combined = preserved + CURSE_GET(curse_id);
            int curse_cap = CURSE_CURSE_CAP(curse_id);
            int blessing_cap = CURSE_BLESSING_CAP(curse_id);
            if (curse_cap > 0 && combined > curse_cap)
                combined = curse_cap;
            if (blessing_cap > 0 && combined < -blessing_cap)
                combined = -blessing_cap;
            CURSE_SET(curse_id, combined);
        }

        /* Update maximum difficulty reached */
        if (choice > metar.max_difficulty_reached) {
            metar.max_difficulty_reached = (byte)choice;
        }

        /* Restore seen flags */
        metar.curses_seen |= preserved_seen;

        if (!sync_current_metarun_slot(false)) {
            log_warn("Difficulty change failed to sync metarun slot");
        }
        
        /* Save changes */
        save_metaruns();
        
        const char *new_name = "Unknown";
        if (runtype_info && choice < z_info->rt_max && runtype_info[choice].name[0])
            new_name = runtype_info[choice].name;
        
        msg_print(format("Difficulty changed to: %s", new_name));
    }
    
    screen_load();
    
    /* Return to metarun stats to show updated information */
    print_metarun_stats();
}

/* compact table of all meta-runs */
void list_metaruns(void)
{
    screen_save();
    bool steamdeck = get_sdl_steamdeck_mode();
    char accept_label[16] = "";
    int term_h = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int footer_row = term_h - 1;

    if (steamdeck) {
        /* Steam Deck UI: A=ok */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
    }
    Term_clear();
    c_prt(TERM_L_GREEN, "Meta-run history", 1, 2);
    c_put_str(TERM_L_DARK,
              " *ID      Score     Sil  Dth  Res  Last played", 3, 2);

    refresh_current_metar_score();

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    s16b *order = NULL;
    if (metarun_max > 0 && metaruns) {
        order = mem_alloc_array(metarun_max, s16b);
        for (s16b i = 0; i < metarun_max; i++) order[i] = i;
        qsort(order, metarun_max, sizeof(s16b), compare_metarun_indices);
    }

    int row = 4;
    for (s16b i = 0; i < metarun_max; i++) {
        s16b idx = order ? order[i] : i;
        const metarun *m = &metaruns[idx];

        /* Get dynamic win/loss conditions for this metarun type */
        int win_goal = WINCON_SILMARILS;
        int death_limit = LOSECON_DEATHS; /* hardcoded death limit for all runtypes */

        if (runtype_info && m->type < z_info->rt_max)
        {
            win_goal = runtype_info[m->type].win_con ? runtype_info[m->type].win_con : WINCON_SILMARILS;
        }

        char res = (m->silmarils >= win_goal) ? 'W' :
                   (m->deaths >= death_limit) ? 'L' : ' ';
        char date[16];
        strftime(date, sizeof date, "%Y-%m-%d",
                 localtime((time_t*)&m->last_played));

        byte attr = (idx == current_run) ? TERM_YELLOW : TERM_WHITE;
        char marker = (idx == current_run) ? '*' : ' ';

        c_put_str(attr,
                  format("%c%08u %8lu   %2d   %2d   %c   %s",
                         marker,
                         (unsigned)m->id,
                         (unsigned long)m->score,
                         m->silmarils, m->deaths, res, date),
                  row++, 2);

        if (row >= footer_row && i+1 < metarun_max) {   /* page break */
            if (steamdeck) {
                char hint_buf[64];
                strnfmt(hint_buf, sizeof(hint_buf), "[more - press %s]", accept_label);
                c_put_str(TERM_L_DARK, hint_buf, footer_row, 2);
            } else {
                c_put_str(TERM_L_DARK, "[more - any key]", footer_row, 2);
            }
            inkey();  Term_clear();
            row = 4;
            c_prt(TERM_L_GREEN, "Meta-run history (cont.)", 1, 2);
            c_put_str(TERM_L_DARK,
                      " *ID      Score     Sil  Dth  Res  Last played", 3, 2);
        }
    }

    order = mem_free(order);
    if (steamdeck) {
        char hint_buf[64];
        strnfmt(hint_buf, sizeof(hint_buf), "Press %s to return.", accept_label);
        c_put_str(TERM_L_DARK, hint_buf, MIN(row + 1, footer_row), 2);
    } else {
        c_put_str(TERM_L_DARK, "Press any key to return.",
            MIN(row + 1, footer_row), 2);
    }
    inkey();
    screen_load();
}

void show_known_curses_menu(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_CURSES);
}

/* Public wrapper for difficulty selection menu */
void choose_difficulty_level(void)
{
    choose_difficulty_menu();
}

/* ------------------------------------------------------------------ */
/*  Oath system tracking                                              */
/* ------------------------------------------------------------------ */

/*
 * Check if an oath is unlocked in the current metarun
 */
bool oath_unlocked(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].unlocked_oaths & oath_bit) != 0;
}

/*
 * Check if an oath is banned in the current metarun
 */
bool oath_banned(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].banned_oaths & oath_bit) != 0;
}

/*
 * Unlock an oath in the current metarun
 */
void metarun_unlock_oath(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath unlock: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath unlock: Invalid oath_id=%d", oath_id);
        return;
    }
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-based oath_id to bitmask */
    
    /* Update both the global metar and the metaruns array */
    metar.unlocked_oaths |= oath_bit;
    metaruns[current_run].unlocked_oaths |= oath_bit;
    
    log_trace("Oath unlock: Unlocked oath %d (bit %d) in metarun[%d], unlocked_oaths=0x%02X", 
              oath_id, oath_bit, current_run, metaruns[current_run].unlocked_oaths);
    
    /* Save immediately to persist the change */
    save_metaruns();
}

/*
 * Ban an oath in the current metarun (when broken)
 */
void metarun_ban_oath(int oath_id)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath ban: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath ban: Invalid oath_id=%d", oath_id);
        return;
    }
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-based oath_id to bitmask */
    
    /* Update both the global metar and the metaruns array */
    metar.banned_oaths |= oath_bit;
    metaruns[current_run].banned_oaths |= oath_bit;

    log_trace("Oath ban: Banned oath %d (bit %d) in metarun[%d], banned_oaths=0x%02X",
              oath_id, oath_bit, current_run, metaruns[current_run].banned_oaths);

    /* Save immediately to persist the change */
    refresh_current_metar_score();
    save_metaruns();
}

/*
 * Get bitmask of oaths available for selection (unlocked but not banned)
 */
int get_available_oaths_mask(void)
{
    if (blitz_oaths_enabled()) {
        int available = 0;
        int max_oath_id;

        if (!z_info)
            return 0;
        if (z_info->oath_max <= 1)
            return 0;

        max_oath_id = MIN(OATH_LIGHT, z_info->oath_max - 1);

        for (int i = 1; i <= max_oath_id; i++)
            available |= (1 << (i - 1));

        return available;
    }

    if (current_run < 0 || current_run >= metarun_max) return 0;
    
    byte unlocked = metaruns[current_run].unlocked_oaths;
    byte banned = metaruns[current_run].banned_oaths;
    byte available = unlocked & ~banned;
    
    log_trace("Oath availability: unlocked=0x%02X, banned=0x%02X, available=0x%02X", 
              unlocked, banned, available);
    
    return available;
}
