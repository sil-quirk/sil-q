/* --------------------------------------------------------------------
 *  src/metarun.c   (2025-07-06)   – final, crash-free, warning-free
 * --------------------------------------------------------------------
 *  Tracks a “meta-run” that ends after 15 Silmarils (win) or
 *  15 deaths (lose).  Finished runs are appended to meta.raw so
 *  the entire history is preserved.  Includes:
 *     • list_metaruns()  – compact history view
 *     • print_metarun_stats() – details for current run
 * -------------------------------------------------------------------- */
#include "angband.h"
#include "metarun.h"
#include "h-define.h"
#include "log.h"
#include "platform.h"    /* MKDIR helper                      */
#include "supplies.h"
#include <SDL3/SDL.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

/* Version structures for backward compatibility
 * v10 = 0.9.0.3 (current: expanded curse capacity, 64-bit known mask)
 * v9 = 0.9.0.2 (progressive scoring, reserved_runtime[32])
 * v8 = 0.9.0.1 (persistent blessing choices, reserved_runtime[1])
 * v7 = 0.9.0.0 (initial versioned, no blessing persistence)
 * v6 = pre-0.9.0 (curse_lo/hi instead of curse_stacks, score/best_run_score)
 * v5 = older (curse_lo/hi, no score fields)
 */

typedef struct metarun_v9 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    int8_t curse_stacks[32];
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[ANGBAND_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
    u32b fallen_score_total;
    u32b fallen_score_pool;
    s16b blessing_points;
    u16b blessing_points_spent;
    u16b major_blessings;
    byte alive_characters;
    byte pending_blessing_choices[3];
    byte pending_blessing_count;
    byte blessing_threshold_mode;
    byte reserved_runtime[31];
} metarun_v9;

typedef struct metarun_v8 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    int8_t curse_stacks[32];
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[ANGBAND_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
    u32b fallen_score_total;
    u32b fallen_score_pool;
    s16b blessing_points;
    u16b blessing_points_spent;
    u16b major_blessings;
    byte alive_characters;
    byte pending_blessing_choices[3];
    byte pending_blessing_count;
    byte reserved_runtime[1];
} metarun_v8;

typedef struct metarun_v7 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    int8_t curse_stacks[32];
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[ANGBAND_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
} metarun_v7;

typedef struct metarun_v6 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b score;
    u32b best_run_score;
    u32b curses_lo;
    u32b curses_hi;
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[ANGBAND_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
} metarun_v6;

typedef struct metarun_v5 {
    u32b id;
    byte type;
    byte deaths;
    byte silmarils;
    u32b last_played;
    u32b curses_lo;
    u32b curses_hi;
    u32b curses_seen;
    u32b persistent_options[8];
    byte persistent_delay_factor;
    byte persistent_hitpoint_warn;
    u32b persistent_window_flags[ANGBAND_TERM_MAX];
    byte persistent_options_initialized;
    u32b completed_quests;
    byte unlocked_oaths;
    byte banned_oaths;
    byte max_difficulty_reached;
    byte quest_reserved[12];
} metarun_v5;

#define METARUN_V9_SIZE (sizeof(metarun_v9))
#define METARUN_V8_SIZE (sizeof(metarun_v8))
#define METARUN_V7_SIZE (sizeof(metarun_v7))
#define METARUN_V6_SIZE (sizeof(metarun_v6))
#define METARUN_V5_SIZE (sizeof(metarun_v5))

#ifdef WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif  

/* --------------------------------------------------------------- */
/*  metarun.c : quick-and-dirty logger                             */
/* --------------------------------------------------------------- */

/* =========================  constants  ========================= */
#define CURSE_MENU_LINES  3

/* =========================  globals  =========================== */
static metarun *metaruns    = NULL;
static s16b     metarun_max = 0;
static s16b     current_run = 0;
bool            metarun_created = false;

/* ==================  tiny local helpers  ======================= */
static int rng_int(int max) { return max ? (int)(rand() % max) : 0; }

static int popcount32(u32b value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);
        count++;
    }
    return count;
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

static void clear_blessing_runtime_fields(metarun *m)
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

static void sanitize_major_blessing_bits(metarun *m)
{
    if (!m) return;
    u16b mask = major_blessing_mask();
    if (mask == 0) {
        m->major_blessings = 0;
    } else {
        m->major_blessings &= mask;
    }
}

static const major_blessing_type *major_blessing_def(int idx)
{
    if (!mb_info || !z_info) return NULL;
    if (idx < 0 || idx >= (int)z_info->mb_max) return NULL;
    return &mb_info[idx];
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

static void decode_legacy_curse_words(u32b lo, u32b hi, int8_t stacks[METAR_CURSE_SLOTS])
{
    if (!stacks) return;
    for (int id = 0; id < METAR_CURSE_SLOTS; ++id) stacks[id] = 0;
    for (int id = 0; id < 32; ++id) {
        u32b cnt = (id < 16)
                 ? ((lo >> (id * 2)) & 0x3U)
                 : ((hi >> ((id - 16) * 2)) & 0x3U);
        stacks[id] = (int8_t)cnt;
    }
}

static void metarun_from_v9(metarun *dst, const metarun_v9 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = src->score;
    dst->best_run_score = src->best_run_score;

    size_t stack_copy = MIN(sizeof(dst->curse_stacks), sizeof(src->curse_stacks));
    memcpy(dst->curse_stacks, src->curse_stacks, stack_copy);
    if (stack_copy < sizeof(dst->curse_stacks)) {
        memset(dst->curse_stacks + stack_copy, 0, sizeof(dst->curse_stacks) - stack_copy);
    }
    dst->curses_seen = (u64b)src->curses_seen;

    C_COPY(dst->persistent_options, src->persistent_options, 8, u32b);
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    C_COPY(dst->persistent_window_flags, src->persistent_window_flags, ANGBAND_TERM_MAX, u32b);
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;

    C_COPY(dst->quest_reserved, src->quest_reserved, 12, byte);

    dst->fallen_score_total = src->fallen_score_total;
    dst->fallen_score_pool = src->fallen_score_pool;
    dst->blessing_points = src->blessing_points;
    dst->blessing_points_spent = src->blessing_points_spent;
    dst->major_blessings = src->major_blessings;
    dst->alive_characters = src->alive_characters;

    C_COPY(dst->pending_blessing_choices, src->pending_blessing_choices, 3, byte);
    dst->pending_blessing_count = src->pending_blessing_count;
    dst->blessing_threshold_mode = src->blessing_threshold_mode;

    size_t runtime_copy = MIN(sizeof(dst->reserved_runtime), sizeof(src->reserved_runtime));
    if (runtime_copy > 0) {
        memcpy(dst->reserved_runtime, src->reserved_runtime, runtime_copy);
    }
    if (runtime_copy < sizeof(dst->reserved_runtime)) {
        memset(dst->reserved_runtime + runtime_copy, 0, sizeof(dst->reserved_runtime) - runtime_copy);
    }

    update_blessing_ledger(dst);
}

static void metarun_from_v8(metarun *dst, const metarun_v8 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = src->score;
    dst->best_run_score = src->best_run_score;

    size_t stack_copy = MIN(sizeof(dst->curse_stacks), sizeof(src->curse_stacks));
    memcpy(dst->curse_stacks, src->curse_stacks, stack_copy);
    if (stack_copy < sizeof(dst->curse_stacks)) {
        memset(dst->curse_stacks + stack_copy, 0, sizeof(dst->curse_stacks) - stack_copy);
    }
    dst->curses_seen = (u64b)src->curses_seen;

    C_COPY(dst->persistent_options, src->persistent_options, 8, u32b);
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    C_COPY(dst->persistent_window_flags, src->persistent_window_flags, ANGBAND_TERM_MAX, u32b);
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;
    
    /* Copy all quest_reserved bytes */
    C_COPY(dst->quest_reserved, src->quest_reserved, 12, byte);
    
    dst->fallen_score_total = src->fallen_score_total;
    dst->blessing_points_spent = src->blessing_points_spent;
    dst->major_blessings = src->major_blessings;
    dst->alive_characters = src->alive_characters;
    
    /* Copy persistent blessing choices (new in v8/0.9.0.1) */
    C_COPY(dst->pending_blessing_choices, src->pending_blessing_choices, 3, byte);
    dst->pending_blessing_count = src->pending_blessing_count;

    /* Copy reserved_runtime */
    size_t runtime_copy = MIN(sizeof(dst->reserved_runtime), sizeof(src->reserved_runtime));
    if (runtime_copy > 0) {
        memcpy(dst->reserved_runtime, src->reserved_runtime, runtime_copy);
    }
    if (runtime_copy < sizeof(dst->reserved_runtime)) {
        memset(dst->reserved_runtime + runtime_copy, 0, sizeof(dst->reserved_runtime) - runtime_copy);
    }

    update_blessing_ledger(dst);
}

static void metarun_from_v7(metarun *dst, const metarun_v7 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = src->score;
    dst->best_run_score = src->best_run_score;

    size_t stack_copy = MIN(sizeof(dst->curse_stacks), sizeof(src->curse_stacks));
    memcpy(dst->curse_stacks, src->curse_stacks, stack_copy);
    if (stack_copy < sizeof(dst->curse_stacks)) {
        memset(dst->curse_stacks + stack_copy, 0, sizeof(dst->curse_stacks) - stack_copy);
    }
    dst->curses_seen = (u64b)src->curses_seen;

    C_COPY(dst->persistent_options, src->persistent_options, 8, u32b);
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    C_COPY(dst->persistent_window_flags, src->persistent_window_flags, ANGBAND_TERM_MAX, u32b);
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;
    
    /* Copy all quest_reserved bytes */
    C_COPY(dst->quest_reserved, src->quest_reserved, 12, byte);

    update_blessing_ledger(dst);
}

static void metarun_from_v6(metarun *dst, const metarun_v6 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;
    dst->score = src->score;
    dst->best_run_score = src->best_run_score;

    decode_legacy_curse_words(src->curses_lo, src->curses_hi, dst->curse_stacks);
    dst->curses_seen = (u64b)src->curses_seen;

    C_COPY(dst->persistent_options, src->persistent_options, 8, u32b);
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    C_COPY(dst->persistent_window_flags, src->persistent_window_flags, ANGBAND_TERM_MAX, u32b);
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;
    
    /* Copy all quest_reserved bytes */
    C_COPY(dst->quest_reserved, src->quest_reserved, 12, byte);

    update_blessing_ledger(dst);
}

static void metarun_from_v5(metarun *dst, const metarun_v5 *src)
{
    if (!dst || !src) return;

    memset(dst, 0, sizeof(*dst));
    clear_blessing_runtime_fields(dst);

    dst->id = src->id;
    dst->type = src->type;
    dst->deaths = src->deaths;
    dst->silmarils = src->silmarils;
    dst->last_played = src->last_played;

    decode_legacy_curse_words(src->curses_lo, src->curses_hi, dst->curse_stacks);
    dst->curses_seen = (u64b)src->curses_seen;

    C_COPY(dst->persistent_options, src->persistent_options, 8, u32b);
    dst->persistent_delay_factor = src->persistent_delay_factor;
    dst->persistent_hitpoint_warn = src->persistent_hitpoint_warn;
    C_COPY(dst->persistent_window_flags, src->persistent_window_flags, ANGBAND_TERM_MAX, u32b);
    dst->persistent_options_initialized = src->persistent_options_initialized;

    dst->completed_quests = src->completed_quests;
    dst->unlocked_oaths = src->unlocked_oaths;
    dst->banned_oaths = src->banned_oaths;
    dst->max_difficulty_reached = src->max_difficulty_reached;
    
    /* Copy all quest_reserved bytes */
    C_COPY(dst->quest_reserved, src->quest_reserved, 12, byte);

    update_blessing_ledger(dst);
}

/* Calculate best run score from the high score table
 * All scores in the score file belong to the current metarun
 * This is kept for display purposes and historical tracking */
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
    
    int quest_count = popcount32(m->completed_quests);
    
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

static void refresh_current_metar_score(void)
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
        if (!path_build(buf, len, ANGBAND_DIR_APEX, name))
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
    if (!path_build(buf, len, ANGBAND_DIR_APEX, sub))
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
    clear_blessing_runtime_fields(m);
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
        FREE(metaruns);
        metaruns = NULL;
    }

    if (reason && *reason)
        log_warn("Metarun recovery triggered (%s); creating default entry", reason);
    else
        log_warn("Metarun recovery triggered; creating default entry");

    metarun_max = 1;
    metaruns = C_ZNEW(metarun_max, metarun);
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

/* Check if a file is in the new versioned format */
static bool is_versioned_meta_file(SDL_IOStream* fd, int file_size)
{
    if (file_size < sizeof(meta_file_header)) return false;

    meta_file_header header;
    sdl_seek(fd, 0);
    if (sdl_read(fd, (char*)&header, sizeof(header)) != 0) return false;

    /* Check for reasonable version numbers (0-255) and entry count */
    if (header.version_major > 255 || header.version_minor > 255 ||
        header.version_patch > 255 || header.version_extra > 255) return false;

    /* Check if the entry count makes sense with file size */
    size_t payload = file_size - sizeof(meta_file_header);
    if (header.entry_count == 0) {
        if (payload != 0) return false;
    } else {
        if ((payload % header.entry_count) != 0) return false;
        size_t entry_size = payload / header.entry_count;
        const size_t accepted[] = {
            sizeof(metarun),
            METARUN_V9_SIZE,
            METARUN_V8_SIZE,
            METARUN_V7_SIZE,
            METARUN_V6_SIZE,
            METARUN_V5_SIZE
        };
        bool supported = false;
        for (size_t i = 0; i < N_ELEMENTS(accepted); i++) {
            if (entry_size == accepted[i]) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            log_debug("is_versioned_meta_file: unsupported entry size %zu (payload %zu, entries %u)",
                      entry_size, payload, header.entry_count);
            return false;
        }
    }

    log_info("Detected versioned meta file: v%d.%d.%d, %u entries",
             header.version_major, header.version_minor, header.version_patch, header.entry_count);
    return true;
}

/*
 * Clean up old save and score files when starting fresh (no meta.raw exists)
 */
void cleanup_old_game_files(void)
{
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
}

errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    SDL_IOStream* fd;
    bool found_existing_data = false;

    if (!build_meta_path(fn, sizeof fn, NULL, META_RAW))
        return -1;
    fd = sdl_fopen(fn, "rb");

    if (!fd && ANGBAND_DIR_METARUN && ANGBAND_DIR_METARUN[0]) {
        char legacy[1024];
        if (path_build(legacy, sizeof legacy, ANGBAND_DIR_METARUN, META_RAW)) {
            fd = sdl_fopen(legacy, "rb");
            if (fd) {
                log_info("Loading legacy metarun file: %s", legacy);
                found_existing_data = true;
            }
        }
    }

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

    /* Check if this is a versioned file */
    Sint64 file_size_64 = sdl_size(fd);
    int file_size = (file_size_64 > 0) ? (int)file_size_64 : 0;
    bool is_versioned = is_versioned_meta_file(fd, file_size);
    
    const char *recovery_reason = NULL;

    if (is_versioned) {
        meta_file_header header;
        sdl_seek(fd, 0);
        if (sdl_read(fd, (char*)&header, sizeof(header)) != 0) {
            log_error("Failed to read metarun header");
            sdl_fclose(fd);
            return -1;
        }

        log_info("Loading versioned meta file v%d.%d.%d (%u entries)",
                 header.version_major, header.version_minor,
                 header.version_patch, header.entry_count);

        metarun_max = header.entry_count;
        size_t payload = (file_size >= (int)sizeof(meta_file_header))
                       ? (size_t)file_size - sizeof(meta_file_header)
                       : 0;
        size_t entry_size = (metarun_max > 0)
                          ? (payload / (size_t)metarun_max)
                          : 0;

        if (metarun_max > 0 && entry_size > 0) {
            metaruns = C_ZNEW(metarun_max, metarun);
            sdl_seek(fd, sizeof(meta_file_header));

            if (entry_size == sizeof(metarun)) {
                sdl_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
                for (s16b i = 0; i < metarun_max; i++) {
                    if (header.version_major == 0 && header.version_minor < 9) {
                        metaruns[i].blessing_points_spent = 0;
                    }
                    /* Initialize pending blessing choices for pre-0.9.0.1 saves
                     * (fields were part of reserved_runtime and may contain garbage) */
                    if (header.version_extra == 0) {
                        /* Clear pending choices - will be regenerated on first menu open */
                        metaruns[i].pending_blessing_count = 0;
                        for (int j = 0; j < 3; j++) {
                            metaruns[i].pending_blessing_choices[j] = 255;
                        }
                        log_debug("Cleared pending blessing choices for metarun %d (loaded from v0.9.0.0)", i);
                    }
                    update_blessing_ledger(&metaruns[i]);
                    sanitize_major_blessing_bits(&metaruns[i]);
                }
            } else if (entry_size == METARUN_V9_SIZE) {
                metarun_v9 *legacy = C_ZNEW(metarun_max, metarun_v9);
                sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v9));
                for (s16b i = 0; i < metarun_max; i++) {
                    metarun_from_v9(&metaruns[i], &legacy[i]);
                    sanitize_major_blessing_bits(&metaruns[i]);
                }
                FREE(legacy);
            } else if (entry_size == METARUN_V8_SIZE) {
                metarun_v8 *legacy = C_ZNEW(metarun_max, metarun_v8);
                sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v8));
                for (s16b i = 0; i < metarun_max; i++) {
                    metarun_from_v8(&metaruns[i], &legacy[i]);
                    sanitize_major_blessing_bits(&metaruns[i]);
                }
                FREE(legacy);
            } else if (entry_size == METARUN_V7_SIZE) {
                metarun_v7 *legacy = C_ZNEW(metarun_max, metarun_v7);
                sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v7));
                for (s16b i = 0; i < metarun_max; i++) {
                    metarun_from_v7(&metaruns[i], &legacy[i]);
                    sanitize_major_blessing_bits(&metaruns[i]);
                }
                FREE(legacy);
            } else if (entry_size == METARUN_V6_SIZE) {
                metarun_v6 *legacy = C_ZNEW(metarun_max, metarun_v6);
                sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v6));
                for (s16b i = 0; i < metarun_max; i++) {
                    metarun_from_v6(&metaruns[i], &legacy[i]);
                    sanitize_major_blessing_bits(&metaruns[i]);
                }
                FREE(legacy);
            } else if (entry_size == METARUN_V5_SIZE) {
                metarun_v5 *legacy = C_ZNEW(metarun_max, metarun_v5);
                sdl_read(fd, (char*)legacy, metarun_max * sizeof(metarun_v5));
                for (s16b i = 0; i < metarun_max; i++) {
                    metarun_from_v5(&metaruns[i], &legacy[i]);
                    sanitize_major_blessing_bits(&metaruns[i]);
                }
                FREE(legacy);
            } else {
                recovery_reason = "versioned meta.raw had unexpected entry size";
                log_warn("Unsupported metarun entry size %zu in versioned file", entry_size);
                FREE(metaruns);
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
            FREE(metaruns);
            metaruns = NULL;
            metarun_max = 0;
        }
    } else {
        /* Non-versioned meta.raw files are no longer supported */
        recovery_reason = "non-versioned meta.raw is no longer supported (requires v0.9.0+)";
        log_warn("Rejected non-versioned meta.raw file (size %d bytes) - please update from a versioned save", file_size);
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
 *  wrong – avoids dereferencing a freed/reallocated block.           *
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
    char *buffer = C_ZNEW(file_size, char);
    if (!buffer) {
        sdl_fclose(fd_src);
        return -1;
    }
    
    if (sdl_read(fd_src, buffer, file_size) != 0) {
        FREE(buffer);
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
        FREE(buffer);
        return -1;
    }
    
    errr result = sdl_write(fd_dst, buffer, file_size);
    sdl_fclose(fd_dst);
    FREE(buffer);
    
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
    sanitize_major_blessing_bits(&metar);
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
    /* Intended for CUR flags such as CUR_NOCHOICE. */
    return (curse_flag_count_cur(flag) > 0);
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
    sanitize_major_blessing_bits(&metar);

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
    sanitize_major_blessing_bits(&metar);
    if (idx < 0) return false;
    int cap = major_blessing_capacity();
    if (idx >= cap) return false;
    return (metar.major_blessings & (1U << idx)) != 0;
}

bool metarun_has_major_blessing_effect(metarun_major_effect effect)
{
    if (effect == METARUN_MAJOR_EFFECT_NONE) return false;
    sanitize_major_blessing_bits(&metar);
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
        
        if (word_idx < 8 && option_text[i] && op_ptr->opt[i]) {
            metar.persistent_options[word_idx] |= (1UL << bit_idx);
        }
    }
    
    /* Save special settings */
    metar.persistent_delay_factor = op_ptr->delay_factor;
    metar.persistent_hitpoint_warn = op_ptr->hitpoint_warn;
    
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
        
        if (word_idx < 8 && option_text[i]) {
            op_ptr->opt[i] = (metar.persistent_options[word_idx] & (1UL << bit_idx)) != 0;
        }
    }
    
    /* Load special settings */
    op_ptr->delay_factor = metar.persistent_delay_factor;
    op_ptr->hitpoint_warn = metar.persistent_hitpoint_warn;
    
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
                (c_info[p_ptr->phouse].flags & RHF_CURSE);

    /* Pass 1 – find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 – sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;           /* cap reached */

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rng_int(z_info->cu_max);    /* safety net */

    /* Pass 3 – roulette wheel */
    long pick = rng_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
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

    return rng_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (cu_info[idx].max_stacks &&
        CURSE_GET(idx) >= cu_info[idx].max_stacks)
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
    /* if any active curse has the "no‐choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES], sel;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }
            
            byte cap = cu_info[pick[i]].max_stacks;
            if (cap && CURSE_GET(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    screen_save();  Term_clear();
    
    /* Fade in the title */
    char str[60];
    const char* seq[] = { "a", "the second", "the third" };
    strnfmt(str, sizeof(str), "Dark powers demand their price - choose %s curse:", seq[n]);
    print_heading_fade(str, TERM_YELLOW);

    /* dynamic vertical layout – ask util.c to count wrapped lines   */
    int row = 4;                                     /* first free row */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 2;                   /* full width     */

    /* Show each curse one by one with fade-in effect */
    bool fast_forward = false;
    
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[pick[i]];
        char name_buf[128];
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
    c_put_str(TERM_L_DARK, "Arrows to navigate     Space/Enter Accept     a/b/c Select", row + 1, 2);
    
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
        strnfmt(highlighted_name_buf, sizeof highlighted_name_buf, "%c) %s", 'a'+highlight, cu_name + highlighted_cu->name);
        int cursor_col = 2 + strlen(highlighted_name_buf);
        Term_gotoxy(cursor_col, option_rows[highlight]);
        Term_fresh();
        char key = inkey();
        
        /* Handle input */
        if (key >= 'a' && key < 'a' + CURSE_MENU_LINES) {
            /* Letter shortcuts */
            sel = key - 'a';
            menu_done = true;
        }
        else if (key >= 'A' && key < 'A' + CURSE_MENU_LINES) {
            /* Capital letter shortcuts */
            sel = key - 'A';
            menu_done = true;
        }
        else if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
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
        else if (key == ESCAPE) {
            /* Escape - default to first option */
            sel = 0;
            menu_done = true;
        }
    }
    screen_load();
    return pick[sel];
}


/* ------------------------------------------------------------------ *
 *  Debug helper – wipe every active curse for the current meta-run.  *
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
 * Metarun narrative & exit logic - refactor **v4** (30 Jul 2025)
 * ------------------------------------------------------------------
 *  ✧ Re‑orders the sequence so NOTHING is overwritten:
 *      0. Escape‑curse chooser (UI)  → clears screen once finished.
 *      1. Chosen‑curse line(s).
 *      2. Victory banner & Silmaril count paragraph.
 *      3. Temptation of Treachery (escalating 1‑3 lines).
 *      4. Story Fragment (depends on Silmarils & Treachery flag).
 *      5. Echoes of Kinslaying (escalating 1‑3 lines)
 *      6. Final pause, then deferred side‑effects.
 *
 *  ✧ `choose_escape_curses_ui()` now **returns** the indices chosen and
 *    does NOT leave the menu clutter on screen. We re‑render the
 *    “The curse of X binds your fate.” lines after a clean clear.
 *
 *  ✧ Adds `print_story_fragment()` – a short narrative bridge keyed off
 *    Silmaril count (1‑3) and whether treachery was overcome.
 *
 *  ✧ Tested matrix: {treachery flag × kinslayer flag × silmarils (1‑3)}
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

/****************  Escape‑curse chooser (clean version) ************/

/*
 * Presents the menu *n* times (or once if CUR_NOCHOICE). Returns the
 * number of curses actually chosen and fills `out` with their indices.
 * The display is cleared afterwards so we can start narrative fresh.
 */
int choose_escape_curses_ui(int n, int out[3])
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
            (n == 1) ? "a" : (n == 2) ? "two" : "three",
            (n == 1) ? "" : "s");
    
    if (!print_paragraph_fade(intro_text, TERM_L_WHITE, 4))
        fast_forward = true;
    
    wait_for_keypress_with_prompt("[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay side‑effect */
        if (taken < 3) out[taken++] = idx;
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
/*  Standard “Press any key…” prompts – use enum, not raw strings     */
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
 * metarun_update_on_exit() – v5, 30 Jul 2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? gift‑of‑Eru?)
 *   1.  Escape‑curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls – stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1‑3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause → apply deferred effects
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
    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d, final_score=%ld", 
             died ? "true" : "false", escaped ? "true" : "false", sil_count, (long)final_score);
    int blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
             
    /* -------- Lineage flags -------------------------------------- */
    u32b f_house = c_info[p_ptr->phouse].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (f_house | f_race) & RHF_GIFTERU;
    bool allow_treachery = (f_house | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (f_house | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool fast_forward = false; // Track if user wants to skip fade effects
    bool morgoth_victory = (p_ptr->morgoth_slain && !escaped && !died);

    /* Treat as a death unless Eru intervenes */
    if (died && !has_gift_eru)
        metarun_increment_deaths();

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    – any path that reaches here counts as a "run end" event  */
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
        int *pool = C_ZNEW(z_info->st_max, int);
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
            if (st->st_type != 1)     continue;                /* not “death”   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback – allow any order-0 message if nothing matched.   */
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
            story_type *pick = &st_info[ pool[rng_int(pool_sz)] ];
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
        FREE(pool);
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
    /*        Enhanced Narrative Path – escaped with ≥1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative", sil_count);
    screen_save();

    /* ============================================================= */
    /* SCENE 1: Escape Curse Selection                              */
    /* ============================================================= */
    int chosen[3] = { -1, -1, -1 };
    int chosen_cnt = choose_escape_curses_ui(sil_count, chosen);

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
            /* One roll only – use kin_pct[] here and *skip* the roll
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

    Term_xtra(TERM_XTRA_DELAY, 3000);
    Term_clear();

    /* ============================================================= */
    /* SCENE 8: Kinslaying Execution & Notifications               */
    /* ============================================================= */
    if (allow_kinslay && kinslaying_victims > 0)
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
    if (allow_kinslay && kinslaying_victims > 0) {
        Term_clear();
        print_heading_fade("Blood Is Demanded", TERM_RED);

        int row = 4;
        for (int k = 0; k < 3; k++) {
            if (!deferred_kill[k]) continue;

            const char *house =
                kinslayer_try_kill(k + 1, /*do_roll=*/false);
            if (!house) continue;               /* should not happen */

            metarun_increment_deaths();
            log_info("Metarun: kinslaying victim counted as death (%u total)", (unsigned)metar.deaths);

            char buf[96];
            strnfmt(buf, sizeof buf,
                    "A hero %s has fallen beneath your blade.", house);

            if (!fast_forward && !print_paragraph_fade(buf, TERM_RED, row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(buf, TERM_RED);

            row += 3;
        }

        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    } else {
        /* no kinslaying scene – still give one clean exit prompt   */
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
    metarun *tmp = C_RNEW(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed - keep everything as is */
        return;
    }

    /* Copy over the previous runs (if any) */
    if (old) {
        C_COPY(tmp, old, old_max, metarun);
    }

    /* Free the old array just once */
    FREE(old);

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
    save_metaruns();      /* safe now that metaruns≠NULL */ 
    ensure_run_dir(&metar);
    log_info("New metarun %d created and initialized", metar.id);
}

/* Show all active curses in a dedicated screen with pagination */
static void show_all_active_curses(void)
{
    int term_height, term_width;
    screen_save();
    
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
        Term_putstr(2, 5, -1, TERM_L_DARK, "Press any key to return.");
        inkey();
        screen_load();
        return;
    }
    
    /* Calculate how many effects fit per page */
    int lines_per_effect = 4; /* name + description + power + blank */
    int header_lines = 4;
    int footer_lines = 2;
    int available_lines = term_height - header_lines - footer_lines;
    int effects_per_page = available_lines / lines_per_effect;
    if (effects_per_page < 1) effects_per_page = 1;
    
    int total_pages = (active_count + effects_per_page - 1) / effects_per_page;
    int current_page = 0;
    
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
        
        int start_idx = current_page * effects_per_page;
        int end_idx = start_idx + effects_per_page;
        if (end_idx > active_count) end_idx = active_count;
        
        int row = 3;
        for (int i = start_idx; i < end_idx; i++) {
            int id = active_ids[i];
            int stacks = CURSE_GET(id);
            bool is_blessing = (stacks < 0);
            int magnitude = is_blessing ? -stacks : stacks;
            bool seen = CURSE_SEEN(id);
            
            const curse_type *cu = &cu_info[id];
            cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
            byte name_attr = is_blessing ? TERM_L_GREEN : TERM_L_RED;
            
            /* Display name and magnitude */
            char buf[120];
            snprintf(buf, sizeof buf, "%s x%d", name, magnitude);
            Term_putstr(2, row++, -1, name_attr, buf);
            
            /* Always display description (D: or E:) */
            cptr desc = is_blessing 
                ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
                : (cu->text ? cu_text + cu->text : NULL);
            if (desc && *desc) {
                snprintf(buf, sizeof buf, "  %s", desc);
                /* Truncate if too long for terminal */
                if ((int)strlen(buf) > term_width - 2) {
                    buf[term_width - 5] = '.';
                    buf[term_width - 4] = '.';
                    buf[term_width - 3] = '.';
                    buf[term_width - 2] = '\0';
                }
                Term_putstr(2, row++, -1, TERM_SLATE, buf);
            }
            
            /* Display power (P: or H:) only if identified */
            if (seen) {
                cptr power = is_blessing
                    ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
                    : (cu->power ? cu_text + cu->power : NULL);
                if (power && *power) {
                    snprintf(buf, sizeof buf, "  Effect: %s", power);
                    if ((int)strlen(buf) > term_width - 2) {
                        buf[term_width - 5] = '.';
                        buf[term_width - 4] = '.';
                        buf[term_width - 3] = '.';
                        buf[term_width - 2] = '\0';
                    }
                    Term_putstr(2, row++, -1, is_blessing ? TERM_L_GREEN : TERM_L_RED, buf);
                }
            } else {
                Term_putstr(2, row++, -1, TERM_L_DARK, "  (Effect not yet identified)");
            }
            
            row++; /* Blank line between effects */
        }
        
        /* Footer with navigation instructions */
        char footer_buf[100];
        if (total_pages > 1) {
            snprintf(footer_buf, sizeof footer_buf, 
                     "Use arrows (left/right) to navigate. Any other key to return.");
        } else {
            SDL_strlcpy(footer_buf, "Press any key to return.", sizeof footer_buf);
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
        } else {
            /* Exit on any other key */
            break;
        }
    }
    
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
            char label = 'a' + i;
            
            /* Display curse name and stacks */
            char buf[128];
            snprintf(buf, sizeof buf, "%c) %-28s stacks: %d",
                     label, curse_display_name(id), stacks);
            
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

        Term_putstr(2, line + 1, -1, TERM_L_DARK,
                    "Arrows to navigate  Space/Enter accept  Letter select  Esc cancel");
        char key = inkey();
        screen_load();

        if (key == ESCAPE) {
            /* Reset text wrapping */
            text_out_wrap = 0;
            text_out_indent = 0;
            return false;
        } else if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
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
        if (idx >= 0 && idx < count) {
            choice = idx;
        } else if (key >= 'A' && key <= 'Z') {
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
            if (c->max_stacks > 0 && blessing_stacks >= c->max_stacks) continue; /* At max */
            
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
            if (c->max_stacks > 0 && blessing_stacks >= c->max_stacks) continue;

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
    while (choice < 0) {
        screen_save();
        Term_clear();

        Term_putstr(2, 1, -1, TERM_YELLOW, "Receive a Blessing (cost 1 blessing point)");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Select a gift to accept:");

        int line = 5;
        for (int i = 0; i < picks; i++) {
            int id = options[i];
            curse_type *c = &cu_info[id];
            char label = 'a' + i;

            cptr name = blessing_display_name(id);
            char buf[160];
            snprintf(buf, sizeof buf, "%c) %-30s", label, name);
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

        Term_putstr(2, line + 1, -1, TERM_L_DARK,
                    "Arrows to navigate  Space/Enter accept  Letter select  Esc cancel");
        char key = inkey();
        screen_load();

        if (key == ESCAPE) {
            return false;
        } else if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
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
        if (idx >= 0 && idx < picks) {
            choice = idx;
        } else if (key >= 'A' && key <= 'Z') {
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
    curse_type *c = &cu_info[blessing_id];
    int blessing_stacks = (stacks < 0) ? -stacks : 0;

    if (c->max_stacks > 0 && blessing_stacks >= c->max_stacks) {
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
    sanitize_major_blessing_bits(&metar);

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
        
        Term_putstr(2, line + 1, -1, TERM_L_DARK,
                    "Arrows to navigate  Space/Enter accept  Letter select  Esc cancel");

        char key = inkey();
        screen_load();

        if (key == ESCAPE) {
            return false;
        }

        if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            key = options[selected].key;
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
        if (lowered >= 'a' && lowered <= 'z') {
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
        Term_putstr(4, 8, -1, attr0, "r) Remove a curse (cost 1)");
        
        /* Option 1: Minor blessing */
        cptr marker1 = (selected == 1) ? ">" : " ";
        byte attr1 = (selected == 1) ? TERM_L_WHITE : TERM_WHITE;
        Term_putstr(2, 9, -1, TERM_L_BLUE, marker1);
        Term_putstr(4, 9, -1, attr1, "m) Gain a minor blessing (cost 1)");
        
        /* Option 2: Major blessing */
        if (major_available) {
            cptr marker2 = (selected == 2) ? ">" : " ";
            byte attr2;
            if (major_affordable) {
                attr2 = (selected == 2) ? TERM_L_WHITE : TERM_WHITE;
            } else {
                attr2 = TERM_L_DARK; /* Grey out if unaffordable */
            }
            snprintf(buf, sizeof buf, "u) Unlock a major blessing (cost %d)", min_major_cost);
            Term_putstr(2, 10, -1, TERM_L_BLUE, marker2);
            Term_putstr(4, 10, -1, attr2, buf);
        } else {
            Term_putstr(4,10, -1, TERM_L_DARK, "u) Unlock a major blessing (none available)");
        }
        Term_putstr(2, 12, -1, TERM_L_DARK, "Arrows to navigate  Space/Enter accept  Letter select  ESC leave");
        
        /* Display status message if present */
        if (status_msg[0] != '\0') {
            Term_putstr(2, 14, -1, status_attr, status_msg);
        }

        char key = inkey();
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
        } else if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            /* Space/Enter activates highlighted option */
            if (selected == 0) key = 'r';
            else if (selected == 1) key = 'm';
            else if (selected == 2) key = 'u';
        }

        switch (key) {
        case ESCAPE:
        case '4':
            done = true;
            break;
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

        Term_putstr(2, row + 1, -1, TERM_L_DARK,
                    "Use arrows or a/b/c to choose. Enter accepts, Esc cancels.");

        char key = inkey();

        if (key == ESCAPE) {
            break;
        } else if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            accepted = true;
            chosen_mode = order[selection];
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selection = (selection + option_count - 1) % option_count;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selection = (selection + 1) % option_count;
            continue;
        } else if (key >= 'a' && key < 'a' + option_count) {
            selection = key - 'a';
            continue;
        } else if (key >= 'A' && key < 'A' + option_count) {
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
        Term_putstr(2, 6, -1, TERM_L_DARK, "Press any key to continue.");
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
        Term_putstr(2, 8, -1, TERM_L_DARK, "Press any key to return.");
        inkey();
        screen_load();
        return;
    }

    compute_blessing_pool();
    sanitize_major_blessing_bits(&metar);

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

    screen_save();
    Term_clear();
    Term_get_size(&term_width, &term_height);
    
    /* Ensure minimum 80 width for layout */
    int effective_width = (term_width < 80) ? 80 : term_width;
    
    /* Calculate blessing meter position (right side) */
    int meter_col = effective_width - 16;
    if (meter_col < 60) meter_col = 60; /* Keep some space for main content */
    int meter_height = (term_height > 20) ? 15 : (term_height - 5);
    if (meter_height < 5) meter_height = 5;

    /* Draw blessing meter on the right side */
    u32b progress = remainder;
    if (threshold == 0) threshold = 1;
    draw_blessing_meter(meter_col, 2, meter_height, progress, threshold);

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

    snprintf(buf, sizeof buf, "Blessing Pool  : %lu total (mode: %s, press 'f' to change)",
             (unsigned long)total_pool, threshold_mode);
    Term_putstr(col, row++, -1, TERM_WHITE, buf);

    Term_putstr(col, row++, -1, TERM_YELLOW, "Major Blessings:");
    int unlocked_major = 0;
    int major_total = metarun_major_blessing_count();
    for (int i = 0; i < major_total; i++) {
        if (!metarun_has_major_blessing_index(i)) continue;
        unlocked_major++;
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
        Term_putstr(col + 2, row++, -1, TERM_L_DARK,
                    "List truncated - press 'u' to view all effects");
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
                snprintf(buf, sizeof buf, "... and %d more effect%s (press 'u' to view all)",
                         entries_remaining, (entries_remaining == 1) ? "" : "s");
            } else {
                SDL_strlcpy(buf, "List truncated - press 'u' to view all effects",
                          sizeof buf);
            }
            Term_putstr(col, row++, -1, TERM_L_DARK, buf);
        }
    }

    char prompt_buf[160];
    const char *base_prompt = "[b] Spend blessings  [f] Threshold  [c] Difficulty  [u] Full list  [s] History";
    
    /* Pad to terminal width (minimum 80) */
    int target_width = (term_width > 80) ? term_width : 80;
    snprintf(prompt_buf, sizeof prompt_buf, "%s", base_prompt);
    size_t plen = strlen(prompt_buf);
    
    if ((int)plen < target_width && plen + 2 < sizeof prompt_buf) {
        int pad_amount = target_width - (int)plen;
        if (pad_amount > (int)(sizeof prompt_buf - plen - 1)) {
            pad_amount = sizeof prompt_buf - plen - 1;
        }
        memset(prompt_buf + plen, ' ', pad_amount);
        prompt_buf[plen + pad_amount] = '\0';
    }

    Term_putstr(0, term_height - 1, -1, TERM_L_DARK, prompt_buf);

    char key = inkey();
    if (key == 'b' || key == 'B') {
        screen_load();
        open_blessing_exchange();
        print_metarun_stats();
        return;
    } else if (key == 'c' || key == 'C') {
        screen_load();
        choose_difficulty_menu();
        return;
    } else if (key == 'f' || key == 'F') {
        screen_load();
        adjust_blessing_threshold_menu();
        print_metarun_stats();
        return;
    } else if (key == 'u' || key == 'U') {
        /* Show the full list of active curses/blessings separately */
        screen_load();
        show_all_active_curses();
        print_metarun_stats();
        return;
    } else if (key == 's' || key == 'S') {
        /* Show history only */
        screen_load();
        list_metaruns();
        print_metarun_stats();
        return;
    }

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
            if (is_locked) {
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
        Term_putstr(2, row + 1, -1, TERM_L_WHITE, "Arrows to navigate     Space/Enter Accept     Esc Cancel");
        
        /* Get input */
        char key = inkey();
        
        /* Handle input */
        if (key == ESCAPE) 
        {
            screen_load();
            return;
        }
        else if (key == '\r' || key == '\n' || key == ' ' || key == '6')  /* Enter/Space/6 key */
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
        else if (key >= 'a' && key <= 'z')  /* Letter selection */
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
        else if (key >= 'A' && key <= 'Z')  /* Capital letter selection */
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
            screen_save();
            Term_clear();
            Term_putstr(2, 5, -1, TERM_YELLOW, "WARNING: Increasing Difficulty");
            Term_putstr(2, 7, -1, TERM_WHITE, "If you increase the difficulty level, you will NOT be able to");
            Term_putstr(2, 8, -1, TERM_WHITE, "go back to an easier level for the rest of this story run.");
            Term_putstr(2, 10, -1, TERM_L_RED, "This change is PERMANENT for this meta-run!");
            Term_putstr(2, 12, -1, TERM_L_WHITE, "Do you want to continue? (y/n)");
            
            char confirm = inkey();
            screen_load();
            
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
            int max_allowed = cu_info[curse_id].max_stacks;
            if (max_allowed > 0) {
                if (combined > max_allowed) combined = max_allowed;
                if (combined < -max_allowed) combined = -max_allowed;
            }
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
        order = C_ZNEW(metarun_max, s16b);
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

        if (row >= 23 && i+1 < metarun_max) {   /* page break */
            c_put_str(TERM_L_DARK, "[more – any key]", 23, 2);
            inkey();  Term_clear();
            row = 4;
            c_prt(TERM_L_GREEN, "Meta-run history (cont.)", 1, 2);
            c_put_str(TERM_L_DARK,
                      " *ID      Score     Sil  Dth  Res  Last played", 3, 2);
        }
    }

    FREE(order);
    c_put_str(TERM_L_DARK, "Press any key to return.", row+1, 2);
    inkey();
    screen_load();
}

void show_known_curses_menu(void)
{
    int shown = 0;
    int row = 2;
    int id;

    /* Collect and count first */
    for (id = 0; id < (int)z_info->cu_max; id++)
        if (CURSE_SEEN(id)) {
                shown++;
            }
    if (!shown) {
        log_debug("No curses have been seen yet");
        msg_print("You have not identified any curses yet.");
        return;
    }

    log_info("Displaying %d known curses", shown);

    screen_save();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curses:");

    row = 2;

    /* Enable wrapped text helper */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 4;   /* generous rhs margin */

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (!CURSE_SEEN(id)) continue;

        curse_type *c = &cu_info[id];
        cptr cname  = cu_name + c->name;
        cptr cdesc  = cu_text + c->text;
        cptr cpower = cu_text + c->power;

        /* Name */
        c_put_str(TERM_L_RED, cname, row, 1);
        row++;

        /* Description (wrapped) */
        Term_gotoxy(3, row);
        text_out_c(TERM_WHITE, cdesc);
        row += count_wrapped_lines(cdesc, text_out_wrap, 3);

        /* Curse effect */
        const char *curse_effect = (*cpower) ? cpower : "[no additional effect listed]";
        char curse_effect_line[256];
        strnfmt(curse_effect_line, sizeof curse_effect_line, "Effect: %s", curse_effect);
        Term_gotoxy(3, row);
        text_out_c(TERM_RED, "Effect: ");
        text_out_c(TERM_L_DARK, curse_effect);
        row += count_wrapped_lines(curse_effect_line, text_out_wrap, 3);

        /* Blessing counterpart */
        cptr bname = blessing_display_name(id);
        cptr bdesc = (c->blessing_text) ? (cu_text + c->blessing_text) : "";
        cptr bpower = (c->blessing_power) ? (cu_text + c->blessing_power) : "";
        bool has_blessing_text = bdesc && *bdesc;
        bool has_blessing_effect = bpower && *bpower;
        bool has_blessing_info = has_blessing_text || has_blessing_effect || (c->blessing_name != 0);

        if (has_blessing_info) {
            Term_putstr(3, row++, -1, TERM_L_GREEN, format("Blessing: %s", bname));

            if (has_blessing_text) {
                Term_gotoxy(5, row);
                text_out_c(TERM_WHITE, bdesc);
                row += count_wrapped_lines(bdesc, text_out_wrap, 5);
            }

            const char *bless_effect = has_blessing_effect ? bpower : "[no additional effect listed]";
            char bless_effect_line[256];
            strnfmt(bless_effect_line, sizeof bless_effect_line, "Effect: %s", bless_effect);
            Term_gotoxy(5, row);
            text_out_c(TERM_L_GREEN, "Effect: ");
            text_out_c(TERM_WHITE, bless_effect);
            row += count_wrapped_lines(bless_effect_line, text_out_wrap, 5);
        }

        row++;

        /* Page wrap (match self_knowledge style) */
        if (row >= 21)
        {
            Term_putstr(1, row, -1, TERM_L_WHITE, "(press any key)");
            (void)inkey();
            Term_clear();
            Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curses:");
            row = 2;
        }
    }

    Term_putstr(1, row+1, -1, TERM_L_WHITE, "(press any key)");
    (void)inkey();
    screen_load();
}

/* Public wrapper for difficulty selection menu */
void choose_difficulty_level(void)
{
    choose_difficulty_menu();
}

/* ================================================================== */
/*  Quest completion tracking functions                               */
/* ================================================================== */

/* Check if a specific quest is completed in the CURRENT metarun */
bool metarun_is_quest_completed(u32b quest_flag)
{
    /* Only check the current metarun, not all metaruns */
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Metarun quest check: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return false;
    }
    
    if (metaruns[current_run].completed_quests & quest_flag) {
        log_trace("Metarun quest check: Found quest 0x%x completed in current metarun[%d] (id=%d)", 
                  quest_flag, current_run, metaruns[current_run].id);
        return true;
    }
    
    log_trace("Metarun quest check: Quest 0x%x not completed in current metarun[%d] (id=%d)", 
              quest_flag, current_run, metaruns[current_run].id);
    return false;
}

/* Mark a quest as completed in the current metarun */
void metarun_mark_quest_completed(u32b quest_flag)
{
    if (current_run < 0 || current_run >= metarun_max) return;
    /* IMPORTANT: modify the live 'metar' copy first, THEN persist.
     * Previous code wrote directly to metaruns[current_run] and was
     * immediately overwritten inside save_metaruns() when that
     * function copied the stale 'metar' struct back into the array.
     * (metaruns[current_run] = metar;). This caused lost quest flags.
     */
    if (!(metar.completed_quests & quest_flag)) {
        metar.completed_quests |= quest_flag;                  /* update live */
        metaruns[current_run].completed_quests = metar.completed_quests; /* keep array in sync early (optional) */
        log_trace("Metarun: Quest flag 0x%x added (completed_quests=0x%08X)", quest_flag, metar.completed_quests);
        refresh_current_metar_score();
        save_metaruns();
    } else {
        log_trace("Metarun: Quest flag 0x%x already set (completed_quests=0x%08X) - no save needed", quest_flag, metar.completed_quests);
    }
}

/* Check and update quest completion status based on player state */
void metarun_check_and_update_quests(void)
{
    log_trace("Metarun quest check: Entry - current_run=%d, metarun_max=%d", current_run, metarun_max);
    
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Metarun quest check: Early return - current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    
    log_trace("Metarun quest check: current_run=%d, tulkas=%d, aule=%d, mandos=%d", 
              current_run, p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);
    
    /* Check Tulkas quest completion - only mark as metarun-complete when REWARDED */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED) {
        if (!metarun_is_quest_completed(METARUN_QUEST_TULKAS)) {
            log_trace("Metarun: Marking Tulkas quest as completed (rewarded, was %d)", p_ptr->tulkas_quest);
            metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
        } else {
            log_trace("Metarun: Tulkas quest already marked as completed");
        }
    }
    
    /* Check Aule quest completion - only mark as metarun-complete when REWARDED */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED) {
        if (!metarun_is_quest_completed(METARUN_QUEST_AULE)) {
            log_trace("Metarun: Marking Aule quest as completed (rewarded)");
            metarun_mark_quest_completed(METARUN_QUEST_AULE);
        } else {
            log_trace("Metarun: Aule quest already marked as completed");
        }
    }

    /* Check Mandos quest completion - only mark as metarun-complete when REWARDED */
    if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED) {
        if (!metarun_is_quest_completed(METARUN_QUEST_MANDOS)) {
            log_trace("Metarun: Marking Mandos quest as completed (rewarded)");
            metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
        } else {
            log_trace("Metarun: Mandos quest already marked as completed");
        }
    }
}

/* Restore quest states from metarun data after character loading */
void metarun_restore_quest_states(void)
{
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Metarun restore: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    
    u32b completed = metaruns[current_run].completed_quests;
    log_trace("Metarun restore: Restoring quest states from metarun[%d], completed_quests=0x%08X", 
              current_run, completed);
    
    /* Restore Tulkas quest state */
    if (completed & METARUN_QUEST_TULKAS) {
        if (p_ptr->tulkas_quest < TULKAS_QUEST_REWARDED) {
            p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
            log_trace("Metarun restore: Tulkas quest set to REWARDED (%d)", TULKAS_QUEST_REWARDED);
        }
    }
    
    /* Restore Aule quest state */
    if (completed & METARUN_QUEST_AULE) {
        if (p_ptr->aule_quest < AULE_QUEST_REWARDED) {
            p_ptr->aule_quest = AULE_QUEST_REWARDED;
            log_trace("Metarun restore: Aule quest set to REWARDED (%d)", AULE_QUEST_REWARDED);
        }
    }
    
    /* Restore Mandos quest state */
    if (completed & METARUN_QUEST_MANDOS) {
        if (p_ptr->mandos_quest < MANDOS_QUEST_REWARDED) {
            p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
            log_trace("Metarun restore: Mandos quest set to REWARDED (%d)", MANDOS_QUEST_REWARDED);
        }
    }
    
    /* Restore Niena quest state */
    if (completed & METARUN_QUEST_NIENA) {
        if (p_ptr->niena_quest < NIENA_QUEST_REWARDED) {
            p_ptr->niena_quest = NIENA_QUEST_REWARDED;
            p_ptr->niena_level = 0; /* Clear depth for previous run attribution */
            log_trace("Metarun restore: Niena quest set to REWARDED (%d)", NIENA_QUEST_REWARDED);
        }
    }
    
    /* Restore Orome quest state */
    if (completed & METARUN_QUEST_OROME) {
        if (p_ptr->orome_quest < OROME_QUEST_REWARDED) {
            p_ptr->orome_quest = OROME_QUEST_REWARDED;
            log_trace("Metarun restore: Orome quest set to REWARDED (%d)", OROME_QUEST_REWARDED);
        }
    }
    
    log_trace("Metarun restore: Final quest states - Tulkas: %d, Aule: %d, Mandos: %d, Niena: %d, Orome: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest);
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
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-4 to bits 1,2,4,8 */
    
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
    
    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    
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
    if (current_run < 0 || current_run >= metarun_max) return 0;
    
    byte unlocked = metaruns[current_run].unlocked_oaths;
    byte banned = metaruns[current_run].banned_oaths;
    byte available = unlocked & ~banned;
    
    log_trace("Oath availability: unlocked=0x%02X, banned=0x%02X, available=0x%02X", 
              unlocked, banned, available);
    
    return available;
}



