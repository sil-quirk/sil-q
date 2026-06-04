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
static style_type* stl_ptr = NULL;
/* Global defaults for vein overlay when a style omits Y: */
static byte g_default_vein_row = 19;
static byte g_default_vein_col = 0;

/* Optional fixed color key for overlays (if provided) */
static bool g_overlay_key_enabled = false;
static byte g_overlay_key_r = 255, g_overlay_key_g = 0, g_overlay_key_b = 255; /* default magenta */

/* Accessors used by rendering */
byte get_default_vein_row(void);
byte get_default_vein_col(void);
byte get_default_vein_row(void) { return g_default_vein_row; }
byte get_default_vein_col(void) { return g_default_vein_col; }
bool get_overlay_key_enabled(void);
void get_overlay_key_rgb(byte* r, byte* g, byte* b);
bool get_overlay_key_enabled(void) { return g_overlay_key_enabled; }
void get_overlay_key_rgb(byte* r, byte* g, byte* b) { if (r) *r = g_overlay_key_r; if (g) *g = g_overlay_key_g; if (b) *b = g_overlay_key_b; }

/* Forward decls for narrative text parsers (S:/M1:/M2:/M:) */
static errr parse_style_short_desc_line(char* buf);
static errr parse_style_m1_line(char* buf);
static errr parse_style_m2_line(char* buf);
static errr parse_style_message_line(char* buf);

errr parse_style_info(char* buf, header* head)
{
    /* Note: L:/U: moved to style-levels.txt for clarity. */
    /* E:<row>:<col> or DY:<row>:<col> - default vein overlay tile (used if a style omits Y:) */
    {
        const char* p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if (((p[0] == 'D' || p[0] == 'd') && (p[1] == 'Y' || p[1] == 'y') && p[2] == ':') ||
            ((p[0] == 'E' || p[0] == 'e') && p[1] == ':' ))
        {
            const char* q = (p[0] == 'E' || p[0] == 'e') ? (p + 2) : (p + 3);
            int r, c; if (2 != sscanf(q, "%d:%d", &r, &c)) return PARSE_ERROR_GENERIC;
            g_default_vein_row = (byte)r; g_default_vein_col = (byte)c; return 0;
        }
    }

    /* EK:R:G:B - optional explicit color key for overlays (e.g., 255:0:255) */
    {
        const char* p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if ((p[0] == 'E' || p[0] == 'e') && (p[1] == 'K' || p[1] == 'k') && p[2] == ':')
        {
            int r, g, b; if (3 != sscanf(p + 3, "%d:%d:%d", &r, &g, &b)) return PARSE_ERROR_GENERIC;
            g_overlay_key_r = (byte)r; g_overlay_key_g = (byte)g; g_overlay_key_b = (byte)b; g_overlay_key_enabled = true; return 0;
        }
    }
    /* N:<index>:<name> */
    if (buf[0] == 'N')
    {
        int idx;
        char* s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
    idx = atoi(buf + 2);

        if (idx <= error_idx) return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (idx >= head->info_num) return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = idx;

    stl_ptr = ((style_type*)head->info_ptr) + idx;
    memset(stl_ptr, 0, sizeof(style_type));
    /* Initialize counts */
    stl_ptr->floor_count = 0;
    stl_ptr->door_count = 0;
        stl_ptr->name = add_name(head, s);
    if (idx == 0) { /* styles only here */ }
        return 0;
    }

    if (!stl_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* G:<group_id> (1..6) */
    if (buf[0] == 'G')
    {
        stl_ptr->group = (byte)atoi(buf + 2);
        return 0;
    }
    /* W:row:col  (wall) */
    if (buf[0] == 'W')
    {
        int r, c; if (2 != sscanf(buf + 2, "%d:%d", &r, &c)) return PARSE_ERROR_GENERIC;
        stl_ptr->wall_row = (byte)r; stl_ptr->wall_col = (byte)c; return 0;
    }
    /* Y:row:col  (vein) - use 'Y' since 'V' is reserved for file version */
    if (buf[0] == 'Y')
    {
        int r, c; if (2 != sscanf(buf + 2, "%d:%d", &r, &c)) return PARSE_ERROR_GENERIC;
    stl_ptr->vein_row = (byte)r; stl_ptr->vein_col = (byte)c; stl_ptr->vein_defined = true; return 0;
    }
    /* F:row:col [row:col ...]  (floor) - multiple allowed; tokens separated by spaces */
    if (buf[0] == 'F')
    {
        const char* p = buf + 2;
        int added = 0;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '#') break;
            int r = -1, c = -1; int n = 0;
            /* parse r:c */
            if (sscanf(p, "%d:%d%n", &r, &c, &n) == 2) {
                if (stl_ptr->floor_count == 0) { stl_ptr->floor_row = (byte)r; stl_ptr->floor_col = (byte)c; }
                if (stl_ptr->floor_count < 8) {
                    stl_ptr->floor_rowv[stl_ptr->floor_count] = (byte)r;
                    stl_ptr->floor_colv[stl_ptr->floor_count] = (byte)c;
                    stl_ptr->floor_count++;
                    added++;
                }
                p += n;
            } else {
                /* stop on invalid token */
                break;
            }
        }
        if (!added) return PARSE_ERROR_GENERIC;
        return 0;
    }
    /* D:row:col [row:col ...]  (door base) - multiple allowed; tokens separated by spaces */
    if (buf[0] == 'D')
    {
        const char* p = buf + 2;
        int added = 0;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '#') break;
            int r = -1, c = -1; int n = 0;
            if (sscanf(p, "%d:%d%n", &r, &c, &n) == 2) {
                if (stl_ptr->door_count == 0) { stl_ptr->door_row = (byte)r; stl_ptr->door_col = (byte)c; }
                if (stl_ptr->door_count < 8) {
                    stl_ptr->door_rowv[stl_ptr->door_count] = (byte)r;
                    stl_ptr->door_colv[stl_ptr->door_count] = (byte)c;
                    stl_ptr->door_count++;
                    added++;
                }
                p += n;
            } else {
                break;
            }
        }
        if (!added) return PARSE_ERROR_GENERIC;
        return 0;
    }

    /* S: <text> - short descriptor for transition composition */
    if (buf[0] == 'S' && buf[1] == ':')
    {
        return parse_style_short_desc_line(buf);
    }

    /* M1: <text> - physical entry description */
    if (buf[0] == 'M' && buf[1] == '1' && buf[2] == ':')
    {
        return parse_style_m1_line(buf);
    }

    /* M2: <text> - lore/atmosphere */
    if (buf[0] == 'M' && buf[1] == '2' && buf[2] == ':')
    {
        return parse_style_m2_line(buf);
    }

    /* M: <text> - legacy per-style message (treated as M1:) */
    if (buf[0] == 'M' && buf[1] == ':')
    {
        return parse_style_message_line(buf);
    }

    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

/* ====================  style-levels.txt parser  ===================== */

static int parse_partition_style_kind(const char* tok)
{
    if (!tok) return -1;
    if (SDL_strcasecmp(tok, "CA") == 0 || SDL_strcasecmp(tok, "CA_BLOB") == 0
        || SDL_strcasecmp(tok, "CA_BLOBS") == 0)
        return PART_STYLE_CA_BLOB;
    if (SDL_strcasecmp(tok, "LAB") == 0 || SDL_strcasecmp(tok, "LABYRINTH") == 0)
        return PART_STYLE_LABYRINTH;
    if (SDL_strcasecmp(tok, "CHASM") == 0 || SDL_strcasecmp(tok, "CHASM_FLOOR") == 0)
        return PART_STYLE_CHASM_FLOOR;
    if (SDL_strcasecmp(tok, "CHASM_BRIDGE") == 0 || SDL_strcasecmp(tok, "CHASM_BRIDGES") == 0)
        return PART_STYLE_CHASM_BRIDGE;
    if (SDL_strcasecmp(tok, "BIG_ICE") == 0 || SDL_strcasecmp(tok, "BIG_CAVE_ICE") == 0
        || SDL_strcasecmp(tok, "ICE") == 0)
        return PART_STYLE_BIG_CAVE_ICE;
    if (SDL_strcasecmp(tok, "BIG_FIRE") == 0 || SDL_strcasecmp(tok, "BIG_CAVE_FIRE") == 0
        || SDL_strcasecmp(tok, "FIRE") == 0)
        return PART_STYLE_BIG_CAVE_FIRE;
    if (SDL_strcasecmp(tok, "BIG_POIS") == 0 || SDL_strcasecmp(tok, "BIG_CAVE_POIS") == 0
        || SDL_strcasecmp(tok, "POIS") == 0 || SDL_strcasecmp(tok, "POISON") == 0)
        return PART_STYLE_BIG_CAVE_POIS;
    return -1;
}

static int parse_big_cave_weight_token(const char* tok)
{
    if (!tok) return BIG_CAVE_NONE;
    if (SDL_strcasecmp(tok, "ICE") == 0 || SDL_strcasecmp(tok, "COLD") == 0)
        return BIG_CAVE_ICE;
    if (SDL_strcasecmp(tok, "FIRE") == 0)
        return BIG_CAVE_FIRE;
    if (SDL_strcasecmp(tok, "POIS") == 0 || SDL_strcasecmp(tok, "POISON") == 0)
        return BIG_CAVE_POIS;
    return BIG_CAVE_NONE;
}

errr parse_style_levels(char* buf, header* head)
{
    (void)head; /* unused */
    /* Version header clears accumulated rules to allow reparse safely */
    if (buf[0] == 'V') {
        styles_rules_clear();
        styles_vault_rules_clear();
        styles_default_vault_clear();
        styles_partition_rules_clear();
        big_cave_type_rules_clear();
    log_debug("parse_style_levels: Version header encountered, cleared existing rules");
        return 0;
    }
    /* Comments or blank lines */
    if (buf[0] == '#' || buf[0] == '\0') return 0;

    /* L:depth: ... (exact)  or  L:min:max: ... (range)  -> level style rules */
    if (buf[0] == 'L')
    {
        int min_d = 0, max_d = 0;
        char* s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        min_d = atoi(buf + 2);
        /* Decide if this is a range by checking for a ':' before any space after the first ':' */
        char* first_space = strchr(s, ' ');
        char* second_colon = strchr(s, ':');
        char* t;
        if (second_colon && (!first_space || second_colon < first_space)) {
            /* Range form: L:min:max: ...  --> s points to max followed by ':' */
            *second_colon = '\0';
            max_d = atoi(s);
            t = second_colon + 1;
        } else {
            /* Exact form: L:depth: ... */
            max_d = min_d;
            t = s;
        }
        int sidx[64]; int wt[64]; int n = 0;
        while (*t)
        {
            while (*t == ' ') t++;
            if (!*t) break;
            char* e = t; while (*e && *e != ' ') e++;
            char hold = *e; if (*e) *e = '\0';
            char* c = strchr(t, ':'); if (!c) { if (hold) *e = hold; break; }
            *c = '\0';
            int si = atoi(t); int w = atoi(c + 1);
            if (si >= 0 && w > 0 && n < 64) { sidx[n] = si; wt[n] = w; n++; }
            if (hold) { *e = hold; t = e + 1; } else break;
        }
        if (n > 0) {
            /* Apply to each depth in the range (min_d..max_d), inclusive */
            if (min_d < 1) min_d = 1;
            if (max_d > 31) max_d = 31;
            for (int d = min_d; d <= max_d; ++d) {
                styles_add_level_rule(d, 0, sidx, wt, n);
            }
            log_debug("parse_style_levels: L:%d..%d with %d entries (first sidx=%d w=%d)",
                min_d, max_d, n, sidx[0], wt[0]);
        }
        return 0;
    }
    /* P:type:depth: sidx:weight ...  or  P:type:min:max: ... (partition styles) */
    if (buf[0] == 'P')
    {
        int min_d = 0, max_d = 0;
        char* s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        int kind = parse_partition_style_kind(buf + 2);
        if (kind < 0) return PARSE_ERROR_GENERIC;

        char* t = strchr(s, ':');
        if (!t) return PARSE_ERROR_GENERIC;
        *t++ = '\0';
        min_d = atoi(s);
        /* Decide if this is a range by checking for a ':' before any space after the first ':' */
        char* first_space = strchr(t, ' ');
        char* second_colon = strchr(t, ':');
        char* list_start;
        if (second_colon && (!first_space || second_colon < first_space)) {
            /* Range form: P:type:min:max: ... */
            *second_colon = '\0';
            max_d = atoi(t);
            list_start = second_colon + 1;
        } else {
            /* Exact form: P:type:depth: ... */
            max_d = min_d;
            list_start = t;
        }

        int sidx[64]; int wt[64]; int n = 0;
        while (*list_start)
        {
            while (*list_start == ' ') list_start++;
            if (!*list_start) break;
            char* e = list_start; while (*e && *e != ' ') e++;
            char hold = *e; if (*e) *e = '\0';
            char* c = strchr(list_start, ':'); if (!c) { if (hold) *e = hold; break; }
            *c = '\0';
            int si = atoi(list_start); int w = atoi(c + 1);
            if (si >= 0 && w > 0 && n < 64) { sidx[n] = si; wt[n] = w; n++; }
            if (hold) { *e = hold; list_start = e + 1; } else break;
        }
        if (n > 0) {
            if (min_d < 1) min_d = 1;
            if (max_d > 31) max_d = 31;
            for (int d = min_d; d <= max_d; ++d)
                styles_add_partition_rule(d, kind, sidx, wt, n);
            log_debug("parse_style_levels: P:kind=%d %d..%d with %d entries (first sidx=%d w=%d)",
                kind, min_d, max_d, n, sidx[0], wt[0]);
        }
        return 0;
    }
    /* B:depth: ICE:w FIRE:w POIS:w  or  B:min:max: ... (big cave type weights) */
    if (buf[0] == 'B')
    {
        int min_d = 0, max_d = 0;
        char* s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        min_d = atoi(buf + 2);
        char* first_space = strchr(s, ' ');
        char* second_colon = strchr(s, ':');
        char* list_start;
        if (second_colon && (!first_space || second_colon < first_space)) {
            *second_colon = '\0';
            max_d = atoi(s);
            list_start = second_colon + 1;
        } else {
            max_d = min_d;
            list_start = s;
        }

        int ice_w = 0, fire_w = 0, pois_w = 0;
        while (*list_start)
        {
            while (*list_start == ' ') list_start++;
            if (!*list_start) break;
            char* e = list_start; while (*e && *e != ' ') e++;
            char hold = *e; if (*e) *e = '\0';
            char* c = strchr(list_start, ':'); if (!c) { if (hold) *e = hold; break; }
            *c = '\0';
            int kind = parse_big_cave_weight_token(list_start);
            int w = atoi(c + 1);
            if (w < 0) w = 0;
            if (kind == BIG_CAVE_ICE) ice_w = w;
            else if (kind == BIG_CAVE_FIRE) fire_w = w;
            else if (kind == BIG_CAVE_POIS) pois_w = w;
            if (hold) { *e = hold; list_start = e + 1; } else break;
        }

        if (min_d < 1) min_d = 1;
        if (max_d > 31) max_d = 31;
        for (int d = min_d; d <= max_d; ++d)
            big_cave_type_set_rule(d, ice_w, fire_w, pois_w);
        log_debug("parse_style_levels: B:%d..%d ice=%d fire=%d pois=%d",
            min_d, max_d, ice_w, fire_w, pois_w);
        return 0;
    }
    /* U:depth: sidx:weight ...   or   U:*: sidx:weight ... (default vault styles) */
    if (buf[0] == 'U')
    {
        char* s = strchr(buf + 2, ':'); if (!s) return PARSE_ERROR_GENERIC; *s++ = '\0';
        const char* depth_tok = buf + 2;
        int sidx[64]; int wt[64]; int n = 0;
        while (*s)
        {
            while (*s == ' ') s++;
            if (!*s) break;
            char* e = s; while (*e && *e != ' ') e++;
            char hold = *e; if (*e) *e = '\0';
            char* c = strchr(s, ':'); if (!c) { if (hold) *e = hold; break; }
            *c = '\0';
            int si = (s[0] == '*' && s[1] == '\0') ? -1 : atoi(s);
            int w = atoi(c + 1);
            if (w > 0 && n < 64) { sidx[n] = si; wt[n] = w; n++; }
            if (hold) { *e = hold; s = e + 1; } else break;
        }
        if (depth_tok[0] == '*' && depth_tok[1] == '\0') {
            styles_default_vault_clear();
            for (int i = 0; i < n; ++i) styles_default_vault_add(sidx[i], wt[i]);
            log_debug("parse_style_levels: U:* default with %d entries (first=%d:%d)", n, sidx[0], wt[0]);
        } else {
            int d = atoi(depth_tok);
            if (n > 0) {
                styles_set_vault_rule(d, sidx, wt, n);
                log_debug("parse_style_levels: U:%d with %d entries (first=%d:%d)", d, n, sidx[0], wt[0]);
            }
        }
        return 0;
    }
    return 0;
}

/* ====================  partition.txt parser  ===================== */

static level_partition_kind parse_partition_kind_token(const char* tok)
{
    if (!tok) return LEVEL_PART_NONE;
    if (SDL_strcasecmp(tok, "ROOMY") == 0) return LEVEL_PART_ROOMY;
    if (SDL_strcasecmp(tok, "CAVEY") == 0) return LEVEL_PART_CAVEY;
    if (SDL_strcasecmp(tok, "RUINED") == 0) return LEVEL_PART_RUINED;
    if (SDL_strcasecmp(tok, "LABYRINTH") == 0 || SDL_strcasecmp(tok, "LAB") == 0)
        return LEVEL_PART_LABYRINTH;
    if (SDL_strcasecmp(tok, "CHASM") == 0) return LEVEL_PART_CHASM;
    if (SDL_strcasecmp(tok, "BIG_CAVE") == 0
        || SDL_strcasecmp(tok, "BIGCAVE") == 0
        || SDL_strcasecmp(tok, "BIG-CAVE") == 0
        || SDL_strcasecmp(tok, "BIG") == 0)
    {
        return LEVEL_PART_BIG_CAVE;
    }
    return LEVEL_PART_NONE;
}

static errr parse_partition_profile_values(const char* data,
    level_partition_kind kind, partition_drop_source_t source)
{
    drop_profile profile;
    int weapon, armor, jewelry, supply;
    int potion, herb, gem, staff, light, arrows, tunneling;
    int allow_damaged = 0;
    int parsed = 0;

    drop_profile_default(&profile);

    parsed = sscanf(data, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
        &weapon, &armor, &jewelry, &supply, &potion, &herb, &gem, &staff,
        &light, &arrows, &tunneling, &allow_damaged);
    if (parsed != 11 && parsed != 12)
    {
        return PARSE_ERROR_GENERIC;
    }

    profile.weight_weapon = weapon;
    profile.weight_armor = armor;
    profile.weight_jewelry = jewelry;
    profile.weight_supply = supply;
    profile.supply_potion = potion;
    profile.supply_herb = herb;
    profile.supply_gem = gem;
    profile.supply_staff = staff;
    profile.supply_light = light;
    profile.supply_arrows = arrows;
    profile.supply_tunneling = tunneling;
    profile.allow_damaged = (parsed == 12) ? (allow_damaged ? true : false) : false;

    partition_config_set_drop_profile(kind, source, &profile);
    return 0;
}

errr parse_partition_info(char* buf, header* head)
{
    static level_partition_kind current_kind = LEVEL_PART_NONE;

    (void)head;

    if (buf[0] == 'V')
    {
        partition_config_reset();
        current_kind = LEVEL_PART_NONE;
        return 0;
    }

    if (buf[0] == '#' || buf[0] == '\0')
        return 0;

    if (buf[0] == 'N' && buf[1] == ':')
    {
        current_kind = parse_partition_kind_token(buf + 2);
        if (current_kind <= LEVEL_PART_NONE || current_kind >= LEVEL_PART_MAX)
            return PARSE_ERROR_GENERIC;
        return 0;
    }

    if (current_kind <= LEVEL_PART_NONE || current_kind >= LEVEL_PART_MAX)
        return PARSE_ERROR_MISSING_RECORD_HEADER;

    if (buf[0] == 'P' && buf[1] == 'F' && buf[2] == ':')
        return parse_partition_profile_values(
            buf + 3, current_kind, PARTITION_DROP_SOURCE_FLOOR);

    if (buf[0] == 'P' && buf[1] == 'C' && buf[2] == ':')
        return parse_partition_profile_values(
            buf + 3, current_kind, PARTITION_DROP_SOURCE_CHEST);

    if (buf[0] == 'P' && buf[1] == 'M' && buf[2] == ':')
        return parse_partition_profile_values(
            buf + 3, current_kind, PARTITION_DROP_SOURCE_MONSTER);

    if (buf[0] == 'F' && buf[1] == 'L' && buf[2] == ':')
    {
        int allow_floor = 0;
        if (1 != sscanf(buf + 3, "%d", &allow_floor))
            return PARSE_ERROR_GENERIC;
        partition_config_set_floor_rules(
            current_kind, allow_floor ? true : false);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'B' && buf[2] == ':')
    {
        int numerator = 0;
        int denominator = 0;
        if (2 != sscanf(buf + 3, "%d:%d", &numerator, &denominator))
            return PARSE_ERROR_GENERIC;
        partition_config_set_base_monster_scale(
            current_kind, numerator, denominator);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'F' && buf[2] == ':')
    {
        int divisor = 0;
        int min_count = 0;
        int max_count = 0;
        if (3 != sscanf(buf + 3, "%d:%d:%d",
                &divisor, &min_count, &max_count))
        {
            return PARSE_ERROR_GENERIC;
        }
        partition_config_set_direct_monster_rule(
            current_kind, divisor, min_count, max_count);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'D' && buf[2] == ':')
    {
        int divisor = 0;
        int min_count = 0;
        int max_count = 0;
        int scale_pct_at_depth_20 = 0;
        int hard_cap_divisor = 0;
        if (5 != sscanf(buf + 3, "%d:%d:%d:%d:%d",
                &divisor, &min_count, &max_count,
                &scale_pct_at_depth_20, &hard_cap_divisor))
        {
            return PARSE_ERROR_GENERIC;
        }
        partition_config_set_depth_monster_rule(current_kind, divisor,
            min_count, max_count, scale_pct_at_depth_20, hard_cap_divisor);
        return 0;
    }

    if (buf[0] == 'O' && buf[1] == 'D' && buf[2] == ':')
    {
        int room_divisor = 0;
        int corridor_divisor = 0;
        if (2 != sscanf(buf + 3, "%d:%d", &room_divisor, &corridor_divisor))
            return PARSE_ERROR_GENERIC;
        partition_config_set_object_rules(
            current_kind, room_divisor, corridor_divisor);
        return 0;
    }

    if (buf[0] == 'M' && buf[1] == 'T' && buf[2] == ':')
    {
        int divisor = 0;
        int min_count = 0;
        int max_count = 0;
        int min_depth = 0;
        if (4 != sscanf(buf + 3, "%d:%d:%d:%d",
                &divisor, &min_count, &max_count, &min_depth))
        {
            return PARSE_ERROR_GENERIC;
        }
        partition_config_set_metal_rule(
            current_kind, divisor, min_count, max_count, min_depth);
        return 0;
    }

    if (buf[0] == 'T' && buf[1] == ':')
    {
        partition_config_set_discovery_text(current_kind, buf + 2);
        return 0;
    }

    if (current_kind == LEVEL_PART_BIG_CAVE && buf[0] == 'B'
        && buf[2] == ':')
    {
        if (buf[1] == 'I')
        {
            partition_config_set_big_cave_discovery_text(
                BIG_CAVE_ICE, buf + 3);
            return 0;
        }
        if (buf[1] == 'F')
        {
            partition_config_set_big_cave_discovery_text(
                BIG_CAVE_FIRE, buf + 3);
            return 0;
        }
        if (buf[1] == 'P')
        {
            partition_config_set_big_cave_discovery_text(
                BIG_CAVE_POIS, buf + 3);
            return 0;
        }
    }

    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

/* ====================  style narrative strings (S:/M1:/M2:)  ===================== */

/*
 * Three-field narrative system per style:
 *   S:  - Short descriptor tag (~3-8 words) for transition composition
 *   M1: - Physical entry description (shown on fresh entry)
 *   M2: - Lore/atmosphere (always shown)
 *
 * Legacy M: lines are treated as M1: for backward compatibility.
 */
#define MAX_STYLE_MSG 8
static const char* g_style_short_desc[128];
static const char* g_style_m1_text[128][MAX_STYLE_MSG];
static byte g_style_m1_count[128];
static const char* g_style_m2_text[128][MAX_STYLE_MSG];
static byte g_style_m2_count[128];

/* Accessor: get short descriptor for transition composition */
const char* styles_get_style_short_desc(int sidx)
{
    if (sidx < 0 || sidx >= 128) return NULL;
    return g_style_short_desc[sidx];
}

/* Accessor: get random M1 (physical entry) */
const char* styles_get_style_m1(int sidx)
{
    if (sidx < 0 || sidx >= 128) return NULL;
    byte n = g_style_m1_count[sidx];
    if (!n) return NULL;
    int pick = (n == 1) ? 0 : rand_int(n);
    return g_style_m1_text[sidx][pick];
}

/* Accessor: get random M2 (lore/atmosphere) */
const char* styles_get_style_m2(int sidx)
{
    if (sidx < 0 || sidx >= 128) return NULL;
    byte n = g_style_m2_count[sidx];
    if (!n) return NULL;
    int pick = (n == 1) ? 0 : rand_int(n);
    return g_style_m2_text[sidx][pick];
}

/* Legacy accessor: compose M1+M2 for backward compat (level entry banner) */
const char* styles_get_style_display(int sidx)
{
    return styles_get_style_m1(sidx);
}

/*
 * Helper: trim a text line (leading/trailing whitespace, optional quotes,
 * trailing #comment). Returns pointer into the buffer.
 */
static char* trim_narrative_text(char* s)
{
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '"' || *s == '\'') { char q = *s++; char* e = strrchr(s, q); if (e) *e = '\0'; }
    char* h = strchr(s, '#'); if (h) *h = '\0';
    for (char* t = s + strlen(s) - 1; t >= s && (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n'); --t) *t = '\0';
    return s;
}

/*
 * Parse a narrative index from a line like "M1:<idx>: <text>" or "M1: <text>".
 * The prefix has already been stripped; colon_start points at the first ':'.
 */
static int parse_narrative_idx(char* colon_start, char** out_text)
{
    if (*colon_start != ':') return -1;
    char* p = colon_start + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p >= '0' && *p <= '9')
    {
        char* c = strchr(p, ':');
        if (!c) return -1;
        *c++ = '\0';
        *out_text = c;
        return atoi(p);
    }
    *out_text = p;
    return error_idx;
}

static errr parse_style_short_desc_line(char* buf)
{
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 1, &text);
    if (idx < 0 || idx >= 128 || !text) return PARSE_ERROR_GENERIC;
    text = trim_narrative_text(text);
    if (g_style_short_desc[idx]) str_free(g_style_short_desc[idx]);
    g_style_short_desc[idx] = str_dup(text);
    return 0;
}

static errr parse_style_m1_line(char* buf)
{
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 2, &text);
    if (idx < 0 || idx >= 128 || !text) return PARSE_ERROR_GENERIC;
    text = trim_narrative_text(text);
    if (g_style_m1_count[idx] >= MAX_STYLE_MSG) {
        log_debug("parse_style_m1_line: style %d M1 list full, dropping", idx);
        return 0;
    }
    g_style_m1_text[idx][g_style_m1_count[idx]] = str_dup(text);
    g_style_m1_count[idx]++;
    return 0;
}

static errr parse_style_m2_line(char* buf)
{
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 2, &text);
    if (idx < 0 || idx >= 128 || !text) return PARSE_ERROR_GENERIC;
    text = trim_narrative_text(text);
    if (g_style_m2_count[idx] >= MAX_STYLE_MSG) {
        log_debug("parse_style_m2_line: style %d M2 list full, dropping", idx);
        return 0;
    }
    g_style_m2_text[idx][g_style_m2_count[idx]] = str_dup(text);
    g_style_m2_count[idx]++;
    return 0;
}

static errr parse_style_message_line(char* buf)
{
    if (buf[0] != 'M') return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    char* text = NULL;
    int idx = parse_narrative_idx(buf + 1, &text);
    if (idx < 0 || idx >= 128 || !text) return PARSE_ERROR_GENERIC;
    text = trim_narrative_text(text);
    if (g_style_m1_count[idx] >= MAX_STYLE_MSG) {
        log_debug("parse_style_message_line: style %d M1 list full, dropping (legacy M:)", idx);
        return 0;
    }
    g_style_m1_text[idx][g_style_m1_count[idx]] = str_dup(text);
    g_style_m1_count[idx]++;
    return 0;
}

/* Clear all loaded per-style narrative messages (free and NULL them). */
void styles_clear_display_messages(void)
{
    for (int i = 0; i < 128; ++i)
    {
        if (g_style_short_desc[i]) {
            str_free(g_style_short_desc[i]);
            g_style_short_desc[i] = NULL;
        }
        for (int j = 0; j < MAX_STYLE_MSG; ++j) {
            if (g_style_m1_text[i][j]) {
                str_free(g_style_m1_text[i][j]);
                g_style_m1_text[i][j] = NULL;
            }
            if (g_style_m2_text[i][j]) {
                str_free(g_style_m2_text[i][j]);
                g_style_m2_text[i][j] = NULL;
            }
        }
        g_style_m1_count[i] = 0;
        g_style_m2_count[i] = 0;
    }
}

/* Reload S:/M1:/M2:/M: lines from style.txt so narratives are available even if RAW cache was used. */
void styles_reload_messages_from_text(void)
{
    char path[1024];
    SDL_IOStream* fp;
    char buf[1024];
    /* Start clean to avoid stale/duplicate entries */
    styles_clear_display_messages();

    /* Build full path to lib/edit/style.txt */
    if (!path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "style")))
    {
        log_warn("styles_reload_messages_from_text: failed to build style.txt path");
        return;
    }
    fp = sdl_fopen(path, "r");
    if (!fp)
    {
        log_warn("styles_reload_messages_from_text: couldn't open %s", path);
        return;
    }

    /* We need to maintain the current style index (error_idx) for in-record lines */
    error_idx = -1;

    while (sdl_fgets(fp, buf, sizeof(buf)) == 0)
    {
        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\0' || *s == '#') continue;
        if (*s == 'N')
        {
            char* colon = strchr(s + 2, ':');
            if (!colon) continue;
            *colon = '\0';
            error_idx = atoi(s + 2);
            continue;
        }
        if (s[0] == 'S' && s[1] == ':')
        {
            (void)parse_style_short_desc_line(s);
            continue;
        }
        if (s[0] == 'M' && s[1] == '1' && s[2] == ':')
        {
            (void)parse_style_m1_line(s);
            continue;
        }
        if (s[0] == 'M' && s[1] == '2' && s[2] == ':')
        {
            (void)parse_style_m2_line(s);
            continue;
        }
        if (s[0] == 'M' && s[1] == ':')
        {
            /* Legacy M: treated as M1: */
            (void)parse_style_message_line(s);
            continue;
        }
    }
    sdl_fclose(fp);
    log_info("styles_reload_messages_from_text: loaded per-style narrative text");
}

#endif /* ALLOW_TEMPLATES */