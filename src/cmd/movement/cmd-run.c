#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "ui/question.h"
#include <math.h>

/*
 * One-shot allowance to step onto a known trap without the usual
 * confirmation: granted when the player already chose "step onto it" in
 * the trap popup (so we never ask the same question twice in a row).
 */
static int allow_trap_step_y = -1;
static int allow_trap_step_x = -1;

void player_allow_trap_step(int y, int x)
{
    allow_trap_step_y = y;
    allow_trap_step_x = x;
}

static bool player_take_trap_step_allowance(int y, int x)
{
    bool allowed = (allow_trap_step_y == y) && (allow_trap_step_x == x);

    allow_trap_step_y = -1;
    allow_trap_step_x = -1;
    return allowed;
}

static bool move_target_exits_gates(int y, int x)
{
    if (!p_ptr || (p_ptr->depth != 0))
        return false;

    if (!in_bounds(y, x))
        return true;

    return (y == 0) || (x == 0) || (y == p_ptr->cur_map_hgt - 1)
        || (x == p_ptr->cur_map_wid - 1);
}

void move_player(int dir)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    /* Find the result of moving */
    player_set_visual_facing_dir(dir);
    y = py + ddy[dir];
    x = px + ddx[dir];

    if (move_target_exits_gates(y, x))
    {
        do_cmd_escape(silmarils_possessed());
        return;
    }

    /* deal with leaving the map */
    if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt)
        || (x >= p_ptr->cur_map_wid))
    {
        do_cmd_escape(silmarils_possessed());
        return;
    }

    /* Hack -- attack visible monsters */
    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
    {
        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* open known doors on movement */
    else if ((cave_info[y][x] & (CAVE_MARK))
        && cave_known_closed_door_bold(y, x))
    {
        /* Open */
        do_cmd_open_aux(y, x);
    }

    /* Player can not walk through "walls", but can go through traps */
    else if (!cave_floor_bold(y, x))
    {
        log_debug("cmd_walk: Hit wall at (%d, %d)", y, x);
        /* Disturb the player */
        disturb(0, 0);

        /* Notice unknown obstacles */
        if (!(cave_info[y][x] & (CAVE_MARK)))
        {
            /* Rubble */
            if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                message(MSG_HITWALL, 0,
                    "You feel a pile of rubble blocking your way.");
                cave_info[y][x] |= (CAVE_MARK);
                lite_spot(y, x);
            }

            /* Closed door */
            else if (cave_known_closed_door_bold(y, x))
            {
                message(MSG_HITWALL, 0, "You feel a door blocking your way.");
                cave_info[y][x] |= (CAVE_MARK);
                lite_spot(y, x);
            }

            /* Wall (or secret door) */
            else
            {
                message(MSG_HITWALL, 0, "You feel a wall blocking your way.");
                cave_info[y][x] |= (CAVE_MARK);
                lite_spot(y, x);
            }
        }

        /* Mention known obstacles */
        else
        {
            /* Rubble */
            if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                message(MSG_HITWALL, 0,
                    "There is a pile of rubble blocking your way.");
            }

            /* Closed door */
            else if (cave_known_closed_door_bold(y, x))
            {
                message(MSG_HITWALL, 0, "There is a door blocking your way.");
            }

            /* Wall (or secret door) */
            else
            {
                message(MSG_HITWALL, 0, "There is a wall blocking your way.");
            }
        }

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;
    }

    /* Normal movement */
    else
    {
        // deal with overburdened characters
        if (p_ptr->total_weight > weight_limit() * 3 / 2)
        {
            /* Abort */
            msg_print("You are too burdened to move.");

            /* Disturb the player */
            disturb(0, 0);

            // don't take a turn...
            p_ptr->energy_use = 0;

            return;
        }

        /* Check before walking on known traps/chasms on movement */
        if ((!p_ptr->confused) && (cave_info[y][x] & (CAVE_MARK)))
        {
            // leapable things: chasms, traps (except roosts and webs).
            // A trap the player has rewired is safe for them -- no leap prompt.
            if ((cave_feat[y][x] == FEAT_CHASM)
                || (((cave_trap_bold(y, x)) && !cave_floorlike_bold(y, x))
                    && !cave_rewired[y][x]
                    && !(cave_feat[y][x] == FEAT_TRAP_ROOST
                        || cave_feat[y][x] == FEAT_TRAP_WEB)))
            {
                char prompt[80];
                int i;
                int d;
                bool run_up = false;
                bool confirm = true;

                // test all three directions roughly towards the chasm/pit
                for (i = -1; i <= 1; i++)
                {
                    d = cycle[chome[dir_from_delta(
                                  y - p_ptr->py, x - p_ptr->px)]
                        + i];

                    // if the last action was a move in this direction, we have
                    // a valid run_up
                    if (p_ptr->previous_action[1] == d)
                        run_up = true;
                }

                if (p_ptr->active_ability[S_EVN][EVN_LEAPING])
                {
                    int y_mid, x_mid; // the midpoint of the leap
                    int y_end, x_end; // the endpoint of the leap

                    /* Get location */
                    y_mid = p_ptr->py + ddy[dir];
                    x_mid = p_ptr->px + ddx[dir];
                    y_end = y_mid + ddy[dir];
                    x_end = x_mid + ddx[dir];

                    /* Disturb the player */
                    disturb(0, 0);

                    /* Flush input */
                    flush();

                    // Can't jump from within pits
                    if (cave_pit_bold(p_ptr->py, p_ptr->px))
                    {
                        msg_print("You cannot leap from within a pit.");
                    }

                    // Can't jump from within webs
                    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
                    {
                        msg_print("You cannot leap from within a web.");
                    }

                    // Can't jump without a run up
                    else if (!run_up)
                    {
                        msg_print("You cannot leap without a run up.");
                    }

                    // need room to land
                    else if ((cave_info[y_end][x_end] & (CAVE_MARK))
                        && (cave_wall_bold(y_end, x_end)
                            || cave_any_closed_door_bold(y_end, x_end)))
                    {
                        msg_print("You cannot leap over as there is no room to "
                                  "land.");
                    }

                    else
                    {
                        // confirm if the destination is unknown
                        if (!(cave_info[y_end][x_end] & (CAVE_SEEN))
                            && !(cave_info[y_end][x_end] & (CAVE_MARK)))
                        {
                            strnfmt(prompt, sizeof(prompt),
                                "Are you sure you wish to leap into the "
                                "unknown? ");
                        }

                        // confirm if the destination is in the chasm
                        else if (cave_feat[y_end][x_end] == FEAT_CHASM)
                        {
                            if (p_ptr->depth >= MORGOTH_DEPTH)
                            {
                                strnfmt(prompt, sizeof(prompt),
                                    "Are you sure you wish to leap into the "
                                    "abyss? You will surely die. ");
                            }
                            else
                            {
                                strnfmt(prompt, sizeof(prompt),
                                    "Are you sure you wish to leap into the "
                                    "abyss? ");
                            }
                        }

                        // confirm if the destination has a visible monster
                        else if ((cave_m_idx[y_end][x_end] > 0)
                            && (&mon_list[cave_m_idx[y_end][x_end]])->ml)
                        {
                            monster_type* m_ptr
                                = &mon_list[cave_m_idx[y_end][x_end]];
                            char m_name[80];

                            /* Get the monster name */
                            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                            strnfmt(prompt, sizeof(prompt),
                                "Are you sure you wish to leap into %s? ",
                                m_name);
                        }

                        // default confirmation
                        else
                        {
                            confirm = false;
                            // strnfmt(prompt, sizeof(prompt), "Leap over the
                            // %s? ", f_name +
                            // f_info[cave_feat[y_mid][x_mid]].name);
                        }

                        // if you say 'yes' to the prompt, then try to leap
                        if (!confirm || get_check_near(y_end, x_end, prompt))
                        {
                            // at this point attack any invisible monster that
                            // may be there
                            if (cave_m_idx[y][x] > 0)
                            {
                                msg_print("An unseen foe blocks your way.");

                                /* Attack */
                                py_attack(y, x, ATT_MAIN);

                                return;
                            }

                            // otherwise do the leap!
                            else
                            {
                                // we generously give you your free flanking
                                // attack...
                                flanking_or_retreat(y_mid, x_mid);

                                /* Take a turn */
                                p_ptr->energy_use = 100;

                                // store the action type
                                p_ptr->previous_action[0] = dir;

                                // move player to the new position
                                monster_swap(
                                    p_ptr->py, p_ptr->px, y_mid, x_mid);

                                // remember that the player is in the air now
                                p_ptr->leaping = true;

                                return;
                            }
                        }
                    }
                }

                // if the player hasn't already leapt
                if (!p_ptr->leaping && (cave_feat[y][x] == FEAT_CHASM))
                {
                    /* Disturb the player */
                    disturb(0, 0);

                    /* Flush input */
                    flush();

                    cptr prompt = "Step into the chasm? ";
                    if (p_ptr->depth >= MORGOTH_DEPTH)
                        prompt = "Step into the chasm? You will surely die. ";

                    if (!get_check_near(y, x, prompt))
                    {
                        // don't take a turn...
                        p_ptr->energy_use = 0;

                        return;
                    }
                }
            }

            // traps (a trap the player has rewired is safe for them -- no prompt)
            if ((cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
                && !cave_rewired[y][x]
                && !player_take_trap_step_allowance(y, x))
            {
                ui_question_option options[2];
                char title[80];
                int choice;
                int disarm_choice = -1;
                int step_choice = 0;
                int count = 0;
                cptr name = (f_name + f_info[cave_feat[y][x]].name);

                /* Disturb the player */
                disturb(0, 0);

                /* Flush input */
                flush();

                strnfmt(title, sizeof(title), "%^s in the way", name);

                /* Offer disarming when the trap allows it -- or rewiring, if the
                 * player has the ability and the trap can be re-keyed (this
                 * block only runs for not-yet-rewired traps). */
                if (trap_disarm_power(cave_feat[y][x], NULL))
                {
                    bool rewiring
                        = p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS]
                        && trap_is_rewireable(cave_feat[y][x]);
                    options[count].key = 'd';
                    options[count].label = rewiring ? "Rewire it" : "Disarm it";
                    options[count].attr = TERM_L_WHITE;
                    disarm_choice = count;
                    count++;
                }

                options[count].key = 'w';
                options[count].label = "Step onto it";
                options[count].attr = TERM_L_WHITE;
                step_choice = count;
                count++;

                choice = ui_question_ask(title, NULL, options, count, y, x,
                    step_choice);

                if (choice < 0)
                {
                    // don't take a turn...
                    p_ptr->energy_use = 0;

                    return;
                }

                if ((disarm_choice >= 0) && (choice == disarm_choice))
                {
                    /* Disarm instead of moving; this takes the turn */
                    p_ptr->previous_action[0] = ACTION_MISC;
                    if (!do_cmd_disarm_aux(y, x))
                        disturb(0, 0);

                    return;
                }
            }
        }

        // if there is an invisible monster present and you haven't yet
        // attacked, do so now
        if (cave_m_idx[y][x] > 0)
        {
            msg_print("An unseen foe blocks your way.");

            /* Attack */
            py_attack(y, x, ATT_MAIN);

            return;
        }

        // It is hard to get out of a pit
        if (cave_pit_bold(py, px))
        {
            int difficulty;

            if (cave_feat[py][px] == FEAT_TRAP_PIT)
                difficulty = 10;
            else
                difficulty = 15;

            /* Disturb the player */
            disturb(0, 0);

            if (check_hit(difficulty, false))
            {
                msg_print("You try to climb out of the pit, but fail.");

                /* Take a turn */
                p_ptr->energy_use = 100;

                // store the action type
                p_ptr->previous_action[0] = ACTION_MISC;

                return;
            }
            else
            {
                msg_print("You climb out of the pit.");
            }
        }

        // It is hard to get out of a web
        if (cave_feat[py][px] == FEAT_TRAP_WEB)
        {
            if (!break_free_of_web())
                return;
        }

        if ((p_ptr->depth == MORGOTH_DEPTH) && p_ptr->morgoth_hall_entered
            && (silmarils_possessed() == 0)
            && (cave_info[py][px] & CAVE_G_VAULT)
            && !(cave_info[y][x] & CAVE_G_VAULT))
        {
            msg_print("The Shadow bars your way: you cannot flee without a Silmaril.");
            disturb(0, 0);
            p_ptr->previous_action[0] = ACTION_MISC;
            return;
        }

        if ((p_ptr->depth == MORGOTH_DEPTH) && !p_ptr->morgoth_hall_entered
            && (cave_info[y][x] & CAVE_G_VAULT))
        {
            if (!preconfirm_enter_morgoth_hall())
            {
                disturb(0, 0);
                p_ptr->energy_use = 0;
                return;
            }
        }

        /* Sound */
        sound(MSG_WALK);

        // do flanking or controlled retreat attack if any
        flanking_or_retreat(y, x);

        /* Move player */
        monster_swap(py, px, y, x);

        /* Check for Mandos quest interaction after movement */
        check_mandos_quest_interaction();
        
        /* Check for Nienna quest completion after movement */
        check_niena_quest_completion();

        if (cave_feat[y][x] == FEAT_SUNLIGHT
            && cave_feat[py][px] != FEAT_SUNLIGHT)
        {
            msg_print("You step into a patch of sunlight.");
        }
        else if (cave_feat[y][x] != FEAT_SUNLIGHT
            && cave_feat[py][px] == FEAT_SUNLIGHT)
        {
            msg_print("You step out of the sunlight.");
        }

        /* New location */
        y = py = p_ptr->py;
        x = px = p_ptr->px;

        /* Chasm sanctum EVIL drops trigger their ambush on entry. */
        trigger_chasm_sanctum_ambush_if_needed(y, x);

        /* Spontaneous Searching */
        perceive();

        // remember this direction of movement
        p_ptr->previous_action[0] = dir;

        /* Discover stairs if blind */
        if (cave_stair_bold(y, x))
        {
            cave_info[y][x] |= (CAVE_MARK);
            lite_spot(y, x);
        }

        /* Remark on Forge and discover it if blind */
        if (cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            if ((cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_UNIQUE_HEAD)
                && !p_ptr->unique_forge_seen)
            {
                msg_print("You enter the forge 'Orodruth' - the Mountain's "
                          "Anger - where "
                          "Grond was made in days of old.");
                msg_print("The fires burn still.");
                p_ptr->unique_forge_seen = true;
                do_cmd_note("Entered the forge 'Orodruth'", p_ptr->depth);
            }

            else
            {
                char* article;

                if (cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_UNIQUE_HEAD)
                    article = "the";
                else if (cave_feat[p_ptr->py][p_ptr->px]
                    >= FEAT_FORGE_GOOD_HEAD)
                    article = "an";
                else
                    article = "a";

                msg_format("You enter %s %s.", article,
                    f_name + f_info[cave_feat[p_ptr->py][p_ptr->px]].name);
            }

            cave_info[y][x] |= (CAVE_MARK);
            lite_spot(y, x);
        }

        /* Set off traps */
        if (cave_trap_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
        {
            // If it is hidden
            if (cave_info[y][x] & (CAVE_HIDDEN))
            {
                /* Reveal the trap */
                reveal_trap(y, x);
            }

            /* Hit the trap */
            hit_trap(y, x);
        }

        // read any notes the player stumbles upon
        if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
        {
            object_type* o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            if (o_ptr->tval == TV_NOTE)
            {
                note_info_screen(o_ptr);
            }
        }
    }
}

/*
 * Hack -- Check for a "known wall" (see below)
 */
static int see_wall(int dir, int y, int x)
{
    /* Get the new location */
    y += ddy[dir];
    x += ddx[dir];

    /* Illegal grids are not known walls XXX XXX XXX */
    if (!in_bounds(y, x))
        return (false);

    /* Non-wall grids are not known walls */
    if (!cave_wall_bold(y, x))
        return (false);

    /* Unknown walls are not known walls */
    if (!(cave_info[y][x] & (CAVE_MARK)))
        return (false);

    /* Default */
    return (true);
}

/*
 * Hack -- Check for an "unknown corner" (see below)
 */
// static int see_nothing(int dir, int y, int x)
//{
//	/* Get the new location */
//	y += ddy[dir];
//	x += ddx[dir];

//	/* Illegal grids are unknown XXX XXX XXX */
//	if (!in_bounds(y, x)) return (true);

//	/* Memorized grids are always known */
//	if (cave_info[y][x] & (CAVE_MARK)) return (false);

//	/* Default */
//	return (true);
//}

/*
 * The running algorithm  -CJS-
 *
 * Basically, once you start running, you keep moving until something
 * interesting happens.  In an enclosed space, you run straight, but
 * you follow corners as needed (i.e. hallways).  In an open space,
 * you run straight, but you stop before entering an enclosed space
 * (i.e. a room with a doorway).  In a semi-open space (with walls on
 * one side only), you run straight, but you stop before entering an
 * enclosed space or an open space (i.e. running along side a wall).
 *
 * All discussions below refer to what the player can see, that is,
 * an unknown wall is just like a normal floor.  This means that we
 * must be careful when dealing with "illegal" grids.
 *
 * No assumptions are made about the layout of the dungeon, so this
 * algorithm works in hallways, rooms, destroyed areas, etc.
 *
 * In the diagrams below, the player has just arrived in the grid
 * marked as '@', and he has just come from a grid marked as 'o',
 * and he is about to enter the grid marked as 'x'.
 *
 * Running while confused is not allowed, and so running into a wall
 * is only possible when the wall is not seen by the player.  This
 * will take a turn and stop the running.
 *
 * Several conditions are tracked by the running variables.
 *
 *   p_ptr->run_open_area (in the open on at least one side)
 *   p_ptr->run_break_left (wall on the left, stop if it opens)
 *   p_ptr->run_break_right (wall on the right, stop if it opens)
 *
 * When running begins, these conditions are initialized by examining
 * the grids adjacent to the requested destination grid (marked 'x'),
 * two on each side (marked 'L' and 'R').  If either one of the two
 * grids on a given side is a wall, then that side is considered to
 * be "closed".  Both sides enclosed yields a hallway.
 *
 *    LL                     @L
 *    @x      (normal)       RxL   (diagonal)
 *    RR      (east)          R    (south-east)
 *
 * In the diagram below, in which the player is running east along a
 * hallway, he will stop as indicated before attempting to enter the
 * intersection (marked 'x').  Starting a new run in any direction
 * will begin a new hallway run.
 *
 *  #.#
 * ##.##
 * o@x..
 * ##.##
 *  #.#
 *
 * Note that a minor hack is inserted to make the angled corridor
 * entry (with one side blocked near and the other side blocked
 * further away from the runner) work correctly. The runner moves
 * diagonally, but then saves the previous direction as being
 * straight into the gap. Otherwise, the tail end of the other
 * entry would be perceived as an alternative on the next move.
 *
 * In the diagram below, the player is running east down a hallway,
 * and will stop in the grid (marked '1') before the intersection.
 * Continuing the run to the south-east would result in a long run
 * stopping at the end of the hallway (marked '2').
 *
 * ##################
 * o@x       1
 * ########### ######
 * #2          #
 * #############
 *
 * After each step, the surroundings are examined to determine if
 * the running should stop, and to determine if the running should
 * change direction.  We examine the new current player location
 * (at which the runner has just arrived) and the direction from
 * which the runner is considered to have come.
 *
 * Moving one grid in some direction places you adjacent to three
 * or five new grids (for straight and diagonal moves respectively)
 * to which you were not previously adjacent (marked as '!').
 *
 *   ...!              ...
 *   .o@!  (normal)    .o.!  (diagonal)
 *   ...!  (east)      ..@!  (south east)
 *                      !!!
 *
 * If any of the newly adjacent grids are "interesting" (monsters,
 * objects, some terrain features) then running stops.
 *
 * If any of the newly adjacent grids seem to be open, and you are
 * looking for a break on that side, then running stops.
 *
 * If any of the newly adjacent grids do not seem to be open, and
 * you are in an open area, and the non-open side was previously
 * entirely open, then running stops.
 *
 * If you are in a hallway, then the algorithm must determine if
 * the running should continue, turn, or stop.  If only one of the
 * newly adjacent grids appears to be open, then running continues
 * in that direction, turning if necessary.  If there are more than
 * two possible choices, then running stops.  If there are exactly
 * two possible choices, separated by a grid which does not seem
 * to be open, then running stops.  Otherwise, as shown below, the
 * player has probably reached a "corner".
 *
 *    ###             o##
 *    o@x  (normal)   #@!   (diagonal)
 *    ##!  (east)     ##x   (south east)
 *
 * In this situation, there will be two newly adjacent open grids,
 * one touching the player on a diagonal, and one directly adjacent.
 * We must consider the two "option" grids further out (marked '?').
 * We assign "option" to the straight-on grid, and "option2" to the
 * diagonal grid.
 *
 *    ###s
 *    o@x?   (may be incorrect diagram!)
 *    ##!?
 *
 * If both "option" grids are closed, then there is no reason to enter
 * the corner, and so we can cut the corner, by moving into the other
 * grid (diagonally).  If we choose not to cut the corner, then we may
 * go straight, but we pretend that we got there by moving diagonally.
 * Below, we avoid the obvious grid (marked 'x') and cut the corner
 * instead (marked 'n').
 *
 *    ###:               o##
 *    o@x#   (normal)    #@n    (maybe?)
 *    ##n#   (east)      ##x#
 *                       ####
 *
 * If one of the "option" grids is open, then we may have a choice, so
 * we check to see whether it is a potential corner or an intersection
 * (or room entrance).  If the grid two spaces straight ahead, and the
 * space marked with 's' are both open, then it is a potential corner
 * and we enter it if requested.  Otherwise, we stop, because it is
 * not a corner, and is instead an intersection or a room entrance.
 *
 *    ###
 *    o@x
 *    ##!#
 *
 * (This documentation may no longer be correct)
 */

/*
 * Hack -- allow quick "cycling" through the legal directions
 */
const byte cycle[] = { 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3,
    6, 9, 8, 7, 4 };

/*
 * Hack -- map each direction into the "middle" of the "cycle[]" array
 */
const byte chome[] = { 0, 8, 9, 10, 15, 0, 11, 14, 13, 12 };

/*
 * Initialize the running algorithm for a new direction.
 *
 * Diagonal Corridor -- allow diaginal entry into corridors.
 *
 * Blunt Corridor -- If there is a wall two spaces ahead and
 * we seem to be in a corridor, then force a turn into the side
 * corridor, must be moving straight into a corridor here. (?)
 *
 * Diagonal Corridor    Blunt Corridor (?)
 *       # #                  #
 *       #x#                 @x#
 *       @p.                  p
 */
static void run_init(int dir)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, row, col;

    bool deepleft, deepright;
    bool shortleft, shortright;

    /* Save the direction */
    p_ptr->run_cur_dir = dir;

    /* Assume running straight */
    p_ptr->run_old_dir = dir;

    /* Assume looking for open area */
    p_ptr->run_open_area = true;

    /* Assume not looking for breaks */
    p_ptr->run_break_right = false;
    p_ptr->run_break_left = false;

    /* Assume no nearby walls */
    deepleft = deepright = false;
    shortright = shortleft = false;

    /* Find the destination grid */
    row = py + ddy[dir];
    col = px + ddx[dir];

    /* Extract cycle index */
    i = chome[dir];

    /* Check for nearby wall */
    if (see_wall(cycle[i + 1], py, px))
    {
        p_ptr->run_break_left = true;
        shortleft = true;
    }

    /* Check for distant wall */
    else if (see_wall(cycle[i + 1], row, col))
    {
        p_ptr->run_break_left = true;
        deepleft = true;
    }

    /* Check for nearby wall */
    if (see_wall(cycle[i - 1], py, px))
    {
        p_ptr->run_break_right = true;
        shortright = true;
    }

    /* Check for distant wall */
    else if (see_wall(cycle[i - 1], row, col))
    {
        p_ptr->run_break_right = true;
        deepright = true;
    }

    /* Looking for a break */
    if (p_ptr->run_break_left && p_ptr->run_break_right)
    {
        /* Not looking for open area */
        p_ptr->run_open_area = false;

        /* Hack -- allow angled corridor entry */
        if (dir & 0x01)
        {
            if (deepleft && !deepright)
            {
                p_ptr->run_old_dir = cycle[i - 1];
            }
            else if (deepright && !deepleft)
            {
                p_ptr->run_old_dir = cycle[i + 1];
            }
        }

        /* Hack -- allow blunt corridor entry */
        else if (see_wall(cycle[i], row, col))
        {
            if (shortleft && !shortright)
            {
                p_ptr->run_old_dir = cycle[i - 2];
            }
            else if (shortright && !shortleft)
            {
                p_ptr->run_old_dir = cycle[i + 2];
            }
        }
    }
}

/*
 * Update the current "run" path
 *
 * Return true if the running should be stopped
 */
static bool run_test(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int prev_dir;
    int new_dir;

    int row, col;
    int i, max, inv;
    int option, option2;

    /* No options yet */
    option = 0;
    option2 = 0;

    /* Where we came from */
    prev_dir = p_ptr->run_old_dir;

    /* Range of newly adjacent grids */
    max = (prev_dir & 0x01) + 1;

    /* Look at every newly adjacent square. */
    for (i = -max; i <= max; i++)
    {
        object_type* o_ptr;

        /* New direction */
        new_dir = cycle[chome[prev_dir] + i];

        /* New location */
        row = py + ddy[new_dir];
        col = px + ddx[new_dir];

        /* Visible monsters abort running */
        if (cave_m_idx[row][col] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[row][col]];

            /* Visible monster */
            if (m_ptr->ml)
                return (true);
        }

        /* Visible objects abort running */
        for (o_ptr = get_first_object(row, col); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Visible object */
            if (o_ptr->marked && !object_is_searched_skeleton(o_ptr))
                return (true);
        }

        /* Assume unknown */
        inv = true;

        /* Check memorized grids */
        if (cave_info[row][col] & (CAVE_MARK))
        {
            bool notice = true;

            /* Examine the terrain */
            switch (cave_feat[row][col])
            {
            /* Floors */
            case FEAT_FLOOR:

            /* Secret doors */
            case FEAT_SECRET:

            /* Walls */
            case FEAT_QUARTZ:
            case FEAT_WALL_EXTRA:
            case FEAT_WALL_INNER:
            case FEAT_WALL_OUTER:
            case FEAT_WALL_SOLID:
            case FEAT_WALL_PERM:
            {
                /* Ignore */
                notice = false;

                /* Done */
                break;
            }

            /* Open doors */
            case FEAT_OPEN:
            case FEAT_BROKEN:
            {
                /* ignore */
                notice = false;

                /* Done */
                break;
            }

            /* Stairs */
            case FEAT_LESS:
            case FEAT_MORE:
            case FEAT_LESS_SHAFT:
            case FEAT_MORE_SHAFT:
            {
                /* Done */
                break;
            }

            /* Deal with traps */
            default:
            {
                // ignore hidden traps
                if (cave_floorlike_bold(row, col))
                {
                    /* ignore */
                    notice = false;

                    /* Done */
                    break;
                }
            }
            }

            /* Interesting feature */
            if (notice)
                return (true);

            /* The grid is "visible" */
            inv = false;
        }

        /* Analyze unknown grids and floors */
        if (inv || cave_floor_bold(row, col))
        {
            /* Looking for open area */
            if (p_ptr->run_open_area)
            {
                /* Nothing */
            }

            /* The first new direction. */
            else if (!option)
            {
                option = new_dir;
            }

            /* Three new directions. Stop running. */
            else if (option2)
            {
                return (true);
            }

            /* Two non-adjacent new directions.  Stop running. */
            else if (option != cycle[chome[prev_dir] + i - 1])
            {
                return (true);
            }

            /* Two new (adjacent) directions (case 1) */
            else if (new_dir & 0x01)
            {
                option2 = new_dir;
            }

            /* Two new (adjacent) directions (case 2) */
            else
            {
                option2 = option;
                option = new_dir;
            }
        }

        /* Obstacle, while looking for open area */
        else
        {
            if (p_ptr->run_open_area)
            {
                if (i < 0)
                {
                    /* Break to the right */
                    p_ptr->run_break_right = true;
                }

                else if (i > 0)
                {
                    /* Break to the left */
                    p_ptr->run_break_left = true;
                }
            }
        }
    }

    // Now check to see if running another step would bring us next to an
    // immobile monster (such as a mold).
    /* Look at every soon to be newly adjacent square. */
    for (i = -max; i <= max; i++)
    {
        /* New direction */
        new_dir = cycle[chome[prev_dir] + i];

        /* New location */
        row = py + ddy[prev_dir] + ddy[new_dir];
        col = px + ddx[prev_dir] + ddx[new_dir];

        /* Visible immovable monsters abort running */
        if (cave_m_idx[row][col] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[row][col]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Visible monster */
            if (m_ptr->ml && (r_ptr->flags1 & (RF1_NEVER_MOVE)))
                return (true);
        }
    }

    /* Looking for open area */
    if (p_ptr->run_open_area)
    {
        /* Hack -- look again */
        for (i = -max; i < 0; i++)
        {
            new_dir = cycle[chome[prev_dir] + i];

            row = py + ddy[new_dir];
            col = px + ddx[new_dir];

            /* Unknown grid or non-wall */
            /* Was: cave_floor_bold(row, col) */
            if (!(cave_info[row][col] & (CAVE_MARK))
                || (!cave_wall_bold(row, col)))
            {
                /* Looking to break right */
                if (p_ptr->run_break_right)
                {
                    return (true);
                }
            }

            /* Obstacle */
            else
            {
                /* Looking to break left */
                if (p_ptr->run_break_left)
                {
                    return (true);
                }
            }
        }

        /* Hack -- look again */
        for (i = max; i > 0; i--)
        {
            new_dir = cycle[chome[prev_dir] + i];

            row = py + ddy[new_dir];
            col = px + ddx[new_dir];

            /* Unknown grid or non-wall */
            /* Was: cave_floor_bold(row, col) */
            if (!(cave_info[row][col] & (CAVE_MARK))
                || (!cave_wall_bold(row, col)))
            {
                /* Looking to break left */
                if (p_ptr->run_break_left)
                {
                    return (true);
                }
            }

            /* Obstacle */
            else
            {
                /* Looking to break right */
                if (p_ptr->run_break_right)
                {
                    return (true);
                }
            }
        }
    }

    /* Not looking for open area */
    else
    {
        /* No options */
        if (!option)
        {
            return (true);
        }

        /* One option */
        else if (!option2)
        {
            /* Primary option */
            p_ptr->run_cur_dir = option;

            /* No other options */
            p_ptr->run_old_dir = option;
        }

        /* Two options, examining corners */
        else
        {
            /* Primary option */
            p_ptr->run_cur_dir = option;

            /* Stop in the doorway of a room */
            row = py + 2 * ddy[option];
            col = px + 2 * ddx[option];
            if ((cave_info[row][col] & CAVE_MARK) && !cave_wall_bold(row, col))
            {
                return (true);
            }

            /* Hack -- allow curving */
            p_ptr->run_old_dir = option2;
        }
    }

    /* About to hit a known wall, stop */
    if (see_wall(p_ptr->run_cur_dir, py, px))
    {
        return (true);
    }

    /* Failure */
    return (false);
}

/*
 * Take one step along the current "run" path
 *
 * Called with a real direction to begin a new run, and with zero
 * to continue a run in progress.
 */
void run_step(int dir)
{
    /* Start run */
    if (dir)
    {
        /* Initialize */
        run_init(dir);

        /* Hack -- Set the run counter */
        p_ptr->running = (p_ptr->command_arg ? p_ptr->command_arg : 1000);
    }

    /* Continue run */
    else
    {
        /* Update run */
        if (run_test())
        {
            /* Disturb */
            disturb(0, 0);

            /* Done */
            return;
        }
    }

    /* Decrease counter */
    p_ptr->running--;

    /* Take time */
    p_ptr->energy_use = 100;

    /* Move the player */
    move_player(p_ptr->run_cur_dir);
}
