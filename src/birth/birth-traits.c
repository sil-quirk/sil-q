/* File: birth/birth-traits.c */

#include "angband.h"
#include "birth/birth-internal.h"

/* Character ability names */
static const char *character_ability_names[S_MAX][ABILITIES_MAX] =
{
    [S_MEL] = {
        [MEL_POWER]            = "Power",
        [MEL_FINESSE]          = "Finesse",
        [MEL_KNOCK_BACK]       = "Knock Back",
        [MEL_THROWING]         = "Throwing",
        [MEL_POLEARMS]         = "Polearm Mastery",
        [MEL_CHARGE]           = "Charge",
        [MEL_FOLLOW_THROUGH]   = "Follow-Through",
        [MEL_IMPALE]           = "Impale",
        [MEL_CONTROL]          = "Subtlety",
        [MEL_WHIRLWIND_ATTACK] = "Whirlwind Attack",
        [MEL_ZONE_OF_CONTROL]  = "Zone of Control",
        [MEL_SMITE]            = "Smite",
        [MEL_TWO_WEAPON]       = "Two Weapon Fighting",
        [MEL_RAPID_ATTACK]     = "Rapid Attack",
        [MEL_STR]              = NULL,  /* if you care about STR */
        [MEL_WARDEN]           = "Warden",
        [MEL_POWER_THROW]      = "Power Throw",
    },
    [S_ARC] = {
        [ARC_ROUT]        = "Rout",
        [ARC_FLETCHERY]   = "Fletchery",
        [ARC_POINT_BLANK] = "Point Blank Archery",
        [ARC_PUNCTURE]    = "Puncture",
        [ARC_AMBUSH]      = "Ambush",
        [ARC_VERSATILITY] = "Versatility",
        [ARC_CRIPPLING]   = "Crippling Shot",
        [ARC_DEADLY_HAIL] = "Deadly Hail",
        [ARC_DEX]         = NULL,
    },
    [S_EVN] = {
        [EVN_DODGING]            = "Dodging",
        [EVN_BLOCKING]           = "Blocking",
        [EVN_PARRY]              = "Parry",
        [EVN_CROWD_FIGHTING]     = "Crowd Fighting",
        [EVN_LEAPING]            = "Leaping",
        [EVN_SPRINTING]          = "Sprinting",
        [EVN_FLANKING]           = "Flanking",
        [EVN_HEAVY_ARMOUR]       = "Heavy Armour Use",
        [EVN_RIPOSTE]            = "Riposte",
        [EVN_CONTROLLED_RETREAT] = "Controlled Retreat",
        [EVN_DEX]                = NULL,
    },
    [S_STL] = {
        [STL_DISGUISE]          = "Disguise",
        [STL_ASSASSINATION]     = "Assassination",
        [STL_CRUEL_BLOW]        = "Cruel Blow",
        [STL_EXCHANGE_PLACES]   = "Exchange Places",
        [STL_OPPORTUNIST]       = "Opportunist",
        [STL_VANISH]            = "Vanish",
        [STL_DEX]               = NULL,
    },
    [S_PER] = {
        [PER_QUICK_STUDY]    = "Quick Study",
        [PER_FOCUSED_ATTACK] = "Focused Attack",
        [PER_KEEN_SENSES]    = "Keen Senses",
        [PER_CONCENTRATION]  = "Concentration",
        [PER_ALCHEMY]        = "Alchemy",
        [PER_BANE]           = "Bane",
        [PER_OUTWIT]         = "Outwit",
        [PER_LISTEN]         = "Resonance",
        [PER_MASTER_HUNTER]  = "Master Hunter",
        [PER_GRA]            = NULL,
    },
    [S_WIL] = {
        [WIL_CURSE_BREAKING]        = "Curse Breaking",
        [WIL_CHANNELING]            = "Channeling",
        [WIL_STRENGTH_IN_ADVERSITY] = "Strength in Adversity",
        [WIL_FORMIDABLE]            = "Formidable",
        [WIL_INNER_LIGHT]           = "Inner Light",
        [WIL_INDOMITABLE]           = "Indomitable",
        [WIL_OATH]                  = "Oath",
        [WIL_POISON_RESISTANCE]     = "Poison Resistance",
        [WIL_VENGEANCE]             = "Vengeance",
        [WIL_MAJESTY]               = "Majesty",
        [WIL_CON]                   = NULL,
    },
    [S_SMT] = {
        [SMT_WEAPONSMITH]   = "Weaponsmith",
        [SMT_ARMOURSMITH]   = "Armoursmith",
        [SMT_JEWELLER]      = "Jeweller",
        [SMT_ENCHANTMENT]   = "Enchantment",
        [SMT_EXPERTISE]     = "Expertise",
        [SMT_ARTEFACT]      = "Artifice",
        [SMT_MASTERPIECE]   = "Masterpiece",
        [SMT_ALLOY_MASTERY] = "Alloy mastery",
        [SMT_GRA]           = NULL,
    },
    [S_SNG] = {
        [SNG_ELBERETH]      = "Song of Elbereth",
        [SNG_CHALLENGE]     = "Song of Challenge",
        [SNG_DELVINGS]      = "Song of Delvings",
        [SNG_FREEDOM]       = "Song of Freedom",
        [SNG_SILENCE]       = "Song of Silence",
        [SNG_STAUNCHING]    = "Song of Staunching",
        [SNG_THRESHOLDS]    = "Song of Thresholds",
        [SNG_TREES]         = "Song of the Trees",
        [SNG_REVEALING]     = "Song of Revealing",
        [SNG_WOVEN_THEMES]  = "Woven Themes",
        [SNG_SLAYING]       = "Song of Slaying",
        [SNG_ELVENESS]      = "Song of Elveness",
        [SNG_STAYING]       = "Song of Staying",
        [SNG_DISGUISE]      = "Song of Disguise",
        [SNG_LORIEN]        = "Song of LÃ³rien",
        [SNG_SHATTERING]    = "Song of Shattering",
        [SNG_MASTERY]       = "Song of Mastery",
        [SNG_CONTEST]       = "Song of Contest",
        [SNG_LAMENT]        = "Song of Lament",
        [SNG_GRA]           = NULL,
    },
    [S_SPC] = {
        [SPC_MANDOS] = "Mandos' Doom", /* immunity reward */
        [SPC_AULE] = "AulÃ«'s Forge", /* improved masterpiece forging */
        [SPC_OATH_MERCY] = "Oath of Mercy",
        [SPC_OATH_SILENCE] = "Oath of Silence",
        [SPC_OATH_IRON] = "Oath of Iron",
        [SPC_NIENA_MERCY] = "Nienna's Gift of Mercy", /* Enhanced stealth from mercy quest */
        [SPC_OATH_SMITH] = "Oath of the Smith",
        [SPC_OATH_VALOROUS] = "Oath of the Valorous Heart",
        [SPC_UNIQUE_BANE] = "Unique Bane", /* Enhanced effectiveness against unique monsters */
        [SPC_OATH_LIGHT] = "Oath of Light",
    },
};

int collect_character_starting_abilities(int character, cptr out[],
    int out_max, int out_skill[], int out_ability[])
{
    int count = 0;

    if (character <= 0)
        return 0;

    if (c_info[character].flags_u & UNQ_MIM)
        return 0;

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[character].a_adj[slot][0];
        int ability = c_info[character].a_adj[slot][1];
        cptr name;

        if (stat < 0)
            break;

        if (stat >= S_MAX || ability < 0 || ability >= ABILITIES_MAX)
            continue;

        name = character_ability_names[stat][ability];
        if (!name)
            continue;

        if (out && count < out_max)
        {
            out[count] = name;
            if (out_skill)
                out_skill[count] = stat;
            if (out_ability)
                out_ability[count] = ability;
        }

        count++;
    }

    return count;
}

void birth_format_ability_hint(int skill, int ability, char* buf,
    size_t buflen)
{
    int idx;
    ability_type* b_ptr;
    cptr name;
    cptr effect = NULL;
    cptr lore = NULL;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (skill < 0 || skill >= S_MAX || ability < 0 || ability >= ABILITIES_MAX)
        return;

    idx = ability_index(skill, ability);
    if (idx < 0 || idx >= z_info->b_max)
        return;

    b_ptr = &b_info[idx];
    if (!b_ptr->name)
        return;

    name = b_name + b_ptr->name;
    if (b_ptr->effect)
        effect = b_text + b_ptr->effect;
    if (b_ptr->text)
        lore = b_text + b_ptr->text;

    if (effect && effect[0])
        strnfmt(buf, buflen, "%s (%s): %s", name,
            skill_names_full[skill], effect);
    else if (lore && lore[0])
        strnfmt(buf, buflen, "%s (%s): %s", name,
            skill_names_full[skill], lore);
    else
        strnfmt(buf, buflen, "%s (%s): Starting ability.", name,
            skill_names_full[skill]);
}

int collect_character_trait_lines(int race, int character, bool short_labels,
    birth_compact_flag_line out[], int out_max, int* max_line_len)
{
    int total = 0;

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery = TERM_L_GREEN;
    byte attr_penalty = TERM_RED;
    byte attr_gr_penalty = TERM_L_RED;

    birth_compact_flag_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH_TRAIT(arr, n, text, color, side_value, skill_value, score_value, \
    proficiency_value, aff_value, pen_value, desc_label_value)                \
    do {                                                                      \
        if ((text) && (n) < (int)N_ELEMENTS(arr))                             \
        {                                                                     \
            birth_compact_flag_line* _line = &(arr)[(n)++];                  \
            _line->txt = (text);                                              \
            _line->attr = (color);                                            \
            _line->side = (side_value);                                       \
            _line->skill = (skill_value);                                     \
            _line->trait_score = (score_value);                               \
            _line->proficiency = (proficiency_value);                         \
            _line->aff_flag = (aff_value);                                    \
            _line->pen_flag = (pen_value);                                    \
            _line->desc_label = (desc_label_value);                           \
        }                                                                     \
    } while (0)

#define HANDLE_SKILL_EX(LABEL_LONG, LABEL_SHORT, SKILL, AFF_FLAG, PEN_FLAG,   \
    PROFICIENCY)                                                              \
    do {                                                                      \
        int score = 0;                                                        \
        if (p_info[race].flags & (AFF_FLAG)) score++;                         \
        if (c_info[character].flags & (AFF_FLAG)) score++;                    \
        if ((PEN_FLAG) && (p_info[race].flags & (PEN_FLAG))) score--;         \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;    \
        score += curse_flag_count_rhf(AFF_FLAG);                              \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);              \
        if (score > 2) score = 2;                                             \
        if (score < -2) score = -2;                                           \
        if (score == 2)                                                       \
            PUSH_TRAIT(ma_buf, ma_n,                                          \
                short_labels ? LABEL_SHORT "++" : LABEL_LONG " mastery",      \
                attr_mastery, 0, (SKILL), score, (PROFICIENCY),               \
                (AFF_FLAG), (PEN_FLAG), NULL);                                \
        else if (score == 1)                                                  \
            PUSH_TRAIT(af_buf, af_n,                                          \
                short_labels ? LABEL_SHORT "+ " : LABEL_LONG " affinity",     \
                attr_affinity, 0, (SKILL), score, (PROFICIENCY),              \
                (AFF_FLAG), (PEN_FLAG), NULL);                                \
        else if (score == -1)                                                 \
            PUSH_TRAIT(pen_buf, pen_n,                                        \
                short_labels ? LABEL_SHORT "- " : LABEL_LONG " penalty",      \
                attr_penalty, 1, (SKILL), score, (PROFICIENCY),               \
                (AFF_FLAG), (PEN_FLAG), NULL);                                \
        else if (score == -2)                                                 \
            PUSH_TRAIT(pen_buf, pen_n,                                        \
                short_labels ? LABEL_SHORT "--" : LABEL_LONG " grand penalty",\
                attr_gr_penalty, 1, (SKILL), score, (PROFICIENCY),            \
                (AFF_FLAG), (PEN_FLAG), NULL);                                \
    } while (0)

#define HANDLE_UNIQUE_EX(LABEL_LONG, LABEL_SHORT, DESC_LABEL, FLAG, COLOR)    \
    do {                                                                      \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG))) \
            PUSH_TRAIT(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG, \
                (COLOR), 1, -1, 0, false, 0, 0, (DESC_LABEL));                \
    } while (0)

#define HANDLE_UNIQUE_U_EX(LABEL_LONG, LABEL_SHORT, DESC_LABEL, FLAG, COLOR)  \
    do {                                                                      \
        if (c_info[character].flags_u & (FLAG))                               \
            PUSH_TRAIT(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG, \
                (COLOR), 1, -1, 0, false, 0, 0, (DESC_LABEL));                \
    } while (0)

#define EMIT(arr, n)                                                          \
    do {                                                                      \
        for (int _i = 0; _i < (n); ++_i)                                      \
        {                                                                     \
            cptr _txt = (arr)[_i].txt ? (arr)[_i].txt : "";                  \
            if (max_line_len && (int)strlen(_txt) > *max_line_len)            \
                *max_line_len = (int)strlen(_txt);                            \
            if (out && total < out_max)                                       \
                out[total] = (arr)[_i];                                       \
            total++;                                                          \
        }                                                                     \
    } while (0)

    HANDLE_SKILL_EX("melee", "melee", S_MEL, RHF_MEL_AFFINITY, RHF_MEL_PENALTY, false);
    HANDLE_SKILL_EX("evasion", "evasion", S_EVN, RHF_EVN_AFFINITY, RHF_EVN_PENALTY, false);
    HANDLE_SKILL_EX("stealth", "stealth", S_STL, RHF_STL_AFFINITY, RHF_STL_PENALTY, false);
    HANDLE_SKILL_EX("archery", "archery", S_ARC, RHF_ARC_AFFINITY, RHF_ARC_PENALTY, false);
    HANDLE_SKILL_EX("will", "will", S_WIL, RHF_WIL_AFFINITY, RHF_WIL_PENALTY, false);
    HANDLE_SKILL_EX("perception", "perception", S_PER, RHF_PER_AFFINITY, RHF_PER_PENALTY, false);
    HANDLE_SKILL_EX("smithing", "smithing", S_SMT, RHF_SMT_AFFINITY, RHF_SMT_PENALTY, false);
    HANDLE_SKILL_EX("song", "song", S_SNG, RHF_SNG_AFFINITY, RHF_SNG_PENALTY, false);
    HANDLE_SKILL_EX("bow", "bow", S_ARC, RHF_BOW_PROFICIENCY, 0, true);
    HANDLE_SKILL_EX("axe", "axe", S_MEL, RHF_AXE_PROFICIENCY, 0, true);

    HANDLE_UNIQUE_U_EX("Master Artisan", "Master Artisan", "Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Galvorn", "Galvorn Maker", "Creator of Galvorn", UNQ_SMT_EOL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("One Handed", "One Handed", "One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Agarwaen", "Agarwaen", "Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Shadow Walker", "Shadow Walk", "Shadow Walker", UNQ_SNG_TURGON, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Chosen of Ulmo", "Ulmo's Chosen", "Chosen of Ulmo", UNQ_WIL_TUOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Indomitable Will", "Indom. Will", "Indomitable Will", UNQ_EARENDIL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("OromÃ« Himself", "OromÃ«", "Himself", UNQ_WIL_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Songs of Power", "Songs of Power", "Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Elven Dance", "Elven Dance", "Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Girdle of Melian", "Melian's Girdle", "Girdle of Melian", UNQ_SNG_MEL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Angrist", "Angrist Maker", "Creator of Angrist", UNQ_SMT_TELCHAR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Old Master", "Old Master", "Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Ring Master", "Ring Master", "Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Aure entuluva", "Aure Entuluva", "Aure entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Voice of the Girdle", "Girdle Voice", "Voice of the Girdle", UNQ_SNG_THINGOL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Forgotten", "Forgotten", "Forgotten", UNQ_MIM, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Minstrel", "Minstrel", "Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Woven Master", "Woven Master", "Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);

    HANDLE_UNIQUE_EX("Gift of Eru", "Gift of Eru", "Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Seafarer", "Seafarer", "Seafarer", RHF_FREE, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Kinslayer", "Kinslayer", "Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    HANDLE_UNIQUE_EX("Treacherous", "Treacherous", "Treacherous", RHF_TREACHERY, TERM_UMBER);
    HANDLE_UNIQUE_EX("Doom of Mandos", "Mandos' Doom", "Doom of Mandos", RHF_CURSE, TERM_UMBER);
    HANDLE_UNIQUE_EX("Morgoth Curse", "Morgoth Curse", "Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U_EX
#undef HANDLE_UNIQUE_EX
#undef HANDLE_SKILL_EX
#undef PUSH_TRAIT

    return total;
}

void birth_format_trait_hint(const birth_compact_flag_line* line,
    char* buf, size_t buflen)
{
    cptr desc_label;
    cptr desc = NULL;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (!line || !line->txt)
        return;

    desc_label = line->desc_label ? line->desc_label : line->txt;
    if (desc_label && desc_label[0])
    {
        desc = character_sheet_trait_description(desc_label);
        if (desc && !desc[0])
            desc = NULL;
    }

    character_sheet_format_trait_description(line->txt, line->skill,
        line->trait_score, line->proficiency, line->aff_flag, line->pen_flag,
        desc, buf, buflen);
}
