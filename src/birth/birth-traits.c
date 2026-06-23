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
        [SNG_LORIEN]        = "Song of Lórien",
        [SNG_SHATTERING]    = "Song of Shattering",
        [SNG_MASTERY]       = "Song of Mastery",
        [SNG_CONTEST]       = "Song of Contest",
        [SNG_LAMENT]        = "Song of Lament",
        [SNG_GRA]           = NULL,
    },
    [S_SPC] = {
        [SPC_MANDOS] = "Mandos' Doom", /* immunity reward */
        [SPC_AULE] = "Aulë's Forge", /* improved masterpiece forging */
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

static int collect_character_starting_abilities(int character, cptr out[],
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

static void birth_format_ability_hint(int skill, int ability, char* buf,
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
        strnfmt(buf, buflen, "%s: %s", name, effect);
    else if (lore && lore[0])
        strnfmt(buf, buflen, "%s: %s", name, lore);
    else
        strnfmt(buf, buflen, "%s: Starting ability.", name);
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
    HANDLE_UNIQUE_U_EX("Oromë Himself", "Oromë", "Himself", UNQ_WIL_FIN, TERM_VIOLET);
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

static void birth_detail_hover_add_trait(int col, int row,
    const birth_compact_flag_line* line)
{
    char desc[640];
    int width;

    if (!line || !line->txt)
        return;

    birth_format_trait_hint(line, desc, sizeof(desc));
    if (!desc[0])
        return;

    width = (int)strlen(line->txt);
    if (width < 1)
        width = 1;
    birth_detail_hover_add(col, row, width, desc);
}


/*
 * Show race/character flags in priority order.
 * Masteries first, then single-side affinities, then penalties,
 * and finally any "headline / unique" flags.
 */
void print_rh_flags(int race, int character, int col, int row)
{
    int flags_left  = 0;
    int flags_right = 0;
    bool compact_layout = character_flags_need_compact_layout();
    int description_row = birth_description_base_row();
    int term_wid = 80;
    int term_hgt = 24;
    cptr ability_lines[CHARACTER_ABILITY_MAX];
    int ability_skills[CHARACTER_ABILITY_MAX];
    int ability_ids[CHARACTER_ABILITY_MAX];
    int ability_line_n = collect_character_starting_abilities(character,
        ability_lines, N_ELEMENTS(ability_lines), ability_skills, ability_ids);

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery  = TERM_L_GREEN;
    byte attr_penalty  = TERM_RED;
    byte attr_gr_penalty  = TERM_L_RED;

    const int col_pen = col + 21;

    birth_compact_flag_line mastery_buf [16], affinity_buf[16], penalty_buf[16], unique_buf[32];
    int mastery_n = 0, affinity_n = 0, penalty_n = 0, unique_n = 0;

    if (Term)
        Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;
    if (term_wid < 1)
        term_wid = 80;

/*
 * Show one skill line according to the new +/-2<->mastery / grand-penalty rule.
 *
 *   +1 for every ..._AFFINITY bit, -1 for every ..._PENALTY bit.
 *
 *        score   meaning            colour / buffer
 *        =====   ===============    =========================
 *          +2    mastery            mastery_buf  / attr_mastery
 *          +1    affinity           affinity_buf / attr_affinity
 *           0    (omit line)        -
 *          -1    penalty            penalty_buf  / attr_penalty
 *          -2    grand penalty      penalty_buf  / attr_penalty
 */
/* Show one skill line according to the new +/-2 rule,
 * now counting curse affinities / penalties too.
 */
#define BIRTH_PUSH_TRAIT(arr, n, text_value, attr_value, side_value,          \
    skill_value, score_value, proficiency_value, aff_value, pen_value,        \
    desc_label_value)                                                         \
    do {                                                                      \
        birth_compact_flag_line* _line = &(arr)[(n)++];                      \
        _line->txt = (text_value);                                            \
        _line->attr = (attr_value);                                           \
        _line->side = (side_value);                                           \
        _line->skill = (skill_value);                                         \
        _line->trait_score = (score_value);                                   \
        _line->proficiency = (proficiency_value);                             \
        _line->aff_flag = (aff_value);                                        \
        _line->pen_flag = (pen_value);                                        \
        _line->desc_label = (desc_label_value);                               \
    } while (0)

#define HANDLE_SKILL_EX(label, SKILL, AFF_FLAG, PEN_FLAG, PROFICIENCY)       \
    do {                                                                    \
        int score = 0;                                                      \
                                                                            \
        /* race + character bits */                                             \
        if (p_info[race].flags  & (AFF_FLAG)) score++;                      \
        if (c_info[character].flags & (AFF_FLAG)) score++;                      \
        if (p_info[race].flags  & (PEN_FLAG)) score--;                      \
        if (c_info[character].flags & (PEN_FLAG)) score--;                      \
                                                                            \
        /* every copy of the same *RHF* curse flag */                       \
        score += curse_flag_count_rhf(AFF_FLAG);                            \
        score -= curse_flag_count_rhf(PEN_FLAG);                            \
                                                                            \
        /* clamp so the UI never shows >mastery or >grand-penalty */        \
        if (score >  2) score =  2;                                         \
        if (score < -2) score = -2;                                         \
                                                                            \
        if (score ==  2) {                                                  \
            BIRTH_PUSH_TRAIT(mastery_buf, mastery_n, label " mastery",      \
                attr_mastery, 0, (SKILL), score, (PROFICIENCY),             \
                (AFF_FLAG), (PEN_FLAG), NULL);                              \
        } else if (score == 1) {                                            \
            BIRTH_PUSH_TRAIT(affinity_buf, affinity_n, label " affinity",   \
                attr_affinity, 0, (SKILL), score, (PROFICIENCY),            \
                (AFF_FLAG), (PEN_FLAG), NULL);                              \
        } else if (score == -1) {                                           \
            BIRTH_PUSH_TRAIT(penalty_buf, penalty_n, label " penalty",      \
                attr_penalty, 1, (SKILL), score, (PROFICIENCY),             \
                (AFF_FLAG), (PEN_FLAG), NULL);                              \
        } else if (score == -2) {                                           \
            BIRTH_PUSH_TRAIT(penalty_buf, penalty_n,                        \
                label " grand penalty", attr_gr_penalty, 1, (SKILL),        \
                score, (PROFICIENCY), (AFF_FLAG), (PEN_FLAG), NULL);        \
        }                                                                   \
    } while (0)


// New: (label, desc_label, FLAG, COLOR, SIDE) where SIDE = 0 (left) or 1 (right)
#define HANDLE_UNIQUE(label, desc_label, FLAG, COLOR, SIDE)                 \
    do {                                                                    \
        int race_has     = p_info[race].flags & (FLAG);                     \
        int character_has = c_info[character].flags & (FLAG);               \
        if (race_has || character_has) {                                    \
            BIRTH_PUSH_TRAIT(unique_buf, unique_n, label, (COLOR), (SIDE),  \
                -1, 0, false, 0, 0, (desc_label));                          \
        }                                                                   \
    } while (0)

// New: (label, desc_label, FLAG, COLOR, SIDE) where SIDE = 0 (left) or 1 (right)
#define HANDLE_UNIQUE_U(label, desc_label, FLAG, COLOR, SIDE)               \
    do {                                                                    \
        int character_has = c_info[character].flags_u & (FLAG);             \
        if (character_has) {                                                \
            BIRTH_PUSH_TRAIT(unique_buf, unique_n, label, (COLOR), (SIDE),  \
                -1, 0, false, 0, 0, (desc_label));                          \
        }                                                                   \
    } while (0)

    // Skills
    HANDLE_SKILL_EX("melee",      S_MEL, RHF_MEL_AFFINITY, RHF_MEL_PENALTY, false);
    HANDLE_SKILL_EX("evasion",    S_EVN, RHF_EVN_AFFINITY, RHF_EVN_PENALTY, false);
    HANDLE_SKILL_EX("stealth",    S_STL, RHF_STL_AFFINITY, RHF_STL_PENALTY, false);
    HANDLE_SKILL_EX("archery",    S_ARC, RHF_ARC_AFFINITY, RHF_ARC_PENALTY, false);
    HANDLE_SKILL_EX("will",       S_WIL, RHF_WIL_AFFINITY, RHF_WIL_PENALTY, false);
    HANDLE_SKILL_EX("perception", S_PER, RHF_PER_AFFINITY, RHF_PER_PENALTY, false);
    HANDLE_SKILL_EX("smithing",   S_SMT, RHF_SMT_AFFINITY, RHF_SMT_PENALTY, false);
    HANDLE_SKILL_EX("song",       S_SNG, RHF_SNG_AFFINITY, RHF_SNG_PENALTY, false);
    HANDLE_SKILL_EX("bow",        S_ARC, RHF_BOW_PROFICIENCY, 0, true);
    HANDLE_SKILL_EX("axe",        S_MEL, RHF_AXE_PROFICIENCY, 0, true);

    // Unique skills: SIDE = 0 (left), 1 (right)
    HANDLE_UNIQUE_U("Master Artisan", "Master Artisan",   UNQ_SMT_FEANOR,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Creator of Galvorn", "Creator of Galvorn",   UNQ_SMT_EOL,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("One Handed", "One Handed",   UNQ_MEL_MAEDHROS,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Agarwaen", "Agarwaen",   UNQ_WIL_TURIN,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Shadow Walker", "Shadow Walker",   UNQ_SNG_TURGON,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Chosen of Ulmo", "Chosen of Ulmo",   UNQ_WIL_TUOR, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Indomitable Will", "Indomitable Will",   UNQ_EARENDIL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Oromë Himself", "Himself",   UNQ_WIL_FIN, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Songs of Power", "Songs of Power",   UNQ_SNG_FIN, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Elven Dance", "Elven Dance",   UNQ_SNG_LUT, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Girdle of Melian", "Girdle of Melian",   UNQ_SNG_MEL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Creator of Angrist", "Creator of Angrist",   UNQ_SMT_TELCHAR, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Old Master", "Old Master",   UNQ_SMT_GAMIL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Ring Master", "Ring Master",   UNQ_SMT_CELEBRIMBOR, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Aure entuluva", "Aure entuluva",   UNQ_SNG_HURIN, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Voice of the Girdle", "Voice of the Girdle",   UNQ_SNG_THINGOL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Forgotten", "Forgotten",   UNQ_MIM, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Minstrel", "Minstrel",   UNQ_MINSTREL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Woven Master", "Woven Master",   UNQ_WOVEN_MASTER, TERM_VIOLET,   1);

    HANDLE_UNIQUE("Gift of Eru", "Gift of Eru",   RHF_GIFTERU,     TERM_VIOLET,     1);
    HANDLE_UNIQUE("Seafarer", "Seafarer",   RHF_FREE, TERM_VIOLET,   1);

    HANDLE_UNIQUE("Kinslayer", "Kinslayer",   RHF_KINSLAYER, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Treacherous", "Treacherous",   RHF_TREACHERY, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Doom of Mandos", "Doom of Mandos",   RHF_CURSE, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Morgoth Curse", "Morgoth Curse",   RHF_MOR_CURSE, TERM_UMBER,   1); // right

    if (compact_layout)
    {
        char line_buf[64];
        birth_compact_flag_line compact_lines[64];
        birth_compact_flag_line short_trait_lines[64];
        int compact_line_n = 0;
        int compact_max_line_len = 0;
        int short_trait_n = 0;
        int short_trait_max_line_len = 0;
        int prompt_row = birth_prompt_row();
        bool tight_height = character_selection_tight_height();
        bool use_swapped_layout = false;

        compact_line_n = collect_character_trait_lines(race, character, false,
            compact_lines, N_ELEMENTS(compact_lines), &compact_max_line_len);
        short_trait_n = collect_character_trait_lines(race, character, true,
            short_trait_lines, N_ELEMENTS(short_trait_lines), &short_trait_max_line_len);

        if (tight_height)
        {
            const int target_traits = (short_trait_n < 9) ? short_trait_n : 9;
            const int min_upper_two_col_wid = 12;
            int upper_rows_total = description_row - row;
            int lower_rows_total = prompt_row - description_row;
            int upper_col = col;
            int upper_width = term_wid - upper_col;
            int upper_col_gap = 2;
            int upper_col_wid = (upper_width - upper_col_gap) / 2;
            int lower_width = term_wid - 2;
            int target_ability_rows;
            bool show_trait_heading = true;
            bool show_ability_heading = true;
            bool use_upper_two_columns = false;
            bool traits_fit = false;
            bool abilities_fit = false;

            if (upper_rows_total < 0)
                upper_rows_total = 0;
            if (lower_rows_total < 0)
                lower_rows_total = 0;
            if (upper_col_wid < 0)
                upper_col_wid = 0;
            if (lower_width < 1)
                lower_width = 1;

            target_ability_rows = birth_wrapped_entry_lines(ability_lines,
                ability_line_n, lower_width, 3);

            if (target_ability_rows == 0)
            {
                show_ability_heading = false;
                abilities_fit = true;
            }
            else if ((lower_rows_total - 1) >= target_ability_rows)
            {
                show_ability_heading = true;
                abilities_fit = true;
            }
            else if (lower_rows_total >= target_ability_rows)
            {
                show_ability_heading = false;
                abilities_fit = true;
            }

            if (target_traits == 0)
            {
                show_trait_heading = false;
                traits_fit = true;
            }
            else if (upper_col_wid >= min_upper_two_col_wid)
            {
                if ((upper_rows_total - 1) > 0 && ((upper_rows_total - 1) * 2 >= target_traits))
                {
                    show_trait_heading = true;
                    use_upper_two_columns = true;
                    traits_fit = true;
                }
                else if (upper_rows_total > 0 && (upper_rows_total * 2 >= target_traits))
                {
                    show_trait_heading = false;
                    use_upper_two_columns = true;
                    traits_fit = true;
                }
            }

            if (!traits_fit && upper_width >= short_trait_max_line_len)
            {
                if ((upper_rows_total - 1) >= target_traits)
                {
                    show_trait_heading = true;
                    use_upper_two_columns = false;
                    traits_fit = true;
                }
                else if (upper_rows_total >= target_traits)
                {
                    show_trait_heading = false;
                    use_upper_two_columns = false;
                    traits_fit = true;
                }
            }

            use_swapped_layout = traits_fit && abilities_fit;

            if (use_swapped_layout)
            {
                int traits_row = row;
                int ability_row = description_row;

                if (show_trait_heading && short_trait_n > 0)
                    Term_putstr(upper_col, traits_row++, -1, TERM_L_BLUE, "Traits:");

                if (use_upper_two_columns)
                {
                    int col2 = upper_col + upper_col_wid + upper_col_gap;
                    int rows_per_col = description_row - traits_row;
                    int draw_lines;
                    int left_count;
                    int right_count;

                    if (rows_per_col < 0)
                        rows_per_col = 0;
                    draw_lines = (short_trait_n < rows_per_col * 2)
                        ? short_trait_n : rows_per_col * 2;
                    left_count = (draw_lines + 1) / 2;
                    if (left_count > rows_per_col)
                        left_count = rows_per_col;
                    right_count = draw_lines - left_count;
                    if (right_count > rows_per_col)
                        right_count = rows_per_col;
                    left_count = draw_lines - right_count;

                    for (int i = 0; i < left_count; ++i)
                    {
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", upper_col_wid,
                            upper_col_wid, short_trait_lines[i].txt);
                        Term_putstr(upper_col, traits_row + i, -1,
                            short_trait_lines[i].attr, line_buf);
                        birth_detail_hover_add_trait(upper_col,
                            traits_row + i, &short_trait_lines[i]);
                    }

                    for (int i = 0; i < right_count; ++i)
                    {
                        int idx = left_count + i;
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", upper_col_wid,
                            upper_col_wid, short_trait_lines[idx].txt);
                        Term_putstr(col2, traits_row + i, -1,
                            short_trait_lines[idx].attr, line_buf);
                        birth_detail_hover_add_trait(col2, traits_row + i,
                            &short_trait_lines[idx]);
                    }
                }
                else
                {
                    int rows = description_row - traits_row;
                    int draw_lines;

                    if (rows < 0)
                        rows = 0;
                    draw_lines = (short_trait_n < rows) ? short_trait_n : rows;

                    for (int i = 0; i < draw_lines; ++i)
                    {
                        Term_putstr(upper_col, traits_row + i, -1,
                            short_trait_lines[i].attr, short_trait_lines[i].txt);
                        birth_detail_hover_add_trait(upper_col,
                            traits_row + i, &short_trait_lines[i]);
                    }
                }

                if (show_ability_heading && ability_line_n > 0)
                    Term_putstr(2, ability_row++, -1, TERM_L_BLUE, "Abilities:");

                {
                    int rows = prompt_row - ability_row;
                    int ability_width = term_wid - 2;

                    if (rows < 0)
                        rows = 0;
                    if (ability_width < 1)
                        ability_width = 1;

                    birth_put_wrapped_entries(TERM_YELLOW, ability_lines,
                        ability_line_n, ability_row, 2, ability_width, rows,
                        ability_line_n);
                }
            }
        }

        if (!use_swapped_layout)
        {
            int compact_row = description_row;
            int compact_col = 2;
            int col_gap = 2;
            int col_wid;
            int ability_width = term_wid - col;
            int ability_rows = description_row - row;
            bool use_two_columns = false;
            int right_offset = 0; /* 0=normal, -1=right column starts on title row, -2=one row above */

            if (ability_width < 1)
                ability_width = 1;
            if (ability_rows < 0)
                ability_rows = 0;

            birth_put_wrapped_entries(TERM_YELLOW, ability_lines, ability_line_n,
                row, col, ability_width, ability_rows, ability_line_n);

            col_wid = (term_wid - compact_col - col_gap) / 2;
            if (col_wid < 1)
                col_wid = 1;

            if (col_wid >= compact_max_line_len)
                use_two_columns = true;

            {
                const bool short_screen = (Term->hgt > 0) && (Term->hgt < 24);
                const int target_limit = tight_height ? 9 : 10;
                const int target_traits = (compact_line_n < target_limit) ? compact_line_n : target_limit;
                const int min_col_wid_for_forced_two_cols = 14;

#define MAX0(v) ((v) > 0 ? (v) : 0)
#define CAPACITY_ONE(_row) (MAX0(Term->hgt - ((_row) + 1) - 1))
#define CAPACITY_TWO(_row, _roff) \
    (MAX0(Term->hgt - ((_row) + 1) - 1) + MAX0(Term->hgt - ((_row) + 1 + (_roff)) - 1))

                int base_capacity = use_two_columns ? CAPACITY_TWO(compact_row, right_offset)
                                                   : CAPACITY_ONE(compact_row);

                if (short_screen && (base_capacity < target_traits))
                {
                    if (compact_row > 0)
                        compact_row = description_row - 1;

                    base_capacity = use_two_columns ? CAPACITY_TWO(compact_row, right_offset)
                                                   : CAPACITY_ONE(compact_row);

                    if ((base_capacity < target_traits) && !use_two_columns
                        && (col_wid >= min_col_wid_for_forced_two_cols))
                    {
                        use_two_columns = true;
                        base_capacity = CAPACITY_TWO(compact_row, right_offset);
                    }

                    if ((base_capacity < target_traits) && use_two_columns)
                    {
                        right_offset = -1;
                        base_capacity = CAPACITY_TWO(compact_row, right_offset);
                    }

                    if ((base_capacity < target_traits) && use_two_columns && (compact_row > 0))
                    {
                        right_offset = -2;
                        base_capacity = CAPACITY_TWO(compact_row, right_offset);
                    }
                }

#undef CAPACITY_TWO
#undef CAPACITY_ONE
#undef MAX0
            }

            {
                int compact_available = Term->hgt - compact_row - 1;

                if ((compact_available > 0) && (Term->hgt > 0))
                    Term_putstr(compact_col, compact_row, -1, TERM_L_BLUE, "Character traits:");

                if (use_two_columns)
                {
                    int col2 = compact_col + col_wid + col_gap;
                    int left_start = compact_row + 1;
                    int right_start = compact_row + 1 + right_offset;
                    int left_rows = Term->hgt - left_start - 1;
                    int right_rows = Term->hgt - right_start - 1;
                    int max_lines;
                    int draw_lines;
                    int left_count;
                    int right_count;

                    if (left_rows < 0) left_rows = 0;
                    if (right_rows < 0) right_rows = 0;

                    max_lines = left_rows + right_rows;
                    draw_lines = (compact_line_n < max_lines) ? compact_line_n : max_lines;

                    left_count = (draw_lines + 1) / 2;
                    if (left_count > left_rows) left_count = left_rows;
                    right_count = draw_lines - left_count;
                    if (right_count > right_rows) right_count = right_rows;
                    if (left_count > (draw_lines - right_count))
                        left_count = draw_lines - right_count;
                    draw_lines = left_count + right_count;

                    for (int i = 0; i < left_count; ++i)
                    {
                        int y = left_start + i;
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", col_wid, col_wid,
                            compact_lines[i].txt);
                        Term_putstr(compact_col, y, -1, compact_lines[i].attr, line_buf);
                        birth_detail_hover_add_trait(compact_col, y,
                            &compact_lines[i]);
                    }

                    for (int i = 0; i < right_count; ++i)
                    {
                        int idx = left_count + i;
                        int y = right_start + i;
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", col_wid, col_wid,
                            compact_lines[idx].txt);
                        Term_putstr(col2, y, -1, compact_lines[idx].attr, line_buf);
                        birth_detail_hover_add_trait(col2, y,
                            &compact_lines[idx]);
                    }
                }
                else
                {
                    int start_row = compact_row + 1;
                    int rows = Term->hgt - start_row - 1;
                    int draw_lines;

                    if (rows < 0)
                        rows = 0;
                    draw_lines = (compact_line_n < rows) ? compact_line_n : rows;

                    for (int i = 0; i < draw_lines; ++i)
                    {
                        Term_putstr(compact_col, start_row + i, -1,
                            compact_lines[i].attr, compact_lines[i].txt);
                        birth_detail_hover_add_trait(compact_col,
                            start_row + i, &compact_lines[i]);
                    }
                }
            }
        }
    }
    else
    {
        // Left column
        for (int i = 0; i < unique_n; ++i)
            if (unique_buf[i].side == 0)
            {
                int y = row + flags_left++;
                Term_putstr(col, y, -1, unique_buf[i].attr, unique_buf[i].txt);
                birth_detail_hover_add_trait(col, y, &unique_buf[i]);
            }
        for (int i = 0; i < mastery_n;  ++i)
        {
            int y = row + flags_left++;
            Term_putstr(col, y, -1, mastery_buf[i].attr, mastery_buf[i].txt);
            birth_detail_hover_add_trait(col, y, &mastery_buf[i]);
        }
        for (int i = 0; i < affinity_n; ++i)
        {
            int y = row + flags_left++;
            Term_putstr(col, y, -1, affinity_buf[i].attr, affinity_buf[i].txt);
            birth_detail_hover_add_trait(col, y, &affinity_buf[i]);
        }

        // Right column
        for (int i = 0; i < unique_n; ++i)
            if (unique_buf[i].side == 1)
            {
                int y = row + flags_right++;
                Term_putstr(col_pen, y, -1, unique_buf[i].attr, unique_buf[i].txt);
                birth_detail_hover_add_trait(col_pen, y, &unique_buf[i]);
            }
        for (int i = 0; i < penalty_n; ++i)
        {
            int y = row + flags_right++;
            Term_putstr(col_pen, y, -1, penalty_buf[i].attr, penalty_buf[i].txt);
            birth_detail_hover_add_trait(col_pen, y, &penalty_buf[i]);
        }
    }

#undef HANDLE_SKILL_EX
#undef HANDLE_UNIQUE
#undef HANDLE_UNIQUE_U
#undef BIRTH_PUSH_TRAIT

if (!compact_layout)
{
    Term_erase(col +7, row - 5, 30);


/* Display starting abilities */
    if (ability_line_n > 0)
    {
        const int x     = col + 7;
        const int y0    = row - 5;
        const int width = 30;   /* how many cols to clear */

        /* 1) clear out every possible line first */
        for (int i = 0; i < CHARACTER_ABILITY_MAX - 3; i++)
        {
            Term_erase(x, y0 + i, width);
        }

        /* 2) now draw the actual list */
        int y = y0;
        int max_lines = CHARACTER_ABILITY_MAX - 3;
        if (ability_line_n < max_lines)
            max_lines = ability_line_n;

        for (int slot = 0; slot < max_lines; slot++)
        {
            char desc[640];

            Term_putstr(x, y, -1, TERM_YELLOW, ability_lines[slot]);
            birth_format_ability_hint(ability_skills[slot], ability_ids[slot],
                desc, sizeof(desc));
            if (desc[0])
                birth_detail_hover_add(x, y, (int)strlen(ability_lines[slot]),
                    desc);
            y++;
        }
    }
}
}

