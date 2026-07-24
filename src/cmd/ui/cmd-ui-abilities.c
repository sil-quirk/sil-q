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

static bool ability_menu_use_compact_layout(void)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    return (wid < 80);
}
static int ability_menu_list_col(void)
{
    return ability_menu_use_compact_layout() ? COL_SKILL : COL_ABILITY;
}

static int ability_menu_description_col(void)
{
    return ability_menu_use_compact_layout()
        ? COL_SKILL + ABILITY_MENU_LIST_WIDTH
        : COL_DESCRIPTION;
}

static int ability_menu_description_wrap(int desc_col)
{
    int wid = Term ? Term->wid : 80;

    if (wid < 1)
        wid = 80;

    if (wid <= desc_col)
        return desc_col + 1;

    return wid - 1;
}

static int ability_menu_click_width(int text_col, int next_col, cptr label)
{
    int prefix_col = indexed_menu_prefix_col(text_col);
    int text_width = menu_text_display_width(label) + text_col - prefix_col;
    int column_width = next_col - prefix_col - 1;

    if (text_width < 1)
        text_width = 1;

    return MAX(text_width, column_width);
}

static void ability_menu_put_exit_button(void)
{
    cptr label = "[Exit]";
    int len = (int)strlen(label);
    int wid = Term ? Term->wid : 80;
    int col;

    if (wid < 1)
        wid = 80;

    col = wid - len - 1;
    if (col < 0)
        col = 0;

    Term_putstr(col, 0, -1, TERM_WHITE, label);
    ui_menu_click_add(ABILITY_MENU_CLICK_EXIT, col, 0, len);
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_EXIT, col, 0, label,
        "Exit");
}

int abilities_in_skill(int skilltype);
bool prereqs(int skilltype, int abilitynum);

static int ability_purchase_exp_cost(int skilltype)
{
    int is_free = (c_info[p_ptr->pcharacter].flags & RHF_FREE) ? 1 : 0;
    int unit_cost = 500 - 200 * is_free;
    int exp_cost = (abilities_in_skill(skilltype) + 1) * unit_cost;

    exp_cost -= unit_cost * affinity_level(skilltype);

    if (skilltype == S_SNG)
        exp_cost -= unit_cost * minstrel_level();

    exp_cost += 100 * curse_flag_delta_cur(CUR_ABILITY_COST);

    if (exp_cost < 0)
        exp_cost = 0;

    return exp_cost;
}

static void ability_menu_sort_entries_by_level(ability_type* entries[],
    byte attrs[], int abilitynums[], int count)
{
    int i;

    for (i = 1; i < count; i++)
    {
        ability_type* entry = entries[i];
        byte attr = attrs[i];
        int abilitynum = abilitynums[i];
        int j = i - 1;

        while ((j >= 0) && (entry->level < entries[j]->level))
        {
            entries[j + 1] = entries[j];
            attrs[j + 1] = attrs[j];
            abilitynums[j + 1] = abilitynums[j];
            j--;
        }

        entries[j + 1] = entry;
        attrs[j + 1] = attr;
        abilitynums[j + 1] = abilitynum;
    }
}

static int ability_menu_text_width(int desc_col, int indent)
{
    int wrap = ability_menu_description_wrap(desc_col);
    int start = desc_col + indent;

    if (wrap < start)
        return 1;

    return wrap - start + 1;
}

static void ability_menu_format_amount_line(char* buf, size_t buflen,
    cptr long_label, cptr short_label, int need, int have, int max_width)
{
    if (max_width <= 30)
        strnfmt(buf, buflen, "%s %d / %d", short_label, need, have);
    else
        strnfmt(buf, buflen, "%d %s (you have %d)", need, long_label, have);
}

static void ability_menu_append_text(char* out, size_t outsz, size_t* cur,
    cptr text)
{
    if (!out || !outsz || !cur || !text)
        return;

    strnfcat(out, outsz, cur, "%s", text);
}

static cptr ability_menu_controller_text(cptr src, char* out, size_t outsz)
{
    char wait_label[32];
    char fletch_label[32];
    char exchange_label[32];
    char wait_token[64];
    char fletch_token[64];
    char exchange_token[64];
    struct replacement
    {
        cptr from;
        cptr to;
    } replacements[3];
    size_t cur = 0;
    const char* p;

    if (!src || !out || outsz == 0)
        return src;

    if (!steamdeck_controls_active())
        return src;

    controller_prompt_label_no_sticks('z', "z", wait_label, sizeof(wait_label));
    controller_prompt_label_no_sticks('-', "-", fletch_label, sizeof(fletch_label));
    controller_prompt_label_no_sticks('X', "X", exchange_label, sizeof(exchange_label));

    strnfmt(wait_token, sizeof(wait_token), "(%s/5)", wait_label);
    strnfmt(fletch_token, sizeof(fletch_token), "Use %s to", fletch_label);
    strnfmt(exchange_token, sizeof(exchange_token), "Use %s to", exchange_label);

    replacements[0].from = "(z/5)";
    replacements[0].to = wait_token;
    replacements[1].from = "Use '-' to";
    replacements[1].to = fletch_token;
    replacements[2].from = "Use 'X' to";
    replacements[2].to = exchange_token;

    out[0] = '\0';
    p = src;

    while (*p)
    {
        bool replaced = false;

        for (int i = 0; i < (int)N_ELEMENTS(replacements); i++)
        {
            size_t from_len = strlen(replacements[i].from);

            if (!strncmp(p, replacements[i].from, from_len))
            {
                ability_menu_append_text(out, outsz, &cur, replacements[i].to);
                p += from_len;
                replaced = true;
                break;
            }
        }

        if (!replaced)
        {
            char tmp[2] = { *p, '\0' };
            ability_menu_append_text(out, outsz, &cur, tmp);
            p++;
        }
    }

    return out;
}

static int ability_menu_next_row_after_text(int desc_col, int fallback_row)
{
    int x = desc_col;
    int y = fallback_row;

    Term_locate(&x, &y);

    if (x > desc_col)
        y++;

    return y;
}

static void ability_menu_render_prerequisites_block(int skilltype,
    const ability_type* b_ptr, int desc_col)
{
    int j;
    int row = ability_menu_next_row_after_text(desc_col, 3);
    int info_width = ability_menu_text_width(desc_col, 2);
    char buf[80];

    Term_putstr(desc_col, row, -1, TERM_YELLOW, "Prerequisites:");

    ability_menu_format_amount_line(buf, sizeof(buf), "skill points", "Skill",
        b_ptr->level, p_ptr->skill_base[skilltype], info_width);

    Term_putstr(desc_col + 2, row + 1, -1,
        (b_ptr->level <= p_ptr->skill_base[skilltype]) ? TERM_L_GREEN
                                                       : TERM_L_DARK,
        buf);

    row += 2;

    if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
    {
        for (j = 0; j < b_ptr->prereqs; j++)
        {
            if (j == 0)
            {
                strnfmt(buf, sizeof(buf), "%s",
                    b_name
                        + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                               b_ptr->prereq_abilitynum[j])])
                              ->name);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "or %s",
                    b_name
                        + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                               b_ptr->prereq_abilitynum[j])])
                              ->name);
            }

            Term_putstr(j == 0 ? desc_col + 2 : desc_col + 5, row + j, -1,
                p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                 [b_ptr->prereq_abilitynum[j]]
                    ? TERM_L_GREEN
                    : TERM_L_DARK,
                buf);
        }

        row += b_ptr->prereqs;
    }
    else if (b_ptr->prereqs > 0)
    {
        Term_putstr(desc_col + 2, row, -1, TERM_GREEN, "Quick Study");
        row++;
    }

    if (skilltype != S_SPC && prereqs(skilltype, b_ptr->abilitynum))
    {
        int exp_cost = ability_purchase_exp_cost(skilltype);

        Term_putstr(desc_col, row, -1, TERM_YELLOW, "Current price:");

        ability_menu_format_amount_line(buf, sizeof(buf), "experience", "Exp",
            exp_cost, p_ptr->new_exp, info_width);
        Term_putstr(desc_col + 2, row + 1, -1,
            (exp_cost <= p_ptr->new_exp) ? TERM_L_GREEN : TERM_L_DARK, buf);

        row += 2;
    }

    Term_gotoxy(desc_col, row);
}

static int ability_menu_stepped_song_bonus(int skill, int first_threshold,
    int next_gap)
{
    int bonus = 1;
    int threshold = first_threshold;
    int gap = next_gap;

    if (skill < 0)
        skill = 0;

    while (skill > threshold)
    {
        bonus++;
        threshold += gap;
        gap++;
    }

    return bonus;
}

static int ability_menu_current_song_score(void)
{
    return MAX(0, p_ptr->skill_use[S_SNG]);
}

static bool ability_menu_weapon_skill_bonus_text(int skilltype,
    int abilitynum, char* bonus_text, size_t text_size)
{
    cptr target_skill = NULL;
    cptr partner_name = NULL;
    cptr partner_skill = NULL;
    int partner_skilltype = -1;
    int partner_abilitynum = -1;
    bool current;
    int bonus;

    if (!bonus_text || text_size == 0)
        return false;

    if (skilltype == S_MEL && abilitynum == MEL_WARDEN)
    {
        target_skill = "Archery";
        partner_name = "Versatility";
        partner_skill = "Melee";
        partner_skilltype = S_ARC;
        partner_abilitynum = ARC_VERSATILITY;
    }
    else if (skilltype == S_ARC && abilitynum == ARC_VERSATILITY)
    {
        target_skill = "Melee";
        partner_name = "Warden";
        partner_skill = "Archery";
        partner_skilltype = S_MEL;
        partner_abilitynum = MEL_WARDEN;
    }
    else
        return false;

    current = p_ptr->have_ability[skilltype][abilitynum]
        && p_ptr->active_ability[skilltype][abilitynum];
    bonus = current
        ? ability_current_skill_bonus(skilltype, abilitynum)
        : ability_potential_skill_bonus(skilltype, abilitynum);

    strnfmt(bonus_text, text_size, "%s bonus: +%d %s.",
        current ? "Current" : "Potential", bonus, target_skill);

    if (!current
        && p_ptr->active_ability[partner_skilltype][partner_abilitynum])
    {
        size_t used = strlen(bonus_text);

        strnfcat(bonus_text, text_size, &used,
            " With both active, %s's %s bonus becomes +%d.",
            partner_name, partner_skill,
            ability_potential_skill_bonus_with_partner(partner_skilltype,
                partner_abilitynum));
    }

    return true;
}

static bool ability_menu_other_bonus_text(int skilltype, int abilitynum,
    char* bonus_text, size_t text_size)
{
    bool current;
    int bonus;

    if (!bonus_text || text_size == 0)
        return false;

    current = p_ptr->have_ability[skilltype][abilitynum]
        && p_ptr->active_ability[skilltype][abilitynum];
    bonus_text[0] = '\0';

    switch (skilltype)
    {
    case S_EVN:
        switch (abilitynum)
        {
        case EVN_DODGING:
            if (current)
            {
                strnfmt(bonus_text, text_size,
                    "Current bonus: %+d evasion while moving in light armour.",
                    dodging_bonus());
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: +3 evasion when you move while wearing only light armour.");
            }
            break;

        case EVN_PARRY:
            bonus = inventory[INVEN_WIELD].k_idx
                ? inventory[INVEN_WIELD].evn : 0;
            if (current)
            {
                strnfmt(bonus_text, text_size,
                    "Current bonus: %+d evasion from your primary melee weapon.",
                    player_active_weapon_is_melee() ? bonus : 0);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: %+d evasion from your primary melee weapon when melee is active.",
                    bonus);
            }
            break;

        default:
            break;
        }
        break;

    case S_STL:
        if (abilitynum == STL_ASSASSINATION)
        {
            bonus = p_ptr->skill_use[S_STL];
            if (current)
            {
                strnfmt(bonus_text, text_size,
                    "Current bonus: %+d melee vs non-alert creatures and Song of Disguise targets.",
                    bonus);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: %+d melee vs non-alert creatures and Song of Disguise targets.",
                    bonus);
            }
        }
        break;

    case S_PER:
        switch (abilitynum)
        {
        case PER_FOCUSED_ATTACK:
            bonus = p_ptr->skill_use[S_PER] / 2;
            if (current)
            {
                strnfmt(bonus_text, text_size,
                    "Current bonus: up to %+d attack after passing the previous turn.",
                    bonus);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: up to %+d attack after passing the previous turn.",
                    bonus);
            }
            break;

        case PER_CONCENTRATION:
            bonus = p_ptr->skill_use[S_PER] / 2;
            if (current)
            {
                int current_bonus = 0;

                if (p_ptr->last_attack_m_idx > 0
                    && p_ptr->last_attack_m_idx < mon_max
                    && mon_list[p_ptr->last_attack_m_idx].r_idx)
                {
                    current_bonus = MIN(p_ptr->consecutive_attacks, bonus);
                }

                strnfmt(bonus_text, text_size,
                    "Current bonus: up to %+d attack on the same target (currently %+d).",
                    bonus, current_bonus);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: up to %+d attack per consecutive round on the same target.",
                    bonus);
            }
            break;

        case PER_MASTER_HUNTER:
            bonus = p_ptr->skill_use[S_PER] / 2;
            if (current)
            {
                strnfmt(bonus_text, text_size,
                    "Current bonus: up to %+d attack per previous kill of the same monster type.",
                    bonus);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: up to %+d attack per previous kill of the same monster type.",
                    bonus);
            }
            break;

        default:
            break;
        }
        break;

    case S_WIL:
        switch (abilitynum)
        {
        case WIL_STRENGTH_IN_ADVERSITY:
            if (current)
            {
                bonus = 0;
                if (health_level(p_ptr->chp, p_ptr->mhp)
                    <= HEALTH_BADLY_WOUNDED)
                    bonus = 1;
                if (health_level(p_ptr->chp, p_ptr->mhp)
                    <= HEALTH_ALMOST_DEAD)
                    bonus = 3;

                strnfmt(bonus_text, text_size,
                    "Current bonus: %+d Strength, %+d Dexterity, %+d Grace at current health.",
                    bonus, bonus, bonus);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: +1 Strength, Dexterity, and Grace at 50%% HP; +3 each at 25%% HP.");
            }
            break;

        case WIL_VENGEANCE:
            if (current)
            {
                strnfmt(bonus_text, text_size,
                    "Current bonus: %+d melee damage die ready after taking melee damage.",
                    p_ptr->vengeance);
            }
            else
            {
                strnfmt(bonus_text, text_size,
                    "Potential bonus: +1 melee damage die after taking melee damage.");
            }
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return bonus_text[0] != '\0';
}

static bool ability_menu_dynamic_bonus_text(int skilltype, int abilitynum,
    char* bonus_text, size_t text_size)
{
    if (ability_menu_weapon_skill_bonus_text(skilltype, abilitynum,
            bonus_text, text_size))
        return true;

    return ability_menu_other_bonus_text(skilltype, abilitynum, bonus_text,
        text_size);
}

static int ability_menu_minor_song_score(int song_skill)
{
    if (song_skill <= 0)
        return 0;

    if (c_info[p_ptr->pcharacter].flags_u & UNQ_WOVEN_MASTER)
        return song_skill;

    return song_skill / 2;
}

static int ability_menu_song_synergy_bonus(int song_skill)
{
    if (song_skill <= 0)
        return 0;

    return (song_skill + 5) / 10;
}

static void ability_menu_append_song_cost(char* text, size_t text_size,
    const ability_type* b_ptr)
{
    cptr cost_desc = song_voice_cost_desc(b_ptr->abilitynum);

    if (!cost_desc || !cost_desc[0])
        return;

    SDL_strlcat(text, " Cost: ", text_size);
    SDL_strlcat(text, cost_desc, text_size);
    SDL_strlcat(text, ".", text_size);
}

static void ability_menu_render_song_bonus_block(const ability_type* b_ptr)
{
    int song_skill = ability_menu_current_song_score();
    char bonus_text[384];

    bonus_text[0] = '\0';

    switch (b_ptr->abilitynum)
    {
    case SNG_ELBERETH:
    {
        int will_penalty = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: enemy Will -%d.", will_penalty);
        break;
    }
    case SNG_CHALLENGE:
    {
        int debuff = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: enemy Will and Stealth -%d.", debuff);
        break;
    }
    case SNG_DELVINGS:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: delving range %d squares.", song_skill + 8);
        break;
    }
    case SNG_FREEDOM:
    {
        SDL_strlcpy(bonus_text,
            "\n\nCurrent effect: +1 free action while singing.",
            sizeof(bonus_text));
        break;
    }
    case SNG_SILENCE:
    {
        int silence_bonus = song_skill / 2;
        int enemy_song_penalty = silence_bonus / 2;
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: +%d to hush/noise checks; enemy songs -%d.",
            silence_bonus, enemy_song_penalty);
        break;
    }
    case SNG_STAUNCHING:
    {
        int base_heal = song_skill / 12;
        int extra_turns = song_skill % 12;

        if (extra_turns > 0)
        {
            strnfmt(bonus_text, sizeof(bonus_text),
                "\n\nCurrent effect: stops bleeding and heals %d HP/turn, with +1 extra on %d turns in 12.",
                base_heal, extra_turns);
        }
        else
        {
            strnfmt(bonus_text, sizeof(bonus_text),
                "\n\nCurrent effect: stops bleeding and heals %d HP/turn.",
                base_heal);
        }
        break;
    }
    case SNG_THRESHOLDS:
    {
        SDL_strlcpy(bonus_text,
            "\n\nCurrent effect: closes doors as warded barriers.",
            sizeof(bonus_text));
        break;
    }
    case SNG_TREES:
    {
        int light_radius = ability_menu_stepped_song_bonus(song_skill, 5, 6);
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: +%d light radius.", light_radius);
        break;
    }
    case SNG_WOVEN_THEMES:
    {
        int minor_skill = ability_menu_minor_song_score(song_skill);
        int synergy_bonus = ability_menu_song_synergy_bonus(song_skill);
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: a minor theme uses Song %d; a valid synergy pair adds +%d Song. Minor themes pay their normal Voice cost.",
            minor_skill, synergy_bonus);
        break;
    }
    case SNG_SLAYING:
    {
        int hp_threshold = song_skill * 2;
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN)
            hp_threshold *= 2;

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: criticals can slay foes at %d HP or less.",
            hp_threshold);
        break;
    }
    case SNG_REVEALING:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: rolls to reveal monsters/items within %d squares; revealed, carried, and equipped items get +1d5 identification.",
            (song_skill / 2) + 8);
        break;
    }
    case SNG_ELVENESS:
    {
        int evasion_bonus = ability_menu_stepped_song_bonus(song_skill, 7, 8);
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: +1 Grace and +%d Evasion.",
            evasion_bonus);
        break;
    }
    case SNG_STAYING:
    {
        int will_bonus = song_skill / 2;
        int protection_dice = 2;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN)
        {
            will_bonus = song_skill * 2;
            protection_dice = 4;
        }

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: +%d Will and [%dd2] protection.",
            will_bonus, protection_dice);
        break;
    }
    case SNG_DISGUISE:
    {
        int disguise_bonus = song_skill + 5;
        const char* extra = (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_TURGON)
            ? " + Perception"
            : "";

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: disguise checks use %d + Will%s.",
            disguise_bonus, extra);
        break;
    }
    case SNG_LORIEN:
    {
        int sleep_score = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_LUT)
            sleep_score = (3 * song_skill) / 2;

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: sleep checks use %d.", sleep_score);
        break;
    }
    case SNG_SHATTERING:
    {
        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: each successful shatter has a %d%% weaken chance.",
            song_skill / 3);
        break;
    }
    case SNG_MASTERY:
    {
        int mastery_bonus = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL)
            mastery_bonus = (7 * song_skill) / 4;

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: mastery rolls are 2d8 + %d.",
            mastery_bonus);
        break;
    }
    case SNG_GRA:
    {
        SDL_strlcpy(bonus_text, "\n\nCurrent effect: +1 Grace.",
            sizeof(bonus_text));
        break;
    }
    case SNG_CONTEST:
    {
        int will_penalty = MAX(1, song_skill / 3);
        int stealth_penalty = MAX(1, song_skill / 2);
        int evasion_penalty = MAX(1, song_skill / 5);
        int armour_penalty = MAX(1, song_skill / 12);

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: duel checks add Will/2; victory inflicts -%d Will, -%d Stealth, -%d Evasion, -%d armour die.",
            will_penalty, stealth_penalty, evasion_penalty,
            armour_penalty);
        break;
    }
    case SNG_LAMENT:
    {
        int will_penalty = MAX(1, song_skill / 2);
        int attrition_steps = MAX(1, song_skill / 12);

        strnfmt(bonus_text, sizeof(bonus_text),
            "\n\nCurrent effect: duel checks add Will/2; victory inflicts -%d Will and -%d health/damage steps.",
            will_penalty, attrition_steps);
        break;
    }
    default:
        break;
    }

    if (bonus_text[0])
    {
        ability_menu_append_song_cost(bonus_text, sizeof(bonus_text), b_ptr);
        text_out_to_screen(TERM_L_GREEN, bonus_text);
    }
}

static void ability_menu_render_dynamic_bonus(int skilltype,
    int abilitynum)
{
    char bonus_text[320];
    char rendered_text[340];

    if (!ability_menu_dynamic_bonus_text(skilltype, abilitynum,
            bonus_text, sizeof(bonus_text)))
        return;

    strnfmt(rendered_text, sizeof(rendered_text), "\n\n%s", bonus_text);
    text_out_to_screen(TERM_L_GREEN, rendered_text);
}

/* ------------------------------------------------------------------
 * add_random_curse()
 *    Marks the item cursed
 *    Gives it random negative modifiers
 *   Compatible with SIL-QH object_type (no flags1/2/3 fields)
 * ------------------------------------------------------------------ */
void add_random_curse(object_type *o_ptr)
{
    /* 1. make it show up as {cursed} right away */
    o_ptr->ident |= IDENT_CURSED;

    /* 2. negative pval / attack / evasion */
    int old_pval = o_ptr->pval;
    if (o_ptr->pval > 0)  o_ptr->pval = -(rand_int(3) + 1); /* 1  3 */
    int pval_delta = o_ptr->pval - old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
    if (o_ptr->att > 0) o_ptr->att = -(rand_int(3) + 1);
    if (o_ptr->evn > 0) o_ptr->evn = -(rand_int(3) + 1);

    /* 3. very small chance to damage dice on weapons / armour */
    if (one_in_(8))
    {
        if (o_ptr->dd) o_ptr->dd = MAX(1, o_ptr->dd - 1);
        if (o_ptr->pd) o_ptr->pd = MAX(1, o_ptr->pd - 1);
    }
}


int ability_index(int skilltype, int abilitynum)
{
    int i;
    ability_type* b_ptr;

    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* Stop if you get the correct ability number */
        if (b_ptr->abilitynum == abilitynum)
            return (i);
    }

    // Hack: there is no reasonable default value, but this will do
    return (0);
}

/*
 *  Counts the number of innate abilities in a skill
 */

int abilities_in_skill(int skilltype)
{
    int i;
    ability_type* b_ptr;
    int count = 0;

    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* Add to the count */
        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            count++;
    }

    return (count);
}

static bool prereq_abilities_met(const ability_type* b_ptr)
{
    int i;

    if (b_ptr->prereqs > 0 && !(p_ptr->active_ability[S_PER][PER_QUICK_STUDY]))
    {
        for (i = 0; i < b_ptr->prereqs; i++)
        {
            if (p_ptr->innate_ability[b_ptr->prereq_skilltype[i]]
                                     [b_ptr->prereq_abilitynum[i]])
                return (true);
        }
        return (false);
    }

    return (true);
}

bool prereqs(int skilltype, int abilitynum)
{
    ability_type* b_ptr;

    b_ptr = &b_info[ability_index(skilltype, abilitynum)];

    if (p_ptr->skill_base[skilltype] < b_ptr->level)
    {
        return (false);
    }

    return prereq_abilities_met(b_ptr);
}

static char song_menu_letter(int song_index)
{
    char letter = (char)('a' + song_index);

    if (letter >= 's')
        letter++;

    return letter;
}

static int song_index_from_menu_letter(char letter)
{
    if (letter < 'a' || letter > 'z')
        return -1;

    if (letter == 's')
        return -1;

    if (letter > 's')
        letter--;

    return (int)(letter - 'a');
}

static u32b song_menu_use_counter = 0;
static u32b song_menu_last_used[SNG_MAX];

static bool song_menu_is_singable(int song)
{
    return (song >= 0) && (song < SNG_MAX) && (song != SNG_WOVEN_THEMES)
        && (song != SNG_GRA);
}

static bool song_menu_sorts_before(int song, int other)
{
    if (op_ptr && song_list_sort_by_recent
        && (song_menu_last_used[song] != song_menu_last_used[other]))
    {
        return song_menu_last_used[song] > song_menu_last_used[other];
    }

    return song < other;
}

static int song_menu_collect_available(int songs[], int max_songs)
{
    int i, j;
    int count = 0;

    for (i = 0; i < SNG_MAX; i++)
    {
        if (!song_menu_is_singable(i))
            continue;

        if (!p_ptr->active_ability[S_SNG][i])
            continue;

        if (count < max_songs)
            songs[count++] = i;
    }

    for (i = 1; i < count; i++)
    {
        int song = songs[i];

        for (j = i - 1; j >= 0 && song_menu_sorts_before(song, songs[j]); j--)
        {
            songs[j + 1] = songs[j];
        }

        songs[j + 1] = song;
    }

    return count;
}

static int song_menu_choice_from_highlight(int highlight, const int songs[],
    int song_count)
{
    if (highlight == 0)
        return SNG_NOTHING;

    if ((highlight > 0) && (highlight <= song_count))
        return songs[highlight - 1];

    if ((p_ptr->song2 != SNG_NOTHING) && (highlight == song_count + 1))
        return SNG_EXCHANGE_THEMES;

    return -1;
}

static int song_menu_total_options(int song_count)
{
    int total = 1 + song_count;

    if (p_ptr->song2 != SNG_NOTHING)
        total++;

    return total;
}

static void song_menu_mark_used(int song)
{
    if (!song_menu_is_singable(song))
        return;

    song_menu_last_used[song] = ++song_menu_use_counter;
}

/*
 * Present the available songs in the small SDL overlay panel.  Choices are
 * sequential line indices matching song_menu_choice_from_highlight; pointer
 * input comes back through ui_menu_click_take_action.
 */
static void song_menu_show_overlay(const int songs[], int song_count,
    int highlight)
{
    bool steamdeck = steamdeck_controls_active();
    char letter[4];
    int line = 0;

    sdl_song_menu_begin("Songs");

    sdl_song_menu_add_entry(line, steamdeck ? "" : "s)", "Stop Singing",
        TERM_SLATE);
    line++;

    for (int j = 0; j < song_count; j++)
    {
        int i = songs[j];
        cptr desc = b_name + (&b_info[ability_index(S_SNG, i)])->name;

        sprintf(letter, "%c)", song_menu_letter(i));
        sdl_song_menu_add_entry(line, steamdeck ? "" : letter, desc,
            TERM_L_WHITE);
        line++;
    }

    if (p_ptr->song2 != SNG_NOTHING)
    {
        sdl_song_menu_add_entry(line, steamdeck ? "" : "x)",
            "Exchange themes", TERM_L_BLUE);
        line++;
    }

    sdl_song_menu_set_highlight(highlight);
    sdl_song_menu_finish();
}

/*
 * Show an information-only song panel (no selectable rows) and wait for a
 * key or click to dismiss it.
 */
static void song_menu_show_message(cptr text)
{
    bool saved_hide_cursor = hide_cursor;

    /* request_command erased the top line; repaint the map behind the
     * overlay before blocking for input */
    p_ptr->redraw |= (PR_MAP);
    handle_stuff();
    Term_fresh();

    sdl_song_menu_begin("Songs");
    sdl_song_menu_add_text(text, TERM_L_WHITE);
    sdl_song_menu_finish();

    hide_cursor = true;
    ui_key_wait_dismiss_begin(ESCAPE);
    while (inkey() == UI_MENU_CLICK_WAKE_KEY)
        ;
    ui_key_wait_dismiss_clear();
    hide_cursor = saved_hide_cursor;
    sdl_song_menu_clear();
}

void do_cmd_change_song()
{
    bool done = false;

    int songs[SNG_MAX];
    int song_count = 0;
    int song_choice = -1;
    int highlight = 0; // Add highlight tracking

    char which;
    bool steamdeck = steamdeck_controls_active();
    bool saved_hide_cursor = hide_cursor;

    log_debug("Player opening song selection menu");

    // Check for song lockout timer first
    if (p_ptr->song_lockout_timer > 0)
    {
        char buf[80];

        strnfmt(buf, sizeof(buf), "You cannot sing for %d more turn%s.",
            p_ptr->song_lockout_timer,
            (p_ptr->song_lockout_timer == 1) ? "" : "s");
        song_menu_show_message(buf);
        return;
    }

    song_count = song_menu_collect_available(songs, SNG_MAX);

    // abort if you know no songs
    if (song_count == 0)
    {
        log_trace("No songs available - player knows no songs of power");
        song_menu_show_message("You do not know any songs of power.");
        return;
    }

    log_debug("Player has %d songs available", song_count);

    /* request_command erased the top line; repaint the map behind the
     * overlay before blocking for input */
    p_ptr->redraw |= (PR_MAP);
    handle_stuff();
    Term_fresh();

    /* The overlay panel owns the selection; never show the term cursor */
    hide_cursor = true;

    /* Repeat until done */
    while (!done)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        song_menu_show_overlay(songs, song_count, highlight);

        /* Get a key */
        which = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                int total_options = song_menu_total_options(song_count);

                if (clicked_choice >= 0 && clicked_choice < total_options)
                {
                    bool same_choice = (highlight == clicked_choice);

                    highlight = clicked_choice;

                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (!same_choice)
                        continue;

                    song_choice = song_menu_choice_from_highlight(highlight,
                        songs, song_count);
                    if (song_choice >= 0)
                        done = true;
                    continue;
                }

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
            }
            else if (which == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }
        }

        if (which == ESCAPE || (steamdeck && which == steamdeck_back_key()))
        {
            log_trace("Song selection cancelled by player");
            done = true;
            continue;
        }

        if (steamdeck && which == steamdeck_confirm_key())
        {
            which = ' ';
        }

        /* Parse it */
        switch (which)
        {
        case '\r': // Enter - select the highlighted entry
        case ' ': // Space - select the highlighted entry
        {
            song_choice =
                song_menu_choice_from_highlight(highlight, songs, song_count);

            if (song_choice >= 0)
            {
                done = true;
            }
            break;
        }

        case '*':
        case '?': // the overlay list is always visible
            break;

        case '2': // Down arrow / scroll down
        {
            int total_options = song_menu_total_options(song_count);

            highlight = (highlight + 1) % total_options;
            break;
        }

        case '8': // Up arrow / scroll up
        {
            int total_options = song_menu_total_options(song_count);

            highlight = (highlight - 1 + total_options) % total_options;
            break;
        }

        case '6': // Right arrow / select highlighted
        {
            song_choice =
                song_menu_choice_from_highlight(highlight, songs, song_count);

            if (song_choice >= 0)
            {
                done = true;
            }
            break;
        }

        case 's':
        {
            log_debug("Player selected to stop singing");
            song_choice = SNG_NOTHING;
            done = true;
            break;
        }

        case 'x':
        {
            if (steamdeck)
            {
                log_trace("Illegal song choice attempted");
                bell("Illegal song choice.");
                break;
            }
            if (p_ptr->song2 != SNG_NOTHING)
            {
                log_debug("Player exchanging woven themes");
                song_choice = SNG_EXCHANGE_THEMES;
                done = true;
                break;
            }
            else
            {
                log_trace("Illegal song choice - no second theme to exchange");
                bell("Illegal song choice.");
                break;
            }
        }

        default:
        {
            if (steamdeck)
            {
                log_trace("Illegal song choice attempted");
                bell("Illegal song choice.");
                break;
            }

            song_choice = song_index_from_menu_letter(which);

            if (song_choice >= 0 && song_choice < SNG_MAX)
            {
                if (!song_menu_is_singable(song_choice))
                {
                    song_choice = -1;
                }
                else if (p_ptr->active_ability[S_SNG][song_choice])
                {
                    log_debug("Player selected song %d", song_choice);
                    done = true;
                    break;
                }
                else
                {
                    song_choice = -1;
                }
            }

            log_trace("Illegal song choice attempted");
            bell("Illegal song choice.");
            break;
        }
        }
    }

    hide_cursor = saved_hide_cursor;
    sdl_song_menu_clear();
    ui_menu_click_clear();

    if (song_choice >= 0)
    {
        if (death_spectator_active())
        {
            msg_print("You cannot do that during this final look.");
            return;
        }

        bool choice_stops_current_song = (song_choice == p_ptr->song1)
            || ((p_ptr->song2 != SNG_NOTHING) && (song_choice == p_ptr->song2));

        if ((song_choice != SNG_NOTHING)
            && (song_choice != SNG_EXCHANGE_THEMES)
            && !choice_stops_current_song)
        {
            if (chosen_oath(OATH_SILENCE) && !oath_invalid(OATH_SILENCE))
            {
                /* Use oath-specific confirmation prompt */
                char* prompt = oath_confirmation_prompt(OATH_SILENCE);
                if (!prompt || !prompt[0]) prompt = "Are you certain you wish to break your Oath of Silence?";

                if (get_check_oath_multiline(prompt))
                {
                    log_info("Player broke oath of silence to sing");

                    /* Curse message and selection handled by apply_oath_breaking_curse */
                    do_cmd_note("Broke your oath", p_ptr->depth);

                    /* Apply oath breaking consequences */
                    apply_oath_breaking_curse(OATH_SILENCE);

                    /* Only mark oath as broken if player actually has it */
                    p_ptr->oaths_broken |= OATH_SILENCE_FLAG;
                }
                else
                {
                    log_debug("Player cancelled song due to oath of silence");
                    return;
                }
            }
        }

        log_info("Player changed song to %s", song_choice == SNG_NOTHING ? "silence" :
                 song_choice == SNG_EXCHANGE_THEMES ? "exchange themes" : "new song");
        change_song(song_choice);
        if (song_menu_is_singable(song_choice) && singing(song_choice))
            song_menu_mark_used(song_choice);
    }
}

void wipe_screen_from(int col)
{
    int i;
    int wid = 80;
    int hgt = 24;

    (void)Term_get_size(&wid, &hgt);

    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    if (col >= wid)
        return;

    for (i = 1; i < hgt; i++)
        Term_erase(col, i, wid - col);
}

int elf_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    /* race.txt serials 0-2 are Noldor; serial 3 is Sindar */
    if (((r_ptr->flags2 & (RF2_ELFBANE)) && (p_ptr->prace <= 3))
        || ((r_ptr->flags4 & (RF4_NOLDORBANE)) && (p_ptr->prace <= 2))
        || ((r_ptr->flags4 & (RF4_SINDARBANE)) && (p_ptr->prace == 3)))
    {
        return (5);
    }

    return (0);
}

int dwarf_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    /* race.txt serial 4 is Naugrim */
    if ((r_ptr->flags4 & (RF4_DWARFBANE)) && (p_ptr->prace == 4))
    {
        return (5);
    }

    return (0);
}

int edain_bane_bonus(monster_type* m_ptr)
{
    monster_race* r_ptr;

    if (m_ptr == NULL)
        return (0);
    else
        r_ptr = &r_info[m_ptr->r_idx];

    /* race.txt serial 5 is Edain */
    if ((r_ptr->flags4 & (RF4_EDAINBANE)) && (p_ptr->prace == 5))
    {
        return (5);
    }

    return (0);
}

#define BANE_TYPES 13

static u32b bane_flag[] = { 0L, RF3_ORC, RF3_WOLF, RF3_SPIDER, RF3_TROLL,
    RF3_UNDEAD, RF3_RAUKO, RF3_SERPENT, RF3_DRAGON, RF3_VAMPIRE,
    RF3_HORROR, RF3_CAT, RF3_GIANT };

char* bane_name[] = { "Nothing", "Orc", "Wolf", "Spider", "Troll", "Wraith",
    "Rauko", "Serpent", "Dragon", "Vampire", "Horror", "Cat", "Giant" };

int bane_type_killed(int i)
{
    int j;
    int k = 0;

    /* Scan the monster races */
    for (j = 1; j < z_info->r_max; j++)
    {
        monster_race* r_ptr = &r_info[j];
        monster_lore* l_ptr = &l_list[j];

        if (r_ptr->flags3 & (bane_flag[i]))
        {
            k += l_ptr->pkills;
        }
    }

    return (k);
}

int bane_bonus_aux(void)
{
    int i = 2;
    int bonus = 0;
    int killed;

    killed = bane_type_killed(p_ptr->bane_type);
    while (i <= killed)
    {
        i *= 2;
        bonus++;
    }

    return (bonus);
}

int bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    monster_race* r_ptr;

    // paranoia
    if (m_ptr == NULL)
        return (0);

    // entranced players don't get the bonus
    if (p_ptr->entranced)
        return (0);

    // knocked out players don't get the bonus
    if (p_ptr->stun > 100)
        return (0);

    r_ptr = &r_info[m_ptr->r_idx];

    if (r_ptr->flags3 & (bane_flag[p_ptr->bane_type]))
    {
        bonus = bane_bonus_aux();
    }

    return (bonus);
}

/*
 * Calculate bane bonus for a specific bane type.
 * This is a helper function that can be used for both player bane and artifact bane.
 */
int bane_bonus_for_type(int bane_type_idx)
{
    int i = 2;
    int bonus = 0;
    int killed;

    if (bane_type_idx <= 0 || bane_type_idx >= BANE_TYPES)
        return 0;

    killed = bane_type_killed(bane_type_idx);
    while (i <= killed)
    {
        i *= 2;
        bonus++;
    }

    return bonus;
}

/*
 * Calculate bane bonus from artifact-granted Bane abilities.
 * These use a pre-selected bane type from the artifact definition.
 */
int artifact_bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    int i, j;
    monster_race* r_ptr;
    object_type* o_ptr;

    // paranoia
    if (m_ptr == NULL)
        return 0;

    // entranced players don't get the bonus
    if (p_ptr->entranced)
        return 0;

    // knocked out players don't get the bonus
    if (p_ptr->stun > 100)
        return 0;

    r_ptr = &r_info[m_ptr->r_idx];

    // Check all equipped items
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // Skip empty slots
        if (!o_ptr->k_idx)
            continue;

        // Check all abilities on this item
        for (j = 0; j < o_ptr->abilities; j++)
        {
            // Is this a Bane ability with a pre-selected type?
            if (o_ptr->skilltype[j] == S_PER && o_ptr->abilitynum[j] == PER_BANE
                && o_ptr->bane_type[j] > 0)
            {
                // Skip if this matches the player's innate bane type
                // (they already get bonus from innate, no stacking)
                if (o_ptr->bane_type[j] == p_ptr->bane_type)
                    continue;

                // Does the monster match this bane type?
                if (r_ptr->flags3 & bane_flag[o_ptr->bane_type[j]])
                {
                    int this_bonus = bane_bonus_for_type(o_ptr->bane_type[j]);
                    if (this_bonus > bonus)
                        bonus = this_bonus;
                }
            }
        }
    }

    return bonus;
}

int spider_bane_bonus(void)
{
    if (bane_flag[p_ptr->bane_type] == RF3_SPIDER)
        return (bane_bonus_aux());
    else
        return (0);
}

/*
 * Calculate spider bane bonus from artifact-granted Bane abilities.
 * Used for web-related difficulty checks.
 */
int artifact_spider_bane_bonus(void)
{
    int bonus = 0;
    int i, j;
    object_type* o_ptr;

    // Check all equipped items
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        // Skip empty slots
        if (!o_ptr->k_idx)
            continue;

        // Check all abilities on this item
        for (j = 0; j < o_ptr->abilities; j++)
        {
            // Is this a Bane ability with Spider type? (Spider = 3)
            if (o_ptr->skilltype[j] == S_PER && o_ptr->abilitynum[j] == PER_BANE
                && bane_flag[o_ptr->bane_type[j]] == RF3_SPIDER)
            {
                // Skip if this matches the player's innate bane type
                if (o_ptr->bane_type[j] == p_ptr->bane_type)
                    continue;

                int this_bonus = bane_bonus_for_type(o_ptr->bane_type[j]);
                if (this_bonus > bonus)
                    bonus = this_bonus;
            }
        }
    }

    return bonus;
}

int unique_bane_bonus(monster_type* m_ptr)
{
    int bonus = 0;
    monster_race* r_ptr;

    // paranoia
    if (m_ptr == NULL)
        return (0);

    // entranced players don't get the bonus
    if (p_ptr->entranced)
        return (0);

    // knocked out players don't get the bonus
    if (p_ptr->stun > 100)
        return (0);

    // Must have the unique bane special ability
    if (!p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE])
        return (0);

    r_ptr = &r_info[m_ptr->r_idx];

    // Check if the monster is unique
    if (r_ptr->flags1 & RF1_UNIQUE)
    {
        // Calculate bonus using the same formula as normal bane
        int uniques_killed = unique_bane_type_killed();

        // Use same scaling as bane_bonus_aux: 1, 2, 4, 8, 16, etc.
        int threshold = 2;
        bonus = 0;
        while (threshold <= uniques_killed)
        {
            threshold *= 2;
            bonus++;
        }
    }

    return (bonus);
}

/* Calculate total unique monsters killed for unique bane */
int unique_bane_type_killed(void)
{
    int uniques_killed = 0;
    int i;

    // Count all unique monsters that have been killed
    for (i = 1; i < z_info->r_max; i++) {
        monster_race* check_r_ptr = &r_info[i];

        // Skip if not unique
        if (!(check_r_ptr->flags1 & RF1_UNIQUE)) continue;

        // Check if this unique has been killed (max_num is set to 0 when killed)
        if (check_r_ptr->max_num == 0) {
            uniques_killed++;
        }
    }

    return uniques_killed;
}

int bane_menu(int* highlight)
{
    int i;
    int ch;
    int options = BANE_TYPES - 1;
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    bool compact_layout = ability_menu_use_compact_layout();
    int list_col = compact_layout ? ability_menu_list_col()
                                  : ability_menu_description_col();
    int desc_col = compact_layout ? ability_menu_description_col()
                                  : list_col;
    int prefix_col = indexed_menu_prefix_col(list_col);
    int list_first_row = 4;
    int nav_row_1 = MAX(0, term_hgt - 2);
    int nav_row_2 = MAX(0, term_hgt - 1);
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    char buf[80];
    char prefix[8];

    byte attr;

    wipe_screen_from(prefix_col);
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    Term_putstr(list_col, 2, -1, TERM_WHITE, "Enemy types");

    // list the enemies
    for (i = 1; i < BANE_TYPES; i++)
    {
        int row = list_first_row + i - 1;
        int k = bane_type_killed(i);

        // Determine the appropriate colour
        if (k >= 4)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        indexed_menu_entry_label(buf, sizeof(buf), i - 1, bane_name[i]);
        Term_putstr(list_col, row, -1, attr, buf);
        ui_menu_click_add(i, prefix_col, row,
            ability_menu_click_width(list_col, desc_col, buf));

        indexed_menu_normal_prefix(prefix, sizeof(prefix), i - 1);
        Term_putstr(prefix_col, row, -1, attr, prefix);

        if (*highlight == i)
        {
            // highlight the label
            indexed_menu_focus_prefix(prefix, sizeof(prefix), i - 1);
            Term_putstr(prefix_col, row, -1, TERM_L_BLUE, prefix);
        }
    }

    if (options > 0)
    {
        ui_scroll_area_begin(list_first_row, list_first_row + options - 1,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
    }
    else
    {
        ui_scroll_area_clear();
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > options)
        *highlight = options;

    if (*highlight >= 1 && *highlight <= options)
    {
        int k = bane_type_killed(*highlight);
        int old_wrap = text_out_wrap;
        int old_indent = text_out_indent;
        int detail_row = compact_layout ? 4 : (BANE_TYPES + 4);
        byte detail_attr = (k >= 4) ? TERM_SLATE : TERM_L_DARK;

        if (compact_layout)
        {
            wipe_screen_from(desc_col);
            Term_putstr(desc_col, 2, -1, TERM_WHITE, "Enemy Details");
            Term_putstr(desc_col, detail_row, term_wid - desc_col,
                (k >= 4) ? TERM_SLATE : TERM_L_DARK,
                bane_name[*highlight]);
            detail_row += 2;
        }

        text_out_wrap = ability_menu_description_wrap(desc_col);
        text_out_indent = desc_col;

        Term_gotoxy(text_out_indent, detail_row);

        if (k >= 4)
        {
            strnfmt(buf, 80, "You have slain %d of these foes.", k);
        }
        else
        {
            strnfmt(buf, 80,
                "You have slain %d of these foes, and need to slay %d more.",
                k, 4 - k);
        }
        text_out_to_screen(detail_attr, buf);

        text_out_wrap = old_wrap;
        text_out_indent = old_indent;
    }

    if (compact_layout)
    {
        Term_putstr(desc_col, nav_row_1, term_wid - desc_col, TERM_SLATE,
            sdl_touch_only_device_active() ? "Tap row; selected row chooses"
                : (steamdeck ? "D-pad navigate" : "Dir navigate"));
        {
            char prompt[96];
            if (steamdeck)
            {
                char confirm_label[16];
                char back_label[16];

                controller_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                controller_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                strnfmt(prompt, sizeof(prompt), "%s Select  %s Back",
                    confirm_label, back_label);
            }
            else if (sdl_touch_only_device_active())
            {
                SDL_strlcpy(prompt, "Tap Back to exit", sizeof(prompt));
            }
            else
            {
                SDL_strlcpy(prompt, "Enter Select  Esc Back",
                    sizeof(prompt));
            }

            Term_putstr(desc_col, nav_row_2, term_wid - desc_col, TERM_SLATE,
                prompt);
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_ACTION,
                desc_col, nav_row_2, prompt, "Enter");
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_ACTION,
                desc_col, nav_row_2, prompt, "Select");
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_EXIT,
                desc_col, nav_row_2, prompt, "Esc");
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_EXIT,
                desc_col, nav_row_2, prompt, "Back");
        }
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(prefix_col, list_first_row + *highlight - 1);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = -1;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (clicked_choice == ABILITY_MENU_CLICK_EXIT)
            {
                if (click_action != UI_MENU_CLICK_HOVER)
                    return (BANE_TYPES + 1);
            }
            else if (clicked_choice == ABILITY_MENU_CLICK_ACTION)
            {
                if (click_action != UI_MENU_CLICK_HOVER)
                    return (*highlight);
            }
            else if (clicked_choice >= 1 && clicked_choice <= options)
            {
                bool same_choice = (*highlight == clicked_choice);

                *highlight = clicked_choice;
                if (click_action != UI_MENU_CLICK_HOVER && same_choice)
                    return (*highlight);
            }
            return (0);
        }
    }

    if (menu_letters && (ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        bane_menu(highlight);

        return (*highlight);
    }

    if (menu_letters && (ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;
        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (BANE_TYPES + 1);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    /* Next item */
    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

#define OATH_TYPES 6

static u32b oath_flag[] = { 0L, OATH_MERCY_FLAG, OATH_SILENCE_FLAG, OATH_IRON_FLAG, OATH_SMITH_FLAG, OATH_VALOROUS_FLAG, OATH_LIGHT_FLAG };

char* oath_name[] = {
    "Nothing",
    "Mercy",
    "Silence",
    "Iron",
    "Smith",
    "Valorous Heart",
    "Light",
};

char* oath_desc1[] = {
    "Nothing",
    "to leave Angband without shedding blood of Man or Elf",
    "to leave Angband as you came, grim and silent",
    "that none will daunt you from facing Morgoth forthwith",
    "to craft all blades and armour by thine own hand",
    "to face your enemy while it has the heart to fight",
    "to bear the light of the stars and refuse all shadowed gear",
};

char* oath_desc2[] = {
    "Nothing",
    "attack Men or Elves",
    "sing",
    "go up stairs without a Silmaril",
    "pick up weapons or armour from the ground",
    "attack or deal damage to enemies that are fleeing in terror",
    "wear items that dim or shroud your light",
};

char* oath_reward[] = {
    "Nothing",
    "+1 Grace",
    "+1 Strength",
    "+2 Constitution",
    "+5 Smithing",
    "+1 Dexterity",
    "+1 Light Radius",
};

static const char* oath_name_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_name)) return "Unknown";
    return oath_name[oath_id];
}

static const char* oath_desc2_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_desc2)) return "";
    return oath_desc2[oath_id];
}

static const char* oath_reward_short(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_reward)) return "";
    return oath_reward[oath_id];
}

bool oath_invalid(int i)
{
    if (i < 0 || i >= (int)N_ELEMENTS(oath_flag)) return false;
    return ((p_ptr->oaths_broken & oath_flag[i]) > 0);
}

bool chosen_oath(int oath)
{
    return p_ptr->oath_type == oath;
}

/*
 * Helper functions to retrieve oath text from oath_info
 */
char* oath_confirmation_prompt(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].confirmation_prompt) return "";
    return oath_name_text + oath_info[oath_id].confirmation_prompt;
}

char* oath_curse_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].curse_message) return "";
    return oath_name_text + oath_info[oath_id].curse_message;
}

char* oath_permanent_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].permanent_message) return "";
    return oath_name_text + oath_info[oath_id].permanent_message;
}

char* oath_death_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].death_message) return "";
    return oath_name_text + oath_info[oath_id].death_message;
}

char* oath_banned_text(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].banned_text) return "";
    return oath_desc_text + oath_info[oath_id].banned_text;
}

char* oath_name_str(int oath_id)
{
    if (oath_id == 0) return "No oath";
    if (!z_info) return "";
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].name) return "";
    return oath_name_text + oath_info[oath_id].name;
}

char* oath_description(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].text) return "";
    return oath_desc_text + oath_info[oath_id].text;
}

char* oath_pledge(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].pledge_text) return "";
    return oath_name_text + oath_info[oath_id].pledge_text;
}

char* oath_forbidden(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].forbidden_text) return "";
    return oath_name_text + oath_info[oath_id].forbidden_text;
}

char* oath_reward_text(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max) return "";
    if (!oath_info[oath_id].reward_text) return "";
    return oath_name_text + oath_info[oath_id].reward_text;
}

static int oath_menu_put_wrapped(int desc_col, int row, byte attr, cptr text)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = ability_menu_description_wrap(desc_col);
    text_out_indent = desc_col;
    Term_gotoxy(desc_col, row);
    text_out_to_screen(attr, text);

    row = ability_menu_next_row_after_text(desc_col, row);
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    return row;
}

int oath_menu(int* highlight)
{
    int i, ch;
    int visible_count = 0;
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    int ability_col = ability_menu_list_col();
    int desc_col = ability_menu_description_col();
    int nav_row_1 = MAX(0, term_hgt - 2);
    int nav_row_2 = MAX(0, term_hgt - 1);
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    /* Support up to 16 oaths without realloc. */
    int visible_oaths[16]; // Map display letters to oath indices
    char buf[80];
    byte attr;

    /* Tolkien-themed descriptions for better immersion */
    char* oath_tolkien_desc[] = {
        "",
        "\"Let no blood of the Children stain thy blade in these halls of sorrow\"",
        "\"In silence came I, and in silence shall I depart, as befits the wise\"",
        "\"Though darkness gather and Balrogs rise, I shall not yield nor turn aside\"",
        "\"By mine own hand shall all blades be wrought, and no other's craft shall I bear\"",
        "\"Valor guards the fallen foe; the honorable blade stays when terror takes them\"",
        "\"I will carry unsullied starlight, shunning the shadowed tools that would dim it\""
    };

    // Clear the abilities and description area (following abilities_menu2 pattern)
    wipe_screen_from(ability_col);
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    // Title in the abilities column
    Term_putstr(ability_col, 2, -1, TERM_WHITE, "Oaths");

    // Build visible oaths list and display them (1..OATH_TYPES)
    for (i = 1; i <= OATH_TYPES && i < (int)N_ELEMENTS(oath_name); i++)
    {
        if (visible_count >= (int)N_ELEMENTS(visible_oaths)) break;

        // Map this visible oath to its position
        visible_oaths[visible_count] = i;

        // Determine display color based on oath status
        if (oath_invalid(i))
        {
            attr = TERM_L_RED; // Broken oaths in red
        }
        else
        {
            attr = (*highlight == visible_count + 1) ? TERM_L_BLUE : TERM_WHITE;
        }

        // Format oath name with status indicator
        indexed_menu_entry_label(buf, sizeof(buf), visible_count,
            oath_name_short(i));

        // Display in abilities column with proper spacing
        Term_putstr(ability_col, 4 + visible_count, -1, attr, buf);
        ui_menu_click_add(visible_count + 1,
            indexed_menu_prefix_col(ability_col), 4 + visible_count,
            ability_menu_click_width(ability_col, desc_col, buf));
        visible_count++;
    }

    if (visible_count > 0)
    {
        ui_scroll_area_begin(4, 4 + visible_count - 1,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
    }
    else
    {
        ui_scroll_area_clear();
    }

    // Display detailed description for highlighted oath in description column
    if (*highlight >= 1 && *highlight <= visible_count)
    {
        int oath_idx = visible_oaths[*highlight - 1];

        // Clear description area first
        int row = 4;

        wipe_screen_from(desc_col);

        // Oath title
        Term_putstr(desc_col, 2, -1, TERM_WHITE, "Oath Details");

        if (oath_invalid(oath_idx))
        {
            // Menacing text for broken oaths
            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_RED,
                "OATH BROKEN");
            row++;
            row = oath_menu_put_wrapped(desc_col, row, TERM_RED,
                "\"Thy oath lies shattered, thy word worthless as dust.\"");
            row++;
            row = oath_menu_put_wrapped(desc_col, row, TERM_L_RED,
                "\"No Valar shall hear thy voice, no light shall guide thy path.\"");
            row++;
            (void)oath_menu_put_wrapped(desc_col, row, TERM_RED,
                "Forever marked as oathbreaker in this age.");
        }
        else
        {
            // Tolkien-themed quote
            char* quote = (oath_idx < (int)N_ELEMENTS(oath_tolkien_desc)) ? oath_tolkien_desc[oath_idx] : "";

            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_YELLOW,
                "Quote:");
            row = oath_menu_put_wrapped(desc_col, row, TERM_SLATE, quote);

            // Oath vow
            Term_putstr(desc_col, row++, term_wid - desc_col, TERM_WHITE,
                "Vow:");
            row = oath_menu_put_wrapped(desc_col, row, TERM_SLATE,
                (oath_idx >= 0 && oath_idx < (int)N_ELEMENTS(oath_desc1))
                    ? oath_desc1[oath_idx]
                    : "");

            // Restriction
            if (row < nav_row_1)
            {
                Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_RED,
                    "Restriction:");
                row = oath_menu_put_wrapped(desc_col, row, TERM_L_RED,
                    oath_desc2_short(oath_idx));
            }

            // Reward
            if (row < nav_row_1)
            {
                Term_putstr(desc_col, row++, term_wid - desc_col, TERM_L_GREEN,
                    "Reward:");
                (void)oath_menu_put_wrapped(desc_col, row, TERM_L_GREEN,
                    oath_reward_short(oath_idx));
            }
        }

        // Navigation instructions at bottom
        Term_putstr(desc_col, nav_row_1, term_wid - desc_col, TERM_SLATE,
            sdl_touch_only_device_active() ? "Tap row; selected row chooses"
                : (steamdeck ? "D-pad navigate" : "Dir navigate"));
        {
            char prompt[96];
            if (steamdeck)
            {
                char confirm_label[16];
                char back_label[16];

                controller_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                controller_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                strnfmt(prompt, sizeof(prompt), "%s Select  %s Back",
                    confirm_label, back_label);
            }
            else if (sdl_touch_only_device_active())
            {
                SDL_strlcpy(prompt, "Tap Back to exit", sizeof(prompt));
            }
            else
            {
                SDL_strlcpy(prompt, "Enter Select  Esc Back",
                    sizeof(prompt));
            }

            Term_putstr(desc_col, nav_row_2, term_wid - desc_col, TERM_SLATE,
                prompt);
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_ACTION,
                desc_col, nav_row_2, prompt, "Enter");
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_ACTION,
                desc_col, nav_row_2, prompt, "Select");
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_EXIT,
                desc_col, nav_row_2, prompt, "Esc");
            ui_menu_click_add_text_token(ABILITY_MENU_CLICK_EXIT,
                desc_col, nav_row_2, prompt, "Back");
        }
    }

    // Ensure highlight is within valid range
    if (*highlight < 1) *highlight = 1;
    if (*highlight > visible_count) *highlight = visible_count;

    /* Flush the prompt */
    Term_fresh();

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = -1;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (clicked_choice == ABILITY_MENU_CLICK_EXIT)
            {
                if (click_action != UI_MENU_CLICK_HOVER)
                    return OATH_TYPES + 1;
            }
            else if (clicked_choice == ABILITY_MENU_CLICK_ACTION)
            {
                if (click_action != UI_MENU_CLICK_HOVER && visible_count > 0)
                    return visible_oaths[*highlight - 1];
            }
            else if (clicked_choice >= 1 && clicked_choice <= visible_count)
            {
                bool same_choice = (*highlight == clicked_choice);

                *highlight = clicked_choice;
                if (click_action != UI_MENU_CLICK_HOVER && same_choice)
                    return visible_oaths[*highlight - 1];
            }
            return (0);
        }
    }

    /* Handle letter selection (a-z) for immediate highlighting */
    if (menu_letters && (ch >= 'a') && (ch < 'a' + visible_count))
    {
        *highlight = (int)ch - 'a' + 1;
        return oath_menu(highlight); // Recursive call to update display
    }

    /* Handle capital letter selection (A-Z) for immediate selection */
    if (menu_letters && (ch >= 'A') && (ch < 'A' + visible_count))
    {
        *highlight = (int)ch - 'A' + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* ESC or 'q' - exit menu */
    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        /* Return a sentinel that's outside valid oath indices */
        return OATH_TYPES + 1;
    }

    /* Enter or Space - select current highlighted oath */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        if (visible_count <= 0) return OATH_TYPES + 1;
        return visible_oaths[*highlight - 1]; // Return actual oath index
    }

    /* Navigation: Up (8) */
    if (ch == '8')
    {
        (*highlight)--;
        if (*highlight < 1) *highlight = visible_count;
    }

    /* Navigation: Down (2) */
    if (ch == '2')
    {
    (*highlight)++;
        if (*highlight > visible_count) *highlight = 1;
    }

    /* Recursive call to continue menu interaction */
    return oath_menu(highlight);
}

typedef struct ability_browser_layout
{
    int term_wid;
    int term_hgt;
    int visible_col;
    int visible_w;
    int title_row;
    int summary_row;
    int summary_rows;
    int skill_row;
    int skill_rows;
    int header_row;
    int divider_row;
    int list_row;
    int list_rows;
    int status_row;
    int prompt_row;
    int skill_col;
    int skill_w;
    int skill_divider_col;
    int ability_col;
    int ability_w;
    int ability_divider_col;
    int desc_col;
    int desc_w;
    bool stacked;
    int ability_row;
    int ability_rows;
    int ability_entry_rows;
    int desc_header_row;
    int desc_row;
    int desc_rows;
} ability_browser_layout;

typedef struct ability_browser_entry
{
    ability_type* b_ptr;
    int abilitynum;
    byte attr;
    char name[96];
} ability_browser_entry;

typedef struct ability_browser_desc_line
{
    byte attr;
    char text[ABILITY_BROWSER_DESC_LINE_LEN];
} ability_browser_desc_line;

static int ability_browser_cell_width_n(cptr text, int bytes)
{
    int safe_len;
    int display_width;
    int cell_width;
    int pixel_width;

    if (!text || bytes <= 0)
        return 0;

    safe_len = utf8_safe_prefix_len(text, bytes);
    if (safe_len <= 0)
        return 0;

    if (sdl_is_story_font_enabled() && !sdl_is_story_font_grid())
    {
        cell_width = sdl_get_cell_width();
        pixel_width = sdl_story_font_text_width(text, safe_len);
        if (cell_width > 0 && pixel_width > 0)
            return (pixel_width + cell_width - 1) / cell_width;
    }

    display_width = utf8_display_width_n(text, safe_len);

    return MAX(display_width, 1);
}

static int ability_browser_utf8_prefix_len(cptr text, int max_cols)
{
    int bytes = 0;
    int cols = 0;

    if (!text || max_cols <= 0)
        return 0;

    while (text[bytes])
    {
        int char_len = utf8_sequence_len(text + bytes);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = ability_browser_cell_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        cols += char_width;
        bytes += char_len;
    }

    return bytes;
}

static void ability_browser_fit_text(char* buf, size_t buflen, cptr text,
    int width)
{
    size_t len;
    int display_width;

    if (!buf || buflen == 0)
        return;

    if (!text)
        text = "";

    if (width < 1)
    {
        buf[0] = '\0';
        return;
    }

    len = strlen(text);
    display_width = utf8_display_width_n(text, (int)len);
    if (display_width <= width)
    {
        if (len < buflen)
        {
            SDL_strlcpy(buf, text, buflen);
        }
        else
        {
            int copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);

            SDL_memcpy(buf, text, (size_t)copy_len);
            buf[copy_len] = '\0';
        }
        return;
    }

    if (width == 1)
    {
        SDL_strlcpy(buf, "~", buflen);
        return;
    }

    if (width < 2 || buflen < 2)
    {
        buf[0] = '\0';
        return;
    }

    {
        int copy_len = ability_browser_utf8_prefix_len(text, width - 1);

        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 2);
        if (copy_len < 0)
            copy_len = 0;

        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        SDL_strlcat(buf, "~", buflen);
    }
}

static void ability_browser_put_fitted(int col, int row, int width, byte attr,
    cptr text)
{
    char fitted[180];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;

    ability_browser_fit_text(fitted, sizeof(fitted), text, width);
    Term_putstr(col, row, width, attr, fitted);
}

static void ability_browser_fill_row(int col, int row, int width, byte attr)
{
    char fill[180];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    SDL_memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, attr, fill);
}

static byte ability_browser_selected_attr(byte source_attr)
{
    (void)source_attr;
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static bool ability_browser_wrap_next(cptr* cursor, int width, char* line,
    size_t line_len);

static int ability_browser_wrapped_rows(cptr text, int width)
{
    const char* cursor = text;
    char line[180];
    int rows = 0;

    if (!text || !text[0] || width <= 0)
        return 1;

    while (ability_browser_wrap_next(&cursor, width, line, sizeof(line)))
        rows++;

    return MAX(rows, 1);
}

static void ability_browser_init_layout(ability_browser_layout* layout,
    int ability_count, cptr summary)
{
    int min_ability_w = 28;
    int min_desc_w = 26;
    int ability_w;
    int visible_col;
    int visible_w;
    int max_summary_rows;
    bool portrait;

    Term_get_size(&layout->term_wid, &layout->term_hgt);

    if (layout->term_wid < 1)
        layout->term_wid = 80;
    if (layout->term_hgt < 1)
        layout->term_hgt = 24;

    visible_col = sdl_main_view_visible_col0();
    visible_w = sdl_main_view_visible_cols();
    if (visible_col < 0 || visible_col >= layout->term_wid)
        visible_col = 0;
    if (visible_w <= 0 || visible_col + visible_w > layout->term_wid)
        visible_w = layout->term_wid - visible_col;
    if (visible_w < 1)
    {
        visible_col = 0;
        visible_w = layout->term_wid;
    }

    layout->visible_col = visible_col;
    layout->visible_w = visible_w;
    portrait = sdl_mobile_portrait_layout_active();
    layout->title_row = 0;
    layout->summary_row = (layout->term_hgt > 1) ? 1 : 0;
    max_summary_rows = MAX(1, layout->term_hgt - 8);
    layout->summary_rows = portrait
        ? MIN(ability_browser_wrapped_rows(summary, layout->visible_w),
            max_summary_rows)
        : 1;
    layout->skill_row = layout->summary_row + layout->summary_rows;
    if (layout->skill_row >= layout->term_hgt)
    {
        layout->summary_rows = 1;
        layout->skill_row = (layout->term_hgt > layout->summary_row + 1)
            ? layout->summary_row + 1
            : layout->summary_row;
    }
    layout->skill_rows = (portrait && layout->term_hgt > 4) ? 2 : 1;
    layout->header_row = (layout->term_hgt
            > layout->skill_row + layout->skill_rows)
        ? layout->skill_row + layout->skill_rows
        : layout->summary_row;
    layout->divider_row = (layout->term_hgt > layout->header_row + 1)
        ? layout->header_row + 1
        : layout->header_row;
    layout->list_row = layout->divider_row + 1;
    layout->prompt_row = layout->term_hgt - 1
        - sdl_touch_menu_button_reserved_rows();
    if (layout->prompt_row < layout->list_row)
        layout->prompt_row = layout->list_row;
    layout->status_row = (layout->prompt_row > layout->list_row)
        ? layout->prompt_row - 1
        : layout->prompt_row;
    layout->list_rows = layout->status_row - layout->list_row;
    if (layout->list_rows < 1)
        layout->list_rows = 1;

    ability_w = (layout->visible_w >= 96) ? 43 : 38;
    if (layout->visible_w < 75)
        ability_w = 34;
    if (layout->visible_w < 55)
        ability_w = 30;

    if (ability_w > layout->visible_w - min_desc_w - 3)
        ability_w = layout->visible_w - min_desc_w - 3;
    if (ability_w < min_ability_w)
        ability_w = min_ability_w;

    layout->skill_col = layout->visible_col;
    layout->skill_w = layout->visible_w;
    layout->skill_divider_col = -1;
    layout->ability_col = layout->visible_col;
    layout->ability_w = ability_w;
    layout->ability_divider_col = layout->ability_col + layout->ability_w + 1;
    layout->desc_col = layout->ability_divider_col + 2;
    layout->desc_w = layout->visible_col + layout->visible_w - layout->desc_col;
    if (layout->desc_w < 1)
        layout->desc_w = 1;
    layout->stacked = false;
    layout->ability_row = layout->list_row;
    layout->ability_rows = layout->list_rows;
    layout->ability_entry_rows = 1;
    layout->desc_header_row = layout->header_row;
    layout->desc_row = layout->list_row;
    layout->desc_rows = layout->list_rows;

    if (portrait && layout->list_rows >= 3)
    {
        int ability_rows = MAX(ability_count, 1);
        int desc_rows;

        /* Portrait is tall enough to keep one compact row per ability.  Size
         * the section to its contents instead of reserving two or three rows
         * per name and then capping the whole list at six rows. */
        if (ability_rows > layout->list_rows - 2)
            ability_rows = MAX(1, layout->list_rows - 2);
        desc_rows = layout->list_rows - ability_rows - 1;
        if (desc_rows < 1)
        {
            desc_rows = 1;
            ability_rows = MAX(1, layout->list_rows - desc_rows - 1);
        }

        layout->stacked = true;
        layout->ability_col = layout->visible_col;
        layout->ability_w = layout->visible_w;
        layout->ability_divider_col = -1;
        layout->desc_col = layout->visible_col;
        layout->desc_w = layout->visible_w;
        layout->ability_row = layout->list_row;
        layout->ability_rows = ability_rows;
        layout->ability_entry_rows = 1;
        layout->desc_header_row = layout->ability_row + ability_rows;
        layout->desc_row = layout->desc_header_row + 1;
        layout->desc_rows = desc_rows;
    }
}

static void ability_desc_add_line(ability_browser_desc_line lines[],
    int* line_count, byte attr, cptr text)
{
    if (!lines || !line_count || *line_count >= ABILITY_BROWSER_DESC_MAX_LINES)
        return;

    lines[*line_count].attr = attr;
    ability_browser_fit_text(lines[*line_count].text,
        sizeof(lines[*line_count].text), text ? text : "",
        ABILITY_BROWSER_DESC_LINE_LEN - 1);
    (*line_count)++;
}

static void ability_desc_add_blank(ability_browser_desc_line lines[],
    int* line_count)
{
    ability_desc_add_line(lines, line_count, TERM_SLATE, "");
}

static int ability_browser_wrap_take(cptr text, int max_bytes, int max_cols)
{
    int bytes = 0;
    int cols = 0;
    int last_space = -1;

    if (!text || max_bytes <= 0 || max_cols <= 0)
        return 0;

    while (bytes < max_bytes && text[bytes] && text[bytes] != '\n')
    {
        unsigned char ch = (unsigned char)text[bytes];
        int remaining = max_bytes - bytes;
        int char_len = utf8_sequence_len_n(text + bytes, remaining);
        int char_width;

        if (char_len <= 0 || char_len > remaining)
            break;

        char_width = ability_browser_cell_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        if (ch == ' ' || ch == '\t')
            last_space = bytes;

        cols += char_width;
        bytes += char_len;

        if (cols >= max_cols)
            break;
    }

    if (bytes >= max_bytes || text[bytes] == '\0' || text[bytes] == '\n')
        return bytes;
    if (last_space > 0)
        return last_space;
    if (bytes > 0)
        return bytes;

    return utf8_sequence_len_n(text, max_bytes);
}

static bool ability_browser_wrap_next(cptr* cursor, int width, char* line,
    size_t line_len)
{
    cptr p;
    int take;
    int copy_len;

    if (!cursor || !*cursor || !line || line_len == 0 || width <= 0)
        return false;

    p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\n')
        p++;
    if (!*p) {
        *cursor = p;
        line[0] = '\0';
        return false;
    }

    take = ability_browser_wrap_take(p, (int)strlen(p), width);
    if (take <= 0)
        return false;
    copy_len = take;
    while (copy_len > 0
        && (p[copy_len - 1] == ' ' || p[copy_len - 1] == '\t'))
    {
        copy_len--;
    }
    copy_len = utf8_safe_prefix_len(p, copy_len);
    if (copy_len >= (int)line_len)
        copy_len = utf8_safe_prefix_len(p, (int)line_len - 1);

    SDL_memcpy(line, p, (size_t)copy_len);
    line[copy_len] = '\0';
    p += take;
    while (*p == ' ' || *p == '\t' || *p == '\n')
        p++;
    *cursor = p;
    return true;
}

static void ability_desc_add_wrapped_segment(ability_browser_desc_line lines[],
    int* line_count, byte attr, cptr text, int width)
{
    const char* p = text;
    int max_width;
    int source_len;

    if (!p)
        return;

    max_width = width;
    if (max_width < 1)
        max_width = 1;
    if (max_width >= ABILITY_BROWSER_DESC_LINE_LEN)
        max_width = ABILITY_BROWSER_DESC_LINE_LEN - 1;

    source_len = (int)strlen(p);
    log_debug("ABILITY_WRAP begin width=%d max_width=%d bytes=%d story=%d grid=%d cell_w=%d text='%.100s'",
        width, max_width, source_len,
        sdl_is_story_font_enabled() ? 1 : 0,
        sdl_is_story_font_grid() ? 1 : 0,
        sdl_get_cell_width(), p);

    while (*p && *line_count < ABILITY_BROWSER_DESC_MAX_LINES)
    {
        if (*p == '\n')
        {
            log_debug("ABILITY_WRAP explicit-blank");
            ability_desc_add_blank(lines, line_count);
            p++;
            continue;
        }

        while (*p && *p != '\n'
            && *line_count < ABILITY_BROWSER_DESC_MAX_LINES)
        {
            char line[ABILITY_BROWSER_DESC_LINE_LEN];
            int paragraph_len = 0;
            int copy_len;
            int take;

            while (*p == ' ' || *p == '\t')
                p++;

            if (!*p || *p == '\n')
                break;

            while (p[paragraph_len] && p[paragraph_len] != '\n')
                paragraph_len++;

            take = ability_browser_wrap_take(p, paragraph_len, max_width);
            log_debug("ABILITY_WRAP take paragraph_bytes=%d take=%d max_width=%d preview='%.100s'",
                paragraph_len, take, max_width, p);
            if (take <= 0)
            {
                take = utf8_sequence_len_n(p, paragraph_len);
                if (take <= 0)
                    break;
            }

            copy_len = take;
            while (copy_len > 0
                && (p[copy_len - 1] == ' ' || p[copy_len - 1] == '\t'))
            {
                copy_len--;
            }
            copy_len = utf8_safe_prefix_len(p, copy_len);
            if (copy_len >= (int)sizeof(line))
                copy_len = utf8_safe_prefix_len(p, (int)sizeof(line) - 1);

            SDL_memcpy(line, p, (size_t)copy_len);
            line[copy_len] = '\0';
            log_debug("ABILITY_WRAP line copy_bytes=%d cells=%d text='%.120s'",
                copy_len, ability_browser_cell_width_n(line, copy_len), line);
            ability_desc_add_line(lines, line_count, attr, line);

            p += take;
            while (*p == ' ' || *p == '\t')
                p++;
        }

        if (*p == '\n')
        {
            log_debug("ABILITY_WRAP paragraph-blank");
            ability_desc_add_blank(lines, line_count);
            p++;
        }
    }
}

static void ability_desc_add_wrapped(ability_browser_desc_line lines[],
    int* line_count, byte attr, cptr text, int width)
{
    if (!text || !text[0])
        return;

    ability_desc_add_wrapped_segment(lines, line_count, attr, text, width);
}

static bool ability_menu_show_special_skill(void)
{
    int i;

    for (i = 0; i < ABILITIES_MAX; i++)
    {
        if (p_ptr->have_ability[S_SPC][i])
            return true;
    }

    return p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE];
}

static int ability_menu_skill_options(void)
{
    return ability_menu_show_special_skill() ? S_MAX : (S_MAX - 1);
}

static int ability_browser_next_skill_cost(int skilltype)
{
    if (skilltype < 0 || skilltype >= S_MAX || skilltype == S_SPC)
        return 0;

    return (p_ptr->skill_base[skilltype] + 1) * 100;
}

static void ability_browser_build_summary(int skilltype, char* summary,
    size_t summary_len)
{
    char next_skill[32];
    char ability_price[32];

    if (!summary || summary_len == 0)
        return;

    if (skilltype >= 0 && skilltype < S_MAX && skilltype != S_SPC)
    {
        strnfmt(next_skill, sizeof(next_skill), "%d XP",
            ability_browser_next_skill_cost(skilltype));
        strnfmt(ability_price, sizeof(ability_price), "%d XP",
            ability_purchase_exp_cost(skilltype));
    }
    else
    {
        SDL_strlcpy(next_skill, "granted", sizeof(next_skill));
        SDL_strlcpy(ability_price, "n/a", sizeof(ability_price));
    }

    strnfmt(summary, summary_len,
        "XP %ld | %s base %d, current %d | next skill +1: %s | ability price %s",
        (long)p_ptr->new_exp,
        (skilltype >= 0 && skilltype < S_MAX) ? skill_names_full[skilltype]
                                              : "Skill",
        (skilltype >= 0 && skilltype < S_MAX) ? p_ptr->skill_base[skilltype] : 0,
        (skilltype >= 0 && skilltype < S_MAX) ? p_ptr->skill_use[skilltype] : 0,
        next_skill, ability_price);
}

static bool ability_browser_train_skill(int skilltype)
{
    int cost;
    int old_base;
    char prompt[120];

    if (skilltype < 0 || skilltype >= S_MAX || skilltype == S_SPC)
    {
        bell("Special abilities are granted, not trained.");
        return false;
    }

    if (death_spectator_active())
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    if (p_ptr->skill_base[skilltype] >= BASE_SKILL_MAX)
    {
        bell("That skill cannot be increased further.");
        return false;
    }

    old_base = p_ptr->skill_base[skilltype];
    cost = ability_browser_next_skill_cost(skilltype);
    if (cost > p_ptr->new_exp)
    {
        bell("You do not have enough experience to increase that skill.");
        return false;
    }

    strnfmt(prompt, sizeof(prompt), "Increase %s from %d to %d for %d XP? ",
        skill_names_full[skilltype], p_ptr->skill_base[skilltype],
        p_ptr->skill_base[skilltype] + 1, cost);
    if (!get_check(prompt))
        return false;

    p_ptr->new_exp -= cost;
    p_ptr->skill_base[skilltype]++;
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    p_ptr->update |= (PU_BONUS | PU_MANA);
    handle_stuff();

    msg_format("%s increased to %d.", skill_names_full[skilltype],
        p_ptr->skill_base[skilltype]);
    if (old_base == 0)
        sdl_quick_access_suggest_skill_shortcut(skilltype);
    return true;
}

byte ability_skill_color(int skilltype)
{
    switch (skilltype)
    {
    case S_MEL: return TERM_RED;
    case S_ARC: return TERM_ORANGE;
    case S_EVN: return TERM_GREEN;
    case S_STL: return TERM_L_UMBER;
    case S_PER: return TERM_VIOLET;
    case S_WIL: return TERM_YELLOW;
    case S_SMT: return TERM_L_RED;
    case S_SNG: return TERM_L_GREEN;
    case S_SPC: return TERM_VIOLET;
    default: return TERM_WHITE;
    }
}

static byte ability_browser_skill_attr(int skilltype, bool hovered)
{
    return hovered ? TERM_L_BLUE : ability_skill_color(skilltype);
}

static cptr ability_browser_skill_trait_suffix(int skilltype)
{
    int score;

    if (!p_ptr || !rp_ptr || !current_character_profile
        || skilltype < 0 || skilltype >= S_MAX || skilltype == S_SPC)
    {
        return "";
    }

    score = affinity_level(skilltype);
    if (score >= 2)
        return "++";
    if (score == 1)
        return "+";
    if (score == -1)
        return "-";
    if (score <= -2)
        return "--";

    return "";
}

static void ability_browser_build_skill_tokens(int skill_options,
    int skill_cur, bool full_labels, char tokens[][40], int token_widths[])
{
    for (int skill = 0; skill < skill_options; skill++)
    {
        cptr base_label = full_labels ? skill_names_full[skill]
                                      : skill_names[skill];
        cptr suffix = ability_browser_skill_trait_suffix(skill);
        char label[32];

        strnfmt(label, sizeof(label), "%s%s", base_label, suffix);
        if (skill == skill_cur)
            strnfmt(tokens[skill], 40, "[%s]", label);
        else
            strnfmt(tokens[skill], 40, " %s ", label);
        token_widths[skill] = ability_browser_cell_width_n(tokens[skill],
            (int)strlen(tokens[skill]));
    }
}

static int ability_browser_skill_tab_split(const int token_widths[],
    int skill_options)
{
    int total_width = 0;
    int first_width = 0;
    int best_split = MAX(1, skill_options / 2);
    int best_max_width = 0x7fffffff;
    int best_difference = 0x7fffffff;

    if (skill_options < 2)
        return skill_options;

    for (int skill = 0; skill < skill_options; skill++)
        total_width += token_widths[skill];

    for (int split = 1; split < skill_options; split++)
    {
        int second_width;
        int max_width;
        int difference;

        first_width += token_widths[split - 1];
        second_width = total_width - first_width;
        max_width = MAX(first_width, second_width);
        difference = (first_width > second_width)
            ? first_width - second_width
            : second_width - first_width;

        if (max_width < best_max_width
            || (max_width == best_max_width
                && difference < best_difference))
        {
            best_split = split;
            best_max_width = max_width;
            best_difference = difference;
        }
    }

    return best_split;
}

static int ability_browser_skill_tab_width(const int token_widths[],
    int skill_options, int split)
{
    int first_width = 0;
    int second_width = 0;

    for (int skill = 0; skill < skill_options; skill++)
    {
        if (skill < split)
            first_width += token_widths[skill];
        else
            second_width += token_widths[skill];
    }

    return MAX(first_width, second_width);
}

static void ability_browser_draw_skill_summary(
    const ability_browser_layout* layout, int skill_options, int skill_cur,
    int skill_hover)
{
    char tokens[S_MAX][40];
    int token_widths[S_MAX];
    int end_col = layout->skill_col + layout->skill_w;
    int split = skill_options;
    bool full_labels = (layout->skill_rows > 1 || layout->skill_w >= 78);
    int col = layout->skill_col;
    int tab_row = layout->skill_row;

    ability_browser_build_skill_tokens(skill_options, skill_cur, full_labels,
        tokens, token_widths);
    if (layout->skill_rows > 1)
        split = ability_browser_skill_tab_split(token_widths, skill_options);

    if (ability_browser_skill_tab_width(token_widths, skill_options, split)
        > layout->skill_w && full_labels)
    {
        full_labels = false;
        ability_browser_build_skill_tokens(skill_options, skill_cur,
            full_labels, tokens, token_widths);
        if (layout->skill_rows > 1)
            split = ability_browser_skill_tab_split(token_widths,
                skill_options);
    }

    for (int row = 0; row < layout->skill_rows; row++)
        Term_erase(layout->skill_col, tab_row + row, layout->skill_w);
    for (int skill = 0; skill < skill_options; skill++)
    {
        bool hovered = (skill == skill_hover);
        byte attr = ability_browser_skill_attr(skill, hovered);
        int token_len = (int)strlen(tokens[skill]);

        if (layout->skill_rows > 1 && skill == split)
        {
            tab_row++;
            col = layout->skill_col;
        }

        if (col + token_widths[skill] > end_col)
            break;

        Term_putstr(col, tab_row, token_len, attr, tokens[skill]);
        ui_menu_click_add(ABILITY_MENU_CLICK_SKILL_BASE + skill,
            col, tab_row, token_widths[skill]);
        col += token_widths[skill];
    }
}

/*
 * For an ability the player has turned on, determine whether it currently does
 * nothing because of the player's loadout (active weapon, off-hand weapon, or
 * armour weight). Returns true and fills `buf` with a short reason if so.
 *
 * Only stable, loadout-based conditions are reported here (not transient
 * per-turn state such as "moved last turn"), so the menu marking does not flip
 * while the player is browsing.
 */
static bool ability_inactive_reason(int skilltype, int abilitynum,
    char* buf, size_t buflen)
{
    cptr reason = NULL;

    switch (skilltype)
    {
    case S_MEL:
        switch (abilitynum)
        {
        case MEL_POWER:
        case MEL_FINESSE:
        case MEL_KNOCK_BACK:
        case MEL_POLEARMS:
        case MEL_CHARGE:
        case MEL_FOLLOW_THROUGH:
        case MEL_IMPALE:
        case MEL_CONTROL:
        case MEL_WHIRLWIND_ATTACK:
        case MEL_ZONE_OF_CONTROL:
        case MEL_SMITE:
        case MEL_RAPID_ATTACK:
            if (!player_active_weapon_is_melee())
                reason = "Requires your melee weapon to be active.";
            break;
        case MEL_POWER_THROW:
            if (!player_active_weapon_is_melee())
            {
                reason = "Requires your melee weapon to be active.";
            }
            else if (!inventory[INVEN_WIELD].k_idx)
            {
                reason = "Requires an equipped melee weapon.";
            }
            else if (!player_power_throw_weapon_eligible(
                         &inventory[INVEN_QUIVER1])
                && !player_power_throw_weapon_eligible(
                    &inventory[INVEN_QUIVER2]))
            {
                reason = "Requires a spear or hand axe in a quiver.";
            }
            break;
        case MEL_TWO_WEAPON:
            if (!player_active_weapon_is_melee())
            {
                reason = "Requires your melee weapon to be active.";
            }
            else
            {
                object_type* off = &inventory[INVEN_ARM];
                bool has_offhand_weapon = off->k_idx
                    && ((off->tval == TV_SWORD) || (off->tval == TV_POLEARM)
                        || (off->tval == TV_HAFTED)
                        || (off->tval == TV_DIGGING));

                if (!has_offhand_weapon)
                    reason = "Requires a second weapon in your off hand.";
            }
            break;
        default:
            break;
        }
        break;

    case S_EVN:
        switch (abilitynum)
        {
        case EVN_DODGING:
        case EVN_FLANKING:
            if (!wearing_only_light_armour())
                reason = "Requires wearing only light armour.";
            break;
        case EVN_PARRY:
            if (!player_active_weapon_is_melee())
                reason = "Requires your melee weapon to be active.";
            break;
        default:
            break;
        }
        break;

    case S_ARC:
        switch (abilitynum)
        {
        case ARC_ROUT:
        case ARC_POINT_BLANK:
        case ARC_PUNCTURE:
        case ARC_AMBUSH:
        case ARC_CRIPPLING:
        case ARC_DEADLY_HAIL:
            if (!player_active_weapon_is_ranged())
                reason = "Requires your ranged weapon to be active.";
            break;
        case ARC_SKIRMISHING:
            if (!player_active_weapon_is_ranged())
                reason = "Requires your ranged weapon to be active.";
            else if (!wearing_only_light_armour())
                reason = "Requires wearing only light armour.";
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    if (reason && buf && buflen)
        SDL_strlcpy(buf, reason, buflen);

    return (reason != NULL);
}

/*
 * Whether an owned, switched-on ability is currently inert due to loadout.
 */
static bool ability_is_blocked(int skilltype, int abilitynum)
{
    if (!p_ptr->have_ability[skilltype][abilitynum])
        return (false);
    if (!p_ptr->active_ability[skilltype][abilitynum])
        return (false);

    return ability_inactive_reason(skilltype, abilitynum, NULL, 0);
}

static byte ability_browser_entry_attr(int skilltype, int abilitynum)
{
    if (p_ptr->have_ability[skilltype][abilitynum])
    {
        if (p_ptr->active_ability[skilltype][abilitynum]
            && ability_is_blocked(skilltype, abilitynum))
            return TERM_ORANGE;

        if (p_ptr->innate_ability[skilltype][abilitynum])
            return p_ptr->active_ability[skilltype][abilitynum]
                ? TERM_WHITE
                : TERM_RED;

        return p_ptr->active_ability[skilltype][abilitynum]
            ? TERM_L_GREEN
            : TERM_RED;
    }

    return prereqs(skilltype, abilitynum) ? TERM_SLATE : TERM_L_DARK;
}

static void ability_browser_sort_entries(ability_browser_entry entries[],
    int count)
{
    for (int i = 1; i < count; i++)
    {
        ability_browser_entry entry = entries[i];
        int j = i - 1;

        while (j >= 0
            && (entry.b_ptr->level < entries[j].b_ptr->level
                || (entry.b_ptr->level == entries[j].b_ptr->level
                    && entry.abilitynum < entries[j].abilitynum)))
        {
            entries[j + 1] = entries[j];
            j--;
        }

        entries[j + 1] = entry;
    }
}

static int ability_browser_collect_entries(int skilltype,
    ability_browser_entry entries[], int max_entries)
{
    int count = 0;

    for (int i = 0; i < z_info->b_max; i++)
    {
        ability_type* b_ptr = &b_info[i];
        ability_browser_entry* entry;

        if (!b_ptr->name)
            continue;
        if (b_ptr->skilltype != skilltype)
            continue;
        if (b_ptr->abilitynum >= ABILITIES_MAX)
            continue;
        if (skilltype == S_SPC
            && !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            continue;
        if (skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH)
            continue;
        if (count >= max_entries)
            break;

        entry = &entries[count++];
        entry->b_ptr = b_ptr;
        entry->abilitynum = b_ptr->abilitynum;
        entry->attr = ability_browser_entry_attr(skilltype, b_ptr->abilitynum);

        if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
            && (p_ptr->bane_type > 0))
        {
            strnfmt(entry->name, sizeof(entry->name), "%s-%s",
                bane_name[p_ptr->bane_type], b_name + b_ptr->name);
        }
        else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
            && (p_ptr->oath_type > 0))
        {
            strnfmt(entry->name, sizeof(entry->name), "%s: %s",
                b_name + b_ptr->name, oath_name_short(p_ptr->oath_type));
        }
        else
        {
            SDL_strlcpy(entry->name, b_name + b_ptr->name,
                sizeof(entry->name));
        }
    }

    if (skilltype == S_SMT || skilltype == S_ARC || skilltype == S_MEL
        || skilltype == S_PER)
        ability_browser_sort_entries(entries, count);

    return count;
}

static void ability_browser_entry_state(char* buf, size_t buflen,
    int skilltype, const ability_browser_entry* entry)
{
    int abilitynum;

    if (!buf || buflen == 0 || !entry)
        return;

    abilitynum = entry->abilitynum;

    if (p_ptr->have_ability[skilltype][abilitynum])
    {
        if ((skilltype != S_SPC)
            && p_ptr->active_ability[skilltype][abilitynum]
            && ability_is_blocked(skilltype, abilitynum))
        {
            SDL_strlcpy(buf, "idle", buflen);
        }
        else if (!p_ptr->innate_ability[skilltype][abilitynum])
            SDL_strlcpy(buf,
                p_ptr->active_ability[skilltype][abilitynum] ? "item" : "off",
                buflen);
        else if (skilltype == S_SPC)
            SDL_strlcpy(buf,
                p_ptr->active_ability[skilltype][abilitynum] ? "grant" : "lost",
                buflen);
        else
            SDL_strlcpy(buf,
                p_ptr->active_ability[skilltype][abilitynum] ? "on" : "off",
                buflen);
        return;
    }

    if (skilltype == S_SPC)
    {
        SDL_strlcpy(buf, "grant", buflen);
        return;
    }

    if (prereqs(skilltype, abilitynum))
        strnfmt(buf, buflen, "%d XP", ability_purchase_exp_cost(skilltype));
    else
        SDL_strlcpy(buf, "locked", buflen);
}

static void ability_browser_draw_ability_list(
    const ability_browser_layout* layout, int skilltype,
    const ability_browser_entry entries[], int entry_count, int entry_cur,
    int entry_top, bool focused)
{
    int prefix_w = indexed_menu_letters_enabled() ? 3 : 2;
    int level_col = layout->ability_col + prefix_w;
    int name_col = level_col + 4;
    int state_col = layout->ability_col + layout->ability_w - 8;
    int name_w = state_col - name_col - 1;
    int entry_rows = MAX(layout->ability_entry_rows, 1);
    int visible_entries = MAX(layout->ability_rows / entry_rows, 1);

    if (name_w < 1)
        name_w = 1;
    if (state_col <= name_col)
        state_col = layout->ability_col + layout->ability_w;

    ability_browser_put_fitted(layout->ability_col, layout->header_row,
        layout->ability_w, TERM_SLATE, "Lvl Ability                       State");

    for (int i = 0; i < visible_entries; i++)
    {
        int idx = entry_top + i;
        int y = layout->ability_row + i * entry_rows;
        const ability_browser_entry* entry;
        char state[32];
        char level[8];
        char prefix[8];
        bool selected;
        bool highlighted;
        byte row_attr;
        byte prefix_attr;
        byte level_attr;
        byte state_attr;
        cptr name_cursor;

        if (idx >= entry_count)
            break;

        entry = &entries[idx];
        selected = (idx == entry_cur);
        highlighted = selected && focused;
        row_attr = highlighted ? ability_browser_selected_attr(entry->attr)
                               : entry->attr;
        prefix_attr = highlighted ? row_attr
            : (selected ? TERM_L_BLUE : TERM_L_DARK);
        level_attr = highlighted ? row_attr : TERM_SLATE;
        state_attr = highlighted ? row_attr : entry->attr;

        for (int line_idx = 0; line_idx < entry_rows; line_idx++) {
            ability_browser_fill_row(layout->ability_col, y + line_idx,
                layout->ability_w, highlighted ? row_attr : TERM_DARK);
            ui_menu_click_add(ABILITY_MENU_CLICK_ABILITY_BASE + idx,
                layout->ability_col, y + line_idx, layout->ability_w);
        }

        if (selected)
            indexed_menu_focus_prefix(prefix, sizeof(prefix), idx);
        else
            indexed_menu_normal_prefix(prefix, sizeof(prefix), idx);
        Term_putstr(layout->ability_col, y, prefix_w, prefix_attr, prefix);

        strnfmt(level, sizeof(level), "%2d", entry->b_ptr->level);
        ability_browser_put_fitted(level_col, y, 2, level_attr, level);
        name_cursor = entry->name;
        for (int line_idx = 0; line_idx < entry_rows; line_idx++) {
            char name_line[96];

            if (!ability_browser_wrap_next(&name_cursor, name_w, name_line,
                    sizeof(name_line)))
            {
                name_line[0] = '\0';
            }
            ability_browser_put_fitted(name_col, y + line_idx, name_w,
                row_attr, name_line);
        }

        ability_browser_entry_state(state, sizeof(state), skilltype, entry);
        ability_browser_put_fitted(state_col, y,
            layout->ability_col + layout->ability_w - state_col,
            state_attr, state);

    }
}

static bool ability_browser_song_bonus_text(const ability_type* b_ptr,
    char* bonus_text, size_t text_size)
{
    int song_skill = ability_menu_current_song_score();

    if (!b_ptr || !bonus_text || text_size == 0)
        return false;

    bonus_text[0] = '\0';

    switch (b_ptr->abilitynum)
    {
    case SNG_ELBERETH:
    {
        int will_penalty = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(bonus_text, text_size,
            "Current effect: enemy Will -%d.", will_penalty);
        break;
    }
    case SNG_CHALLENGE:
    {
        int debuff = (song_skill > 0) ? MAX(1, song_skill / 5) : 0;
        strnfmt(bonus_text, text_size,
            "Current effect: enemy Will and Stealth -%d.", debuff);
        break;
    }
    case SNG_DELVINGS:
    {
        strnfmt(bonus_text, text_size,
            "Current effect: delving range %d squares.", song_skill + 8);
        break;
    }
    case SNG_FREEDOM:
    {
        SDL_strlcpy(bonus_text,
            "Current effect: +1 free action while singing.", text_size);
        break;
    }
    case SNG_SILENCE:
    {
        int silence_bonus = song_skill / 2;
        int enemy_song_penalty = silence_bonus / 2;
        strnfmt(bonus_text, text_size,
            "Current effect: +%d to hush/noise checks; enemy songs -%d.",
            silence_bonus, enemy_song_penalty);
        break;
    }
    case SNG_STAUNCHING:
    {
        int base_heal = song_skill / 12;
        int extra_turns = song_skill % 12;

        if (extra_turns > 0)
        {
            strnfmt(bonus_text, text_size,
                "Current effect: stops bleeding and heals %d HP/turn, with +1 extra on %d turns in 12.",
                base_heal, extra_turns);
        }
        else
        {
            strnfmt(bonus_text, text_size,
                "Current effect: stops bleeding and heals %d HP/turn.",
                base_heal);
        }
        break;
    }
    case SNG_THRESHOLDS:
    {
        SDL_strlcpy(bonus_text,
            "Current effect: closes doors as warded barriers.", text_size);
        break;
    }
    case SNG_TREES:
    {
        int light_radius = ability_menu_stepped_song_bonus(song_skill, 5, 6);
        strnfmt(bonus_text, text_size,
            "Current effect: +%d light radius.", light_radius);
        break;
    }
    case SNG_WOVEN_THEMES:
    {
        int minor_skill = ability_menu_minor_song_score(song_skill);
        int synergy_bonus = ability_menu_song_synergy_bonus(song_skill);
        strnfmt(bonus_text, text_size,
            "Current effect: a minor theme uses Song %d; a valid synergy pair adds +%d Song. Minor themes pay their normal Voice cost.",
            minor_skill, synergy_bonus);
        break;
    }
    case SNG_SLAYING:
    {
        int hp_threshold = song_skill * 2;
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_HURIN)
            hp_threshold *= 2;

        strnfmt(bonus_text, text_size,
            "Current effect: criticals can slay foes at %d HP or less.",
            hp_threshold);
        break;
    }
    case SNG_REVEALING:
    {
        strnfmt(bonus_text, text_size,
            "Current effect: rolls to reveal monsters/items within %d squares; revealed, carried, and equipped items get +1d5 identification.",
            (song_skill / 2) + 8);
        break;
    }
    case SNG_ELVENESS:
    {
        int evasion_bonus = ability_menu_stepped_song_bonus(song_skill, 7, 8);
        strnfmt(bonus_text, text_size,
            "Current effect: +1 Grace and +%d Evasion.", evasion_bonus);
        break;
    }
    case SNG_STAYING:
    {
        int will_bonus = song_skill / 2;
        int protection_dice = 2;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_FIN)
        {
            will_bonus = song_skill * 2;
            protection_dice = 4;
        }

        strnfmt(bonus_text, text_size,
            "Current effect: +%d Will and [%dd2] protection.",
            will_bonus, protection_dice);
        break;
    }
    case SNG_DISGUISE:
    {
        int disguise_bonus = song_skill + 5;
        const char* extra = (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_TURGON)
            ? " + Perception"
            : "";

        strnfmt(bonus_text, text_size,
            "Current effect: disguise checks use %d + Will%s.",
            disguise_bonus, extra);
        break;
    }
    case SNG_LORIEN:
    {
        int sleep_score = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_LUT)
            sleep_score = (3 * song_skill) / 2;

        strnfmt(bonus_text, text_size,
            "Current effect: sleep checks use %d.", sleep_score);
        break;
    }
    case SNG_SHATTERING:
    {
        strnfmt(bonus_text, text_size,
            "Current effect: each successful shatter has a %d%% weaken chance.",
            song_skill / 3);
        break;
    }
    case SNG_MASTERY:
    {
        int mastery_bonus = song_skill;

        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_THINGOL)
            mastery_bonus = (7 * song_skill) / 4;

        strnfmt(bonus_text, text_size,
            "Current effect: mastery rolls are 2d8 + %d.", mastery_bonus);
        break;
    }
    case SNG_GRA:
    {
        SDL_strlcpy(bonus_text, "Current effect: +1 Grace.", text_size);
        break;
    }
    case SNG_CONTEST:
    {
        int will_penalty = MAX(1, song_skill / 3);
        int stealth_penalty = MAX(1, song_skill / 2);
        int evasion_penalty = MAX(1, song_skill / 5);
        int armour_penalty = MAX(1, song_skill / 12);

        strnfmt(bonus_text, text_size,
            "Current effect: duel checks add Will/2; victory inflicts -%d Will, -%d Stealth, -%d Evasion, -%d armour die.",
            will_penalty, stealth_penalty, evasion_penalty, armour_penalty);
        break;
    }
    case SNG_LAMENT:
    {
        int will_penalty = MAX(1, song_skill / 2);
        int attrition_steps = MAX(1, song_skill / 12);

        strnfmt(bonus_text, text_size,
            "Current effect: duel checks add Will/2; victory inflicts -%d Will and -%d health/damage steps.",
            will_penalty, attrition_steps);
        break;
    }
    default:
        break;
    }

    if (!bonus_text[0])
        return false;

    ability_menu_append_song_cost(bonus_text, text_size, b_ptr);
    return true;
}

static int ability_browser_oath_for_special(int abilitynum)
{
    switch (abilitynum)
    {
    case SPC_OATH_MERCY: return OATH_MERCY;
    case SPC_OATH_SILENCE: return OATH_SILENCE;
    case SPC_OATH_IRON: return OATH_IRON;
    case SPC_OATH_SMITH: return OATH_SMITH;
    case SPC_OATH_VALOROUS: return OATH_VALOROUS;
    case SPC_OATH_LIGHT: return OATH_LIGHT;
    default: return 0;
    }
}

static void ability_browser_add_prerequisites(
    ability_browser_desc_line lines[], int* line_count, int skilltype,
    const ability_type* b_ptr, int width)
{
    char buf[160];

    if (!b_ptr)
        return;

    ability_desc_add_blank(lines, line_count);
    ability_desc_add_line(lines, line_count, TERM_YELLOW, "Requirements");

    strnfmt(buf, sizeof(buf), "Skill: %d %s base (%d now)",
        b_ptr->level, skill_names_full[skilltype],
        p_ptr->skill_base[skilltype]);
    ability_desc_add_wrapped(lines, line_count,
        (b_ptr->level <= p_ptr->skill_base[skilltype]) ? TERM_L_GREEN
                                                       : TERM_L_DARK,
        buf, width);

    if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
    {
        for (int j = 0; j < b_ptr->prereqs; j++)
        {
            ability_type* prereq = &b_info[ability_index(
                b_ptr->prereq_skilltype[j], b_ptr->prereq_abilitynum[j])];
            byte attr = p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                           [b_ptr->prereq_abilitynum[j]]
                ? TERM_L_GREEN
                : TERM_L_DARK;

            strnfmt(buf, sizeof(buf), "%s%s",
                (j == 0) ? "Ability: " : "or ",
                b_name + prereq->name);
            ability_desc_add_wrapped(lines, line_count, attr, buf, width);
        }
    }
    else if (b_ptr->prereqs > 0)
    {
        ability_desc_add_line(lines, line_count, TERM_GREEN,
            "Ability: Quick Study bypasses prerequisites");
    }

    if (skilltype != S_SPC
        && !p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && prereqs(skilltype, b_ptr->abilitynum))
    {
        int exp_cost = ability_purchase_exp_cost(skilltype);

        strnfmt(buf, sizeof(buf), "Price: %d XP (%ld available)",
            exp_cost, (long)p_ptr->new_exp);
        ability_desc_add_wrapped(lines, line_count,
            (exp_cost <= p_ptr->new_exp) ? TERM_L_GREEN : TERM_L_DARK,
            buf, width);
    }
}

static void ability_browser_add_current_blocks(
    ability_browser_desc_line lines[], int* line_count, int skilltype,
    const ability_type* b_ptr, int width)
{
    char buf[320];

    if (!b_ptr)
        return;

    if (skilltype == S_SNG)
    {
        char bonus_text[384];

        if (ability_browser_song_bonus_text(b_ptr, bonus_text,
                sizeof(bonus_text)))
        {
            ability_desc_add_blank(lines, line_count);
            ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN,
                bonus_text, width);
        }
    }

    if (ability_menu_dynamic_bonus_text(skilltype, b_ptr->abilitynum,
            buf, sizeof(buf)))
    {
        ability_desc_add_blank(lines, line_count);
        ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN, buf,
            width);
    }

    if (skilltype == S_SPC && b_ptr->abilitynum == SPC_NIENA_MERCY
        && p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY])
    {
        int total_seen = 0;
        int total_killed = 0;

        for (int i = 1; i < z_info->r_max; i++)
        {
            monster_lore* l_ptr = &l_list[i];
            monster_race* r_ptr = &r_info[i];

            if (r_ptr->flags1 & RF1_UNIQUE)
                continue;

            total_seen += l_ptr->psights;
            total_killed += l_ptr->pkills;
        }

        if (total_seen > 0)
        {
            int mercy_ratio_times_10 = 10 * (total_seen - total_killed);
            int stealth_bonus =
                (mercy_ratio_times_10 + total_seen - 1) / total_seen;

            strnfmt(buf, sizeof(buf),
                "Current bonus: +%d stealth (%d seen, %d spared)",
                stealth_bonus, total_seen, total_seen - total_killed);
        }
        else
        {
            SDL_strlcpy(buf,
                "Current bonus: +0 stealth (no monsters encountered yet)",
                sizeof(buf));
        }

        ability_desc_add_blank(lines, line_count);
        ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN, buf, width);
    }

    if ((skilltype == S_EVN) && (b_ptr->abilitynum == EVN_HEAVY_ARMOUR))
    {
        int armour_weight = heavy_armour_desc_current_weight();
        int protection_bonus = armour_weight / 150;
        int evasion_bonus = heavy_armour_desc_current_evasion_bonus();
        bool learned = p_ptr->have_ability[skilltype][b_ptr->abilitynum];

        strnfmt(buf, sizeof(buf),
            learned
                ? "Current bonus: +%d protection vs physical attacks and %+d evasion (%d.%d lb counted)"
                : "With current equipment: +%d protection vs physical attacks and %+d evasion (%d.%d lb counted)",
            protection_bonus, evasion_bonus, armour_weight / 10,
            armour_weight % 10);
        ability_desc_add_blank(lines, line_count);
        ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN, buf, width);
    }

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && (skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
        && (p_ptr->bane_type > 0))
    {
        int killed = bane_type_killed(p_ptr->bane_type);
        int current_bonus = bane_bonus_aux();
        int threshold = 2;

        while (threshold <= killed)
            threshold *= 2;

        ability_desc_add_blank(lines, line_count);
        strnfmt(buf, sizeof(buf), "%s-Bane: %d slain, %+d bonus",
            bane_name[p_ptr->bane_type], killed, current_bonus);
        ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN, buf, width);
        if (threshold <= 64 || current_bonus == 0)
        {
            strnfmt(buf, sizeof(buf), "Next bonus at %d slain.", threshold);
            ability_desc_add_wrapped(lines, line_count, TERM_SLATE, buf, width);
        }
    }

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && (skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
        && (p_ptr->oath_type > 0))
    {
        ability_desc_add_blank(lines, line_count);
        strnfmt(buf, sizeof(buf), "Oath: %s",
            oath_name_short(p_ptr->oath_type));
        ability_desc_add_wrapped(lines, line_count, TERM_L_BLUE, buf, width);
        strnfmt(buf, sizeof(buf), "You have sworn not to %s.",
            oath_desc2_short(p_ptr->oath_type));
        ability_desc_add_wrapped(lines, line_count, TERM_L_WHITE, buf, width);

        if (oath_invalid(p_ptr->oath_type))
            ability_desc_add_wrapped(lines, line_count, TERM_RED,
                "You are an oathbreaker.", width);
        else
        {
            strnfmt(buf, sizeof(buf), "Bonus: %s.",
                oath_reward_short(p_ptr->oath_type));
            ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN, buf,
                width);
        }
    }

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && (skilltype == S_SPC) && (b_ptr->abilitynum == SPC_UNIQUE_BANE))
    {
        int uniques_killed = unique_bane_type_killed();
        int current_bonus = 0;
        int threshold = 2;

        while (threshold <= uniques_killed)
        {
            threshold *= 2;
            current_bonus++;
        }

        ability_desc_add_blank(lines, line_count);
        strnfmt(buf, sizeof(buf), "Unique Bane: %d uniques slain, %+d bonus",
            uniques_killed, current_bonus);
        ability_desc_add_wrapped(lines, line_count, TERM_L_GREEN, buf, width);
        if (threshold <= 64 || current_bonus == 0)
        {
            strnfmt(buf, sizeof(buf), "Next bonus at %d uniques.", threshold);
            ability_desc_add_wrapped(lines, line_count, TERM_SLATE, buf, width);
        }
    }
}

static int ability_browser_build_description(int skilltype,
    const ability_browser_entry* entry, ability_browser_desc_line lines[],
    int width)
{
    ability_type* b_ptr;
    char status[160];
    char desc_controller_text[2048];
    char effect_controller_text[2048];
    const char* desc_text;
    const char* effect_text;
    int line_count = 0;
    int oath_id;

    if (!entry || !entry->b_ptr)
    {
        ability_desc_add_line(lines, &line_count, TERM_L_DARK,
            "No abilities available for this skill.");
        return line_count;
    }

    b_ptr = entry->b_ptr;
    ability_desc_add_line(lines, &line_count, TERM_YELLOW, entry->name);

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
    {
        strnfmt(status, sizeof(status), "%s; %s.",
            p_ptr->innate_ability[skilltype][b_ptr->abilitynum]
                ? "Learned"
                : "Granted by equipment",
            p_ptr->active_ability[skilltype][b_ptr->abilitynum]
                ? "active"
                : "inactive");
    }
    else if (skilltype == S_SPC)
    {
        SDL_strlcpy(status, "Special ability; granted by quests or story.",
            sizeof(status));
    }
    else
    {
        strnfmt(status, sizeof(status), "%s. Price %d XP.",
            prereqs(skilltype, b_ptr->abilitynum) ? "Available" : "Locked",
            ability_purchase_exp_cost(skilltype));
    }
    ability_desc_add_wrapped(lines, &line_count, TERM_SLATE, status, width);

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
        && p_ptr->active_ability[skilltype][b_ptr->abilitynum])
    {
        char reason[128];

        if (ability_inactive_reason(skilltype, b_ptr->abilitynum, reason,
                sizeof(reason)))
        {
            char blocked[160];

            ability_desc_add_blank(lines, &line_count);
            strnfmt(blocked, sizeof(blocked), "Not active now: %s", reason);
            ability_desc_add_wrapped(lines, &line_count, TERM_ORANGE, blocked,
                width);
        }
    }

    oath_id = (skilltype == S_SPC)
        ? ability_browser_oath_for_special(b_ptr->abilitynum)
        : 0;
    if (oath_id > 0 && oath_invalid(oath_id))
    {
        char* death_message = oath_death_message(oath_id);

        ability_desc_add_blank(lines, &line_count);
        ability_desc_add_line(lines, &line_count, TERM_RED, "Broken oath");
        if (death_message && death_message[0])
            ability_desc_add_wrapped(lines, &line_count, TERM_RED,
                death_message, width);
    }
    else
    {
        effect_text = b_ptr->effect
            ? ability_menu_controller_text(b_text + b_ptr->effect,
                effect_controller_text, sizeof(effect_controller_text))
            : NULL;
        desc_text = b_ptr->text
            ? ability_menu_controller_text(b_text + b_ptr->text,
                desc_controller_text, sizeof(desc_controller_text))
            : NULL;

        if (effect_text && effect_text[0])
        {
            ability_desc_add_blank(lines, &line_count);
            ability_desc_add_line(lines, &line_count, TERM_YELLOW, "Effect");
            ability_desc_add_wrapped(lines, &line_count, TERM_L_WHITE,
                effect_text, width);
        }

        ability_browser_add_current_blocks(lines, &line_count, skilltype,
            b_ptr, width);

        if (desc_text && desc_text[0])
        {
            ability_desc_add_blank(lines, &line_count);
            ability_desc_add_line(lines, &line_count, TERM_YELLOW, "Lore");
            ability_desc_add_wrapped(lines, &line_count, TERM_SLATE,
                desc_text, width);
        }
    }

    ability_browser_add_prerequisites(lines, &line_count, skilltype, b_ptr,
        width);

    return line_count;
}

static bool ability_browser_word_boundary(char ch)
{
    return (ch == '\0') || !isalnum((unsigned char)ch);
}

static bool ability_browser_phrase_matches_exact(cptr line, int offset,
    cptr phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strlen(line + offset) < len)
        return false;
    if (strncmp(line + offset, phrase, len) != 0)
        return false;
    if (offset > 0 && !ability_browser_word_boundary(line[offset - 1]))
        return false;

    return ability_browser_word_boundary(line[offset + len]);
}

static bool ability_browser_phrase_matches_ci(cptr line, int offset,
    cptr phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strlen(line + offset) < len)
        return false;
    if (SDL_strncasecmp(line + offset, phrase, len) != 0)
        return false;
    if (offset > 0 && !ability_browser_word_boundary(line[offset - 1]))
        return false;

    return ability_browser_word_boundary(line[offset + len]);
}

static int ability_browser_number_match(cptr line, int offset)
{
    int i = offset;

    if (!line || offset < 0 || !line[offset])
        return 0;

    if ((line[i] == '+' || line[i] == '-') && isdigit((unsigned char)line[i + 1]))
        i++;
    else if (!isdigit((unsigned char)line[i]))
        return 0;

    while (isdigit((unsigned char)line[i]))
        i++;

    if (line[i] == '.' && isdigit((unsigned char)line[i + 1]))
    {
        i++;
        while (isdigit((unsigned char)line[i]))
            i++;
    }

    return i - offset;
}

static int ability_browser_highlight_match(cptr line, int offset, byte* attr,
    bool color_skills)
{
    static const struct {
        cptr phrase;
        byte attr;
    } rules[] = {
        { "Learned", TERM_L_GREEN },
        { "Available", TERM_L_GREEN },
        { "active", TERM_L_GREEN },
        { "Current bonus", TERM_L_GREEN },
        { "Current effect", TERM_L_GREEN },
        { "Bonus", TERM_L_GREEN },
        { "Price", TERM_WHITE },
        { "price", TERM_WHITE },
        { "XP", TERM_L_GREEN },
        { "Locked", TERM_RED },
        { "inactive", TERM_RED },
        { "lost", TERM_RED },
        { "Broken oath", TERM_RED },
        { "oathbreaker", TERM_RED },
        { "Granted", TERM_L_BLUE },
        { "grant", TERM_L_BLUE },
        { "Special", TERM_VIOLET }
    };

    if (!line || offset < 0 || !line[offset] || !attr)
        return 0;

    {
        int number_len = ability_browser_number_match(line, offset);

        if (number_len > 0)
        {
            *attr = TERM_L_GREEN;
            return number_len;
        }
    }

    if (color_skills)
    {
        for (int skill = 0; skill < S_MAX; skill++)
        {
            cptr full_name = skill_names_full[skill];
            cptr short_name = skill_names[skill];

            if (ability_browser_phrase_matches_exact(line, offset, full_name))
            {
                *attr = ability_skill_color(skill);
                return (int)strlen(full_name);
            }
            if (ability_browser_phrase_matches_exact(line, offset, short_name))
            {
                *attr = ability_skill_color(skill);
                return (int)strlen(short_name);
            }
        }
    }

    for (size_t i = 0; i < N_ELEMENTS(rules); i++)
    {
        if (ability_browser_phrase_matches_ci(line, offset, rules[i].phrase))
        {
            *attr = rules[i].attr;
            return (int)strlen(rules[i].phrase);
        }
    }

    return 0;
}

static void ability_browser_draw_colored_text_line_ex(int col, int row,
    int width, byte base_attr, cptr text, bool color_skills)
{
    char fitted[ABILITY_BROWSER_DESC_LINE_LEN];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;
    int display_limit;
    int display_cursor;
    int fitted_len;
    int offset = 0;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;

    ability_browser_fit_text(fitted, sizeof(fitted), text, width);

    display_limit = col + width;
    display_cursor = col;
    fitted_len = (int)strlen(fitted);
    while (fitted[offset] && display_cursor < display_limit)
    {
        byte attr = base_attr;
        int write_col = col + offset;
        int byte_room;
        int remaining_bytes;
        int run_len;
        int run_width;
        int match_len = ability_browser_highlight_match(fitted, offset, &attr,
            color_skills);

        if (write_col >= term_wid)
            break;

        byte_room = term_wid - write_col;
        remaining_bytes = fitted_len - offset;
        if (match_len > 0)
        {
            run_len = match_len;
        }
        else
        {
            run_len = utf8_sequence_len_n(fitted + offset, remaining_bytes);
            attr = base_attr;
        }

        if (run_len <= 0)
            break;
        if (run_len > byte_room)
            run_len = utf8_safe_prefix_len(fitted + offset, byte_room);
        if (run_len <= 0)
            break;

        run_width = ability_browser_cell_width_n(fitted + offset, run_len);
        if (run_width > 0 && display_cursor + run_width > display_limit)
            break;

        Term_putstr(write_col, row, run_len, attr, fitted + offset);
        display_cursor += run_width;
        offset += run_len;
    }
}

static void ability_browser_draw_colored_text_line(int col, int row, int width,
    byte base_attr, cptr text)
{
    ability_browser_draw_colored_text_line_ex(col, row, width, base_attr,
        text, true);
}

static void ability_browser_draw_description(
    const ability_browser_layout* layout,
    const ability_browser_desc_line lines[], int line_count, int desc_top,
    bool focused)
{
    char label[64];
    int visible_rows = layout->desc_rows;
    int header_row = layout->stacked ? layout->desc_header_row
                                     : layout->header_row;

    ability_browser_put_fitted(layout->desc_col, header_row,
        layout->desc_w, focused ? TERM_L_BLUE : TERM_SLATE, "Information");

    if (line_count > visible_rows)
    {
        strnfmt(label, sizeof(label), "[%d-%d/%d]", desc_top + 1,
            MIN(desc_top + visible_rows, line_count), line_count);
        ability_browser_put_fitted(
            layout->desc_col + MAX(0, layout->desc_w - (int)strlen(label)),
            header_row, (int)strlen(label), TERM_L_BLUE, label);
    }

    for (int i = 0; i < visible_rows; i++)
    {
        int idx = desc_top + i;
        int y = layout->desc_row + i;

        Term_erase(layout->desc_col, y, layout->desc_w);
        if (idx >= line_count)
            continue;

        ability_browser_draw_colored_text_line(layout->desc_col, y,
            layout->desc_w,
            lines[idx].attr, lines[idx].text);
    }
}

static void ability_browser_draw_summary(const ability_browser_layout* layout,
    int skilltype, cptr summary, bool train_hovered)
{
    const char* cursor = summary;
    const char* train_start = NULL;
    const char* train_end = NULL;
    char line[180];

    if (!layout || !summary)
        return;

    if (skilltype >= 0 && skilltype < S_MAX && skilltype != S_SPC)
    {
        train_start = strstr(summary, " | ");
        if (train_start)
        {
            train_start += 3;
            train_end = strstr(train_start, " | ability price");
        }
    }

    for (int row_offset = 0; row_offset < layout->summary_rows; row_offset++)
    {
        const char* line_start;
        const char* line_end;
        int row = layout->summary_row + row_offset;

        Term_erase(layout->visible_col, row, layout->visible_w);
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n')
            cursor++;
        line_start = cursor;
        if (!ability_browser_wrap_next(&cursor, layout->visible_w, line,
                sizeof(line)))
        {
            continue;
        }
        line_end = line_start + strlen(line);
        ability_browser_draw_colored_text_line_ex(layout->visible_col, row,
            layout->visible_w, TERM_WHITE, line, false);

        if (train_start && train_end && line_end > train_start
            && line_start < train_end)
        {
            const char* segment_start = (line_start > train_start)
                ? line_start
                : train_start;
            const char* segment_end = (line_end < train_end)
                ? line_end
                : train_end;
            int prefix_bytes = (int)(segment_start - line_start);
            int segment_bytes = (int)(segment_end - segment_start);
            int prefix_w = ability_browser_cell_width_n(line_start,
                prefix_bytes);
            int segment_w = ability_browser_cell_width_n(segment_start,
                segment_bytes);
            int segment_col = layout->visible_col + prefix_w;
            byte train_attr = train_hovered
                ? ability_browser_selected_attr(TERM_L_BLUE)
                : TERM_L_BLUE;

            if (segment_w > 0)
            {
                if (train_hovered)
                    ability_browser_fill_row(segment_col, row, segment_w,
                        train_attr);
                Term_putstr(segment_col, row, segment_bytes, train_attr,
                    segment_start);
                ui_menu_click_add_span(ABILITY_MENU_CLICK_TRAIN, segment_col,
                    row, segment_col + segment_w);
            }
        }
    }
}

static void ability_browser_draw_frame(const ability_browser_layout* layout,
    int skilltype, cptr summary, bool train_hovered)
{
    char title[96];

    Term_clear();

    strnfmt(title, sizeof(title), "Abilities - %s",
        (skilltype >= 0 && skilltype < S_MAX) ? skill_names_full[skilltype]
                                              : "Skills");
    ability_browser_put_fitted(layout->visible_col, layout->title_row,
        layout->visible_w, TERM_L_WHITE + TERM_SHADE, title);
    ability_browser_draw_summary(layout, skilltype, summary, train_hovered);

    for (int i = layout->visible_col;
        i < layout->visible_col + layout->visible_w; i++)
    {
        Term_putch(i, layout->divider_row, TERM_L_DARK, '=');
    }

    for (int i = 0; i < layout->list_rows; i++)
    {
        int y = layout->list_row + i;

        if (layout->skill_divider_col >= 0
            && layout->skill_divider_col < layout->term_wid)
        {
            Term_putch(layout->skill_divider_col, y, TERM_L_DARK, '|');
        }
        if (layout->ability_divider_col >= 0
            && layout->ability_divider_col < layout->term_wid)
        {
            Term_putch(layout->ability_divider_col, y, TERM_L_DARK, '|');
        }
    }
}

static void ability_browser_register_prompt_clicks(
    const ability_browser_layout* layout, cptr prompt)
{
    if (!layout || !prompt)
        return;

    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_ACTION,
        layout->visible_col,
        layout->prompt_row, prompt, "Space");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_ACTION,
        layout->visible_col,
        layout->prompt_row, prompt, "buy/toggle");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_TRAIN, layout->visible_col,
        layout->prompt_row, prompt, "+");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_TRAIN, layout->visible_col,
        layout->prompt_row, prompt, "train");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_NEXT_SKILL,
        layout->visible_col,
        layout->prompt_row, prompt, "Tab");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_PREV_SKILL,
        layout->visible_col,
        layout->prompt_row, prompt, "[/]");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_PREV_SKILL,
        layout->visible_col,
        layout->prompt_row, prompt, "prev skill");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_SCROLL_UP,
        layout->visible_col,
        layout->prompt_row, prompt, "9/3");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_SCROLL_DOWN,
        layout->visible_col,
        layout->prompt_row, prompt, "scroll");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_SKILL_ALLOCATE,
        layout->visible_col,
        layout->prompt_row, prompt, "i skills");
    ui_menu_click_add_text_token(ABILITY_MENU_CLICK_EXIT, layout->visible_col,
        layout->prompt_row, prompt, "Esc");
}

static void ability_browser_draw_prompt(const ability_browser_layout* layout)
{
    char prompt[180];

    Term_erase(layout->visible_col, layout->prompt_row, layout->visible_w);

    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char train_label[16];
        char back_label[16];
        char prompt_full[180];
        char prompt_mid[140];
        char prompt_short[100];
        const char* variants[3];

        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X", train_label,
            sizeof(train_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));

        strnfmt(prompt_full, sizeof(prompt_full),
            "D-pad move/scroll  [%s] buy/toggle  [%s] train  [%s] back",
            confirm_label, train_label, back_label);
        strnfmt(prompt_mid, sizeof(prompt_mid),
            "[%s] buy/toggle  [%s] train  [%s] back",
            confirm_label, train_label, back_label);
        strnfmt(prompt_short, sizeof(prompt_short), "[%s] buy  [%s] back",
            confirm_label, back_label);
        variants[0] = prompt_full;
        variants[1] = prompt_mid;
        variants[2] = prompt_short;
        terminal_prompt_pick_variant(prompt, sizeof(prompt), layout->visible_w,
            false, variants, N_ELEMENTS(variants));
        ability_browser_put_fitted(layout->visible_col, layout->prompt_row,
            layout->visible_w, TERM_L_DARK, prompt);
    }
    else if (sdl_touch_only_device_active())
    {
        /* Keep the tappable action words (buy/toggle, train, prev skill,
         * scroll, i skills) present: ability_browser_register_prompt_clicks()
         * below scans this string for them, so dropping them would remove
         * those tap targets on touch. */
        const char* variants[] = {
            "Tap row to select; selected row buy/toggle, train, prev skill, scroll, i skills",
            "Selected row: buy/toggle, train, scroll, i skills",
            "Selected row buy/toggle"
        };

        terminal_prompt_pick_variant(prompt, sizeof(prompt), layout->visible_w,
            false, variants, N_ELEMENTS(variants));
        ability_browser_put_fitted(layout->visible_col, layout->prompt_row,
            layout->visible_w, TERM_SLATE, prompt);
    }
    else
    {
        const char* variants[] = {
            "Dir move  Tab skill  Space buy/toggle  + train  Pg scroll  i skills  Esc",
            "Dir move  Tab skill  Space buy/toggle  + train  Esc",
            "Space buy/toggle  + train  Esc"
        };

        terminal_prompt_pick_variant(prompt, sizeof(prompt), layout->visible_w,
            false, variants, N_ELEMENTS(variants));
        ability_browser_put_fitted(layout->visible_col, layout->prompt_row,
            layout->visible_w, TERM_SLATE, prompt);
    }

    ability_browser_register_prompt_clicks(layout, prompt);
}

static bool ability_browser_activate_choice(int skilltype, int abilitynum)
{
    int banechoice = -1;
    int oathchoice = -1;
    int highlight3 = 1;
    bool skip_purchase = false;

    if (skilltype < 0 || skilltype >= S_MAX || abilitynum < 0
        || abilitynum >= ABILITIES_MAX)
    {
        return false;
    }

    if (death_spectator_active())
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    if (!p_ptr->have_ability[skilltype][abilitynum])
    {
        ability_type* b_ptr;
        bool has_skill_prereq;
        bool has_ability_prereq;
        int exp_cost;

        if (skilltype == S_SPC)
        {
            bell("This special ability cannot be purchased.");
            return false;
        }

        b_ptr = &b_info[ability_index(skilltype, abilitynum)];
        has_skill_prereq = (p_ptr->skill_base[skilltype] >= b_ptr->level);
        has_ability_prereq = prereq_abilities_met(b_ptr);

        if (!has_skill_prereq)
        {
            bell("Insufficient skill points for ability!");
            return false;
        }

        if (!has_ability_prereq)
        {
            bell("Insufficient prerequisite abilities for ability!");
            return false;
        }

        exp_cost = ability_purchase_exp_cost(skilltype);
        if (exp_cost > p_ptr->new_exp)
        {
            bell("You do not have enough experience to acquire this ability.");
            return false;
        }

        if ((skilltype == S_PER) && (abilitynum == PER_BANE))
        {
            while (true)
            {
                banechoice = bane_menu(&highlight3);
                if (banechoice >= 1 && banechoice <= BANE_TYPES)
                {
                    if (bane_type_killed(banechoice) < 4)
                    {
                        bell("Insufficient kills to become a bane.");
                        continue;
                    }
                    break;
                }

                skip_purchase = true;
                break;
            }
        }

        if (!skip_purchase && (skilltype == S_WIL)
            && (abilitynum == WIL_OATH))
        {
            while (true)
            {
                oathchoice = oath_menu(&highlight3);
                if (oathchoice >= 1 && oathchoice <= OATH_TYPES)
                {
                    if (oath_invalid(oathchoice))
                    {
                        bell("This oath was broken before it was made.");
                        continue;
                    }
                    break;
                }

                skip_purchase = true;
                break;
            }
        }

        if (skilltype == S_SMT && abilitynum == SMT_MASTERPIECE
            && p_ptr->have_ability[S_SPC][SPC_AULE])
        {
            bell("Aule's Forge supersedes Masterpiece; you cannot purchase it.");
            skip_purchase = true;
        }

        if (skip_purchase)
            return false;

        {
            char gain_name[80];
            char prompt[160];

            if (banechoice > 0)
            {
                strnfmt(gain_name, sizeof(gain_name), "%s-%s",
                    bane_name[banechoice], b_name + b_ptr->name);
            }
            else if (oathchoice > 0)
            {
                strnfmt(gain_name, sizeof(gain_name), "%s: %s",
                    b_name + b_ptr->name, oath_name_short(oathchoice));
            }
            else
            {
                SDL_strlcpy(gain_name, b_name + b_ptr->name,
                    sizeof(gain_name));
            }

            strnfmt(prompt, sizeof(prompt), "Gain %s for %d XP? ",
                gain_name, exp_cost);
            if (!get_check(prompt))
                return false;
        }

        p_ptr->innate_ability[skilltype][abilitynum] = true;
        p_ptr->have_ability[skilltype][abilitynum] = true;
        p_ptr->active_ability[skilltype][abilitynum] = true;
        ability_log_record_gain(skilltype, abilitynum);
        p_ptr->new_exp -= exp_cost;

        if (banechoice <= 0 && oathchoice <= 0)
        {
            do_cmd_note(format("(%s)", b_name + b_ptr->name), p_ptr->depth);
        }
        else if (oathchoice <= 0)
        {
            p_ptr->bane_type = banechoice;
            do_cmd_note(format("(%s-%s)", bane_name[banechoice],
                b_name + b_ptr->name), p_ptr->depth);
        }
        else
        {
            int oath_special = -1;

            p_ptr->oath_type = oathchoice;
            switch (oathchoice)
            {
            case OATH_MERCY: oath_special = SPC_OATH_MERCY; break;
            case OATH_SILENCE: oath_special = SPC_OATH_SILENCE; break;
            case OATH_IRON: oath_special = SPC_OATH_IRON; break;
            case OATH_SMITH: oath_special = SPC_OATH_SMITH; break;
            case OATH_VALOROUS: oath_special = SPC_OATH_VALOROUS; break;
            case OATH_LIGHT: oath_special = SPC_OATH_LIGHT; break;
            default: break;
            }

            if (oath_special >= 0)
            {
                p_ptr->have_ability[S_SPC][oath_special] = true;
                p_ptr->innate_ability[S_SPC][oath_special] = true;
                p_ptr->active_ability[S_SPC][oath_special] = true;
                ability_log_record_gain(S_SPC, oath_special);
            }

            do_cmd_note(format("(%s: %s)", b_name + b_ptr->name,
                oath_name_short(oathchoice)), p_ptr->depth);
        }

        msg_print("Ability gained.");
        sdl_quick_access_suggest_ability_shortcut(skilltype, abilitynum);
    }
    else
    {
        if (skilltype == S_SPC
            && (abilitynum == SPC_OATH_MERCY
                || abilitynum == SPC_OATH_SILENCE
                || abilitynum == SPC_OATH_IRON
                || abilitynum == SPC_OATH_SMITH
                || abilitynum == SPC_OATH_VALOROUS
                || abilitynum == SPC_OATH_LIGHT))
        {
            int oath_id = ability_browser_oath_for_special(abilitynum);

            if (p_ptr->active_ability[skilltype][abilitynum])
            {
                msg_print("Sacred oaths cannot be deactivated once sworn.");
                return false;
            }
            if (oath_id > 0 && oath_invalid(oath_id))
            {
                msg_print("Broken oaths cannot be reactivated.");
                return false;
            }

            p_ptr->active_ability[skilltype][abilitynum] = true;
            msg_print("Oath ability reactivated.");
        }
        else if (p_ptr->active_ability[skilltype][abilitynum])
        {
            p_ptr->active_ability[skilltype][abilitynum] = false;
            msg_print("Ability switched off.");

            if ((skilltype == S_SNG) && (abilitynum == SNG_WOVEN_THEMES))
                p_ptr->song2 = SNG_NOTHING;
        }
        else
        {
            p_ptr->active_ability[skilltype][abilitynum] = true;
            msg_print("Ability switched on.");
        }
    }

    p_ptr->redraw |= (PR_EXP | PR_BASIC);
    p_ptr->update |= (PU_BONUS | PU_MANA);
    handle_stuff();
    return true;
}

static int ability_browser_move_skill(int skill_cur, int skill_options,
    int delta)
{
    if (skill_options <= 0)
        return 0;

    skill_cur += delta;
    while (skill_cur < 0)
        skill_cur += skill_options;
    while (skill_cur >= skill_options)
        skill_cur -= skill_options;

    return skill_cur;
}

static void ability_menu_draw_skills(int highlight, int options, int click_base)
{
    int i;
    char buf[80];

    Term_putstr(COL_SKILL, 2, -1, TERM_WHITE, "Skills");

    for (i = 0; i < options; i++)
    {
        int row = i + 4;

        indexed_menu_entry_label(buf, sizeof(buf), i, skill_names_full[i]);
        Term_putstr(COL_SKILL, row, -1,
            (highlight == i + 1) ? TERM_L_BLUE : TERM_WHITE, buf);

        if (click_base > 0)
        {
            ui_menu_click_add(click_base + i,
                indexed_menu_prefix_col(COL_SKILL), row,
                ability_menu_click_width(COL_SKILL, COL_ABILITY, buf));
        }
    }
}

int abilities_menu1(int* highlight)
{
    int ch;
    int options = ability_menu_skill_options();
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    // Clear the whole screen body so compact-layout submenu rows do not
    // linger when returning from an ability list to the skills list.
    wipe_screen_from(indexed_menu_prefix_col(COL_SKILL));
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ability_menu_draw_skills(*highlight, options, 1);
    ability_menu_put_exit_button();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(COL_SKILL, 3 + *highlight);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = -1;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action)
            && clicked_choice == ABILITY_MENU_CLICK_EXIT)
        {
            if (click_action == UI_MENU_CLICK_HOVER)
                return (0);
            return (S_MAX + 1);
        }

        if (clicked_choice >= 1 && clicked_choice <= options)
        {
            bool same_choice = (*highlight == clicked_choice);

            *highlight = clicked_choice;
            if (click_action == UI_MENU_CLICK_HOVER)
                return (0);
            if (same_choice)
                return (*highlight);
            return (0);
        }
    }

    if (menu_letters && (ch >= 'a') && (ch <= (char)'a' + options - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        return (*highlight);
    }

    if (menu_letters && (ch >= 'A') && (ch <= (char)'A' + options - 1))
    {
        *highlight = (int)ch - 'A' + 1;
        return (*highlight);
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '\t')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (S_MAX + 1);  // Always return S_MAX + 1 to exit, regardless of options
    }

    if (ch == 'i')
    {
        return (S_MAX + 2);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        *highlight = (*highlight + (options - 2)) % options + 1;
    }

    /* Next item */
    if (ch == '2')
    {
        *highlight = *highlight % options + 1;
    }

    return (0);
}

int abilities_menu2(int skilltype, int* highlight)
{
    int i;
    bool compact_layout = ability_menu_use_compact_layout();
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    int ability_col = ability_menu_list_col();
    int desc_col = ability_menu_description_col();
    int list_first_row = 3;
    int list_rows = (Term && Term->hgt > list_first_row) ? (Term->hgt - list_first_row) : 1;

    ability_type* b_ptr;
    ability_type* visible_entries[ABILITIES_MAX];
    byte visible_attrs[ABILITIES_MAX];

    int ch;
    int visible_count = 0; // Count of actually visible abilities
    int visible_abilities[ABILITIES_MAX]; // Map display letters to ability numbers
    int top_visible = 0;
    int highlight_display_index = -1;

    char buf[80];

    byte attr;

    // In compact layout the abilities list reuses the skills column.
    wipe_screen_from(indexed_menu_prefix_col(
        compact_layout ? COL_SKILL : COL_ABILITY));
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    if (!compact_layout)
        ability_menu_draw_skills(skilltype + 1, ability_menu_skill_options(),
            ABILITY_MENU_CLICK_SKILL_BASE);

    // abilities title with color
    Term_putstr(ability_col, 1, -1, TERM_L_BLUE, "Abilities");
    ability_menu_put_exit_button();

    // For special abilities, we may need to adjust highlight to first visible ability
    int first_visible_ability = -1;

    /* Pre-scan for Special abilities to adjust highlight before display */
    if (skilltype == S_SPC)
    {
        int temp_visible_count = 0;
        int temp_first_visible = -1;

        for (i = 0; i < z_info->b_max; i++)
        {
            b_ptr = &b_info[i];
            if (!b_ptr->name || b_ptr->skilltype != skilltype) continue;

            if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
            {
                if (temp_first_visible == -1)
                {
                    temp_first_visible = b_ptr->abilitynum;
                }
                temp_visible_count++;
            }
        }

        /* Adjust highlight before display if needed */
        if (temp_visible_count > 0 && temp_first_visible != -1)
        {
            /* Check if current highlight corresponds to a visible ability */
            int current_ability_num = *highlight - 1; /* Convert 1-based to 0-based */
            bool highlight_is_visible = false;

            for (i = 0; i < z_info->b_max; i++)
            {
                b_ptr = &b_info[i];
                if (!b_ptr->name || b_ptr->skilltype != skilltype) continue;

                if (b_ptr->abilitynum == current_ability_num && p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                {
                    highlight_is_visible = true;
                    break;
                }
            }

            if (!highlight_is_visible)
            {
                *highlight = temp_first_visible + 1; /* Convert back to 1-based */
            }
        }
    }

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skilltype)
            continue;

        /* For special abilities, only show granted abilities */
        if (skilltype == S_SPC && !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            continue;
        }

        /* Hide deprecated WIL_OATH ability from menu (now handled at birth) */
        if (skilltype == S_WIL && b_ptr->abilitynum == WIL_OATH)
            continue;

        // Safety check for ability number bounds
        if (b_ptr->abilitynum >= ABILITIES_MAX) {
            continue;
        }

        // Safety check for array bounds
        if (visible_count >= ABILITIES_MAX) {
            break;
        }

        /* Determine the appropriate colour. */
        if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
        {
            if (p_ptr->active_ability[skilltype][b_ptr->abilitynum]
                && ability_is_blocked(skilltype, b_ptr->abilitynum))
            {
                attr = TERM_ORANGE;
            }
            else if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                    attr = TERM_WHITE;
                else
                    attr = TERM_RED;
            }
            else
            {
                if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                    attr = TERM_L_GREEN;
                else
                    attr = TERM_RED;
            }
        }
        else if (prereqs(skilltype, b_ptr->abilitynum))
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
        }

        visible_entries[visible_count] = b_ptr;
        visible_attrs[visible_count] = attr;

        // Map this visible ability to its position
        visible_abilities[visible_count] = b_ptr->abilitynum;

        // Track first visible ability for highlight adjustment
        if (first_visible_ability == -1) {
            first_visible_ability = b_ptr->abilitynum;
        }

        visible_count++;
    }

    if (skilltype == S_SMT || skilltype == S_ARC || skilltype == S_MEL
        || skilltype == S_PER)
    {
        ability_menu_sort_entries_by_level(visible_entries, visible_attrs,
            visible_abilities, visible_count);
    }

    /* Safety check: if no abilities are visible, show message and exit */
    if (visible_count == 0) {
        Term_putstr(ability_col, 4, -1, TERM_L_DARK, "No abilities available for this skill.");
        do {
            int clicked_choice = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;

            Term_fresh();
            ch = inkey(); /* Wait for keypress */

            if (ui_menu_click_take_action(&clicked_choice, &click_action)
                && clicked_choice == ABILITY_MENU_CLICK_EXIT)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                return (ABILITIES_MAX + 2);
            }
        } while (ch == UI_MENU_CLICK_WAKE_KEY);

        return (ABILITIES_MAX + 1); /* Return to skills menu */
    }

    for (i = 0; i < visible_count; i++)
    {
        if (visible_abilities[i] == *highlight - 1)
        {
            highlight_display_index = i;
            break;
        }
    }

    if (highlight_display_index < 0)
        highlight_display_index = 0;

    if (list_rows < 1)
        list_rows = 1;

    if (highlight_display_index < top_visible)
        top_visible = highlight_display_index;
    if (highlight_display_index >= top_visible + list_rows)
        top_visible = highlight_display_index - list_rows + 1;
    if (top_visible < 0)
        top_visible = 0;
    if (top_visible > visible_count - list_rows)
        top_visible = visible_count - list_rows;
    if (top_visible < 0)
        top_visible = 0;

    if (visible_count > list_rows)
    {
        strnfmt(buf, sizeof(buf), "[%d-%d/%d]", top_visible + 1,
            MIN(top_visible + list_rows, visible_count), visible_count);
        Term_putstr(ability_col, 2, -1, TERM_SLATE, buf);
    }

    for (i = top_visible; i < visible_count && i < top_visible + list_rows; i++)
    {
        int display_row = list_first_row + (i - top_visible);

        b_ptr = visible_entries[i];
        attr = visible_attrs[i];

        if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
            && (p_ptr->bane_type > 0))
        {
            char name_buf[80];
            strnfmt(name_buf, sizeof(name_buf), "%s-%s",
                bane_name[p_ptr->bane_type], (b_name + b_ptr->name));
            indexed_menu_entry_label(buf, sizeof(buf), i, name_buf);
        }
        else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
            && (p_ptr->oath_type > 0))
        {
            char name_buf[80];
            strnfmt(name_buf, sizeof(name_buf), "%s: %s",
                (b_name + b_ptr->name), oath_name_short(p_ptr->oath_type));
            indexed_menu_entry_label(buf, sizeof(buf), i, name_buf);
        }
        else
        {
            indexed_menu_entry_label(buf, sizeof(buf), i, (b_name + b_ptr->name));
        }

        Term_putstr(ability_col, display_row, -1, attr, buf);
        ui_menu_click_add(ABILITY_MENU_CLICK_ABILITY_BASE + i,
            indexed_menu_prefix_col(ability_col), display_row,
            ability_menu_click_width(ability_col, desc_col, buf));

        if (*highlight == b_ptr->abilitynum + 1)
        {
            /* Highlight the label with bright blue */
            indexed_menu_focus_prefix(buf, sizeof(buf), i);
            Term_putstr(indexed_menu_prefix_col(ability_col), display_row, -1,
                TERM_L_BLUE, buf);

            /* Print the description of the highlighted ability. */
            /* (ability_type::text is an offset, so it's always non-negative) */
            /* Determine compact mode from terminal height; use single newline between
             * sections when space is tight, double newline when there is room. */
            int term_hgt_ab = Term ? Term->hgt : 24;
            bool compact_mode = (term_hgt_ab < 28);
            const char *desc_sep = compact_mode ? "\n" : "\n\n";
            int post_desc_row = 3; /* updated after description renders */
            {
                /* Check if this is a broken oath ability and use Q: text instead */
                char* description_text = NULL;
                bool use_death_message = false;

                if (skilltype == S_SPC &&
                    (b_ptr->abilitynum == SPC_OATH_MERCY ||
                     b_ptr->abilitynum == SPC_OATH_SILENCE ||
                     b_ptr->abilitynum == SPC_OATH_IRON ||
                     b_ptr->abilitynum == SPC_OATH_SMITH ||
                     b_ptr->abilitynum == SPC_OATH_VALOROUS ||
                     b_ptr->abilitynum == SPC_OATH_LIGHT))
                {
                    /* Check if this oath is broken */
                    int oath_id = 0;
                    if (b_ptr->abilitynum == SPC_OATH_MERCY) oath_id = OATH_MERCY;
                    else if (b_ptr->abilitynum == SPC_OATH_SILENCE) oath_id = OATH_SILENCE;
                    else if (b_ptr->abilitynum == SPC_OATH_IRON) oath_id = OATH_IRON;
                    else if (b_ptr->abilitynum == SPC_OATH_SMITH) oath_id = OATH_SMITH;
                    else if (b_ptr->abilitynum == SPC_OATH_VALOROUS) oath_id = OATH_VALOROUS;
                    else if (b_ptr->abilitynum == SPC_OATH_LIGHT) oath_id = OATH_LIGHT;

                    if (oath_id > 0 && oath_invalid(oath_id))
                    {
                        description_text = oath_death_message(oath_id);
                        use_death_message = true;
                    }
                }

                /* Clear description area first */
                wipe_screen_from(desc_col);

                /* Display ability name in description area with appropriate color */
                Term_putstr(desc_col, 1, -1, TERM_YELLOW, b_name + b_ptr->name);

                /* Wrap to the active terminal width so compact layouts do not overflow. */
                text_out_wrap = ability_menu_description_wrap(desc_col);
                text_out_indent = desc_col;

                /* Description starts at row 3 for more space */
                Term_gotoxy(text_out_indent, 3);

                if (use_death_message && description_text && description_text[0])
                {
                    /* Display Q: text in red for broken oaths */
                    text_out_to_screen(TERM_RED, description_text);
                }
                else
                {
                    /* Display effect first, then prerequisites and lore. */
                    char desc_controller_text[2048];
                    char effect_controller_text[2048];
                    const char *desc_text = (b_ptr->text)
                        ? ability_menu_controller_text(b_text + b_ptr->text,
                              desc_controller_text, sizeof(desc_controller_text))
                        : NULL;
                    const char *effect_text = (b_ptr->effect)
                        ? ability_menu_controller_text(b_text + b_ptr->effect,
                              effect_controller_text, sizeof(effect_controller_text))
                        : NULL;
                    bool has_desc = desc_text && desc_text[0];
                    bool has_effect = effect_text && effect_text[0];
                    bool song_bonus_rendered = false;

                    if (has_effect)
                    {
                        text_out_to_screen(TERM_L_WHITE, effect_text);
                        if (skilltype == S_SNG)
                        {
                            ability_menu_render_song_bonus_block(b_ptr);
                            song_bonus_rendered = true;
                        }
                    }
                    if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                    {
                        if (has_effect)
                            text_out_to_screen(TERM_L_WHITE, desc_sep);
                        ability_menu_render_prerequisites_block(skilltype,
                            b_ptr, desc_col);
                    }
                    if (has_desc)
                    {
                        if (has_effect
                            || !p_ptr->have_ability[skilltype][b_ptr->abilitynum])
                            text_out_to_screen(TERM_L_WHITE, desc_sep);
                        text_out_to_screen(TERM_SLATE, desc_text);
                    }

                    if (skilltype == S_SNG && !song_bonus_rendered)
                        ability_menu_render_song_bonus_block(b_ptr);

                    ability_menu_render_dynamic_bonus(skilltype,
                        b_ptr->abilitynum);

                    /* For Nienna's Gift of Mercy, show current bonus */
                    if (skilltype == S_SPC && b_ptr->abilitynum == SPC_NIENA_MERCY &&
                        p_ptr->have_ability[S_SPC][SPC_NIENA_MERCY])
                    {
                        /* Calculate current stealth bonus (same logic as in player-bonuses.c) */
                        int total_monsters_seen = 0;
                        int total_monsters_killed = 0;

                        /* Sum up global monster tracking (excluding uniques) */
                        for (int i = 1; i < z_info->r_max; i++)
                        {
                            monster_lore *l_ptr = &l_list[i];
                            monster_race *r_ptr = &r_info[i];

                            if (r_ptr->flags1 & RF1_UNIQUE) continue;

                            total_monsters_seen += l_ptr->psights;
                            total_monsters_killed += l_ptr->pkills;
                        }

                        if (total_monsters_seen > 0)
                        {
                            /* Calculate stealth bonus: 10*(seen-killed)/seen, rounded up */
                            int mercy_ratio_times_10 = (10 * (total_monsters_seen - total_monsters_killed));
                            int stealth_bonus = (mercy_ratio_times_10 + total_monsters_seen - 1) / total_monsters_seen;

                            char bonus_text[100];
                            strnfmt(bonus_text, sizeof(bonus_text),
                                   "\n\nCurrent bonus: +%d stealth (%d seen, %d spared)",
                                   stealth_bonus, total_monsters_seen,
                                   total_monsters_seen - total_monsters_killed);
                            text_out_to_screen(TERM_L_GREEN, bonus_text);
                        }
                        else
                        {
                            text_out_to_screen(TERM_SLATE, "\n\nCurrent bonus: +0 stealth (no monsters encountered yet)");
                        }
                    }

                    if ((skilltype == S_EVN)
                        && (b_ptr->abilitynum == EVN_HEAVY_ARMOUR))
                    {
                        const int armour_weight = heavy_armour_desc_current_weight();
                        const int protection_bonus = armour_weight / 150;
                        const int evasion_bonus =
                            heavy_armour_desc_current_evasion_bonus();
                        const bool learned =
                            p_ptr->have_ability[skilltype][b_ptr->abilitynum];
                        char bonus_text[160];

                        strnfmt(bonus_text, sizeof(bonus_text),
                            learned
                                ? "\n\nCurrent bonus: +%d protection vs physical attacks and %+d evasion (%d.%d lb counted)"
                                : "\n\nWith current equipment, this would grant +%d protection vs physical attacks and %+d evasion (%d.%d lb counted)",
                            protection_bonus, evasion_bonus, armour_weight / 10,
                            armour_weight % 10);
                        text_out_to_screen(TERM_L_GREEN, bonus_text);
                    }
                }

                /* Capture the row where description text ended for dynamic placement */
                {
                    int pdx;
                    Term_locate(&pdx, &post_desc_row);
                    if (pdx > text_out_indent) post_desc_row++;
                }

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;
            }

            // if you have the ability and it is Bane...
            if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
                && (p_ptr->bane_type > 0))
            {
                int killed = bane_type_killed(p_ptr->bane_type);
                int current_bonus = bane_bonus_aux();
                int next_threshold = 2;

                // Calculate next threshold using same formula as bane
                int threshold = 2;
                while (threshold <= killed)
                {
                    threshold *= 2;
                }
                next_threshold = threshold;  // This is the next power of 2

                /* Place bane stats dynamically after description text */
                int bane_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, bane_row, -1, TERM_WHITE,
                    format("%s-Bane:", bane_name[p_ptr->bane_type]));
                Term_putstr(desc_col, bane_row + 2, -1, TERM_WHITE,
                    format("  %d slain, giving a %+d bonus", killed, current_bonus));

                if (current_bonus == 0 && killed < 2) {
                    Term_putstr(desc_col, bane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(desc_col, bane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d slain)", next_threshold));
                }
            }
            else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
                && (p_ptr->oath_type > 0))
            {
                /* Place oath info dynamically after description text */
                int oath_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, oath_row, -1, TERM_WHITE, "Oath:");
                Term_putstr(desc_col + 6, oath_row, -1, TERM_L_BLUE,
                    oath_name_short(p_ptr->oath_type));

                /* Wrap to the active terminal width here too. */
                text_out_wrap = ability_menu_description_wrap(desc_col);
                text_out_indent = desc_col;

                /* History */
                Term_gotoxy(text_out_indent, oath_row + 1);
                strnfmt(buf, 80, "You have sworn not to %s.",
                    oath_desc2_short(p_ptr->oath_type));
                text_out_to_screen(TERM_L_WHITE, buf);

                /* Reset text_out() vars */
                text_out_wrap = 0;
                text_out_indent = 0;

                if (oath_invalid(p_ptr->oath_type))
                    Term_putstr(desc_col, oath_row + 4, -1, TERM_RED,
                        "You are an oathbreaker.");
                else
                    Term_putstr(desc_col, oath_row + 4, -1, TERM_WHITE,
                        format("Bonus: %s.", oath_reward_short(p_ptr->oath_type)));
            }
            // if you have the unique bane special ability
            else if (p_ptr->have_ability[skilltype][b_ptr->abilitynum]
                && (skilltype == S_SPC) && (b_ptr->abilitynum == SPC_UNIQUE_BANE))
            {
                int uniques_killed = unique_bane_type_killed();
                int current_bonus = 0;
                int next_threshold = 2;

                // Calculate current bonus using same formula as bane
                int threshold = 2;
                while (threshold <= uniques_killed)
                {
                    threshold *= 2;
                    current_bonus++;
                }

                // Calculate next threshold
                if (current_bonus == 0) {
                    next_threshold = 2;
                } else {
                    next_threshold = threshold;  // This is the next power of 2
                }

                /* Place unique bane stats dynamically after description text */
                int ubane_row = post_desc_row + (compact_mode ? 1 : 2);
                Term_putstr(desc_col, ubane_row, -1, TERM_WHITE, "Unique Bane:");
                Term_putstr(desc_col, ubane_row + 2, -1, TERM_WHITE,
                    format("  %d uniques slain, giving a %+d bonus",
                           uniques_killed, current_bonus));

                if (current_bonus == 0 && uniques_killed < 2) {
                    Term_putstr(desc_col, ubane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                } else if (next_threshold <= 64) {  // Don't show if threshold is too high
                    Term_putstr(desc_col, ubane_row + 3, -1, TERM_SLATE,
                        format("  (next bonus at %d uniques)", next_threshold));
                }
            }
        }

    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice - single column layout */
    if (highlight_display_index >= 0)
    {
        int cursor_row = list_first_row + (highlight_display_index - top_visible);
        if (cursor_row >= list_first_row && cursor_row < list_first_row + list_rows)
            Term_gotoxy(ability_col, cursor_row);
    }

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = -1;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (clicked_choice == ABILITY_MENU_CLICK_EXIT)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return (0);
                return (ABILITIES_MAX + 2);
            }
            else if (clicked_choice >= ABILITY_MENU_CLICK_SKILL_BASE
                && clicked_choice < ABILITY_MENU_CLICK_SKILL_BASE + S_MAX)
            {
                int clicked_skill = clicked_choice - ABILITY_MENU_CLICK_SKILL_BASE;
                int skill_options = ability_menu_skill_options();

                if (clicked_skill >= 0 && clicked_skill < skill_options)
                    return ABILITY_MENU_SWITCH_SKILL_BASE + clicked_skill;
            }
            else if (clicked_choice >= ABILITY_MENU_CLICK_ABILITY_BASE
                && clicked_choice < ABILITY_MENU_CLICK_ABILITY_BASE + visible_count)
            {
                int selected_index =
                    clicked_choice - ABILITY_MENU_CLICK_ABILITY_BASE;

                if (selected_index >= 0 && selected_index < visible_count)
                {
                    bool same_choice =
                        (*highlight == visible_abilities[selected_index] + 1);

                    *highlight = visible_abilities[selected_index] + 1;
                    if (click_action != UI_MENU_CLICK_HOVER && same_choice)
                        return (*highlight);
                    return (0);
                }
            }
        }
    }

    if (menu_letters && (ch >= 'a') && (ch <= (char)'a' + visible_count - 1))
    {
        int selected_index = (int)ch - 'a';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if (menu_letters && (ch >= 'A') && (ch <= (char)'A' + visible_count - 1))
    {
        int selected_index = (int)ch - 'A';
        /* Bounds check for safety */
        if (selected_index >= 0 && selected_index < visible_count) {
            *highlight = visible_abilities[selected_index] + 1;
            return abilities_menu2(skilltype, highlight);
        }
    }

    if ((ch == ESCAPE) || (ch == 'q') || (ch == '4')
        || (steamdeck && ch == steamdeck_back_key()))
    {
        return (ABILITIES_MAX + 1);
    }

    if (ch == '\t')
    {
        return (ABILITIES_MAX + 2);
    }

    if (ch == 'i')
    {
        return (ABILITIES_MAX + 3);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
        || (steamdeck && ch == steamdeck_confirm_key()))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        /* Only navigate if there are visible abilities */
        if (visible_count > 0) {
            /* Find current visible index */
            int current_visible_index = -1;
            for (int i = 0; i < visible_count; i++) {
                if (visible_abilities[i] + 1 == *highlight) {
                    current_visible_index = i;
                    break;
                }
            }

            /* Move to previous visible ability */
            if (current_visible_index > 0) {
                *highlight = visible_abilities[current_visible_index - 1] + 1;
            } else if (current_visible_index == 0) {
                *highlight = visible_abilities[visible_count - 1] + 1;
            } else {
                /* Fallback if not found - go to first visible */
                *highlight = visible_abilities[0] + 1;
            }
        }
    }

    /* Next item */
    if (ch == '2')
    {
        /* Only navigate if there are visible abilities */
        if (visible_count > 0) {
            /* Find current visible index */
            int current_visible_index = -1;
            for (int i = 0; i < visible_count; i++) {
                if (visible_abilities[i] + 1 == *highlight) {
                    current_visible_index = i;
                    break;
                }
            }

            /* Move to next visible ability */
            if (current_visible_index >= 0 && current_visible_index < visible_count - 1) {
                *highlight = visible_abilities[current_visible_index + 1] + 1;
            } else if (current_visible_index == visible_count - 1) {
                *highlight = visible_abilities[0] + 1;
            } else {
                /* Fallback if not found - go to first visible */
                *highlight = visible_abilities[0] + 1;
            }
        }
    }

    return (0);
}

void do_cmd_ability_screen(void)
{
    int skill_cur = S_MEL;
    int entry_cur = 0;
    int entry_top = 0;
    int column = 0;
    int desc_top = 0;
    bool done = false;

    log_trace("ABILITY_SCREEN: Entering ability browser");

    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_push_terminal_menu_scale();
    sdl_screen_back_gesture_begin();

    while (!done)
    {
        ability_browser_layout layout;
        ability_browser_entry entries[ABILITIES_MAX];
        ability_browser_desc_line desc_lines[ABILITY_BROWSER_DESC_MAX_LINES];
        char summary[160];
        int skill_options;
        int skilltype;
        int entry_count;
        int desc_line_count;
        int desc_max_top;
        int desc_wrap_w;
        int ability_visible_entries;
        int hover_choice;
        int skill_hover = -1;
        bool train_hovered = false;
        int ch;
        bool steamdeck = steamdeck_controls_active();

        skill_options = ability_menu_skill_options();
        if (skill_options < 1)
            skill_options = 1;

        if (skill_cur >= skill_options)
            skill_cur = skill_options - 1;
        if (skill_cur < 0)
            skill_cur = 0;

        skilltype = skill_cur;
        entry_count = ability_browser_collect_entries(skilltype, entries,
            ABILITIES_MAX);
        ability_browser_build_summary(skilltype, summary, sizeof(summary));
        ability_browser_init_layout(&layout, entry_count, summary);
        ability_visible_entries = MAX(1, layout.ability_rows
            / MAX(layout.ability_entry_rows, 1));

        if (entry_count <= 0)
        {
            entry_cur = 0;
            entry_top = 0;
        }
        else
        {
            if (entry_cur >= entry_count)
                entry_cur = entry_count - 1;
            if (entry_cur < 0)
                entry_cur = 0;
        }

        if (sdl_touch_only_device_active())
        {
            /* Touch-only menus are viewport-driven: dragging pans the list,
             * and tapping selects.  Do not let the off-screen selection pull
             * the viewport back on the next redraw. */
            int max_entry_top = MAX(0,
                entry_count - ability_visible_entries);
            (void)ui_scroll_area_take_touch_scrolled();
            if (entry_top > max_entry_top)
                entry_top = max_entry_top;
        }
        else
        {
            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + ability_visible_entries)
                entry_top = entry_cur - ability_visible_entries + 1;
        }
        if (entry_top < 0)
            entry_top = 0;

        desc_wrap_w = MAX(1, layout.desc_w - 2);
        log_debug("ABILITY_WRAP layout term=%dx%d visible_col=%d visible_w=%d ability_w=%d desc_col=%d desc_w=%d wrap_w=%d",
            layout.term_wid, layout.term_hgt, layout.visible_col,
            layout.visible_w, layout.ability_w, layout.desc_col,
            layout.desc_w, desc_wrap_w);
        desc_line_count = ability_browser_build_description(skilltype,
            (entry_count > 0) ? &entries[entry_cur] : NULL, desc_lines,
            desc_wrap_w);
        desc_max_top = MAX(0, desc_line_count - layout.desc_rows);
        if (desc_top > desc_max_top)
            desc_top = desc_max_top;
        if (desc_top < 0)
            desc_top = 0;

        (void)Term_set_extra_cursor(false, 0, 0, false);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_set_touch_exit_button(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_begin_cols(layout.ability_col,
            layout.ability_col + layout.ability_w - 1, layout.ability_row,
            layout.ability_row + layout.ability_rows - 1,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        if (sdl_touch_only_device_active())
        {
            /* Touch-only: drag pans the ability list without moving the
             * selection (tap an ability to pick it). */
            ui_scroll_area_set_offset_target(&entry_top,
                MAX(0, entry_count - ability_visible_entries));
        }
        if (layout.desc_w > 0)
        {
            (void)ui_scroll_area_add_cols(layout.desc_col,
                layout.desc_col + layout.desc_w - 1, layout.desc_row,
                layout.desc_row + layout.desc_rows - 1,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('8', '2', '6', '4');
            if (sdl_touch_only_device_active())
                ui_scroll_area_set_offset_target(&desc_top, desc_max_top);
        }

        if (ui_menu_click_get_hover_choice(&hover_choice)
            && hover_choice == ABILITY_MENU_CLICK_TRAIN)
        {
            train_hovered = true;
        }

        ability_browser_draw_frame(&layout, skilltype, summary, train_hovered);

        if (ui_menu_click_get_hover_choice(&hover_choice)
            && hover_choice >= ABILITY_MENU_CLICK_SKILL_BASE
            && hover_choice < ABILITY_MENU_CLICK_SKILL_BASE + skill_options)
        {
            skill_hover = hover_choice - ABILITY_MENU_CLICK_SKILL_BASE;
        }

        ability_browser_draw_skill_summary(&layout, skill_options, skill_cur,
            skill_hover);
        ability_browser_draw_ability_list(&layout, skilltype, entries,
            entry_count, entry_cur, entry_top, column == 0);
        ability_browser_draw_description(&layout, desc_lines, desc_line_count,
            desc_top, column == 1);

        if (layout.status_row != layout.prompt_row)
        {
            char status[180];

            Term_erase(layout.visible_col, layout.status_row,
                layout.visible_w);
            if (entry_count > 0)
            {
                char state[32];

                ability_browser_entry_state(state, sizeof(state), skilltype,
                    &entries[entry_cur]);
                if (skilltype == S_SPC)
                {
                    strnfmt(status, sizeof(status), "%s: %s",
                        entries[entry_cur].name, state);
                }
                else
                {
                    strnfmt(status, sizeof(status),
                        "%s: %s | next %s point costs %d XP",
                        entries[entry_cur].name, state,
                        skill_names_full[skilltype],
                        ability_browser_next_skill_cost(skilltype));
                }
            }
            else
            {
                strnfmt(status, sizeof(status),
                    "%s has no visible abilities.",
                    skill_names_full[skilltype]);
            }
            ability_browser_draw_colored_text_line_ex(layout.visible_col,
                layout.status_row, layout.visible_w, TERM_L_WHITE, status,
                false);
        }

        ability_browser_draw_prompt(&layout);

        if (column == 0 && entry_count > 0)
            Term_gotoxy(layout.ability_col,
                layout.ability_row + (entry_cur - entry_top)
                    * MAX(layout.ability_entry_rows, 1));
        else
            Term_gotoxy(layout.desc_col,
                layout.desc_row + MIN(layout.desc_rows - 1,
                    MAX(0, desc_top ? 1 : 0)));

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice >= ABILITY_MENU_CLICK_ABILITY_BASE)
                {
                    int clicked_entry =
                        clicked_choice - ABILITY_MENU_CLICK_ABILITY_BASE;

                    if (clicked_entry >= 0 && clicked_entry < entry_count)
                    {
                        bool same = (entry_cur == clicked_entry);

                        entry_cur = clicked_entry;
                        column = 0;
                        desc_top = 0;
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        if (!same)
                            continue;
                        ch = ' ';
                    }
                }
                else if (clicked_choice >= ABILITY_MENU_CLICK_SKILL_BASE)
                {
                    int clicked_skill =
                        clicked_choice - ABILITY_MENU_CLICK_SKILL_BASE;

                    if (clicked_skill >= 0 && clicked_skill < skill_options)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                        {
                            log_debug("ABILITY_SCREEN: hover skill tab %d (%s)",
                                clicked_skill, skill_names_full[clicked_skill]);
                            continue;
                        }

                        log_debug("ABILITY_SCREEN: mouse skill tab action=%d skill=%d (%s)",
                            click_action, clicked_skill,
                            skill_names_full[clicked_skill]);
                        ui_menu_click_clear();
                        skill_cur = clicked_skill;
                        entry_cur = 0;
                        entry_top = 0;
                        desc_top = 0;
                        column = 0;
                        if (click_action == UI_MENU_CLICK_SECONDARY)
                            ch = '+';
                        else
                            continue;
                    }
                }
                else
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    switch (clicked_choice)
                    {
                    case ABILITY_MENU_CLICK_EXIT: ch = ESCAPE; break;
                    case ABILITY_MENU_CLICK_TRAIN: ch = '+'; break;
                    case ABILITY_MENU_CLICK_ACTION: ch = ' '; break;
                    case ABILITY_MENU_CLICK_SCROLL_UP: ch = '9'; break;
                    case ABILITY_MENU_CLICK_SCROLL_DOWN: ch = '3'; break;
                    case ABILITY_MENU_CLICK_SKILL_ALLOCATE: ch = 'i'; break;
                    case ABILITY_MENU_CLICK_PREV_SKILL: ch = '['; break;
                    case ABILITY_MENU_CLICK_NEXT_SKILL: ch = ']'; break;
                    default: break;
                    }
                }
            }
        }

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (steamdeck && ch == steamdeck_confirm_key())
            ch = ' ';
        if (steamdeck && ch == steamdeck_alt_action_key())
            ch = '+';

        if (ch == UI_MENU_CLICK_WAKE_KEY)
            continue;

        if (ch == ESCAPE || ch == 'q' || ch == 'Q')
        {
            done = true;
            continue;
        }

        if (ch == 'i' || ch == 'I')
        {
            ui_scroll_area_clear();
            gain_skills_set_initial_skill(skilltype);
            (void)gain_skills();
            p_ptr->redraw |= (PR_EXP | PR_BASIC);
            p_ptr->update |= (PU_BONUS | PU_MANA);
            handle_stuff();
            desc_top = 0;
            continue;
        }

        if (ch == KTRL('I') || ch == ']' || ch == '}')
        {
            skill_cur = ability_browser_move_skill(skill_cur, skill_options,
                1);
            entry_cur = 0;
            entry_top = 0;
            desc_top = 0;
            column = 0;
            continue;
        }

        if (ch == '[' || ch == '{')
        {
            skill_cur = ability_browser_move_skill(skill_cur, skill_options,
                -1);
            entry_cur = 0;
            entry_top = 0;
            desc_top = 0;
            column = 0;
            continue;
        }

        if (ch == '+' || ch == '=')
        {
            (void)ability_browser_train_skill(skilltype);
            desc_top = 0;
            continue;
        }

        if (ch == '9')
        {
            desc_top -= layout.desc_rows;
            if (desc_top < 0)
                desc_top = 0;
            column = 1;
            continue;
        }

        if (ch == '3')
        {
            desc_top += layout.desc_rows;
            if (desc_top > desc_max_top)
                desc_top = desc_max_top;
            column = 1;
            continue;
        }

        if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            if (entry_count > 0)
            {
                (void)ability_browser_activate_choice(skilltype,
                    entries[entry_cur].abilitynum);
                desc_top = 0;
            }
            continue;
        }

        if (sdl_menu_letters_enabled())
        {
            if (column == 0 && ch >= 'a'
                && ch < (char)('a' + entry_count))
            {
                entry_cur = (int)(ch - 'a');
                desc_top = 0;
                continue;
            }
        }

        {
            int d = target_dir((char)ch);

            if (d)
            {
                if (ddx[d])
                {
                    column += ddx[d];
                    if (column < 0)
                        column = 0;
                    if (column > 1)
                        column = 1;
                }

                if (ddy[d])
                {
                    if (column == 0)
                    {
                        if (entry_count > 0)
                        {
                            entry_cur += ddy[d];
                            while (entry_cur < 0)
                                entry_cur += entry_count;
                            while (entry_cur >= entry_count)
                                entry_cur -= entry_count;
                            desc_top = 0;
                        }
                    }
                    else if (column == 1)
                    {
                        desc_top += ddy[d];
                        if (desc_top < 0)
                            desc_top = 0;
                        if (desc_top > desc_max_top)
                            desc_top = desc_max_top;
                    }
                }

                continue;
            }
        }
    }

    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_pop_terminal_menu_scale();
    sdl_screen_back_gesture_end();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();

    handle_stuff();
    inven_enforce_current_pack_limits();
}
