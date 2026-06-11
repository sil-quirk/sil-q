#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "cmd/world/cmd-interact-chest.h"

static bool is_open(int feat) { return (feat == FEAT_OPEN); }

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
        if (trapped
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
    int score, power, difficulty;

    bool more = false;

    /* Verify legality */
    if (!do_cmd_open_test(y, x))
        return (false);

    /* Jammed door */
    if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
    {
        /* Stuck */
        msg_print("The door appears to be stuck.");
    }

    /* Locked door */
    else if (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
    {
        /* Get the score in favour (=perception) */
        score = p_ptr->skill_use[S_PER];

        /* Determine door power based on the door power (1 to 7)*/
        power = cave_feat[y][x] - FEAT_DOOR_HEAD;

        // Base difficulty is the door power + 5
        difficulty = power + 5;

        /* Penalize some conditions */
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5;
        if (p_ptr->confused)
            difficulty += 5;

        /* Success */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
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

    s16b o_idx;

    bool more = false;

    int num_doors, num_chests;

    /* Count closed doors */
    num_doors = count_feats(&y, &x, is_closed, false);

    /* Count chests (locked) */
    num_chests = count_chests(&y, &x, false);

    /* See if only one target */
    if ((num_doors + num_chests) == 1)
    {
        p_ptr->command_dir = coords_to_dir(y, x);
    }

    else if ((num_doors + num_chests) == 0)
    {
        msg_print("There is nothing in your square (or adjacent) to open.");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Check for chests */
    o_idx = chest_check(y, x);

    /* Verify legality */
    if (!o_idx && !do_cmd_open_test(y, x))
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
 * Interactive "which adjacent door?" selection for the close/bash commands.
 *
 * Each candidate door is highlighted on the map and a small popup prompt is
 * shown (never the top message row). The player can click or tap a door, move
 * the highlight with the direction keys, or press the direction toward a door.
 * A single candidate resolves immediately; with none, false is returned.
 *
 * Returns true and stores the chosen direction in *dp; false on cancel.
 */
static bool get_door_dir(cptr prompt, bool (*test)(int feat), int* dp)
{
    int ys[8], xs[8], dirs[8];
    int count = 0;
    int sel = 0;
    bool done = false;
    bool chosen = false;
    char query;

#ifdef ALLOW_REPEAT
    /* Reuse the stored direction while a command repeats (e.g. bashing). */
    if (repeat_pull(dp))
        return true;
#endif

    /* Collect adjacent candidate doors (known grids only) */
    for (int d = 0; d < 8; d++)
    {
        int yy = p_ptr->py + ddy_ddd[d];
        int xx = p_ptr->px + ddx_ddd[d];

        if (!in_bounds_fully(yy, xx))
            continue;
        if (!(cave_info[yy][xx] & (CAVE_MARK)))
            continue;
        if (!test(cave_feat[yy][xx]))
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

    message_flush();

    sdl_pointer_aim_select_begin(1, false);
    sdl_pointer_aim_select_set_choices(ys, xs, count, prompt);
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

    /* request_command blanked the map's top row before this handler ran;
     * repaint the map so the highlights/popup sit over a clean view. */
    p_ptr->redraw |= (PR_MAP);

    while (!done)
    {
        sdl_pointer_aim_select_update(ys[sel], xs[sel]);
        handle_stuff();
        move_cursor_relative(ys[sel], xs[sel]);

        query = inkey();

        /* Pointer events from the map: hover moves the highlight, a click or
         * tap on a candidate confirms it. */
        if (query == UI_MENU_CLICK_WAKE_KEY)
        {
            int kind = 0, ey = 0, ex = 0;

            if (!sdl_pointer_aim_select_take_event(&kind, &ey, &ex))
                continue;

            for (int i = 0; i < count; i++)
            {
                if ((ys[i] == ey) && (xs[i] == ex))
                {
                    sel = i;
                    if ((kind == AIM_SELECT_EVENT_CLICK)
                        || (kind == AIM_SELECT_EVENT_TAP))
                    {
                        chosen = true;
                        done = true;
                    }
                    break;
                }
            }
            continue;
        }

        switch (query)
        {
        case ESCAPE:
        case 'q':
            done = true;
            break;

        case '*':
        case '+':
            sel = (sel + 1) % count;
            break;

        case '-':
            sel = (sel + count - 1) % count;
            break;

        case ' ':
        case '\r':
        case '\n':
        case 't':
        case '5':
        case INPUT_BIND_CONFIRM:
            chosen = true;
            done = true;
            break;

        default:
        {
            int d = target_dir(query);

            if (d)
            {
                bool matched = false;

                /* Press the direction toward a door to pick it. */
                for (int i = 0; i < count; i++)
                {
                    if (dirs[i] == d)
                    {
                        sel = i;
                        chosen = true;
                        done = true;
                        matched = true;
                        break;
                    }
                }

                if (!matched)
                    bell("No door that way.");
            }
            else
            {
                bell("Illegal door direction.");
            }
            break;
        }
        }
    }

    sdl_object_tooltip_clear();
    sdl_pointer_aim_select_end();
    ui_menu_click_clear();
    verify_panel();
    handle_stuff();

    if (!chosen)
        return false;

    *dp = dirs[sel];
#ifdef ALLOW_REPEAT
    repeat_push(*dp);
#endif
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
    else if (!get_door_dir("Close which door?", is_open, &dir))
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
            
            bool try_mithril = allow_mithril && (depth >= 12) && (rand_int(100) < 45);

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
        difficulty = 3;
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
        difficulty = 2;
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
        difficulty = 1;
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
        difficulty = 3;
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

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

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
static bool do_cmd_disarm_aux(int y, int x)
{
    int score, difficulty, result;
    int power = 0; // default to soothe compiler warnings

    cptr name;

    bool more = false;

    /* Verify legality */
    if (!do_cmd_disarm_test(y, x))
        return (false);

    /* Get the trap name */
    name = (f_name + f_info[cave_feat[y][x]].name);

    /* Get the score in favour (=perception) */
    score = p_ptr->skill_use[S_PER];

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

    // Base difficulty is the trap power
    difficulty = power;

    /* Penalize some conditions */
    if (p_ptr->blind || no_light() || p_ptr->image)
        difficulty += 5;
    if (p_ptr->confused)
        difficulty += 5;

    // perform the check
    result = skill_check(PLAYER, score, difficulty, NULL);

    /* Success, always succeed with player trap */
    if (result > 0)
    {
        /* Special message for glyphs. */
        if (cave_feat[y][x] == FEAT_GLYPH)
            msg_format("You have scuffed the %s.", name);

        /* Normal message otherwise */
        else
            msg_format("You have disarmed the %s.", name);

        /* Forget the trap */
        cave_info[y][x] &= ~(CAVE_MARK);

        /* Remove the trap */
        cave_set_feat(y, x, FEAT_FLOOR);
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

    /* See if only one target */
    if (num_traps || num_chests)
    {
        if (num_traps + num_chests <= 1)
            p_ptr->command_dir = coords_to_dir(y, x);
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

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
static bool do_cmd_bash_aux(int y, int x)
{
    int score, difficulty, power;

    bool more = false;
    bool success = false;

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

        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
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
    else if (!get_door_dir("Bash which door?", is_closed, &dir))
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
        if (!do_cmd_bash_aux(y, x))
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
    if (cave_o_idx[y][x])
    {
        object_type* o_ptr = &o_list[cave_o_idx[y][x]];

        if (o_ptr->tval == TV_CHEST)
        {
            chest_present = true;

            if ((o_ptr->pval > 0) && object_chest_trap_flags(o_ptr)
                && object_known_p(o_ptr))
                chest_trap = true;
        }
        else if ((o_ptr->tval == TV_SKELETON)
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
        do_cmd_bash_aux(y, x);
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
        more = do_cmd_disarm_chest(y, x, cave_o_idx[y][x]);
    }

    /* Open chest with no known traps */
    else if (chest_present)
    {
        /* Disarm */
        more = do_cmd_open_chest(y, x, cave_o_idx[y][x]);
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
        if (get_check("Are you sure you wish to ascend? "))
            do_cmd_go_up();
    }

    /* Descend downwards stairs */
    else if ((dir == 5) && ((feat == FEAT_MORE) || (feat == FEAT_MORE_SHAFT)))
    {
        /* Descend */
        if (get_check("Are you sure you wish to descend? "))
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
