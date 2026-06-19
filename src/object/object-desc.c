/* File: object/object-desc.c */

#include "angband.h"
#include "externs.h"
#include "object/object-desc.h"
#include "object/object-internal.h"
#include "log/log.h"
#include <ctype.h>


/*
 * Efficient version of '(T) += sprintf((T), "%c", (C))'
 */
#define object_desc_chr_macro(T, C)                                            \
    do                                                                         \
    {                                                                          \
        /* Copy the char */                                                    \
        *(T)++ = (C);                                                          \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%s", (S))'
 */
#define object_desc_str_macro(T, S)                                            \
    do                                                                         \
    {                                                                          \
        cptr s = (S);                                                          \
                                                                               \
        /* Copy the string */                                                  \
        while (*s)                                                             \
            *(T)++ = *s++;                                                     \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%u", (N))'
 */
#define object_desc_num_macro(T, N)                                            \
    do                                                                         \
    {                                                                          \
        int n = (N);                                                           \
                                                                               \
        int p;                                                                 \
                                                                               \
        /* Find "size" of "n" */                                               \
        for (p = 1; n >= p * 10; p = p * 10) /* loop */                        \
            ;                                                                  \
                                                                               \
        /* Dump each digit */                                                  \
        while (p >= 1)                                                         \
        {                                                                      \
            /* Dump the digit */                                               \
            *(T)++ = I2D(n / p);                                               \
                                                                               \
            /* Remove the digit */                                             \
            n = n % p;                                                         \
                                                                               \
            /* Process next digit */                                           \
            p = p / 10;                                                        \
        }                                                                      \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%+d", (I))'
 */
#define object_desc_int_macro(T, I)                                            \
    do                                                                         \
    {                                                                          \
        int i = (I);                                                           \
                                                                               \
        /* Negative */                                                         \
        if (i < 0)                                                             \
        {                                                                      \
            /* Take the absolute value */                                      \
            i = 0 - i;                                                         \
                                                                               \
            /* Use a "minus" sign */                                           \
            *(T)++ = '-';                                                      \
        }                                                                      \
                                                                               \
        /* Positive (or zero) */                                               \
        else                                                                   \
        {                                                                      \
            /* Use a "plus" sign */                                            \
            *(T)++ = '+';                                                      \
        }                                                                      \
                                                                               \
        /* Dump the number itself */                                           \
        object_desc_num_macro(T, i);                                           \
                                                                               \
    } while (0)

/*
 * Strip an "object name" into a buffer.
 */
void strip_name(char* buf, int k_idx)
{
    char* t;

    object_kind* k_ptr = &k_info[k_idx];

    cptr str = (k_name + k_ptr->name);

    /* Skip past leading characters */
    while ((*str == ' ') || (*str == '&'))
        str++;

    /* Copy useful chars */
    for (t = buf; *str; str++)
    {
        if (*str != '~')
            *t++ = *str;
    }

    /* Terminate the new name */
    *t = '\0';
}

/*
 * Creates a description of the item "o_ptr", and stores it in "buf".
 *
 * One can choose the "verbosity" of the description, including whether
 * or not the "number" of items should be described, and how much detail
 * should be used when describing the item.
 *
 * The given "buf" should be at least 80 chars long to hold the longest
 * possible description, which can get pretty long, including inscriptions,
 * such as:
 * "no more Maces of Disruption (Defender) (+10,+10) [+5] (+3 to stealth)".

 * Note that the object description will be clipped to fit into the given
 * buffer size.
 *
 * Note the use of "object_desc_int_macro()" and "object_desc_num_macro()"
 * and "object_desc_str_macro()" and "object_desc_chr_macro()" as extremely
 * efficient, portable, versions of some common "sprintf()" commands (without
 * the bounds checking or termination writing), which allow a pointer to
 * efficiently move through a buffer while modifying it in various ways.
 *
 * Various improper uses and/or placements of "&" or "~" characters can
 * easily induce out-of-bounds memory accesses.  Some of these could be
 * easily checked for, if efficiency was not a concern.
 *
 * Note that all special items (when known) append an "Ego-Item Name", unless
 * the item is also an artefact, which should never happen.
 *
 * Note that all artefacts (when known) append an "Artefact Name", so we
 * have special processing for "Specials" (artefact Lites, Rings, Amulets).
 * The "Specials" never use "modifiers" if they are "known", since they
 * have special "descriptions", such as "The Necklace of the Dwarves".
 *
 * Special Lite's use the "k_info" base-name (Phial, Star, or Arkenstone),
 * plus the artefact name, just like any other artefact, if known.
 *
 * Special Ring's and Amulet's, if not "aware", use the same code as normal
 * rings and amulets, and if "aware", use the "k_info" base-name (Ring or
 * Amulet or Necklace).  They will NEVER "append" the "k_info" name.  But,
 * they will append the artefact name, just like any artefact, if known.
 *
 * None of the Special Rings/Amulets are "EASY_KNOW", though they could be,
 * at least, those which have no "pluses", such as the three artefact lites.
 *
 * The "pluralization" rules are extremely hackish, in fact, for efficiency,
 * we only handle things like "torch"/"torches" and "cutlass"/"cutlasses",
 * and we would not handle "box"/"boxes", or "knife"/"knives", correctly.
 * Of course, it would be easy to add rules for these forms.
 *
 * If "pref" is true then a "numeric" prefix will be pre-pended, else is is
 * assumed that a string such as "The" or "Your" will be pre-pended later.
 *
 * Modes ("pref" is true):
 *  -1 -- Chain Mail
 *   0 -- Chain Mail of Death
 *   1 -- A Cloak of Death [1,+3]
 *   2 -- An Amulet of Death [1,+3] <+2>
 *   3 -- 5 Rings of Death [1,+3] <+2> {nifty}
 *
 * Modes ("pref" is false):
 *  -1 -- Chain Mail
 *   0 -- Chain Mail of Death
 *   1 -- Cloak of Death [1,+3]
 *   2 -- Amulet of Death [1,+3] <+2>
 *   3 -- Rings of Death [1,+3] <+2> {nifty}
 */
static void object_desc_trim_spaces(char* s)
{
    if (!s) return;

    char* start = s;
    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static void object_desc_mode4_shorten(char* buf, size_t max, const object_type* o_ptr, bool apply_rules)
{
    if (!buf) return;

    log_debug("mode4_shorten: INPUT buf='%s' max=%zu apply_rules=%d", buf, max, apply_rules);

    char source[256];
    size_t src_idx = 0;
    bool insert_space = false;

    for (size_t i = 0; buf[i] && src_idx < sizeof(source) - 1; ++i)
    {
        unsigned char c = (unsigned char)buf[i];

        if (c < 32)
        {
            if (src_idx > 0 && source[src_idx - 1] != ' ')
                insert_space = true;
            continue;
        }

        if (insert_space && c != ' ' && src_idx < sizeof(source) - 1)
            source[src_idx++] = ' ';

        insert_space = false;

        source[src_idx++] = (char)c;
    }
    source[src_idx] = '\0';

    log_debug("mode4_shorten: after cleanup source='%s'", source);

    SDL_strlcpy(buf, source, max);

    size_t len = strlen(source);
    if (!len)
    {
        log_debug("mode4_shorten: empty source, returning");
        return;
    }

    size_t stats_idx = len;
    for (size_t i = 0; i < len; ++i)
    {
        char c = source[i];
        if (c == '(' || c == '[' || c == '<' || c == '{')
        {
            stats_idx = i;
            break;
        }
    }

    log_debug("mode4_shorten: stats_idx=%zu len=%zu", stats_idx, len);

    char base[256];
    char stats[256];
    base[0] = '\0';
    stats[0] = '\0';

    if (stats_idx < len)
    {
        /* Copy only the characters before stats_idx - strnfmt was not respecting %.*s */
        if (stats_idx > 0 && stats_idx < sizeof(base))
        {
            memcpy(base, source, stats_idx);
            base[stats_idx] = '\0';
        }
        SDL_strlcpy(stats, source + stats_idx, sizeof(stats));
        log_debug("mode4_shorten: split - base='%s' stats='%s'", base, stats);
    }
    else
    {
        SDL_strlcpy(base, source, sizeof(base));
        log_debug("mode4_shorten: no stats found - base='%s'", base);
    }

    object_desc_trim_spaces(base);
    object_desc_trim_spaces(stats);
    
    log_debug("mode4_shorten: after trim - base='%s' stats='%s'", base, stats);

    /* If base name is empty after processing, use the original source instead of stats-only */
    if (!base[0])
    {
        log_debug("mode4_shorten: base is empty! source='%s'", source);
        
        /* Try to extract at least something from the original - fallback to first word */
        const char* first_word_end = source;
        while (*first_word_end && !isspace((unsigned char)*first_word_end) 
               && *first_word_end != '(' && *first_word_end != '[' 
               && *first_word_end != '<' && *first_word_end != '{')
            first_word_end++;
        
        size_t first_word_len = first_word_end - source;
        log_debug("mode4_shorten: first_word_len=%zu", first_word_len);
        
        if (first_word_len > 0 && first_word_len < sizeof(base))
        {
            memcpy(base, source, first_word_len);
            base[first_word_len] = '\0';
            object_desc_trim_spaces(base);
            log_debug("mode4_shorten: extracted first word base='%s'", base);
        }
        
        /* If still nothing, just use the whole source unchanged */
        if (!base[0])
        {
            log_debug("mode4_shorten: still empty, returning source unchanged");
            SDL_strlcpy(buf, source, max);
            return;
        }
    }

    char trailing_suffix[64];
    trailing_suffix[0] = '\0';

    if (apply_rules)
    {
        log_debug("mode4_shorten: applying rules to base='%s'", base);
        
        size_t base_len_tmp = strlen(base);
        size_t idx = base_len_tmp;

        while (idx > 0 && isspace((unsigned char)base[idx - 1]))
            --idx;

        size_t digit_start = idx;
        while (digit_start > 0 && isdigit((unsigned char)base[digit_start - 1]))
            --digit_start;

        if (digit_start < idx)
        {
            size_t copy_len = idx - digit_start;
            if (copy_len < sizeof(trailing_suffix))
            {
                memcpy(trailing_suffix, base + digit_start, copy_len);
                trailing_suffix[copy_len] = '\0';
                base[digit_start] = '\0';
                object_desc_trim_spaces(base);
                log_debug("mode4_shorten: extracted trailing_suffix='%s' base_after='%s'", trailing_suffix, base);
            }
        }
    }
    else
    {
        log_debug("mode4_shorten: skipping rules (artifact)");
    }

    if (!apply_rules)
    {
        char rebuilt_basic[256];
        rebuilt_basic[0] = '\0';
        SDL_strlcpy(rebuilt_basic, base, sizeof(rebuilt_basic));
        if (stats[0])
        {
            if (rebuilt_basic[0])
                SDL_strlcat(rebuilt_basic, " ", sizeof(rebuilt_basic));
            SDL_strlcat(rebuilt_basic, stats, sizeof(rebuilt_basic));
        }
        object_desc_trim_spaces(rebuilt_basic);
        log_debug("mode4_shorten: no rules, OUTPUT='%s'", rebuilt_basic);
        SDL_strlcpy(buf, rebuilt_basic, max);
        return;
    }

    /* Preserve ego prefix in shortened output (prefixes moved from "(Ego)" suffix to leading words). */
    char ego_prefix_label[128];
    ego_prefix_label[0] = '\0';
    if (o_ptr && object_ego_prefix(o_ptr))
    {
        byte e_idx = object_ego_prefix(o_ptr);
        if (e_idx > 0 && e_idx < z_info->e_max && e_info[e_idx].name)
        {
            const char* raw = e_name + e_info[e_idx].name;
            if (ego_name_is_prefix(raw))
            {
                size_t raw_len = strlen(raw);
                size_t copy_len = (raw_len >= 2) ? (raw_len - 2) : 0;
                if (copy_len >= sizeof(ego_prefix_label))
                    copy_len = sizeof(ego_prefix_label) - 1;
                if (copy_len > 0)
                {
                    memcpy(ego_prefix_label, raw + 1, copy_len);
                    ego_prefix_label[copy_len] = '\0';
                    object_desc_trim_spaces(ego_prefix_label);
                }
            }
        }
    }
    if (ego_prefix_label[0])
    {
        /* Only preserve the prefix when it is actually visible in the base name (avoid leaking unknown egos). */
        size_t pre_len = strlen(ego_prefix_label);
        if (strncmp(base, ego_prefix_label, pre_len) != 0
            || (base[pre_len] && !isspace((unsigned char)base[pre_len])))
        {
            ego_prefix_label[0] = '\0';
        }
    }

    log_debug("mode4_shorten: processing 'of' pattern in base='%s'", base);

    /* Determine if this item has an ego suffix enchantment (e.g., "... of Speed") */
    bool has_ego = (o_ptr && object_ego_suffix(o_ptr));
    
    char lower[256];
    size_t base_len = strlen(base);
    for (size_t i = 0; i < base_len && i < sizeof(lower) - 1; ++i)
        lower[i] = (char)tolower((unsigned char)base[i]);
    lower[base_len] = '\0';

    log_debug("mode4_shorten: lower='%s' has_ego=%d", lower, has_ego);

    /*
     * Split on " of " to separate base type from qualifier:
     * - Ego items: split on the LAST " of " (handles phrases like "Pair of Boots of Speed").
     * - Non-ego items: split only when the word before "of" is a known base type
     *   ("Ring of Frost", "Potion of Healing", etc), avoiding cases where "of"
     *   is part of the base phrase.
     */
    char* split_point = NULL;
    if (has_ego)
    {
        /* Find the LAST " of " - this should be the ego enchantment */
        for (char* search = lower; (search = strstr(search, " of ")) != NULL; ++search)
            split_point = search;
    }
    else
    {
        char* first_of = strstr(lower, " of ");
        if (first_of)
        {
            size_t word_end = (size_t)(first_of - lower);
            while (word_end > 0 && isspace((unsigned char)lower[word_end - 1]))
                --word_end;

            size_t word_start = word_end;
            while (word_start > 0 && !isspace((unsigned char)lower[word_start - 1]))
                --word_start;

            char head_word[32];
            size_t word_len = word_end - word_start;
            if (word_len > 0 && word_len < sizeof(head_word))
            {
                memcpy(head_word, lower + word_start, word_len);
                head_word[word_len] = '\0';

                static const char* split_words[] = {
                    "amulet", "amulets",
                    "gem",    "gems",
                    "herb",   "herbs",
                    "horn",   "horns",
                    "potion", "potions",
                    "ring",   "rings",
                    "staff",  "staves",
                };

                for (size_t i = 0; i < N_ELEMENTS(split_words); ++i)
                {
                    if (!strcmp(head_word, split_words[i]))
                    {
                        split_point = first_of;
                        break;
                    }
                }
            }
        }
    }

    char first_part[256];
    char second_part[256];
    first_part[0] = '\0';
    second_part[0] = '\0';

    if (split_point)
    {
        size_t index = (size_t)(split_point - lower);
        if (index < sizeof(first_part))
        {
            memcpy(first_part, base, index);
            first_part[index] = '\0';
        }
        SDL_strlcpy(second_part, base + index + 4, sizeof(second_part));
        log_debug("mode4_shorten: found ego 'of' at %zu - first='%s' second='%s'", index, first_part, second_part);
    }
    else
    {
        SDL_strlcpy(first_part, base, sizeof(first_part));
        log_debug("mode4_shorten: no ego split - first='%s'", first_part);
    }

    object_desc_trim_spaces(first_part);
    object_desc_trim_spaces(second_part);
    
    log_debug("mode4_shorten: after trim - first='%s' second='%s'", first_part, second_part);

    /* If we have a prefix ego, shorten the base type without dropping the prefix. */
    char* first_part_for_short = first_part;
    if (ego_prefix_label[0])
    {
        size_t pre_len = strlen(ego_prefix_label);
        if (!strncmp(first_part_for_short, ego_prefix_label, pre_len))
        {
            char* p = first_part_for_short + pre_len;
            if (*p == '\0')
            {
                first_part_for_short = p;
            }
            else if (isspace((unsigned char)*p))
            {
                while (*p && isspace((unsigned char)*p))
                    ++p;
                first_part_for_short = p;
            }
        }
    }

    char short_first[128];
    short_first[0] = '\0';

    if (first_part_for_short[0])
    {
        log_debug("mode4_shorten: extracting last words from first_part='%s'", first_part_for_short);

        char* cursor = first_part_for_short;
        char* last_start = NULL;
        size_t last_len = 0;
        char* prev_start = NULL;
        size_t prev_len = 0;

        while (*cursor)
        {
            while (*cursor && isspace((unsigned char)*cursor))
                ++cursor;
            if (!*cursor)
                break;

            char* word_start = cursor;
            bool has_alpha = false;
            while (*cursor && !isspace((unsigned char)*cursor))
            {
                if (isalpha((unsigned char)*cursor))
                    has_alpha = true;
                ++cursor;
            }

            size_t word_len = (size_t)(cursor - word_start);
            if (has_alpha && word_len > 0)
            {
                prev_start = last_start;
                prev_len = last_len;
                last_start = word_start;
                last_len = word_len;
                log_debug("mode4_shorten: found alpha word at offset %td len=%zu", word_start - first_part, word_len);
            }
        }

        bool use_prev = false;
        if (apply_rules && last_start && last_len > 0 && prev_start && prev_len > 0)
        {
            char prev_word[32];
            size_t copy_len = prev_len;
            if (copy_len >= sizeof(prev_word))
                copy_len = sizeof(prev_word) - 1;
            memcpy(prev_word, prev_start, copy_len);
            prev_word[copy_len] = '\0';

            for (size_t k = 0; prev_word[k]; ++k)
                prev_word[k] = (char)tolower((unsigned char)prev_word[k]);

            if (strcmp(prev_word, "of") && strcmp(prev_word, "a")
                && strcmp(prev_word, "an") && strcmp(prev_word, "the")
                && strcmp(prev_word, "and"))
            {
                use_prev = true;
            }
        }

        if (last_start && last_len > 0)
        {
            size_t out = 0;

            if (use_prev)
            {
                size_t take_prev = prev_len;
                if (take_prev >= sizeof(short_first))
                    take_prev = sizeof(short_first) - 1;
                memcpy(short_first + out, prev_start, take_prev);
                out += take_prev;

                if (out + 1 < sizeof(short_first))
                    short_first[out++] = ' ';
            }

            size_t remaining = sizeof(short_first) - 1 - out;
            size_t take_last = last_len;
            if (take_last > remaining)
                take_last = remaining;
            memcpy(short_first + out, last_start, take_last);
            out += take_last;

            short_first[out] = '\0';
            object_desc_trim_spaces(short_first);
            log_debug("mode4_shorten: short_first='%s' (use_prev=%d)", short_first, use_prev ? 1 : 0);
        }
    }

    if (ego_prefix_label[0])
    {
        if (short_first[0] && !strncmp(short_first, ego_prefix_label, strlen(ego_prefix_label)))
        {
            /* Already includes prefix; do nothing. */
        }
        else
        {
            char with_prefix[128];
            with_prefix[0] = '\0';
            SDL_strlcpy(with_prefix, ego_prefix_label, sizeof(with_prefix));
            if (short_first[0])
            {
                SDL_strlcat(with_prefix, " ", sizeof(with_prefix));
                SDL_strlcat(with_prefix, short_first, sizeof(with_prefix));
            }
            SDL_strlcpy(short_first, with_prefix, sizeof(short_first));
            object_desc_trim_spaces(short_first);
        }
    }

    char cleaned_second[256];
    cleaned_second[0] = '\0';
    const char* s = second_part;
    while (s && *s)
    {
        while (isspace((unsigned char)*s))
            ++s;
        if (!*s)
            break;

        const char* token_start = s;
        while (*s && !isspace((unsigned char)*s))
            ++s;
        size_t token_len = (size_t)(s - token_start);
        if (!token_len)
            continue;

        char token[64];
        if (token_len < sizeof(token))
        {
            memcpy(token, token_start, token_len);
            token[token_len] = '\0';
        }
        else
        {
            continue;
        }

        char token_lower[64];
        size_t tok_len = strlen(token);
        for (size_t k = 0; k < tok_len && k < sizeof(token_lower) - 1; ++k)
            token_lower[k] = (char)tolower((unsigned char)token[k]);
        token_lower[tok_len] = '\0';

        if (!strcmp(token_lower, "of") || !strcmp(token_lower, "a") || !strcmp(token_lower, "the"))
            continue;

        if (cleaned_second[0])
            SDL_strlcat(cleaned_second, " ", sizeof(cleaned_second));
        SDL_strlcat(cleaned_second, token, sizeof(cleaned_second));
    }

    char name_part[256];
    name_part[0] = '\0';
    
    /* Build name from shortened first part (base item name) */
    if (short_first[0])
    {
        SDL_strlcpy(name_part, short_first, sizeof(name_part));
        log_debug("mode4_shorten: name_part from short_first='%s'", name_part);
    }
    
    /* Add enchantment qualifier if present (e.g., "of Speed") */
    if (cleaned_second[0])
    {
        if (name_part[0])
            SDL_strlcat(name_part, " ", sizeof(name_part));
        SDL_strlcat(name_part, cleaned_second, sizeof(name_part));
        log_debug("mode4_shorten: name_part after adding cleaned_second='%s'", name_part);
    }
    
    /* Fallback if nothing was extracted */
    if (!name_part[0])
    {
        SDL_strlcpy(name_part, base, sizeof(name_part));
        log_debug("mode4_shorten: name_part fallback to base='%s'", name_part);
    }

    if (trailing_suffix[0])
    {
        if (name_part[0])
            SDL_strlcat(name_part, " ", sizeof(name_part));
        SDL_strlcat(name_part, trailing_suffix, sizeof(name_part));
        log_debug("mode4_shorten: name_part after suffix='%s'", name_part);
    }

    object_desc_trim_spaces(name_part);
    log_debug("mode4_shorten: final name_part='%s'", name_part);

    char rebuilt[256];
    rebuilt[0] = '\0';
    SDL_strlcpy(rebuilt, name_part, sizeof(rebuilt));
    if (stats[0])
    {
        if (rebuilt[0])
            SDL_strlcat(rebuilt, " ", sizeof(rebuilt));
        SDL_strlcat(rebuilt, stats, sizeof(rebuilt));
    }

    object_desc_trim_spaces(rebuilt);
    log_debug("mode4_shorten: FINAL OUTPUT='%s'", rebuilt);
    SDL_strlcpy(buf, rebuilt, max);
}

static const char* object_desc_unidentified_inscription(int pref, int mode)
{
    if (mode >= 4)
        return "?";
    if (pref)
        return "unknown";
    return "not identified";
}

bool object_is_unidentified_for_display(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->ident & IDENT_SPOIL)
        return false;

    if (!object_aware_p(o_ptr))
        return true;

    if (o_ptr->tval == TV_STAFF || o_ptr->tval == TV_HORN)
        return !object_known_p(o_ptr);

    if (object_uses_smithing_difficulty(o_ptr))
        return !object_known_p(o_ptr);

    return false;
}

static void object_desc_append_inscription(char* buf, size_t max, const char* tag)
{
    if (!tag || !tag[0] || !buf || max < 2)
        return;

    if (buf[0])
        SDL_strlcat(buf, ", ", max);
    SDL_strlcat(buf, tag, max);
}

static const char* object_desc_curse_inscription(const object_type* o_ptr,
    bool known, u32b f3)
{
    if (!known || !cursed_p(o_ptr))
        return NULL;

    if (f3 & TR3_PERMA_CURSE)
        return "bound by the Oath of Fëanor";

    if (f3 & TR3_HEAVY_CURSE)
        return "heavily cursed";

    return "cursed";
}

static const char* object_desc_feeling_inscription(int discount)
{
    if (discount < INSCRIP_NULL)
        return NULL;

    switch (discount)
    {
    case INSCRIP_AVERAGE:
    case INSCRIP_GOOD_STRONG:
    case INSCRIP_EXCELLENT:
    case INSCRIP_SPECIAL:
        return NULL;

    case INSCRIP_TERRIBLE:
    case INSCRIP_WORTHLESS:
        return "cursed";

    default:
        return inscrip_text[discount - INSCRIP_NULL];
    }
}

static void object_desc_prepend_prefix(char* dst, size_t dst_size,
    const char* base, const char* prefix)
{
    if (!dst || dst_size == 0)
        return;

    dst[0] = '\0';

    if (!base)
        base = "";
    if (!prefix || !prefix[0])
    {
        SDL_strlcpy(dst, base, dst_size);
        return;
    }

    if (base[0] == '&' && base[1] == ' ')
    {
        SDL_strlcpy(dst, "& ", dst_size);
        SDL_strlcat(dst, prefix, dst_size);
        SDL_strlcat(dst, " ", dst_size);
        SDL_strlcat(dst, base + 2, dst_size);
    }
    else
    {
        SDL_strlcpy(dst, prefix, dst_size);
        SDL_strlcat(dst, " ", dst_size);
        SDL_strlcat(dst, base, dst_size);
    }
}

void object_desc(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    cptr basenm;
    cptr modstr;

    bool aware;
    bool known;

    bool flavor;

    bool append_name;

    char* b;

    char* t;

    cptr s;

    cptr u;
    cptr v;

    char a1 = '<', a2 = '>';
    char p1 = '(', p2 = ')';
    char b1 = '[', b2 = ']';
    char c1 = '{', c2 = '}';

    char discount_buf[80];
    char special_buf[80];

    char tmp_buf[128];

    u32b f1, f2, f3;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Extract some flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* See if the object is "aware" */
    aware = (object_aware_p(o_ptr) ? true : false);

    /* See if the object is "known" */
    known = (object_known_p(o_ptr) ? true : false);

    /* See if the object is "flavored" */
    flavor = (k_ptr->flavor ? true : false);

    /* We have seen the object */
    if (aware)
        k_ptr->everseen = true;

    /* Object is being listed in the object knowledge or smithing screen */
    if (o_ptr->ident & IDENT_SPOIL)
    {
        /* Don't show flavors */
        flavor = false;

        /* Pretend known and aware */
        aware = true;
        known = true;
    }

    /* Player has now seen the item
     *
     * This code must be exactly here to properly handle objects in
     * stores (fake assignment to "aware", see above) and unaware objects
     * in the dungeon.
     */
    if (aware)
        k_ptr->everseen = true;

    /* Assume no name appending */
    append_name = false;

    /* Extract default "base" string */
    basenm = (k_name + k_ptr->name);

    /* Assume no "modifier" string */
    modstr = "";

    /* Analyze the object */
    switch (o_ptr->tval)
    {
    /* Some objects are easy to describe */
    case TV_SKELETON:
    case TV_METAL:
    case TV_NOTE:
    case TV_FLASK:
    case TV_CHEST:
    {
        break;
    }

    /* Missiles/Bows/Weapons */
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    {
        break;
    }

    /* Armour */
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_SHIELD:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        break;
    }

    /* Lights (including a few "Specials") */
    case TV_LIGHT:
    {
        break;
    }

    /* Amulets (including a few "Specials") */
    case TV_AMULET:
    {
        /* Hack -- Known artefacts */
        if (artefact_p(o_ptr) && aware)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Amulet~" : "& Amulet~");

        break;
    }

    /* Rings (including a few "Specials") */
    case TV_RING:
    {
        /* Hack -- Known artefacts */
        if (artefact_p(o_ptr) && aware)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Ring~" : "& Ring~");

        break;
    }

    /* Staffs */
    case TV_STAFF:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Staff~" : "& Staff~");

        break;
    }

    /* Gems */
    case TV_GEM:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Gem~" : "& Gem~");

        break;
    }

    /* Horns */
    case TV_HORN:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Horn~" : "& Horn~");

        break;
    }

    /* Potions */
    case TV_POTION:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Potion~" : "& Potion~");

        break;
    }

    /* Food */
    case TV_FOOD:
    {
        /* Ordinary food is "boring" */
        if (o_ptr->sval >= SV_FOOD_MIN_FOOD)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Herb~" : "& Herb~");

        // Easter Eggs
        if (easter_time())
            basenm = (flavor ? "& # Easter Egg~" : "& Easter Egg~");

        break;
    }

    /* Hack -- Default -- Used in the "inventory" routine */
    default:
    {
        SDL_strlcpy(buf, "(nothing)", max);
        return;
    }
    }

    /* Start dumping the result */
    t = b = tmp_buf;

    /* Insert visible runtime prefixes before normal ego prefixes. */
    char basenm_with_runtime[128];
    basenm_with_runtime[0] = '\0';
    if (object_is_fire_broken(o_ptr))
    {
        object_desc_prepend_prefix(
            basenm_with_runtime, sizeof(basenm_with_runtime), basenm, "(broken)");
        basenm = basenm_with_runtime;
    }

    /* Insert ego prefix into base name (after '& ' if present). */
    char basenm_with_prefix[128];
    basenm_with_prefix[0] = '\0';
    if (known && object_ego_prefix(o_ptr))
    {
        ego_item_type* e_ptr = &e_info[object_ego_prefix(o_ptr)];
        const char* raw = e_name + e_ptr->name;

        char prefix_buf[80];
        prefix_buf[0] = '\0';
        if (raw && raw[0])
        {
            if (ego_name_is_prefix(raw))
            {
                size_t len = strlen(raw);
                size_t copy_len = (len >= 2) ? (len - 2) : 0;
                if (copy_len >= sizeof(prefix_buf))
                    copy_len = sizeof(prefix_buf) - 1;
                if (copy_len > 0)
                {
                    memcpy(prefix_buf, raw + 1, copy_len);
                    prefix_buf[copy_len] = '\0';
                }
            }
            else
            {
                SDL_strlcpy(prefix_buf, raw, sizeof(prefix_buf));
            }
        }

        if (prefix_buf[0])
        {
            object_desc_prepend_prefix(
                basenm_with_prefix, sizeof(basenm_with_prefix), basenm, prefix_buf);
            basenm = basenm_with_prefix;
        }
    }

    /* Begin */
    s = basenm;

    /* Handle objects which sometimes use "a" or "an" */
    if (*s == '&')
    {
        /* Paranoia XXX XXX XXX */
        /* ASSERT(s[1] == ' '); */

        /* Skip the ampersand and the following space */
        s += 2;

        /* No prefix */
        if (!pref)
        {
            /* Nothing */
        }

        /* Hack -- None left */
        else if (o_ptr->number <= 0)
        {
            object_desc_str_macro(t, "no more ");
        }

        /* Extract the number */
        else if (o_ptr->number > 1)
        {
            object_desc_num_macro(t, o_ptr->number);
            object_desc_chr_macro(t, ' ');
        }

        /* Hack -- The only one of its kind */
        else if (known && artefact_p(o_ptr))
        {
            object_desc_str_macro(t, "The ");
        }

        /* Hack -- A single one, and next character will be a vowel */
        else if ((*s == '#') ? is_a_vowel(modstr[0]) : is_a_vowel(*s))
        {
            object_desc_str_macro(t, "an ");
        }

        /* A single one, and next character will be a non-vowel */
        else
        {
            object_desc_str_macro(t, "a ");
        }
    }

    /* Handle objects which never use "a" or "an" */
    else
    {
        /* No pref */
        if (!pref)
        {
            /* Nothing */
        }

        /* Hack -- all gone */
        else if (o_ptr->number <= 0)
        {
            object_desc_str_macro(t, "no more ");
        }

        /* Prefix a number if required */
        else if (o_ptr->number > 1)
        {
            object_desc_num_macro(t, o_ptr->number);
            object_desc_chr_macro(t, ' ');
        }

        /* Hack -- The only one of its kind */
        else if (known && artefact_p(o_ptr))
        {
            object_desc_str_macro(t, "The ");
        }

        /* Hack -- A single item, so no prefix needed */
        else
        {
            /* Nothing */
        }
    }

    /* Paranoia XXX XXX XXX */
    /* ASSERT(*s != '~'); */

    /* Copy the string */
    for (; *s; s++)
    {
        /* Pluralizer */
        if (*s == '~')
        {
            /* Add a plural if needed */
            if (o_ptr->number != 1)
            {
                char k = t[-1];

                /* Hack -- "Cutlass-es" and "Torch-es" */
                if ((k == 's') || (k == 'h'))
                    *t++ = 'e';

                /* Add an 's' */
                *t++ = 's';
            }
        }

        /* Modifier */
        else if (*s == '#')
        {
            /* Append the modifier */
            object_desc_str_macro(t, modstr);
        }

        /* Normal */
        else
        {
            /* Copy */
            *t++ = *s;
        }
    }

    /* No more details wanted */
    if (mode < 0)
        goto object_desc_done;

    /* Append the "kind name" to the "base name" */
    if (append_name)
    {
        object_desc_str_macro(t, " of ");
        object_desc_str_macro(t, (k_name + k_ptr->name));
    }

    /* Hack -- Append "Artefact" or "Special" names */
    if (known)
    {
        /* Grab any artefact name */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            object_desc_chr_macro(t, ' ');
            object_desc_str_macro(t, a_ptr->name);
        }

        /* Grab any special item suffix name */
        else if (object_ego_suffix(o_ptr))
        {
            ego_item_type* e_ptr = &e_info[object_ego_suffix(o_ptr)];
            object_desc_chr_macro(t, ' ');
            object_desc_str_macro(t, (e_name + e_ptr->name));
        }
    }

    /* No more details wanted */
    if (mode < 1)
        goto object_desc_done;

    /* Hack -- Chests and skeletons must be described in detail */
    if (o_ptr->tval == TV_SKELETON)
    {
        cptr tail = "";

        /* May be "searched" */
        if (!o_ptr->pval)
        {
            tail = " (searched)";
        }

        object_desc_str_macro(t, tail);
    }
    else if (o_ptr->tval == TV_CHEST)
    {
        cptr tail = "";

        /* Not searched yet */
        if (!known || (o_ptr->sval == SV_CHEST_PRESENT))
        {
            /* Nothing */
        }

        /* May be "empty" */
        else if (!o_ptr->pval)
        {
            tail = " (empty)";
        }

        /* May be "disarmed" */
        else if (o_ptr->pval < 0)
        {
            if (object_chest_trap_flags(o_ptr))
            {
                tail = " (disarmed)";
            }
            else
            {
                tail = " (unlocked)";
            }
        }

        /* Describe the traps, if any */
        else
        {
            /* Describe the traps */
            switch (object_chest_trap_flags(o_ptr))
            {
            case 0:
            {
                tail = " (Locked)";
                break;
            }
            case CHEST_GAS_CONF:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_GAS_STUN:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_GAS_POISON:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_NEEDLE_HALLU:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_NEEDLE_ENTRANCE:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_NEEDLE_LOSE_STR:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_FLAME:
            {
                tail = " (Flame Trap)";
                break;
            }
            default:
            {
                tail = " (Multiple Traps)";
                break;
            }
            }
        }

        /* Append the tail */
        object_desc_str_macro(t, tail);
    }

    /* Dump base weapon info */
    switch (o_ptr->tval)
    {
    /* Missiles */
    case TV_ARROW:
    {
        /* Append a "hit" string if nonzero */
        if (o_ptr->att != 0)
        {
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);
            object_desc_int_macro(t, o_ptr->att);
            object_desc_chr_macro(t, p2);
        }

        /* All done */
        break;
    }

    /* Weapons */
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    case TV_BOW:
    {
        /* Append a "hit,damage" string */
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, p1);
        object_desc_int_macro(t, o_ptr->att);
        object_desc_chr_macro(t, ',');
        object_desc_num_macro(t, o_ptr->dd);
        object_desc_chr_macro(t, 'd');
        /* Bonus for 'hand and a half' weapons like the bastard sword when used
         * with two hands */
        object_desc_num_macro(t, o_ptr->ds + hand_and_a_half_bonus(o_ptr));
        object_desc_chr_macro(t, p2);

        /* All done */
        break;
    }

    default: /* not a weapon */
    {
        /* don't display for unidentified rings */
        if ((o_ptr->tval == TV_RING) && !object_known_p(o_ptr))
        {
            break;
        }
        if (o_ptr->att)
        {
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);
            object_desc_int_macro(t, o_ptr->att);
            object_desc_chr_macro(t, p2);
        }
    }
    }

    /* show evasion/protection info */

    /* but don't display for unidentified rings */
    if ((o_ptr->tval == TV_RING) && !object_known_p(o_ptr))
    {
        // do nothing
    }
    else if (o_ptr->pd && o_ptr->ps)
    {
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, b1);
        object_desc_int_macro(t, o_ptr->evn);
        object_desc_chr_macro(t, ',');
        object_desc_num_macro(t, o_ptr->pd);
        object_desc_chr_macro(t, 'd');
        object_desc_num_macro(t, o_ptr->ps);
        object_desc_chr_macro(t, b2);
    }
    else if (o_ptr->evn)
    {
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, b1);
        object_desc_int_macro(t, o_ptr->evn);
        object_desc_chr_macro(t, b2);
    }

    /* No more details wanted */
    if (mode < 2)
        goto object_desc_done;

    /* Hack -- Staffs have charges */
    if (o_ptr->tval == TV_STAFF)
    {
        if (known)
        {
            /* Dump " (N charges)" */
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);

            /* Always show actual usable charges (internal pval is 2x for mechanics) */
            int visible_charges = (o_ptr->pval + CHANNELING_CHARGE_MULTIPLIER - 1)
                / CHANNELING_CHARGE_MULTIPLIER;
            object_desc_num_macro(t, visible_charges);

            /*write out the word charge(s) as appropriate*/
            object_desc_str_macro(t, " charge");
            if (visible_charges != 1)
            {
                object_desc_chr_macro(t, 's');
            }
            object_desc_chr_macro(t, p2);
        }

        else if ((o_ptr->xtra1 > 0) && !(o_ptr->ident & (IDENT_EMPTY)))
        {
            /* Dump " (used N times)" */
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);

            /*write out the word charge(s) as appropriate*/
            object_desc_str_macro(t, "used ");
            object_desc_num_macro(t, o_ptr->xtra1);
            object_desc_str_macro(t, " time");
            if (o_ptr->xtra1 != 1)
            {
                object_desc_chr_macro(t, 's');
            }
            object_desc_chr_macro(t, p2);
        }
    }

    /* Hack -- Process Lanterns/Torches */
    if (fuelable_light_p(o_ptr)
        && !(o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
            && player_light_uses_oil_pool(o_ptr)))
    {
        /* Hack -- Turns of light for normal lites */
        object_desc_str_macro(t, " (");
        object_desc_num_macro(t, player_light_fuel(o_ptr));
        object_desc_str_macro(t, " turns)");
    }

    /* Dump "pval" flags for wearable items */
    u32b pval_f1 = object_pval_flags1(o_ptr);
    if (known && pval_f1)
    {
        int best = 0;
        int best_abs = 0;

        if (pval_f1 & (TR1_STR | TR1_NEG_STR))
        {
            int v = o_ptr->stat_bonus[A_STR];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & (TR1_DEX | TR1_NEG_DEX))
        {
            int v = o_ptr->stat_bonus[A_DEX];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & (TR1_CON | TR1_NEG_CON))
        {
            int v = o_ptr->stat_bonus[A_CON];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & (TR1_GRA | TR1_NEG_GRA))
        {
            int v = o_ptr->stat_bonus[A_GRA];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }

        if (pval_f1 & TR1_MEL)
        {
            int v = o_ptr->skill_bonus[S_MEL];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_ARC)
        {
            int v = o_ptr->skill_bonus[S_ARC];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_STL)
        {
            int v = o_ptr->skill_bonus[S_STL];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_PER)
        {
            int v = o_ptr->skill_bonus[S_PER];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_WIL)
        {
            int v = o_ptr->skill_bonus[S_WIL];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_SMT)
        {
            int v = o_ptr->skill_bonus[S_SMT];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_SNG)
        {
            int v = o_ptr->skill_bonus[S_SNG];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }

        if (pval_f1 & (TR1_TUNNEL | TR1_DAMAGE_SIDES))
        {
            int v = o_ptr->pval;
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }

        cptr tail = "";
        cptr tail2 = "";

        /* Start the display */
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, a1);

        /* Dump the best representative pval-style bonus. */
        object_desc_int_macro(t, best);

        /* Add the descriptor */
        object_desc_str_macro(t, tail);
        object_desc_str_macro(t, tail2);

        /* Finish the display */
        object_desc_chr_macro(t, a2);
    }

    /* Indicate "charging" objects, but not horns or lights */
    if (known && o_ptr->timeout && !fuelable_light_p(o_ptr))
    {
        /* Hack -- Dump " (charging)" if relevant */
        object_desc_str_macro(t, " (recharging)");
    }

    /* No more details wanted */
    if (mode < 3)
        goto object_desc_done;

    /* Use standard inscription */
    if (o_ptr->obj_note)
    {
        u = quark_str(o_ptr->obj_note);
    }

    /* Use nothing */
    else
    {
        u = NULL;
    }

    special_buf[0] = '\0';
    if (object_is_unidentified_for_display(o_ptr))
    {
        object_desc_append_inscription(
            special_buf, sizeof(special_buf),
            object_desc_unidentified_inscription(pref, mode));
    }

    v = NULL;
    discount_buf[0] = '\0';

    if (o_ptr->discount >= INSCRIP_NULL)
    {
        v = object_desc_feeling_inscription(o_ptr->discount);
    }
    else if ((v = object_desc_curse_inscription(o_ptr, known, f3)) != NULL)
    {
    }
    else if (!known && (o_ptr->ident & (IDENT_EMPTY)))
    {
        v = "empty";
    }
    else if (!aware && object_tried_p(o_ptr))
    {
        v = "tried";
    }
    else if (o_ptr->discount > 0)
    {
        char* q = discount_buf;
        object_desc_num_macro(q, o_ptr->discount);
        object_desc_str_macro(q, "% off");
        *q = '\0';
        v = discount_buf;
    }

    object_desc_append_inscription(special_buf, sizeof(special_buf), v);
    v = special_buf[0] ? special_buf : NULL;

    /* Inscription */
    if (u || v)
    {
        /* Begin the inscription */
        *t++ = ' ';
        *t++ = c1;

        /* Standard inscription */
        if (u)
        {
            /* Append the inscription */
            while ((t < b + 75) && *u)
                *t++ = *u++;
        }

        /* Special inscription too */
        if (u && v && (t < b + 75))
        {
            /* Separator */
            *t++ = ',';
            *t++ = ' ';
        }

        /* Special inscription */
        if (v)
        {
            /* Append the inscription */
            while ((t < b + 75) && *v)
                *t++ = *v++;
        }

        /* Terminate the inscription */
        *t++ = c2;
    }

object_desc_done:

    /* Terminate */
    *t = '\0';

    if ((mode == 4) && !pref)
    {
        bool apply_rules = !artefact_p(o_ptr);
        object_desc_mode4_shorten(tmp_buf, sizeof(tmp_buf), o_ptr, apply_rules);
    }

    /* Copy the string over */
    SDL_strlcpy(buf, tmp_buf, max);
}

/*
 * Describe an item that is known to be lying on the floor.
 *
 * For smithing-difficulty items that have not been identified yet (and have
 * not been handled by the player), suppress the combat stats in the short
 * name display (e.g. show just "Short Sword").
 */
void object_desc_floor(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    const char* u;
    const char* v;
    char discount_buf[80];
    char special_buf[80];

    bool aware;
    bool known;
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
    {
        SDL_strlcpy(buf, "(nothing)", max);
        return;
    }

    if (!object_uses_smithing_difficulty(o_ptr) || object_known_p(o_ptr)
        || (o_ptr->ident & IDENT_HANDLED))
    {
        object_desc(buf, max, o_ptr, pref, mode);
        return;
    }

    /* Base name only (no combat stats). */
    object_desc(buf, max, o_ptr, pref, 0);

    /* Match object_desc() inscription behavior for mode 3+. */
    if (mode < 3)
        return;

    aware = (object_aware_p(o_ptr) ? true : false);
    known = (object_known_p(o_ptr) ? true : false);
    object_flags(o_ptr, &f1, &f2, &f3);
    (void)f1;
    (void)f2;

    if (o_ptr->obj_note)
        u = quark_str(o_ptr->obj_note);
    else
        u = NULL;

    special_buf[0] = '\0';
    if (object_is_unidentified_for_display(o_ptr))
    {
        object_desc_append_inscription(
            special_buf, sizeof(special_buf),
            object_desc_unidentified_inscription(pref, mode));
    }

    v = NULL;
    discount_buf[0] = '\0';

    if (o_ptr->discount >= INSCRIP_NULL)
    {
        v = object_desc_feeling_inscription(o_ptr->discount);
    }
    else if ((v = object_desc_curse_inscription(o_ptr, known, f3)) != NULL)
    {
    }
    else if (!known && (o_ptr->ident & (IDENT_EMPTY)))
    {
        v = "empty";
    }
    else if (!aware && object_tried_p(o_ptr))
    {
        v = "tried";
    }
    else if (o_ptr->discount > 0)
    {
        strnfmt(discount_buf, sizeof(discount_buf), "%d%% off", o_ptr->discount);
        v = discount_buf;
    }

    object_desc_append_inscription(special_buf, sizeof(special_buf), v);
    v = special_buf[0] ? special_buf : NULL;

    if (u || v)
    {
        SDL_strlcat(buf, " {", max);
        if (u)
            SDL_strlcat(buf, u, max);
        if (u && v)
            SDL_strlcat(buf, ", ", max);
        if (v)
            SDL_strlcat(buf, v, max);
        SDL_strlcat(buf, "}", max);
    }
}

/*
 * Describe an item and pretend the item is fully known and has no flavor.
 */
void object_desc_spoil(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    /* Make a backup */
    object_copy(i_ptr, o_ptr);

    /* Set it to display as identified but without flavour */
    i_ptr->ident |= IDENT_SPOIL;

    /* Describe */
    object_desc(buf, max, i_ptr, pref, mode);
}

/*
 * Describe an item's random attributes for "character dumps"
 */
void identify_random_gen(const object_type* o_ptr)
{
    /* Set hooks for character dump */
    object_info_out_flags = object_flags_known;

    /* Set the indent/wrap */
    text_out_indent = 3;
    text_out_wrap = 65;

    /* Dump the info */
    if (object_info_out(o_ptr))
        text_out("\n");

    /* Reset indent/wrap */
    text_out_indent = 0;
    text_out_wrap = 0;
}

