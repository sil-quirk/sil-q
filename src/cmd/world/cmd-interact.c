#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "cmd/world/cmd-interact-chest.h"
#include "ui/question.h"
#include <SDL3/SDL_timer.h>

#define INTERACTION_ROLL_ANIM_FRAME_MS 250

static bool is_open(int feat) { return (feat == FEAT_OPEN); }

static u32b interaction_roll_mix32(u32b x)
{
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    x *= 3266489917u;
    x ^= x >> 16;

    return x;
}

static u32b interaction_roll_visual_seed(cptr title, cptr action, int y, int x,
    int skill, int difficulty)
{
    Uint64 ticks = SDL_GetTicksNS();
    u32b seed = (u32b)ticks ^ (u32b)(ticks >> 32);

    seed ^= (u32b)(y + 257) * 2654435761u;
    seed ^= (u32b)(x + 263) * 2246822519u;
    seed ^= (u32b)(skill + 31) * 3266489917u;
    seed ^= (u32b)(difficulty + 17) * 668265263u;

    for (const unsigned char* p = (const unsigned char*)title; p && *p; p++)
        seed = interaction_roll_mix32(seed ^ (u32b)*p);
    for (const unsigned char* p = (const unsigned char*)action; p && *p; p++)
        seed = interaction_roll_mix32(seed ^ ((u32b)*p << 1));

    return interaction_roll_mix32(seed);
}

static int interaction_roll_visual_die(u32b seed, int frame, int salt,
    int previous, int sides)
{
    u32b x = seed;
    int die;

    if (sides < 1)
        sides = 1;

    x ^= (u32b)(frame + 1) * 1103515245u;
    x ^= (u32b)(salt + 1) * 374761393u;
    x = interaction_roll_mix32(x);

    die = (int)(x % (u32b)sides) + 1;
    if (die == previous)
        die = (die % sides) + 1;

    return die;
}

static void interaction_roll_format_total(
    char* buf, size_t buf_sz, cptr label, int die, int sides, int bonus)
{
    strnfmt(buf, buf_sz, "%-10s d%d %2d %c %d = %d", label, sides, die,
        (bonus < 0) ? '-' : '+', ABS(bonus), die + bonus);
}

static void interaction_roll_present_frame(void)
{
    Term_fresh();
    Term_xtra(TERM_XTRA_EVENT, 0);
    Term_fresh();
}

static void interaction_roll_render_overlay(cptr title, cptr action, int y,
    int x, const skill_roll_details* roll, int skill_die, int difficulty_die,
    bool final, bool blocking, int timeout_ms)
{
    char line[96];
    char result[96];
    int skill_total = skill_die + roll->skill;
    int difficulty_total = difficulty_die + roll->difficulty;
    int margin = skill_total - difficulty_total;

    sdl_question_menu_begin(title);
    if ((y >= 0) && (x >= 0))
        sdl_question_menu_set_anchor_grid(y, x);
    sdl_question_menu_set_desc(final ? "Final throw" : "Throwing...");

    sdl_question_menu_add_text(action, final ? TERM_L_WHITE : TERM_L_BLUE);

    interaction_roll_format_total(
        line, sizeof(line), "You", skill_die, roll->skill_sides, roll->skill);
    sdl_question_menu_add_text(line, TERM_WHITE);

    interaction_roll_format_total(
        line, sizeof(line), "Difficulty", difficulty_die,
        roll->difficulty_sides, roll->difficulty);
    sdl_question_menu_add_text(line, TERM_WHITE);

    if (final)
    {
        if (roll->result > 0)
        {
            strnfmt(result, sizeof(result), "Result     success by %d",
                roll->result);
            sdl_question_menu_add_text(result, TERM_L_GREEN);
        }
        else if (roll->result == 0)
        {
            sdl_question_menu_add_text("Result     failure (tie)", TERM_L_RED);
        }
        else
        {
            strnfmt(result, sizeof(result), "Result     failure by %d",
                ABS(roll->result));
            sdl_question_menu_add_text(result, TERM_L_RED);
        }

        if (roll->skill_curse_active)
        {
            strnfmt(line, sizeof(line), "Curse      lower player die kept (%d/%d)",
                roll->skill_die_primary, roll->skill_die_alt);
            sdl_question_menu_add_text(line, TERM_SLATE);
        }
        else if (roll->difficulty_curse_active)
        {
            strnfmt(line, sizeof(line),
                "Curse      lower difficulty die kept (%d/%d)",
                roll->difficulty_die_primary, roll->difficulty_die_alt);
            sdl_question_menu_add_text(line, TERM_SLATE);
        }
    }
    else
    {
        strnfmt(result, sizeof(result), "Margin     %+d", margin);
        sdl_question_menu_add_text(result, TERM_SLATE);
    }

    sdl_question_menu_finish();
    if (blocking)
    {
        sdl_question_menu_set_blocking_input(true);
    }
    else if (final)
    {
        sdl_question_menu_set_blocking_input(false);
        sdl_question_menu_set_nonblocking(true);
        sdl_question_menu_set_timeout_ms(timeout_ms);
    }
    interaction_roll_present_frame();
}

static void interaction_roll_add_final_result(const skill_roll_details* roll)
{
    char line[96];

    if (roll->result > 0)
    {
        strnfmt(line, sizeof(line), "Result     success by %d", roll->result);
        sdl_question_menu_add_text(line, TERM_L_GREEN);
    }
    else if (roll->result == 0)
    {
        sdl_question_menu_add_text("Result     failure (tie)", TERM_L_RED);
    }
    else
    {
        strnfmt(line, sizeof(line), "Result     failure by %d",
            ABS(roll->result));
        sdl_question_menu_add_text(line, TERM_L_RED);
    }
}

static void interaction_roll_add_final_throw(cptr label,
    const skill_roll_details* roll)
{
    char line[96];

    sdl_question_menu_add_text(label, TERM_L_WHITE);
    interaction_roll_format_total(line, sizeof(line), "You", roll->skill_die,
        roll->skill_sides, roll->skill);
    sdl_question_menu_add_text(line, TERM_WHITE);
    interaction_roll_format_total(line, sizeof(line), "Difficulty",
        roll->difficulty_die, roll->difficulty_sides, roll->difficulty);
    sdl_question_menu_add_text(line, TERM_WHITE);
    interaction_roll_add_final_result(roll);
}

/* Keep two consecutive interaction checks visible together after the second
 * animation replaces the first result overlay. */
void show_interaction_skill_roll_pair(cptr title, int y, int x,
    cptr first_label, const skill_roll_details* first,
    cptr second_label, const skill_roll_details* second)
{
    if (!Term || character_icky || !first || !second)
        return;

    sdl_question_menu_begin(title);
    if ((y >= 0) && (x >= 0))
        sdl_question_menu_set_anchor_grid(y, x);
    sdl_question_menu_set_desc("Final throws");

    interaction_roll_add_final_throw(first_label, first);
    interaction_roll_add_final_throw(second_label, second);

    sdl_question_menu_finish();
    sdl_question_menu_set_blocking_input(false);
    sdl_question_menu_set_nonblocking(true);
    sdl_question_menu_set_timeout_ms(get_sdl_dice_roll_overlay_ms());
    interaction_roll_present_frame();
}

/* Keep a consequence discovered immediately after a check visible alongside
 * that check's final throw. */
void show_interaction_skill_roll_status(cptr title, int y, int x,
    cptr roll_label, const skill_roll_details* roll, cptr status,
    byte status_attr)
{
    if (!Term || character_icky || !roll || !status)
        return;

    sdl_question_menu_begin(title);
    if ((y >= 0) && (x >= 0))
        sdl_question_menu_set_anchor_grid(y, x);
    sdl_question_menu_set_desc("Final throw");

    interaction_roll_add_final_throw(roll_label, roll);
    sdl_question_menu_add_text(status, status_attr);

    sdl_question_menu_finish();
    sdl_question_menu_set_blocking_input(false);
    sdl_question_menu_set_nonblocking(true);
    sdl_question_menu_set_timeout_ms(get_sdl_dice_roll_overlay_ms());
    interaction_roll_present_frame();
}

static int show_interaction_skill_roll_animation_actor_sided(
    monster_type* actor, cptr title,
    cptr action, int y, int x, int skill, int difficulty,
    int skill_sides, int difficulty_sides, skill_roll_details* roll)
{
    skill_roll_details local_roll;
    skill_roll_details preview_roll;
    bool saved_hide_cursor;
    int lock_ms;
    int overlay_ms;
    int elapsed_ms = 0;
    int frame = 0;
    int prev_skill_die = 0;
    int prev_difficulty_die = 0;
    int result;
    u32b visual_seed;

    if (!roll)
        roll = &local_roll;

    if (skill_sides < 1)
        skill_sides = 1;
    if (difficulty_sides < 1)
        difficulty_sides = 1;

    if (!Term || character_icky)
        return skill_check_details_sided(actor, skill, difficulty, NULL,
            skill_sides, difficulty_sides, roll);

    memset(&preview_roll, 0, sizeof(preview_roll));
    preview_roll.skill = skill;
    preview_roll.difficulty = difficulty;
    preview_roll.skill_sides = skill_sides;
    preview_roll.difficulty_sides = difficulty_sides;

    if (p_ptr)
    {
        p_ptr->redraw |= (PR_MAP);
        handle_stuff();
    }

    saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    lock_ms = get_sdl_dice_roll_lock_ms();
    overlay_ms = get_sdl_dice_roll_overlay_ms();
    visual_seed = interaction_roll_visual_seed(title, action, y, x, skill,
        difficulty);

    while (elapsed_ms < lock_ms)
    {
        int skill_die = interaction_roll_visual_die(visual_seed, frame, 0,
            prev_skill_die, skill_sides);
        int difficulty_die = interaction_roll_visual_die(visual_seed, frame, 1,
            prev_difficulty_die, difficulty_sides);
        int delay_ms = MIN(INTERACTION_ROLL_ANIM_FRAME_MS,
            lock_ms - elapsed_ms);

        interaction_roll_render_overlay(title, action, y, x, &preview_roll,
            skill_die, difficulty_die, false, true, 0);
        Term_xtra(TERM_XTRA_DELAY, delay_ms);
        elapsed_ms += delay_ms;
        prev_skill_die = skill_die;
        prev_difficulty_die = difficulty_die;
        frame++;
    }

    result = skill_check_details_sided(actor, skill, difficulty, NULL,
        skill_sides, difficulty_sides, roll);
    interaction_roll_render_overlay(title, action, y, x, roll, roll->skill_die,
        roll->difficulty_die, true, false, overlay_ms);

    hide_cursor = saved_hide_cursor;
    interaction_roll_present_frame();

    return result;
}

int show_interaction_skill_roll_animation_actor(monster_type* actor, cptr title,
    cptr action, int y, int x, int skill, int difficulty,
    skill_roll_details* roll)
{
    return show_interaction_skill_roll_animation_actor_sided(actor, title,
        action, y, x, skill, difficulty, 10, 10, roll);
}

/*
 * Convenience wrapper: the acting creature is the player.  Existing callers
 * (disarm, lockpick, bash) use this; the monster trap-engagement code calls the
 * _actor variant above so a monster's own Perception roll is shown/resolved.
 */
int show_interaction_skill_roll_animation(cptr title, cptr action, int y,
    int x, int skill, int difficulty, skill_roll_details* roll)
{
    return show_interaction_skill_roll_animation_actor_sided(
        PLAYER, title, action, y, x, skill, difficulty, 10, 10, roll);
}

int show_interaction_skill_roll_animation_sided(cptr title, cptr action,
    int y, int x, int skill, int difficulty, int skill_sides,
    int difficulty_sides, skill_roll_details* roll)
{
    return show_interaction_skill_roll_animation_actor_sided(PLAYER, title,
        action, y, x, skill, difficulty, skill_sides, difficulty_sides, roll);
}

/*
 * Return true if the given feature is a closed door
 */
static bool is_closed(int feat)
{
    return (((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL))
        || feat == FEAT_WARDED || feat == FEAT_WARDED2 || feat == FEAT_WARDED3);
}

/*
 * Return true if the given feature is a trap
 */
static bool is_trap(int feat)
{
    bool test_trap = false;

    if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
        test_trap = true;

    return (test_trap);
}

/*
 * Return the number of doors/traps around (or under) the character.
 */
static int count_feats(int* y, int* x, bool (*test)(int feat), bool under)
{
    int d;
    int xx, yy;
    int count = 0; /* Count how many matches */

    /* Check around (and under) the character */
    for (d = 0; d < 9; d++)
    {
        /* if not searching under player continue */
        if ((d == 8) && !under)
            continue;

        /* Extract adjacent (legal) location */
        yy = p_ptr->py + ddy_ddd[d];
        xx = p_ptr->px + ddx_ddd[d];

        /* Paranoia */
        if (!in_bounds_fully(yy, xx))
            continue;

        /* Must have knowledge */
        if (!(cave_info[yy][xx] & (CAVE_MARK)))
            continue;

        /* Not looking for this feature */
        if (!((*test)(cave_feat[yy][xx])))
            continue;

        /* Count it */
        ++count;

        /* Remember the location of the last door found */
        *y = yy;
        *x = xx;
    }

    /* All done */
    return count;
}

/*
 * Return the number of chests around (or under) the character.
 * If requested, count only trapped chests.
 */
static int count_chests(int* y, int* x, bool trapped)
{
    int d, count, o_idx;

    object_type* o_ptr;

    /* Count how many matches */
    count = 0;

    /* Check around (and under) the character */
    for (d = 0; d < 9; d++)
    {
        /* Extract adjacent (legal) location */
        int yy = p_ptr->py + ddy_ddd[d];
        int xx = p_ptr->px + ddx_ddd[d];

        /* No (visible) chest is there */
        if ((o_idx = chest_check(yy, xx)) == 0)
            continue;

        /* Grab the object */
        o_ptr = &o_list[o_idx];

        /* Already open */
        if (o_ptr->pval == 0)
            continue;

        /* No (known) traps here */
        if (trapped && !chest_trap_minigame
            && (!object_known_p(o_ptr) || (o_ptr->pval < 0)
                || !object_chest_trap_flags(o_ptr)))
        {
            continue;
        }

        /* Count it */
        ++count;

        /* Remember the location of the last chest found */
        *y = yy;
        *x = xx;
    }

    /* All done */
    return count;
}

/*
 * Extract a "direction" which will move one step from the player location
 * towards the given "target" location (or "5" if no motion necessary).
 */
static int coords_to_dir(int y, int x)
{
    return (motion_dir(p_ptr->py, p_ptr->px, y, x));
}

static bool get_interact_dir(cptr prompt, bool (*test)(int y, int x),
    bool under, int* dp);
static bool grid_is_open_target(int y, int x);
static bool grid_is_disarm_target(int y, int x);
static bool grid_is_tunnel_target(int y, int x);
static bool do_cmd_bash_aux(int y, int x, skill_roll_details* out_roll,
    bool* out_rolled);

typedef struct door_minigame_retry_state
{
    bool active;
    bool pause_before_prompt;
    int y;
    int x;
    char previous[240];
} door_minigame_retry_state;

typedef enum door_minigame_choice
{
    DOOR_CHOICE_LOCKPICK,
    DOOR_CHOICE_BASH,
    DOOR_CHOICE_LEAVE
} door_minigame_choice;

#define DOOR_MINIGAME_RETRY_DELAY_MS 1000
#define DOOR_JAM_CHANCE_PER_MISS 10
#define DOOR_JAM_CHANCE_MAX 75
#define DOOR_LOCKPICK_BASE_DIFFICULTY 8
#define FLOOR_TRAP_DISARM_BASE_DIFFICULTY 3

static door_minigame_retry_state door_retry;

static void door_minigame_clear_retry(void)
{
    memset(&door_retry, 0, sizeof(door_retry));
}

static bool door_minigame_retry_target(int* y, int* x)
{
    if (!door_retry.active)
        return false;

    if (y)
        *y = door_retry.y;
    if (x)
        *x = door_retry.x;
    return true;
}

static void door_minigame_schedule_retry(int y, int x, cptr previous,
    bool pause_before_prompt)
{
    door_retry.active = true;
    door_retry.pause_before_prompt = pause_before_prompt;
    door_retry.y = y;
    door_retry.x = x;
    SDL_strlcpy(door_retry.previous, previous ? previous : "",
        sizeof(door_retry.previous));
    p_ptr->command_new = 'o';
}

static int door_condition_penalty(void)
{
    int penalty = 0;

    if (p_ptr->blind || no_light() || p_ptr->image)
        penalty += 5;
    if (p_ptr->confused)
        penalty += 5;
    return penalty;
}

static int door_lockpick_difficulty(int y, int x)
{
    int power = cave_feat[y][x] - FEAT_DOOR_HEAD;

    return (power & 0x07) + DOOR_LOCKPICK_BASE_DIFFICULTY
        + door_condition_penalty();
}

static int door_bash_difficulty(int y, int x)
{
    return (cave_feat[y][x] - FEAT_DOOR_HEAD) & 0x07;
}

/* Overall chance that a lockpick attempt jams the door.  Each possible failed
 * throw contributes its actual per-point jam chance up to the cap, so the
 * displayed percentage describes the whole attempt rather than only failures. */
static int door_lockpick_jam_percent(int skill, int difficulty)
{
    int total_jam_chance = 0;
    int outcomes = 0;
    int alternate_throws = (p_ptr && p_ptr->cursed) ? 10 : 1;

    for (int skill_die = 1; skill_die <= 10; skill_die++)
    {
        for (int skill_alt = 1; skill_alt <= alternate_throws; skill_alt++)
        {
            int kept_skill_die = (p_ptr && p_ptr->cursed)
                ? MIN(skill_die, skill_alt) : skill_die;

            for (int difficulty_die = 1; difficulty_die <= 10;
                 difficulty_die++)
            {
                int result = skill + kept_skill_die
                    - difficulty - difficulty_die;

                outcomes++;
                if (result < 0)
                    total_jam_chance += MIN(DOOR_JAM_CHANCE_MAX,
                        (0 - result) * DOOR_JAM_CHANCE_PER_MISS);
            }
        }
    }

    return outcomes
        ? (total_jam_chance + outcomes / 2) / outcomes : 0;
}

static int door_minigame_question(int y, int x)
{
    ui_question_option options[3];
    int actions[3];
    int count = 0;
    int choice;
    int power = cave_feat[y][x] - FEAT_DOOR_HEAD;
    bool stuck = power >= 0x08;
    char desc[480];
    char lockpick_label[64];
    char bash_label[64];

    strnfmt(bash_label, sizeof(bash_label), "Bash: %d%%",
        player_skill_check_success_percent(p_ptr->stat_use[A_STR] * 2,
            door_bash_difficulty(y, x), 10, 10));

    desc[0] = '\0';
    if (door_retry.active && door_retry.y == y && door_retry.x == x
        && door_retry.previous[0])
    {
        /* A repeated command can reach this retry before request_command()
         * consumes the queued open command.  Consume it here as well so a
         * successful retry cannot leave a stale extra open command behind. */
        if (p_ptr->command_new == 'o')
            p_ptr->command_new = 0;
        SDL_strlcpy(desc, door_retry.previous, sizeof(desc));
        SDL_strlcat(desc, " ", sizeof(desc));
    }

    if (stuck)
    {
        SDL_strlcat(desc,
            "The lock is jammed. The door can only be bashed open.",
            sizeof(desc));
        options[count]
            = (ui_question_option){ 'b', bash_label, TERM_ORANGE, false };
        actions[count++] = DOOR_CHOICE_BASH;
        options[count]
            = (ui_question_option){ 'l', "Leave", TERM_SLATE, false };
        actions[count++] = DOOR_CHOICE_LEAVE;
    }
    else
    {
        strnfmt(lockpick_label, sizeof(lockpick_label),
            "Lockpick: %d%% (jam: %d%%)",
            player_skill_check_success_percent(p_ptr->skill_use[S_PER],
                door_lockpick_difficulty(y, x), 10, 10),
            door_lockpick_jam_percent(p_ptr->skill_use[S_PER],
                door_lockpick_difficulty(y, x)));
        SDL_strlcat(desc,
            "The door is locked. Pick the lock or force it open. A failed "
            "lockpick may jam it.", sizeof(desc));
        options[count]
            = (ui_question_option){
                'p', lockpick_label, TERM_L_GREEN, false
            };
        actions[count++] = DOOR_CHOICE_LOCKPICK;
        options[count]
            = (ui_question_option){ 'b', bash_label, TERM_ORANGE, false };
        actions[count++] = DOOR_CHOICE_BASH;
        options[count]
            = (ui_question_option){ 'l', "Leave", TERM_SLATE, false };
        actions[count++] = DOOR_CHOICE_LEAVE;
    }

    if (door_retry.active && door_retry.pause_before_prompt
        && Term && !character_icky)
    {
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, DOOR_MINIGAME_RETRY_DELAY_MS);
    }

    choice = ui_question_ask(stuck ? "Stuck door" : "Locked door", desc,
        options, count, y, x, 0);
    if (choice < 0 || choice >= count)
        return DOOR_CHOICE_LEAVE;
    return actions[choice];
}

/*
 * Determine if a given grid may be "opened"
 */
static bool do_cmd_open_test(int y, int x)
{
    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Must be a closed door */
    if (!cave_known_closed_door_bold(y, x))
    {
        /* Message */
        message(MSG_NOTHING_TO_OPEN, 0, "You see nothing there to open.");

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Perform the basic "open" command on doors
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool do_cmd_open_aux(int y, int x)
{
    int score, power, difficulty, result;
    skill_roll_details roll;

    bool more = false;

    /* Verify legality */
    if (!do_cmd_open_test(y, x))
        return (false);

    if (lockpick_minigame
        && cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
    {
        door_minigame_choice choice = (door_minigame_choice)
            door_minigame_question(y, x);
        char previous[240];

        door_minigame_clear_retry();

        if (choice == DOOR_CHOICE_LEAVE)
        {
            p_ptr->energy_use = 0;
            return false;
        }

        if (choice == DOOR_CHOICE_BASH)
        {
            bool rolled = false;

            (void)do_cmd_bash_aux(y, x, &roll, &rolled);
            if (cave_known_closed_door_bold(y, x))
            {
                if (rolled)
                {
                    SDL_strlcpy(previous,
                        "Bash failed. The door holds.",
                        sizeof(previous));
                }
                else
                {
                    SDL_strlcpy(previous,
                        "Bash failed. The door remains closed.",
                        sizeof(previous));
                }
                flush();
                door_minigame_schedule_retry(y, x, previous, true);
                more = true;
            }
            return more;
        }

        score = p_ptr->skill_use[S_PER];
        power = (cave_feat[y][x] - FEAT_DOOR_HEAD) & 0x07;
        difficulty = door_lockpick_difficulty(y, x);
        result = show_interaction_skill_roll_animation("Picking the lock",
            "Working the lockpick", y, x, score, difficulty, &roll);

        if (result > 0)
        {
            message(MSG_OPENDOOR, 0, "You have picked the lock.");
            cave_set_feat(y, x, FEAT_OPEN);
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
            return false;
        }
        else
        {
            int jam_chance = MIN(DOOR_JAM_CHANCE_MAX,
                MAX(0, 0 - result) * DOOR_JAM_CHANCE_PER_MISS);
            bool jammed = jam_chance > 0 && rand_int(100) < jam_chance;

            flush();
            if (jammed)
            {
                cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
                message(MSG_LOCKPICK_FAIL, 0,
                    "The pick twists in the lock, jamming it fast!");
                SDL_strlcpy(previous,
                    "Lockpick failed and jammed the door.",
                    sizeof(previous));
            }
            else
            {
                message(MSG_LOCKPICK_FAIL, 0,
                    "You failed to pick the lock.");
                SDL_strlcpy(previous,
                    "Lockpick failed. The door remains locked.",
                    sizeof(previous));
            }
            door_minigame_schedule_retry(y, x, previous, true);
            return true;
        }
    }

    /* Legacy jammed-door prompt. */
    if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
    {
        if (get_check_near(y, x,
                "Stuck door, do you want to bash it? "))
        {
            more = do_cmd_bash_aux(y, x, NULL, NULL);
        }
    }

    /* Legacy locked-door behavior.  The setting explicitly suppresses the
     * new throw overlay when disabled. */
    else if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
    {
        /* Get the score in favour (=perception) */
        score = p_ptr->skill_use[S_PER];

        /* Determine door power based on the door power (1 to 7)*/
        power = cave_feat[y][x] - FEAT_DOOR_HEAD;

        // Base difficulty is the door power plus the lockpick baseline.
        difficulty = power + DOOR_LOCKPICK_BASE_DIFFICULTY;

        /* Penalize some conditions */
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5;
        if (p_ptr->confused)
            difficulty += 5;

        result = skill_check(PLAYER, score, difficulty, NULL);

        /* Success */
        if (result > 0)
        {
            /* Message */
            message(MSG_OPENDOOR, 0, "You have picked the lock.");

            /* Open the door */
            cave_set_feat(y, x, FEAT_OPEN);

            /* Update the visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
        }

        /* Failure */
        else
        {
            /* Failure */
            flush();

            /* Message */
            message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");

            /* We may keep trying */
            more = true;
        }
    }

    /* Closed door */
    else
    {
        /* Open the door */
        cave_set_feat(y, x, FEAT_OPEN);

        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        /* Sound */
        sound(MSG_OPENDOOR);
    }

    /* Result */
    return (more);
}

/*
 * Open a closed/locked/jammed door or a closed/locked chest.
 */
void do_cmd_open(void)
{
    int y = 0, x = 0, dir;

    s16b o_idx = 0;

    bool more = false;
    bool retry_target = false;
    bool door_target = false;
    bool chest_target = false;

    int num_doors, num_chests;

    door_target = lockpick_minigame && door_minigame_retry_target(&y, &x);
    chest_target = !door_target && chest_trap_minigame
        && chest_minigame_retry_target(&y, &x);
    retry_target = door_target || chest_target;

    if (retry_target)
    {
        bool adjacent = (ABS(y - p_ptr->py) <= 1)
            && (ABS(x - p_ptr->px) <= 1);

        if (!adjacent || y < 0 || x < 0 || y >= p_ptr->cur_map_hgt
            || x >= p_ptr->cur_map_wid
            || (door_target && !cave_known_closed_door_bold(y, x))
            || (chest_target
                && (!(o_idx = chest_check(y, x))
                    || o_list[o_idx].pval == 0)))
        {
            door_minigame_clear_retry();
            chest_minigame_clear_retry();
            msg_print("That lock is no longer available.");
            return;
        }

        dir = motion_dir(p_ptr->py, p_ptr->px, y, x);
        p_ptr->command_dir = dir;
    }
    else
    {
        /* Count closed doors */
        num_doors = count_feats(&y, &x, is_closed, false);

        /* Count chests (locked) */
        num_chests = count_chests(&y, &x, false);

        if ((num_doors + num_chests) == 0)
        {
            msg_print("There is nothing in your square (or adjacent) to open.");
            return;
        }

        /* Honour a pre-supplied direction, else pick a target interactively */
        if (p_ptr->command_dir)
            dir = p_ptr->command_dir;
        else if (!get_interact_dir("Open what?", grid_is_open_target, false,
                     &dir))
            return;

        p_ptr->command_dir = dir;

        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];

        /* Check for chests */
        o_idx = chest_check(y, x);
    }

    /* Verify legality */
    if (!o_idx && !do_cmd_open_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (!retry_target && confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];

        /* Check for chest */
        o_idx = chest_check(y, x);
    }

    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Chest */
    else if (o_idx)
    {
        /* Open the chest */
        more = do_cmd_open_chest(y, x, o_idx);
    }

    /* Door */
    else
    {
        /* Open the door */
        more = do_cmd_open_aux(y, x);
    }

    /* Cancel repeat unless we may continue */
    if (!more)
        disturb(0, 0);
}

/*
 * Determine if a given grid may be "closed"
 */
static bool do_cmd_close_test(int y, int x)
{
    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Require open/broken door */
    if ((cave_feat[y][x] != FEAT_OPEN) && (cave_feat[y][x] != FEAT_BROKEN))
    {
        /* Message */
        msg_print("You see nothing there to close.");

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Perform the basic "close" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_close_aux(int y, int x)
{
    /* Verify legality */
    if (!do_cmd_close_test(y, x))
        return (false);

    /* Broken door */
    if (cave_feat[y][x] == FEAT_BROKEN)
    {
        /* Message */
        msg_print("The door appears to be broken.");
        return (false);
    }
    /* Ward the open door */
    else if (singing(SNG_THRESHOLDS))
    {
        int difficulty = (c_info[p_ptr->pcharacter].flags & UNQ_SNG_MEL) ? 15 : 0;
        int result = skill_check(
            PLAYER, ability_bonus(S_SNG, SNG_THRESHOLDS), difficulty, NULL);
        if (result > 9)
        {
            msg_print("You close the door, singing a song of trust unbroken.");
            cave_set_feat(y, x, FEAT_WARDED3);
        }
        else if (result > 0)
        {
            msg_print("You close the door, singing charms of binding.");
            cave_set_feat(y, x, FEAT_WARDED2);
        }
        else
        {
            msg_print("You close the door, singing words of warding.");
            cave_set_feat(y, x, FEAT_WARDED);
        }
    }
    else
    {
        /* Close the open door */
        cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
    }

    /* Update the visuals */
    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

    /* Sound */
    sound(MSG_SHUTDOOR);

    /* Result */
    return (false);
}

/*
 * Close an open door.
 */
/*
 * Interactive "which adjacent grid?" selection used by the door, disarm,
 * open and tunnel commands.  Candidates are collected with a grid
 * predicate; each is highlighted on the map and a small popup prompt is
 * shown (never the top message row) by get_grid_choice_dir.  A single
 * candidate resolves immediately; with none, false is returned.
 *
 * Returns true and stores the chosen direction in *dp; false on cancel.
 */
static bool get_interact_dir(cptr prompt, bool (*test)(int y, int x),
    bool under, int* dp)
{
    int ys[9], xs[9], dirs[9];
    int count = 0;

#ifdef ALLOW_REPEAT
    /* Reuse the stored direction while a command repeats (e.g. bashing). */
    if (repeat_pull(dp))
        return true;
#endif

    /* Collect adjacent (and optionally under) candidate grids */
    for (int d = 0; d < (under ? 9 : 8); d++)
    {
        int yy = p_ptr->py + ddy_ddd[d];
        int xx = p_ptr->px + ddx_ddd[d];

        if (!in_bounds_fully(yy, xx))
            continue;
        if (!test(yy, xx))
            continue;

        ys[count] = yy;
        xs[count] = xx;
        dirs[count] = coords_to_dir(yy, xx);
        count++;
    }

    if (count == 0)
        return false;

    /* A single candidate needs no interaction. */
    if (count == 1)
    {
        *dp = dirs[0];
#ifdef ALLOW_REPEAT
        repeat_push(*dp);
#endif
        return true;
    }

    if (!get_grid_choice_dir(prompt, ys, xs, dirs, count, dp))
        return false;

#ifdef ALLOW_REPEAT
    repeat_push(*dp);
#endif
    return true;
}

/* Grid predicates for get_interact_dir */
static bool grid_is_known_open_door(int y, int x)
{
    return (cave_info[y][x] & (CAVE_MARK)) && is_open(cave_feat[y][x]);
}

static bool grid_is_known_closed_door(int y, int x)
{
    return (cave_info[y][x] & (CAVE_MARK)) && is_closed(cave_feat[y][x]);
}

static bool grid_is_closed_chest(int y, int x)
{
    s16b o_idx = chest_check(y, x);

    return (o_idx != 0) && (o_list[o_idx].pval != 0);
}

static bool grid_is_known_trapped_chest(int y, int x)
{
    s16b o_idx = chest_check(y, x);
    object_type* o_ptr;

    if (!o_idx)
        return false;

    o_ptr = &o_list[o_idx];
    return (o_ptr->pval > 0)
        && (chest_trap_minigame ? chest_trap_presence_known(o_ptr)
                                : object_known_p(o_ptr))
        && object_chest_trap_flags(o_ptr);
}

static bool grid_is_open_target(int y, int x)
{
    return grid_is_known_closed_door(y, x) || grid_is_closed_chest(y, x);
}

static bool grid_is_known_trap(int y, int x)
{
    /* A rewired trap is harmless to the player and is not a disarm target. */
    return (cave_info[y][x] & (CAVE_MARK)) && is_trap(cave_feat[y][x])
        && !cave_rewired[y][x];
}

static bool grid_is_disarm_target(int y, int x)
{
    return grid_is_known_trap(y, x) || grid_is_known_trapped_chest(y, x)
        || (chest_trap_minigame && grid_is_closed_chest(y, x));
}

static bool grid_is_tunnel_target(int y, int x)
{
    if (!(cave_info[y][x] & (CAVE_MARK)))
        return false;
    if (cave_floor_bold(y, x))
        return false;
    if (cave_known_closed_door_bold(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_WALL_PERM)
        return false;

    return true;
}

static bool get_door_dir(cptr prompt, bool (*test)(int y, int x), int* dp)
{
    return get_interact_dir(prompt, test, false, dp);
}

/*
 * ------------------------------------------------------------------------
 * Grid interaction popup: the question overlay shown when right-clicking
 * (or long-pressing) an adjacent door, trap, wall, vein, chest, skeleton
 * or empty/dark square.  Describes the grid and offers every applicable
 * action; the answer is translated into a normal game command so the
 * existing command code (open/close/bash/disarm/tunnel/alter/walk) runs
 * unchanged.
 * ------------------------------------------------------------------------
 */

/* Disarm difficulty of a trap feature; false when it cannot be disarmed.
 * Keep the powers in sync with do_cmd_disarm_aux. */
bool trap_disarm_power(int feat, int* power)
{
    int p = 0;

    switch (feat)
    {
    case FEAT_TRAP_PIT:
    case FEAT_TRAP_SPIKED_PIT:
    case FEAT_TRAP_ROOST:
    case FEAT_TRAP_WEB:
        return false;
    case FEAT_TRAP_false_FLOOR:
        p = 1;
        break;
    case FEAT_TRAP_DART:
        p = 3;
        break;
    case FEAT_TRAP_GAS_CONF:
    case FEAT_TRAP_GAS_MEMORY:
        p = 5;
        break;
    case FEAT_TRAP_ALARM:
        p = 2;
        break;
    case FEAT_TRAP_FLASH:
        p = 4;
        break;
    case FEAT_TRAP_CALTROPS:
        p = 1;
        break;
    case FEAT_TRAP_DEADFALL:
        p = 7;
        break;
    case FEAT_TRAP_ACID:
        p = 1;
        break;
    case FEAT_TRAP_IMPRISONMENT:
        p = 4;
        break;
    default:
        p = 0;
        break;
    }

    if (power)
        *power = p;
    return true;
}

/* Keep the floor-trap preview and the actual disarm roll on the same values. */
static int trap_disarm_score(void)
{
    int score = p_ptr->skill_use[S_PER];

    if (p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS])
        score += 5;
    return score;
}

static int trap_disarm_difficulty(int power)
{
    int difficulty = FLOOR_TRAP_DISARM_BASE_DIFFICULTY + power
        + p_ptr->depth / 6;

    if (p_ptr->blind || no_light() || p_ptr->image)
        difficulty += 5;
    if (p_ptr->confused)
        difficulty += 5;
    return difficulty;
}

/*
 * Whether a trap can be "rewired" (re-keyed to catch monsters) by a player who
 * has the Rewire Traps ability.  Only disarmable, offensive "device" traps
 * qualify -- rewiring happens through a successful disarm, and the effect must
 * make sense when a monster steps onto it.  Pits/webs/roosts are not disarmable;
 * alarm/memory-gas/false-floor/imprisonment have no useful monster-victim effect.
 */
bool trap_is_rewireable(int feat)
{
    switch (feat)
    {
    case FEAT_TRAP_DART:
    case FEAT_TRAP_GAS_CONF:
    case FEAT_TRAP_CALTROPS:
    case FEAT_TRAP_DEADFALL:
    case FEAT_TRAP_ACID:
        return true;
    default:
        return false;
    }
}

/* Short flavour line for a trap feature. */
static cptr trap_flavor_text(int feat)
{
    switch (feat)
    {
    case FEAT_TRAP_false_FLOOR:
        return "The floor here is a fragile shell over a drop to the level "
               "below.";
    case FEAT_TRAP_PIT:
        return "A simple pit. Not deadly, but climbing out takes time.";
    case FEAT_TRAP_SPIKED_PIT:
        return "A pit lined with cruel spikes.";
    case FEAT_TRAP_DART:
        return "A concealed dart-thrower aimed at whoever treads here.";
    case FEAT_TRAP_GAS_CONF:
        return "A hidden bladder of confusing vapours.";
    case FEAT_TRAP_GAS_MEMORY:
        return "A hidden bladder of vapours that fog the memory.";
    case FEAT_TRAP_ALARM:
        return "A trip-wire rigged to raise a clamour and wake the level.";
    case FEAT_TRAP_FLASH:
        return "An alchemical charge that erupts in blinding light.";
    case FEAT_TRAP_CALTROPS:
        return "Sharp caltrops strewn to lame the unwary.";
    case FEAT_TRAP_ROOST:
        return "A roost of bats that bursts upward when disturbed.";
    case FEAT_TRAP_WEB:
        return "A great web of sticky strands.";
    case FEAT_TRAP_DEADFALL:
        return "A mass of rock rigged to come crashing down.";
    case FEAT_TRAP_ACID:
        return "A spray of corrosive liquid waits beneath this square.";
    case FEAT_TRAP_IMPRISONMENT:
        return "A rune of binding that holds the unwary fast.";
    case FEAT_GLYPH:
        return "A glyph of warding.";
    default:
        return "A hidden danger.";
    }
}

/* The best digging tool carried (wielded weapon first), or NULL. */
static object_type* grid_question_best_digger(int* out_score)
{
    object_type* o_ptr = &inventory[INVEN_WIELD];
    object_type* best = NULL;
    int best_score = 0;
    u32b f1, f2, f3;

    object_flags(o_ptr, &f1, &f2, &f3);
    if (o_ptr->k_idx && (f1 & (TR1_TUNNEL)))
    {
        best = o_ptr;
        best_score = o_ptr->pval;
    }
    else
    {
        for (int i = 0; i < INVEN_PACK; i++)
        {
            o_ptr = &inventory[i];
            if (!o_ptr->k_idx)
                continue;

            object_flags(o_ptr, &f1, &f2, &f3);
            if ((f1 & (TR1_TUNNEL)) && (o_ptr->pval > best_score))
            {
                best = o_ptr;
                best_score = o_ptr->pval;
            }
        }
    }

    if (out_score)
        *out_score = best_score;
    return best;
}

/* Append a sentence to a description buffer with a joining space. */
static void grid_question_append(char* buf, size_t buflen, cptr text)
{
    if (!text || !text[0])
        return;
    if (buf[0])
        SDL_strlcat(buf, " ", buflen);
    SDL_strlcat(buf, text, buflen);
}

/*
 * True when right-clicking the given adjacent grid should open the
 * interaction popup (rather than fall through to the describe/recall
 * popup).  Must stay in sync with grid_interact_question below.
 */
bool grid_interact_available(int y, int x)
{
    int dir;

    if (!p_ptr || !character_dungeon || !in_bounds(y, x))
        return false;

    /* Adjacent squares only: these are the ctrl+direction alt actions */
    dir = coords_to_dir(y, x);
    if (dir < 1 || dir > 9 || dir == 5)
        return false;
    if ((y != p_ptr->py + ddy[dir]) || (x != p_ptr->px + ddx[dir]))
        return false;

    /* Alert thralls are peaceful quest givers, so expose Talk here. */
    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
    {
        if (is_alert_thrall(&mon_list[cave_m_idx[y][x]]))
            return true;

        return false;
    }

    /* Dark/unknown squares: strike into the darkness */
    if (!(cave_info[y][x] & (CAVE_MARK | CAVE_SEEN)))
        return true;

    /* Known doors, traps, walls and rubble */
    if (is_open(cave_feat[y][x]) || (cave_feat[y][x] == FEAT_BROKEN))
        return true;
    if (cave_known_closed_door_bold(y, x))
        return true;
    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
        return true;
    if (cave_wall_bold(y, x) || (cave_feat[y][x] == FEAT_RUBBLE))
        return true;

    /* Chests and unsearched skeletons */
    if (chest_check(y, x))
        return true;
    if (cave_o_idx[y][x])
    {
        object_type* o_ptr = &o_list[cave_o_idx[y][x]];

        if ((o_ptr->tval == TV_SKELETON)
            && !object_is_searched_skeleton(o_ptr) && o_ptr->marked)
        {
            return true;
        }

        /* Other visible objects keep the describe popup */
        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            if (o_ptr->k_idx && o_ptr->marked)
                return false;
        }
    }

    /* Empty floor: strike at the square without stepping in */
    if (cave_floorlike_bold(y, x))
        return true;

    return false;
}

/*
 * Build and run the interaction popup for an adjacent grid.  On success
 * stores the chosen game command and direction and returns true; returns
 * false when the player cancels (or nothing applies).
 */
bool grid_interact_question(int y, int x, int* out_command, int* out_dir)
{
    ui_question_option options[6];
    char commands[6];
    int count = 0;
    int dir;
    int feat;
    int choice;
    s16b chest_o_idx;
    bool step_choice[6];
    char title[80];
    char desc[480];
    char line[160];
    char disarm_label[64];

    if (out_command)
        *out_command = 0;
    if (out_dir)
        *out_dir = 0;

    if (!grid_interact_available(y, x))
        return false;

    dir = coords_to_dir(y, x);
    feat = cave_feat[y][x];
    title[0] = '\0';
    desc[0] = '\0';
    memset(step_choice, 0, sizeof(step_choice));

    /* These targets own richer cyclic overlays.  A pointer/long-press has
     * already selected the grid, so route straight to the canonical minigame
     * instead of presenting a redundant Pick/Bash or Open/Disarm popup. */
    if (lockpick_minigame && cave_known_closed_door_bold(y, x)
        && feat >= FEAT_DOOR_HEAD + 0x01 && feat <= FEAT_DOOR_TAIL)
    {
        if (out_command)
            *out_command = 'o';
        if (out_dir)
            *out_dir = dir;
        return true;
    }
    chest_o_idx = chest_check(y, x);
    if (chest_trap_minigame && chest_o_idx
        && o_list[chest_o_idx].pval != 0)
    {
        if (out_command)
            *out_command = 'o';
        if (out_dir)
            *out_dir = dir;
        return true;
    }

#define GRID_Q_ADD_EX(cmd_, key_, label_, attr_, disabled_)                   \
    do                                                                        \
    {                                                                         \
        options[count].key = (key_);                                          \
        options[count].label = (label_);                                      \
        options[count].attr = (attr_);                                        \
        options[count].disabled = (disabled_);                                \
        commands[count] = (cmd_);                                             \
        count++;                                                              \
    } while (0)
#define GRID_Q_ADD(cmd_, key_, label_, attr_)                                 \
    GRID_Q_ADD_EX((cmd_), (key_), (label_), (attr_), false)

    /* --- Alert thrall --- */
    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml
        && is_alert_thrall(&mon_list[cave_m_idx[y][x]]))
    {
        monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
        char m_name[80];

        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
        strnfmt(title, sizeof(title), "%^s", m_name);
        SDL_strlcpy(desc,
            "This captive is alert, but not hostile. You can speak with the "
            "thrall to hear the request, offer the needed item, or claim an "
            "earned reward.",
            sizeof(desc));
        GRID_Q_ADD(';', 't', "Talk", TERM_L_GREEN);
    }

    /* --- Dark / unknown square --- */
    else if (!(cave_info[y][x] & (CAVE_MARK | CAVE_SEEN)))
    {
        SDL_strlcpy(title, "Darkness", sizeof(title));
        SDL_strlcpy(desc,
            "You cannot make out what lies there. You could strike into the "
            "darkness without stepping in - an unseen enemy might lurk "
            "there, or your blow may just find a wall.",
            sizeof(desc));
        GRID_Q_ADD('/', 's', "Strike at it", TERM_L_RED);
    }

    /* --- Open / broken doors --- */
    else if (is_open(feat))
    {
        SDL_strlcpy(title, "Open door", sizeof(title));
        SDL_strlcpy(desc,
            "An open doorway. Closing it would slow pursuers and block line "
            "of sight.",
            sizeof(desc));
        GRID_Q_ADD('c', 'c', "Close it", TERM_L_GREEN);
    }
    else if (feat == FEAT_BROKEN)
    {
        SDL_strlcpy(title, "Broken door", sizeof(title));
        SDL_strlcpy(desc,
            "The door has been smashed beyond use; it can no longer be "
            "closed.",
            sizeof(desc));
        GRID_Q_ADD(0, 0, "Never mind", TERM_SLATE);
    }

    /* --- Closed, locked, stuck and warded doors --- */
    else if (cave_known_closed_door_bold(y, x))
    {
        bool warded = (feat == FEAT_WARDED) || (feat == FEAT_WARDED2)
            || (feat == FEAT_WARDED3);
        int power = (feat >= FEAT_DOOR_HEAD) ? (feat - FEAT_DOOR_HEAD) : 0;
        int bash_power = power & 0x07;

        if (power >= 0x08)
        {
            /* Stuck door: bashing is the only way through */
            SDL_strlcpy(title, "Stuck door", sizeof(title));
            strnfmt(desc, sizeof(desc),
                "The door is stuck fast (jam %d). It cannot be opened "
                "normally: it must be forced with your shoulder, and the "
                "crash will carry. Bashing tests your Strength.",
                bash_power);
            GRID_Q_ADD('b', 'b', "Bash it open", TERM_ORANGE);
        }
        else if (power >= 0x01)
        {
            /* Locked door */
            SDL_strlcpy(title, "Locked door", sizeof(title));
            strnfmt(desc, sizeof(desc),
                "The door is locked (lock difficulty %d). Picking the lock "
                "quietly tests your Perception; bashing it down tests your "
                "Strength and makes a great noise.",
                power + DOOR_LOCKPICK_BASE_DIFFICULTY);
            GRID_Q_ADD('o', 'o', "Pick the lock", TERM_L_GREEN);
            GRID_Q_ADD('b', 'b', "Bash it open", TERM_ORANGE);
        }
        else
        {
            /* Plain closed (possibly warded) door */
            SDL_strlcpy(title, warded ? "Warded door" : "Closed door",
                sizeof(title));
            SDL_strlcpy(desc,
                "A closed door, and it is not locked. Opening it is quiet; "
                "bashing it down is fast but loud, and may break the door "
                "for good.",
                sizeof(desc));
            if (warded)
            {
                grid_question_append(desc, sizeof(desc),
                    "Words of warding glimmer about its frame.");
            }
            GRID_Q_ADD('o', 'o', "Open it", TERM_L_GREEN);
            GRID_Q_ADD('b', 'b', "Bash it open", TERM_ORANGE);
        }
    }

    /* --- Traps (known) --- */
    else if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
    {
        int power = 0;
        bool disarmable = trap_disarm_power(feat, &power);
        cptr name = (f_name + f_info[feat].name);
        bool rewired = (cave_rewired[y][x] != 0);
        bool can_rewire = (!rewired && disarmable
            && p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS]
            && trap_is_rewireable(feat));

        if (rewired)
            strnfmt(title, sizeof(title), "%^s (rewired)", name);
        else
            strnfmt(title, sizeof(title), "%^s", name);
        SDL_strlcpy(desc, trap_flavor_text(feat), sizeof(desc));

        if (rewired)
        {
            /* Harmless to the player -- no disarm option is offered. */
            grid_question_append(desc, sizeof(desc),
                "You have rewired this trap: it is harmless to you, and may "
                "catch monsters that cross it.");
        }
        else if (can_rewire)
        {
            strnfmt(disarm_label, sizeof(disarm_label), "Rewire: %d%%",
                player_skill_check_success_percent(trap_disarm_score(),
                    trap_disarm_difficulty(power), 10, 10));
            grid_question_append(desc, sizeof(desc),
                "A bad failure may set it off; the wider your successful "
                "margin, the harder foes find it to notice or undo.");
            /* Same command as disarm: do_cmd_disarm_aux re-keys it when you
             * have the ability (see the rewire branch there). */
            GRID_Q_ADD('D', 'd', disarm_label, TERM_L_BLUE);
        }
        else if (disarmable)
        {
            strnfmt(disarm_label, sizeof(disarm_label), "Disarm: %d%%",
                player_skill_check_success_percent(trap_disarm_score(),
                    trap_disarm_difficulty(power), 10, 10));
            grid_question_append(desc, sizeof(desc),
                "A bad failure may set it off.");
            GRID_Q_ADD('D', 'd', disarm_label, TERM_L_GREEN);
        }
        else
        {
            strnfmt(line, sizeof(line), "The %s cannot be disarmed.", name);
            grid_question_append(desc, sizeof(desc), line);
        }

        GRID_Q_ADD(';', 'w', "Step onto it", TERM_L_RED);
        step_choice[count - 1] = true;
    }

    /* --- Walls, veins and rubble --- */
    else if (cave_wall_bold(y, x) || (feat == FEAT_RUBBLE))
    {
        int digging_score = 0;
        object_type* digger = grid_question_best_digger(&digging_score);
        int difficulty;

        if (feat == FEAT_WALL_PERM)
        {
            SDL_strlcpy(title, "Unbreakable wall", sizeof(title));
            SDL_strlcpy(desc,
                "This wall has stood since the delving of the fortress; no "
                "tool you could carry will bite on it.",
                sizeof(desc));
            GRID_Q_ADD(0, 0, "Never mind", TERM_SLATE);
        }
        else
        {
            if (feat == FEAT_RUBBLE)
            {
                difficulty = TUNNEL_DIFFICULTY_RUBBLE;
                SDL_strlcpy(title, "Pile of rubble", sizeof(title));
                SDL_strlcpy(desc,
                    "Broken rock blocks the way. It is the easiest of "
                    "obstacles to dig through (difficulty 1).",
                    sizeof(desc));
            }
            else if (feat == FEAT_QUARTZ)
            {
                level_partition_kind part_kind
                    = level_partition_kind_for_point(y, x);
                bool in_chasm_area
                    = (cave_info[y][x] & CAVE_CHASM_AREA) != 0;
                bool loot_ground = ((part_kind == LEVEL_PART_CAVEY)
                    || (part_kind == LEVEL_PART_BIG_CAVE))
                    && ((cave_info[y][x] & CAVE_ROOM) != 0) && !in_chasm_area;
                bool star_ground
                    = (part_kind == LEVEL_PART_CHASM) && in_chasm_area;

                difficulty = TUNNEL_DIFFICULTY_QUARTZ;
                SDL_strlcpy(title, "Quartz vein", sizeof(title));
                SDL_strlcpy(desc,
                    "A vein of milky quartz seams the rock (digging "
                    "difficulty 2). Miners tell that veins in great caverns "
                    "can hold gems below 500 ft and even mithril below 600 "
                    "ft, and that veins in the chasm's depths may yield "
                    "star-iron.",
                    sizeof(desc));
                if ((loot_ground || star_ground) && (p_ptr->depth >= 10))
                {
                    grid_question_append(desc, sizeof(desc),
                        "This one lies in promising ground.");
                }
            }
            else
            {
                /* Granite (secret doors look the same; say nothing) */
                difficulty = TUNNEL_DIFFICULTY_GRANITE;
                SDL_strlcpy(title, "Granite wall", sizeof(title));
                SDL_strlcpy(desc,
                    "A wall of solid granite (digging difficulty 3).",
                    sizeof(desc));
            }

            if (digger)
            {
                char o_name[80];
                bool can_tunnel = (digging_score >= difficulty)
                    && (p_ptr->stat_use[A_STR] >= difficulty);

                object_desc(o_name, sizeof(o_name), digger, false, -1);
                if (can_tunnel)
                {
                    strnfmt(line, sizeof(line),
                        "Your %s (digging %d) is up to the task, though the "
                        "noise will carry.",
                        o_name, digging_score);
                }
                else if (digging_score >= difficulty)
                {
                    strnfmt(line, sizeof(line),
                        "Your %s (digging %d) could bite here, but you lack "
                        "the Strength (%d needed).",
                        o_name, digging_score, difficulty);
                }
                else
                {
                    strnfmt(line, sizeof(line),
                        "Your %s (digging %d) is not up to it.", o_name,
                        digging_score);
                }
                grid_question_append(desc, sizeof(desc), line);
                GRID_Q_ADD_EX('T', 't', "Tunnel through", TERM_ORANGE,
                    !can_tunnel);
                if (!can_tunnel)
                    GRID_Q_ADD(0, 0, "Never mind", TERM_SLATE);
            }
            else
            {
                grid_question_append(desc, sizeof(desc),
                    "You carry no shovel or mattock to dig with.");
                GRID_Q_ADD_EX('T', 't', "Tunnel through", TERM_ORANGE, true);
                GRID_Q_ADD(0, 0, "Never mind", TERM_SLATE);
            }
        }
    }

    /* --- Chests --- */
    else if (chest_check(y, x))
    {
        s16b o_idx = chest_check(y, x);
        object_type* o_ptr = &o_list[o_idx];
        char o_name[80];

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        strnfmt(title, sizeof(title), "%^s", o_name);

        if (o_ptr->pval == 0)
        {
            SDL_strlcpy(desc, "The chest stands open and empty.",
                sizeof(desc));
            GRID_Q_ADD(0, 0, "Never mind", TERM_SLATE);
        }
        else
        {
            SDL_strlcpy(desc, "A closed chest.", sizeof(desc));
            if (grid_is_known_trapped_chest(y, x))
            {
                grid_question_append(desc, sizeof(desc),
                    "You have found a trap on it. A separate disarm attempt "
                    "can fail safely; opening it will try the lock and then "
                    "the trap, setting off the trap if that attempt fails.");
                GRID_Q_ADD('D', 'd', "Disarm the trap", TERM_L_GREEN);
            }
            else if (!object_known_p(o_ptr))
            {
                grid_question_append(desc, sizeof(desc),
                    "It has not been searched for traps.");
            }
            GRID_Q_ADD('o', 'o', "Open it", TERM_L_GREEN);
        }
    }

    /* --- Skeletons --- */
    else if (cave_o_idx[y][x]
        && (o_list[cave_o_idx[y][x]].tval == TV_SKELETON)
        && !object_is_searched_skeleton(&o_list[cave_o_idx[y][x]]))
    {
        char o_name[80];

        object_desc(o_name, sizeof(o_name), &o_list[cave_o_idx[y][x]], true,
            3);
        strnfmt(title, sizeof(title), "%^s", o_name);
        SDL_strlcpy(desc,
            "The bones of some unfortunate. Searching them may turn up "
            "something of use.",
            sizeof(desc));
        GRID_Q_ADD('/', 's', "Search it", TERM_L_BLUE);
    }

    /* --- Empty floor --- */
    else
    {
        SDL_strlcpy(title, "Empty square", sizeof(title));
        SDL_strlcpy(desc,
            "Nothing lies there that you can see. You could strike at the "
            "square without stepping in - an unseen enemy might lurk there.",
            sizeof(desc));
        GRID_Q_ADD('/', 's', "Strike at it", TERM_L_RED);
    }

#undef GRID_Q_ADD
#undef GRID_Q_ADD_EX

    if (count == 0)
        return false;

    choice = ui_question_ask(title, desc, options, count, y, x, 0);
    if ((choice < 0) || (choice >= count) || !commands[choice])
        return false;

    /* Stepping onto a trap from the popup is deliberate: don't ask again */
    if (step_choice[choice])
        player_allow_trap_step(y, x);

    if (out_command)
        *out_command = commands[choice];
    if (out_dir)
        *out_dir = dir;
    return true;
}

void do_cmd_close(void)
{
    int y, x, dir;

    bool more = false;

    /* No open door adjacent */
    if (count_feats(&y, &x, is_open, false) == 0)
    {
        msg_print("There is no adjacent door to close.");
        return;
    }

    /* Honour a pre-supplied direction, else pick a door interactively */
    if (p_ptr->command_dir)
        dir = p_ptr->command_dir;
    else if (!get_door_dir("Close which door?", grid_is_known_open_door, &dir))
        return;

    p_ptr->command_dir = dir;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Verify legality */
    if (!do_cmd_close_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Door */
    else
    {
        /* Close door */
        more = do_cmd_close_aux(y, x);
    }

    /* Cancel repeat unless told not to */
    if (!more)
        disturb(0, 0);
}

/*
 * Exchange places with a monster.
 */

static bool do_cmd_tunnel_test(int y, int x)
{
    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Must be a wall or rubble */
    if (cave_floor_bold(y, x))
    {
        /* Message */
        msg_print("You see nothing there to tunnel.");

        /* Nope */
        return (false);
    }
    if (cave_known_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You cannot tunnel through a door. Try bashing it.");

        /* Nope */
        return (false);
    }

    /* Permanent */
    if (cave_feat[y][x] == FEAT_WALL_PERM)
    {
        /* Message */
        msg_print("You cannot tunnel any further in that direction.");

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Tunnel through wall.  Assumes valid location.
 *
 * Note that it is impossible to "extend" rooms past their
 * outer walls (which are actually part of the room).
 *
 * Attempting to do so will produce floor grids which are not part
 * of the room, and whose "illumination" status do not change with
 * the rest of the room.
 */
static bool twall(int y, int x)
{
    /* Paranoia -- Require a wall or door or some such */
    if (cave_floor_bold(y, x))
        return (false);

    /* Sound */
    sound(MSG_DIG);

    /* Forget the wall */
    // cave_info[y][x] &= ~(CAVE_MARK);

    /* Granite */
    if (cave_feat[y][x] >= FEAT_WALL_EXTRA && cave_feat[y][x] <= FEAT_WALL_SOLID)
    {
        /* Regular granite walls - just convert to rubble, no special drops */
        cave_set_feat(y, x, FEAT_RUBBLE);
    }

    /* Quartz */
    else if (cave_feat[y][x] == FEAT_QUARTZ)
    {
        /* Cave and big-cave quartz can yield gems or mithril; chasm-tagged quartz yields star-iron. */
        int depth = p_ptr->depth;
        level_partition_kind part_kind = level_partition_kind_for_point(y, x);
        bool in_chasm_area = (cave_info[y][x] & CAVE_CHASM_AREA) != 0;
        bool in_cave_loot_quartz = ((part_kind == LEVEL_PART_CAVEY)
            || (part_kind == LEVEL_PART_BIG_CAVE))
            && ((cave_info[y][x] & CAVE_ROOM) != 0)
            && !in_chasm_area;
        bool allow_mithril = in_cave_loot_quartz;
        bool allow_star_iron = (part_kind == LEVEL_PART_CHASM) && in_chasm_area;
        
        /* Base 10% chance at depth 10, scaling up to 25% at depth 20+ */
        int special_chance = 10 + depth;
        if (special_chance > 25) special_chance = 25;
        
        log_debug("twall: digging vein at (%d,%d) depth=%d part=%d cave_info=0x%04x in_cave_loot_quartz=%d in_chasm=%d allow_mithril=%d allow_star_iron=%d special_chance=%d%%",
                  y, x, depth, part_kind, cave_info[y][x], in_cave_loot_quartz, in_chasm_area, allow_mithril, allow_star_iron, special_chance);
        
        if ((allow_mithril || allow_star_iron) && depth >= 10 && rand_int(100) < special_chance)
        {
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;
            object_wipe(i_ptr);
            
            log_debug("twall: PASSED chance check! Attempting drop at depth=%d", depth);
            
            bool try_mithril = allow_mithril
                && (depth >= MITHRIL_VEIN_MIN_DEPTH) && (rand_int(100) < 45);

            log_debug("twall: try_star_iron=%d try_mithril=%d", allow_star_iron, try_mithril);

            if (allow_star_iron)
            {
                /* Drop star iron */
                s16b k_idx = lookup_kind(TV_METAL, SV_METAL_STAR_IRON);
                if (k_idx > 0)
                {
                    object_prep(i_ptr, k_idx);
                    drop_near(i_ptr, -1, y, x);
                    msg_print("You find a jagged shard of star iron!");
                }
            }
            else if (try_mithril)
            {
                /* Drop mithril */
                s16b k_idx = lookup_kind(TV_METAL, SV_METAL_MITHRIL);
                if (k_idx > 0)
                {
                    object_prep(i_ptr, k_idx);
                    drop_near(i_ptr, -1, y, x);
                    msg_print("You find a gleaming piece of mithril!");
                }
            }
            else
            {
                /* Try to drop a gem using profiled generation to ensure we get a gem */
                log_debug("twall: Attempting gem drop via profile");
                drop_profile gem_profile;
                drop_profile_default(&gem_profile);
                gem_profile.weight_weapon = 0;
                gem_profile.weight_armor = 0;
                gem_profile.weight_jewelry = 0;
                gem_profile.weight_supply = 120;
                gem_profile.supply_potion = 0;
                gem_profile.supply_herb = 0;
                gem_profile.supply_gem = 50;
                gem_profile.supply_staff = 0;
                gem_profile.supply_light = 0;
                gem_profile.supply_arrows = 0;

                if (drop_generate_object_profiled(depth, DROP_QUALITY_NORMAL,
                        DROP_TYPE_STAFF, 0, false, &gem_profile, i_ptr))
                {
                    log_debug("twall: gem generated successfully, tval=%d", i_ptr->tval);
                    if (i_ptr->tval == TV_GEM)
                    {
                        char gem_name[80];

                        i_ptr->number = 1;
                        object_aware(i_ptr);
                        object_desc(gem_name, sizeof(gem_name), i_ptr, true, 0);
                        drop_near(i_ptr, -1, y, x);
                        msg_format("%^s glitters in the rubble!", gem_name);
                    }
                    else
                    {
                        drop_near(i_ptr, -1, y, x);
                        msg_print("A gem glitters in the rubble!");
                    }
                }
                else
                {
                    log_debug("twall: gem generation FAILED");
                }
            }
        }
        
        /* Leave a pile of rubble */
        cave_set_feat(y, x, FEAT_RUBBLE);
    }

    /* Rubble */
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Clear the rubble */
        cave_set_feat(y, x, FEAT_FLOOR);
    }

    /* Secret doors */
    else
    {
        /* Leave a closed door */
        place_closed_door(y, x);
    }

    /* Update the visuals */
    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

    /* Result */
    return (true);
}

/*
 * Perform the basic "tunnel" command
 *
 * Assumes that no monster is blocking the destination
 *
 * Uses "twall" (above) to do all "terrain feature changing".
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_tunnel_aux(int y, int x)
{
    int i;
    int item;
    bool more = false;
    bool digger_choice = false;
    int difficulty;
    int digging_score = 0;
    char o_name[80];
    char success_message[80];
    char failure_message[80];
    object_type* o_ptr;
    object_type* digger_ptr = NULL; // default to soothe compiler warnings

    u32b f1, f2, f3;

    /* Verify legality */
    if (!do_cmd_tunnel_test(y, x))
        return (false);

    // examine the wielded weapon
    o_ptr = &inventory[INVEN_WIELD];
    object_flags(o_ptr, &f1, &f2, &f3);

    // if it is a digger, then use it
    if (f1 & (TR1_TUNNEL))
    {
        digging_score = o_ptr->pval;
        digger_ptr = o_ptr;
    }
    else
    {
        // find one or more diggers in the pack
        for (i = 0; i < INVEN_PACK; i++)
        {
            o_ptr = &inventory[i];

            object_flags(o_ptr, &f1, &f2, &f3);

            if (f1 & (TR1_TUNNEL))
            {
                if (digging_score > 0)
                {
                    digger_choice = true;
                }
                digging_score = o_ptr->pval;
                digger_ptr = o_ptr;
            }
        }

        if (digger_choice)
        {
            /* Restrict the choices */
            item_tester_hook = item_tester_hook_digger;

            /* Get an item */
            if (!open_inventory_item_select_menu(USE_INVEN,
                    "Use which digger? ",
                    "You are not carrying a shovel or mattock.", &item))
                return (false);
            else
            {
                /* Get the object */
                if (item >= 0)
                {
                    digger_ptr = &inventory[item];
                }
                else
                {
                    digger_ptr = &o_list[0 - item];
                }

                digging_score = digger_ptr->pval;
            }
        }
    }

    // abort if you have no digger
    if (digging_score == 0)
    {
        // confused players trying to dig without a digger waste their turn
        // (otherwise control-dir is safe in a corridor)
        if (p_ptr->confused)
        {
            if (cave_feat[y][x] == FEAT_RUBBLE)
                msg_print("You bump into the rubble.");
            else
                msg_print("You bump into the wall.");

            return (false);
        }

        else
        {
            msg_print("You are not carrying a shovel or mattock.");

            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;

            // don't take a turn
            p_ptr->energy_use = 0;

            return (false);
        }
    }

    // get the short name of the item
    object_desc(o_name, sizeof(o_name), digger_ptr, false, -1);

    /* Granite */
    if (cave_feat[y][x] >= FEAT_WALL_EXTRA)
    {
        difficulty = TUNNEL_DIFFICULTY_GRANITE;
        SDL_strlcpy(success_message, "You break through the granite.",
            sizeof(success_message));

        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are unable to break the granite with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are not strong enough to break the granite.");
        }
    }
    /* Quartz */
    else if (cave_feat[y][x] >= FEAT_QUARTZ)
    {
        difficulty = TUNNEL_DIFFICULTY_QUARTZ;
        SDL_strlcpy(success_message, "You shatter the quartz.",
            sizeof(success_message));

        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are unable to break the quartz with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are not strong enough to break the quartz.");
        }
    }
    /* Rubble */
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        difficulty = TUNNEL_DIFFICULTY_RUBBLE;
        SDL_strlcpy(
            success_message, "You clear the rubble.", sizeof(success_message));

        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are unable to shift the rubble with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are not strong enough to shift the rubble.");
        }
    }
    /* Secret doors */
    else
    {
        difficulty = TUNNEL_DIFFICULTY_GRANITE;
        SDL_strlcpy(success_message, "You uncover a secret door.",
            sizeof(success_message));

        if (difficulty > digging_score)
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are unable to break the granite with your %s.", o_name);
        }
        else
        {
            strnfmt(failure_message, sizeof(failure_message),
                "You are not strong enough to break the granite.");
        }
    }

    /* test for success */
    if ((difficulty <= digging_score) && (difficulty <= p_ptr->stat_use[A_STR]))
    {
        u32b f1, f2, f3;
        object_flags(digger_ptr, &f1, &f2, &f3);

        /* Make a lot of noise */
        monster_perception(true, false, -10);

        twall(y, x);
        msg_print(success_message);

        // Possibly identify the digger
        if (!object_known_p(digger_ptr) && (f1 & (TR1_TUNNEL)))
        {
            char o_short_name[80];

            /* Short, pre-identification object description */
            object_desc(
                o_short_name, sizeof(o_short_name), digger_ptr, false, 0);

            msg_format(
                "You notice that your %s is especially suited to tunneling.",
                o_short_name);

            if (object_uses_smithing_difficulty(digger_ptr))
            {
                player_mark_object_experienced(digger_ptr);
            }
            else
            {
                char o_full_name[80];

                ident(digger_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), digger_ptr, true, 3);

                msg_format("You are wielding %s.", o_full_name);
            }
        }
    }

    else
    {
        msg_print(failure_message);

        // confused players trying to dig without a digger waste their turn
        // (otherwise control-dir is safe in a corridor)
        if (!p_ptr->confused)
        {
            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;

            // don't take a turn
            p_ptr->energy_use = 0;
        }

        return (false);
    }

    // Break the truce if creatures see
    break_truce(false);

    // provoke attacks of opportunity from adjacent monsters
    attacks_of_opportunity(0, 0);

    /* Result */
    return (more);
}

/*
 * Tunnel through "walls" (including rubble and secret doors)
 *
 * Digging is only possible with a "digger" weapon.
 */
void do_cmd_tunnel(void)
{
    int y, x, dir;

    bool more = false;

    int num_targets = 0;

    /* Count adjacent diggable grids */
    for (int d = 0; d < 8; d++)
    {
        y = p_ptr->py + ddy_ddd[d];
        x = p_ptr->px + ddx_ddd[d];

        if (in_bounds_fully(y, x) && grid_is_tunnel_target(y, x))
            num_targets++;
    }

    if (!p_ptr->command_dir && (num_targets == 0))
    {
        msg_print("There is nothing nearby that you can tunnel through.");
        return;
    }

    /* Honour a pre-supplied direction, else pick a target interactively */
    if (p_ptr->command_dir)
        dir = p_ptr->command_dir;
    else if (!get_interact_dir("Tunnel where?", grid_is_tunnel_target, false,
                 &dir))
        return;

    p_ptr->command_dir = dir;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Oops */
    if (!do_cmd_tunnel_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Walls */
    else
    {
        /* Tunnel through walls */
        more = do_cmd_tunnel_aux(y, x);
    }

    /* Cancel repetition unless we can continue */
    if (!more)
        disturb(0, 0);
}

/*
 * Determine if a given grid may be "disarmed"
 */
static bool do_cmd_disarm_test(int y, int x)
{
    bool can_disarm = false;

    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Require an actual trap */
    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
    {
        can_disarm = true;
    }

    /*not a trap*/
    else
        msg_print("You see nothing there to disarm.");

    /* Okay */
    return (can_disarm);
}

/*
 * Attempts to break free of a web.
 */
bool break_free_of_web(void)
{
    int difficulty = p_ptr->depth / 2;
    int score = MAX(p_ptr->stat_use[A_STR] * 2,
        difficulty - 8); // capped so you always have some chance
    u32b f1, f2, f3;
    object_type* o_ptr = &inventory[INVEN_WIELD];

    /* Disturb the player */
    disturb(0, 0);

    object_flags(o_ptr, &f1, &f2, &f3);

    bool appropriate_weapon
        = (f1 & TR1_SLAY_SPIDER || f1 & TR1_SHARPNESS || f1 & TR1_SHARPNESS2);

    if (appropriate_weapon)
    {
        difficulty -= 5;
    }

    // Free action helps a lot
    if (p_ptr->free_act)
        difficulty -= 10 * p_ptr->free_act;

    // Spider bane bonus helps
    difficulty -= spider_bane_bonus();
    difficulty -= artifact_spider_bane_bonus();

    if (skill_check(PLAYER, score, difficulty, NULL) <= 0)
    {
        msg_print("You fail to break free of the web.");

        /* Take a turn */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        return (false);
    }
    else
    {
        if (appropriate_weapon)
            msg_print("You cut yourself free!");
        else
            msg_print("You break free!");

        /* Forget the trap */
        cave_info[p_ptr->py][p_ptr->px] &= ~(CAVE_MARK);

        /* Remove the trap */
        cave_set_feat(p_ptr->py, p_ptr->px, FEAT_FLOOR);

        return (true);
    }
}

/*
 * Perform the basic "disarm" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
bool do_cmd_disarm_aux(int y, int x)
{
    int score, difficulty, result;
    int power = 0; // default to soothe compiler warnings
    skill_roll_details roll;

    cptr name;

    bool more = false;

    /* Verify legality */
    if (!do_cmd_disarm_test(y, x))
        return (false);

    /* Get the trap name */
    name = (f_name + f_info[cave_feat[y][x]].name);

    /* Get the score in favour (=perception), including trap mastery. */
    score = trap_disarm_score();

    /* Determine trap power based on the dungeon level (1--7)*/
    // power = 1 + p_ptr->depth / 5;
    // if (p_ptr->depth == 0) power = 7;

    switch (cave_feat[y][x])
    {
    case FEAT_TRAP_false_FLOOR:
    {
        power = 1;
        break;
    }
    case FEAT_TRAP_PIT:
    {
        msg_format("You cannot disarm the %s.", name);
        return (false);
    }
    case FEAT_TRAP_SPIKED_PIT:
    {
        msg_format("You cannot disarm the %s.", name);
        return (false);
    }
    case FEAT_TRAP_DART:
    {
        power = 3;
        break;
    }
    case FEAT_TRAP_GAS_CONF:
    {
        power = 5;
        break;
    }
    case FEAT_TRAP_GAS_MEMORY:
    {
        power = 5;
        break;
    }
    case FEAT_TRAP_ALARM:
    {
        power = 2;
        break;
    }
    case FEAT_TRAP_FLASH:
    {
        power = 4;
        break;
    }
    case FEAT_TRAP_CALTROPS:
    {
        power = 1;
        break;
    }
    case FEAT_TRAP_ROOST:
    {
        msg_format("You cannot disarm the %s.", name);
        return (false);
    }
    case FEAT_TRAP_WEB:
    {
        if ((p_ptr->py == y) && (p_ptr->px == x))
        {
            int more = break_free_of_web();
            return (!more);
        }
        else
        {
            msg_format("You cannot disarm the %s.", name);
            return (false);
        }
    }
    case FEAT_TRAP_DEADFALL:
    {
        power = 7;
        break;
    }
    case FEAT_TRAP_ACID:
    {
        power = 1;
        break;
    }
    case FEAT_TRAP_IMPRISONMENT:
    {
        power = 4;
        break;
    }
    }

    // Base difficulty is the trap power, made harder with depth so that
    // skilled characters cannot trivially disarm deep traps (and so more of
    // them are set off when an attempt fails badly).
    difficulty = trap_disarm_difficulty(power);

    /* With the Rewire Traps ability, a successful disarm of a suitable trap
     * re-keys it instead of removing it (see below). */
    bool rewiring = p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS]
        && trap_is_rewireable(cave_feat[y][x]) && !cave_rewired[y][x];

    // perform the check
    result = show_interaction_skill_roll_animation(
        rewiring ? "Rewiring trap" : "Disarming trap",
        rewiring ? "Re-keying the mechanism" : "Testing the mechanism", y, x,
        score, difficulty, &roll);

    /* Success, always succeed with player trap */
    if (result > 0)
    {
        int feat = cave_feat[y][x];

        /* Re-key a suitable trap instead of removing it: it becomes safe for
         * you and may catch monsters.  How hard it is for a monster to notice
         * or undo scales with your margin.  (Disarming an already-rewired trap
         * removes it, so you can undo.) */
        if (rewiring)
        {
            int quality = power + result;
            if (quality < 1)
                quality = 1;
            if (quality > 255)
                quality = 255;

            cave_rewired[y][x] = (byte)quality;

            /* Keep the trap known and refresh its look immediately (the violet
             * mark must show without waiting for the next full redraw). */
            cave_info[y][x] |= (CAVE_MARK);
            lite_spot(y, x);
            p_ptr->redraw |= (PR_MAP);

            msg_format("You re-key the %s to catch your foes.", name);
        }
        else
        {
            /* Special message for glyphs. */
            if (feat == FEAT_GLYPH)
                msg_format("You have scuffed the %s.", name);

            /* Normal message otherwise */
            else
                msg_format("You have disarmed the %s.", name);

            /* Forget the trap */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Remove the trap */
            cave_set_feat(y, x, FEAT_FLOOR);
        }
    }

    /* Failure by a small amount allows one to keep trying */
    else if (result > -3)
    {
        /* Failure */
        flush();

        /* Message */
        msg_format("You failed to disarm the %s.", name);

        /* We may keep trying */
        more = true;
    }

    /* Failure by a larger amount sets off the trap */
    else
    {
        /* Message */
        monster_swap(p_ptr->py, p_ptr->px, y, x);
        msg_format("You set off the %s!", name);

        /* Hit the trap */
        hit_trap(y, x);
    }

    /* Result */
    return (more);
}

/*
 * Disarms a trap, or a chest
 */
void do_cmd_disarm(void)
{
    int y = 0, x = 0, dir;

    s16b o_idx;

    bool more = false;

    int num_traps, num_chests;

    /* Count visible traps */
    num_traps = count_feats(&y, &x, is_trap, true);

    /* Count chests (trapped) */
    num_chests = count_chests(&y, &x, true);

    if ((num_traps + num_chests) == 0)
    {
        msg_print("There is nothing in your square (or adjacent) to disarm.");
        return;
    }

    /* Honour a pre-supplied direction, else pick a target interactively */
    if (p_ptr->command_dir)
        dir = p_ptr->command_dir;
    else if (!get_interact_dir("Disarm what?", grid_is_disarm_target, true,
                 &dir))
        return;

    p_ptr->command_dir = dir;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Check for chests */
    o_idx = chest_check(y, x);

    /* Verify legality */
    if (!o_idx && !do_cmd_disarm_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];

        /* Check for chests */
        o_idx = chest_check(y, x);
    }

    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Chest */
    else if (o_idx)
    {
        /* Disarm the chest */
        more = do_cmd_disarm_chest(y, x, o_idx);
    }

    /* Disarm trap */
    else
    {
        /* Disarm the trap */
        more = do_cmd_disarm_aux(y, x);
    }

    /* Cancel repeat unless told not to */
    if (!more)
        disturb(0, 0);
}

/*
 * Determine if a given grid may be "bashed"
 */
static bool do_cmd_bash_test(int y, int x)
{
    /* Must have knowledge */
    if (!(cave_info[y][x] & (CAVE_MARK)))
    {
        /* Message */
        msg_print("You see nothing there.");

        /* Nope */
        return (false);
    }

    /* Require a door */
    if (!cave_known_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You see no door there to bash.");

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Perform the basic "bash" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_bash_aux(int y, int x, skill_roll_details* out_roll,
    bool* out_rolled)
{
    int score, difficulty, power, result;
    skill_roll_details roll;

    bool more = false;
    bool success = false;

    if (out_rolled)
        *out_rolled = false;

    /* Verify legality */
    if (!do_cmd_bash_test(y, x))
        return (false);

    // store the action type
    p_ptr->previous_action[0] = ACTION_BASH;

    // It is hard to get out of a pit
    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        int pit_difficulty;

        if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_PIT)
            pit_difficulty = 10;
        else
            pit_difficulty = 15;

        /* Disturb the player */
        disturb(0, 0);

        if (check_hit(pit_difficulty, false))
        {
            msg_print("You try to climb out of the pit, but fail.");

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_BASH;

            return (false);
        }
        else
        {
            msg_print("You climb out of the pit.");
        }
    }

    // It is hard to get out of a web
    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        if (!break_free_of_web())
        {
            // store the action type
            p_ptr->previous_action[0] = ACTION_BASH;

            return (false);
        }
    }

    /* If it was actually a door */
    if (cave_known_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You slam into the door!");

        // get the score in favour (=str)
        score = p_ptr->stat_use[A_STR] * 2;

        // get the difficulty
        power = ((cave_feat[y][x] - FEAT_DOOR_HEAD) & 0x07);

        // the base difficulty is the door power
        difficulty = 0;
        difficulty += power;

        result = show_interaction_skill_roll_animation("Bashing the door",
            "Putting your shoulder into it", y, x, score, difficulty, &roll);
        if (out_roll)
            *out_roll = roll;
        if (out_rolled)
            *out_rolled = true;

        if (result > 0)
        {
            success = true;

            if (cave_feat[y][x] == FEAT_SECRET)
            {
                if (singing(SNG_SILENCE))
                {
                    /* Message */
                    message(
                        MSG_BASHDOOR, 0, "A door opens with a muffled crash!");
                }
                else
                {
                    /* Message */
                    message(MSG_BASHDOOR, 0, "A door crashes open!");
                }
            }
            else
            {
                if (singing(SNG_SILENCE))
                {
                    /* Message */
                    message(MSG_BASHDOOR, 0,
                        "The door opens with a muffled crash!");
                }
                else
                {
                    /* Message */
                    message(MSG_BASHDOOR, 0, "The door crashes open!");
                }
            }

            /* Break down the door */
            if (one_in_(2))
            {
                cave_set_feat(y, x, FEAT_BROKEN);
            }

            /* Open the door */
            else
            {
                cave_set_feat(y, x, FEAT_OPEN);
            }

            // Move the player onto the door square
            monster_swap(p_ptr->py, p_ptr->px, y, x);

            /* Make a lot of noise */
            monster_perception(true, false, -10);

            /* Update the visuals */
            p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
        }
    }

    if (!success)
    {
        /*
         * Match other retryable interaction failures: discard input queued
         * during the roll animation so it cannot consume the next bash
         * confirmation before the player sees it.
         */
        flush();

        if (cave_known_closed_door_bold(y, x))
        {
            /* Message */
            message(MSG_BASHDOOR_FAIL, 0, "The door holds firm.");
        }

        /* Stuns */
        if (allow_player_stun(NULL))
        {
            (void)set_stun(p_ptr->stun + 10);
        }
        else
        {
            /* Allow repeated bashing */
            more = true;
        }

        /* Make some noise */
        monster_perception(true, false, -5);
    }

    /* Result */
    return (more);
}

/*
 * Bash open a door, success based on character strength
 *
 * For a closed door, pval is positive if locked; negative if stuck.
 *
 * For an open door, pval is positive for a broken door.
 *
 * A closed door can be opened - harder if locked. Any door might be
 * bashed open (and thereby broken). Bashing a door is (potentially)
 * faster! You move into the door way. To open a stuck door, it must
 * be bashed.
 *
 * Creatures can also open or bash doors, see elsewhere.
 */
void do_cmd_bash(void)
{
    int y, x, dir;

    /* No closed door adjacent */
    if (count_feats(&y, &x, is_closed, false) == 0)
    {
        msg_print("There is no adjacent door to bash.");
        return;
    }

    /* Honour a pre-supplied direction, else pick a door interactively */
    if (p_ptr->command_dir)
        dir = p_ptr->command_dir;
    else if (!get_door_dir("Bash which door?", grid_is_known_closed_door, &dir))
        return;

    p_ptr->command_dir = dir;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Verify legality */
    if (!do_cmd_bash_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_BASH;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    /* Monster */
    if (cave_m_idx[y][x] > 0)
    {
        /* Message */
        msg_print("There is a monster in the way!");

        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* Door */
    else
    {
        /* Bash the door */
        if (!do_cmd_bash_aux(y, x, NULL, NULL))
        {
            /* Cancel repeat */
            disturb(0, 0);
        }
    }
}

/*
 * Manipulate an adjacent grid in some way
 *
 * Attack monsters, tunnel through walls, disarm traps, open doors.
 *
 * This command must always take energy, to prevent free detection
 * of invisible monsters.
 *
 * The "semantics" of this command must be chosen before the player
 * is confused, and it must be verified against the new grid.
 */
void do_cmd_alter(void)
{
    int y, x, dir;
    s16b chest_o_idx = 0;

    int feat;
    bool chest_trap = false;
    bool chest_present = false;
    bool skeleton_present = false;

    bool more = false;

    /* Get a direction */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Original feature */
    feat = cave_feat[y][x];

    /* Must have knowledge to know feature XXX XXX */
    if (!(cave_info[y][x] & (CAVE_MARK)))
        feat = FEAT_NONE;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    /* Allow repeated command */
    if (p_ptr->command_arg)
    {
        /* Set repeat count */
        p_ptr->command_rep = p_ptr->command_arg - 1;

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Cancel the arg */
        p_ptr->command_arg = 0;
    }

    // check for chests and chest traps
    if (chest_trap_minigame)
        chest_o_idx = chest_check(y, x);
    else if (cave_o_idx[y][x]
        && o_list[cave_o_idx[y][x]].tval == TV_CHEST)
        chest_o_idx = cave_o_idx[y][x];
    if (chest_o_idx)
    {
        object_type* o_ptr = &o_list[chest_o_idx];

        chest_present = true;

        if ((o_ptr->pval > 0) && object_chest_trap_flags(o_ptr)
            && (chest_trap_minigame ? chest_trap_presence_known(o_ptr)
                                    : object_known_p(o_ptr)))
            chest_trap = true;
    }
    else if (cave_o_idx[y][x])
    {
        object_type* o_ptr = &o_list[cave_o_idx[y][x]];

        if ((o_ptr->tval == TV_SKELETON)
            && !object_is_searched_skeleton(o_ptr))
        {
            skeleton_present = true;
        }
    }

    bool is_marked = (cave_info[y][x] & CAVE_MARK) > 0;
    bool is_visible = (cave_info[y][x] & CAVE_SEEN) > 0;

    /*Is there a monster on the space?*/
    if (cave_m_idx[y][x] > 0)
    {
        py_attack(y, x, ATT_MAIN);
    }
    // deal with players who can't see the square
    else if ((dir != 5) && !(is_marked || is_visible))
    {
        if (cave_floor_bold(y, x))
        {
            /* Oops */
            msg_print("You strike, but there is nothing there.");
        }
        else
        {
            msg_print("You hit something hard.");
            cave_info[y][x] |= (CAVE_MARK);
            lite_spot(y, x);
        }
    }

    /* Tunnel through walls */
    else if (cave_wall_bold(y, x))
    {
        /* Tunnel */
        do_cmd_tunnel_aux(y, x);
    }

    /* Bash doors */
    else if (cave_known_closed_door_bold(y, x))
    {
        /* Bash */
        do_cmd_bash_aux(y, x, NULL, NULL);
    }

    /* Disarm known dungeon traps */
    else if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
    {
        /* Disarm */
        more = do_cmd_disarm_aux(y, x);
    }

    /* Disarm known chest traps */
    else if (chest_trap)
    {
        /* Disarm */
        more = do_cmd_disarm_chest(y, x, chest_o_idx);
    }

    /* Open chest with no known traps */
    else if (chest_present)
    {
        /* Disarm */
        more = do_cmd_open_chest(y, x, chest_o_idx);
    }

    /* Search a skeleton */
    else if (skeleton_present)
    {
        /* Disarm */
        do_cmd_search_skeleton(y, x, cave_o_idx[y][x]);
    }

    /* Close open doors */
    else if (feat == FEAT_OPEN)
    {
        if (dir == 5)
        {
            msg_print("To close the door you would need to move out from the "
                      "doorway.");
        }
        else
        {
            /* Close */
            do_cmd_close_aux(y, x);
        }
    }

    /* Ascend upwards stairs */
    else if ((dir == 5) && ((feat == FEAT_LESS) || (feat == FEAT_LESS_SHAFT)))
    {
        /* Ascend */
        if (get_check_near(y, x, "Are you sure you wish to ascend? "))
            do_cmd_go_up();
    }

    /* Descend downwards stairs */
    else if ((dir == 5) && ((feat == FEAT_MORE) || (feat == FEAT_MORE_SHAFT)))
    {
        /* Descend */
        if (get_check_near(y, x, "Are you sure you wish to descend? "))
            do_cmd_go_down();
    }

    /* Use forges */
    else if ((dir == 5) && cave_forge_bold(y, x))
    {
        /* Use forge */
        do_cmd_smithing_screen();
        more = true;

        // don't take a turn...
        p_ptr->energy_use = 0;
    }

    /* Pick up items */
    else if ((dir == 5) && (cave_o_idx[y][x]))
    {
        /* Get item */
        do_cmd_pickup();
    }

    /* Oops */
    else if (dir == 5)
    {
        /* Oops */
        msg_print("There is nothing here to use.");

        // don't take a turn...
        p_ptr->energy_use = 0;
    }

    /* Oops */
    else
    {
        /* Oops */
        msg_print("You strike, but there is nothing there.");
    }

    /* Cancel repetition unless we can continue */
    if (!more)
        disturb(0, 0);
}
