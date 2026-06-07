/* File: cave-styles.c */

#include "cave-internal.h"

/* Encoded color range that indicates an absolute style index per cell.
 * We now store the chosen style for each cell directly in cave_color as
 * COLOR_STYLE_BASE + style_index. This guarantees deterministic visuals
 * and removes the need for group indirection.
 *
 * Additionally, we reserve an offset of 64 to indicate a "first-variant"
 * override. That is, values in the range COLOR_STYLE_BASE+64..+127 encode
 * the same style indices 0..63, but signal that when a style offers multiple
 * floor/door variants, variant index 0 should be used regardless of the
 * per-level/per-vault random choice. This is used by the vault halo so that
 * adjacent tiles reliably use the vault's first variant.
 */
#ifndef COLOR_STYLE_BASE
#define COLOR_STYLE_BASE 128 /* 128..(128+style_max-1) map to style_info indices */
#endif
/* Max supported styles in encoded color and the special first-variant flag */
#ifndef COLOR_STYLE_SLOT_MAX
#define COLOR_STYLE_SLOT_MAX 64
#endif
#ifndef COLOR_STYLE_FLAG_FIRSTVAR
#define COLOR_STYLE_FLAG_FIRSTVAR COLOR_STYLE_SLOT_MAX
#endif
/*
 * Weighted style selection
 * ------------------------
 * For each level (and while building a vault), we maintain a weighted list
 * of style indices. When a grid feature is set without an explicit style,
 * we pick one style randomly according to weights and encode it in
 * cave_color[y][x] as COLOR_STYLE_BASE + style_index.
 */
typedef struct {
    int count;            /* number of entries */
    int total_weight;     /* cached sum of weights */
    int sidx[64];         /* style indices */
    int weight[64];       /* weights for each index */
} style_weight_list;

/* For each style, pick one floor/door variant per level/vault */
static byte g_level_floor_choice[64];  /* index into floor_rowv/colv, 0..count-1 */
static byte g_level_door_choice[64];
static byte g_vault_floor_choice[64];
static byte g_vault_door_choice[64];
static byte g_hallucination_style_map[64];
static bool g_hallucination_style_map_ready = false;

/* Active weighted style lists and selections */
static style_weight_list g_level_styles;
static style_weight_list g_vault_styles;
static style_weight_list* g_active_styles = &g_level_styles;
static int g_level_primary_style = -1;
static int g_vault_primary_style = -1;
static int g_vault_avoid_style = -1;

bool cave_style_index_is_valid(int sidx)
{
    return z_info && style_info && sidx >= 0 && sidx < z_info->style_max
        && style_info[sidx].name;
}

/* Level rules table (indexed by exact depth 0..31) */
static style_weight_list g_level_rule[32];
/* Per-depth partition style rules (by kind, indexed by exact depth 0..31) */
static style_weight_list g_partition_rule[PART_STYLE_MAX][32];

/* Helpers to mutate weight lists */
static void styles_clear(style_weight_list* L)
{
    if (!L) return;
    L->count = 0; L->total_weight = 0;
}

static void styles_add(style_weight_list* L, int sidx, int weight)
{
    if (!L || !z_info || !style_info) return;
    if (sidx < 0 || sidx >= z_info->style_max) return;
    if (!style_info[sidx].name) return;
    if (L->count >= 64) return;
    if (weight <= 0) return;
    L->sidx[L->count] = sidx;
    L->weight[L->count] = weight;
    L->count++;
    L->total_weight += weight;
}

/* Debug helper */
static void styles_log_list(const char* tag, const style_weight_list* L)
{
    if (!L) return;
    log_debug("%s: count=%d total=%d", tag ? tag : "styles", L->count, L->total_weight);
}

/* Ensure depth-based wall styling is enabled unless explicitly disabled elsewhere */
#ifndef DEPTH_BASED_WALLS
#define DEPTH_BASED_WALLS 1
#endif

/* Forward declarations for rule APIs defined later in this file */
void styles_rules_clear(void);
void styles_default_vault_clear(void);
void styles_vault_rules_clear(void);
void styles_add_level_rule(int depth, int unused, const int* sidx, const int* weight, int count);
void styles_set_vault_rule(int depth, const int* sidx, const int* weight, int count);
void styles_default_vault_add(int sidx_or_star, int weight);
void styles_partition_rules_clear(void);
void styles_add_partition_rule(int depth, int kind, const int* sidx, const int* weight, int count);
int styles_pick_partition_style(int depth, int kind);

/* Backward-compatibility: reset any cached depth/style state between levels */
void reset_depth_color_cache(void)
{
    styles_clear(&g_level_styles);
    styles_clear(&g_vault_styles);
    g_active_styles = &g_level_styles;
    g_level_primary_style = -1;
    g_vault_primary_style = -1;
    g_vault_avoid_style = -1;
    log_debug("reset_depth_color_cache: cleared style lists and selections");
}

/* Initialize the level style weights: use matching rule, else default to all */
void styles_init_for_level(void)
{
    /* Depth 0 is the Gates of Angband: force style 13 for the whole level */
    if (p_ptr && p_ptr->depth == 0) {
        styles_clear(&g_level_styles);
        styles_add(&g_level_styles, 13, 1);
        g_active_styles = &g_level_styles;
        g_level_primary_style = 13;
        /* Reset per-style variant picks */
        for (int i = 0; i < 64; ++i) { g_level_floor_choice[i] = 0; g_level_door_choice[i] = 0; }
        /* No need to randomize variants; keep first variant for cohesion */
        log_debug("styles_init_for_level: depth=0 forced style 13 as primary");
        styles_log_list("styles_init_for_level list", &g_level_styles);
        return;
    }
    /* Normal depths: initialize per rules or fallback */
    styles_clear(&g_level_styles);
    /* Reset per-style variant picks */
    for (int i = 0; i < 64; ++i) { g_level_floor_choice[i] = 0; g_level_door_choice[i] = 0; }
    bool applied = false;
    /* Apply rule matching exact depth (0..31); depth 0 is special (Gates) */
    if (p_ptr->depth >= 0 && p_ptr->depth < 32) {
        style_weight_list* L = &g_level_rule[p_ptr->depth];
        for (int i = 0; i < L->count; ++i) styles_add(&g_level_styles, L->sidx[i], L->weight[i]);
        applied = (g_level_styles.count > 0);
    }
    /* Fallback: all styles */
    if (!applied && z_info && style_info) {
        for (int i = 0; i < z_info->style_max; i++) {
            if (style_info[i].name) styles_add(&g_level_styles, i, 1);
        }
    }
    g_active_styles = &g_level_styles;
    /* Choose one exact primary style for this level (used by vault '*') */
    if (g_level_styles.count > 0) {
        int total = g_level_styles.total_weight;
        int r = rand_int(total);
        int pick = g_level_styles.sidx[0];
        for (int i = 0; i < g_level_styles.count; ++i) {
            if (r < g_level_styles.weight[i]) { pick = g_level_styles.sidx[i]; break; }
            r -= g_level_styles.weight[i];
        }
        g_level_primary_style = pick;
    } else {
        g_level_primary_style = -1;
    }
    /* For all styles, pick a variant index (if multiple) once per level */
    if (z_info && style_info) {
        for (int i = 0; i < z_info->style_max && i < 64; ++i) {
            if (!style_info[i].name) continue;
            byte fc = style_info[i].floor_count;
            byte dc = style_info[i].door_count;
            if (fc > 1) g_level_floor_choice[i] = (byte)rand_int(fc);
            if (dc > 1) g_level_door_choice[i] = (byte)rand_int(dc);
        }
    }

    log_debug("styles_init_for_level: depth=%d initialized %d styles (total_weight=%d) primary=%d",
        p_ptr->depth, g_level_styles.count, g_level_styles.total_weight, g_level_primary_style);
    styles_log_list("styles_init_for_level list", &g_level_styles);
}

/* Begin vault: by default prefer the level's chosen style with weight 5 and
 * optionally add/boost one extra style with a given weight (e.g., 2).
 */
void styles_begin_vault(int extra_sidx, int extra_weight)
{
    styles_clear(&g_vault_styles);
    g_vault_primary_style = -1;
    /* Reset and start with level picks, then override randomly per vault */
    for (int i = 0; i < 64; ++i) {
        /* Floors inside vaults use the first variant for visual cohesion */
        g_vault_floor_choice[i] = 0;
        /* Keep door variant selection from level unless overridden */
        g_vault_door_choice[i] = g_level_door_choice[i];
    }
    /* Default: start empty; callers may clone level list via API */
    /* Optionally add one more style */
    if (extra_sidx >= 0 && extra_weight > 0) styles_add(&g_vault_styles, extra_sidx, extra_weight);
    g_active_styles = &g_vault_styles;
    log_debug("styles_begin_vault: active styles=%d (extra=%d, w=%d)",
        g_vault_styles.count, extra_sidx, extra_weight);
    styles_log_list("styles_begin_vault list", &g_vault_styles);
}

/* End vault: restore level styles */
void styles_end_vault(void)
{
    g_active_styles = &g_level_styles;
    g_vault_primary_style = -1;
    g_vault_avoid_style = -1;
}

/* Helpers to get current variant choice for a style index (vault if active) */
byte cave_style_floor_choice(int sidx) {
    return (g_vault_primary_style >= 0) ? g_vault_floor_choice[sidx & 63] : g_level_floor_choice[sidx & 63];
}
byte cave_style_door_choice(int sidx) {
    return (g_vault_primary_style >= 0) ? g_vault_door_choice[sidx & 63] : g_level_door_choice[sidx & 63];
}

/* Pick a style index using the active weighted list */
/* styles_pick_random was removed; we always pick primary styles at scope start */

/* External APIs to explicitly control level/vault weight lists */
void styles_reset_level_weights(void) { styles_clear(&g_level_styles); g_active_styles = &g_level_styles; }
void styles_add_level_weight(int sidx, int weight) { styles_add(&g_level_styles, sidx, weight); }
void styles_reset_vault_weights(void) { styles_clear(&g_vault_styles); }
void styles_add_vault_weight(int sidx, int weight) { styles_add(&g_vault_styles, sidx, weight); }
void styles_add_vault_from_level(int factor)
{
    if (factor <= 0) factor = 1;
    for (int i = 0; i < g_level_styles.count; ++i) {
        styles_add(&g_vault_styles, g_level_styles.sidx[i], g_level_styles.weight[i] * factor);
    }
}

/* Default vault style list: can include '*' which means "clone level list".
 * We represent '*' as sidx == -1 and apply it when starting a vault. */
static int g_vault_default_count = 0;
static int g_vault_default_sidx[64];
static int g_vault_default_weight[64];

void styles_default_vault_clear(void) { g_vault_default_count = 0; }
void styles_default_vault_add(int sidx_or_star, int weight)
{
    if (g_vault_default_count >= 64) return;
    g_vault_default_sidx[g_vault_default_count] = sidx_or_star; /* -1 means '*' */
    g_vault_default_weight[g_vault_default_count] = weight > 0 ? weight : 0;
    g_vault_default_count++;
}

/* Apply a vault style array into the active vault list; tokens may include '*' */
void styles_apply_vault_list(const int* sidx, const int* weight, int count)
{
    styles_reset_vault_weights();
    for (int i = 0; i < count; ++i) {
        if (sidx[i] == -1) {
            if (g_level_primary_style >= 0) styles_add_vault_weight(g_level_primary_style, weight[i]);
        }
        else styles_add_vault_weight(sidx[i], weight[i]);
    }
    styles_log_list("styles_apply_vault_list", &g_vault_styles);
}

/* Per-depth default vault lists (1..20), tokens may include '*' (-1) */
typedef struct { int count; int sidx[64]; int weight[64]; } vault_rule_list;
static vault_rule_list g_vault_rule[32];

void styles_vault_rules_clear(void) { for (int i = 0; i < 32; ++i) g_vault_rule[i].count = 0; }
void styles_set_vault_rule(int depth, const int* sidx, const int* weight, int count)
{
    if (depth < 1 || depth >= 32) return;
    vault_rule_list* R = &g_vault_rule[depth];
    R->count = 0;
    for (int i = 0; i < count && i < 64; ++i) { R->sidx[i] = sidx[i]; R->weight[i] = weight[i]; R->count++; }
}
void styles_apply_vault_default_for_depth(int depth)
{
    if (depth >= 1 && depth < 32 && g_vault_rule[depth].count > 0) {
        log_debug("styles_apply_vault_default_for_depth: using per-depth defaults for depth=%d", depth);
        styles_apply_vault_list(g_vault_rule[depth].sidx, g_vault_rule[depth].weight, g_vault_rule[depth].count);
    } else if (g_vault_default_count > 0) {
        log_debug("styles_apply_vault_default_for_depth: using global defaults for depth=%d", depth);
        styles_apply_vault_list(g_vault_default_sidx, g_vault_default_weight, g_vault_default_count);
    } else {
    /* Fallback: 100% same as the exact chosen level style */
    log_debug("styles_apply_vault_default_for_depth: using fallback to level primary for depth=%d", depth);
    if (g_level_primary_style >= 0) styles_add_vault_weight(g_level_primary_style, 10);
    }
}

int styles_get_level_primary_style(void) { return g_level_primary_style; }
void styles_set_loaded_level_primary(int sidx) { g_level_primary_style = sidx; }

/* Clear in-memory level rules table */
void styles_rules_clear(void)
{
    for (int i = 0; i < 32; ++i) {
        g_level_rule[i].count = 0;
        g_level_rule[i].total_weight = 0;
    }
}

/* Add a rule for exact depth (0..31) with arrays of indices/weights */
void styles_add_level_rule(int depth, int unused, const int* sidx, const int* weight, int count)
{
    (void)unused;
    if (depth < 0 || depth >= 32) return;
    style_weight_list* L = &g_level_rule[depth];
    L->count = 0;
    L->total_weight = 0;
    if (!sidx || !weight || count <= 0) return;
    for (int i = 0; i < count && i < 64; ++i) {
        int si = sidx[i];
        int wt = weight[i];
        /* Defer validity checks that need style_info until use; store raw */
        L->sidx[L->count] = si;
        L->weight[L->count] = wt;
        L->count++;
        if (wt > 0) L->total_weight += wt;
    }
}

void styles_partition_rules_clear(void)
{
    for (int k = 0; k < PART_STYLE_MAX; ++k) {
        for (int d = 0; d < 32; ++d) {
            g_partition_rule[k][d].count = 0;
            g_partition_rule[k][d].total_weight = 0;
        }
    }
}

void styles_add_partition_rule(int depth, int kind, const int* sidx, const int* weight, int count)
{
    if (depth < 0 || depth >= 32) return;
    if (kind < 0 || kind >= PART_STYLE_MAX) return;
    style_weight_list* L = &g_partition_rule[kind][depth];
    L->count = 0;
    L->total_weight = 0;
    if (!sidx || !weight || count <= 0) return;
    for (int i = 0; i < count && i < 64; ++i) {
        int si = sidx[i];
        int wt = weight[i];
        L->sidx[L->count] = si;
        L->weight[L->count] = wt;
        L->count++;
        if (wt > 0) L->total_weight += wt;
    }
}

int styles_pick_partition_style(int depth, int kind)
{
    if (depth < 0 || depth >= 32) return styles_pick_random_from_level();
    if (kind < 0 || kind >= PART_STYLE_MAX) return styles_pick_random_from_level();

    style_weight_list* R = &g_partition_rule[kind][depth];
    if (R->count <= 0) return styles_pick_random_from_level();

    /* Filter invalid styles into a temporary list. */
    style_weight_list filtered;
    styles_clear(&filtered);
    for (int i = 0; i < R->count; ++i)
        styles_add(&filtered, R->sidx[i], R->weight[i]);

    if (filtered.count <= 0) return styles_pick_random_from_level();

    int total = filtered.total_weight;
    int r = rand_int(total);
    int pick = filtered.sidx[0];
    for (int i = 0; i < filtered.count; ++i) {
        if (r < filtered.weight[i]) { pick = filtered.sidx[i]; break; }
        r -= filtered.weight[i];
    }
    return pick;
}

/* Expose capacity for style choice arrays (for save/load) */
int styles_get_choice_capacity(void) { return 64; }

/* Copy out the current per-level door variant choices. Caller provides buffer. */
void styles_copy_level_door_choices(byte* out_buf, int max_n)
{
    if (!out_buf || max_n <= 0) return;
    int n = styles_get_choice_capacity();
    if (max_n < n) n = max_n;
    for (int i = 0; i < n; ++i) out_buf[i] = g_level_door_choice[i];
}

/* Load per-level door variant choices from a buffer of length n. */
void styles_load_level_door_choices(const byte* in_buf, int n)
{
    if (!in_buf || n <= 0) return;
    int cap = styles_get_choice_capacity();
    if (n > cap) n = cap;
    for (int i = 0; i < n; ++i) g_level_door_choice[i] = in_buf[i];
}

/* Decode a style index from a cave_color cell (or -1 if not encoded) */
int cave_style_index_for_color(byte color_value)
{
    if (!z_info || !style_info) return -1;
    /* Support both new (base=128) and legacy (base=200) encodings */
    int bases[2] = { 128, 200 };
    for (int bi = 0; bi < 2; ++bi) {
        int base = bases[bi];
        if (color_value >= base) {
            int slot = color_value - base;
            if (slot >= COLOR_STYLE_SLOT_MAX) slot -= COLOR_STYLE_SLOT_MAX; /* strip first-variant flag */
            if (slot >= 0 && slot < z_info->style_max && style_info[slot].name) return slot;
        }
    }
    return -1;
}

/* Does the encoded color request the first variant explicitly? */
bool cave_style_color_force_first_variant(byte color_value)
{
    /* Handle both encodings: if value minus either base is >= flag offset, it's first-variant */
    int bases[2] = { 128, 200 };
    for (int bi = 0; bi < 2; ++bi) {
        int base = bases[bi];
        if (color_value >= base) {
            int slot = color_value - base;
            if (slot >= COLOR_STYLE_FLAG_FIRSTVAR) return true;
        }
    }
    return false;
}

static void hallucination_set_identity_style_map(void)
{
    for (int i = 0; i < COLOR_STYLE_SLOT_MAX; i++)
        g_hallucination_style_map[i] = (byte)i;
}

void hallucination_clear_style_transitions(void)
{
    g_hallucination_style_map_ready = false;
}

void hallucination_randomize_style_transitions(void)
{
    int valid[COLOR_STYLE_SLOT_MAX];
    int valid_count = 0;
    int limit = COLOR_STYLE_SLOT_MAX;

    hallucination_set_identity_style_map();

    if (z_info && z_info->style_max < limit)
        limit = z_info->style_max;

    for (int i = 0; i < limit; i++)
    {
        if (cave_style_index_is_valid(i))
            valid[valid_count++] = i;
    }

    for (int i = 0; i < valid_count; i++)
    {
        int source = valid[i];
        int target = source;

        if (valid_count > 1)
        {
            int pick = rand_int(valid_count - 1);

            target = valid[pick];
            if (target == source)
                target = valid[valid_count - 1];
        }

        g_hallucination_style_map[source] = (byte)target;
    }

    g_hallucination_style_map_ready = true;
    log_debug("hallucination_randomize_style_transitions: remapped %d styles",
        valid_count);
}

int cave_hallucination_style_for_display(int sidx)
{
    int mapped;

    if (!cave_style_index_is_valid(sidx))
        return sidx;

    if (!p_ptr || !p_ptr->image)
        return sidx;

    if (sidx >= COLOR_STYLE_SLOT_MAX)
        return sidx;

    if (!g_hallucination_style_map_ready)
        hallucination_randomize_style_transitions();

    mapped = g_hallucination_style_map[sidx];
    if (cave_style_index_is_valid(mapped))
        return mapped;

    return sidx;
}

/* Return COLOR_STYLE_BASE + active chosen style (vault if active, else level) */
byte cave_get_active_style_color(void)
{
    int sidx = (g_vault_primary_style >= 0) ? g_vault_primary_style : g_level_primary_style;
    /* Depth-0 safety: if style rules weren't loaded yet, force style 13 */
    if (sidx < 0) {
        if (p_ptr && p_ptr->depth == 0) {
            sidx = 13;
        } else {
            return 0;
        }
    }
    return (byte)(COLOR_STYLE_BASE + sidx);
}
/* Variant of cave_get_active_style_color() that forces the first variant */
/* Note: first-variant override is encoded directly into cave_color by callers */
void styles_select_vault_primary(void)
{
    if (g_vault_styles.count <= 0) {
        g_vault_primary_style = g_level_primary_style;
        log_debug("styles_select_vault_primary: no vault list, defaulting to level primary=%d", g_level_primary_style);
        return;
    }
    int total = 0;
    for (int i = 0; i < g_vault_styles.count; ++i) {
        if (g_vault_styles.sidx[i] == g_vault_avoid_style) continue;
        total += g_vault_styles.weight[i];
    }
    if (total <= 0) total = g_vault_styles.total_weight; /* fallback: nothing to avoid */

    int r = rand_int(total);
    int pick = g_vault_styles.sidx[0];
    for (int i = 0; i < g_vault_styles.count; ++i) {
        if (g_vault_styles.sidx[i] == g_vault_avoid_style && total != g_vault_styles.total_weight) continue;
        if (r < g_vault_styles.weight[i]) { pick = g_vault_styles.sidx[i]; break; }
        r -= g_vault_styles.weight[i];
    }
    g_vault_primary_style = pick;
    log_debug("styles_select_vault_primary: selected vault primary=%d from %d entries (total=%d, avoid=%d)",
        g_vault_primary_style, g_vault_styles.count, g_vault_styles.total_weight, g_vault_avoid_style);
    styles_log_list("styles_select_vault_primary list", &g_vault_styles);
}

int styles_get_vault_primary_style(void) { return g_vault_primary_style; }

int cave_style_primary_for_grid(int y, int x)
{
    return (g_vault_primary_style >= 0 && (cave_info[y][x] & CAVE_ICKY))
        ? g_vault_primary_style
        : g_level_primary_style;
}

/* Pick one weighted-random style from the current level's available list. */
int styles_pick_random_from_level(void)
{
    if (g_level_styles.count <= 0) return -1;
    int total = g_level_styles.total_weight;
    int r = rand_int(total);
    int pick = g_level_styles.sidx[0];
    for (int i = 0; i < g_level_styles.count; ++i) {
        if (r < g_level_styles.weight[i]) { pick = g_level_styles.sidx[i]; break; }
        r -= g_level_styles.weight[i];
    }
    return pick;
}

void styles_set_vault_avoid_style(int sidx)
{
    g_vault_avoid_style = sidx;
}

/*
 * ASCII wall colour by style.
 *
 * In ASCII/text mode there are no per-style wall tiles, so we colour the wall
 * glyph ('#' / '%') according to the colour word named in the style's S:
 * short descriptor in style.txt (e.g. "cold grey ashstone", "sickly green
 * slime-covered bricks", "ornate deep sapphire stone"). Returns a TERM_* attr,
 * or -1 when the descriptor carries no recognisable colour (the caller then
 * keeps the feature's default colour).
 */

/* Case-insensitive substring search (no portable strcasestr in the codebase). */
static bool str_contains_ci(const char* hay, const char* needle)
{
    size_t nlen;
    if (!hay || !needle) return false;
    nlen = strlen(needle);
    if (nlen == 0) return false;
    for (; *hay; hay++) {
        if (SDL_strncasecmp(hay, needle, nlen) == 0) return true;
    }
    return false;
}

int cave_style_ascii_attr(int sidx)
{
    /* Colour words mapped to TERM_* attrs, in priority order. The descriptors
     * often mention more than one colour (e.g. "blue-grey", "purple-green",
     * "blood-red heraldry"); earlier entries win, so the dominant/material
     * colour is listed before accent colours. */
    static const struct { const char* word; byte attr; } color_words[] = {
        { "grey",       TERM_SLATE  },
        { "gray",       TERM_SLATE  },
        { "marble",     TERM_WHITE  },
        { "bone",       TERM_WHITE  },
        { "parchment",  TERM_WHITE  },
        { "white",      TERM_WHITE  },
        { "yellow",     TERM_YELLOW },
        { "crimson",    TERM_RED    },
        { "blood",      TERM_RED    },
        { "red",        TERM_RED    },
        { "lavender",   TERM_VIOLET },
        { "violet",     TERM_VIOLET },
        { "purple",     TERM_VIOLET },
        { "pink",       TERM_VIOLET },
        { "jade",       TERM_GREEN  },
        { "moss",       TERM_GREEN  },
        { "serpentine", TERM_GREEN  },
        { "green",      TERM_GREEN  },
        { "sapphire",   TERM_BLUE   },
        { "blue",       TERM_BLUE   },
        { "ember",      TERM_ORANGE },
        { "fire",       TERM_ORANGE },
        { "molten",     TERM_RED    },
        { "rust",       TERM_UMBER  },
        { "iron",       TERM_UMBER  },
        { "brown",      TERM_UMBER  },
        { "earth",      TERM_UMBER  },
        { "shadow",     TERM_L_DARK },
        { "lightless",  TERM_L_DARK },
        { "void",       TERM_L_DARK },
        { "black",      TERM_L_DARK },
        { "dark",       TERM_L_DARK },
    };

    const char* desc;

    if (!cave_style_index_is_valid(sidx)) return -1;

    desc = styles_get_style_short_desc(sidx);
    if (!desc || !desc[0]) return -1;

    for (size_t i = 0; i < N_ELEMENTS(color_words); i++) {
        if (str_contains_ci(desc, color_words[i].word))
            return color_words[i].attr;
    }

    return -1;
}

int styles_decode_color_style(byte color_value)
{
    int bases[2] = { COLOR_STYLE_BASE, COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR };
    for (int bi = 0; bi < 2; ++bi) {
        int base = bases[bi];
        if (color_value >= base) {
            int slot = color_value - base;
            if (slot >= COLOR_STYLE_FLAG_FIRSTVAR) slot -= COLOR_STYLE_FLAG_FIRSTVAR;
            slot &= (COLOR_STYLE_SLOT_MAX - 1);
            return slot;
        }
    }
    return -1;
}
