#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

#define MIN_DEPTH_COUNTER_STEP_BASE 150000
#define MIN_DEPTH_COUNTER_STEP_SETTING_DELTA 30000
#define MIN_DEPTH_BASE_INCREMENT_START 85
#define MIN_DEPTH_BASE_INCREMENT_DIVISOR 850
#define MIN_DEPTH_INCREMENT_PER_BONUS 5
#define MIN_DEPTH_KILL_BONUS_STEP 500
#define MIN_DEPTH_KILL_BONUS_AMOUNT 5
#define MORGOTH_CALL_START_DIFFICULTY 10
#define MORGOTH_CALL_DRAIN_ESCALATION_MAX 7

typedef enum
{
    MORGOTH_CALL_DRAIN_RESISTED = 0,
    MORGOTH_CALL_DRAIN_TURIN = 1,
    MORGOTH_CALL_DRAIN_TAKEN = 2
} morgoth_call_drain_result;

typedef struct
{
    int resistance;
    int adjusted_difficulty;
    int player_will;
    int check_result;
    int stat_drain_before;
    int stat_drain_after;
    bool skill_check_rolled;
    bool dec_stat_succeeded;
} morgoth_call_drain_log;

static cptr morgoth_call_drain_result_name(morgoth_call_drain_result result)
{
    switch (result)
    {
    case MORGOTH_CALL_DRAIN_RESISTED:
        return "resisted";
    case MORGOTH_CALL_DRAIN_TURIN:
        return "turin-resisted";
    case MORGOTH_CALL_DRAIN_TAKEN:
        return "drained";
    default:
        return "unknown";
    }
}

static int min_depth_counter_step_adjustment(void)
{
    if (!op_ptr)
        return 0;

    switch (op_ptr->min_depth_timer_mode)
    {
    case MIN_DEPTH_TIMER_MODE_RELAXED:
        return MIN_DEPTH_COUNTER_STEP_SETTING_DELTA;
    case MIN_DEPTH_TIMER_MODE_HARSH:
        return -MIN_DEPTH_COUNTER_STEP_SETTING_DELTA;
    default:
        return 0;
    }
}

static int min_depth_counter_step(void)
{
    int step = MIN_DEPTH_COUNTER_STEP_BASE + min_depth_counter_step_adjustment();

    if (step < 1)
        step = 1;

    return step;
}

static bool min_depth_timer_bonus_slot_active(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return true;
}

static int min_depth_timer_item_bonus_units(void)
{
    int units = 0;

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        u32b f1, f2, f3, f4;
        bool equipped = (i >= INVEN_WIELD);

        if (!o_ptr->k_idx)
            continue;

        object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        (void)f1;
        (void)f2;
        if (!min_depth_timer_bonus_slot_active(o_ptr))
            continue;

        if (f4 & TR4_DEEP_CALL)
            units += equipped ? MIN_DEPTH_ITEM_BONUS_DEEP_CALL_EQUIPPED
                              : MIN_DEPTH_ITEM_BONUS_DEEP_CALL_INVENTORY;
        /* Count the item grant itself, even if the player disables the ability. */
        if (equipped && object_grants_ability(o_ptr, S_STL, STL_CRUEL_BLOW))
            units += MIN_DEPTH_ITEM_BONUS_CRUEL_BLOW_EQUIPPED;
        if (f3 & TR3_PERMA_CURSE)
            units += MIN_DEPTH_ITEM_BONUS_PERMA_CURSE;
    }

    return units;
}

static int min_depth_timer_kill_bonus(void)
{
    u32b total_kills = 0;

    if (!l_list || !z_info)
        return 0;

    for (int i = 1; i < z_info->r_max; i++)
    {
        monster_lore* lore = &l_list[i];

        if (lore->pkills > 0)
            total_kills += (u32b)lore->pkills;
    }

    return MIN_DEPTH_KILL_BONUS_AMOUNT
        * (int)(total_kills / MIN_DEPTH_KILL_BONUS_STEP);
}

static int min_depth_timer_base_increment(void)
{
    return MIN_DEPTH_BASE_INCREMENT_START - (playerturn / MIN_DEPTH_BASE_INCREMENT_DIVISOR);
}

static int min_depth_timer_additional_increment(void)
{
    int min_depth_value = min_depth();
    int current_depth = p_ptr ? p_ptr->depth : min_depth_value;
    int depth_bonus;
    int item_bonus_units = min_depth_timer_item_bonus_units();
    /* Use half-depth units so carried Deep Call items can be worth 1.5 depths. */
    int item_bonus = (MIN_DEPTH_INCREMENT_PER_BONUS * item_bonus_units
        + (MIN_DEPTH_BONUS_UNITS_PER_DEPTH / 2))
        / MIN_DEPTH_BONUS_UNITS_PER_DEPTH;
    int kill_bonus = min_depth_timer_kill_bonus();

    /* Character creation has not placed the player on depth 1 yet. */
    if ((playerturn == 0) && (current_depth <= 0))
        current_depth = min_depth_value;

    depth_bonus = MIN_DEPTH_INCREMENT_PER_BONUS
        * (current_depth - min_depth_value);

    return depth_bonus + item_bonus + kill_bonus;
}

void min_depth_timer_status(int* base_increment, int* additional_increment,
    int* total_increment, int* progress, int* threshold)
{
    int base = min_depth_timer_base_increment();
    int additional = min_depth_timer_additional_increment();
    int total = base + additional;
    int step = min_depth_counter_step();
    int current_progress = min_depth_counter % step;

    if (current_progress < 0)
        current_progress += step;

    if (base_increment)
        *base_increment = base;
    if (additional_increment)
        *additional_increment = additional;
    if (total_increment)
        *total_increment = total;
    if (progress)
        *progress = current_progress;
    if (threshold)
        *threshold = step;
}

/*
 * Determines the shallowest a player is allowed to go.
 * As time goes on, they are forced deeper and deeper.
 */
int min_depth(void)
{
    int min_depth_value = min_depth_counter / min_depth_counter_step() + 1;

    // bounds on the base
    if (min_depth_value < 1)
        min_depth_value = 1;
    if (min_depth_value > MORGOTH_DEPTH)
        min_depth_value = MORGOTH_DEPTH;

    // can't leave Morgoth's hall once entered
    if ((p_ptr->depth == MORGOTH_DEPTH) && p_ptr->morgoth_hall_entered)
    {
        min_depth_value = MORGOTH_DEPTH;
    }

    // no limits in the endgame
    if (p_ptr->on_the_run)
    {
        min_depth_value = 0;
    }

    return (min_depth_value);
}

static s32b min_depth_counter_stage(void)
{
    int step = min_depth_counter_step();

    if (min_depth_counter <= 0)
        return 0;

    return min_depth_counter / step;
}

void morgoth_call_sync_loaded_stage(void)
{
    if (!p_ptr)
        return;

    p_ptr->morgoth_call_last_stage = min_depth_counter_stage();
}

static int morgoth_call_current_difficulty(void)
{
    int escalation =
        p_ptr->morgoth_call_state & SAVEFILE_MORGOTH_CALL_ESCALATION_MASK;

    return MORGOTH_CALL_START_DIFFICULTY + escalation;
}

static void morgoth_call_advance_difficulty(void)
{
    int escalation =
        p_ptr->morgoth_call_state & SAVEFILE_MORGOTH_CALL_ESCALATION_MASK;

    if (escalation < MORGOTH_CALL_DRAIN_ESCALATION_MAX)
        escalation++;

    p_ptr->morgoth_call_state =
        (byte)((p_ptr->morgoth_call_state
                   & ~SAVEFILE_MORGOTH_CALL_ESCALATION_MASK)
            | escalation);
}

static cptr morgoth_call_stat_name(int stat)
{
    switch (stat)
    {
    case A_STR:
        return "strength";
    case A_DEX:
        return "dexterity";
    case A_CON:
        return "constitution";
    case A_GRA:
        return "grace";
    default:
        return "spirit";
    }
}

static int morgoth_call_stat_sustain(int stat)
{
    switch (stat)
    {
    case A_STR:
        return p_ptr->sustain_str;
    case A_DEX:
        return p_ptr->sustain_dex;
    case A_CON:
        return p_ptr->sustain_con;
    case A_GRA:
        return p_ptr->sustain_gra;
    default:
        return 0;
    }
}

static u32b morgoth_call_stat_sustain_flag(int stat)
{
    switch (stat)
    {
    case A_STR:
        return TR2_SUST_STR;
    case A_DEX:
        return TR2_SUST_DEX;
    case A_CON:
        return TR2_SUST_CON;
    case A_GRA:
        return TR2_SUST_GRA;
    default:
        return 0L;
    }
}

static void show_morgoth_call_first_screen(void)
{
    const char lines[][100] = {
        { "Then the hour struck its lowest note," },
        { "  and all paths under earth bent toward Angband." },
        { "" },
        { "Out of the black seat of the North there came a thought," },
        { "  heavy as iron and cold as the void between stars." },
        { "" },
        { "'Come down,' said the Dark Lord, 'for I have marked thee." },
        { "  Tarry above my throne, and a toll shall be taken.'" },
        { "" }
    };

    pause_with_text(lines, 4, 8, NULL, 0);
}

static void show_morgoth_call_drain_screen(int stat,
    morgoth_call_drain_result result)
{
    char lines[10][100];
    int n = 0;
    cptr stat_name = morgoth_call_stat_name(stat);

    strnfmt(lines[n++], sizeof(lines[0]),
        "Again the Dark Lord's summons rose.");
    strnfmt(lines[n++], sizeof(lines[0]),
        "His hand sought your %s.", stat_name);

    if (result == MORGOTH_CALL_DRAIN_TURIN)
    {
        strnfmt(lines[n++], sizeof(lines[0]),
            "A wrathful fire answered within your blood,");
        strnfmt(lines[n++], sizeof(lines[0]),
            "  and for this hour the shadow passed over you.");
    }
    else if (result == MORGOTH_CALL_DRAIN_RESISTED)
    {
        strnfmt(lines[n++], sizeof(lines[0]),
            "Your will held fast against the unseen chain,");
        strnfmt(lines[n++], sizeof(lines[0]),
            "  and the word of command broke like spent thunder.");
    }
    else
    {
        strnfmt(lines[n++], sizeof(lines[0]),
            "The toll was taken in silence.");
        strnfmt(lines[n++], sizeof(lines[0]),
            "  Your %s was diminished by the malice of the throne.",
            stat_name);
    }

    lines[n][0] = '\0';

    pause_with_text(lines, 4, 8, NULL, 0);
}

static morgoth_call_drain_result morgoth_call_try_drain_stat(
    int stat, int difficulty, morgoth_call_drain_log* drain_log)
{
    int resistance;
    int adjusted_difficulty;
    int check_result;
    u32b sustain_flag;

    if (drain_log)
    {
        memset(drain_log, 0, sizeof(*drain_log));
        drain_log->adjusted_difficulty = difficulty;
        drain_log->player_will = p_ptr->skill_use[S_WIL];
        drain_log->stat_drain_before = p_ptr->stat_drain[stat];
        drain_log->stat_drain_after = p_ptr->stat_drain[stat];
    }

    if (turin_resist_bad_effect())
        return MORGOTH_CALL_DRAIN_TURIN;

    resistance = morgoth_call_stat_sustain(stat);
    adjusted_difficulty = difficulty - (10 * resistance);
    check_result = skill_check(NULL, adjusted_difficulty, p_ptr->skill_use[S_WIL],
        PLAYER);

    if (drain_log)
    {
        drain_log->resistance = resistance;
        drain_log->adjusted_difficulty = adjusted_difficulty;
        drain_log->player_will = p_ptr->skill_use[S_WIL];
        drain_log->check_result = check_result;
        drain_log->skill_check_rolled = true;
    }

    if (check_result <= 0)
    {
        sustain_flag = morgoth_call_stat_sustain_flag(stat);
        if (sustain_flag)
            ident_resist(sustain_flag);
        return MORGOTH_CALL_DRAIN_RESISTED;
    }

    if (drain_log)
        drain_log->stat_drain_before = p_ptr->stat_drain[stat];

    if (dec_stat(stat, 1, false))
    {
        if (drain_log)
            drain_log->dec_stat_succeeded = true;
    }

    if (drain_log)
        drain_log->stat_drain_after = p_ptr->stat_drain[stat];

    return MORGOTH_CALL_DRAIN_TAKEN;
}

void process_morgoth_call_pressure(void)
{
    s32b stage;
    const s32b first_morgoth_stage = MORGOTH_DEPTH - 1;
    int stat;
    int difficulty;
    int escalation_before;
    int escalation_after;
    morgoth_call_drain_result result;
    morgoth_call_drain_log drain_log;

    if (!p_ptr || p_ptr->is_dead || p_ptr->game_type != 0)
        return;
    if (p_ptr->on_the_run || p_ptr->depth <= 0)
        return;
    if (p_ptr->morgoth_call_last_stage < 0)
        p_ptr->morgoth_call_last_stage = 0;
    p_ptr->morgoth_call_state &=
        (SAVEFILE_MORGOTH_CALL_SEEN
            | SAVEFILE_MORGOTH_CALL_ESCALATION_MASK);

    stage = min_depth_counter_stage();
    if (stage < first_morgoth_stage)
        return;

    if (!(p_ptr->morgoth_call_state & SAVEFILE_MORGOTH_CALL_SEEN))
    {
        p_ptr->morgoth_call_state = SAVEFILE_MORGOTH_CALL_SEEN;
        p_ptr->morgoth_call_last_stage = stage;
        do_cmd_note("Heard the summons of the Dark Lord", p_ptr->depth);
        log_info("Morgoth call: first summons stage=%d counter=%d step=%d "
                 "depth=%d; no drain attempted until next stage",
            (int)stage, min_depth_counter, min_depth_counter_step(),
            p_ptr->depth);
        show_morgoth_call_first_screen();
        return;
    }

    if (stage <= p_ptr->morgoth_call_last_stage)
        return;

    p_ptr->morgoth_call_last_stage = stage;

    stat = rand_int(A_MAX);
    difficulty = morgoth_call_current_difficulty();
    escalation_before =
        p_ptr->morgoth_call_state & SAVEFILE_MORGOTH_CALL_ESCALATION_MASK;
    result = morgoth_call_try_drain_stat(stat, difficulty, &drain_log);
    morgoth_call_advance_difficulty();
    escalation_after =
        p_ptr->morgoth_call_state & SAVEFILE_MORGOTH_CALL_ESCALATION_MASK;

    if (result == MORGOTH_CALL_DRAIN_TAKEN)
    {
        do_cmd_note(format("Dark Lord's summons drained %s",
                        morgoth_call_stat_name(stat)),
            p_ptr->depth);
    }

    if (drain_log.skill_check_rolled)
    {
        log_info("Morgoth call: drain attempt stage=%d counter=%d step=%d "
                 "depth=%d stat=%s difficulty=%d resistance=%d "
                 "adjusted_difficulty=%d player_will=%d check_result=%d "
                 "outcome=%s stat_drain=%d->%d dec_stat=%s "
                 "escalation=%d->%d",
            (int)stage, min_depth_counter, min_depth_counter_step(),
            p_ptr->depth, morgoth_call_stat_name(stat), difficulty,
            drain_log.resistance, drain_log.adjusted_difficulty,
            drain_log.player_will, drain_log.check_result,
            morgoth_call_drain_result_name(result),
            drain_log.stat_drain_before, drain_log.stat_drain_after,
            drain_log.dec_stat_succeeded ? "applied" : "not-applied",
            escalation_before, escalation_after);
    }
    else
    {
        log_info("Morgoth call: drain attempt stage=%d counter=%d step=%d "
                 "depth=%d stat=%s difficulty=%d outcome=%s "
                 "stat_drain=%d->%d escalation=%d->%d; no skill check rolled",
            (int)stage, min_depth_counter, min_depth_counter_step(),
            p_ptr->depth, morgoth_call_stat_name(stat), difficulty,
            morgoth_call_drain_result_name(result),
            drain_log.stat_drain_before, drain_log.stat_drain_after,
            escalation_before, escalation_after);
    }
    show_morgoth_call_drain_screen(stat, result);
}

void note_lost_greater_vault(void)
{
    char note[120];
    char* fmt = "Left without entering %s";
    int y, x;
    bool discovered = false;

    /* Handle lost greater vaults */
    if (g_vault_name[0] != '\0')
    {
        /* Analyze the actual map */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_info[y][x] & (CAVE_G_VAULT))
                    && (cave_info[y][x] & (CAVE_MARK)))
                {
                    discovered = true;
                }
            }
        }

        if (discovered)
        {
            strnfmt(note, sizeof(note), fmt, g_vault_name);
            do_cmd_note(note, p_ptr->depth);
        }

        g_vault_name[0] = '\0';
    }
}
