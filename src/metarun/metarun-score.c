#include "angband.h"
#include "metarun-internal.h"

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

void update_blessing_ledger(metarun *m)
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

int major_blessing_capacity(void)
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

const major_blessing_type *major_blessing_def(int idx)
{
    if (!mb_info || !z_info) return NULL;
    if (idx < 0 || idx >= (int)z_info->mb_max) return NULL;
    const major_blessing_type *def = &mb_info[idx];
    if (!def->name) return NULL;
    return def;
}

cptr major_blessing_name_str(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_name || !def->name) return "(unknown)";
    return mb_name + def->name;
}

cptr major_blessing_short_desc(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->short_desc) return NULL;
    return mb_text + def->short_desc;
}

cptr major_blessing_detail_desc(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->detail_desc) return NULL;
    return mb_text + def->detail_desc;
}

cptr major_blessing_unlock_msg(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def || !mb_text || !def->unlock_msg) return NULL;
    return mb_text + def->unlock_msg;
}

int major_blessing_cost(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def) return 0;
    if (def->cost == 0) return 3;
    return def->cost;
}

metarun_major_effect major_blessing_effect(int idx)
{
    const major_blessing_type *def = major_blessing_def(idx);
    if (!def) return METARUN_MAJOR_EFFECT_NONE;
    return (metarun_major_effect)def->effect;
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

u32b runtype_threshold_for_mode(int runtype_id, metarun_blessing_threshold_mode mode)
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

u32b metarun_threshold_value(const metarun *m)
{
    if (!m) return METARUN_BLESSING_POINT_THRESHOLD;
    return runtype_threshold_for_mode(m->type, metarun_get_threshold_mode(m));
}

const char *threshold_mode_name(metarun_blessing_threshold_mode mode)
{
    switch (mode) {
        case METARUN_BLESSING_THRESHOLD_EASIER: return "Easier";
        case METARUN_BLESSING_THRESHOLD_HARDER: return "Harder";
        default: return "Normal";
    }
}

u32b get_best_run_score_from_highscores(void)
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

u32b compute_metarun_score(const metarun *m)
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
