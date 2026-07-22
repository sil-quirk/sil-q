#include "angband.h"
#include "externs.h"
#include "cmd/world/cmd-interact-chest.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

bool trapped_stairs(void)
{
    int chance;

    chance = p_ptr->staircasiness / 100;
    chance = chance * chance * chance;
    chance = chance / 10000;

    if (p_ptr->on_the_run)
        chance = 0;

    // msg_debug("%d, %d", p_ptr->staircasiness, chance);

    if (percent_chance(chance))
        return (true);
    else
        return (false);
}

/*
 * Go up a staircase
 */
void do_cmd_go_up(void)
{
    int min;
    int new;

    /* Verify stairs */
    if (!cave_up_stairs_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You see no up staircase here.");
        return;
    }

    /* Ironman */
    if (birth_ironman && (silmarils_possessed() == 0))
    {
        msg_print("You have vowed not to return until you hold a Silmaril.");
        return;
    }

    if (chosen_oath(OATH_IRON) && !oath_invalid(OATH_IRON) &&
       (silmarils_possessed() == 0))
    {
        /* Use oath-specific confirmation prompt */
        char* prompt = oath_confirmation_prompt(OATH_IRON);
        if (!prompt || !prompt[0]) prompt = "Are you certain you wish to break your Oath of Iron?";
        
        if (get_check_oath_multiline(prompt))
        {
            /* Curse message and selection handled by apply_oath_breaking_curse */
            do_cmd_note("Broke your oath", p_ptr->depth);
            apply_oath_breaking_curse(OATH_IRON);
            
            /* Only mark oath as broken if player actually has it */
            p_ptr->oaths_broken |= OATH_IRON_FLAG;
        }
        else
        {
            return;
        }
    }

    // warn player if they have an active Nienna quest and are trying to leave
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE)
    {
        msg_print("Nienna's voice echoes in your mind:");
        msg_print("'If you leave now, you will have failed the mercy quest.'");
        msg_print("'All the compassion you have shown will be for naught.'");
        if (!get_check_near(p_ptr->py, p_ptr->px, "Are you sure you wish to abandon the quest and ascend? "))
        {
            return;
        }
    }

    // warn player if they have an active Aulë quest and are trying to leave
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        msg_print("The forge fires dim as you prepare to leave...");
        msg_print("Abandoning Aulë's forge will mean failure of the quest.");
        if (!get_check_near(p_ptr->py, p_ptr->px, "Are you sure you wish to abandon the forge and ascend? "))
        {
            return;
        }
    }

    // warn player if they have an active Mandos quest and are trying to leave
    if (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
    {
        msg_print("The spirits in the tomb grow restless as you prepare to leave...");
        msg_print("Abandoning the tomb will mean failure of Mandos' quest.");
        if (!get_check_near(p_ptr->py, p_ptr->px, "Are you sure you wish to abandon the tomb and ascend? "))
        {
            return;
        }
    }

    if (!varda_quest_confirm_leave_bastion())
    {
        return;
    }

    /* Hack -- take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Calculate the shallowest a player is allowed to go */
    min = min_depth();

    /* At 1000ft, once locked in (by time or by entering Morgoth's hall),
     * you cannot retreat without a Silmaril. */
    if ((p_ptr->depth == MORGOTH_DEPTH) && (min == MORGOTH_DEPTH)
        && (silmarils_possessed() == 0))
    {
        msg_print("You enter a maze of staircases, but cannot find your way.");

        return;
    }

    // Store information for the combat rolls window
    combat_roll_special_char
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_char;
    combat_roll_special_attr
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_attr;

    // calculate the new depth to arrive at
    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_LESS_SHAFT)
        && (p_ptr->depth > 0))
    {
        /* Create a way back (usually) */
        p_ptr->create_stair = FEAT_MORE_SHAFT;

        new = p_ptr->depth - 2;
    }
    else
    {
        /* Create a way back */
        p_ptr->create_stair = FEAT_MORE;

        new = p_ptr->depth - 1;
    }

    // deal with most cases where you can't find your way
    if ((new < min)
        && !((p_ptr->depth == MORGOTH_DEPTH) && (silmarils_possessed() > 0)))
    {
        message(MSG_STAIRS, 0,
            "You enter a maze of up staircases, but cannot find your way.");

        // deal with trapped stairs when trying and failing to go upwards
        if (trapped_stairs())
        {
            msg_print("The stairs crumble beneath you!");
            message_flush();
            msg_print("You fall through...");
            message_flush();
            msg_print("...and land somewhere deeper in the Iron Hells.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell through a crumbling stair", p_ptr->depth);

            // take some damage
            falling_damage(false);

            // no stairs back
            p_ptr->create_stair = false;
        }

        else
        {
            if (p_ptr->depth == min)
            {
                message(MSG_STAIRS, 0, "You emerge near where you began.");
            }
            else
            {
                message(
                    MSG_STAIRS, 0, "You emerge even deeper in the dungeon.");
            }

            if (p_ptr->create_stair == FEAT_MORE)
            {
                /* Change the way back */
                p_ptr->create_stair = FEAT_LESS;
            }
            else
            {
                /* Change the way back */
                p_ptr->create_stair = FEAT_LESS_SHAFT;
            }
        }

        new = min;
    }

    // deal with cases where you can find your way
    else
    {
        message(MSG_STAIRS, 0, "You enter a maze of up staircases.");

        if (silmarils_possessed() > 0)
        {
            message(MSG_STAIRS, 0, "The divine light reveals the way.");
        }

        if ((p_ptr->depth == MORGOTH_DEPTH) && (silmarils_possessed() > 0))
        {
            if (!p_ptr->morgoth_slain)
            {
                msg_print("As you climb the stair, a great cry of rage and "
                          "anguish comes "
                          "from below.");
                msg_print("Make quick your escape: it shall be hard-won.");
            }

            // set the 'on the run' flag
            p_ptr->on_the_run = true;

            // remove the 'truce' flag if it hasn't been done already
            p_ptr->truce = false;
            
            /* Check for crown theft and silmarils */
            /* Priority: Crown theft is more serious than individual silmarils */
            int crown_art = has_iron_crown();
            int sils = silmarils_possessed();
            int target_state = 1;  // Default: just crown dropped
            
            log_debug("do_cmd_go_up: pursuit begins - has_crown=%d, silmarils=%d, current_state=%d",
                     crown_art, sils, p_ptr->morgoth_state);
            
            /* Determine target anger state based on what player has */
            if (crown_art > 0)
            {
                /* Player has the crown itself - this is a major theft! */
                /* Crown with 0 silmarils still means you stole his crown -> State 3 */
                target_state = 3;
                log_debug("do_cmd_go_up: player has crown (art=%d), target_state=3", crown_art);
                
                /* If crown still has silmarils on it, that's even worse */
                if (crown_art == ART_MORGOTH_3)  // 3 silmarils on crown
                {
                    target_state = 4;
                    log_debug("do_cmd_go_up: crown has all 3 silmarils, target_state=4");
                }
            }
            else if (sils > 0)
            {
                /* Player has prised silmarils (not carrying crown) */
                target_state = 1 + sils;  /* 1 sil -> state 2, 2 sils -> state 3, 3 sils -> state 4 */
                log_debug("do_cmd_go_up: player has %d prised silmarils, target_state=%d", sils, target_state);
            }
            
            /* Apply anger if target exceeds current state */
            if (target_state > p_ptr->morgoth_state)
            {
                if (crown_art > 0)
                {
                    /* Crown theft messages */
                    if (target_state >= 4)
                    {
                        msg_print("Morgoth's rage shakes the very foundations of Angband!");
                        msg_print("You have stolen his crown with all the Silmarils intact!");
                    }
                    else  // State 3
                    {
                        msg_print("Morgoth howls in rage - you have stolen his Iron Crown!");
                    }
                }
                else if (sils > 0)
                {
                    /* Silmaril theft messages (without crown) */
                    switch(sils)
                    {
                        case 1:
                            msg_print("Morgoth roars with rage as he realizes a Silmaril is missing!");
                            break;
                        case 2:
                            msg_print("Morgoth howls in fury - two Silmarils stolen!");
                            break;
                        case 3:
                            msg_print("Morgoth's wrath is terrible - all Silmarils are gone!");
                            break;
                    }
                }
                
                log_debug("do_cmd_go_up: calling anger_morgoth(%d)", target_state);
                anger_morgoth(target_state);
            }
        }

        // deal with trapped stairs when going upwards
        else if (trapped_stairs())
        {
            msg_print("The stairs crumble beneath you!");
            message_flush();
            msg_print("You fall through...");
            message_flush();
            msg_print("...and land somewhere deeper in the Iron Hells.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell through a crumbling stair", p_ptr->depth);

            // take some damage
            falling_damage(false);

            // no stairs back
            p_ptr->create_stair = false;

            // go to a lower floor
            new ++;
        }
    }

    varda_quest_fail_if_bastion_missed();

    // make a note if the player loses a greater vault
    note_lost_greater_vault();

    /* New depth */
    p_ptr->depth = new;

    /* Reset tulkas quest */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    }

    /* Reset niena quest */
    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
        msg_print("You have failed Nienna's mercy quest by leaving the level.");
    }

    /* Reset aule quest if active */
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        msg_print("You have abandoned Aulë's forge. The quest is lost.");
    }
    else if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    }

    /* Reset mandos quest if active */
    if (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
    {
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
        msg_print("You have abandoned the tomb. Mandos' quest is lost.");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT)
    {
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    }

    /* Reset Varda quest if she was waiting on the previous level */
    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT)
    {
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_level = 0;
        /* Encountering a quest giver still counts against the initiated quest cap. */
    }

    // another staircase has been used...
    p_ptr->stairs_taken++;
    p_ptr->staircasiness += 1000;

    /* Remember disconnected stairs */
    if (birth_discon_stair)
        p_ptr->create_stair = false;

    /* Leaving */
    p_ptr->leaving = true;
}

/*
 * Go down a staircase
 */
void do_cmd_go_down(void)
{
    int min;
    int new;

    /* Verify stairs */
    if (!cave_down_stairs_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You see no down staircase here.");
        return;
    }

    // special message for tutorial
    if (p_ptr->game_type == -1)
    {
        // display the tutorial leaving text
        if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE)
        {
            pause_with_text(tutorial_leave_text, 5, 10, NULL, 0);
        }
        else
        {
            pause_with_text(tutorial_win_text, 5, 10, NULL, 0);
        }

        p_ptr->is_dead = true;
        p_ptr->energy_use = 100;
        p_ptr->leaving = true;
        close_game();
        return;
    }

    // warn player if they have an active Nienna quest and are trying to leave
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE)
    {
        msg_print("Nienna's voice echoes in your mind:");
        msg_print("'If you leave now, you will have failed the mercy quest.'");
        msg_print("'All the compassion you have shown will be for naught.'");
        if (!get_check_near(p_ptr->py, p_ptr->px, "Are you sure you wish to abandon the quest and descend? "))
        {
            return;
        }
    }

    // warn player if they have an active Aulë quest and are trying to leave
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        msg_print("The forge fires dim as you prepare to leave...");
        msg_print("Abandoning Aulë's forge will mean failure of the quest.");
        if (!get_check_near(p_ptr->py, p_ptr->px, "Are you sure you wish to abandon the forge and descend? "))
        {
            return;
        }
    }

    // warn player if they have an active Mandos quest and are trying to leave
    if (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
    {
        msg_print("The spirits in the tomb grow restless as you prepare to leave...");
        msg_print("Abandoning the tomb will mean failure of Mandos' quest.");
        if (!get_check_near(p_ptr->py, p_ptr->px, "Are you sure you wish to abandon the tomb and descend? "))
        {
            return;
        }
    }

    if (!varda_quest_confirm_leave_bastion())
    {
        return;
    }

    // Do not descend from the Gates
    if (p_ptr->depth == 0)
    {
        msg_print("You have made it to the very gates of Angband and can once "
                  "more taste "
                  "the freshness on the air.");
        msg_print("You will not re-enter that fell pit.");
        return;
    }

    // Store information for the combat rolls window
    combat_roll_special_char
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_char;
    combat_roll_special_attr
        = (&f_info[cave_feat[p_ptr->py][p_ptr->px]])->d_attr;

    min = min_depth();
    if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT)
        && (p_ptr->depth < MORGOTH_DEPTH - 1))
    {
        /* Create a way back (usually) */
        p_ptr->create_stair = FEAT_LESS_SHAFT;

        new = p_ptr->depth + 2;
    }
    else
    {
        /* Create a way back */
        p_ptr->create_stair = FEAT_LESS;

        new = p_ptr->depth + 1;
    }

    /* Hack -- take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    message(MSG_STAIRS, 0, "You enter a maze of down staircases.");

    // Can never return to the throne room...
    if ((p_ptr->on_the_run) && (new == MORGOTH_DEPTH))
    {
        message(MSG_STAIRS, 0,
            "Try though you might, you cannot find your way back to Morgoth's "
            "throne.");
        message(MSG_STAIRS, 0, "You emerge near where you began.");
        p_ptr->create_stair = FEAT_MORE;
        new = MORGOTH_DEPTH - 1;
    }

    // deal with trapped stairs
    else if (trapped_stairs())
    {
        msg_print("The stairs crumble beneath you!");
        message_flush();
        msg_print("You fall through...");
        message_flush();
        msg_print("...and land somewhere deeper in the Iron Hells.");
        message_flush();

        // add to the notes file
        do_cmd_note("Fell through a crumbling stair", p_ptr->depth);

        // take some damage
        falling_damage(false);

        // no stairs back
        p_ptr->create_stair = false;
    }

    else if (new < min)
    {
        message(MSG_STAIRS, 0, "You emerge much deeper in the dungeon.");
        new = min;
    }

    varda_quest_fail_if_bastion_missed();

    // make a note if the player loses a greater vault
    note_lost_greater_vault();

    /* New depth */
    p_ptr->depth = new;

    /* Reset tulkas quest */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    }

    /* Reset Varda quest if she was waiting on the previous level */
    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT)
    {
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_level = 0;
        /* Encountering a quest giver still counts against the initiated quest cap. */
    }

    /* Reset aule quest if active */
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        msg_print("You have abandoned Aulë's forge. The quest is lost.");
    }
    else if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    }

    /* Reset mandos quest if active */
    if (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
    {
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
        msg_print("You have abandoned the tomb. Mandos' quest is lost.");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT)
    {
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    }

    /* Reset niena quest if active */
    if (p_ptr->niena_quest >= NIENA_QUEST_ACTIVE && p_ptr->niena_quest < NIENA_QUEST_REWARDED)
    {
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
        msg_print("You have abandoned Nienna's mercy quest. The quest is lost.");
    }
    else if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
    }

    // another staircase has been used...
    p_ptr->stairs_taken++;
    p_ptr->staircasiness += 1000;

    /* Remember disconnected stairs */
    if (birth_discon_stair)
        p_ptr->create_stair = false;

    /* Leaving */
    p_ptr->leaving = true;
}

/*
 * Simple command to "search" for one turn
 */
void do_cmd_search(void)
{
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

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Search */
    search();
}

/*
 * Hack -- toggle stealth mode
 */
void do_cmd_toggle_stealth(void)
{
    /* Stop stealth mode */
    if (p_ptr->stealth_mode)
    {
        /* Clear the stealth mode flag */
        p_ptr->stealth_mode = false;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);
        if (pixel_monster_status_icons)
            p_ptr->redraw |= (PR_MAP);
    }

    /* Start stealth mode */
    else
    {
        if (p_ptr->rage)
        {
            msg_print("You are far too enraged to move stealthily.");
            return;
        }

        /* Set the stealth mode flag */
        p_ptr->stealth_mode = true;

        /* Update stuff */
        p_ptr->update |= (PU_BONUS);

        /* Redraw stuff */
        p_ptr->redraw |= (PR_STATE | PR_SPEED);
        if (pixel_monster_status_icons)
            p_ptr->redraw |= (PR_MAP);
    }

    if (pixel_monster_status_icons)
    {
        force_map_redraw();
        handle_stuff();
        Term_fresh();
    }
}

static bool walk_target_exits_gates(int y, int x)
{
    if (!p_ptr || (p_ptr->depth != 0))
        return false;

    if (!in_bounds(y, x))
        return true;

    return (y == 0) || (x == 0) || (y == p_ptr->cur_map_hgt - 1)
        || (x == p_ptr->cur_map_wid - 1);
}

bool do_cmd_walk_test(int y, int x)
{
    /* Let the Gates edge hand off to move_player(), which ends the run. */
    if (walk_target_exits_gates(y, x))
    {
        return true;
    }

    if (!in_bounds(y, x))
        return false;

    /* Hack -- walking obtains knowledge XXX XXX */
    if (!(cave_info[y][x] & (CAVE_MARK)))
        return (true);

    /* Allow attack on visible monsters */
    if ((cave_m_idx[y][x] > 0) && (mon_list[cave_m_idx[y][x]].ml))
    {
        return true;
    }

    /* Require open space */
    if (!cave_floor_bold(y, x))
    {
        /* Rubble */
        if (cave_feat[y][x] == FEAT_RUBBLE)
        {
            /* Message */
            message(MSG_HITWALL, 0, "There is a pile of rubble in the way!");

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;
        }

        /* Door */
        else if (cave_known_closed_door_bold(y, x))
        {
            /* Hack -- Handle "easy_alter" */
            return (true);
        }

        /* Wall */
        else
        {
            /* Message */
            message(MSG_HITWALL, 0, "There is a wall in the way!");

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;
        }

        /* Nope */
        return (false);
    }

    /* Okay */
    return (true);
}

/*
 * Helper function for the "walk" and "jump" commands.
 */
void do_cmd_walk(void)
{
    int y, x, dir;

    /* A movement command supersedes a lingering interaction-roll result. */
    sdl_question_menu_clear_nonblocking();

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    // convert walking in place to 'hold'
    if (dir == 5)
    {
        do_cmd_hold();
        return;
    }

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Verify legality */
    if (!do_cmd_walk_test(y, x))
        return;

    /* Take a turn */
    p_ptr->energy_use = 100;

    /* Confuse direction */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    /* Verify legality */
    if (!do_cmd_walk_test(y, x))
        return;

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

    /* Move the player */
    move_player(dir);
}

/*
 * Start running.
 *
 * Note that running while confused is not allowed.
 */
void do_cmd_run(void)
{
    int y, x, dir;

    /* A movement command supersedes a lingering interaction-roll result. */
    sdl_question_menu_clear_nonblocking();

    /* Hack XXX XXX XXX */
    if (p_ptr->confused)
    {
        msg_print("You are too confused!");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    // convert into rest
    if (dir == 5)
    {
        do_cmd_rest();
    }

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    /* Verify legality */
    if (!do_cmd_walk_test(y, x))
        return;

    /* Start run */
    run_step(dir);
}

/*
 * Hold still
 */
void do_cmd_hold(void)
{
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

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = 5;

    // store the 'focus' attribute
    p_ptr->focused = true;

    // make less noise if you did nothing at all
    // (+7 in total whether or not stealth mode is used)
    if (p_ptr->stealth_mode)
        stealth_score += 2;
    else
        stealth_score += 7;

    // passing in stealth mode removes the speed penalty (as there was no bonus
    // either)
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATE | PR_SPEED);

    /* Searching */
    search();
}

/*
 * Get items
 */
void do_cmd_pickup(void)
{
    s16b chest_o_idx;

    /* A deliberate pickup attempt is also an interaction with a closed
     * chest.  Route it through the same minigame instead of allowing pickup
     * to bypass the first trap decision.  Passive/automatic pickup remains
     * unchanged so merely stepping onto the tile does not open an overlay. */
    chest_o_idx = chest_trap_minigame
        ? chest_check(p_ptr->py, p_ptr->px) : 0;
    if (chest_o_idx && o_list[chest_o_idx].pval != 0)
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;
        (void)do_cmd_open_chest(
            p_ptr->py, p_ptr->px, chest_o_idx);
        return;
    }

    // Usually pickup if there is an object here
    if (cave_o_idx[p_ptr->py][p_ptr->px])
    {
        /* Handle "objects" */
        py_pickup();
    }

    else
    {
        msg_print("There is nothing here to get.");
    }
}

/*
 * Rest (restores hit points and mana and such)
 */
void do_cmd_rest(void)
{
    /* Prompt for time if needed */
    if (p_ptr->command_arg == 0)
    {
        p_ptr->command_arg = (-2);
    }

    // typically resting ends your current song
    if (stop_singing_on_rest)
        change_song(SNG_NOTHING);

    /* Take a turn XXX XXX XXX (?) */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = 5;

    // store the 'focus' attribute
    p_ptr->focused = true;

    /* Save the rest code */
    p_ptr->resting = p_ptr->command_arg;

    /* Cancel the arg */
    p_ptr->command_arg = 0;

    /* Cancel stealth mode */
    if (p_ptr->stealth_mode && pixel_monster_status_icons)
        p_ptr->redraw |= (PR_MAP);
    p_ptr->stealth_mode = false;

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Redraw the state */
    p_ptr->redraw |= (PR_STATE);

    /* Handle stuff */
    handle_stuff();

    /* Refresh */
    Term_fresh();
}
