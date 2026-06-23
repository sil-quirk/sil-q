#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "melee/melee-combat-display.h"
#include "pane.h"
#include "sdl-config.h"

static void combat_rolls_mark_dirty(void)
{
    if (p_ptr)
        p_ptr->window |= PW_COMBAT_ROLLS;
}

void new_combat_round(void)
{
    int i;
    bool display_changed = (combat_number != 0)
        || (turns_since_combat == 10 && combat_number_old != 0);

    log_trace("[ROUND] new_combat_round: ENTER turns_since_combat=%d, combat_number=%d, combat_number_old=%d", turns_since_combat, combat_number, combat_number_old);
    if (combat_number != 0)
    {
        /* Add the finished round to combat history before we lose it */
        add_combat_round_to_history();
        combat_number_old = combat_number;
    }
    combat_number = 0;
    turns_since_combat++;

    log_trace("[ROUND] new_combat_round: after inc, turns_since_combat=%d", turns_since_combat);
    if (turns_since_combat == 1)
    {
        // copy previous round's rolls into old round's rolls
        log_trace("[ROUND] copy current->old: combat_number_old(before)=%d", combat_number_old);
        for (i = 0; i < MAX_COMBAT_ROLLS; i++)
        {
            memcpy(&combat_rolls[1][i], &combat_rolls[0][i], sizeof(combat_roll));
            log_trace("[ROUND]   copied i=%d att_type=%d att=%d evn=%d dam=%d prot=%d atk=%c def=%c", i,
                      combat_rolls[1][i].att_type,
                      combat_rolls[1][i].att,
                      combat_rolls[1][i].evn,
                      combat_rolls[1][i].dam,
                      combat_rolls[1][i].prot,
                      combat_rolls[1][i].attacker_char,
                      combat_rolls[1][i].defender_char);
        }
    }
    else if (turns_since_combat == 11)
    {
        // reset old round's rolls
        combat_number_old = 0;
        for (i = 0; i < MAX_COMBAT_ROLLS; i++)
        {
            combat_rolls[1][i].att_type = COMBAT_ROLL_NONE;
        }
    }

    // reset new round's rolls
    for (i = 0; i < MAX_COMBAT_ROLLS; i++)
    {
        combat_rolls[0][i].att_type = COMBAT_ROLL_NONE;
    }
    if (display_changed)
        combat_rolls_mark_dirty();
    log_trace("[ROUND] new_combat_round: EXIT turns_since_combat=%d, combat_number=%d, combat_number_old=%d", turns_since_combat, combat_number, combat_number_old);
}

/*
 * Update combat roll table part 1 (the attack rolls)
 */
void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll)
{
    monster_race* r_ptr1;
    monster_race* r_ptr2;

    if (m_ptr1 == PLAYER)
    {
        r_ptr1 = &r_info[0];
    }
    else if (m_ptr1 == NULL)
    {
        // hack for traps hitting you
        r_ptr1 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr1 = &r_info[m_ptr1->image_r_idx];
    }
    else
    {
        r_ptr1 = &r_info[m_ptr1->r_idx];
    }

    if (m_ptr2 == PLAYER)
    {
        r_ptr2 = &r_info[0];
    }
    else if (m_ptr2 == NULL)
    {
        // hack for attacking Morgoth's crown
        r_ptr2 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr2 = &r_info[m_ptr2->image_r_idx];
    }
    else
    {
        r_ptr2 = &r_info[m_ptr2->r_idx];
    }

    log_trace("[ROLL1] enter: combat_number=%d old=%d turns_since_combat=%d", combat_number, combat_number_old, turns_since_combat);
    if (combat_number < MAX_COMBAT_ROLLS)
    {
        memset(&combat_rolls[0][combat_number], 0, sizeof(combat_roll));
        combat_rolls[0][combat_number].att_type = COMBAT_ROLL_ROLL;
        combat_rolls[0][combat_number].sequence = log_history_next_sequence();

        if (m_ptr1 == NULL)
        {
            combat_rolls[0][combat_number].attacker_char
                = combat_roll_special_char;
            combat_rolls[0][combat_number].attacker_attr
                = combat_roll_special_attr;
            combat_rolls[0][combat_number].is_attacker_player = false;
        }
        else if (vis || (m_ptr1 == PLAYER))
        {
            if (m_ptr1 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_attacker_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
                    combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].attacker_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].attacker_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_attacker_player = false;
                combat_rolls[0][combat_number].attacker_char = graphics_are_ascii() ? r_ptr1->d_char : r_ptr1->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].attacker_attr = graphics_are_ascii() ? r_ptr1->d_attr : r_ptr1->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_attacker_player = false;
            combat_rolls[0][combat_number].attacker_char = '?';
            combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
        }

        // hack for Iron Crown
        if (m_ptr2 == NULL)
        {
            combat_rolls[0][combat_number].defender_char = ']';
            combat_rolls[0][combat_number].defender_attr = TERM_L_DARK;
            combat_rolls[0][combat_number].is_defender_player = false;
        }
        else if (vis || (m_ptr2 == PLAYER))
        {
            if (m_ptr2 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_defender_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
                    combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].defender_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].defender_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_defender_player = false;
                combat_rolls[0][combat_number].defender_char = graphics_are_ascii() ? r_ptr2->d_char : r_ptr2->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].defender_attr = graphics_are_ascii() ? r_ptr2->d_attr : r_ptr2->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_defender_player = false;
            combat_rolls[0][combat_number].defender_char = '?';
            combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
        }

        combat_rolls[0][combat_number].att = att;
        combat_rolls[0][combat_number].att_roll = att_roll;
        combat_rolls[0][combat_number].evn = evn;
        combat_rolls[0][combat_number].evn_roll = evn_roll;

    log_trace("[ROLL1] added at index=%d atk=%c def=%c att=%d ar=%d evn=%d er=%d", combat_number,
          combat_rolls[0][combat_number].attacker_char,
          combat_rolls[0][combat_number].defender_char,
          combat_rolls[0][combat_number].att,
          combat_rolls[0][combat_number].att_roll,
          combat_rolls[0][combat_number].evn,
          combat_rolls[0][combat_number].evn_roll);
    combat_number++;
    turns_since_combat = 0;
    log_trace("[ROLL1] exit: combat_number=%d old=%d", combat_number, combat_number_old);
    }

    /* Wait for update_combat_rolls2() or update_combat_rolls_no_damage() so
     * consumers never render a half-populated roll. */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Update combat roll table part 1b (the attack when there is no roll made -- eg
 * breath attack)
 */
void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis)
{
    monster_race* r_ptr1;
    monster_race* r_ptr2;

    if (m_ptr1 == PLAYER)
    {
        r_ptr1 = &r_info[0];
    }
    else if (m_ptr1 == NULL)
    {
        // hack for traps hitting you
        r_ptr1 = NULL;
    }
    else if (p_ptr->image)
    {
        r_ptr1 = &r_info[m_ptr1->image_r_idx];
    }
    else
    {
        r_ptr1 = &r_info[m_ptr1->r_idx];
    }

    if (m_ptr2 == PLAYER)
    {
        r_ptr2 = &r_info[0];
    }
    else if (p_ptr->image)
    {
        r_ptr2 = &r_info[m_ptr2->image_r_idx];
    }
    else
    {
        r_ptr2 = &r_info[m_ptr2->r_idx];
    }

    log_trace("[ROLL1B] enter: combat_number=%d old=%d turns_since_combat=%d", combat_number, combat_number_old, turns_since_combat);
    if (combat_number < MAX_COMBAT_ROLLS)
    {
        memset(&combat_rolls[0][combat_number], 0, sizeof(combat_roll));
        combat_rolls[0][combat_number].att_type = COMBAT_ROLL_AUTO;
        combat_rolls[0][combat_number].sequence = log_history_next_sequence();

        if (m_ptr1 == NULL)
        {
            combat_rolls[0][combat_number].attacker_char
                = combat_roll_special_char;
            combat_rolls[0][combat_number].attacker_attr
                = combat_roll_special_attr;
            combat_rolls[0][combat_number].is_attacker_player = false;
        }
        else if (vis || (m_ptr1 == PLAYER))
        {
            if (m_ptr1 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_attacker_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_char = r_ptr1->d_char;
                    combat_rolls[0][combat_number].attacker_attr = r_ptr1->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].attacker_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].attacker_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_attacker_player = false;
                combat_rolls[0][combat_number].attacker_char = graphics_are_ascii() ? r_ptr1->d_char : r_ptr1->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].attacker_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].attacker_attr = graphics_are_ascii() ? r_ptr1->d_attr : r_ptr1->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_attacker_player = false;
            combat_rolls[0][combat_number].attacker_char = '?';
            combat_rolls[0][combat_number].attacker_attr = TERM_SLATE;
        }

        if (vis || (m_ptr2 == PLAYER))
        {
            if (m_ptr2 == PLAYER)
            {
                /* Player appearance */
                combat_rolls[0][combat_number].is_defender_player = true;
                if (graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_char = r_ptr2->d_char;
                    combat_rolls[0][combat_number].defender_attr = r_ptr2->d_attr;
                }
                else
                {
                    /* In graphics mode, use race sprite with equipment offset */
                    monster_race* player_r_ptr = &r_info[p_ptr->prace];
                    combat_rolls[0][combat_number].defender_char = player_r_ptr->x_char + player_tile_offset();
                    combat_rolls[0][combat_number].defender_attr = player_r_ptr->x_attr;
                }
            }
            else
            {
                /* Monster appearance */
                combat_rolls[0][combat_number].is_defender_player = false;
                combat_rolls[0][combat_number].defender_char = graphics_are_ascii() ? r_ptr2->d_char : r_ptr2->x_char;

                if (p_ptr->rage && graphics_are_ascii())
                {
                    combat_rolls[0][combat_number].defender_attr = TERM_RED;
                }
                else
                {
                    combat_rolls[0][combat_number].defender_attr = graphics_are_ascii() ? r_ptr2->d_attr : r_ptr2->x_attr;
                }
            }
        }
        else
        {
            combat_rolls[0][combat_number].is_defender_player = false;
            combat_rolls[0][combat_number].defender_char = '?';
            combat_rolls[0][combat_number].defender_attr = TERM_SLATE;
        }

    log_trace("[ROLL1B] added index=%d atk=%c def=%c (AUTO)", combat_number,
          combat_rolls[0][combat_number].attacker_char,
          combat_rolls[0][combat_number].defender_char);
    combat_number++;
    turns_since_combat = 0;
    log_trace("[ROLL1B] exit: combat_number=%d old=%d", combat_number, combat_number_old);
    }

    /* The damage phase marks the completed roll dirty. */
    /* p_ptr->window |= (PW_COMBAT_ROLLS); */
}

/*
 * Update combat roll table part 2 (the damage rolls)
 */
void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps, int prot,
    int prt_percent, int dam_type, bool melee)
{
    log_trace("[ROLL2] enter: combat_number=%d old=%d last_index=%d", combat_number, combat_number_old, combat_number - 1);
    if ((combat_number > 0) && (combat_number - 1 < MAX_COMBAT_ROLLS))
    {
        combat_rolls[0][combat_number - 1].dam_type = dam_type;
        combat_rolls[0][combat_number - 1].sequence = log_history_next_sequence();
        combat_rolls[0][combat_number - 1].dd = dd;
        combat_rolls[0][combat_number - 1].ds = ds;
        combat_rolls[0][combat_number - 1].dam = dam;
        combat_rolls[0][combat_number - 1].pd = pd;
        combat_rolls[0][combat_number - 1].ps = ps;
        combat_rolls[0][combat_number - 1].prot = prot;
        combat_rolls[0][combat_number - 1].prt_percent = prt_percent;
        combat_rolls[0][combat_number - 1].melee = melee;
        log_trace("[ROLL2] filled index=%d dd=%d ds=%d dam=%d pd=%d ps=%d prot=%d prt%%=%d melee=%d", 
                  combat_number - 1, dd, ds, dam, pd, ps, prot, prt_percent, melee);

        // deal with protection for the player
        // this hackishly uses the pd and ps to store the min and max prot for
        // the player
        if (pd == -1)
        {
            // use the protection values for pure elemental types if there was
            // no attack roll
            if (combat_rolls[0][combat_number - 1].att_type == COMBAT_ROLL_AUTO)
            {
                combat_rolls[0][combat_number - 1].pd = p_min(dam_type, melee);
                combat_rolls[0][combat_number - 1].ps = p_max(dam_type, melee);
            }
            // otherwise use the normal protection values
            else
            {
                combat_rolls[0][combat_number - 1].pd = p_min(GF_HURT, melee);
                combat_rolls[0][combat_number - 1].ps = p_max(GF_HURT, melee);
            }
        }

        log_trace("[ROLL2] exit: index=%d done", combat_number - 1);
        combat_rolls_mark_dirty();
    }
}

void update_combat_rolls2_combo(int dd, int ds, int dam, int dd2, int ds2,
    int dam2, int pd, int ps, int prot, int prt_percent, int dam_type,
    bool melee)
{
    update_combat_rolls2(dd, ds, dam + dam2, pd, ps, prot, prt_percent,
        dam_type, melee);

    if ((combat_number > 0) && (combat_number - 1 < MAX_COMBAT_ROLLS))
    {
        combat_rolls[0][combat_number - 1].dd2 = dd2;
        combat_rolls[0][combat_number - 1].ds2 = ds2;
        combat_rolls[0][combat_number - 1].force_damage = true;
        combat_rolls_mark_dirty();
    }
}

/*
 * Finish an attack roll that has no damage-roll phase, such as a miss.  Call
 * this after the visible outcome message so combined log order stays:
 * message, then roll.
 */
void update_combat_rolls_no_damage(void)
{
    log_trace("[ROLL0] enter: combat_number=%d old=%d last_index=%d",
        combat_number, combat_number_old, combat_number - 1);

    if ((combat_number > 0) && (combat_number - 1 < MAX_COMBAT_ROLLS)
        && (combat_rolls[0][combat_number - 1].att_type != COMBAT_ROLL_NONE))
    {
        combat_rolls[0][combat_number - 1].no_damage = true;
        combat_rolls[0][combat_number - 1].sequence =
            log_history_next_sequence();
        log_trace("[ROLL0] stamped index=%d", combat_number - 1);
        combat_rolls_mark_dirty();
    }
}

/*
 * Display combat rolls in a window
 */

typedef struct combat_display_entry
{
    int round;
    int index;
    u32b sequence;
    int tie_breaker;
} combat_display_entry;

static int combat_display_entry_compare_newest_first(const void* a,
    const void* b)
{
    const combat_display_entry* entry_a = (const combat_display_entry*)a;
    const combat_display_entry* entry_b = (const combat_display_entry*)b;

    if (entry_a->sequence < entry_b->sequence)
        return 1;
    if (entry_a->sequence > entry_b->sequence)
        return -1;

    if (entry_a->tie_breaker < entry_b->tie_breaker)
        return 1;
    if (entry_a->tie_breaker > entry_b->tie_breaker)
        return -1;

    return entry_a->round - entry_b->round;
}

static int collect_combat_display_entries(combat_display_entry* ordered, int max_entries)
{
    int count = 0;

    for (int round = 0; round < 2; round++)
    {
        int combat_num_for_round = (round == 0) ? combat_number : combat_number_old;
        if (combat_num_for_round <= 0)
            continue;

        for (int idx = 0; (idx < combat_num_for_round) && (count < max_entries);
             idx++)
        {
            if (combat_rolls[round][idx].att_type == COMBAT_ROLL_NONE)
                continue;

            ordered[count].round = round;
            ordered[count].index = idx;
            ordered[count].sequence = combat_rolls[round][idx].sequence;
            if (ordered[count].sequence == 0)
                ordered[count].sequence = (round == 0) ? 2 : 1;
            ordered[count].tie_breaker = idx;
            count++;
        }
    }

    qsort(ordered, count, sizeof(combat_display_entry),
        combat_display_entry_compare_newest_first);

    return count;
}

static bool combat_roll_overlay_compact(void)
{
    return Term && PANE_ROLLS < ANGBAND_TERM_MAX
        && angband_term[PANE_ROLLS] == Term;
}

static int combat_roll_tile_cols(void)
{
    return (use_bigtile && !graphics_are_ascii()) ? 2 : 1;
}

/*
 * When a token sink is active, combat-roll drawing appends semantic tokens to
 * it instead of writing the term cell grid.  This lets the SDL layer re-pack
 * the full line in pixels even when the visible panel is too narrow in cells.
 */
static combat_roll_token* g_combat_tok_buf = NULL;
static int g_combat_tok_max = 0;
static int g_combat_tok_count = 0;

static void combat_roll_tok_append_text(byte attr, cptr text)
{
    combat_roll_token* t;

    if (!g_combat_tok_buf || g_combat_tok_count >= g_combat_tok_max)
        return;

    t = &g_combat_tok_buf[g_combat_tok_count++];
    t->is_tile = false;
    t->attr = attr;
    t->tile_char = 0;
    SDL_strlcpy(t->text, text, sizeof(t->text));
}

static void combat_roll_tok_append_tile(byte attr, char ch)
{
    combat_roll_token* t;

    if (!g_combat_tok_buf || g_combat_tok_count >= g_combat_tok_max)
        return;

    t = &g_combat_tok_buf[g_combat_tok_count++];
    t->is_tile = true;
    t->attr = attr;
    t->tile_char = ch;
    t->text[0] = '\0';
}

static int combat_roll_put_tile(int col, int row, byte attr, char ch)
{
    if (g_combat_tok_buf)
    {
        /* In text (ASCII) mode the "tile" is a glyph; keep it as text. */
        if (graphics_are_ascii())
        {
            char s[2] = { ch, '\0' };
            combat_roll_tok_append_text(attr, s);
        }
        else
        {
            combat_roll_tok_append_tile(attr, ch);
        }
        return col + combat_roll_tile_cols();
    }

    Term_queue_char(col, row, attr, ch, 0, 0);
    if (combat_roll_tile_cols() > 1)
    {
        if ((attr & 0x80) && ((byte)ch & 0x80))
            Term_queue_char(col + 1, row, 255, -1, 0, 0);
        else
            Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
    }

    return col + combat_roll_tile_cols();
}

static int combat_roll_put_text(int col, int row, byte attr, cptr text)
{
    int len;

    if (!text)
        return col;

    len = (int)strlen(text);
    if (len <= 0)
        return col;

    if (g_combat_tok_buf)
    {
        combat_roll_tok_append_text(attr, text);
        return col + len;
    }

    Term_putstr(col, row, -1, attr, text);
    return col + len;
}

static int combat_roll_put_number(int col, int row, byte attr, int value)
{
    char buf[32];

    strnfmt(buf, sizeof(buf), "%d", value);
    return combat_roll_put_text(col, row, attr, buf);
}

static int combat_roll_text_cols(cptr text)
{
    if (!text)
        return 0;

    return (int)strlen(text);
}

static int combat_roll_number_cols(int value)
{
    char buf[32];

    strnfmt(buf, sizeof(buf), "%d", value);
    return combat_roll_text_cols(buf);
}

static void combat_roll_format_damage_dice(const combat_roll* roll,
    char* buf, size_t buflen)
{
    bool first = roll->dd > 0 && roll->ds > 0;
    bool second = roll->dd2 > 0 && roll->ds2 > 0;

    if (first && second)
    {
        strnfmt(buf, buflen, "(%dd%d+%dd%d)", roll->dd, roll->ds,
            roll->dd2, roll->ds2);
    }
    else if (second)
    {
        strnfmt(buf, buflen, "(%dd%d)", roll->dd2, roll->ds2);
    }
    else
    {
        strnfmt(buf, buflen, "(%dd%d)", roll->dd, roll->ds);
    }
}

static int combat_roll_compact_cols(const combat_roll* roll, int res)
{
    char buf[80];
    int cols = 0;
    int net_att = 0;
    int net_dam;

    if (!roll)
        return 0;

    cols += combat_roll_text_cols(" ");
    cols += combat_roll_tile_cols();
    cols += combat_roll_text_cols(" ");

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        strnfmt(buf, sizeof(buf), "(%+d) ", roll->att);
        cols += combat_roll_text_cols(buf);
        cols += combat_roll_number_cols(roll->att + roll->att_roll);
        cols += combat_roll_text_cols(" ");

        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        cols += (net_att > 0) ? combat_roll_number_cols(net_att)
            : combat_roll_text_cols("-");
        cols += combat_roll_text_cols(" ");

        cols += combat_roll_number_cols(roll->evn + roll->evn_roll);
        cols += combat_roll_text_cols(" ");

        strnfmt(buf, sizeof(buf), "[%+d] ", roll->evn);
        cols += combat_roll_text_cols(buf);
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        cols += combat_roll_text_cols("auto ");
    }
    else
    {
        return cols;
    }

    cols += combat_roll_tile_cols();

    if (roll->no_damage
        || (roll->att_type == COMBAT_ROLL_ROLL && net_att <= 0
            && !roll->force_damage))
        return cols;

    cols += combat_roll_text_cols(" -> ");
    combat_roll_format_damage_dice(roll, buf, sizeof(buf));
    cols += combat_roll_text_cols(buf);
    cols += combat_roll_text_cols(" ");

    cols += combat_roll_number_cols(roll->dam);
    cols += combat_roll_text_cols(" ");

    if (roll->att_type == COMBAT_ROLL_AUTO && !roll->melee)
    {
        if (res > 0)
            net_dam = (roll->dam / res) - roll->prot;
        else
            net_dam = (roll->dam * (-res)) - roll->prot;
    }
    else
    {
        net_dam = roll->dam - roll->prot;
    }
    if (net_dam < 0)
        net_dam = 0;

    cols += (net_dam > 0) ? combat_roll_number_cols(net_dam)
        : combat_roll_text_cols("-");
    cols += combat_roll_text_cols(" ");

    cols += combat_roll_number_cols(roll->prot);

    if (roll->is_defender_player)
    {
        if (roll->att_type == COMBAT_ROLL_AUTO && !roll->melee)
        {
            if (res > 1)
            {
                strnfmt(buf, sizeof(buf), " 1/%d then", res);
                cols += combat_roll_text_cols(buf);
            }
            else if (res < 0)
            {
                strnfmt(buf, sizeof(buf), " x%d then", -res);
                cols += combat_roll_text_cols(buf);
            }
        }

        if (roll->att_type == COMBAT_ROLL_ROLL)
        {
            strnfmt(buf, sizeof(buf), " [%d-%d]",
                (roll->pd * roll->prt_percent) / 100,
                (roll->ps * roll->prt_percent) / 100);
        }
        else
        {
            strnfmt(buf, sizeof(buf), " [%d-%d]", roll->pd, roll->ps);
        }
        cols += combat_roll_text_cols(buf);
    }
    else if (roll->ps >= 1 && roll->pd >= 1)
    {
        strnfmt(buf, sizeof(buf), " [%dd%d]", roll->pd, roll->ps);
        cols += combat_roll_text_cols(buf);
        if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
        {
            strnfmt(buf, sizeof(buf), " %d%%", roll->prt_percent);
            cols += combat_roll_text_cols(buf);
        }
    }

    return cols;
}

static void combat_roll_set_cursor_after(int row, int col)
{
    if (g_combat_tok_buf)
        return;
    if (!Term || !Term->scr)
        return;
    if (row < 0 || row >= Term->hgt)
        return;

    if (col < 0)
        col = 0;

    Term->scr->cy = row;
    if (col >= Term->wid)
    {
        Term->scr->cx = Term->wid;
        Term->scr->cu = 1;
    }
    else
    {
        Term->scr->cx = col;
        Term->scr->cu = 0;
    }
}

static void draw_combat_roll_line_compact(int row, int base_col_offset,
    const combat_roll* roll, int a_att, int a_evn, int a_hit,
    int a_dam_roll, int a_prot_roll, int a_net_dam, int res)
{
    char buf[80];
    int col = base_col_offset;
    int net_att = 0;
    int net_dam;

    col = combat_roll_put_text(col, row, TERM_WHITE, " ");
    col = combat_roll_put_tile(col, row, roll->attacker_attr,
        roll->attacker_char);
    col = combat_roll_put_text(col, row, TERM_WHITE, " ");

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        strnfmt(buf, sizeof(buf), "(%+d) ", roll->att);
        col = combat_roll_put_text(col, row, a_att, buf);
        col = combat_roll_put_number(col, row, a_att,
            roll->att + roll->att_roll);
        col = combat_roll_put_text(col, row, a_att, " ");

        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        if (net_att > 0)
            col = combat_roll_put_number(col, row, a_hit, net_att);
        else
            col = combat_roll_put_text(col, row, TERM_SLATE, "-");
        col = combat_roll_put_text(col, row, TERM_WHITE, " ");

        col = combat_roll_put_number(col, row, a_evn,
            roll->evn + roll->evn_roll);
        col = combat_roll_put_text(col, row, TERM_WHITE, " ");

        strnfmt(buf, sizeof(buf), "[%+d] ", roll->evn);
        col = combat_roll_put_text(col, row, a_evn, buf);
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        col = combat_roll_put_text(col, row, TERM_L_DARK, "auto ");
    }
    else
    {
        combat_roll_set_cursor_after(row, col);
        return;
    }

    col = combat_roll_put_tile(col, row, roll->defender_attr,
        roll->defender_char);

    if (roll->no_damage
        || (roll->att_type == COMBAT_ROLL_ROLL && net_att <= 0
            && !roll->force_damage))
    {
        combat_roll_set_cursor_after(row, col);
        return;
    }

    col = combat_roll_put_text(col, row, TERM_L_DARK, " -> ");
    combat_roll_format_damage_dice(roll, buf, sizeof(buf));
    col = combat_roll_put_text(col, row, a_dam_roll, buf);
    col = combat_roll_put_text(col, row, TERM_WHITE, " ");

    col = combat_roll_put_number(col, row, a_dam_roll, roll->dam);
    col = combat_roll_put_text(col, row, TERM_WHITE, " ");

    if (roll->att_type == COMBAT_ROLL_AUTO && !roll->melee) {
        if (res > 0)
            net_dam = (roll->dam / res) - roll->prot;
        else
            net_dam = (roll->dam * (-res)) - roll->prot;
    } else {
        net_dam = roll->dam - roll->prot;
    }
    if (net_dam < 0)
        net_dam = 0;

    if (net_dam > 0)
        col = combat_roll_put_number(col, row, a_net_dam, net_dam);
    else
        col = combat_roll_put_text(col, row, TERM_SLATE, "-");
    col = combat_roll_put_text(col, row, TERM_WHITE, " ");

    col = combat_roll_put_number(col, row, a_prot_roll, roll->prot);

    if (roll->is_defender_player)
    {
        if (roll->att_type == COMBAT_ROLL_AUTO && !roll->melee)
        {
            if (res > 1)
            {
                strnfmt(buf, sizeof(buf), " 1/%d then", res);
                col = combat_roll_put_text(col, row, TERM_L_BLUE, buf);
            }
            else if (res < 0)
            {
                strnfmt(buf, sizeof(buf), " x%d then", -res);
                col = combat_roll_put_text(col, row, TERM_L_BLUE, buf);
            }
        }

        if (roll->att_type == COMBAT_ROLL_ROLL)
        {
            strnfmt(buf, sizeof(buf), " [%d-%d]",
                (roll->pd * roll->prt_percent) / 100,
                (roll->ps * roll->prt_percent) / 100);
        }
        else
        {
            strnfmt(buf, sizeof(buf), " [%d-%d]", roll->pd, roll->ps);
        }
        col = combat_roll_put_text(col, row, a_prot_roll, buf);
    }
    else if (roll->ps >= 1 && roll->pd >= 1)
    {
        strnfmt(buf, sizeof(buf), " [%dd%d]", roll->pd, roll->ps);
        col = combat_roll_put_text(col, row, a_prot_roll, buf);
        if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
        {
            strnfmt(buf, sizeof(buf), " %d%%", roll->prt_percent);
            col = combat_roll_put_text(col, row, a_prot_roll, buf);
        }
    }

    combat_roll_set_cursor_after(row, col);
}

static void combat_roll_compute_attrs(const combat_roll* roll, int* res_out,
    int* a_att, int* a_evn, int* a_hit, int* a_dam_roll, int* a_prot_roll,
    int* a_net_dam)
{
    int res = 1;

    *a_net_dam = TERM_L_RED;

    if (roll->is_defender_player)
    {
        switch (roll->dam_type)
        {
        case GF_FIRE:
            res = resist_fire();
            break;
        case GF_COLD:
            res = resist_cold();
            break;
        case GF_POIS:
            res = resist_pois();
            *a_net_dam = TERM_GREEN;
            break;
        case GF_DARK:
            res = resist_dark();
            break;
        default:
            res = 1;
            *a_net_dam = TERM_L_RED;
            break;
        }
    }

    if (roll->is_attacker_player)
    {
        *a_att = TERM_L_BLUE;
        *a_evn = TERM_WHITE;
        *a_hit = TERM_L_RED;
        *a_dam_roll = TERM_L_BLUE;
        if (roll->prt_percent >= 100)
            *a_prot_roll = TERM_WHITE;
        else if (roll->prt_percent >= 1)
            *a_prot_roll = TERM_SLATE;
        else
            *a_prot_roll = TERM_DARK;
    }
    else
    {
        *a_att = TERM_WHITE;
        *a_evn = TERM_L_BLUE;
        *a_hit = TERM_L_RED;
        *a_dam_roll = TERM_WHITE;
        if (roll->prt_percent >= 100)
            *a_prot_roll = TERM_L_BLUE;
        else if (roll->prt_percent >= 1)
            *a_prot_roll = TERM_BLUE;
        else
            *a_prot_roll = TERM_DARK;
    }

    *res_out = res;
}

/*
 * Emit the full combat-roll line as semantic tokens (text runs + tiles),
 * bypassing the term cell grid so the line is never truncated by panel width.
 * Returns the number of tokens written.
 */
int combat_roll_emit_tokens(const combat_roll* roll, combat_roll_token* out,
    int max)
{
    int a_att, a_evn, a_hit, a_dam_roll, a_prot_roll, a_net_dam;
    int res;

    if (!roll || !out || max <= 0)
        return 0;

    combat_roll_compute_attrs(roll, &res, &a_att, &a_evn, &a_hit, &a_dam_roll,
        &a_prot_roll, &a_net_dam);

    g_combat_tok_buf = out;
    g_combat_tok_max = max;
    g_combat_tok_count = 0;
    draw_combat_roll_line_compact(0, 0, roll, a_att, a_evn, a_hit, a_dam_roll,
        a_prot_roll, a_net_dam, res);
    g_combat_tok_buf = NULL;

    return g_combat_tok_count;
}

static void draw_combat_roll_line(int row, int base_col_offset,
    const combat_roll* roll)
{
    char buf[80];
    int net_att = 0;
    int net_dam;
    int a_att;
    int a_evn;
    int a_hit;
    int a_dam_roll;
    int a_prot_roll;
    int a_net_dam = TERM_L_RED;
    int res = 1;

    log_trace("draw_combat_roll_line: row=%d att_type=%d attacker=%c defender=%c",
        row, roll->att_type, roll->attacker_char, roll->defender_char);

    combat_roll_compute_attrs(roll, &res, &a_att, &a_evn, &a_hit, &a_dam_roll,
        &a_prot_roll, &a_net_dam);

    if (combat_roll_overlay_compact())
    {
        int compact_cols = combat_roll_compact_cols(roll, res);
        int compact_col = base_col_offset;
        bool prev_pixel_pack;

        if (Term && compact_cols > 0 && Term->wid > compact_cols)
            compact_col = Term->wid - compact_cols;
        if (compact_col < base_col_offset)
            compact_col = base_col_offset;

        /*
         * Mark the whole line so the SDL layer renders it proportionally in
         * pixels (tiles inline, right-aligned to the pane edge) rather than
         * letting the cell grid stretch it out.  The cell positions written
         * here are only a fallback for non-pixel back-ends.
         */
        prev_pixel_pack = Term ? Term->story_pixel_pack : false;
        if (Term)
            Term->story_pixel_pack = true;
        draw_combat_roll_line_compact(row, compact_col, roll, a_att,
            a_evn, a_hit, a_dam_roll, a_prot_roll, a_net_dam, res);
        if (Term)
            Term->story_pixel_pack = prev_pixel_pack;
        return;
    }

    Term_putstr(base_col_offset, row, 1, TERM_WHITE, " ");
    Term_queue_char(base_col_offset + 1, row,
        roll->attacker_attr, roll->attacker_char, 0, 0);
    if (use_bigtile && !graphics_are_ascii())
    {
        if ((roll->attacker_attr & 0x80) && ((byte)roll->attacker_char & 0x80))
            Term_queue_char(base_col_offset + 2, row, 255, -1, 0, 0);
        else
            Term_queue_char(base_col_offset + 2, row, TERM_WHITE, ' ', 0, 0);
    }

    int tile_offset = (use_bigtile && !graphics_are_ascii()) ? 1 : 0;
    int base_col = base_col_offset + 2 + tile_offset;

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        int col = base_col;

        if (roll->att < 10)
        {
            strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
        }
        else
        {
            strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
        }
        Term_putstr(col, row, -1, a_att, buf);
        col += 6;

        strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
        Term_putstr(col, row, -1, a_att, buf);
        col += 4;

        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        if (net_att > 0)
        {
            strnfmt(buf, sizeof(buf), "%4d", net_att);
            Term_putstr(col, row, -1, a_hit, buf);
        }
        else
        {
            Term_putstr(col, row, -1, TERM_SLATE, "   -");
        }
        col += 4;

        strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
        Term_putstr(col, row, -1, a_evn, buf);
        col += 4;

        if (roll->evn < 10)
        {
            strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
        }
        Term_putstr(col, row, -1, a_evn, buf);
        col += 7;

        Term_putstr(col, row, 1, TERM_WHITE, " ");
        col += 1;

        Term_queue_char(col, row,
            roll->defender_attr, roll->defender_char, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                Term_queue_char(col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
            col += 2;
        }
        else
        {
            col += 1;
        }

        if (roll->no_damage)
            return;

        int damage_col = base_col + 25 + 1;
        if (use_bigtile && !graphics_are_ascii())
            damage_col += 2;
        else
            damage_col += 1;

        if (net_att > 0 || roll->force_damage)
        {
            int dice_width;

            Term_putstr(damage_col, row, -1, TERM_L_DARK, "  ->");
            damage_col += 4;

            if (roll->dd2 > 0 && roll->ds2 > 0)
            {
                combat_roll_format_damage_dice(roll, buf, sizeof(buf));
                Term_putstr(damage_col, row, -1, a_dam_roll, buf);
                dice_width = strlen(buf);
                Term_putstr(damage_col + dice_width, row, 1, TERM_WHITE, " ");
                damage_col += dice_width + 1;
            }
            else
            {
                if (roll->ds < 10)
                {
                    strnfmt(
                        buf, sizeof(buf), "   (%dd%d) ", roll->dd, roll->ds);
                }
                else
                {
                    strnfmt(
                        buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
                }
                Term_putstr(damage_col, row, -1, a_dam_roll, buf);
                damage_col += 9;
            }

            strnfmt(buf, sizeof(buf), "%4d", roll->dam);
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            damage_col += 4;

            net_dam = roll->dam - roll->prot;
            if (net_dam < 0)
                net_dam = 0;

            if (net_dam > 0)
            {
                strnfmt(buf, sizeof(buf), "%4d", net_dam);
                Term_addstr(-1, a_net_dam, buf);
            }
            else
            {
                Term_addstr(-1, TERM_SLATE, "   -");
            }

            strnfmt(buf, sizeof(buf), "%4d", roll->prot);
            Term_addstr(-1, a_prot_roll, buf);

            log_debug("COMBAT_ROLL_ROLL protection: is_defender_player=%d",
                roll->is_defender_player);

            if (roll->is_defender_player)
            {
                strnfmt(buf, sizeof(buf), "  [%d-%d]", (roll->pd * roll->prt_percent) / 100,
                    (roll->ps * roll->prt_percent) / 100);
                Term_addstr(-1, a_prot_roll, buf);
            }
            else
            {
                if ((roll->ps < 1) || (roll->pd < 1))
                {
                    SDL_strlcpy(buf, "        ", sizeof(buf));
                    Term_addstr(-1, a_prot_roll, buf);
                }
                else if (roll->ps < 10)
                {
                    strnfmt(buf, sizeof(buf), "   [%dd%d]", roll->pd, roll->ps);
                    Term_addstr(-1, a_prot_roll, buf);
                }
                else
                {
                    strnfmt(buf, sizeof(buf), "  [%dd%d]", roll->pd, roll->ps);
                    Term_addstr(-1, a_prot_roll, buf);
                }
                if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
                {
                    strnfmt(buf, sizeof(buf), " (%d%%)", roll->prt_percent);
                    Term_addstr(-1, a_prot_roll, buf);
                }
            }
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        int col = base_col;
        Term_putstr(col, row, -1, TERM_L_DARK,
            "                         ");
        col += 25;

        Term_putstr(col, row, 1, TERM_WHITE, " ");
        col += 1;

        Term_queue_char(col, row,
            roll->defender_attr, roll->defender_char, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                Term_queue_char(col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(col + 1, row, TERM_WHITE, ' ', 0, 0);
            col += 2;
        }
        else
        {
            col += 1;
        }

        int damage_col = base_col + 25 + 1;
        if (use_bigtile && !graphics_are_ascii())
            damage_col += 2;
        else
            damage_col += 1;

        int net_auto;
        if (roll->melee)
            net_auto = roll->dam - roll->prot;
        else if (res > 0)
            net_auto = (roll->dam / res) - roll->prot;
        else
            net_auto = (roll->dam * (-res)) - roll->prot;

        Term_putstr(damage_col, row, -1, TERM_L_DARK, "  ->");
        damage_col += 4;

        if (roll->dd2 > 0 && roll->ds2 > 0)
        {
            int dice_width;

            combat_roll_format_damage_dice(roll, buf, sizeof(buf));
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            dice_width = strlen(buf);
            Term_putstr(damage_col + dice_width, row, 1, TERM_WHITE, " ");
            damage_col += dice_width + 1;
        }
        else
        {
            if (roll->ds < 10)
            {
                strnfmt(
                    buf, sizeof(buf), "   (%dd%d) ", roll->dd, roll->ds);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
            }
            Term_putstr(damage_col, row, -1, a_dam_roll, buf);
            damage_col += 9;
        }

        strnfmt(buf, sizeof(buf), "%4d", roll->dam);
        Term_putstr(damage_col, row, -1, a_dam_roll, buf);
        damage_col += 4;

        if (net_auto > 0)
        {
            strnfmt(buf, sizeof(buf), "%4d", net_auto);
            Term_addstr(-1, a_net_dam, buf);
        }
        else
        {
            Term_addstr(-1, TERM_SLATE, "   -");
        }

        strnfmt(buf, sizeof(buf), "%4d", roll->prot);
        Term_addstr(-1, a_prot_roll, buf);

        log_debug("COMBAT_ROLL_AUTO protection: is_defender_player=%d",
            roll->is_defender_player);

        if (roll->is_defender_player)
        {
            if (!(roll->melee))
            {
                if (res > 1)
                {
                    strnfmt(buf, sizeof(buf), "  1/%d then", res);
                    Term_addstr(-1, TERM_L_BLUE, buf);
                }
                else if (res < 0)
                {
                    strnfmt(buf, sizeof(buf), "  x%d then", -res);
                    Term_addstr(-1, TERM_L_BLUE, buf);
                }
            }

            strnfmt(buf, sizeof(buf), "  [%d-%d]", roll->pd, roll->ps);
            Term_addstr(-1, a_prot_roll, buf);
        }
        else
        {
            if ((roll->ps < 1) || (roll->pd < 1))
            {
                SDL_strlcpy(buf, "        ", sizeof(buf));
                Term_addstr(-1, a_prot_roll, buf);
            }
            else if (roll->ps < 10)
            {
                strnfmt(buf, sizeof(buf), "   [%dd%d]", roll->pd, roll->ps);
                Term_addstr(-1, a_prot_roll, buf);
            }
            else
            {
                strnfmt(buf, sizeof(buf), "  [%dd%d]", roll->pd, roll->ps);
                Term_addstr(-1, a_prot_roll, buf);
            }
            if ((roll->prt_percent > 0) && (roll->prt_percent < 100))
            {
                strnfmt(buf, sizeof(buf), " (%d%%)", roll->prt_percent);
                Term_addstr(-1, a_prot_roll, buf);
            }
        }
    }
}

void display_combat_roll_line_at(int row, int base_col_offset,
    const combat_roll* roll)
{
    if (!roll)
        return;

    draw_combat_roll_line(row, base_col_offset, roll);
}

void display_combat_rolls(void)
{

    int i;

    log_trace("display_combat_rolls: Starting - combat_number=%d, combat_number_old=%d",
        combat_number, combat_number_old);

    for (i = 0; i < Term->hgt; i++)
    {
        Term_erase(0, i, 255);
    }

    combat_display_entry ordered[MAX_COMBAT_ROLLS * 2];
    int total_entries = collect_combat_display_entries(ordered, MAX_COMBAT_ROLLS * 2);
    int entries_to_show = MIN(total_entries, Term->hgt);

    for (int entry_idx = 0; entry_idx < entries_to_show; entry_idx++)
    {
        int round = ordered[entry_idx].round;
        int idx = ordered[entry_idx].index;

        draw_combat_roll_line(entry_idx, 0, &combat_rolls[round][idx]);
    }
}


/*
 * Add the current combat round to the history buffer
 */
void add_combat_round_to_history(void)
{
    int i;
    
    /* Only add if there were actual combat rolls this round */
    if (combat_number == 0) {
        return;
    }

    /* Get next position in circular buffer */
    combat_history_head = (combat_history_head + 1) % MAX_COMBAT_HISTORY;
    
    /* Update count if we haven't filled the buffer yet */
    if (combat_history_count < MAX_COMBAT_HISTORY) {
        combat_history_count++;
    }
    
    /* Store the combat round data */
    combat_history[combat_history_head].turn_count = turn;
    combat_history[combat_history_head].num_rolls = combat_number;
    
    /* Copy the finished combat rolls */
    for (i = 0; i < combat_number && i < MAX_COMBAT_ROLLS; i++) {
        memcpy(&combat_history[combat_history_head].rolls[i], &combat_rolls[0][i], sizeof(combat_roll));
        if (combat_history[combat_history_head].rolls[i].sequence == 0)
            combat_history[combat_history_head].rolls[i].sequence = log_history_next_sequence();
    }
}

/*
 * Display combat history menu similar to message log
 */
enum {
    LOG_HISTORY_CLICK_MESSAGE_TAB = -20001,
    LOG_HISTORY_CLICK_COMBAT_TAB = -20002
};

static int log_history_draw_tab(int row, int col, cptr label, bool active,
    bool hovered, int click_choice)
{
    char tab[32];
    byte attr = hovered ? TERM_YELLOW + TERM_SHADE : TERM_YELLOW;

    strnfmt(tab, sizeof(tab), active ? "[%s]" : " %s ", label);
    Term_putstr(col, row, -1, attr, tab);
    ui_menu_click_add(click_choice, col, row, (int)strlen(tab));
    return col + (int)strlen(tab) + 1;
}

static void log_history_draw_tabs(bool combat_active, int hover_tab, int term_wid)
{
    int col = 0;

    if (term_wid < 1)
        term_wid = 80;

    Term_putstr(0, 0, term_wid, TERM_L_WHITE + TERM_SHADE, "Logs");
    Term_erase(0, 1, 255);

    col = log_history_draw_tab(1, col, "Messages", !combat_active,
        hover_tab == LOG_HISTORY_CLICK_MESSAGE_TAB,
        LOG_HISTORY_CLICK_MESSAGE_TAB);
    (void)log_history_draw_tab(1, col, "Combat", combat_active,
        hover_tab == LOG_HISTORY_CLICK_COMBAT_TAB,
        LOG_HISTORY_CLICK_COMBAT_TAB);
}

static bool log_history_tab_key(char ch)
{
    return (ch == 'e') || (ch == 'E') || (ch == 'i') || (ch == 'I')
        || (ch == '\t');
}

void do_cmd_combat_history_legacy(void)
{
    char ch;
    int i, j, n, q;
    int wid, hgt;
    char finder[80];
    char buf[120];
    int hover_tab = 0;
    cptr prompt =
        "e/i tabs  Up/Down line  PgUp/PgDn page  Wheel/drag  / find  Left/Right pan  Esc";
    
    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));
    
    /* Count total combat rolls across all history */
    n = 0;
    for (i = 0; i < combat_history_count; i++) {
        n += combat_history[i].num_rolls;
    }
    
    /* Start on first roll */
    i = 0;
    
    /* Start at leftmost edge */
    q = 0;
    
    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Get size after any hidden-pane layout change */
    Term_get_size(&wid, &hgt);
    
    /* Process requests until done */
    while (1) {
        int body_top;
        int body_bottom;
        int visible_rows;
        int max_i;
        int page_rows;
        int range_first;
        int range_last;
        int old_i;
        int old_q;

        /* Clear screen */
        Term_clear();

        body_top = 3;
        body_bottom = hgt - 3;
        if (body_bottom < body_top)
        {
            body_top = 2;
            body_bottom = hgt - 2;
        }
        if (body_bottom < body_top)
        {
            body_top = 0;
            body_bottom = hgt - 1;
        }

        visible_rows = body_bottom - body_top + 1;
        if (visible_rows < 1)
            visible_rows = 1;

        max_i = (n > visible_rows) ? (n - visible_rows) : 0;
        if (i > max_i)
            i = max_i;
        if (i < 0)
            i = 0;

        page_rows = (visible_rows > 1) ? (visible_rows - 1) : 1;
        ui_scroll_area_begin(body_top, body_bottom,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        
        /* Display combat rolls */
        for (j = 0; (j < visible_rows) && (i + j < n); j++) {
            /* Find which history entry and which roll within that entry */
            int total_rolls = 0;
            int history_idx = -1;
            int roll_idx = -1;
            
            /* Walk through history from newest to oldest to find the roll at index (i + j) */
            for (int h = 0; h < combat_history_count; h++) {
                int hist_idx = (combat_history_head - h + MAX_COMBAT_HISTORY) % MAX_COMBAT_HISTORY;
                
                if (total_rolls + combat_history[hist_idx].num_rolls > i + j) {
                    history_idx = hist_idx;
                    roll_idx = (i + j) - total_rolls;
                    break;
                }
                total_rolls += combat_history[hist_idx].num_rolls;
            }
            
            if (history_idx == -1 || roll_idx == -1) continue;
            
            combat_history_round* round = &combat_history[history_idx];
            combat_roll* roll = &round->rolls[roll_idx];
            
            /* Skip empty rolls */
            if (roll->att_type == COMBAT_ROLL_NONE) continue;
            
            /* Format the combat roll with proper colors and alignment */
            
            /* Set up color scheme like display_combat_rolls */
            int a_att, a_evn, a_hit, a_dam_roll, a_prot_roll, a_net_dam;
            
            /* Determine if player attack or monster attack */
            bool is_player_attack = roll->is_attacker_player;
            
            if (is_player_attack) {
                a_att = TERM_L_BLUE;
                a_evn = TERM_WHITE;
                a_hit = TERM_L_RED;
                a_dam_roll = TERM_L_BLUE;
                a_net_dam = TERM_L_RED;
                if (roll->prt_percent >= 100)
                    a_prot_roll = TERM_WHITE;
                else if (roll->prt_percent >= 1)
                    a_prot_roll = TERM_SLATE;
                else
                    a_prot_roll = TERM_DARK;
            } else {
                a_att = TERM_WHITE;
                a_evn = TERM_L_BLUE;
                a_hit = TERM_L_RED;
                a_dam_roll = TERM_WHITE;
                a_net_dam = TERM_L_RED;
                if (roll->prt_percent >= 100)
                    a_prot_roll = TERM_L_BLUE;
                else if (roll->prt_percent >= 1)
                    a_prot_roll = TERM_BLUE;
                else
                    a_prot_roll = TERM_DARK;
            }
            
            /* Start building the line with proper formatting */
            int line_y = body_bottom - j;
            int col = 0;
            
            /* Apply horizontal scroll to starting column */
            col = -q;
            
            /* Display like combat rolls window - attacker symbol */
            if (col >= 0) Term_putstr(col, line_y, 1, TERM_WHITE, " ");
            col += 1;
            if (col >= 0) {
                Term_queue_char(col, line_y, roll->attacker_attr, roll->attacker_char, 0, 0);
                if (use_bigtile && !graphics_are_ascii())
                {
                    if ((roll->attacker_attr & 0x80) && ((byte)roll->attacker_char & 0x80))
                        Term_queue_char(col + 1, line_y, 255, -1, 0, 0);
                    else
                        Term_queue_char(col + 1, line_y, TERM_WHITE, ' ', 0, 0);
                }
            }
            col += 1;
            if (use_bigtile && !graphics_are_ascii()) col += 1;
            
            /* Attack roll section */
            if (roll->att_type == COMBAT_ROLL_ROLL) {
                /* Attack bonus */
                if (roll->att < 10) {
                    strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
                } else {
                    strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
                }
                if (col >= 0) Term_putstr(col, line_y, -1, a_att, buf);
                col += strlen(buf);
                
                /* Attack total */
                strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
                if (col >= 0) Term_putstr(col, line_y, -1, a_att, buf);
                col += 4;
                
                /* Net attack (hit calculation) */
                int net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
                if (net_att > 0) {
                    strnfmt(buf, sizeof(buf), "%4d", net_att);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_hit, buf);
                } else {
                    if (col >= 0) Term_putstr(col, line_y, -1, TERM_SLATE, "   -");
                }
                col += 4;
                
                /* Evasion total */
                strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
                if (col >= 0) Term_putstr(col, line_y, -1, a_evn, buf);
                col += 4;
                
                /* Evasion bonus */
                if (roll->evn < 10) {
                    strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
                } else {
                    strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
                }
                if (col >= 0) Term_putstr(col, line_y, -1, a_evn, buf);
                col += strlen(buf);
                
                /* Defender symbol */
                if (col >= 0) Term_putstr(col, line_y, 1, TERM_WHITE, " ");
                col += 1;
                if (col >= 0) {
                    Term_queue_char(col, line_y, roll->defender_attr, roll->defender_char, 0, 0);
                    if (use_bigtile && !graphics_are_ascii())
                    {
                        if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                            Term_queue_char(col + 1, line_y, 255, -1, 0, 0);
                        else
                            Term_queue_char(col + 1, line_y, TERM_WHITE, ' ', 0, 0);
                    }
                }
                col += 1;
                if (use_bigtile && !graphics_are_ascii()) col += 1;
                
                /* Damage section */
                if (!roll->no_damage
                    && (net_att > 0 || roll->force_damage)) {
                    if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, "  ->");
                    col += 4;
                    
                    /* Damage dice */
                    if (roll->dd2 > 0 && roll->ds2 > 0) {
                        combat_roll_format_damage_dice(
                            roll, buf, sizeof(buf));
                    } else {
                        if (roll->ds < 10) {
                            strnfmt(buf, sizeof(buf), "   (%dd%d)",
                                roll->dd, roll->ds);
                        } else {
                            strnfmt(buf, sizeof(buf), "  (%dd%d)",
                                roll->dd, roll->ds);
                        }
                    }
                    if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                    col += strlen(buf);
                    
                    /* Damage total */
                    strnfmt(buf, sizeof(buf), "%4d", roll->dam);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                    col += 4;
                    
                    /* Net damage */
                    int net_dam = roll->dam - roll->prot;
                    if (net_dam < 0) net_dam = 0;
                    
                    if (net_dam > 0) {
                        strnfmt(buf, sizeof(buf), "%4d", net_dam);
                        if (col >= 0) Term_putstr(col, line_y, -1, a_net_dam, buf);
                    } else {
                        if (col >= 0) Term_putstr(col, line_y, -1, TERM_SLATE, "   -");
                    }
                    col += 4;
                    
                    /* Protection */
                    strnfmt(buf, sizeof(buf), "%4d", roll->prot);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_prot_roll, buf);
                    col += 4;
                }
                
            } else if (roll->att_type == COMBAT_ROLL_AUTO) {
                /* Auto-hit attacks */
                if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, "                         ");
                col += 25;
                
                /* Defender symbol */
                if (col >= 0) Term_putstr(col, line_y, 1, TERM_WHITE, " ");
                col += 1;
                if (col >= 0) {
                    Term_queue_char(col, line_y, roll->defender_attr, roll->defender_char, 0, 0);
                    if (use_bigtile && !graphics_are_ascii())
                    {
                        if ((roll->defender_attr & 0x80) && ((byte)roll->defender_char & 0x80))
                            Term_queue_char(col + 1, line_y, 255, -1, 0, 0);
                        else
                            Term_queue_char(col + 1, line_y, TERM_WHITE, ' ', 0, 0);
                    }
                }
                col += 1;
                if (use_bigtile && !graphics_are_ascii()) col += 1;
                
                /* Damage section */
                if (col >= 0) Term_putstr(col, line_y, -1, TERM_L_DARK, "  ->");
                col += 4;
                
                /* Damage dice */
                if (roll->ds < 10) {
                    strnfmt(buf, sizeof(buf), "   (%dd%d)", roll->dd, roll->ds);
                } else {
                    strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
                }
                if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                col += strlen(buf);
                
                /* Damage total */
                strnfmt(buf, sizeof(buf), "%4d", roll->dam);
                if (col >= 0) Term_putstr(col, line_y, -1, a_dam_roll, buf);
                col += 4;
                
                /* Net damage */
                int net_dam = roll->dam - roll->prot;
                if (net_dam < 0) net_dam = 0;
                
                if (net_dam > 0) {
                    strnfmt(buf, sizeof(buf), "%4d", net_dam);
                    if (col >= 0) Term_putstr(col, line_y, -1, a_net_dam, buf);
                } else {
                    if (col >= 0) Term_putstr(col, line_y, -1, TERM_SLATE, "   -");
                }
                col += 4;
                
                /* Protection */
                strnfmt(buf, sizeof(buf), "%4d", roll->prot);
                if (col >= 0) Term_putstr(col, line_y, -1, a_prot_roll, buf);
                col += 4;
            }
        }
        
        range_first = (n > 0) ? (i + 1) : 0;
        range_last = (n > 0) ? (i + j) : 0;

        log_history_draw_tabs(true, hover_tab, wid);

        /* Display header */
        prt(format("Combat Log (%d-%d of %d rolls), Offset %d",
                   range_first, range_last, n, q),
            (body_top > 0) ? body_top - 1 : 0, 0);
        
        /* Display prompt */
        prt(prompt, hgt - 1, 0);
        ui_menu_click_add_text_token('8', 0, hgt - 1, prompt, "Up");
        ui_menu_click_add_text_token('2', 0, hgt - 1, prompt, "Down");
        ui_menu_click_add_text_token('9', 0, hgt - 1, prompt, "PgUp");
        ui_menu_click_add_text_token('3', 0, hgt - 1, prompt, "PgDn");
        ui_menu_click_add_text_token('i', 0, hgt - 1, prompt, "tabs");
        ui_menu_click_add_text_token('i', 0, hgt - 1, prompt, "e/i");
        ui_menu_click_add_text_token('/', 0, hgt - 1, prompt, "/");
        ui_menu_click_add_text_token('/', 0, hgt - 1, prompt, "find");
        ui_menu_click_add_text_token('4', 0, hgt - 1, prompt, "Left");
        ui_menu_click_add_text_token('6', 0, hgt - 1, prompt, "Right");
        ui_menu_click_add_text_token(ESCAPE, 0, hgt - 1, prompt, "Esc");
        
        /* Get a command without showing the terminal cursor */
        (void)Term_set_cursor(false);
        Term_fresh();
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
                if (clicked_choice == LOG_HISTORY_CLICK_MESSAGE_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        hover_tab = clicked_choice;
                        continue;
                    }
                    ch = 'i';
                }
                else if (clicked_choice == LOG_HISTORY_CLICK_COMBAT_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        hover_tab = clicked_choice;
                    continue;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    hover_tab = 0;
                    continue;
                }
                else
                    ch = (char)clicked_choice;
            }
            else if (ch == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }
        }

        if (log_history_tab_key(ch))
        {
            ui_scroll_area_clear();
            ui_menu_click_clear();
            screen_pop_supporting_panes_hidden();
            screen_load();
            do_cmd_messages();
            return;
        }
        
        /* Exit on Escape */
        if (ch == ESCAPE) break;
        
        /* Handle navigation and search same as messages */
        old_i = i;
        old_q = q;
        
        /* Horizontal scroll */
        if (ch == '4') {
            q = (q >= wid / 2) ? (q - wid / 2) : 0;
            continue;
        }
        if (ch == '6') {
            q = q + wid / 2;
            continue;
        }
        
        /* Search functionality */
        if (ch == '/') {
            s16b z;

            ui_menu_click_clear();
            ui_scroll_area_clear();

            prt("Find: ", hgt - 1, 0);
            if (!askfor_aux(finder, sizeof(finder))) continue;
            
            /* Search through combat rolls */
            for (z = i + 1; z < n; z++) {
                /* Find the roll at index z and check if it matches */
                int total_rolls = 0;
                bool found = false;
                
                for (int h = 0; h < combat_history_count; h++) {
                    int hist_idx = (combat_history_head - h + MAX_COMBAT_HISTORY) % MAX_COMBAT_HISTORY;
                    
                    if (total_rolls + combat_history[hist_idx].num_rolls > z) {
                        int r_idx = z - total_rolls;
                        combat_roll* search_roll = &combat_history[hist_idx].rolls[r_idx];
                        
                        /* Create search string for this roll */
                        char search_buf[120];
                        strnfmt(search_buf, sizeof(search_buf), "%c (%+d) (%dd%d)",
                                search_roll->attacker_char,
                                search_roll->att,
                                search_roll->dd, search_roll->ds);
                        
                        if (strstr(search_buf, finder)) {
                            i = z;
                            found = true;
                            break;
                        }
                        break;
                    }
                    total_rolls += combat_history[hist_idx].num_rolls;
                }
                
                if (found) break;
            }
        }
        
        /* Navigation */
        if (ch == '8') {
            if (i < max_i) i += 1;
        }
        if (ch == '2') {
            if (i > 0) i -= 1;
        }
        if (ch == '9') {
            i += page_rows;
            if (i > max_i) i = max_i;
        }
        if (ch == '3') {
            i -= page_rows;
            if (i < 0) i = 0;
        }
        if (ch == '7') {
            i = max_i;
        }
        if (ch == '1') {
            i = 0;
        }
        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' ')) {
            i += page_rows;
            if (i > max_i) i = max_i;
        }
        if (ch == '+') {
            if (i + 10 < max_i) i += 10;
            else i = max_i;
        }
        if ((ch == 'n') || (ch == KTRL('N'))) {
            i = (i >= page_rows) ? (i - page_rows) : 0;
        }
        if (ch == '-') {
            i = (i >= 10) ? (i - 10) : 0;
        }
        
        /* Error if no change */
        if (i == old_i && q == old_q) bell(NULL);
    }
    
    ui_scroll_area_clear();
    ui_menu_click_clear();

    /* Restore screen */
    screen_pop_supporting_panes_hidden();
    screen_load();
}

void do_cmd_combat_history(void)
{
    do_cmd_messages_with_filter(LOG_HISTORY_FILTER_COMBAT);
}

/*
 * Display detailed combat rolls for a specific round
 */
void display_combat_round_details(combat_history_round* round)
{
    int i;
    char buf[120];
    
    /* Save screen */
    screen_save();
    
    /* Clear screen */
    Term_clear();
    
    /* Display header */
    strnfmt(buf, sizeof(buf), "Combat Details - Turn %d (%d roll%s)", 
            round->turn_count, round->num_rolls, 
            (round->num_rolls == 1) ? "" : "s");
    prt(buf, 0, 0);
    
    /* Display each combat roll using similar logic to display_combat_rolls */
    int line = 2;
    int player_attacks = 0;
    int monster_attacks = 0;
    
    /* Count player attacks first */
    int total_player_attacks = 0;
    for (i = 0; i < round->num_rolls; i++) {
        if (round->rolls[i].is_attacker_player) {
            total_player_attacks++;
        }
    }
    
    /* Display each roll */
    for (i = 0; i < round->num_rolls; i++) {
        combat_roll* roll = &round->rolls[i];
        
        /* Skip empty rolls */
        if (roll->att_type == COMBAT_ROLL_NONE) continue;
        
        /* Determine line position based on attacker */
        if (roll->is_attacker_player) {
            /* Player attack */
            player_attacks++;
            line = 1 + player_attacks;
        } else {
            /* Monster attack */
            monster_attacks++;
            line = 2 + total_player_attacks + monster_attacks;
            if (total_player_attacks == 0) line--;
        }
        
        /* Build and display the combat roll line */
        char roll_line[120];
        roll_line[0] = '\0';
        
        /* Attacker symbol */
        strnfmt(buf, sizeof(buf), " %c", roll->attacker_char);
        SDL_strlcat(roll_line, buf, sizeof(roll_line));
        
        /* Attack roll info */
        if (roll->att_type == COMBAT_ROLL_ROLL) {
            strnfmt(buf, sizeof(buf), " (%+d)%4d %4d %4d [%+d] %c",
                    roll->att, roll->att + roll->att_roll,
                    (roll->att_roll + roll->att - roll->evn_roll - roll->evn > 0) ?
                        roll->att_roll + roll->att - roll->evn_roll - roll->evn : 0,
                    roll->evn + roll->evn_roll, roll->evn, roll->defender_char);
            SDL_strlcat(roll_line, buf, sizeof(roll_line));
            
            /* Damage info */
            if (!roll->no_damage
                && (roll->att_roll + roll->att - roll->evn_roll - roll->evn > 0
                    || roll->force_damage)) {
                int net_dam = roll->dam - roll->prot;
                if (net_dam < 0) net_dam = 0;
                combat_roll_format_damage_dice(roll, buf, sizeof(buf));
                {
                    char damage_buf[120];

                    strnfmt(damage_buf, sizeof(damage_buf),
                        " -> %s %4d %4d %4d", buf, roll->dam, net_dam,
                        roll->prot);
                    SDL_strlcat(
                        roll_line, damage_buf, sizeof(roll_line));
                }
            }
        } else if (roll->att_type == COMBAT_ROLL_AUTO) {
            strnfmt(buf, sizeof(buf), "                         %c -> (%dd%d) %4d",
                    roll->defender_char, roll->dd, roll->ds, roll->dam);
            SDL_strlcat(roll_line, buf, sizeof(roll_line));
            
            int net_dam = roll->dam - roll->prot;
            if (net_dam < 0) net_dam = 0;
            strnfmt(buf, sizeof(buf), " %4d %4d", net_dam, roll->prot);
            SDL_strlcat(roll_line, buf, sizeof(roll_line));
        }
        
        /* Display the line */
        prt(roll_line, line, 0);
    }
    
    /* Display instructions */
    prt("[Press any key to return]", Term->hgt - 1, 0);
    
    /* Wait for input */
    inkey();
    
    /* Restore screen */
    screen_load();
}
