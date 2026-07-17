#include "angband.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"

void do_cmd_redraw(void)
{
    int j;

    term* old = Term;

    /* Low level flush */
    Term_flush();

    /* Reset "inkey()" */
    flush();

    if (!active_narrative_banner_visible())
        clear_active_narrative_banner();

    /* Hack -- React to changes */
    Term_xtra(TERM_XTRA_REACT, 0);

    /* Combine and Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update stuff */
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw everything */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Window stuff */
    p_ptr->window
        |= (PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT | PW_MONLIST
            | PW_SUPPLY);

    /* Clear screen */
    Term_clear();

    /* Hack -- update */
    handle_stuff();

    /* Redraw every window */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Dead window */
        if (!angband_term[j])
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Redraw */
        Term_redraw();

        /* Refresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- character sheet
 */
#define CHARACTER_SHEET_CLICK_ITEM_BASE 12000
#define CHARACTER_SHEET_MAX_ITEMS 96

typedef enum {
    CHARACTER_SHEET_ITEM_SKILL = 0,
    CHARACTER_SHEET_ITEM_TRAIT = 1,
    CHARACTER_SHEET_ITEM_VALUE = 2
} character_sheet_item_kind;

typedef enum {
    CHARACTER_SHEET_VALUE_EXP = 0,
    CHARACTER_SHEET_VALUE_BURDEN,
    CHARACTER_SHEET_VALUE_DEPTH,
    CHARACTER_SHEET_VALUE_DEEP_CALL,
    CHARACTER_SHEET_VALUE_TURN,
    CHARACTER_SHEET_VALUE_LIGHT,
    CHARACTER_SHEET_VALUE_MELEE,
    CHARACTER_SHEET_VALUE_MELEE_X2,
    CHARACTER_SHEET_VALUE_OFFHAND,
    CHARACTER_SHEET_VALUE_BOWS,
    CHARACTER_SHEET_VALUE_ARMOR,
    CHARACTER_SHEET_VALUE_HEALTH,
    CHARACTER_SHEET_VALUE_VOICE,
    CHARACTER_SHEET_VALUE_SONG,
    CHARACTER_SHEET_VALUE_STR,
    CHARACTER_SHEET_VALUE_DEX,
    CHARACTER_SHEET_VALUE_CON,
    CHARACTER_SHEET_VALUE_GRA,
    CHARACTER_SHEET_VALUE_DEPTH_PROGRESS
} character_sheet_value_kind;

typedef struct {
    character_sheet_item_kind kind;
    int id;
    int row;
    int col;
    int width;
    int skill;
    int trait_score;
    int value_kind;
    bool proficiency;
    u32b aff_flag;
    u32b pen_flag;
    cptr label;
    cptr desc;
    char label_buf[64];
} character_sheet_item;

typedef struct {
    cptr label;
    cptr desc;
} character_sheet_named_trait;

static const character_sheet_named_trait character_sheet_named_traits[] = {
    { "Master Artisan", "Artifacts use only 1 forge charge; fire and light items are easier to make." },
    { "Creator of Galvorn", "You may smith Galvorn armour and begin with humbler bread." },
    { "Chosen of Ulmo", "Horns are twice as effective." },
    { "Indomitable Will", "Will Affinity is fixed at 3 and cannot be reduced by curses." },
    { "Himself", "Majesty is 1.5x effective." },
    { "Songs of Power", "Song of Staying is twice as effective." },
    { "Elven Dance", "Song of Lorien is 1.5x effective." },
    { "Girdle of Melian", "Song of Thresholds and Gems of Warding are twice as effective." },
    { "Creator of Angrist", "You can create very sharp items; sharp and accurate items are easier to smith." },
    { "Old Master", "Using 3 forge charges can create mithril items without mithril." },
    { "Ring Master", "Rings cost 30% less to create and ring slots are treated as major slots." },
    { "Aure entuluva", "Song of Slaying is twice as effective." },
    { "Voice of the Girdle", "Song of Mastery is 1.75x effective." },
    { "Forgotten", "You begin with all stealth abilities." },
    { "One Handed", "Melee abilities are twice as effective and one-handed combat is stronger." },
    { "Agarwaen", "Will abilities are twice as effective and you can break fate-cursed items." },
    { "Shadow Walker", "Song of Disguise checks add your Perception skill." },
    { "Minstrel", "Song ability costs are reduced." },
    { "Woven Master", "Song is not reduced for woven minor themes." },
    { "Gift of Eru", "Story deaths are not counted if you die." },
    { "Seafarer", "Abilities cost less to acquire." },
    { "Kinslayer", "Retrieving Silmarils may endanger your kin." },
    { "Treacherous", "You may steal a Silmaril at the end." },
    { "Doom of Mandos", "Curses offered to you grow more complex." },
    { "Morgoth Curse", "You will encounter more dangerous creatures." },
};

static const cptr character_sheet_skill_descriptions[S_MAX] = {
    "Melee chance to hit and control in close combat.",
    "Ranged chance to hit with bows and thrown weapons.",
    "Avoiding attacks; also shown in armour as hit-avoid chance.",
    "Avoiding detection and moving quietly.",
    "Noticing hidden doors, traps, monsters, and subtle details.",
    "Mental resistance and force of will.",
    "Crafting items at forges.",
    "Power and reliability of songs.",
    ""
};

cptr character_sheet_trait_description(cptr label)
{
    if (!label || !label[0])
        return "";

    for (size_t i = 0; i < N_ELEMENTS(character_sheet_named_traits); i++)
    {
        if (!strcmp(label, character_sheet_named_traits[i].label))
            return character_sheet_named_traits[i].desc;
    }

    return "";
}

static bool character_sheet_add_semantic_item(character_sheet_item items[],
    int* count, int max_items, const character_sheet_item* src)
{
    if (!items || !count || !src || *count >= max_items)
        return false;

    items[*count] = *src;
    if (items[*count].label_buf[0])
        items[*count].label = items[*count].label_buf;
    (*count)++;
    return true;
}

static void character_sheet_add_semantic_value(character_sheet_item items[],
    int* count, int max_items, cptr label, character_sheet_value_kind kind,
    int row, int col)
{
    character_sheet_item item;

    SDL_zero(item);
    item.kind = CHARACTER_SHEET_ITEM_VALUE;
    item.id = (int)kind;
    item.value_kind = (int)kind;
    item.row = row;
    item.col = col;
    item.width = 24;
    item.label = label;
    (void)character_sheet_add_semantic_item(items, count, max_items, &item);
}

static void character_sheet_add_semantic_unique_trait(
    character_sheet_item items[], int* count, int max_items, cptr label,
    int row, int col)
{
    character_sheet_item item;

    SDL_zero(item);
    item.kind = CHARACTER_SHEET_ITEM_TRAIT;
    item.id = 100 + *count;
    item.row = row;
    item.col = col;
    item.width = label ? (int)strlen(label) + 3 : 16;
    item.label = label;
    item.desc = character_sheet_trait_description(label);
    (void)character_sheet_add_semantic_item(items, count, max_items, &item);
}

static bool character_sheet_add_semantic_skill_trait(
    character_sheet_item items[], int* count, int max_items, cptr label,
    int skill, int score, bool proficiency, u32b aff_flag, u32b pen_flag,
    int row, int col)
{
    character_sheet_item item;

    if (!label || !label[0] || score == 0)
        return false;

    SDL_zero(item);
    item.kind = CHARACTER_SHEET_ITEM_TRAIT;
    item.id = 1000 + *count;
    item.row = row;
    item.col = col;
    item.width = (int)strlen(label) + 4;
    item.skill = skill;
    item.trait_score = score;
    item.proficiency = proficiency;
    item.aff_flag = aff_flag;
    item.pen_flag = pen_flag;
    item.label = item.label_buf;

    if (score == 2)
        strnfmt(item.label_buf, sizeof(item.label_buf), "%s++", label);
    else if (score == 1)
        strnfmt(item.label_buf, sizeof(item.label_buf), "%s+", label);
    else if (score == -1)
        strnfmt(item.label_buf, sizeof(item.label_buf), "%s-", label);
    else
        strnfmt(item.label_buf, sizeof(item.label_buf), "%s--", label);

    return character_sheet_add_semantic_item(items, count, max_items, &item);
}

static int character_sheet_collect_semantic_items(character_sheet_item items[],
    int max_items)
{
    int count = 0;
    int race;
    int character;
    int row;

    if (!items || max_items <= 0 || !p_ptr)
        return 0;

    row = 1;
    character_sheet_add_semantic_value(items, &count, max_items, "Exp",
        CHARACTER_SHEET_VALUE_EXP, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Burden",
        CHARACTER_SHEET_VALUE_BURDEN, row++, 0);
    if (turn > 0)
    {
        character_sheet_add_semantic_value(items, &count, max_items,
            "Depth c/m", CHARACTER_SHEET_VALUE_DEPTH, row++, 0);
        character_sheet_add_semantic_value(items, &count, max_items,
            "Minimum depth progress", CHARACTER_SHEET_VALUE_DEPTH_PROGRESS,
            row++, 0);
    }
    character_sheet_add_semantic_value(items, &count, max_items, "Deep Call",
        CHARACTER_SHEET_VALUE_DEEP_CALL, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Turn",
        CHARACTER_SHEET_VALUE_TURN, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Light",
        CHARACTER_SHEET_VALUE_LIGHT, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Melee",
        CHARACTER_SHEET_VALUE_MELEE, row++, 0);
    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        character_sheet_add_semantic_value(items, &count, max_items,
            "Melee x2", CHARACTER_SHEET_VALUE_MELEE_X2, row++, 0);
    }
    if (p_ptr->mds2 > 0)
    {
        character_sheet_add_semantic_value(items, &count, max_items,
            "Offhand", CHARACTER_SHEET_VALUE_OFFHAND, row++, 0);
    }
    character_sheet_add_semantic_value(items, &count, max_items, "Bows",
        CHARACTER_SHEET_VALUE_BOWS, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Armor",
        CHARACTER_SHEET_VALUE_ARMOR, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Health",
        CHARACTER_SHEET_VALUE_HEALTH, row++, 0);
    character_sheet_add_semantic_value(items, &count, max_items, "Voice",
        CHARACTER_SHEET_VALUE_VOICE, row++, 0);
    if (p_ptr->song1 != SNG_NOTHING)
    {
        character_sheet_add_semantic_value(items, &count, max_items, "Song",
            CHARACTER_SHEET_VALUE_SONG, row++, 0);
    }
    if (p_ptr->song2 != SNG_NOTHING)
    {
        character_sheet_add_semantic_value(items, &count, max_items, "Theme",
            CHARACTER_SHEET_VALUE_SONG, row++, 0);
    }

    character_sheet_add_semantic_value(items, &count, max_items, "Str",
        CHARACTER_SHEET_VALUE_STR, 1, 50);
    character_sheet_add_semantic_value(items, &count, max_items, "Dex",
        CHARACTER_SHEET_VALUE_DEX, 2, 50);
    character_sheet_add_semantic_value(items, &count, max_items, "Con",
        CHARACTER_SHEET_VALUE_CON, 3, 50);
    character_sheet_add_semantic_value(items, &count, max_items, "Gra",
        CHARACTER_SHEET_VALUE_GRA, 4, 50);

    row = 1;
    for (int skill = 0; skill < S_MAX; skill++)
    {
        character_sheet_item item;

        if (skill == S_SPC)
            continue;

        SDL_zero(item);
        item.kind = CHARACTER_SHEET_ITEM_SKILL;
        item.id = skill;
        item.skill = skill;
        item.row = row++;
        item.col = 66;
        item.width = 30;
        item.label = skill_names_full[skill];
        (void)character_sheet_add_semantic_item(items, &count, max_items,
            &item);
    }

    if (!p_info || !c_info)
        return count;

    race = p_ptr->prace;
    character = p_ptr->pcharacter;
    row = 1;

#define ADD_SEMANTIC_UNIQUE(LABEL, FLAG)                                       \
    do {                                                                       \
        if ((p_info[race].flags & (FLAG))                                      \
            || (c_info[character].flags & (FLAG)))                             \
        {                                                                      \
            character_sheet_add_semantic_unique_trait(items, &count,           \
                max_items, (LABEL), row++, 25);                                \
        }                                                                      \
    } while (0)

#define ADD_SEMANTIC_UNIQUE_U(LABEL, FLAG)                                     \
    do {                                                                       \
        if (c_info[character].flags_u & (FLAG))                                \
        {                                                                      \
            character_sheet_add_semantic_unique_trait(items, &count,           \
                max_items, (LABEL), row++, 25);                                \
        }                                                                      \
    } while (0)

#define ADD_SEMANTIC_SKILL_TRAIT(LABEL, SKILL, AFF, PEN, PROF)                 \
    do {                                                                       \
        int score = 0;                                                         \
        if (p_info[race].flags & (AFF)) score++;                               \
        if (c_info[character].flags & (AFF)) score++;                          \
        if ((PEN) && (p_info[race].flags & (PEN))) score--;                    \
        if ((PEN) && (c_info[character].flags & (PEN))) score--;               \
        score += curse_flag_count_rhf(AFF);                                    \
        if (PEN) score -= curse_flag_count_rhf(PEN);                           \
        score = MIN(2, MAX(-2, score));                                        \
        if (character_sheet_add_semantic_skill_trait(items, &count,           \
                max_items, (LABEL), (SKILL), score, (PROF), (AFF), (PEN),     \
                row, 25))                                                     \
        {                                                                     \
            row++;                                                            \
        }                                                                     \
    } while (0)

    ADD_SEMANTIC_UNIQUE_U("Master Artisan", UNQ_SMT_FEANOR);
    ADD_SEMANTIC_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL);
    ADD_SEMANTIC_UNIQUE_U("Chosen of Ulmo", UNQ_WIL_TUOR);
    ADD_SEMANTIC_UNIQUE_U("Indomitable Will", UNQ_EARENDIL);
    ADD_SEMANTIC_UNIQUE_U("Himself", UNQ_WIL_FIN);
    ADD_SEMANTIC_UNIQUE_U("Songs of Power", UNQ_SNG_FIN);
    ADD_SEMANTIC_UNIQUE_U("Elven Dance", UNQ_SNG_LUT);
    ADD_SEMANTIC_UNIQUE_U("Girdle of Melian", UNQ_SNG_MEL);
    ADD_SEMANTIC_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR);
    ADD_SEMANTIC_UNIQUE_U("Old Master", UNQ_SMT_GAMIL);
    ADD_SEMANTIC_UNIQUE_U("Ring Master", UNQ_SMT_CELEBRIMBOR);
    ADD_SEMANTIC_UNIQUE_U("Aure entuluva", UNQ_SNG_HURIN);
    ADD_SEMANTIC_UNIQUE_U("Voice of the Girdle", UNQ_SNG_THINGOL);
    ADD_SEMANTIC_UNIQUE_U("Forgotten", UNQ_MIM);
    ADD_SEMANTIC_UNIQUE_U("One Handed", UNQ_MEL_MAEDHROS);
    ADD_SEMANTIC_UNIQUE_U("Agarwaen", UNQ_WIL_TURIN);
    ADD_SEMANTIC_UNIQUE_U("Shadow Walker", UNQ_SNG_TURGON);
    ADD_SEMANTIC_UNIQUE_U("Minstrel", UNQ_MINSTREL);
    ADD_SEMANTIC_UNIQUE_U("Woven Master", UNQ_WOVEN_MASTER);
    ADD_SEMANTIC_UNIQUE("Gift of Eru", RHF_GIFTERU);
    ADD_SEMANTIC_UNIQUE("Seafarer", RHF_FREE);
    ADD_SEMANTIC_UNIQUE("Kinslayer", RHF_KINSLAYER);
    ADD_SEMANTIC_UNIQUE("Treacherous", RHF_TREACHERY);
    ADD_SEMANTIC_UNIQUE("Doom of Mandos", RHF_CURSE);
    ADD_SEMANTIC_UNIQUE("Morgoth Curse", RHF_MOR_CURSE);

    ADD_SEMANTIC_SKILL_TRAIT("melee", S_MEL, RHF_MEL_AFFINITY,
        RHF_MEL_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("evasion", S_EVN, RHF_EVN_AFFINITY,
        RHF_EVN_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("stealth", S_STL, RHF_STL_AFFINITY,
        RHF_STL_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("archery", S_ARC, RHF_ARC_AFFINITY,
        RHF_ARC_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("will", S_WIL, RHF_WIL_AFFINITY,
        RHF_WIL_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("perception", S_PER, RHF_PER_AFFINITY,
        RHF_PER_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("smithing", S_SMT, RHF_SMT_AFFINITY,
        RHF_SMT_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("song", S_SNG, RHF_SNG_AFFINITY,
        RHF_SNG_PENALTY, false);
    ADD_SEMANTIC_SKILL_TRAIT("bow", S_ARC, RHF_BOW_PROFICIENCY, 0, true);
    ADD_SEMANTIC_SKILL_TRAIT("axe", S_MEL, RHF_AXE_PROFICIENCY, 0, true);

#undef ADD_SEMANTIC_SKILL_TRAIT
#undef ADD_SEMANTIC_UNIQUE_U
#undef ADD_SEMANTIC_UNIQUE

    return count;
}

static int character_sheet_find_best_focus(const character_sheet_item items[],
    int item_count, int current, int dx, int dy)
{
    const character_sheet_item* base;
    int best = -1;
    int best_score = 1000000;

    if (!items || item_count <= 0)
        return -1;

    if (current < 0 || current >= item_count)
        return 0;

    base = &items[current];

    if (dx != 0)
    {
        for (int i = 0; i < item_count; i++)
        {
            int side;
            int score;

            if (i == current)
                continue;
            side = items[i].col - base->col;
            if ((dx < 0 && side >= 0) || (dx > 0 && side <= 0))
                continue;

            score = ABS(items[i].row - base->row) * 100 + ABS(side);
            if (items[i].kind == base->kind)
                score += 1000;
            if (score < best_score)
            {
                best = i;
                best_score = score;
            }
        }

        if (best >= 0)
            return best;
    }

    if (dy != 0)
    {
        int wrap = -1;
        int wrap_score = (dy > 0) ? 1000000 : -1000000;

        best_score = 1000000;
        for (int i = 0; i < item_count; i++)
        {
            int row_delta;
            int score;

            if (i == current || items[i].kind != base->kind)
                continue;

            row_delta = items[i].row - base->row;
            if ((dy > 0 && row_delta > 0) || (dy < 0 && row_delta < 0))
            {
                score = ABS(row_delta) * 100 + ABS(items[i].col - base->col);
                if (score < best_score)
                {
                    best = i;
                    best_score = score;
                }
            }

            if (dy > 0)
            {
                if (items[i].row < wrap_score)
                {
                    wrap = i;
                    wrap_score = items[i].row;
                }
            }
            else
            {
                if (items[i].row > wrap_score)
                {
                    wrap = i;
                    wrap_score = items[i].row;
                }
            }
        }

        if (best >= 0)
            return best;
        if (wrap >= 0)
            return wrap;
    }

    return current;
}

cptr character_sheet_skill_description(int skill)
{
    if (skill < 0 || skill >= S_MAX)
        return "";
    if (character_sheet_skill_descriptions[skill])
        return character_sheet_skill_descriptions[skill];
    return "";
}

static void character_sheet_append_effect_text(char* buf, size_t buflen,
    cptr prefix, cptr text)
{
    char tmp[384];

    if (!buf || !buflen || !text || !text[0])
        return;

    if (prefix && prefix[0])
        strnfmt(tmp, sizeof(tmp), " %s: %s", prefix, text);
    else
        strnfmt(tmp, sizeof(tmp), " %s", text);

    SDL_strlcat(buf, tmp, buflen);
}

static void character_sheet_append_curse_hint(char* buf, size_t buflen,
    u32b flag)
{
    if (!flag || !z_info || !cu_info || !cu_text)
        return;

    for (int id = 0; id < (int)z_info->cu_max; id++)
    {
        int stacks = CURSE_GET(id);
        bool blessing = (stacks < 0);
        bool matches;
        cptr desc = NULL;
        cptr power = NULL;

        if (!stacks)
            continue;

        matches = blessing
            ? ((cu_info[id].blessing_flags & flag) != 0)
            : ((cu_info[id].flags & flag) != 0);
        if (!matches)
            continue;

        desc = blessing
            ? (cu_info[id].blessing_text ? cu_text + cu_info[id].blessing_text : NULL)
            : (cu_info[id].text ? cu_text + cu_info[id].text : NULL);
        power = blessing
            ? (cu_info[id].blessing_power ? cu_text + cu_info[id].blessing_power : NULL)
            : (cu_info[id].power ? cu_text + cu_info[id].power : NULL);

        if (CURSE_SEEN(id) && power && power[0])
        {
            character_sheet_append_effect_text(buf, buflen,
                blessing ? "Known blessing" : "Known curse", power);
        }
        else if (desc && desc[0])
        {
            character_sheet_append_effect_text(buf, buflen,
                blessing ? "Unknown blessing" : "Unknown curse", desc);
        }
        return;
    }
}

static void character_sheet_format_tenths(char* buf, size_t buflen, long tenths)
{
    if (!buf || !buflen)
        return;

    if (tenths < 0)
        tenths = 0;

    strnfmt(buf, buflen, "%ld.%ld", tenths / 10L, tenths % 10L);
}

static cptr character_sheet_stat_full_name(int stat)
{
    switch (stat)
    {
    case A_STR: return "Strength";
    case A_DEX: return "Dexterity";
    case A_CON: return "Constitution";
    case A_GRA: return "Grace";
    }

    return "Attribute";
}

static cptr character_sheet_stat_description(int stat)
{
    switch (stat)
    {
    case A_STR: return "melee damage dice and weight capacity";
    case A_DEX: return "melee, evasion, archery, and stealth";
    case A_CON: return "maximum health";
    case A_GRA: return "will, perception, smithing, song, and voice";
    }

    return "character performance";
}

void character_sheet_format_stat_hint(int stat, int value, bool has_value,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (stat < 0 || stat >= A_MAX)
        return;

    if (has_value)
    {
        strnfmt(buf, buflen, "%s: Affects %s. Selection total modifier %+d.",
            character_sheet_stat_full_name(stat),
            character_sheet_stat_description(stat), value);
    }
    else
    {
        strnfmt(buf, buflen, "%s: Affects %s.",
            character_sheet_stat_full_name(stat),
            character_sheet_stat_description(stat));
    }
}

static int character_sheet_value_stat(character_sheet_value_kind kind)
{
    switch (kind)
    {
    case CHARACTER_SHEET_VALUE_STR: return A_STR;
    case CHARACTER_SHEET_VALUE_DEX: return A_DEX;
    case CHARACTER_SHEET_VALUE_CON: return A_CON;
    case CHARACTER_SHEET_VALUE_GRA: return A_GRA;
    default: return -1;
    }
}

static void character_sheet_format_stat_item(const character_sheet_item* item,
    char* buf, size_t buflen)
{
    int stat;
    char use_buf[16];
    char base_buf[16];

    if (!buf || !buflen || !item)
        return;

    buf[0] = '\0';
    stat = character_sheet_value_stat((character_sheet_value_kind)item->value_kind);
    if (stat < 0 || stat >= A_MAX)
        return;

    cnv_stat(p_ptr->stat_use[stat], use_buf);
    cnv_stat(p_ptr->stat_base[stat], base_buf);
    strnfmt(buf, buflen,
        "%s: %s current = %s base%+d equip%+d misc%+d drain. Affects %s.",
        character_sheet_stat_full_name(stat), use_buf, base_buf,
        p_ptr->stat_equip_mod[stat], p_ptr->stat_misc_mod[stat],
        p_ptr->stat_drain[stat], character_sheet_stat_description(stat));
}

static void character_sheet_format_value_item(const character_sheet_item* item,
    char* buf, size_t buflen)
{
    if (!buf || !buflen || !item)
        return;

    buf[0] = '\0';

    if (character_sheet_value_stat((character_sheet_value_kind)item->value_kind)
        >= 0)
    {
        character_sheet_format_stat_item(item, buf, buflen);
        return;
    }

    switch ((character_sheet_value_kind)item->value_kind)
    {
    case CHARACTER_SHEET_VALUE_EXP:
        strnfmt(buf, buflen,
            "Experience: %ld unspent / %ld earned. Unspent XP pays the increasing cost to raise skills and purchase abilities; earned is your lifetime total and does not fall when XP is spent.",
            (long)p_ptr->new_exp, (long)p_ptr->exp);
        break;
    case CHARACTER_SHEET_VALUE_BURDEN:
    {
        char carried[32];
        char limit[32];
        character_sheet_format_tenths(carried, sizeof(carried),
            (long)p_ptr->total_weight);
        character_sheet_format_tenths(limit, sizeof(limit),
            (long)weight_limit());
        strnfmt(buf, buflen,
            "Burden: %s/%s lb carried. Strength sets the limit. Exceeding it costs 1 speed; above 150%% you cannot move or pick up more. Inventory, supplies, and lamp oil all count.",
            carried, limit);
        break;
    }
    case CHARACTER_SHEET_VALUE_DEPTH:
        strnfmt(buf, buflen,
            "Depth c/m: %d ft current / %d ft minimum. Current is your location; minimum is the shallowest depth stairs can return you to as time and Deep Call pressure force you deeper.",
            p_ptr->depth * 50, min_depth() * 50);
        break;
    case CHARACTER_SHEET_VALUE_DEEP_CALL:
    {
        int base = 0;
        int additional = 0;
        int total = 0;
        int progress = 0;
        int threshold = 0;

        min_depth_timer_status(&base, &additional, &total, &progress,
            &threshold);
        if (threshold > 0)
        {
            strnfmt(buf, buflen,
                "Deep Call: minimum-depth pressure gains %d per turn (%d base %+d from extra depth, carried Deep Call items, and kills). Progress is %d/%d toward the next 50-ft rise.",
                total, base, additional, progress, threshold);
        }
        else
        {
            SDL_strlcpy(buf,
                "Deep Call: minimum-depth pressure from time, extra depth, carried Deep Call items, and kills. Each completed threshold raises the shallowest depth stairs can return you to.",
                buflen);
        }
        break;
    }
    case CHARACTER_SHEET_VALUE_DEPTH_PROGRESS:
    {
        int progress = 0;
        int threshold = 0;

        min_depth_timer_status(NULL, NULL, NULL, &progress, &threshold);
        strnfmt(buf, buflen,
            "Minimum-depth progress: %d/%d toward the next 50-ft rise. It advances each turn at the Deep Call rate; extra depth, carried Deep Call items, and kills can accelerate it.",
            progress, threshold);
        break;
    }
    case CHARACTER_SHEET_VALUE_TURN:
    {
        char turns[32];
        comma_number(turns, playerturn);
        strnfmt(buf, buflen,
            "Turn: %s player turns elapsed. This clock drives regeneration, timed effects, and the changing base rate of minimum-depth pressure.",
            turns);
        break;
    }
    case CHARACTER_SHEET_VALUE_LIGHT:
        strnfmt(buf, buflen,
            "Light: radius %d. It illuminates nearby tiles and helps you see, but your light also contributes to how readily monsters notice you.",
            p_ptr->cur_light);
        break;
    case CHARACTER_SHEET_VALUE_MELEE:
        strnfmt(buf, buflen,
            "Melee: main hand (%+d,%dd%d). The first value is your attack score against enemy Evasion; the dice are base weapon damage before protection. Criticals, slays, and abilities may add dice.",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        break;
    case CHARACTER_SHEET_VALUE_MELEE_X2:
        strnfmt(buf, buflen,
            "Melee x2: Rapid Attack grants a second main-hand attack at (%+d,%dd%d). The first value contests Evasion; the dice are its base damage before protection.",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        break;
    case CHARACTER_SHEET_VALUE_OFFHAND:
        strnfmt(buf, buflen,
            "Offhand: secondary attack (%+d,%dd%d). The first value contests Evasion; the dice are offhand base damage before protection and may differ from your main hand.",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
            p_ptr->mdd2, p_ptr->mds2);
        break;
    case CHARACTER_SHEET_VALUE_BOWS:
        strnfmt(buf, buflen,
            "Bows: ranged attacks (%+d,%dd%d). The first value is your attack score against enemy Evasion; the dice are base bow damage before protection. Range, criticals, and abilities may modify a shot.",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
        break;
    case CHARACTER_SHEET_VALUE_ARMOR:
        strnfmt(buf, buflen,
            "Armor: [%+d,%d-%d]. The first value is total Evasion used to avoid attacks; the range is minimum-maximum protection against physical damage after a hit lands.",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true),
            p_max(GF_HURT, true));
        break;
    case CHARACTER_SHEET_VALUE_HEALTH:
        strnfmt(buf, buflen,
            "Health: %d/%d hit points. Damage removes current Health and reaching 0 is fatal. Constitution sets maximum Health; resting can restore missing Health.",
            MIN(p_ptr->chp, 999), MIN(p_ptr->mhp, 999));
        break;
    case CHARACTER_SHEET_VALUE_VOICE:
        strnfmt(buf, buflen,
            "Voice: %d/%d song points. Singing spends current Voice, which does not regenerate while a song is active. Grace sets maximum Voice; resting restores it when you are not singing.",
            MIN(p_ptr->csp, 999), MIN(p_ptr->msp, 999));
        break;
    case CHARACTER_SHEET_VALUE_SONG:
    {
        cptr song1 = (p_ptr->song1 != SNG_NOTHING)
            ? b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name
            : NULL;
        cptr song2 = (p_ptr->song2 != SNG_NOTHING)
            ? b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name
            : NULL;

        if (song1 && prefix(song1, "Song of "))
            song1 += 8;
        if (song2 && prefix(song2, "Song of "))
            song2 += 8;
        if (item->label && streq(item->label, "Theme") && song2)
        {
            strnfmt(buf, buflen,
                "Theme: %s is the minor theme woven with %s. Minor themes normally use reduced Song skill, still spend their Voice cost, and may form a synergy pair.",
                song2, song1 ? song1 : "your primary song");
        }
        else if (song1)
        {
            strnfmt(buf, buflen,
                "Song: %s is your primary song%s. It uses your full Song skill and spends Voice while maintained; Voice does not regenerate while singing.",
                song1, song2 ? " with a woven minor theme" : "");
        }
        else if (song2)
            strnfmt(buf, buflen,
                "Theme: %s is an active woven minor theme. It spends Voice while maintained; Voice does not regenerate while singing.", song2);
        else
            SDL_strlcpy(buf, "Song: no active song.", buflen);
        break;
    }
    default:
        SDL_strlcpy(buf, item->desc ? item->desc : "Character sheet value.",
            buflen);
        break;
    }
}

static void character_sheet_format_skill_item(const character_sheet_item* item,
    char* buf, size_t buflen)
{
    int skill;
    int next_cost;
    int stat_mod;
    int equip_mod;
    int misc_mod;

    if (!buf || !buflen || !item)
        return;

    buf[0] = '\0';
    skill = item->skill;
    if (skill < 0 || skill >= S_MAX || skill == S_SPC)
        return;

    next_cost = (p_ptr->skill_base[skill] + 1) * 100;
    stat_mod = p_ptr->skill_stat_mod[skill];
    equip_mod = p_ptr->skill_equip_mod[skill];
    misc_mod = p_ptr->skill_misc_mod[skill];

    strnfmt(buf, buflen,
        "%s: %s Current %d = base %d%+d stat%+d equip%+d misc. Next point: %d XP.",
        skill_names_full[skill], character_sheet_skill_description(skill),
        p_ptr->skill_use[skill], p_ptr->skill_base[skill], stat_mod,
        equip_mod, misc_mod, next_cost);
}

static void character_sheet_format_trait_item(const character_sheet_item* item,
    char* buf, size_t buflen)
{
    cptr label;

    if (!buf || !buflen || !item)
        return;

    buf[0] = '\0';
    label = item->label ? item->label : "Trait";

    if (item->trait_score != 0)
    {
        int amount = ABS(item->trait_score);
        cptr rank;

        if (item->proficiency)
        {
            cptr weapon = (item->aff_flag == RHF_BOW_PROFICIENCY)
                ? "bows" : "axes";

            strnfmt(buf, buflen,
                "%s: %+d attack bonus when using %s.",
                label, amount, weapon);
            character_sheet_append_curse_hint(buf, buflen, item->aff_flag);
            return;
        }

        if (item->trait_score == 2)
            rank = "Mastery";
        else if (item->trait_score == 1)
            rank = "Affinity";
        else if (item->trait_score == -1)
            rank = "Penalty";
        else
            rank = "Grand penalty";

        strnfmt(buf, buflen,
            "%s: %s gives %+d current %s and changes ability costs in that skill.",
            label, rank, item->trait_score, skill_names_full[item->skill]);
        character_sheet_append_curse_hint(buf, buflen,
            item->trait_score > 0 ? item->aff_flag : item->pen_flag);
        return;
    }

    strnfmt(buf, buflen, "%s: %s", label,
        item->desc ? item->desc : "A special racial or character trait.");
}

void character_sheet_format_trait_description(cptr label, int skill,
    int trait_score, bool proficiency, u32b aff_flag, u32b pen_flag,
    cptr desc, char* buf, size_t buflen)
{
    character_sheet_item item;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    SDL_zero(item);
    item.kind = CHARACTER_SHEET_ITEM_TRAIT;
    item.label = label;
    item.skill = skill;
    item.trait_score = trait_score;
    item.proficiency = proficiency;
    item.aff_flag = aff_flag;
    item.pen_flag = pen_flag;
    item.desc = desc;

    character_sheet_format_trait_item(&item, buf, buflen);
}

static void character_sheet_format_item_description(
    const character_sheet_item* item, char* desc, size_t descsz)
{
    if (!desc || !descsz)
        return;

    desc[0] = '\0';
    if (!item)
        return;

    if (item->kind == CHARACTER_SHEET_ITEM_SKILL)
        character_sheet_format_skill_item(item, desc, descsz);
    else if (item->kind == CHARACTER_SHEET_ITEM_TRAIT)
        character_sheet_format_trait_item(item, desc, descsz);
    else
        character_sheet_format_value_item(item, desc, descsz);
}

/* Share the live character sheet's exact vital explanations with birth-time
 * attribute/skill allocation.  Those screens display the same live values,
 * so maintaining a second, abbreviated description table in the SDL renderer
 * would let the two views drift apart. */
void character_sheet_format_vital_description(cptr label, char* buf,
    size_t buflen)
{
    character_sheet_item item;
    int kind = -1;

    if (!buf || buflen == 0)
        return;
    buf[0] = '\0';
    if (!label || !label[0] || !p_ptr)
        return;

    if (streq(label, "Exp")) kind = CHARACTER_SHEET_VALUE_EXP;
    else if (streq(label, "Burden")) kind = CHARACTER_SHEET_VALUE_BURDEN;
    else if (streq(label, "Depth c/m")) kind = CHARACTER_SHEET_VALUE_DEPTH;
    else if (streq(label, "Depth timer")
        || streq(label, "Minimum depth progress"))
    {
        kind = CHARACTER_SHEET_VALUE_DEPTH_PROGRESS;
    }
    else if (streq(label, "Deep Call"))
        kind = CHARACTER_SHEET_VALUE_DEEP_CALL;
    else if (streq(label, "Turn")) kind = CHARACTER_SHEET_VALUE_TURN;
    else if (streq(label, "Light")) kind = CHARACTER_SHEET_VALUE_LIGHT;
    else if (streq(label, "Melee")) kind = CHARACTER_SHEET_VALUE_MELEE;
    else if (streq(label, "Melee x2"))
        kind = CHARACTER_SHEET_VALUE_MELEE_X2;
    else if (streq(label, "Offhand")) kind = CHARACTER_SHEET_VALUE_OFFHAND;
    else if (streq(label, "Bows")) kind = CHARACTER_SHEET_VALUE_BOWS;
    else if (streq(label, "Armor")) kind = CHARACTER_SHEET_VALUE_ARMOR;
    else if (streq(label, "Health")) kind = CHARACTER_SHEET_VALUE_HEALTH;
    else if (streq(label, "Voice")) kind = CHARACTER_SHEET_VALUE_VOICE;
    else if (streq(label, "Song") || streq(label, "Theme"))
        kind = CHARACTER_SHEET_VALUE_SONG;

    if (kind < 0)
        return;

    SDL_zero(item);
    item.kind = CHARACTER_SHEET_ITEM_VALUE;
    item.value_kind = kind;
    item.label = label;
    character_sheet_format_value_item(&item, buf, buflen);
}

void do_cmd_character_sheet(void)
{
    char ch;
    int focus_item = -1;
    bool focus_from_pointer = false;

    /* Clear any active banner before opening character sheet */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_push_terminal_menu_scale();
    sdl_screen_back_gesture_begin();

    /* Forever */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();
        character_sheet_item sheet_items[CHARACTER_SHEET_MAX_ITEMS];
        int sheet_item_count;
        int focus_choice;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

        sheet_item_count = character_sheet_collect_semantic_items(sheet_items,
            CHARACTER_SHEET_MAX_ITEMS);
        if (focus_item >= sheet_item_count)
        {
            focus_item = -1;
            focus_from_pointer = false;
        }

        /* Drive the pixel-semantic live overlay; it renders its own title,
         * columns, and prompt, so nothing is drawn to the terminal. */
        focus_choice = (focus_item >= 0)
            ? CHARACTER_SHEET_CLICK_ITEM_BASE + focus_item
            : -1;
        sdl_character_sheet_screen_begin_live(focus_choice);
        for (int i = 0; i < sheet_item_count; i++)
        {
            char desc[640];

            character_sheet_format_item_description(&sheet_items[i],
                desc, sizeof(desc));
            sdl_character_sheet_screen_add_live_item(
                CHARACTER_SHEET_CLICK_ITEM_BASE + i,
                (int)sheet_items[i].kind, sheet_items[i].skill,
                sheet_items[i].value_kind, sheet_items[i].label, desc);
        }
        sdl_hover_tooltip_clear();

        Term_fresh();  /* Present the live character-sheet overlay. */

        /* Keep the cursor hidden while the character sheet is active. */
        {
            bool saved_hide_cursor = hide_cursor;
            hide_cursor = true;
            ch = inkey();
            hide_cursor = saved_hide_cursor;
        }

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice >= CHARACTER_SHEET_CLICK_ITEM_BASE
                    && clicked_choice < CHARACTER_SHEET_CLICK_ITEM_BASE
                        + sheet_item_count)
                {
                    int clicked_item =
                        clicked_choice - CHARACTER_SHEET_CLICK_ITEM_BASE;
                    bool same_item = (clicked_item == focus_item);

                    focus_item = clicked_item;
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        focus_from_pointer = true;
                        continue;
                    }

                    focus_from_pointer = false;

                    if (sheet_items[focus_item].kind
                            == CHARACTER_SHEET_ITEM_SKILL
                        && (same_item
                            || click_action == UI_MENU_CLICK_SECONDARY))
                    {
                        gain_skills_set_initial_skill(
                            sheet_items[focus_item].skill);
                        ch = 'i';
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = (char)clicked_choice;
                }
            }
            else if (ch == UI_MENU_CLICK_WAKE_KEY)
            {
                int hover_choice = 0;

                if (focus_from_pointer
                    && !ui_menu_click_get_hover_choice(&hover_choice))
                {
                    focus_item = -1;
                    focus_from_pointer = false;
                    sdl_hover_tooltip_clear();
                }
                continue;
            }
        }

        /* Exit - B button (back) or ESC */
        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
        {
            sdl_hover_tooltip_clear();
            break;
        }
        if ((ch == '\r') || (ch == '\n') || (ch == 'q') || (ch == 'Q'))
        {
            sdl_hover_tooltip_clear();
            break;
        }

        {
            int d = target_dir((char)ch);

            if (d && sheet_item_count > 0)
            {
                int next_focus = character_sheet_find_best_focus(sheet_items,
                    sheet_item_count, focus_item, ddx[d], ddy[d]);

                if (next_focus >= 0 && next_focus < sheet_item_count
                    && next_focus != focus_item)
                {
                    focus_item = next_focus;
                    focus_from_pointer = false;
                    continue;
                }
                if (focus_item < 0 && next_focus >= 0)
                {
                    focus_item = next_focus;
                    focus_from_pointer = false;
                    continue;
                }
            }
        }

        /* Increase skills - 'i', Space, or confirm button */
        if (ch == 'i' || ch == ' ' || ch == (char)INPUT_BIND_CONFIRM
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            if (focus_item >= 0 && focus_item < sheet_item_count
                && sheet_items[focus_item].kind == CHARACTER_SHEET_ITEM_SKILL)
            {
                gain_skills_set_initial_skill(sheet_items[focus_item].skill);
            }
            gain_skills();
            /* Force redraw after skill changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* Show notes - 'n' */
        else if (ch == 'n')
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            do_cmd_knowledge_notes();
        }

        /* Story stats - 's' or Y button */
        else if (ch == 's' || (steamdeck && ch == steamdeck_secondary_key()))
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            print_metarun_stats();
        }

#ifdef DEBUG_CURSES
        /* Curses Menu */
        else if (ch == 'c')
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            dbg_show_active_flags();
        }
#endif

        /* Abilities - 'x', 'y', or X button */
        else if ((ch == 'x') || (ch == 'y') || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            (void)do_cmd_ability_screen();
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump - 'f' or L1 */
        else if (ch == 'f' || (steamdeck && ch == steamdeck_prev_page_key()))
        {
            char ftmp[80];

            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

            if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
            {
                if (ftmp[0] && (ftmp[0] != ' '))
                {
                    if (file_character(ftmp, false))
                    {
                        msg_print("Character dump failed!");
                    }
                    else
                    {
                        msg_print("Character dump successful.");
                    }
                }
            }
        }

        /* Tutorial / Help - '?' or RS Right.
         * Keep the live sheet visible so the coach overlay can use it as the
         * backdrop; the loop hides/redraws the sheet after this returns. */
        else if (ch == '?' || (steamdeck && ch == steamdeck_info_key()))
        {
            sdl_hover_tooltip_clear();
            display_character_tutorial();
        }

        /* Oops */
        else
        {
            bell("Illegal command for character sheet!");
        }

        /* Flush messages */
        sdl_character_sheet_screen_hide();
        message_flush();
    }

    /* Load screen */
    ui_menu_click_clear();
    sdl_hover_tooltip_clear();
    sdl_character_sheet_screen_hide();
    sdl_pop_terminal_menu_scale();
    sdl_screen_back_gesture_end();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();

    /* Force redraw after screen restore if skills/abilities were changed */
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
    handle_stuff();
}

/*
 * Read-only full character sheet shown during birth, after race/character
 * selection and before stat allocation: the player sees their whole sheet
 * "full at first", then any key or click continues into stat allocation.
 * Reuses the same pixel-semantic live-sheet renderer as do_cmd_character_sheet.
 */
void character_sheet_show_birth_preview(void)
{
    character_sheet_item sheet_items[CHARACTER_SHEET_MAX_ITEMS];
    int count;

    if (!p_ptr)
        return;

    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();

    sdl_character_sheet_screen_begin_birth_preview();
    count = character_sheet_collect_semantic_items(sheet_items,
        CHARACTER_SHEET_MAX_ITEMS);
    for (int i = 0; i < count; i++)
    {
        char desc[640];

        character_sheet_format_item_description(&sheet_items[i], desc,
            sizeof(desc));
        sdl_character_sheet_screen_add_live_item(
            CHARACTER_SHEET_CLICK_ITEM_BASE + i,
            (int)sheet_items[i].kind, sheet_items[i].skill,
            sheet_items[i].value_kind, sheet_items[i].label, desc);
    }
    sdl_hover_tooltip_clear();

    /* Wait for a fresh key or click to continue (hover wakeups are ignored). */
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    Term_fresh();
    flush();
    while (1)
    {
        int ch = inkey();

        if (ch == UI_MENU_CLICK_WAKE_KEY)
        {
            continue;
        }
        break;
    }

    ui_menu_click_clear();
    sdl_hover_tooltip_clear();
    sdl_character_sheet_screen_hide();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();
}

#define COL_SKILL 2
#define COL_ABILITY 16
#define COL_DESCRIPTION 41
#define ABILITY_MENU_LIST_WIDTH (COL_DESCRIPTION - COL_ABILITY)
#define ABILITY_MENU_CLICK_EXIT -1
#define ABILITY_MENU_CLICK_TRAIN -2
#define ABILITY_MENU_CLICK_ACTION -3
#define ABILITY_MENU_CLICK_SCROLL_UP -4
#define ABILITY_MENU_CLICK_SCROLL_DOWN -5
#define ABILITY_MENU_CLICK_SKILL_ALLOCATE -6
#define ABILITY_MENU_CLICK_PREV_SKILL -7
#define ABILITY_MENU_CLICK_NEXT_SKILL -8
#define ABILITY_MENU_CLICK_SKILL_BASE 1000
#define ABILITY_MENU_CLICK_ABILITY_BASE 2000
#define ABILITY_MENU_SWITCH_SKILL_BASE (ABILITIES_MAX + 10)
#define ABILITY_BROWSER_DESC_MAX_LINES 512
#define ABILITY_BROWSER_DESC_LINE_LEN 160
