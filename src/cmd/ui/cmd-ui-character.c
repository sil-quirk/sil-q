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

    if (g_banner_force_redraw_remaining <= 0)
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

typedef struct {
    cptr label;
    character_sheet_value_kind kind;
    cptr desc;
    bool skip_skill_rows;
    bool require_digit_after_label;
    int width;
} character_sheet_value_desc;

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

typedef struct {
    cptr label;
    int skill;
    u32b aff_flag;
    u32b pen_flag;
    bool proficiency;
} character_sheet_skill_trait;

static const character_sheet_skill_trait character_sheet_skill_traits[] = {
    { "melee", S_MEL, RHF_MEL_AFFINITY, RHF_MEL_PENALTY, false },
    { "evasion", S_EVN, RHF_EVN_AFFINITY, RHF_EVN_PENALTY, false },
    { "stealth", S_STL, RHF_STL_AFFINITY, RHF_STL_PENALTY, false },
    { "archery", S_ARC, RHF_ARC_AFFINITY, RHF_ARC_PENALTY, false },
    { "will", S_WIL, RHF_WIL_AFFINITY, RHF_WIL_PENALTY, false },
    { "perception", S_PER, RHF_PER_AFFINITY, RHF_PER_PENALTY, false },
    { "smithing", S_SMT, RHF_SMT_AFFINITY, RHF_SMT_PENALTY, false },
    { "song", S_SNG, RHF_SNG_AFFINITY, RHF_SNG_PENALTY, false },
    { "bow", S_ARC, RHF_BOW_PROFICIENCY, 0, true },
    { "axe", S_MEL, RHF_AXE_PROFICIENCY, 0, true },
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

static const character_sheet_value_desc character_sheet_value_descriptions[] = {
    { "Melee x2", CHARACTER_SHEET_VALUE_MELEE_X2,
        "Extra rapid-attack swing: (attack bonus, damage dice).", false, true, 20 },
    { "Deep Call", CHARACTER_SHEET_VALUE_DEEP_CALL,
        "Deep Call: pressure on the minimum-depth timer; higher values mean the call below is growing stronger.", false, true, 20 },
    { "Depth c/m", CHARACTER_SHEET_VALUE_DEPTH,
        "Depth c/m: current depth / minimum return depth.", false, true, 20 },
    { "Burden", CHARACTER_SHEET_VALUE_BURDEN,
        "Burden: carried weight / maximum carrying capacity.", false, true, 20 },
    { "Offhand", CHARACTER_SHEET_VALUE_OFFHAND,
        "Offhand attack: (attack bonus, damage dice) for the weapon in your other hand.", false, true, 20 },
    { "Health", CHARACTER_SHEET_VALUE_HEALTH,
        "Health: current / maximum hit points.", false, true, 20 },
    { "Voice", CHARACTER_SHEET_VALUE_VOICE,
        "Voice: current / maximum song points.", false, true, 20 },
    { "Armor", CHARACTER_SHEET_VALUE_ARMOR,
        "Armor: [evasion, protection] shows hit-avoid chance and damage absorption.", false, true, 20 },
    { "Melee", CHARACTER_SHEET_VALUE_MELEE,
        "Main-hand melee: (attack bonus, damage dice).", true, true, 20 },
    { "Bows", CHARACTER_SHEET_VALUE_BOWS,
        "Ranged attacks: (attack bonus, damage dice).", false, true, 20 },
    { "Exp", CHARACTER_SHEET_VALUE_EXP,
        "Experience: unspent / total XP.", false, true, 20 },
    { "Depth", CHARACTER_SHEET_VALUE_DEPTH,
        "Depth: current depth / minimum return depth.", false, true, 20 },
    { "Turn", CHARACTER_SHEET_VALUE_TURN,
        "Turn: total game turns elapsed.", false, true, 20 },
    { "Light", CHARACTER_SHEET_VALUE_LIGHT,
        "Light: current light radius.", false, true, 20 },
    { "Song", CHARACTER_SHEET_VALUE_SONG,
        "Song: currently active song.", true, false, 20 },
    { "Str", CHARACTER_SHEET_VALUE_STR,
        "Strength: melee damage dice and weight capacity.", false, true, 24 },
    { "Dex", CHARACTER_SHEET_VALUE_DEX,
        "Dexterity: melee, evasion, archery, and stealth.", false, true, 24 },
    { "Con", CHARACTER_SHEET_VALUE_CON,
        "Constitution: maximum health.", false, true, 24 },
    { "Gra", CHARACTER_SHEET_VALUE_GRA,
        "Grace: will, perception, smithing, song, and voice.", false, true, 24 },
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

static void character_sheet_fit_prompt_text(int col, int wid, cptr text,
    char* out, size_t outsz);

static char character_sheet_screen_char(int row, int col)
{
    unsigned char ch;

    if (!Term || !Term->scr || !Term->scr->c)
        return ' ';
    if (row < 0 || row >= Term->hgt || col < 0 || col >= Term->wid)
        return ' ';

    ch = (unsigned char)Term->scr->c[row][col];
    if (!ch || ch == (unsigned char)Term->char_blank)
        return ' ';

    return (char)ch;
}

static bool character_sheet_screen_text_matches(int row, int col, cptr text,
    int len)
{
    if (!text || len <= 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        if (character_sheet_screen_char(row, col + i) != text[i])
            return false;
    }

    return true;
}

static bool character_sheet_screen_row_has_skill_value(int row, int start_col,
    int end_col)
{
    int wid = Term ? Term->wid : 0;

    if (end_col <= start_col || end_col > wid)
        end_col = wid;

    for (int col = start_col; col < end_col; col++)
    {
        if (character_sheet_screen_char(row, col) == '=')
            return true;
    }

    return false;
}

static bool character_sheet_is_word_char(char ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

static bool character_sheet_text_boundary_ok(int row, int col, int len,
    int score)
{
    char prev;
    char next;

    if (len <= 0)
        return false;

    prev = character_sheet_screen_char(row, col - 1);
    next = character_sheet_screen_char(row, col + len);

    if (character_sheet_is_word_char(prev)
        || character_sheet_is_word_char(next))
    {
        return false;
    }

    if (score == 1 && next == '+')
        return false;
    if (score == -1 && next == '-')
        return false;

    return true;
}

static bool character_sheet_add_item(character_sheet_item items[], int* count,
    int max_items, character_sheet_item item)
{
    int wid = 80;

    if (!items || !count || *count >= max_items)
        return false;

    for (int i = 0; i < *count; i++)
    {
        if (items[i].row == item.row && items[i].col == item.col
            && items[i].kind == item.kind)
        {
            return false;
        }
    }

    Term_get_size(&wid, NULL);
    if (wid < 1)
        wid = 80;

    if (item.col < 0)
        item.col = 0;
    if (item.col >= wid)
        return false;
    if (item.width <= 0 || item.col + item.width > wid)
        item.width = wid - item.col;
    if (item.width <= 0)
        return false;

    ui_menu_click_add(CHARACTER_SHEET_CLICK_ITEM_BASE + *count, item.col,
        item.row, item.width);
    items[*count] = item;
    if (items[*count].label_buf[0])
        items[*count].label = items[*count].label_buf;
    (*count)++;
    return true;
}

static bool character_sheet_find_text(cptr text, int* out_row, int* out_col,
    int* out_len, int score)
{
    int wid = 80;
    int hgt = 24;
    int len;

    if (!Term || !Term->scr || !Term->scr->c || !text || !text[0])
        return false;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    len = (int)strlen(text);
    if (len <= 0 || len > wid)
        return false;

    for (int row = 0; row < hgt - 1; row++)
    {
        for (int col = 0; col <= wid - len; col++)
        {
            if (!character_sheet_screen_text_matches(row, col, text, len))
                continue;
            if (!character_sheet_text_boundary_ok(row, col, len, score))
                continue;

            if (out_row)
                *out_row = row;
            if (out_col)
                *out_col = col;
            if (out_len)
                *out_len = len;
            return true;
        }
    }

    return false;
}

static bool character_sheet_row_has_digit_after(int row, int col)
{
    int wid = Term ? Term->wid : 0;

    for (int i = MAX(0, col); i < wid; i++)
    {
        if (isdigit((unsigned char)character_sheet_screen_char(row, i)))
            return true;
    }

    return false;
}

static bool character_sheet_row_has_nonspace_after(int row, int col)
{
    int wid = Term ? Term->wid : 0;

    for (int i = MAX(0, col); i < wid; i++)
    {
        if (character_sheet_screen_char(row, i) != ' ')
            return true;
    }

    return false;
}

static void character_sheet_collect_progress_bar_items(
    character_sheet_item items[], int* count, int max_items)
{
    int wid = 80;
    int hgt = 24;

    if (!Term || !Term->scr || !Term->scr->c)
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int row = 0; row < hgt - 1; row++)
    {
        for (int col = 0; col < wid; col++)
        {
            int end = col + 1;
            character_sheet_item item;

            if (character_sheet_screen_char(row, col) != '[')
                continue;

            while (end < wid)
            {
                char ch = character_sheet_screen_char(row, end);

                if (ch == ']')
                    break;
                if (ch != '#' && ch != '.')
                    break;
                end++;
            }

            if (end >= wid || character_sheet_screen_char(row, end) != ']')
                continue;
            if (end - col < 8)
                continue;

            SDL_zero(item);
            item.kind = CHARACTER_SHEET_ITEM_VALUE;
            item.id = CHARACTER_SHEET_VALUE_DEPTH_PROGRESS;
            item.value_kind = CHARACTER_SHEET_VALUE_DEPTH_PROGRESS;
            item.row = row;
            item.col = MAX(0, col - 1);
            item.width = MIN(end - col + 3, wid - item.col);
            item.label = "Minimum depth progress";
            item.desc = "Progress toward the next minimum-depth increase.";
            character_sheet_add_item(items, count, max_items, item);
            return;
        }
    }
}

static int character_sheet_collect_value_items(character_sheet_item items[],
    int* count, int max_items)
{
    int wid = 80;
    int hgt = 24;

    if (!Term || !Term->scr || !Term->scr->c)
        return 0;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (size_t i = 0; i < N_ELEMENTS(character_sheet_value_descriptions); i++)
    {
        const character_sheet_value_desc* desc =
            &character_sheet_value_descriptions[i];
        int len;

        if (!desc->label || !desc->label[0])
            continue;

        len = (int)strlen(desc->label);
        if (len <= 0 || len > wid)
            continue;

        for (int row = 0; row < hgt - 1; row++)
        {
            for (int col = 0; col <= wid - len; col++)
            {
                character_sheet_item item;
                int width;

                if (!character_sheet_screen_text_matches(row, col,
                        desc->label, len))
                {
                    continue;
                }
                if (!character_sheet_text_boundary_ok(row, col, len, 0))
                    continue;
                if (desc->skip_skill_rows
                    && character_sheet_screen_row_has_skill_value(row,
                        col + len, col + 32))
                {
                    continue;
                }
                if (desc->require_digit_after_label
                    && !character_sheet_row_has_digit_after(row, col + len))
                {
                    continue;
                }
                if (!desc->require_digit_after_label
                    && !character_sheet_row_has_nonspace_after(row, col + len))
                {
                    continue;
                }

                SDL_zero(item);
                width = desc->width;
                if (width < len + 2)
                    width = len + 2;

                item.kind = CHARACTER_SHEET_ITEM_VALUE;
                item.id = (int)desc->kind;
                item.value_kind = (int)desc->kind;
                item.row = row;
                item.col = MAX(0, col - 1);
                item.width = MIN(width, wid - item.col);
                item.label = desc->label;
                item.desc = desc->desc;
                character_sheet_add_item(items, count, max_items, item);
            }
        }
    }

    character_sheet_collect_progress_bar_items(items, count, max_items);
    return *count;
}

static int character_sheet_collect_skill_items(character_sheet_item items[],
    int* count, int max_items)
{
    int wid = 80;
    int hgt = 24;

    if (!Term || !Term->scr || !Term->scr->c)
        return 0;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        cptr name;
        int name_len;
        int match_len;
        bool found = false;

        if (skill == S_SPC)
            continue;

        name = skill_names_full[skill];
        if (!name || !name[0])
            continue;

        name_len = (int)strlen(name);
        match_len = name_len;
        if (match_len > 5)
            match_len = 5;
        if (match_len < 4)
            match_len = name_len;

        for (int row = 0; row < hgt - 1 && !found; row++)
        {
            for (int col = 0; col <= wid - match_len; col++)
            {
                int start_col;

                if (!character_sheet_screen_text_matches(row, col, name,
                    name_len)
                    && !character_sheet_screen_text_matches(row, col, name,
                        match_len))
                {
                    continue;
                }

                if (!character_sheet_screen_row_has_skill_value(row,
                    col + match_len, col + 32))
                {
                    continue;
                }

                start_col = MAX(0, col - 1);
                character_sheet_item item;

                SDL_zero(item);
                item.kind = CHARACTER_SHEET_ITEM_SKILL;
                item.id = skill;
                item.skill = skill;
                item.row = row;
                item.col = start_col;
                item.width = MIN(28, wid - start_col);
                item.label = name;
                character_sheet_add_item(items, count, max_items, item);
                found = true;
                break;
            }
        }
    }

    return *count;
}

static int character_sheet_collect_trait_items(character_sheet_item items[],
    int* count, int max_items)
{
    int wid = 80;

    Term_get_size(&wid, NULL);
    if (wid < 1)
        wid = 80;

    for (size_t i = 0; i < N_ELEMENTS(character_sheet_named_traits); i++)
    {
        int row;
        int col;
        int len;
        character_sheet_item item;

        if (!character_sheet_find_text(character_sheet_named_traits[i].label,
                &row, &col, &len, 0))
        {
            continue;
        }

        SDL_zero(item);
        item.kind = CHARACTER_SHEET_ITEM_TRAIT;
        item.id = (int)i;
        item.row = row;
        item.col = MAX(0, col - 1);
        item.width = MIN(len + 3, wid - item.col);
        item.label = character_sheet_named_traits[i].label;
        item.desc = character_sheet_named_traits[i].desc;
        character_sheet_add_item(items, count, max_items, item);
    }

    for (size_t i = 0; i < N_ELEMENTS(character_sheet_skill_traits); i++)
    {
        static const int scores[] = { 2, -2, 1, -1 };

        for (size_t j = 0; j < N_ELEMENTS(scores); j++)
        {
            int score = scores[j];
            int row;
            int col;
            int len;
            character_sheet_item item;
            char label[32];

            if (score == 2)
                strnfmt(label, sizeof(label), "%s++",
                    character_sheet_skill_traits[i].label);
            else if (score == 1)
                strnfmt(label, sizeof(label), "%s+",
                    character_sheet_skill_traits[i].label);
            else if (score == -1)
                strnfmt(label, sizeof(label), "%s-",
                    character_sheet_skill_traits[i].label);
            else
                strnfmt(label, sizeof(label), "%s--",
                    character_sheet_skill_traits[i].label);

            if (!character_sheet_find_text(label, &row, &col, &len, score))
                continue;

            SDL_zero(item);
            item.kind = CHARACTER_SHEET_ITEM_TRAIT;
            item.id = 1000 + (int)(i * 10) + (score + 2);
            item.row = row;
            item.col = MAX(0, col - 1);
            item.width = MIN(len + 3, wid - item.col);
            item.skill = character_sheet_skill_traits[i].skill;
            item.trait_score = score;
            item.proficiency = character_sheet_skill_traits[i].proficiency;
            item.aff_flag = character_sheet_skill_traits[i].aff_flag;
            item.pen_flag = character_sheet_skill_traits[i].pen_flag;
            item.label = item.label_buf;
            SDL_strlcpy(item.label_buf, label, sizeof(item.label_buf));
            character_sheet_add_item(items, count, max_items, item);
        }
    }

    return *count;
}

static int character_sheet_collect_items(character_sheet_item items[],
    int max_items)
{
    int count = 0;

    character_sheet_collect_value_items(items, &count, max_items);
    character_sheet_collect_skill_items(items, &count, max_items);
    character_sheet_collect_trait_items(items, &count, max_items);

    return count;
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

static void character_sheet_highlight_item(const character_sheet_item* item)
{
    cptr label;
    int len;
    bool story;

    if (!item || !item->label)
        return;

    label = item->label;
    len = (int)strlen(label);
    if (len <= 0)
        return;

    story = story_character_enabled();
    if (story)
        sdl_story_font_enable();
    Term_putstr(item->col
            + ((item->col > 0
                && character_sheet_screen_char(item->row, item->col) == ' ')
                    ? 1 : 0),
        item->row, len, TERM_L_BLUE, label);
    if (story)
        sdl_story_font_disable();
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
            "Experience: %ld unspent / %ld total. Spend XP to raise skills and buy abilities.",
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
            "Burden: %s/%s lb carried. Heavy loads count against movement and stealth.",
            carried, limit);
        break;
    }
    case CHARACTER_SHEET_VALUE_DEPTH:
        strnfmt(buf, buflen,
            "Depth c/m: %d ft current / %d ft minimum return depth.",
            p_ptr->depth * 50, min_depth() * 50);
        break;
    case CHARACTER_SHEET_VALUE_DEEP_CALL:
    {
        int total = 0;
        int progress = 0;
        int threshold = 0;

        min_depth_timer_status(NULL, NULL, &total, &progress, &threshold);
        if (threshold > 0)
        {
            strnfmt(buf, buflen,
                "Deep Call: minimum-depth pressure. Progress %d/%d; currently advances by %d per turn.",
                progress, threshold, total);
        }
        else
        {
            SDL_strlcpy(buf,
                "Deep Call: minimum-depth pressure from time and certain items.",
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
            "Minimum-depth progress: %d/%d toward the next rise in minimum return depth.",
            progress, threshold);
        break;
    }
    case CHARACTER_SHEET_VALUE_TURN:
    {
        char turns[32];
        comma_number(turns, playerturn);
        strnfmt(buf, buflen, "Turn: %s game turns elapsed.", turns);
        break;
    }
    case CHARACTER_SHEET_VALUE_LIGHT:
        strnfmt(buf, buflen, "Light: radius %d around you.", p_ptr->cur_light);
        break;
    case CHARACTER_SHEET_VALUE_MELEE:
        strnfmt(buf, buflen,
            "Melee: main hand (%+d,%dd%d): attack bonus and damage dice.",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        break;
    case CHARACTER_SHEET_VALUE_MELEE_X2:
        strnfmt(buf, buflen,
            "Melee x2: Rapid Attack grants another swing at (%+d,%dd%d).",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        break;
    case CHARACTER_SHEET_VALUE_OFFHAND:
        strnfmt(buf, buflen,
            "Offhand: secondary attack (%+d,%dd%d).",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
            p_ptr->mdd2, p_ptr->mds2);
        break;
    case CHARACTER_SHEET_VALUE_BOWS:
        strnfmt(buf, buflen,
            "Bows: ranged attacks (%+d,%dd%d): attack bonus and damage dice.",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
        break;
    case CHARACTER_SHEET_VALUE_ARMOR:
        strnfmt(buf, buflen,
            "Armor: [%+d,%d-%d] = evasion bonus and protection dice.",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true),
            p_max(GF_HURT, true));
        break;
    case CHARACTER_SHEET_VALUE_HEALTH:
        strnfmt(buf, buflen, "Health: %d/%d hit points.",
            MIN(p_ptr->chp, 999), MIN(p_ptr->mhp, 999));
        break;
    case CHARACTER_SHEET_VALUE_VOICE:
        strnfmt(buf, buflen, "Voice: %d/%d song points.",
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
        if (song1 && song2)
            strnfmt(buf, buflen, "Song: currently singing %s and %s.",
                song1, song2);
        else if (song1)
            strnfmt(buf, buflen, "Song: currently singing %s.", song1);
        else if (song2)
            strnfmt(buf, buflen, "Song: currently singing %s.", song2);
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

static void character_sheet_show_item_tooltip(const character_sheet_item* item)
{
    char desc[640];

    character_sheet_format_item_description(item, desc, sizeof(desc));

    if (!desc[0])
        return;

    (void)sdl_hover_tooltip_show_text(item->col, item->row, item->width,
        desc, false);
}

static void character_sheet_fit_prompt_text(int col, int wid, cptr text,
    char* out, size_t outsz)
{
    int max_len;

    if (!out || !outsz)
        return;

    out[0] = '\0';

    if (!text)
        return;

    if (wid < 1)
        wid = 80;

    max_len = wid - col - 1;
    if (max_len < 1)
        return;

    SDL_strlcpy(out, text, outsz);
    if ((int)strlen(out) > max_len)
        out[max_len] = '\0';
}

static void character_sheet_put_prompt_fit(int col, int row, int wid, byte attr,
    cptr text)
{
    char buf[256];

    character_sheet_fit_prompt_text(col, wid, text, buf, sizeof(buf));
    if (!buf[0])
        return;

    Term_putstr(col, row, -1, attr, buf);
}

static bool character_sheet_prompt_append(char* buf, size_t buflen, cptr token, int max_width)
{
    size_t cur_len;
    size_t tok_len;
    int sep = 0;

    if (!buf || !buflen || !token || !token[0])
        return true;

    cur_len = strlen(buf);
    tok_len = strlen(token);
    if (cur_len > 0)
        sep = 2;

    if ((int)(cur_len + sep + tok_len) > max_width)
        return false;

    if (sep)
        SDL_strlcat(buf, "  ", buflen);
    SDL_strlcat(buf, token, buflen);
    return true;
}

static void character_sheet_build_prompt(bool steamdeck, bool include_curses,
    int wid, char* out, size_t outsz)
{
    int max_width;

    if (!out || !outsz)
        return;

    out[0] = '\0';

    if (wid < 1)
        wid = 80;

    max_width = wid - 2;
    if (max_width < 1)
        return;

    if (steamdeck)
    {
        char notes_label[16], story_label[16], file_label[16];
        char abilities_label[16], increase_label[16], help_label[16], back_label[16];
        char token[7][64];

        controller_prompt_label('n', "n", notes_label, sizeof(notes_label));
        controller_prompt_label(steamdeck_secondary_key(), "Y", story_label, sizeof(story_label));
        controller_prompt_label('e', "L1", file_label, sizeof(file_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X", abilities_label, sizeof(abilities_label));
        controller_prompt_label(steamdeck_confirm_key(), "A", increase_label, sizeof(increase_label));
        controller_prompt_label(steamdeck_info_key(), "RS", help_label, sizeof(help_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

        strnfmt(token[0], sizeof(token[0]), "%s abilities", abilities_label);
        strnfmt(token[1], sizeof(token[1]), "%s increase", increase_label);
        strnfmt(token[2], sizeof(token[2]), "%s help", help_label);
        strnfmt(token[3], sizeof(token[3]), "%s back", back_label);
        strnfmt(token[4], sizeof(token[4]), "%s notes", notes_label);
        strnfmt(token[5], sizeof(token[5]), "%s story", story_label);
        strnfmt(token[6], sizeof(token[6]), "%s file", file_label);

        for (int i = 0; i < 4; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
        for (int i = 4; i < 7; i++)
            (void)character_sheet_prompt_append(out, outsz, token[i], max_width);
    }
    else
    {
        const char* essential[] = {
            "a abilities", "Space/i increase", "? help", "ESC back"
        };
        const char* optional[] = {
            "n notes", "s story", "f file"
        };

        for (int i = 0; i < (int)(sizeof(essential) / sizeof(essential[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, essential[i], max_width);

        if (include_curses)
            (void)character_sheet_prompt_append(out, outsz, "c curses", max_width);

        for (int i = 0; i < (int)(sizeof(optional) / sizeof(optional[0])); i++)
            (void)character_sheet_prompt_append(out, outsz, optional[i], max_width);
    }

    if (!out[0])
    {
        if (steamdeck)
            SDL_strlcpy(out, "B back", outsz);
        else
            SDL_strlcpy(out, "ESC back", outsz);
    }
}

static void character_sheet_draw_page_indicator(int sheet_page, int compact_pages,
    int wid, int row, bool use_story_font)
{
    char page_buf[32];
    int col;

    if (compact_pages <= 1)
        return;

    if (wid < 1)
        wid = 80;

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", sheet_page + 1, compact_pages);
    col = wid - (int)strlen(page_buf) - 1;
    if (col < 0)
        col = 0;

    if (use_story_font)
        sdl_story_font_enable();

    Term_putstr(col, row, -1, TERM_SLATE, page_buf);

    if (use_story_font)
        sdl_story_font_disable();
}

void do_cmd_character_sheet(void)
{
    char ch;

    int mode = 0;
    int sheet_page = 1;
    int body_scroll = 0;
    int last_sheet_page = -1;
    int focus_item = -1;
    bool focus_from_pointer = false;

    enum {
        CHAR_SHEET_MODE_COMPACT_DESC_FLAGS = 100,
        CHAR_SHEET_MODE_COMPACT_STATS_SKILLS = 101,
    };

    /* Clear any active banner before opening character sheet */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_proto();
    sdl_push_terminal_menu_scale();
    sdl_screen_back_gesture_begin();

    /* Forever */
    while (1)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;
        int indicator_row = 1;
        bool compact_sheet;
        int compact_pages;
        int max_body_scroll = 0;
        bool steamdeck = steamdeck_controls_active();
        bool sdl_sheet = false;
        character_sheet_item sheet_items[CHARACTER_SHEET_MAX_ITEMS];
        int sheet_item_count = 0;

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        compact_sheet = (wid < 80);
        compact_pages = compact_sheet ? 2 : 1;
        if (sheet_page >= compact_pages)
            sheet_page = compact_pages - 1;
        if (sheet_page < 0)
            sheet_page = 0;
        if (sheet_page != last_sheet_page)
        {
            body_scroll = 0;
            focus_item = -1;
            focus_from_pointer = false;
            sdl_hover_tooltip_clear();
            last_sheet_page = sheet_page;
        }

        if (compact_sheet)
        {
            switch (sheet_page)
            {
            case 0: mode = CHAR_SHEET_MODE_COMPACT_DESC_FLAGS; break;
            case 1: mode = CHAR_SHEET_MODE_COMPACT_STATS_SKILLS; break;
            default: mode = CHAR_SHEET_MODE_COMPACT_DESC_FLAGS; break;
            }
        }
        else
        {
            mode = 0;
        }

        if (compact_sheet && sheet_page == 0)
            display_player_compact_set_scroll(body_scroll);
        else
            display_player_compact_set_scroll(0);

        /* Display the player */
        display_player(mode);
        if (compact_sheet && sheet_page == 0)
        {
            max_body_scroll = display_player_compact_get_max_scroll();
            if (body_scroll > max_body_scroll)
            {
                body_scroll = max_body_scroll;
                display_player_compact_set_scroll(body_scroll);
                display_player(mode);
            }
        }

        if (compact_sheet && hgt <= 18)
            indicator_row = 0;

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;
        Term_erase(0, prompt_row, 255);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
        sdl_sheet = sdl_character_sheet_screen_begin_live(-1);
        sheet_item_count = sdl_sheet
            ? character_sheet_collect_semantic_items(sheet_items,
                CHARACTER_SHEET_MAX_ITEMS)
            : character_sheet_collect_items(sheet_items,
                CHARACTER_SHEET_MAX_ITEMS);
        if (focus_item >= sheet_item_count)
        {
            focus_item = -1;
            focus_from_pointer = false;
        }
        if (sdl_sheet)
        {
            int focus_choice = (focus_item >= 0)
                ? CHARACTER_SHEET_CLICK_ITEM_BASE + focus_item
                : -1;

            (void)sdl_character_sheet_screen_begin_live(focus_choice);
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
        }
        else if (focus_item >= 0 && sheet_item_count > 0)
        {
            character_sheet_highlight_item(&sheet_items[focus_item]);
            character_sheet_show_item_tooltip(&sheet_items[focus_item]);
        }
        else
        {
            sdl_hover_tooltip_clear();
        }

        /* Prompt - dynamic, width-aware, and user-friendly for new players */
        {
            char prompt_buf[256];
            char visible_prompt_buf[256];
#ifdef DEBUG_CURSES
            const bool include_curses = true;
#else
            const bool include_curses = false;
#endif

            character_sheet_build_prompt(steamdeck, include_curses, wid, prompt_buf, sizeof(prompt_buf));
            character_sheet_fit_prompt_text(1, wid, prompt_buf,
                visible_prompt_buf, sizeof(visible_prompt_buf));

            if (story_character_enabled())
                sdl_story_font_enable();

            character_sheet_put_prompt_fit(1, prompt_row, wid, TERM_L_WHITE,
                visible_prompt_buf);
            ui_menu_click_add_text_token('a', 1, prompt_row, visible_prompt_buf,
                "abilities");
            ui_menu_click_add_text_token('i', 1, prompt_row, visible_prompt_buf,
                "increase");
            ui_menu_click_add_text_token('?', 1, prompt_row, visible_prompt_buf,
                "help");
            ui_menu_click_add_text_token(ESCAPE, 1, prompt_row, visible_prompt_buf,
                "back");
            ui_menu_click_add_text_token('n', 1, prompt_row, visible_prompt_buf,
                "notes");
            ui_menu_click_add_text_token('s', 1, prompt_row, visible_prompt_buf,
                "story");
            ui_menu_click_add_text_token('f', 1, prompt_row, visible_prompt_buf,
                "file");
#ifdef DEBUG_CURSES
            ui_menu_click_add_text_token('c', 1, prompt_row, visible_prompt_buf,
                "curses");
#endif

            character_sheet_draw_page_indicator(sheet_page, compact_pages, wid,
                indicator_row,
                story_character_enabled());

            if (story_character_enabled())
                sdl_story_font_disable();
        }

        (void)Term_set_cursor(false);
        Term_fresh();  /* Render commands */

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

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
            bool allow_item_nav = sheet_item_count > 0
                && (!compact_sheet || sheet_page == 1 || focus_item >= 0);

            if (d && allow_item_nav)
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

        if (compact_pages > 1)
        {
            if (sheet_page == 0 && max_body_scroll > 0)
            {
                if (ch == '8')
                {
                    if (body_scroll > 0)
                        body_scroll--;
                    continue;
                }
                if (ch == '2')
                {
                    if (body_scroll < max_body_scroll)
                        body_scroll++;
                    continue;
                }
            }

            if ((ch == '4') || ((ch == '8') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + compact_pages - 1) % compact_pages;
                continue;
            }
            if ((ch == '6') || ((ch == '2') && !(sheet_page == 0 && max_body_scroll > 0)))
            {
                sheet_page = (sheet_page + 1) % compact_pages;
                continue;
            }
        }

        /* Increase skills - 'i', Space, or confirm button */
        if (ch == 'i' || ch == ' ' || ch == INPUT_BIND_CONFIRM
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

        /* Abilities - 'a', Tab, or X button */
        else if ((ch == 'a') || (ch == '\t') || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
            (void)do_cmd_ability_screen();
            /* Force redraw after ability changes */
            p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_EXP);
            handle_stuff();
        }

        /* File dump - 'f' or L1 ('e') */
        else if (ch == 'f' || (steamdeck && ch == 'e'))
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

        /* Tutorial / Help - '?' or RS Right */
        else if (ch == '?' || (steamdeck && ch == steamdeck_info_key()))
        {
            sdl_hover_tooltip_clear();
            sdl_character_sheet_screen_hide();
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
    screen_pop_touch_pane_proto();
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
    bool sdl_sheet;
    int prompt_row = -1;
    int prompt_col = 0;
    int prompt_len = 0;
    cptr prompt = "Continue to Stats";

    if (!p_ptr)
        return;

    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_proto();

    sdl_sheet = sdl_character_sheet_screen_begin_birth_preview();
    if (sdl_sheet)
    {
        int count = character_sheet_collect_semantic_items(sheet_items,
            CHARACTER_SHEET_MAX_ITEMS);

        (void)sdl_character_sheet_screen_begin_birth_preview();
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
    }
    else
    {
        /* Terminal fallback. */
        int wid = 80;
        int hgt = 24;

        display_player(0);
        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        prompt_row = hgt - 1;
        prompt_len = (int)strlen(prompt);
        prompt_col = (wid > prompt_len) ? (wid - prompt_len) / 2 : 0;
        Term_erase(0, prompt_row, 255);
        Term_putstr(prompt_col, prompt_row, -1, TERM_L_BLUE, prompt);
    }

    /* Wait for a fresh key or click to continue (hover wakeups are ignored). */
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    if (!sdl_sheet && prompt_row >= 0)
        ui_menu_click_add('\r', prompt_col, prompt_row, prompt_len);
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
    screen_pop_touch_pane_proto();
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
