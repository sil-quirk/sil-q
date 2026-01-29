/* File: item_set.c */

#include "angband.h"
#include "externs.h"
#include "init.h"
#include "log/log.h"
#include "score/score_guid.h"
#include "item_set.h"

#include <ctype.h>
#include <stdlib.h>

/* --- Data model ---------------------------------------------------- */

#define ITEM_SET_MAX_MEMBERS 16
#define ITEM_SET_NAME_MAX 80

typedef enum item_set_flag
{
    ITEM_SET_FLAG_NONE = 0,
    ITEM_SET_FLAG_PAIRED_WEAPONS = 1 << 0,
} item_set_flag;

typedef struct item_set_type
{
    int id;
    guid64 guid;
    char name[ITEM_SET_NAME_MAX];

    u32b flags;

    int member_count;
    int members[ITEM_SET_MAX_MEMBERS]; /* artefact indices */

    s16b stat_bonus[A_MAX];  /* applied when full set equipped */
    s16b skill_bonus[S_MAX]; /* applied when full set equipped */
} item_set_type;

static item_set_type* set_info = NULL;
static int set_count = 0;
static int set_capacity = 0;

static item_set_type* set_cur = NULL;

/* For fast paired-weapon lookup: partner artefact index (0 if none). */
static int* paired_partner = NULL;

/* --- Helpers ------------------------------------------------------- */

static void trim_inplace(char* s)
{
    if (!s)
        return;

    /* left trim */
    char* p = s;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, SDL_strlen(p) + 1);

    /* right trim */
    size_t len = SDL_strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static void strip_single_quotes_inplace(char* s)
{
    if (!s)
        return;

    size_t len = SDL_strlen(s);
    if (len >= 2 && s[0] == '\'' && s[len - 1] == '\'')
    {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static bool is_weapon_tval(byte tval)
{
    return (tval == TV_SWORD) || (tval == TV_POLEARM) || (tval == TV_HAFTED)
        || (tval == TV_DIGGING);
}

static bool item_set_apply_flag(item_set_type* set, const char* token)
{
    if (!set || !token)
        return false;

    if (SDL_strcasecmp(token, "PAIRED_WEAPONS") == 0
        || SDL_strcasecmp(token, "PAIRED_WEAPON") == 0
        || SDL_strcasecmp(token, "PAIRED") == 0)
    {
        set->flags |= ITEM_SET_FLAG_PAIRED_WEAPONS;
        return true;
    }

    return false;
}

static bool item_set_apply_bonus(item_set_type* set, const char* token, int value)
{
    if (!set || !token)
        return false;

    bool has_neg_prefix = false;
    const char* name = token;
    if (SDL_strncasecmp(name, "NEG_", 4) == 0)
    {
        has_neg_prefix = true;
        name += 4;
    }

    int normalized = value;
    if (has_neg_prefix && normalized > 0)
        normalized = -normalized;

    /* Stats */
    if (SDL_strcasecmp(name, "STR") == 0)
    {
        set->stat_bonus[A_STR] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "DEX") == 0)
    {
        set->stat_bonus[A_DEX] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "CON") == 0)
    {
        set->stat_bonus[A_CON] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "GRA") == 0)
    {
        set->stat_bonus[A_GRA] += (s16b)normalized;
        return true;
    }

    /* Skills (support both long and short tokens) */
    if (SDL_strcasecmp(name, "MELEE") == 0 || SDL_strcasecmp(name, "MEL") == 0)
    {
        set->skill_bonus[S_MEL] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "ARCHERY") == 0 || SDL_strcasecmp(name, "ARC") == 0)
    {
        set->skill_bonus[S_ARC] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "STEALTH") == 0 || SDL_strcasecmp(name, "STL") == 0)
    {
        set->skill_bonus[S_STL] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "PERCEPTION") == 0 || SDL_strcasecmp(name, "PER") == 0)
    {
        set->skill_bonus[S_PER] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "WILL") == 0 || SDL_strcasecmp(name, "WIL") == 0)
    {
        set->skill_bonus[S_WIL] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "SMITHING") == 0 || SDL_strcasecmp(name, "SMT") == 0)
    {
        set->skill_bonus[S_SMT] += (s16b)normalized;
        return true;
    }
    if (SDL_strcasecmp(name, "SONG") == 0 || SDL_strcasecmp(name, "SNG") == 0)
    {
        set->skill_bonus[S_SNG] += (s16b)normalized;
        return true;
    }

    return false;
}

static int item_set_find_art_by_guid_u64(u64b guid_u64)
{
    if (!a_info || !z_info)
        return 0;

    guid64 guid = score_guid_from_u64(guid_u64);

    for (int i = 0; i < z_info->art_max; i++)
    {
        const artefact_type* a_ptr = &a_info[i];
        if (!a_ptr)
            continue;

        if (a_ptr->guid.hi == guid.hi && a_ptr->guid.lo == guid.lo)
            return i;
    }

    return 0;
}

static int item_set_find_art_by_name(const char* raw_name, bool* out_ambiguous)
{
    if (out_ambiguous)
        *out_ambiguous = false;

    if (!a_info || !z_info || !raw_name || !raw_name[0])
        return 0;

    char needle[128];
    SDL_strlcpy(needle, raw_name, sizeof(needle));
    trim_inplace(needle);
    strip_single_quotes_inplace(needle);
    trim_inplace(needle);

    int found = 0;

    for (int i = 0; i < z_info->art_max; i++)
    {
        const artefact_type* a_ptr = &a_info[i];
        if (!a_ptr || !a_ptr->name[0])
            continue;

        char candidate[128];
        SDL_strlcpy(candidate, a_ptr->name, sizeof(candidate));
        trim_inplace(candidate);
        strip_single_quotes_inplace(candidate);
        trim_inplace(candidate);

        if (SDL_strcasecmp(candidate, needle) == 0)
        {
            if (found && out_ambiguous)
            {
                *out_ambiguous = true;
                return 0;
            }
            found = i;
        }
    }

    return found;
}

static int item_set_parse_member_token(const char* token)
{
    if (!token || !token[0] || !z_info)
        return 0;

    char tmp[256];
    SDL_strlcpy(tmp, token, sizeof(tmp));
    trim_inplace(tmp);

    /* Name form (allows spaces): A:'Orcrist' */
    if (tmp[0] == '\'')
    {
        bool ambiguous = false;
        int art = item_set_find_art_by_name(tmp, &ambiguous);
        if (!art && ambiguous)
        {
            msg_format("set.txt: Artefact name '%s' is ambiguous; use artefact index or GUID.",
                tmp);
            return 0;
        }
        return art;
    }

    /* Heuristic: treat as GUID if it contains hex letters/separators/prefix, or is 16+ chars */
    bool looks_like_guid = false;
    size_t len = SDL_strlen(tmp);
    if (len >= 16)
        looks_like_guid = true;
    for (size_t i = 0; i < len && !looks_like_guid; i++)
    {
        char c = tmp[i];
        if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-' || c == '_' || c == 'x' || c == 'X')
            looks_like_guid = true;
    }

    if (looks_like_guid)
    {
        u64b guid = 0;
        if (!parse_u64b_hex(tmp, &guid))
            return 0;
        return item_set_find_art_by_guid_u64(guid);
    }

    /* Decimal artefact index form: A:64 */
    char* end = NULL;
    long idx = strtol(tmp, &end, 10);
    if (!end || *end != '\0')
        return 0;
    if (idx <= 0 || idx >= z_info->art_max)
        return 0;
    return (int)idx;
}

static errr item_set_validate_one(const item_set_type* set)
{
    if (!set)
        return PARSE_ERROR_GENERIC;

    if (set->member_count < 2)
    {
        msg_format("set.txt: Set %d ('%s') must list at least 2 artefacts (A: lines).",
            set->id, set->name[0] ? set->name : "unnamed");
        return PARSE_ERROR_GENERIC;
    }

    if (!a_info || !z_info)
        return PARSE_ERROR_GENERIC;

    for (int mi = 0; mi < set->member_count; mi++)
    {
        int art = set->members[mi];
        if (art <= 0 || art >= z_info->art_max || a_info[art].tval == 0)
        {
            msg_format("set.txt: Set %d ('%s') references unknown artefact index %d.",
                set->id, set->name[0] ? set->name : "unnamed", art);
            return PARSE_ERROR_GENERIC;
        }
    }

    if (set->flags & ITEM_SET_FLAG_PAIRED_WEAPONS)
    {
        if (set->member_count != 2)
        {
            msg_format("set.txt: Set %d ('%s') has PAIRED_WEAPONS but does not have exactly 2 members.",
                set->id, set->name[0] ? set->name : "unnamed");
            return PARSE_ERROR_GENERIC;
        }

        int a = set->members[0];
        int b = set->members[1];
        if (a <= 0 || a >= z_info->art_max || b <= 0 || b >= z_info->art_max)
            return PARSE_ERROR_GENERIC;

        const artefact_type* a_ptr = &a_info[a];
        const artefact_type* b_ptr = &a_info[b];
        if (!is_weapon_tval(a_ptr->tval) || !is_weapon_tval(b_ptr->tval))
        {
            msg_format("set.txt: Set %d ('%s') PAIRED_WEAPONS members must both be weapons.",
                set->id, set->name[0] ? set->name : "unnamed");
            return PARSE_ERROR_GENERIC;
        }
    }

    return 0;
}

static bool item_sets_ensure_capacity(int needed)
{
    if (needed <= set_capacity)
        return true;

    int new_cap = set_capacity ? set_capacity * 2 : 8;
    while (new_cap < needed)
        new_cap *= 2;

    item_set_type* resized = SDL_realloc(set_info, (size_t)new_cap * sizeof(*set_info));
    if (!resized)
        return false;

    if (new_cap > set_capacity)
    {
        memset(resized + set_capacity, 0, (size_t)(new_cap - set_capacity) * sizeof(*set_info));
    }

    set_info = resized;
    set_capacity = new_cap;
    return true;
}

/* --- Public API ---------------------------------------------------- */

void item_sets_reset(void)
{
    mem_free_null(set_info);
    set_count = 0;
    set_capacity = 0;
    set_cur = NULL;

    mem_free_null(paired_partner);
}

#ifdef ALLOW_TEMPLATES
errr parse_set_info(char* buf, header* head)
{
    (void)head;

    char* s;
    char* t;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Validate the previous record (if any). */
        if (set_cur)
        {
            errr verr = item_set_validate_one(set_cur);
            if (verr)
                return verr;
        }

        s = strchr(buf + 2, ':');
        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        if (!*s)
            return PARSE_ERROR_GENERIC;

        int id = atoi(buf + 2);
        if (id <= error_idx)
            return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;

        error_idx = id;

        if (!item_sets_ensure_capacity(set_count + 1))
            return PARSE_ERROR_OUT_OF_MEMORY;

        set_cur = &set_info[set_count++];
        memset(set_cur, 0, sizeof(*set_cur));
        set_cur->id = id;
        SDL_strlcpy(set_cur->name, s, sizeof(set_cur->name));
        return 0;
    }

    if (!set_cur)
        return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* Process 'Q' for GUID */
    if (buf[0] == 'Q')
    {
        u64b guid = 0;
        if (!parse_u64b_hex(buf + 2, &guid))
            return PARSE_ERROR_GENERIC;

        set_cur->guid = score_guid_from_u64(guid);
        return 0;
    }

    /* Process 'A' for artefact member */
    if (buf[0] == 'A')
    {
        if (set_cur->member_count >= ITEM_SET_MAX_MEMBERS)
            return PARSE_ERROR_TOO_MANY_ENTRIES;

        int art = item_set_parse_member_token(buf + 2);
        if (!art)
        {
            msg_format("set.txt: Unknown artefact reference '%s' in set %d ('%s').",
                buf + 2, set_cur->id, set_cur->name);
            return PARSE_ERROR_GENERIC;
        }

        for (int i = 0; i < set_cur->member_count; i++)
        {
            if (set_cur->members[i] == art)
            {
                msg_format("set.txt: Duplicate artefact %d in set %d ('%s').",
                    art, set_cur->id, set_cur->name);
                return PARSE_ERROR_GENERIC;
            }
        }

        set_cur->members[set_cur->member_count++] = art;
        return 0;
    }

    /* Process 'F' for flags */
    if (buf[0] == 'F')
    {
        s = buf + 2;
        while (s && *s)
        {
            t = strchr(s, '|');
            if (t)
                *t++ = '\0';

            trim_inplace(s);
            if (*s)
            {
                if (!item_set_apply_flag(set_cur, s))
                {
                    msg_format("set.txt: Unknown set flag '%s' in set %d ('%s').",
                        s, set_cur->id, set_cur->name);
                    return PARSE_ERROR_INVALID_FLAG;
                }
            }

            s = t;
        }
        return 0;
    }

    /* Process 'M' for stat/skill modifier bonus (full set only) */
    if (buf[0] == 'M')
    {
        s = strchr(buf + 2, ':');
        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        trim_inplace(buf + 2);
        trim_inplace(s);
        if (!buf[2] || !*s)
            return PARSE_ERROR_GENERIC;

        int value = atoi(s);
        if (!item_set_apply_bonus(set_cur, buf + 2, value))
        {
            msg_format("set.txt: Unknown bonus token '%s' in set %d ('%s').",
                buf + 2, set_cur->id, set_cur->name);
            return PARSE_ERROR_GENERIC;
        }

        return 0;
    }

    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}
#endif /* ALLOW_TEMPLATES */

errr item_sets_finalize(void)
{
    if (set_cur)
    {
        errr verr = item_set_validate_one(set_cur);
        if (verr)
            return verr;
    }

    mem_free_null(paired_partner);

    if (!z_info || z_info->art_max <= 0)
        return 0;

    paired_partner = mem_alloc_array(z_info->art_max, int);
    if (!paired_partner)
        return PARSE_ERROR_OUT_OF_MEMORY;

    for (int i = 0; i < set_count; i++)
    {
        const item_set_type* set = &set_info[i];
        if (!set)
            continue;

        if (!(set->flags & ITEM_SET_FLAG_PAIRED_WEAPONS))
            continue;

        if (set->member_count != 2)
            continue;

        int a = set->members[0];
        int b = set->members[1];

        if (a <= 0 || b <= 0 || a >= z_info->art_max || b >= z_info->art_max)
            return PARSE_ERROR_GENERIC;

        if ((paired_partner[a] && paired_partner[a] != b) || (paired_partner[b] && paired_partner[b] != a))
        {
            msg_format("set.txt: Artefact %d is in multiple paired-weapon sets (conflict).", a);
            return PARSE_ERROR_GENERIC;
        }

        paired_partner[a] = b;
        paired_partner[b] = a;

        /* Ensure TR4_PAIRED is present for UI + difficulty calculations. */
        if (a_info)
        {
            a_info[a].flags4 |= TR4_PAIRED;
            a_info[b].flags4 |= TR4_PAIRED;
        }
    }

    log_info("Loaded %d item sets", set_count);
    return 0;
}

int item_sets_get_paired_artefact(int art_idx)
{
    if (!paired_partner || !z_info)
        return 0;
    if (art_idx <= 0 || art_idx >= z_info->art_max)
        return 0;
    return paired_partner[art_idx];
}

static bool item_set_is_complete(const item_set_type* set)
{
    if (!set || set->member_count <= 0)
        return false;

    for (int mi = 0; mi < set->member_count; mi++)
    {
        int art = set->members[mi];
        bool found = false;

        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            const object_type* o_ptr = &inventory[i];
            if (!o_ptr->k_idx)
                continue;
            if (o_ptr->name1 == art)
            {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}

void item_sets_apply_player_bonuses(void)
{
    if (!set_info || set_count <= 0 || !p_ptr)
        return;

    for (int si = 0; si < set_count; si++)
    {
        const item_set_type* set = &set_info[si];
        if (!set)
            continue;

        if (!item_set_is_complete(set))
            continue;

        for (int i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_equip_mod[i] += set->stat_bonus[i];
        }

        for (int i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_equip_mod[i] += set->skill_bonus[i];
        }
    }
}
