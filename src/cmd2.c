/* File: cmd2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

#define THROW_PENDING_NONE -9999
static int throw_pending_slot = THROW_PENDING_NONE;

/*
 * Determines the shallowest a player is allowed to go.
 * As time goes on, they are forced deeper and deeper.
 */
int min_depth(void)
{
    int min_depth_value = min_depth_counter / 200000 + 1;

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

/*
 * Determines whether a staircase is 'trapped' like a false floor trap.
 * This means you fall a level below where you expected to end up (if you were
 * going upwards), take some minor damage, and have no stairs back.
 *
 * It gets more likely the more stairs you have recently taken.
 * It is designed to stop you stair-scumming.
 */
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
        msg_print("You have vowed to not to return until you hold a Silmaril.");
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

    // warn player if they have an active Niena quest and are trying to leave
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE)
    {
        msg_print("Niena's voice echoes in your mind:");
        msg_print("'If you leave now, you will have failed the mercy quest.'");
        msg_print("'All the compassion you have shown will be for naught.'");
        if (!get_check("Are you sure you wish to abandon the quest and ascend? "))
        {
            return;
        }
    }

    // warn player if they have an active Aule quest and are trying to leave
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        msg_print("The forge fires dim as you prepare to leave...");
        msg_print("Abandoning Aule's forge will mean failure of the quest.");
        if (!get_check("Are you sure you wish to abandon the forge and ascend? "))
        {
            return;
        }
    }

    // warn player if they have an active Mandos quest and are trying to leave
    if (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
    {
        msg_print("The spirits in the tomb grow restless as you prepare to leave...");
        msg_print("Abandoning the tomb will mean failure of Mandos' quest.");
        if (!get_check("Are you sure you wish to abandon the tomb and ascend? "))
        {
            return;
        }
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
                /* Crown with 0 silmarils still means you stole his crown → State 3 */
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
                target_state = 1 + sils;  /* 1 sil → state 2, 2 sils → state 3, 3 sils → state 4 */
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
        msg_print("You have failed Niena's mercy quest by leaving the level.");
    }

    /* Reset aule quest if active */
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        msg_print("You have abandoned Aule's forge. The quest is lost.");
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
        if (p_ptr->quest_reserved[0] &&
            p_ptr->tulkas_quest == TULKAS_QUEST_NOT_STARTED &&
            p_ptr->niena_quest == NIENA_QUEST_NOT_STARTED &&
            p_ptr->orome_quest == OROME_QUEST_NOT_STARTED &&
            p_ptr->aule_quest == AULE_QUEST_NOT_STARTED &&
            p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED &&
            p_ptr->varda_quest == VARDA_QUEST_NOT_STARTED)
        {
            p_ptr->quest_reserved[0] = 0;
        }
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

    // warn player if they have an active Niena quest and are trying to leave
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE)
    {
        msg_print("Niena's voice echoes in your mind:");
        msg_print("'If you leave now, you will have failed the mercy quest.'");
        msg_print("'All the compassion you have shown will be for naught.'");
        if (!get_check("Are you sure you wish to abandon the quest and descend? "))
        {
            return;
        }
    }

    // warn player if they have an active Aule quest and are trying to leave
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        msg_print("The forge fires dim as you prepare to leave...");
        msg_print("Abandoning Aule's forge will mean failure of the quest.");
        if (!get_check("Are you sure you wish to abandon the forge and descend? "))
        {
            return;
        }
    }

    // warn player if they have an active Mandos quest and are trying to leave
    if (p_ptr->mandos_quest >= MANDOS_QUEST_ACTIVE && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED)
    {
        msg_print("The spirits in the tomb grow restless as you prepare to leave...");
        msg_print("Abandoning the tomb will mean failure of Mandos' quest.");
        if (!get_check("Are you sure you wish to abandon the tomb and descend? "))
        {
            return;
        }
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
        if (p_ptr->quest_reserved[0] &&
            p_ptr->tulkas_quest == TULKAS_QUEST_NOT_STARTED &&
            p_ptr->niena_quest == NIENA_QUEST_NOT_STARTED &&
            p_ptr->orome_quest == OROME_QUEST_NOT_STARTED &&
            p_ptr->aule_quest == AULE_QUEST_NOT_STARTED &&
            p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED &&
            p_ptr->varda_quest == VARDA_QUEST_NOT_STARTED)
        {
            p_ptr->quest_reserved[0] = 0;
        }
    }

    /* Reset aule quest if active */
    if (p_ptr->aule_quest >= AULE_QUEST_ACTIVE && p_ptr->aule_quest < AULE_QUEST_REWARDED)
    {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        msg_print("You have abandoned Aule's forge. The quest is lost.");
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
        msg_print("You have abandoned Niena's mercy quest. The quest is lost.");
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
    }
}

/*
 * Determine if a grid contains a chest
 */
static s16b chest_check(int y, int x)
{
    s16b this_o_idx, next_o_idx = 0;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Skip unknown chests XXX XXX */
        /* if (!o_ptr->marked) continue; */

        /* Check for chest */
        if (o_ptr->tval == TV_CHEST)
            return (this_o_idx);
    }

    /* No chest */
    return (0);
}

/*
 * Allocate objects upon opening a chest
 *
 * Disperse treasures from the given chest, centered at (x,y).
 *
 */
static void chest_death(int y, int x, s16b o_idx)
{
    int number;
    bool generated_an_item = false;

    object_type* o_ptr;

    object_type* i_ptr;

    object_type object_type_body;

    /* Get the chest */
    o_ptr = &o_list[o_idx];

    /* Determine how much to drop (see above) */
    number = (o_ptr->sval >= SV_CHEST_MIN_LARGE) ? 4 : rand_range(2, 3);

    /* Zero pval means empty chest */
    if (!o_ptr->pval)
        return;

    /* Opening a chest */
    object_generation_mode = OB_GEN_MODE_CHEST;

    /* Determine the "value" of the items */
    int base_depth = ABS(o_ptr->pval);
    if (base_depth < 1)
        base_depth = 1;
    int penalty_depth = base_depth + 5; /* all chests add 5 (min-depth penalty only) */

    level_partition_kind part_kind = LEVEL_PART_NONE;
    if (o_ptr->xtra1 & 0x80)
        part_kind = (level_partition_kind)(o_ptr->xtra1 & 0x7F);
    if (part_kind <= LEVEL_PART_NONE || part_kind >= LEVEL_PART_MAX)
        part_kind = level_partition_kind_for_point(y, x);
    drop_profile part_profile;
    drop_profile_for_partition_kind(part_kind, &part_profile);

    /* Chests should not drop supply items */
    part_profile.weight_supply = 0;

    if (o_ptr->sval == SV_CHEST_PRESENT)
        number = 1;

    /* Chest-specific difficulty bonus */
    drop_quality chest_quality = DROP_QUALITY_NORMAL;
    if ((o_ptr->sval == SV_CHEST_SMALL_WOODEN)
        || (o_ptr->sval == SV_CHEST_LARGE_WOODEN))
        chest_quality = DROP_QUALITY_GOOD;
    else if ((o_ptr->sval == SV_CHEST_SMALL_STEEL)
        || (o_ptr->sval == SV_CHEST_LARGE_STEEL))
        chest_quality = DROP_QUALITY_GREAT;
    else if ((o_ptr->sval == SV_CHEST_SMALL_JEWELLED)
        || (o_ptr->sval == SV_CHEST_LARGE_JEWELLED))
        chest_quality = DROP_QUALITY_SUPERB;

    /* Drop some objects (non-chests) */
    for (; number > 0; --number)
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        bool ok = drop_generate_object_profiled_depths(base_depth, penalty_depth,
            chest_quality, DROP_TYPE_UNTHEMED, 0, true, &part_profile, i_ptr);

        if (ok)
        {
            generated_an_item = true;
            drop_near(i_ptr, -1, y, x);
        }
    }

    /* No longer opening a chest */
    object_generation_mode = OB_GEN_MODE_NORMAL;

    /* Empty */
    o_ptr->pval = 0;

    /*Paranoia, delete chest theme*/
    o_ptr->xtra1 = 0;

    /* Known */
    object_known(o_ptr);

    if (!generated_an_item)
    {
        msg_print("The chest is empty.");
    }
}

/*
 * Chests have traps too.
 *
 * Exploding chest destroys contents (and traps).
 * Note that the chest itself is never destroyed.
 */
static void chest_trap(int y, int x, s16b o_idx)
{
    int trap, dam;

    object_type* o_ptr = &o_list[o_idx];

    (void)x; // casting to soothe compilation warnings
    (void)y;

    /* Ignore disarmed chests */
    if (o_ptr->pval <= 0)
        return;

    /* Obtain the traps */
    trap = chest_traps[o_ptr->pval];

    // Store information for the combat rolls window
    combat_roll_special_char = object_char(o_ptr);
    combat_roll_special_attr = object_attr(o_ptr);

    /* Needle - Hallucination */
    if (trap & (CHEST_NEEDLE_HALLU))
    {
        if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            if (allow_player_image(NULL))
            {
                set_image(p_ptr->image + damroll(80, 4));
            }
            else
            {
                msg_print("You resist the effects!");
            }
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Needle - Entrancement */
    if (trap & (CHEST_NEEDLE_ENTRANCE))
    {
        if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            if (allow_player_entrancement(NULL))
            {
                set_entranced(damroll(10, 4));
            }
            else
            {
                msg_print("You resist the effects!");
            }
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Needle - Lose strength */
    if (trap & (CHEST_NEEDLE_LOSE_STR))
    {
        if (skill_check(NULL, 2, p_ptr->stat_use[A_DEX] * 2, PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            (void)do_dec_stat(A_STR, NULL);
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Confusion Gas */
    if (trap & (CHEST_GAS_CONF))
    {
        msg_print("A noxious vapour escapes from the chest!");
        if (allow_player_confusion(NULL))
        {
            (void)set_confused(p_ptr->confused + damroll(4, 4));
        }
        else
        {
            msg_print("You resist the effects.");
        }
    }

    /* Acrid Smoke */
    if (trap & (CHEST_GAS_STUN))
    {
        msg_print("Acrid smoke pours from the chest!");
        if (allow_player_stun(NULL))
        {
            msg_print("It fills your lungs and your mind reels.");

            dam = damroll(3, 4);

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(3, 4, dam, -1, -1, 0, 0, GF_HURT, false);

            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(dam, "a trapped chest");

            set_stun(p_ptr->stun + damroll(30, 4));
        }
        else
        {
            msg_print("You resist the effects.");
        }
    }

    /* Poison Gas */
    if (trap & (CHEST_GAS_POISON))
    {
        msg_print("A noxious vapour escapes from the chest!");

        update_combat_rolls1b(NULL, PLAYER, true);

        (void)pois_dam_pure(10, 4, true);
    }

    /* Flame */
    if (trap & (CHEST_FLAME))
    {
        msg_print("There is a sudden burst of flame!");

        update_combat_rolls1b(NULL, PLAYER, true);

        fire_dam_pure(10, 4, true, "a trapped chest");

        /* Make some noise */
        monster_perception(true, false, -5);
    }
}

static bool generate_poor_quality_object(object_type* o_ptr)
{
    bool search_failed = false;

    int object_roll = dieroll(5);

    if (object_roll == 1)
    {
        object_prep(o_ptr, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
    }
    else if (object_roll == 2)
    {
        object_prep(o_ptr, lookup_kind(TV_LIGHT, SV_LIGHT_TORCH));
        o_ptr->timeout = rand_range(400, 800);
    }
    else if (object_roll == 3)
    {
        object_prep(o_ptr, lookup_kind(TV_CLOAK, SV_CLOAK));
    }
    else if (object_roll == 4)
    {
        object_prep(o_ptr, lookup_kind(TV_FOOD, SV_FOOD_MEAT));
    }
    else
    {
        search_failed = !make_object(
            o_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_DAMAGED);
    }

    if (!search_failed)
        object_known(o_ptr);
    return search_failed;
}

typedef struct skeleton_note_profile {
    int note_chance;
    int weight_scale[SKEL_HINT_MAX];
} skeleton_note_profile;

typedef struct skeleton_note_state {
    int level_depth;
    int note_cap;
    int notes_shown;
    int map_wid;
    int map_hgt;
    u32b hint_used_mask;
    byte seen_count;
    s16b seen_ids[SKELETON_NOTE_SEEN_MAX];
} skeleton_note_state;

#define SKELETON_TIP_MAX_DEPTH 7

static skeleton_note_state g_skeleton_note_state = { -1, 0, 0, 0, 0, 0, 0, {0} };
static int g_skeleton_note_entry_count = -1;
static const int skeleton_hint_base_weight[SKEL_HINT_MAX]
    = {
        0,  /* NONE */
        113, /* GREAT_VAULT (was 75 -> 1.5x rounded) */
        60, /* VAULT_ARTIFACT */
        55, /* STAIRS */
        0,  /* PARTITION_PRESENCE (deprecated) */
        45, /* FORGE */
        70, /* UNIQUE */
        180, /* TIP */
        35, /* SIZE */
        180, /* QUEST (doubled from 90) */
        40, /* PART_LABYRINTH */
        40, /* PART_CHASM */
        40, /* PART_CAVE */
        40, /* PART_CAVE_ICE */
        40, /* PART_CAVE_FIRE */
        40  /* PART_CAVE_POIS */
    };

static void skeleton_note_ensure_level_state(void);
static bool skeleton_note_has_unseen_template(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint);
static void skeleton_note_recount_templates(void)
{
    g_skeleton_note_entry_count = 0;

    if (!skeleton_note_info || !z_info)
        return;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role == SKELETON_NOTE_ROLE_NONE)
            continue;
        if (t->text == 0 || t->weight == 0)
            continue;
        g_skeleton_note_entry_count++;
    }
}

static int skeleton_note_entry_count(void)
{
    if (g_skeleton_note_entry_count < 0)
        skeleton_note_recount_templates();
    return g_skeleton_note_entry_count;
}

static void skeleton_note_reset_seen(void)
{
    g_skeleton_note_state.seen_count = 0;
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
        g_skeleton_note_state.seen_ids[i] = -1;
}

static bool skeleton_note_seen_id(s16b id)
{
    for (int i = 0; i < g_skeleton_note_state.seen_count; ++i)
    {
        if (g_skeleton_note_state.seen_ids[i] == id)
            return true;
    }
    return false;
}

static void skeleton_note_record_seen(s16b id)
{
    if (id < 0)
        return;
    if (skeleton_note_seen_id(id))
        return;

    if (g_skeleton_note_state.seen_count < SKELETON_NOTE_SEEN_MAX)
    {
        g_skeleton_note_state.seen_ids[g_skeleton_note_state.seen_count++] = id;
        return;
    }

    /* Keep a simple rolling window to reduce repetition across levels. */
    for (int i = 1; i < SKELETON_NOTE_SEEN_MAX; ++i)
        g_skeleton_note_state.seen_ids[i - 1] = g_skeleton_note_state.seen_ids[i];
    g_skeleton_note_state.seen_ids[SKELETON_NOTE_SEEN_MAX - 1] = id;
}

static int skeleton_note_size_bucket(const level_layout_info* layout)
{
    if (!layout)
        return 0;

    /*
     * Size buckets for skeleton-note pacing and {SIZEWORD}.
     *
     * We base this on the current map's side length as a fraction of the
     * maximum supported dungeon side (MAX_DUNGEON_*), so it continues to work
     * even if the generator's exact size distribution changes.
     *
     * With current square levels (generate.c): 88..231 per side, these map to:
     *   0: <  57% (<= 121)   "narrow"
     *   1: <  76% (<= 165)   "broad"
     *   2: <  90% (<= 198)   "sprawling"
     *   3: >= 90% (>= 209)   "vast"
     */
    int side = MAX(layout->map_wid, layout->map_hgt);
    int max_side = MAX(MAX_DUNGEON_WID, MAX_DUNGEON_HGT);
    if (side <= 0 || max_side <= 0)
        return 0;

    int pct = (side * 100) / max_side;
    if (pct >= 90)
        return 3;
    if (pct >= 76)
        return 2;
    if (pct >= 57)
        return 1;
    return 0;
}

static int skeleton_note_cap_from_layout(const level_layout_info* layout)
{
    int bucket = skeleton_note_size_bucket(layout);
    int cap = 1 + bucket;
    if (cap < 1)
        cap = 1;
    if (cap > 4)
        cap = 4;
    return cap;
}

static skeleton_note_profile skeleton_note_profile_for_sval(byte sval)
{
    skeleton_note_profile prof;
    memset(&prof, 0, sizeof(prof));
    
    /* Default all hint scales to 100 */
    for (int i = 0; i < SKEL_HINT_MAX; ++i)
        prof.weight_scale[i] = 100;

    switch (sval)
    {
    case SV_SKELETON_ELF:
        prof.note_chance = 55;
        prof.weight_scale[SKEL_HINT_GREAT_VAULT] = 110;
        prof.weight_scale[SKEL_HINT_VAULT_ARTIFACT] = 120;
        prof.weight_scale[SKEL_HINT_STAIRS] = 90;
        prof.weight_scale[SKEL_HINT_PARTITION_PRESENCE] = 0;
        prof.weight_scale[SKEL_HINT_PART_LABYRINTH] = 155;
        prof.weight_scale[SKEL_HINT_PART_CHASM] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE_ICE] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE_FIRE] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE_POIS] = 155;
        prof.weight_scale[SKEL_HINT_FORGE] = 110;
        prof.weight_scale[SKEL_HINT_UNIQUE_MONSTER] = 120;
        prof.weight_scale[SKEL_HINT_TIP] = 120;
        break;
    case SV_SKELETON_HUMAN:
        prof.note_chance = 40;
        prof.weight_scale[SKEL_HINT_GREAT_VAULT] = 120;
        prof.weight_scale[SKEL_HINT_VAULT_ARTIFACT] = 105;
        prof.weight_scale[SKEL_HINT_STAIRS] = 140;
        prof.weight_scale[SKEL_HINT_PARTITION_PRESENCE] = 0;
        prof.weight_scale[SKEL_HINT_PART_LABYRINTH] = 95;
        prof.weight_scale[SKEL_HINT_PART_CHASM] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE_ICE] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE_FIRE] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE_POIS] = 95;
        prof.weight_scale[SKEL_HINT_FORGE] = 120;
        prof.weight_scale[SKEL_HINT_UNIQUE_MONSTER] = 120;
        prof.weight_scale[SKEL_HINT_TIP] = 240;
        break;
    case SV_SKELETON_ORC:
        prof.note_chance = 25;
        prof.weight_scale[SKEL_HINT_GREAT_VAULT] = 170;
        prof.weight_scale[SKEL_HINT_VAULT_ARTIFACT] = 180;
        prof.weight_scale[SKEL_HINT_STAIRS] = 120;
        prof.weight_scale[SKEL_HINT_PARTITION_PRESENCE] = 0;
        prof.weight_scale[SKEL_HINT_PART_LABYRINTH] = 65;
        prof.weight_scale[SKEL_HINT_PART_CHASM] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE_ICE] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE_FIRE] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE_POIS] = 65;
        prof.weight_scale[SKEL_HINT_FORGE] = 160;
        prof.weight_scale[SKEL_HINT_UNIQUE_MONSTER] = 90;
        prof.weight_scale[SKEL_HINT_TIP] = 100;
        break;
    default:
        break;
    }

    return prof;
}

static int skeleton_note_tip_override_chance(byte sval, int depth)
{
    if (depth < 1)
        depth = 1;
    if (depth > SKELETON_TIP_MAX_DEPTH)
        return 0;

    int t = SKELETON_TIP_MAX_DEPTH - depth; /* 0.. */
    int chance = 0;
    switch (sval)
    {
    case SV_SKELETON_HUMAN:
        chance = 35 + t * 6;
        break;
    case SV_SKELETON_ELF:
        chance = 25 + t * 5;
        break;
    case SV_SKELETON_ORC:
        chance = 20 + t * 4;
        break;
    default:
        chance = 0;
        break;
    }

    if (chance > 90)
        chance = 90;
    if (chance < 0)
        chance = 0;
    return chance;
}

static const char* partition_label(level_partition_kind kind, big_cave_type_t cave_type)
{
    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return "a maze of hewn stone";
    case LEVEL_PART_BIG_CAVE:
        switch (cave_type)
        {
        case BIG_CAVE_ICE:
            return "a vast ice cavern";
        case BIG_CAVE_FIRE:
            return "a vast cavern of fire";
        case BIG_CAVE_POIS:
            return "a cavern of poisonous vapours";
        default:
            return "a vast cavern";
        }
    case LEVEL_PART_CHASM:
        return "a chasm with perilous bridges";
    case LEVEL_PART_RUINED:
        return "ruined halls";
    case LEVEL_PART_CAVEY:
        return "natural caves";
    case LEVEL_PART_ROOMY:
        return "long halls of wrought stone";
    default:
        return "wandering tunnels";
    }
}

static const char* partition_hazard_label(level_partition_kind kind, big_cave_type_t cave_type)
{
    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return "its turns and dead ends will unmake your bearings";
    case LEVEL_PART_CHASM:
        return "bridges are narrow, and a single misstep is death";
    case LEVEL_PART_BIG_CAVE:
        switch (cave_type)
        {
        case BIG_CAVE_ICE:
            return "the floor is slick and the cold bites";
        case BIG_CAVE_FIRE:
            return "the air burns and smoke chokes";
        case BIG_CAVE_POIS:
            return "green fumes cling low and sting the lungs";
        default:
            return "sound carries far, and there is little cover";
        }
    default:
        return "";
    }
}

static const char* size_word_for_bucket(int bucket)
{
    switch (bucket)
    {
    case 0:
        return "narrow";
    case 1:
        return "broad";
    case 2:
        return "sprawling";
    default:
        return "vast";
    }
}

static const char* skeleton_note_pick_size_word(const level_layout_info* layout)
{
    int actual = layout ? skeleton_note_size_bucket(layout) : 0;
    if (actual < 0)
        actual = 0;
    if (actual > 3)
        actual = 3;

    int weights[4] = {0};
    switch (actual)
    {
    case 0:
        weights[0] = 70;
        weights[1] = 20;
        weights[2] = 8;
        weights[3] = 2;
        break;
    case 1:
        weights[0] = 15;
        weights[1] = 55;
        weights[2] = 25;
        weights[3] = 5;
        break;
    case 2:
        weights[0] = 5;
        weights[1] = 20;
        weights[2] = 55;
        weights[3] = 20;
        break;
    default:
        weights[0] = 2;
        weights[1] = 8;
        weights[2] = 25;
        weights[3] = 65;
        break;
    }

    int total = 0;
    for (int i = 0; i < 4; ++i)
        total += weights[i];

    if (total <= 0)
        return size_word_for_bucket(actual);

    int roll = rand_int(total);
    for (int i = 0; i < 4; ++i)
    {
        if (roll < weights[i])
            return size_word_for_bucket(i);
        roll -= weights[i];
    }

    return size_word_for_bucket(actual);
}

static const char* skeleton_note_fallback_opening(byte sval)
{
    switch (sval)
    {
    case SV_SKELETON_ELF:
        return "Flowing script, penned in calmer hours:";
    case SV_SKELETON_HUMAN:
        return "A hurried note from steadier hands:";
    case SV_SKELETON_ORC:
        return "Jagged scrawl on greasy hide:";
    default:
        return "A brittle note clutched by the bones:";
    }
}

static const char* skeleton_note_fallback_signoff(byte sval)
{
    switch (sval)
    {
    case SV_SKELETON_ELF:
        return "If you endure, tread softly.";
    case SV_SKELETON_HUMAN:
        return "Maybe you'll fare better.";
    case SV_SKELETON_ORC:
        return "Take what we couldn't.";
    default:
        return "";
    }
}

static bool level_has_greater_vault(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (cave_info[y][x] & CAVE_G_VAULT)
                return true;
        }
    }
    return false;
}

static bool vault_has_ground_artifact(void)
{
    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        if (!o_ptr->k_idx)
            continue;
        if (o_ptr->held_m_idx)
            continue;
        if (!o_ptr->name1)
            continue;
        if (o_ptr->iy >= p_ptr->cur_map_hgt || o_ptr->ix >= p_ptr->cur_map_wid)
            continue;

        if (cave_info[o_ptr->iy][o_ptr->ix] & CAVE_G_VAULT)
            return true;
    }
    return false;
}

static bool level_has_stairs_down(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            if (feat == FEAT_MORE || feat == FEAT_MORE_SHAFT)
                return true;
        }
    }
    return false;
}

static bool level_has_stairs_up(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            if (feat == FEAT_LESS || feat == FEAT_LESS_SHAFT)
                return true;
        }
    }
    return false;
}

static bool level_has_forge(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (cave_forge_bold(y, x))
                return true;
        }
    }
    return false;
}

static bool skeleton_note_is_quest_giver_r_idx(int r_idx)
{
    switch (r_idx)
    {
    case R_IDX_TULKAS:
    case R_IDX_AULE:
    case R_IDX_MANDOS:
    case R_IDX_NIENA:
    case R_IDX_OROME:
    case R_IDX_VARDA:
        return true;
    default:
        return false;
    }
}

static const char* skeleton_note_quest_site_name(int r_idx)
{
    switch (r_idx)
    {
    case R_IDX_TULKAS:
        return "a strong Power";
    case R_IDX_AULE:
        return "a smith of the West";
    case R_IDX_MANDOS:
        return "a doomsman in shadow";
    case R_IDX_NIENA:
        return "a lady of pity";
    case R_IDX_OROME:
        return "a hunter of the West";
    case R_IDX_VARDA:
        return "a lady of the stars";
    case R_IDX_DURUIN:
        return "a bastion of shadow";
    default:
        return "a Power";
    }
}

static bool level_has_quest_giver(void)
{
    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;
        if (skeleton_note_is_quest_giver_r_idx(m_ptr->r_idx))
            return true;
    }
    return false;
}

static bool level_has_quest_vault(void)
{
    if (p_ptr->aule_level == p_ptr->depth && p_ptr->aule_quest != AULE_QUEST_NOT_STARTED)
        return true;
    if (p_ptr->mandos_level == p_ptr->depth && p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED)
        return true;
    if (p_ptr->varda_level == p_ptr->depth && p_ptr->varda_vault_placed)
        return true;

    /* Fallback: detect Duruin's bastion by its guardian. */
    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;
        if (m_ptr->r_idx == R_IDX_DURUIN)
            return true;
    }

    return false;
}

static void skeleton_note_big_cave_counts(
    const level_layout_info* layout, int counts[BIG_CAVE_TYPE_MAX], int* out_none)
{
    for (int i = 0; i < BIG_CAVE_TYPE_MAX; ++i)
        counts[i] = 0;

    if (out_none)
        *out_none = 0;

    if (!layout || layout->partition_count <= 0 || layout->big_cave_parts <= 0)
        return;

    for (int pi = 0; pi < layout->partition_count; ++pi)
    {
        big_cave_type_t cave_type = level_partition_big_cave_type_for_index(pi);
        if (cave_type > BIG_CAVE_NONE && cave_type < BIG_CAVE_TYPE_MAX)
            counts[cave_type]++;
    }

    int typed = 0;
    for (int t = 1; t < BIG_CAVE_TYPE_MAX; ++t)
        typed += counts[t];

    int none = layout->big_cave_parts - typed;
    if (none < 0)
        none = 0;
    if (out_none)
        *out_none = none;
}

void skeleton_note_level_reset(void)
{
    level_layout_info layout;
    level_layout_info_current(&layout);

    g_skeleton_note_state.level_depth = p_ptr->depth;
    g_skeleton_note_state.map_wid = layout.map_wid;
    g_skeleton_note_state.map_hgt = layout.map_hgt;
    g_skeleton_note_state.note_cap = skeleton_note_cap_from_layout(&layout);
    g_skeleton_note_state.notes_shown = 0;
    g_skeleton_note_state.hint_used_mask = 0;
    g_skeleton_note_entry_count = -1;

    if (g_skeleton_note_state.note_cap < 1)
        g_skeleton_note_state.note_cap = 1;
}

static void skeleton_note_ensure_level_state(void)
{
    if (g_skeleton_note_state.level_depth != p_ptr->depth
        || g_skeleton_note_state.map_wid != p_ptr->cur_map_wid
        || g_skeleton_note_state.map_hgt != p_ptr->cur_map_hgt)
    {
        skeleton_note_level_reset();
    }
}

void skeleton_note_get_state(skeleton_note_state_save* out)
{
    if (!out)
        return;
    skeleton_note_ensure_level_state();
    out->level_depth = (s16b)g_skeleton_note_state.level_depth;
    out->note_cap = (s16b)g_skeleton_note_state.note_cap;
    out->notes_shown = (s16b)g_skeleton_note_state.notes_shown;
    out->map_wid = (s16b)g_skeleton_note_state.map_wid;
    out->map_hgt = (s16b)g_skeleton_note_state.map_hgt;
    out->hint_used_mask = (byte)(g_skeleton_note_state.hint_used_mask & 0xFF);
    out->seen_count = g_skeleton_note_state.seen_count;
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
        out->seen_ids[i] = g_skeleton_note_state.seen_ids[i];
}

void skeleton_note_set_state(const skeleton_note_state_save* in)
{
    skeleton_note_reset_seen();
    if (!in)
    {
        skeleton_note_level_reset();
        return;
    }
    g_skeleton_note_state.level_depth = in->level_depth;
    g_skeleton_note_state.note_cap = (in->note_cap > 0) ? in->note_cap : 1;
    g_skeleton_note_state.notes_shown = (in->notes_shown >= 0)
        ? in->notes_shown
        : 0;
    g_skeleton_note_state.map_wid
        = (in->map_wid > 0) ? in->map_wid : p_ptr->cur_map_wid;
    g_skeleton_note_state.map_hgt
        = (in->map_hgt > 0) ? in->map_hgt : p_ptr->cur_map_hgt;
    g_skeleton_note_state.hint_used_mask = (u32b)in->hint_used_mask;
    g_skeleton_note_state.seen_count = MIN(in->seen_count, SKELETON_NOTE_SEEN_MAX);
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
        g_skeleton_note_state.seen_ids[i] = in->seen_ids[i];
    if (g_skeleton_note_state.notes_shown > g_skeleton_note_state.note_cap)
        g_skeleton_note_state.notes_shown = g_skeleton_note_state.note_cap;
}

static bool skeleton_hint_available(skeleton_hint_kind kind,
    const level_layout_info* layout, bool vault_present,
    bool vault_artifact, byte sval)
{
    if (kind == SKEL_HINT_TIP && disable_skeleton_note_tutorial)
        return false;

    bool ok = false;

    switch (kind)
    {
    case SKEL_HINT_GREAT_VAULT:
        ok = vault_present;
        break;
    case SKEL_HINT_VAULT_ARTIFACT:
        ok = vault_artifact;
        break;
    case SKEL_HINT_STAIRS:
        ok = level_has_stairs_down() || level_has_stairs_up();
        break;
    case SKEL_HINT_PARTITION_PRESENCE:
        ok = false;
        break;
    case SKEL_HINT_FORGE:
        ok = level_has_forge();
        break;
    case SKEL_HINT_UNIQUE_MONSTER:
    {
        for (int i = 1; i < mon_max; i++)
        {
            monster_type *m_ptr = &mon_list[i];
            if (!m_ptr->r_idx) continue;
            monster_race *r_ptr = &r_info[m_ptr->r_idx];
            if (r_ptr->flags1 & RF1_UNIQUE)
            {
                ok = true;
                break;
            }
        }
        break;
    }
    case SKEL_HINT_TIP:
        ok = (p_ptr->depth <= SKELETON_TIP_MAX_DEPTH)
            && (sval == SV_SKELETON_HUMAN || sval == SV_SKELETON_ELF
                || sval == SV_SKELETON_ORC);
        break;
    case SKEL_HINT_LEVEL_SIZE:
        ok = (layout != NULL);
        break;
    case SKEL_HINT_QUEST:
        ok = level_has_quest_giver() || level_has_quest_vault();
        break;
    case SKEL_HINT_PART_LABYRINTH:
        ok = layout && (layout->labyrinth_parts > 0);
        break;
    case SKEL_HINT_PART_CHASM:
        ok = layout && (layout->chasm_parts > 0);
        break;
    case SKEL_HINT_PART_CAVE:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (layout && none > 0);
        break;
    }
    case SKEL_HINT_PART_CAVE_ICE:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (counts[BIG_CAVE_ICE] > 0);
        break;
    }
    case SKEL_HINT_PART_CAVE_FIRE:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (counts[BIG_CAVE_FIRE] > 0);
        break;
    }
    case SKEL_HINT_PART_CAVE_POIS:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (counts[BIG_CAVE_POIS] > 0);
        break;
    }
    default:
        ok = false;
        break;
    }

    if (!ok)
        return false;

    /*
     * Tutorial tips should be repeatable: don't hide them just because we've
     * recently shown every TIP template.
     */
    if (kind != SKEL_HINT_TIP)
    {
        if (!skeleton_note_has_unseen_template(
                sval, SKELETON_NOTE_ROLE_HINT, kind))
            return false;
    }

    return true;
}

typedef struct
{
    level_partition_kind kind;
    big_cave_type_t big_cave_type;
} skeleton_partition_focus;

static skeleton_partition_focus skeleton_pick_partition_presence(
    const level_layout_info* layout)
{
    skeleton_partition_focus focus;
    focus.kind = LEVEL_PART_NONE;
    focus.big_cave_type = BIG_CAVE_NONE;

    if (!layout)
        return focus;

    int big_cave_counts[BIG_CAVE_TYPE_MAX] = {0};
    for (int pi = 0; pi < layout->partition_count; ++pi)
    {
        big_cave_type_t cave_type = level_partition_big_cave_type_for_index(pi);
        if (cave_type > BIG_CAVE_NONE && cave_type < BIG_CAVE_TYPE_MAX)
            big_cave_counts[cave_type]++;
    }

    int elemental = big_cave_counts[BIG_CAVE_ICE] + big_cave_counts[BIG_CAVE_FIRE]
        + big_cave_counts[BIG_CAVE_POIS];
    int big_none = layout->big_cave_parts - elemental;
    if (big_none < 0)
        big_none = 0;

    struct {
        level_partition_kind kind;
        big_cave_type_t big_cave_type;
        int weight;
    } options[8];
    int n_options = 0;

    if (layout->labyrinth_parts > 0)
    {
        options[n_options].kind = LEVEL_PART_LABYRINTH;
        options[n_options].big_cave_type = BIG_CAVE_NONE;
        options[n_options].weight = layout->labyrinth_parts;
        n_options++;
    }
    if (layout->chasm_parts > 0)
    {
        options[n_options].kind = LEVEL_PART_CHASM;
        options[n_options].big_cave_type = BIG_CAVE_NONE;
        options[n_options].weight = layout->chasm_parts;
        n_options++;
    }
    if (big_none > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_NONE;
        options[n_options].weight = big_none;
        n_options++;
    }
    if (big_cave_counts[BIG_CAVE_ICE] > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_ICE;
        options[n_options].weight = big_cave_counts[BIG_CAVE_ICE];
        n_options++;
    }
    if (big_cave_counts[BIG_CAVE_FIRE] > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_FIRE;
        options[n_options].weight = big_cave_counts[BIG_CAVE_FIRE];
        n_options++;
    }
    if (big_cave_counts[BIG_CAVE_POIS] > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_POIS;
        options[n_options].weight = big_cave_counts[BIG_CAVE_POIS];
        n_options++;
    }

    int total = 0;
    for (int i = 0; i < n_options; ++i)
        total += options[i].weight;

    if (total <= 0)
        return focus;

    int roll = rand_int(total);
    for (int i = 0; i < n_options; ++i)
    {
        if (roll < options[i].weight)
        {
            focus.kind = options[i].kind;
            focus.big_cave_type = options[i].big_cave_type;
            return focus;
        }
        roll -= options[i].weight;
    }

    return focus;
}

static skeleton_hint_kind skeleton_note_choose_hint(
    const skeleton_note_profile* profile, const level_layout_info* layout,
    bool vault_present, bool vault_artifact, byte sval, u32b used_mask)
{
    int weights[SKEL_HINT_MAX] = {0};
    int total = 0;

    for (int k = 1; k < SKEL_HINT_MAX; ++k)
    {
        skeleton_hint_kind kind = (skeleton_hint_kind)k;
        if (used_mask & (1UL << k))
            continue;

        if (!skeleton_hint_available(
                kind, layout, vault_present, vault_artifact, sval))
            continue;

        int base = skeleton_hint_base_weight[k];
        
        if (kind == SKEL_HINT_TIP)
        {
            int scaled = SKELETON_TIP_MAX_DEPTH + 1 - p_ptr->depth;
            if (scaled <= 0)
                base = 0;
            else
            {
                if (scaled > SKELETON_TIP_MAX_DEPTH)
                    scaled = SKELETON_TIP_MAX_DEPTH;
                base = (base * scaled) / SKELETON_TIP_MAX_DEPTH;
            }
        }

        int scale = profile->weight_scale[k];

        if (base <= 0 || scale <= 0)
            continue;

        int weight = (base * scale) / 100;
        if (weight < 1)
            weight = 1;

        weights[k] = weight;
        total += weight;
    }

    if (total <= 0)
        return SKEL_HINT_NONE;

    int roll = rand_int(total);
    for (int k = 1; k < SKEL_HINT_MAX; ++k)
    {
        if (weights[k] == 0)
            continue;

        if (roll < weights[k])
            return (skeleton_hint_kind)k;

        roll -= weights[k];
    }

    return SKEL_HINT_NONE;
}

static bool skeleton_note_has_unseen_template(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint)
{
    if (!skeleton_note_info || !z_info)
        return false;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role != role || t->weight == 0 || t->text == 0)
            continue;
        if (t->sval != SV_SKELETON_NOTE_ANY && t->sval != sval)
            continue;
        if (role == SKELETON_NOTE_ROLE_HINT && t->hint != hint)
            continue;
        if (!skeleton_note_seen_id((s16b)i))
            return true;
    }
    return false;
}

static s16b skeleton_note_pick_entry_internal(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint, bool allow_seen)
{
    if (!skeleton_note_info || !z_info)
        return -1;

    int total = 0;
    int max = z_info->skeleton_note_max;
    for (int i = 0; i < max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role != role || t->weight == 0 || t->text == 0)
            continue;
        if (t->sval != SV_SKELETON_NOTE_ANY && t->sval != sval)
            continue;
        if (role == SKELETON_NOTE_ROLE_HINT && t->hint != hint)
            continue;
        if (!allow_seen && skeleton_note_seen_id((s16b)i))
            continue;
        total += t->weight;
    }

    if (total <= 0)
        return -1;

    int roll = rand_int(total);
    for (int i = 0; i < max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role != role || t->weight == 0 || t->text == 0)
            continue;
        if (t->sval != SV_SKELETON_NOTE_ANY && t->sval != sval)
            continue;
        if (role == SKELETON_NOTE_ROLE_HINT && t->hint != hint)
            continue;
        if (!allow_seen && skeleton_note_seen_id((s16b)i))
            continue;
        if (roll < t->weight)
            return (s16b)i;
        roll -= t->weight;
    }

    return -1;
}

static s16b skeleton_note_pick_entry(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint)
{
    return skeleton_note_pick_entry_internal(sval, role, hint, false);
}

typedef struct skeleton_note_line
{
    const char* tpl;
    level_partition_kind presence_kind;
    big_cave_type_t big_cave_type;
    const char* unique_type;
    const char* dir;
    const char* dist;
    const char* site;
    const char* size_word;
} skeleton_note_line;

static void skeleton_note_normalize_spaces(char* s)
{
    if (!s)
        return;

    /* Trim leading spaces */
    char* p = s;
    while (*p == ' ')
        ++p;
    if (p != s)
        memmove(s, p, strlen(p) + 1);

    /* Collapse multiple spaces */
    char* r = s;
    char* w = s;
    bool prev_space = false;
    while (*r)
    {
        if (*r == ' ')
        {
            if (!prev_space)
                *w++ = ' ';
            prev_space = true;
        }
        else
        {
            *w++ = *r;
            prev_space = false;
        }
        ++r;
    }
    *w = '\0';

    /* Trim trailing spaces */
    size_t len = strlen(s);
    while (len > 0 && s[len - 1] == ' ')
        s[--len] = '\0';
}

static int skeleton_note_append_wrapped_segment_mono(
    const char* seg, char lines[][100], int idx, int limit, int wrap)
{
    if (!seg || !seg[0] || limit <= idx)
        return idx;

    if (wrap < 10)
        wrap = 10;
    if (wrap > 95)
        wrap = 95;

    int len = (int)strlen(seg);
    int pos = 0;
    while (pos < len && idx < limit)
    {
        while (pos < len && seg[pos] == ' ')
            pos++;
        if (pos >= len)
            break;

        int remaining = len - pos;
        int take = (remaining <= wrap) ? remaining : wrap;

        if (remaining > wrap)
        {
            int end = pos + take;
            int split = -1;
            for (int j = end - 1; j > pos; --j)
            {
                if (seg[j] == ' ')
                {
                    split = j;
                    break;
                }
            }
            if (split > pos)
                take = split - pos;
        }

        while (take > 0 && seg[pos + take - 1] == ' ')
            take--;

        if (take <= 0)
            break;

        strnfmt(lines[idx++], 100, "%.*s", take, seg + pos);
        pos += take;
    }

    return idx;
}

static int skeleton_note_max_chars_fit_pixels(const char* text, int max_chars, int max_px, int cell_width)
{
    if (!text || max_chars <= 0)
        return 0;

    if (max_px <= 0 || cell_width <= 0)
        return max_chars;

    int lo = 1;
    int hi = max_chars;
    int best = 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        int w = sdl_story_font_text_width(text, mid);
        if (w <= 0)
            w = mid * cell_width;

        if (w <= max_px)
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    return best;
}

static void skeleton_note_pad_line_for_story(char line[100], int wrap_cols, int cell_width)
{
    if (!line || wrap_cols <= 0 || cell_width <= 0)
        return;

    int len = (int)strlen(line);
    if (len <= 0)
        return;

    if (wrap_cols > 99)
        wrap_cols = 99;

    int px = sdl_story_font_text_width(line, len);
    if (px <= 0)
        px = len * cell_width;

    int cells_needed = (px + cell_width - 1) / cell_width;
    if (cells_needed < len)
        cells_needed = len;

    /* Safety margin for measurement/render mismatches. */
    if (cells_needed < wrap_cols)
        cells_needed++;

    if (cells_needed > wrap_cols)
        cells_needed = wrap_cols;

    while (len < cells_needed && len < 99)
        line[len++] = ' ';
    line[len] = '\0';
}

static int skeleton_note_append_wrapped_segment_story(
    const char* seg, char lines[][100], int idx, int limit, int wrap_cols)
{
    if (!seg || !seg[0] || limit <= idx)
        return idx;

    if (wrap_cols < 10)
        wrap_cols = 10;
    if (wrap_cols > 95)
        wrap_cols = 95;

    int cell_width = sdl_get_cell_width();
    if (cell_width <= 0)
        return skeleton_note_append_wrapped_segment_mono(seg, lines, idx, limit, wrap_cols);

    int wrap_px = wrap_cols * cell_width;
    int space_px = sdl_story_font_text_width(" ", 1);
    if (space_px <= 0)
        space_px = cell_width;

    int max_line_chars = wrap_cols;
    if (max_line_chars > 99)
        max_line_chars = 99;

    const char* s = seg;
    while (*s && idx < limit)
    {
        while (*s == ' ')
            s++;
        if (!*s)
            break;

        char out[100];
        int out_len = 0;
        int line_px = 0;
        bool first_word = true;

        while (*s)
        {
            while (*s == ' ')
                s++;
            if (!*s)
                break;

            const char* word = s;
            int word_len = 0;
            while (word[word_len] && word[word_len] != ' ')
                word_len++;

            int word_px = sdl_story_font_text_width(word, word_len);
            if (word_px <= 0)
                word_px = word_len * cell_width;

            int add_px = word_px + (first_word ? 0 : space_px);
            int add_chars = word_len + (first_word ? 0 : 1);

            if (!first_word && ((line_px + add_px) > wrap_px || (out_len + add_chars) > max_line_chars))
                break;

            if (first_word && (word_px > wrap_px || word_len > max_line_chars))
            {
                int remaining_chars = max_line_chars - out_len;
                int max_chars = word_len;
                if (max_chars > remaining_chars)
                    max_chars = remaining_chars;
                int fit = skeleton_note_max_chars_fit_pixels(word, max_chars, wrap_px, cell_width);
                if (fit <= 0)
                    fit = 1;
                memcpy(out + out_len, word, fit);
                out_len += fit;
                out[out_len] = '\0';
                s += fit;
                break;
            }

            if (!first_word)
            {
                out[out_len++] = ' ';
                line_px += space_px;
            }

            int copy = word_len;
            if (copy > 99 - out_len)
                copy = 99 - out_len;
            if (copy > max_line_chars - out_len)
                copy = max_line_chars - out_len;
            memcpy(out + out_len, word, copy);
            out_len += copy;
            out[out_len] = '\0';
            line_px += word_px;

            s += word_len;
            first_word = false;
        }

        if (out_len > 0)
        {
            strnfmt(lines[idx], 100, "%s", out);
            skeleton_note_pad_line_for_story(lines[idx], wrap_cols, cell_width);
            idx++;
        }

        while (*s == ' ')
            s++;
    }

    return idx;
}

static int skeleton_note_append_wrapped_segment(
    const char* seg, char lines[][100], int idx, int limit, int wrap_cols)
{
    if (!seg || !seg[0] || limit <= idx)
        return idx;

    if (sdl_story_font_text_width(" ", 1) > 0 && sdl_get_cell_width() > 0)
        return skeleton_note_append_wrapped_segment_story(seg, lines, idx, limit, wrap_cols);

    return skeleton_note_append_wrapped_segment_mono(seg, lines, idx, limit, wrap_cols);
}

static void skeleton_note_expand_template(const char* tpl,
    const level_layout_info* layout, level_partition_kind presence_kind,
    big_cave_type_t big_cave_type, const char* unique_type, const char* dir,
    const char* dist, const char* site, const char* size_word, char* out,
    size_t out_sz)
{
    const char* part = partition_label(presence_kind, big_cave_type);
    const char* part_hazard = partition_hazard_label(presence_kind, big_cave_type);
    const char* size_word_text = size_word
        ? size_word
        : size_word_for_bucket(layout ? skeleton_note_size_bucket(layout) : 0);
    int width = layout ? layout->map_wid : 0;
    int height = layout ? layout->map_hgt : 0;
    const char* dir_text = dir ? dir : "";
    const char* dist_text = dist ? dist : "";
    const char* site_text = site ? site : "";

    size_t w = 0;
    const char* p = tpl ? tpl : "";
    while (*p && w + 1 < out_sz)
    {
        if (*p == '{')
        {
            if (strncmp(p, "{PART}", 6) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", part);
                p += 6;
                continue;
            }
            if (strncmp(p, "{PART_HAZARD}", 13) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", part_hazard);
                p += 13;
                continue;
            }
            if (strncmp(p, "{SIZEWORD}", 10) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", size_word_text);
                p += 10;
                continue;
            }
            if (strncmp(p, "{WIDTH}", 7) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%d", width);
                p += 7;
                continue;
            }
            if (strncmp(p, "{HEIGHT}", 8) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%d", height);
                p += 8;
                continue;
            }
            if (strncmp(p, "{UNIQUE_TYPE}", 13) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", unique_type ? unique_type : "creature");
                p += 13;
                continue;
            }
            if (strncmp(p, "{DIR}", 5) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", dir_text);
                p += 5;
                continue;
            }
            if (strncmp(p, "{DIST}", 6) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", dist_text);
                p += 6;
                continue;
            }
            if (strncmp(p, "{SITE}", 6) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", site_text);
                p += 6;
                continue;
            }
        }
        out[w++] = *p++;
    }
    out[w] = '\0';
}

static int skeleton_note_append_expanded_lines(const skeleton_note_line* line,
    const level_layout_info* layout, char lines[][100], int idx, int limit, int wrap)
{
    if (!line || !line->tpl || limit <= idx)
        return idx;

    char expanded[512];
    skeleton_note_expand_template(line->tpl, layout, line->presence_kind,
        line->big_cave_type, line->unique_type, line->dir, line->dist,
        line->site, line->size_word, expanded, sizeof(expanded));

    char* seg = expanded;
    while (seg && *seg && idx < limit)
    {
        char* next = strchr(seg, '|');
        if (next)
        {
            *next = '\0';
            next++;
        }

        skeleton_note_normalize_spaces(seg);
        if (seg[0])
            idx = skeleton_note_append_wrapped_segment(seg, lines, idx, limit, wrap);

        seg = next;
    }

    return idx;
}

static int skeleton_note_effective_wrap_width(int col)
{
    int wrap = 70;

    if (Term && Term->wid > 0)
    {
        int avail = Term->wid - col - 1;
        if (avail < wrap)
            wrap = avail;
    }

    if (wrap < 10)
        wrap = 10;
    if (wrap > 95)
        wrap = 95;

    return wrap;
}

static int skeleton_note_append_wrapped_text(
    const char* text, char lines[][100], int idx, int limit, int wrap)
{
    if (!text || !text[0] || limit <= idx)
        return idx;

    char expanded[512];
    strnfmt(expanded, sizeof(expanded), "%s", text);

    char* seg = expanded;
    while (seg && *seg && idx < limit)
    {
        char* next = strchr(seg, '|');
        if (next)
        {
            *next = '\0';
            next++;
        }

        skeleton_note_normalize_spaces(seg);
        if (seg[0])
            idx = skeleton_note_append_wrapped_segment(seg, lines, idx, limit, wrap);

        seg = next;
    }

    return idx;
}

static void skeleton_note_build_lines(const char* opening,
    const skeleton_note_line* body_lines, int body_count, const char* closing,
    const level_layout_info* layout, char lines[][100], int col)
{
    const int max_lines = 12; /* Reserve final slot for terminator */
    int wrap = skeleton_note_effective_wrap_width(col);

    int idx = 0;
    idx = skeleton_note_append_wrapped_text(opening, lines, idx, max_lines, wrap);

    for (int i = 0; i < body_count && idx < max_lines; ++i)
        idx = skeleton_note_append_expanded_lines(&body_lines[i], layout, lines, idx, max_lines, wrap);

    idx = skeleton_note_append_wrapped_text(closing, lines, idx, max_lines, wrap);

    lines[idx][0] = '\0';
}

static const char* skeleton_get_unique_type_name(const monster_race* r_ptr)
{
    if (!r_ptr) return "creature";
    
    if (r_ptr->flags3 & RF3_DRAGON) return "dragon";
    if (r_ptr->flags3 & RF3_RAUKO) return "demon";
    if (r_ptr->flags3 & RF3_UNDEAD) return "spirit";
    if (r_ptr->flags3 & RF3_ORC) return "orc";
    if (r_ptr->flags3 & RF3_TROLL) return "troll";
    if (r_ptr->flags3 & RF3_SPIDER) return "spider";
    if (r_ptr->flags3 & RF3_WOLF) return "wolf";
    if (r_ptr->d_char == 'C') return "hound";
    if (r_ptr->flags3 & RF3_MAN) return "human";
    if (r_ptr->flags3 & RF3_ELF) return "elf";

    return "horror";
}

static int skeleton_note_manhattan_dist(int y1, int x1, int y2, int x2)
{
    int dy = y1 - y2;
    if (dy < 0)
        dy = -dy;

    int dx = x1 - x2;
    if (dx < 0)
        dx = -dx;

    return dy + dx;
}

static const char* skeleton_note_direction_phrase(int from_y, int from_x, int to_y, int to_x)
{
    int dy = to_y - from_y;
    int dx = to_x - from_x;

    int sy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    int sx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);

    if (sy == 0 && sx == 0)
        return "here";
    if (sy < 0 && sx == 0)
        return "to the north";
    if (sy < 0 && sx > 0)
        return "to the north-east";
    if (sy == 0 && sx > 0)
        return "to the east";
    if (sy > 0 && sx > 0)
        return "to the south-east";
    if (sy > 0 && sx == 0)
        return "to the south";
    if (sy > 0 && sx < 0)
        return "to the south-west";
    if (sy == 0 && sx < 0)
        return "to the west";
    return "to the north-west";
}

static const char* skeleton_note_distance_phrase(int dist)
{
    if (dist <= 5)
        return "very near";
    if (dist <= 12)
        return "not far";
    if (dist <= 22)
        return "some way";
    return "far off";
}

static bool skeleton_note_find_nearest_stairs_kind(
    bool want_down, int from_y, int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_feat = 0;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            bool ok = want_down ? (feat == FEAT_MORE || feat == FEAT_MORE_SHAFT)
                                : (feat == FEAT_LESS || feat == FEAT_LESS_SHAFT);
            if (!ok)
                continue;

            int dist = skeleton_note_manhattan_dist(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_feat = feat;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_feat = feat;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_feat) *out_feat = best_feat;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static bool skeleton_note_find_nearest_stairs(
    byte sval, int from_y, int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int prefer_down = 0;
    switch (sval)
    {
    case SV_SKELETON_ORC:
        prefer_down = 80;
        break;
    case SV_SKELETON_ELF:
        prefer_down = 65;
        break;
    default:
        prefer_down = 55;
        break;
    }

    bool want_down = percent_chance(prefer_down);
    if (want_down)
    {
        if (skeleton_note_find_nearest_stairs_kind(
                true, from_y, from_x, out_y, out_x, out_feat, out_dist))
            return true;
        return skeleton_note_find_nearest_stairs_kind(
            false, from_y, from_x, out_y, out_x, out_feat, out_dist);
    }

    if (skeleton_note_find_nearest_stairs_kind(
            false, from_y, from_x, out_y, out_x, out_feat, out_dist))
        return true;
    return skeleton_note_find_nearest_stairs_kind(
        true, from_y, from_x, out_y, out_x, out_feat, out_dist);
}

static bool skeleton_note_find_nearest_forge(
    int from_y, int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_feat = 0;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (!cave_forge_bold(y, x))
                continue;

            int feat = cave_feat[y][x];
            int dist = skeleton_note_manhattan_dist(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_feat = feat;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_feat = feat;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_feat) *out_feat = best_feat;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static bool skeleton_note_find_nearest_quest_site(
    int from_y, int from_x, int* out_y, int* out_x, int* out_dist, const char** out_site)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    const char* best_site = NULL;
    int seen = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;

        int r_idx = m_ptr->r_idx;
        if (!skeleton_note_is_quest_giver_r_idx(r_idx) && r_idx != R_IDX_DURUIN)
            continue;

        int dist = skeleton_note_manhattan_dist(from_y, from_x, m_ptr->fy, m_ptr->fx);
        if (best_y < 0 || dist < best_dist)
        {
            best_y = m_ptr->fy;
            best_x = m_ptr->fx;
            best_dist = dist;
            best_site = skeleton_note_quest_site_name(r_idx);
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                best_y = m_ptr->fy;
                best_x = m_ptr->fx;
                best_site = skeleton_note_quest_site_name(r_idx);
            }
        }
    }

    if (p_ptr->aule_level == p_ptr->depth && p_ptr->aule_quest != AULE_QUEST_NOT_STARTED)
    {
        int y = p_ptr->aule_forge_y;
        int x = p_ptr->aule_forge_x;
        if (in_bounds(y, x) && cave_forge_bold(y, x))
        {
            int dist = skeleton_note_manhattan_dist(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                best_site = "a forge of strange craft";
                seen = 1;
            }
            else if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_site = "a forge of strange craft";
                }
            }
        }
    }

    if (p_ptr->mandos_level == p_ptr->depth && p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED)
    {
        int y = p_ptr->mandos_vault_y;
        int x = p_ptr->mandos_vault_x;
        if (in_bounds(y, x))
        {
            int dist = skeleton_note_manhattan_dist(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                best_site = "a hall of doom";
                seen = 1;
            }
            else if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_site = "a hall of doom";
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    if (out_site) *out_site = best_site ? best_site : "a Power";
    return true;
}

static const char* skeleton_note_stair_site(int feat)
{
    switch (feat)
    {
    case FEAT_MORE:
        return "stair down";
    case FEAT_MORE_SHAFT:
        return "shaft down";
    case FEAT_LESS:
        return "stair up";
    case FEAT_LESS_SHAFT:
        return "shaft up";
    default:
        return "stairs";
    }
}

static void skeleton_note_partition_meta_for_hint(
    skeleton_hint_kind hint, level_partition_kind* out_kind, big_cave_type_t* out_type)
{
    if (out_kind)
        *out_kind = LEVEL_PART_NONE;
    if (out_type)
        *out_type = BIG_CAVE_NONE;

    switch (hint)
    {
    case SKEL_HINT_PART_LABYRINTH:
        if (out_kind) *out_kind = LEVEL_PART_LABYRINTH;
        break;
    case SKEL_HINT_PART_CHASM:
        if (out_kind) *out_kind = LEVEL_PART_CHASM;
        break;
    case SKEL_HINT_PART_CAVE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        break;
    case SKEL_HINT_PART_CAVE_ICE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_ICE;
        break;
    case SKEL_HINT_PART_CAVE_FIRE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_FIRE;
        break;
    case SKEL_HINT_PART_CAVE_POIS:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_POIS;
        break;
    default:
        break;
    }
}

static const char* skeleton_note_forge_site(int feat, char* buf, size_t buf_sz)
{
    if (feat >= FEAT_FORGE_UNIQUE_HEAD && feat <= FEAT_FORGE_UNIQUE_TAIL)
    {
        const char* raw = f_name + f_info[feat].name;
        size_t w = 0;
        while (raw[w] && w + 1 < buf_sz)
        {
            if (raw[w] == ' ' && raw[w + 1] == '(')
                break;
            buf[w] = raw[w];
            w++;
        }
        buf[w] = '\0';
        if (buf[0])
            return buf;
    }

    if (feat >= FEAT_FORGE_GOOD_HEAD && feat <= FEAT_FORGE_GOOD_TAIL)
        return "enchanted forge";

    return "forge";
}

static void skeleton_note_maybe_show(byte sval, int skel_y, int skel_x)
{
    if (skeleton_note_entry_count() == 0)
        return;

    skeleton_note_ensure_level_state();

    if (g_skeleton_note_state.note_cap <= 0)
        return;

    bool at_cap = (g_skeleton_note_state.notes_shown >= g_skeleton_note_state.note_cap);

    skeleton_note_profile profile = skeleton_note_profile_for_sval(sval);
    if (profile.note_chance <= 0)
        return;

    if (!percent_chance(profile.note_chance))
        return;

    level_layout_info layout;
    level_layout_info_current(&layout);

    bool vault_present = level_has_greater_vault();
    bool artifact_in_vault = vault_present && vault_has_ground_artifact();

    u32b base_used_mask = g_skeleton_note_state.hint_used_mask;

    skeleton_hint_kind hint1 = SKEL_HINT_NONE;
    if (at_cap)
    {
        /* Tutorial notes should not be limited by the per-level cap. */
        if (!skeleton_hint_available(
                SKEL_HINT_TIP, &layout, vault_present, artifact_in_vault, sval))
        {
            return;
        }
        hint1 = SKEL_HINT_TIP;
    }
    else
    {
        bool can_tip = skeleton_hint_available(
            SKEL_HINT_TIP, &layout, vault_present, artifact_in_vault, sval);
        int tip_chance = skeleton_note_tip_override_chance(sval, p_ptr->depth);
        if (can_tip && tip_chance > 0 && percent_chance(tip_chance))
        {
            hint1 = SKEL_HINT_TIP;
        }
        else
        {
            hint1 = skeleton_note_choose_hint(
                &profile, &layout, vault_present, artifact_in_vault, sval, base_used_mask);
        }
    }
    if (hint1 == SKEL_HINT_NONE)
        return;

    skeleton_hint_kind hint2 = SKEL_HINT_NONE;
    if (hint1 != SKEL_HINT_TIP)
    {
        int size_bucket = skeleton_note_size_bucket(&layout);
        int second_chance = 0;

        switch (sval)
        {
        case SV_SKELETON_ELF:
            second_chance = 50 + 10 * size_bucket;
            break;
        case SV_SKELETON_ORC:
            second_chance = 30 + 5 * size_bucket;
            break;
        default:
            second_chance = 45 + 10 * size_bucket;
            break;
        }

        if (second_chance > 0 && percent_chance(second_chance))
        {
            u32b used_mask2 = base_used_mask;
            used_mask2 |= (1UL << hint1);

            hint2 = skeleton_note_choose_hint(
                &profile, &layout, vault_present, artifact_in_vault, sval, used_mask2);
        }
    }

    /* Don't mark TIP hints as used - they can repeat. */
    if (hint1 != SKEL_HINT_TIP)
        g_skeleton_note_state.hint_used_mask |= (1UL << hint1);
    if (hint2 != SKEL_HINT_NONE && hint2 != SKEL_HINT_TIP)
        g_skeleton_note_state.hint_used_mask |= (1UL << hint2);

    const char* unique_type = NULL;
    if (hint1 == SKEL_HINT_UNIQUE_MONSTER || hint2 == SKEL_HINT_UNIQUE_MONSTER)
    {
        int candidates[100];
        int n_candidates = 0;
        for (int i = 1; i < mon_max; i++)
        {
            monster_type* m_ptr = &mon_list[i];
            if (!m_ptr->r_idx)
                continue;
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            if (r_ptr->flags1 & RF1_UNIQUE)
            {
                if (n_candidates < 100)
                    candidates[n_candidates++] = m_ptr->r_idx;
            }
        }
        if (n_candidates > 0)
        {
            int r_idx = candidates[rand_int(n_candidates)];
            unique_type = skeleton_get_unique_type_name(&r_info[r_idx]);
        }
    }

    skeleton_partition_focus focus_part;
    focus_part.kind = LEVEL_PART_NONE;
    focus_part.big_cave_type = BIG_CAVE_NONE;
    if (hint1 == SKEL_HINT_PARTITION_PRESENCE
        || hint2 == SKEL_HINT_PARTITION_PRESENCE)
    {
        focus_part = skeleton_pick_partition_presence(&layout);
    }

    s16b opening_id = skeleton_note_pick_entry(
        sval, SKELETON_NOTE_ROLE_OPENING, SKEL_HINT_NONE);
    s16b signoff_id = skeleton_note_pick_entry(
        sval, SKELETON_NOTE_ROLE_SIGNOFF, SKEL_HINT_NONE);

    const char* opening = opening_id >= 0
        ? (skeleton_note_text + skeleton_note_info[opening_id].text)
        : skeleton_note_fallback_opening(sval);
    const char* signoff = signoff_id >= 0
        ? (skeleton_note_text + skeleton_note_info[signoff_id].text)
        : skeleton_note_fallback_signoff(sval);

    skeleton_note_line body_lines[2];
    s16b body_ids[2] = {-1, -1};
    int body_count = 0;

    skeleton_hint_kind hints[2] = {hint1, hint2};
    int hint_count = (hint2 != SKEL_HINT_NONE) ? 2 : 1;

    char forge_site_buf[64];
    forge_site_buf[0] = '\0';

    for (int i = 0; i < hint_count; ++i)
    {
        skeleton_hint_kind hint = hints[i];
        s16b note_id = skeleton_note_pick_entry(
            sval, SKELETON_NOTE_ROLE_HINT, hint);
        if (hint == SKEL_HINT_TIP && note_id < 0)
        {
            note_id = skeleton_note_pick_entry_internal(
                sval, SKELETON_NOTE_ROLE_HINT, hint, true);
        }

        const char* tpl = (note_id >= 0)
            ? (skeleton_note_text + skeleton_note_info[note_id].text)
            : NULL;

        if (!tpl)
        {
            switch (hint)
            {
            case SKEL_HINT_GREAT_VAULT:
                tpl = "A gate of black stone stands somewhere on this level; the warding is unbroken.";
                break;
            case SKEL_HINT_VAULT_ARTIFACT:
                tpl = "I saw a pale light behind sealed doors; some great work of craft lies within.";
                break;
            case SKEL_HINT_STAIRS:
                tpl = "The {SITE} lies {DIST} {DIR}.";
                break;
            case SKEL_HINT_PARTITION_PRESENCE:
                tpl = "Beware {PART}; {PART_HAZARD}.";
                break;
            case SKEL_HINT_PART_LABYRINTH:
                tpl = "A maze of hewn stone lies here; its turns will unmake your bearings.";
                break;
            case SKEL_HINT_PART_CHASM:
                tpl = "There is a chasm on this level. The dark below drinks both light and courage.";
                break;
            case SKEL_HINT_PART_CAVE:
                tpl = "A great cavern opens on this floor. Sound carries far, and there is little cover.";
                break;
            case SKEL_HINT_PART_CAVE_ICE:
                tpl = "A great ice cavern lies here. The cold bites, and the floor is slick.";
                break;
            case SKEL_HINT_PART_CAVE_FIRE:
                tpl = "A cavern of fire lies here. The air burns and smoke chokes.";
                break;
            case SKEL_HINT_PART_CAVE_POIS:
                tpl = "A cavern of poisonous vapours lies here. The reek clings low and stings the lungs.";
                break;
            case SKEL_HINT_FORGE:
                tpl = "Smoke and hammer-sound: the {SITE} lies {DIST} {DIR}.";
                break;
            case SKEL_HINT_UNIQUE_MONSTER:
                tpl = "A {UNIQUE_TYPE} walks these halls. Hide or flee.";
                break;
            case SKEL_HINT_TIP:
                tpl = "In Angband, silence is life. Shut doors, walk softly, and do not let them hear you.";
                break;
            case SKEL_HINT_LEVEL_SIZE:
                tpl = "This place is {SIZEWORD}; do not expect a short road to anywhere.";
                break;
            case SKEL_HINT_QUEST:
                tpl = "A power stirs in these deeps; I saw {SITE} {DIST} {DIR}.";
                break;
            default:
                tpl = "Bones clutch a faded scrap of text.";
                break;
            }
        }

        body_lines[body_count].tpl = tpl;
        if (hint == SKEL_HINT_PARTITION_PRESENCE)
        {
            body_lines[body_count].presence_kind = focus_part.kind;
            body_lines[body_count].big_cave_type = focus_part.big_cave_type;
        }
        else
        {
            skeleton_note_partition_meta_for_hint(
                hint, &body_lines[body_count].presence_kind,
                &body_lines[body_count].big_cave_type);
        }
        body_lines[body_count].unique_type
            = (hint == SKEL_HINT_UNIQUE_MONSTER) ? unique_type : NULL;
        body_lines[body_count].dir = NULL;
        body_lines[body_count].dist = NULL;
        body_lines[body_count].site = NULL;
        body_lines[body_count].size_word = NULL;

        if (hint == SKEL_HINT_STAIRS)
        {
            int ty = 0, tx = 0, feat = 0, dist = 0;
            if (skeleton_note_find_nearest_stairs(
                    sval, skel_y, skel_x, &ty, &tx, &feat, &dist))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(dist);
                body_lines[body_count].site = skeleton_note_stair_site(feat);
            }
        }
        else if (hint == SKEL_HINT_FORGE)
        {
            int ty = 0, tx = 0, feat = 0, dist = 0;
            if (skeleton_note_find_nearest_forge(
                    skel_y, skel_x, &ty, &tx, &feat, &dist))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(dist);
                body_lines[body_count].site
                    = skeleton_note_forge_site(feat, forge_site_buf, sizeof(forge_site_buf));
            }
        }
        else if (hint == SKEL_HINT_LEVEL_SIZE)
        {
            body_lines[body_count].size_word = skeleton_note_pick_size_word(&layout);
        }
        else if (hint == SKEL_HINT_QUEST)
        {
            int ty = 0, tx = 0, dist = 0;
            const char* site = NULL;
            if (skeleton_note_find_nearest_quest_site(skel_y, skel_x, &ty, &tx, &dist, &site))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(dist);
                body_lines[body_count].site = site;
            }
            else
            {
                body_lines[body_count].dist = "somewhere";
                body_lines[body_count].dir = "on this level";
                body_lines[body_count].site = "a warded place";
            }
        }

        body_ids[body_count] = note_id;
        body_count++;
    }

    char note_lines[16][100];
    skeleton_note_build_lines(
        opening, body_lines, body_count, signoff, &layout, note_lines, 8);
    pause_with_text(note_lines, 4, 8, NULL, 0);
    if (hint1 != SKEL_HINT_TIP)
        g_skeleton_note_state.notes_shown++;
    skeleton_note_record_seen(opening_id);
    for (int i = 0; i < body_count; ++i)
        skeleton_note_record_seen(body_ids[i]);
    skeleton_note_record_seen(signoff_id);
}

/*
 * Attempt to search the given skeleton at the given location
 *
 * Assumes there is no monster blocking the destination
 */
static void do_cmd_search_skeleton(int y, int x, s16b o_idx)
{
    bool search_failed = true;
    int drop_result = 0;

    object_generation_mode = OB_GEN_MODE_SKELETON;
    object_type* o_ptr = &o_list[o_idx];

    // Searched already
    if (o_ptr->pval == 0)
    {
        return;
    }

    skeleton_note_maybe_show(o_ptr->sval, y, x);

    object_type* i_ptr;
    object_type object_type_body;
    i_ptr = &object_type_body;

    switch (o_ptr->sval)
    {
    case SV_SKELETON_ELF:
        drop_result = dieroll(10);
        break;
    case SV_SKELETON_HUMAN:
        drop_result = dieroll(10) + 5;
        break;
    case SV_SKELETON_ORC:
        drop_result = 10;
        break;
    }

    switch (drop_result)
    {
    case 1:
    case 2:
    case 3:
    case 4:
        object_prep(i_ptr, lookup_kind(TV_LIGHT, SV_LIGHT_MALLORN));
        i_ptr->timeout = rand_range(20, 50);
        search_failed = false;
        break;
    case 5:
        search_failed = !make_object(
            i_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_BOW);
        break;
    case 6:
        search_failed = !make_object(
            i_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_CLOAK);
        break;
    case 7:
        search_failed = !make_object(
            i_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_BOOTS);
        break;
    case 8:
        search_failed = !make_object(
            i_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_WEAPON);
        break;
    case 9:
        search_failed = !make_object(
            i_ptr, DROP_QUALITY_NORMAL, DROP_TYPE_GLOVES);
        break;
    default:
        search_failed = generate_poor_quality_object(i_ptr);
    }

    o_ptr->pval = 0;

    object_generation_mode = OB_GEN_MODE_NORMAL;

    if (search_failed)
    {
        msg_print("You failed to find anything among the bones.");
    }
    else
    {
        if (i_ptr->k_idx)
        {
            msg_print("You find something among the bones!");

            if (i_ptr->tval != TV_ARROW)
            {
                i_ptr->number = 1;
            }
            else
            {
                i_ptr->number = dieroll(4) + 2;
                msg_format("You gather up %d arrows.", i_ptr->number);
            }

            int slot;
            char o_name[80];

            /* Carry the object */
            slot = inven_carry(i_ptr, true);

            if (slot == SUPPLIES_INDEX)
            {
                object_desc(o_name, sizeof(o_name), i_ptr, true, 3);
                char label = supplies_label_char();
                if (!label)
                    label = 'a';
                msg_format("You add %s to your supplies (%c).", o_name, label);
            }
            else if (slot >= 0)
            {
                /* Get the object again */
                i_ptr = &inventory[slot];

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), i_ptr, true, 3);

                /* Message */
                msg_format("You have %s (%c).", o_name, index_to_label(slot));
            }

            // Break the truce if creatures see
            break_truce(false);
        }
    }
}

/*
 * Attempt to open the given chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_open_chest(int y, int x, s16b o_idx)
{
    int score, power, difficulty;

    bool flag = true;

    bool more = false;

    object_type* o_ptr = &o_list[o_idx];

    /* Attempt to unlock it */
    if (o_ptr->pval > 0)
    {
        /* Assume locked, and thus not open */
        flag = false;

        /* Get the score in favour (=perception) */
        score = p_ptr->skill_use[S_PER];

        /* Determine trap power based on the chest pval (power is 1--7)*/
        power = 1 + (o_ptr->pval / 4);

        // Base difficulty is the lock power + 5
        difficulty = power + 5;

        /* Penalize some conditions */
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5;
        if (p_ptr->confused)
            difficulty += 5;

        /* Success -- May still have traps */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
        {
            msg_print("You have picked the lock.");
            flag = true;
        }

        /* Failure -- Keep trying */
        else
        {
            /* We may continue repeating */
            more = true;
            flush();
            message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");
        }
    }

    /* Allowed to open */
    if (flag)
    {
        /* Apply chest traps, if any */
        chest_trap(y, x, o_idx);

        /* Let the Chest drop items */
        chest_death(y, x, o_idx);

        /*squelch chest if autosquelch calls for it*/
        if ((squelch_level[CHEST_INDEX]) == SQUELCH_OPENED_CHESTS)
        {
            delete_object_idx(o_idx);
            msg_print("Chest squelched after it was opened.");
        }
    }

    /* Result */
    return (more);
}

/*
 * Attempt to disarm the chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_disarm_chest(int y, int x, s16b o_idx)
{
    int score, power, difficulty, result;

    bool more = false;

    object_type* o_ptr = &o_list[o_idx];

    /* Get the score in favour (=perception) */
    score = p_ptr->skill_use[S_PER];

    /* Determine trap power based on the trap pval (power is 1--7)*/
    power = 1 + (o_ptr->pval / 4);

    // Base difficulty is the lock power
    difficulty = power;

    /* Penalize some conditions */
    if (p_ptr->blind || no_light() || p_ptr->image)
        difficulty += 5;
    if (p_ptr->confused)
        difficulty += 5;

    // perform the check
    result = skill_check(PLAYER, score, difficulty, NULL);

    /* Must find the trap first. */
    if (!object_known_p(o_ptr))
    {
        msg_print("You don't see any traps.");
    }

    /* Already disarmed/unlocked */
    else if (o_ptr->pval <= 0)
    {
        msg_print("The chest is not trapped.");
    }

    /* No traps to find. */
    else if (!chest_traps[o_ptr->pval])
    {
        msg_print("The chest is not trapped.");
    }

    /* Success (get a lot of experience) */
    else if (result > 0)
    {
        msg_print("You have disarmed the chest.");
        o_ptr->pval = (0 - o_ptr->pval);
    }

    /* Failure -- Keep trying */
    else if (result > -3)
    {
        /* We may keep trying */
        more = true;
        flush();
        msg_print("You failed to disarm the chest.");
    }

    /* Failure -- Set off the trap */
    else
    {
        msg_print("You set off a trap!");
        chest_trap(y, x, o_idx);
    }

    /* Result */
    return (more);
}

/*
 * Return true if the given feature is an open door
 */
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
                || !chest_traps[o_ptr->pval]))
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
    int y, x, dir;

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
void do_cmd_close(void)
{
    int y, x, dir;

    bool more = false;

    /* Count open doors */
    if (count_feats(&y, &x, is_open, false) == 1)
    {
        p_ptr->command_dir = coords_to_dir(y, x);
    }

    else if (count_feats(&y, &x, is_open, false) == 0)
    {
        msg_print("There is no adjacent door to close.");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

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
void do_cmd_exchange(void)
{
    int y, x, dir;

    monster_type* m_ptr;
    monster_race* r_ptr;
    char m_name[80];

    if (!p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
    {
        msg_print(
            "You need the ability 'exchange places' to use this command.");
        return;
    }

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    // deal with overburdened characters
    if (p_ptr->total_weight > weight_limit() * 3 / 2)
    {
        /* Abort */
        msg_print("You are too burdened to move.");

        return;
    }

    // Can't exchange from within pits
    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        /* Message */
        msg_print(
            "You would have to escape the pit before being able to exchange "
            "places.");

        return;
    }
    // Can't exchange from within webs
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        /* Message */
        msg_print(
            "You would have to escape the web before being able to exchange "
            "places.");

        return;
    }
    else if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }
    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the wall.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the closed door.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("You cannot enter the rubble.");

        return;
    }
    else
    {
        m_ptr = &mon_list[cave_m_idx[y][x]];
        r_ptr = &r_info[m_ptr->r_idx];

        if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
            || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Message */
            msg_format("You cannot get past %s.", m_name);

            return;
        }
    }

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

    // re-check for a visible monster (in case confusion changed the move)
    if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }

    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("There is a wall in the way.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("There is a door in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("There is a pile of rubble in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_CHASM)
    {
        /* Message */
        msg_print("You cannot exchange places over the chasm.");

        return;
    }

    // recalculate the monster info (in case confusion changed the move)
    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Message */
    msg_format("You exchange places with %s.", m_name);

    // attack of opportunity
    if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused
        && !(r_ptr->flags2 & (RF2_MINDLESS)))
    {
        msg_print("It attacks you as you slip past.");
        make_attack_normal(m_ptr);
    }

    // Alert the monster
    make_alert(m_ptr);

    // Swap positions with the monster
    monster_swap(p_ptr->py, p_ptr->px, y, x);

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
}


static bool item_tester_hook_fletchery_source(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    if (o_ptr->tval == TV_ARROW)
    {
        if (o_ptr->name1 || o_ptr->name2 || o_ptr->att > 0)
            return false;
        return true;
    }

    if (o_ptr->tval == TV_LIGHT
        && (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN))
    {
        if (o_ptr->name1 || o_ptr->name2)
            return false;
        return true;
    }

    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->name1 || o_ptr->name2)
            return false;
        return true;
    }

    return false;
}
enum fletch_source_type
{
    FLETCH_SOURCE_INVEN = 0,
    FLETCH_SOURCE_EQUIP = 1,
    FLETCH_SOURCE_FLOOR = 2
};

typedef struct fletch_choice_s
{
    enum fletch_source_type type;
    int index;
} fletch_choice_t;

static void distribute_fletchered_arrows(const object_type* arrows)
{
    if (!arrows || arrows->number <= 0 || arrows->k_idx == 0)
        return;

    object_type leftover = *arrows;
    bool combined_existing = false;

    /* Try to top up quiver slots first */
    for (int slot = INVEN_QUIVER1; slot <= INVEN_QUIVER2 && leftover.number > 0; slot++)
    {
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
            combined_existing = true;
    }

    /* Then fill stacks in the main pack */
    for (int slot = 0; slot < INVEN_PACK && leftover.number > 0; slot++)
    {
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
            combined_existing = true;
    }

    /* Finally, attempt to add to any other equipped stacks */
    for (int slot = INVEN_WIELD; slot < INVEN_TOTAL && leftover.number > 0; slot++)
    {
        if (slot >= INVEN_QUIVER1 && slot <= INVEN_QUIVER2)
            continue;
        object_type* slot_obj = &inventory[slot];
        if (!slot_obj->k_idx)
            continue;
        if (!object_similar(slot_obj, &leftover))
            continue;
        int before = leftover.number;
        object_absorb(slot_obj, &leftover);
        if (leftover.number != before)
            combined_existing = true;
    }

    if (combined_existing)
    {
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    if (leftover.number <= 0)
        return;

    object_type carry_obj = leftover;
    int carry_slot = inven_carry(&carry_obj, true);

    if (carry_slot == SUPPLIES_INDEX)
    {
        char arrow_name[80];
        object_desc(arrow_name, sizeof(arrow_name), &carry_obj, true, 3);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", arrow_name, label);
    }
    else if (carry_slot >= 0)
    {
        object_type* carried = &inventory[carry_slot];
        char arrow_name[80];
        object_desc(arrow_name, sizeof(arrow_name), carried, true, 3);
        msg_format("You have %s (%c).", arrow_name, index_to_label(carry_slot));

        if (carry_obj.number > 0)
        {
            drop_near(&carry_obj, 0, p_ptr->py, p_ptr->px);
            msg_print("Some arrows spill to the ground.");
        }
    }
    else
    {
        drop_near(&carry_obj, 0, p_ptr->py, p_ptr->px);
        msg_print("Your pack is too full; you leave the arrows on the ground.");
    }

    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

static bool fletchery_choose_source(fletch_choice_t* out_choice)
{
    extern int enhanced_menu_action;
    extern int enhanced_inventory_selected_item;
    extern char current_menu_command;

    /* Prepare inventory menu to include equipment */
    inventory_menu_set_include_equip(true);

    bool old_full = item_tester_full;
    bool old_command_see = p_ptr->command_see;
    int old_command_wrk = p_ptr->command_wrk;
    char old_menu_command = current_menu_command;

    /* Only show fletchery candidates */
    item_tester_full = false;
    item_tester_hook = item_tester_hook_fletchery_source;
    p_ptr->command_wrk = (USE_INVEN);
    p_ptr->command_see = true;
    current_menu_command = 0;

    enhanced_menu_action = ENHANCED_ACTION_NONE;
    enhanced_inventory_selected_item = -1;

    screen_save();
    show_inven_enhanced();
    screen_load();

    inventory_menu_set_include_equip(false);

    item_tester_hook = NULL;
    item_tester_full = old_full;
    p_ptr->command_see = old_command_see;
    p_ptr->command_wrk = old_command_wrk;
    current_menu_command = old_menu_command;

    int action = enhanced_menu_action;
    int selection = enhanced_inventory_selected_item;

    enhanced_menu_action = ENHANCED_ACTION_NONE;
    enhanced_inventory_selected_item = -1;

    if (action != ENHANCED_ACTION_USE || selection == -1)
        return false;

    if (selection == SUPPLIES_INDEX)
    {
        msg_print("Supplies cannot be used for fletchery.");
        return false;
    }

    if (selection < 0)
    {
        out_choice->type = FLETCH_SOURCE_FLOOR;
        out_choice->index = 0 - selection;
    }
    else if (selection >= INVEN_WIELD)
    {
        out_choice->type = FLETCH_SOURCE_EQUIP;
        out_choice->index = selection;
    }
    else
    {
        out_choice->type = FLETCH_SOURCE_INVEN;
        out_choice->index = selection;
    }

    return true;
}

void do_cmd_fletchery(void)
{
    object_type* o_ptr;
    fletch_choice_t choice;

    if (!p_ptr->active_ability[S_ARC][ARC_FLETCHERY])
    {
        msg_print("You need the ability 'fletchery' to use this command.");
        return;
    }

    if (!fletchery_choose_source(&choice))
        return;

    bool from_floor = (choice.type == FLETCH_SOURCE_FLOOR);

    int source_index = choice.index;
    int floor_idx = from_floor ? source_index : 0;

    if (from_floor)
        o_ptr = &o_list[floor_idx];
    else
        o_ptr = &inventory[source_index];

    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_torch = (o_ptr->tval == TV_LIGHT)
        && (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN);
    bool is_staff = (o_ptr->tval == TV_STAFF);

    if (is_arrow)
    {
        if (from_floor)
        {
            msg_print("You need to pick up those arrows before you can work on them.");
            return;
        }

        /* Take a turn */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        msg_print(
            "You begin straightening and adjusting the feathering of the arrows.");

        p_ptr->fletch_item = source_index;
        p_ptr->fletching = o_ptr->number;
        return;
    }

    if (is_torch || is_staff)
    {
        int max_convert = o_ptr->number;
        if (max_convert <= 0)
        {
            msg_print("You have nothing to work with.");
            return;
        }

        int amount = get_quantity("Convert how many?", max_convert);
        if (amount <= 0)
            return;

        /* Take a turn */
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;

        object_type source = *o_ptr;
        source.number = amount;

        char source_name[80];
        object_desc(source_name, sizeof(source_name), &source, true, 3);

        int arrows_per = is_staff ? 6 : 3;
        int produced_total = amount * arrows_per;

        msg_format("You carve %d +3 arrow%s from %s.", produced_total,
            (produced_total == 1) ? "" : "s", source_name);

        /* Remove the raw materials */
        if (from_floor)
        {
            floor_item_increase(floor_idx, -amount);
            floor_item_optimize(floor_idx);
        }
        else
        {
            inven_item_increase(source_index, -amount);
            inven_item_optimize(source_index);
        }

        object_type arrow_proto;
        object_prep(&arrow_proto, lookup_kind(TV_ARROW, SV_NORMAL_ARROW));
        arrow_proto.number = produced_total;
        arrow_proto.att = 3;

        distribute_fletchered_arrows(&arrow_proto);
        return;
    }

    msg_print("That item cannot be used for fletchery.");

}
void finish_fletching(int turns_left)
{
    object_type* o_ptr = &inventory[p_ptr->fletch_item];
    int count = o_ptr->number - turns_left;

    /* Unstack if necessary */
    if (count > 0)
    {
        /* Message */
        msg_format("You improve %d arrows.", count);

        object_type* i_ptr;
        object_type object_type_body;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Obtain a local object */
        object_copy(i_ptr, o_ptr);

        /* Modify quantity */
        i_ptr->number = count;
        i_ptr->att = 3;

        /* Reduce original pile */
        inven_item_increase(p_ptr->fletch_item, -count);
        inven_item_optimize(p_ptr->fletch_item);

        /* Add new arrows */
        distribute_fletchered_arrows(i_ptr);
    }
    else
    {
        msg_print("You did not manage to improve any arrows.");
    }

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN);

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP);
}

/*
 * Determine if a given grid may be "tunneled"
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
        /* Check for special drops from quartz in cave or chasm areas only */
        /* Quartz veins in caves are marked with CAVE_ROOM; chasms add CAVE_CHASM_AREA */
        /* Chance scales with depth; allow cave-adjacent quartz and tagged chasm veins */
        int depth = p_ptr->depth;
        bool in_cave = (cave_info[y][x] & CAVE_ROOM) != 0;
        if (!in_cave)
        {
            /* Treat quartz abutting cave floors as part of the cave for drops */
            for (int dy = -1; dy <= 1 && !in_cave; ++dy)
            {
                for (int dx = -1; dx <= 1 && !in_cave; ++dx)
                {
                    if (!dy && !dx) continue;
                    int ny = y + dy, nx = x + dx;
                    if (!in_bounds(ny, nx)) continue;
                    if (cave_info[ny][nx] & CAVE_ROOM)
                        in_cave = true;
                }
            }
        }
        bool in_chasm_area = (cave_info[y][x] & CAVE_CHASM_AREA) != 0;
        bool allow_mithril = in_cave && !in_chasm_area;
        bool allow_star_iron = in_chasm_area;
        
        /* Base 10% chance at depth 10, scaling up to 25% at depth 20+ */
        int special_chance = 10 + depth;
        if (special_chance > 25) special_chance = 25;
        
        log_debug("twall: digging vein at (%d,%d) depth=%d cave_info=0x%04x in_cave=%d in_chasm=%d allow_mithril=%d allow_star_iron=%d special_chance=%d%%",
                  y, x, depth, cave_info[y][x], in_cave, in_chasm_area, allow_mithril, allow_star_iron, special_chance);
        
        if ((allow_mithril || allow_star_iron) && depth >= 10 && rand_int(100) < special_chance)
        {
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;
            object_wipe(i_ptr);
            
            log_debug("twall: PASSED chance check! Attempting drop at depth=%d", depth);
            
            /* 30% chance for metal at depth 12+, otherwise try for gem */
            bool try_star_iron = allow_star_iron && (depth >= 12) && (rand_int(100) < 45);
            bool try_mithril = allow_mithril && (depth >= 12) && (rand_int(100) < 45);
            
            log_debug("twall: try_star_iron=%d try_mithril=%d", try_star_iron, try_mithril);
            
            if (try_star_iron)
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
                gem_profile.supply_misc = 0;

                if (drop_generate_object_profiled(depth, DROP_QUALITY_NORMAL,
                        DROP_TYPE_STAFF, 0, false, &gem_profile, i_ptr))
                {
                    log_debug("twall: gem generated successfully, tval=%d", i_ptr->tval);
                    if (i_ptr->tval == TV_GEM)
                        i_ptr->number = 1;
                    drop_near(i_ptr, -1, y, x);
                    msg_print("A gem glitters in the rubble!");
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
            if (!get_item(&item, "Use which digger? ",
                    "You are not carrying a shovel or mattock.", (USE_INVEN)))
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
            char o_full_name[80];

            /* Short, pre-identification object description */
            object_desc(
                o_short_name, sizeof(o_short_name), digger_ptr, false, 0);

            ident(digger_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), digger_ptr, true, 3);

            /* Print the messages */
            msg_format(
                "You notice that your %s is especially suited to tunneling.",
                o_short_name);
            msg_format("You are wielding %s.", o_full_name);
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
    int y, x, dir;

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
            msg_print("The door holds firm.");
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

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
        return;

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

            if ((o_ptr->pval > 0) && chest_traps[o_ptr->pval]
                && object_known_p(o_ptr))
                chest_trap = true;
        }
        else if (o_ptr->tval == TV_SKELETON)
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

/*
 * Determine if a given grid may be "walked"
 */
bool do_cmd_walk_test(int y, int x)
{
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

/*
 * Determines the percentage chance of an object breaking when thrown or fired
 *
 * Note that artefacts never break, see the "drop_near()" function.
 */
static int breakage_chance(const object_type* o_ptr, bool hit_wall)
{
    int p = 0;

    /* Examine the item type */
    switch (o_ptr->tval)
    {
    /* Always break */
    case TV_FLASK:
    case TV_POTION:
    {
        p = 100;
        break;
    }

    /* Often break */
    case TV_LIGHT:
    {
        /* Jewels don't break */
        if ((o_ptr->tval == TV_LIGHT)
            && ((o_ptr->sval == SV_LIGHT_SILMARIL)
                || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)))
        {
            p = 0;
        }
        else
        {
            p = 20;
        }
        break;
    }

    /* Sometimes break */
    case TV_ARROW:
    {
        p = 10;

        break;
    }

    /* Rarely break */
    default:
    {
        p = 5;

        break;
    }
    }

    /* double breakage chance if it hit a wall */
    if (hit_wall)
    {
        p *= 2;
        if (p > 100)
            p = 100;
    }
    // Unless they hit a wall, items designed for throwing won't break
    else if (player_can_treat_as_throwing(o_ptr))
    {
        p = 0;
    }

    return (p);
}

/*
 *  Determines if a bow shoots radiant arrows and lights the current grid if so
 */
bool do_radiance(int y, int x, const object_type* j_ptr)
{
    bool radiance = false;

    // determine if the bow has 'radiance'
    if (j_ptr->name1 && (a_info[j_ptr->name1].flags2 & (TR2_RADIANCE)))
        radiance = true;
    if (j_ptr->name2 && (e_info[j_ptr->name2].flags2 & (TR2_RADIANCE)))
        radiance = true;

    // If the bow has 'radiance' and the square is dark, then light it
    if (radiance && !(cave_info[y][x] & (CAVE_GLOW)))
    {
        // Give it light
        cave_info[y][x] |= (CAVE_GLOW);

        // Remember the grid
        cave_info[y][x] |= (CAVE_MARK);

        // Fully update the visuals
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

        // Update stuff
        update_stuff();

        return (true);
    }
    else
    {
        return (false);
    }
}

extern int archery_range(const object_type* j_ptr)
{
    int range;

    range = (j_ptr->dd * total_ads(j_ptr) * 3) / 2;
    if (range > MAX_RANGE)
        range = MAX_RANGE;

    return (range);
}

extern int throwing_range(const object_type* i_ptr)
{
    int div;
    int range;
    u32b f1, f2, f3;

    object_flags(i_ptr, &f1, &f2, &f3);

    /* the divisor is the weight + 2lb */
    div = i_ptr->weight + 20;

    range = (weight_limit() / 5) / div;

    /* Max distance of MAX_RANGE */
    if (range > MAX_RANGE)
        range = MAX_RANGE;

    /* Min distance of 1 */
    if (range < 1)
        range = 1;

    return (range);
}

/*
 * Give all adjacent, alert, non-mindless opponents (except one whose
 * coordinates are supplied) a free attack on the player.
 */
void attacks_of_opportunity(int neutralized_y, int neutralized_x)
{
    int i;
    int y, x;
    int start;
    int opportunity_attacks = 0;

    monster_type* m_ptr;
    monster_race* r_ptr;

    start = rand_int(8);

    /* Look for adjacent monsters */
    for (i = start; i < 8 + start; i++)
    {
        y = p_ptr->py + ddy_ddd[i % 8];
        x = p_ptr->px + ddx_ddd[i % 8];

        /* Check Bounds */
        if (!in_bounds(y, x))
            continue;

        // 'Point blank archery' avoids attacks of opportunity from the monster
        // shot at
        if (p_ptr->active_ability[S_ARC][ARC_POINT_BLANK]
            && (neutralized_y == y) && (neutralized_x == x))
            continue;

        // if it is occupied by a monster
        if (cave_m_idx[y][x] > 0)
        {
            m_ptr = &mon_list[cave_m_idx[y][x]];
            r_ptr = &r_info[m_ptr->r_idx];

            // the monster must be alert, not confused, and not fleeing or
            // peaceful
            if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused
                && (m_ptr->stance != STANCE_FLEEING)
                && !(r_ptr->flags2 & (RF2_MINDLESS))
                && !(r_ptr->flags1 & (RF1_PEACEFUL)) && !m_ptr->skip_next_turn
                && !m_ptr->skip_this_turn)
            {
                int evn = p_ptr->skill_use[S_EVN];
                opportunity_attacks++;

                if (opportunity_attacks == 1)
                {
                    msg_print("You provoke attacks of opportunity from "
                              "adjacent enemies!");
                }

                p_ptr->skill_use[S_EVN] = evn / 2;
                make_attack_normal(m_ptr);
                p_ptr->skill_use[S_EVN] = evn;
            }
        }
    }

    return;
}

/*
 * Fire an object from the pack or floor.
 *
 * See "calc_bonuses()" for more calculations and such.
 *
 * Note that "firing" a missile is MUCH better than "throwing" it.
 *
 * Note: "unseen" monsters are very hard to hit.
 *
 * Objects are more likely to break if they "attempt" to hit a monster.
 *
 * The "extra shot" code works by decreasing the amount of energy
 * required to make each shot, spreading the shots out over time.
 *
 * Note that when firing missiles, the launcher multiplier is applied
 * after all the bonuses are added in, making multipliers very useful.
 */
static bool abort_for_valorous_ranged_path(int range, int ty, int tx)
{
    u16b path_g[256];
    int path_n;
    int ty2 = ty;
    int tx2 = tx;

    if (range <= 0)
        return false;

    if (!chosen_oath(OATH_VALOROUS) || oath_invalid(OATH_VALOROUS))
        return false;

    path_n = project_path(
        path_g, range, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_THRU);

    for (int i = 0; i < path_n; i++)
    {
        int y = GRID_Y(path_g[i]);
        int x = GRID_X(path_g[i]);

        /* Stop before hitting walls */
        if (!cave_floor_bold(y, x))
            break;

        if (cave_m_idx[y][x] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            if (m_ptr->ml && (m_ptr->stance == STANCE_FLEEING))
                return abort_for_valorous(m_ptr);
        }
    }

    return false;
}

void do_cmd_fire(int quiver)
{
    int dir, item;
    int i, y, x, ty, tx;
    int ty2,
        tx2; // dummy variables needed to pass to the path projection function
    int first_y = 0, first_x = 0;
    int tdis;

    u32b f1, f2, f3; // the bow's flags

    int attack_mod = 0, total_attack_mod = 0;
    int total_evasion_mod = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0, slay_bonus_dice = 0;
    int crippling_blow_multiplier = 0;
    int total_dd = 0, total_ds = 0;
    int dam = 0, prt = 0, prt_percent = 100;
    int net_dam = 0;

    int shot;
    int shots = 1;

    object_type* o_ptr;
    object_type* j_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    monster_type* m_ptr;
    monster_race* r_ptr;

    char o_name[80];
    char punctuation[20];

    int path_n;
    u16b path_g[256];

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    u32b noticed_arrow_flag
        = 0L; // if a slay is noticed on the arrow/bow it is recorded here
    u32b noticed_bow_flag = 0L; // and the arrow/bow will be identified.

    bool noticed_radiance = false;

    bool pierce = false;
    bool targets_remaining = false;
    bool deadly_hail_bonus = false;
    bool puncture = false;
    bool returning_arrow = false;

    // Determine the projectile in the requested quiver
    if (quiver == 1)
    {
        o_ptr = &inventory[INVEN_QUIVER1];
        item = INVEN_QUIVER1;

        if (!o_ptr->k_idx)
        {
            msg_print("You have nothing in your 1st quiver.");
            return;
        }
    }
    else
    {
        o_ptr = &inventory[INVEN_QUIVER2];
        item = INVEN_QUIVER2;

        if (!o_ptr->k_idx)
        {
            msg_print("You have nothing in your 2nd quiver.");
            return;
        }
    }

    returning_arrow = false;

    /* Determine whether the item should be thrown directly */
    object_flags(o_ptr, &f1, &f2, &f3);
    if (player_can_treat_as_throwing_flags(o_ptr, f3))
    {
        do_cmd_throw_from_slot(item);
        return;
    }

    /* Get the "bow" (if any) */
    j_ptr = &inventory[INVEN_BOW];

    /* Require a usable launcher */
    if (!j_ptr->tval || !p_ptr->ammo_tval)
    {
        msg_print("You have nothing to fire with.");
        return;
    }

    /* Base range */
    tdis = archery_range(j_ptr);

    // bow flags
    object_flags(j_ptr, &f1, &f2, &f3);

    /* Handle player fear */
    if (p_ptr->afraid)
    {
        /* Message */
        msg_print("You are too afraid to aim properly!");

        /* Done */
        return;
    }

    /* Get a direction (or cancel) */
    if (!get_aim_dir(&dir, tdis))
        return;

    /* Start at the player */
    y = p_ptr->py;
    x = p_ptr->px;

    /* Predict the "target" location */
    ty = p_ptr->py + 99 * ddy[dir];
    tx = p_ptr->px + 99 * ddx[dir];

    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        ty = p_ptr->py;
        tx = p_ptr->px;
    }

    /* Check for "target request" */
    if ((dir == 5) && target_okay(tdis))
    {
        ty = p_ptr->target_row;
        tx = p_ptr->target_col;

        m_ptr = &mon_list[cave_m_idx[ty][tx]];
        r_ptr = &r_info[m_ptr->r_idx];

        if (abort_for_mercy(m_ptr))
        {
            return;
        }
    }

    /* Warn before ranged attacks that might hit fleeing enemies (Oath of Valor) */
    if (abort_for_valorous_ranged_path(tdis, ty, tx))
        return;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain a local object */
    object_copy(i_ptr, o_ptr);

    /* Determine the base attack score */
    attack_mod = (p_ptr->skill_use[S_ARC] + i_ptr->att);

    /* Single object */
    i_ptr->number = 1;

    /* Set pickup on fired arrow */
    i_ptr->pickup = true;
    i_ptr->pickup_slot = item;

    /* Sound */
    sound(MSG_SHOOT);
    if (use_sound) {
        Term_xtra(TERM_XTRA_DELAY, 350);
    }

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), i_ptr, false, 3);

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_ARCHERY;

    if (p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL])
    {
        deadly_hail_bonus = p_ptr->killed_enemy_with_arrow;
    }

    p_ptr->killed_enemy_with_arrow = false;

    // set dummy variables to pass to project_path (so it doesn't clobber the
    // real ones)
    ty2 = ty;
    tx2 = tx;

    /* Calculate the path */
    path_n = project_path(
        path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_THRU);

    /* Hack -- Handle stuff */
    handle_stuff();

    /* If the bow has 'radiance', then light the starting square */
    noticed_radiance = do_radiance(y, x, j_ptr);

    for (shot = 0; shot < shots; shot++)
    {
        bool hit_wall = false;
        bool ghost_arrow = false;
        int missed_monsters = 0;
        int final_y = GRID_Y(path_g[path_n - 1]);
        int final_x = GRID_X(path_g[path_n - 1]);

        // abort the later shot(s) if there is no target on the trajectory
        if ((shot > 0) && !targets_remaining)
            break;
        targets_remaining = false;

        /* Reduce and describe inventory */
        if (!returning_arrow && item >= 0)
        {
            inven_item_increase(item, -1);
            inven_item_describe(item);
            inven_item_optimize(item);
        }

        /* Reduce and describe floor item */
        else if (!returning_arrow)
        {
            floor_item_increase(0 - item, -1);
            floor_item_optimize(0 - item);
        }

        /* Project along the path */
        for (i = 0; i < path_n; ++i)
        {
            int oy = y;
            int ox = x;

            int ny = GRID_Y(path_g[i]);
            int nx = GRID_X(path_g[i]);

            /* Hack -- Stop before hitting walls */
            if (!cave_floor_bold(ny, nx))
            {
                // if the arrow hasn't already stopped, do some things...
                if (!ghost_arrow)
                {
                    hit_wall = true;

                    // record resting place of arrow
                    final_y = y;
                    final_x = x;

                    // Show collision
                    /* Only do visuals if the player can "see" the missile */
                    if (panel_contains(ny, nx))
                    {
                        /* Obtain the bolt pict */
                        u16b p = bolt_pict(y, x, y, x, GF_ARROW);

                        /* Extract attr/char */
                        byte a = PICT_A(p);
                        char c = PICT_C(p);

                        /* Display the visual effects */
                        print_rel(c, a, ny, nx);

                        move_cursor_relative(ny, nx);
                        Term_fresh();
                        Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
                        lite_spot(ny, nx);
                        Term_fresh();
                    }

                    /* Delay anyway for consistency */
                    else
                    {
                        /* Pause anyway, for consistancy */
                        Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
                    }
                }

                break;
            }

            /* Advance */
            x = nx;
            y = ny;

            // after an arrow has stopped, keep looking along the path,
            // but don't attempt to hit creatures, or display graphics or
            // anything
            if (ghost_arrow)
            {
                if (cave_m_idx[y][x] > 0)
                {
                    if (!forgo_attacking_unwary
                        || ((&mon_list[cave_m_idx[y][x]])->alertness
                            >= ALERTNESS_ALERT))
                        targets_remaining = true;
                }

                continue;
            }

            /* If the bow has 'radiance', then light the square being passed
             * over */
            noticed_radiance = do_radiance(y, x, j_ptr) | noticed_radiance;

            /* Only do visuals if the player can "see" the missile */
            if (panel_contains(y, x) && player_can_see_bold(y, x))
            {
                /* Obtain the bolt pict */
                u16b p = bolt_pict(oy, ox, y, x, GF_ARROW);

                /* Extract attr/char */
                byte a = PICT_A(p);
                char c = PICT_C(p);

                /* Display the visual effects */
                print_rel(c, a, y, x);

                move_cursor_relative(y, x);
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, msec);
                lite_spot(y, x);
                Term_fresh();
            }

            /* Delay anyway for consistency */
            else
            {
                /* Pause anyway, for consistancy */
                Term_xtra(TERM_XTRA_DELAY, msec);
            }

            /* Handle monster */
            if (cave_m_idx[y][x] > 0)
            {
                m_ptr = &mon_list[cave_m_idx[y][x]];
                r_ptr = &r_info[m_ptr->r_idx];

                if (abort_for_mercy(m_ptr))
                {
                    return;
                }

                // record the co-ordinates of the first monster in line of fire
                if (first_y == 0)
                    first_y = y;
                if (first_x == 0)
                    first_x = x;

                // Determine the player's attack score after all modifiers
                total_attack_mod = total_player_attack(m_ptr, attack_mod);

                /* Monsters might notice */
                player_attacked = true;

                // Modifications for shots that go past the target or strike
                // things before the target...
                if ((dir == 5) && target_okay(tdis))
                {
                    // if there is a specific target and this is not it, then
                    // massively penalise
                    if ((ty != y) || (tx != x))
                    {
                        total_attack_mod = 0;
                    }
                }
                // if it is just a shot in a direction and has already missed
                // something, then massively penalise
                else if (missed_monsters > 0)
                {
                    total_attack_mod = 0;
                }
                // if it is a shot in a direction and this is the first monster
                else
                {
                    /* Hack -- Track this monster race */
                    if (m_ptr->ml)
                        monster_race_track(m_ptr->r_idx);

                    /* Hack -- Track this monster */
                    if (m_ptr->ml)
                        health_track(cave_m_idx[y][x]);

                    /* Hack -- Target this monster */
                    if (m_ptr->ml)
                        target_set_monster(cave_m_idx[y][x]);
                }

                // Aim improved if monster is fleeing. If firing into several
                // fleeing monsters, the chance of hitting one is higher.
                if (p_ptr->active_ability[S_ARC][ARC_ROUT]
                    && m_ptr->stance == STANCE_FLEEING)
                {
                    total_attack_mod += 5;
                }

                // Determine the monster's evasion after all modifiers
                total_evasion_mod = total_monster_evasion(m_ptr, true);

                // No killing peaceful creatures
                if (r_ptr->flags1 & (RF1_PEACEFUL))
                {
                    hit_result = 0;
                }
                else
                {
                    /* Test for hit */
                    hit_result = hit_roll(total_attack_mod, total_evasion_mod,
                        PLAYER, m_ptr, true);
                }

                if (hit_result <= 0 && f3 & TR3_ACCURATE)
                {
                    hit_result = hit_roll(total_attack_mod, total_evasion_mod,
                        PLAYER, m_ptr, true);
                    if (hit_result > 0)
                        msg_print("Your arrow flies true.");
                }

                /* If it hit */
                if (hit_result > 0)
                {
                    /* Assume a default death */
                    cptr note_dies = " dies.";

                    char m_name[80];

                    /* Get the monster name (or "it") */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                    /*Mark the monster as attacked by the player*/
                    m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

                    if (monster_nonliving(r_ptr))
                    {
                        /* Special note at death */
                        note_dies = " is destroyed.";
                    }

                    // Handle sharpness (which can change 'hit' message)
                    prt_percent
                        = prt_after_sharpness(i_ptr, &noticed_arrow_flag);
                    if (percent_chance(100 - prt_percent))
                    {
                        pierce = true;
                    }

                    /* Add 'critical hit' dice based on bow weight */
                    crit_bonus_dice = crit_bonus(
                        hit_result, j_ptr->weight, r_ptr, S_ARC, false, NULL, NULL);

                    if (f3 & TR3_CUMBERSOME)
                    {
                        crit_bonus_dice = 0;
                    }

                    if (p_ptr->active_ability[S_ARC][ARC_AMBUSH]
                        && m_ptr->alertness < ALERTNESS_ALERT)
                    {
                        crit_bonus_dice++;
                    }

                    /* Add slay (or brand) dice based on both arrow and bow */
                    slay_bonus_dice
                        = slay_bonus(i_ptr, m_ptr, &noticed_arrow_flag);
                    slay_bonus_dice
                        += slay_bonus(j_ptr, m_ptr, &noticed_bow_flag);

                    /* Calculate the damage done */
                    total_dd = j_ptr->dd + crit_bonus_dice + slay_bonus_dice;

                    if (deadly_hail_bonus)
                        total_dd *= 2;

                    // Sil-y: debugging test case
                    if (p_ptr->ads <= 0)
                    {
                        msg_format("BUG: Your damage sides for archery are %d.",
                            p_ptr->ads);
                        msg_format("BUG: Recalculating them would give %d.",
                            total_ads(j_ptr));
                        msg_format("BUG: j_ptr->ds is %d.", j_ptr->ds);
                    }

                    total_ds = total_ads(j_ptr);

                    /* Can't have a negative number of sides */
                    if (total_ds < 0)
                        total_ds = 0;

                    dam = damroll(total_dd, total_ds);
                    
                    /* Apply armor dice/sides curses/blessings */
                    int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
                    if (armor_dice_base < 0)
                        armor_dice_base = 0;
                    int armor_dice = armor_dice_base + curse_flag_count_cur(CUR_MON_ARM_DICE);
                    int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_count_cur(CUR_MON_ARM_SIDE);
                    if (armor_dice < 0) armor_dice = 0;
                    if (armor_sides < 1) armor_sides = 1;
                    prt = damroll(armor_dice, armor_sides);

                    prt = (prt * prt_percent) / 100;

                    if (prt >= dam
                        && p_ptr->active_ability[S_ARC][ARC_PUNCTURE])
                    {
                        puncture = true;
                        dam = 5;
                        prt = 0;
                    }

                    net_dam = dam - prt;

                    // no negative damage
                    if (net_dam < 0)
                        net_dam = 0;

                    break_mercy_oath(m_ptr, net_dam);
                    break_valorous_oath(m_ptr, net_dam, ATT_MAIN, -1);  // Direct archery shot

                    /* Handle unseen monster */
                    if (!(m_ptr->ml))
                    {
                        /* Unseen monster */
                        msg_format("The %s finds a mark.", o_name);
                    }

                    /* Handle visible monster */
                    else
                    {
                        char m_name[80];

                        /* Get "the monster" or "it" */
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                        // determine the punctuation for the attack ("...", ".",
                        // "!" etc)
                        attack_punctuation(
                            punctuation, net_dam, crit_bonus_dice);

                        /* Message */
                        if (pierce)
                            msg_format("The %s pierces %s%s", o_name, m_name,
                                punctuation);
                        else if (deadly_hail_bonus)
                            msg_format("The %s tears into %s!", o_name, m_name);
                        else if (puncture)
                            msg_format("The %s hits %s in a vulnerable spot%s",
                                o_name, m_name, punctuation);
                        else
                            msg_format("The %s hits %s%s", o_name, m_name,
                                punctuation);
                    }

                    // if a slay was noticed, then identify the bow/arrow
                    if (noticed_arrow_flag || noticed_bow_flag)
                    {
                        ident_bow_arrow_by_use(j_ptr, i_ptr, o_ptr, m_ptr,
                            noticed_bow_flag, noticed_arrow_flag);
                    }

                    /* No negative damage */
                    if (net_dam < 0)
                        net_dam = 0;

                    update_combat_rolls2(total_dd, total_ds, dam, armor_dice,
                        armor_sides, prt, prt_percent, GF_HURT, false);

                    // hit the monster, check for death
                    p_ptr->killed_enemy_with_arrow = mon_take_hit(
                        cave_m_idx[y][x], net_dam, note_dies, -1);
                    display_hit(
                        y, x, net_dam, GF_HURT, p_ptr->killed_enemy_with_arrow);

                    // if this wasn't the killing shot
                    if (!p_ptr->killed_enemy_with_arrow)
                    {
                        // there is at least one target left on the trajectory
                        targets_remaining = true;

                        // alert the monster, even if no damage was done
                        // (if damage was done, then it was alerted by
                        // mon_take_hit() )
                        if (net_dam == 0)
                        {
                            make_alert(m_ptr);
                        }

                        if ((f2 & (TR2_RADIANCE))
                            && r_ptr->flags3 & RF3_HURT_LITE && net_dam > 5
                            && one_in_(monster_skill(m_ptr, S_WIL)))
                        {
                            bool known_radiance
                                = object_known_p(j_ptr) || noticed_radiance;
                            if (m_ptr->ml && known_radiance)
                            {
                                msg_format("%^s contorts as the shining arrow "
                                           "strikes it!",
                                    m_name);
                            }

                            stun_monster(m_ptr, net_dam);
                        }

                        // Morgoth drops his iron crown if he is hit for 10 or
                        // more net damage twice
                        if (m_ptr->r_idx == R_IDX_MORGOTH)
                        {
                            if (net_dam >= 10
                                && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
                            {
                                if (p_ptr->morgoth_hits == 0)
                                {
                                    msg_print("The force of your shot knocks "
                                              "the Iron Crown off "
                                              "balance.");
                                    p_ptr->morgoth_hits++;
                                }
                                else if (p_ptr->morgoth_hits == 1)
                                {
                                    drop_iron_crown(m_ptr,
                                        "You knock his crown from off his "
                                        "brow, and "
                                        "it falls to the ground nearby.");
                                    p_ptr->morgoth_hits++;
                                }
                            }
                        }

                        /* Message */
                        message_pain(cave_m_idx[y][x], net_dam);

                        // Deal with crippling shot ability
                        if (p_ptr->active_ability[S_ARC][ARC_CRIPPLING]
                            && (crit_bonus_dice >= 1) && (net_dam > 0)
                            && !(r_ptr->flags1 & (RF1_RES_CRIT)))
                        {
                            // Slightly magical. Function that caps out before
                            // 30 but grows quickly early on, and
                            // doesn't need math.h
                            crippling_blow_multiplier
                                = (30 - (60 / (crit_bonus_dice + 2)));
                            if (skill_check(PLAYER, crippling_blow_multiplier,
                                    monster_skill(m_ptr, S_WIL), m_ptr)
                                > 0)
                            {
                                msg_format("Your shot cripples %^s!", m_name);

                                // slow the monster
                                // The +1 is needed as a turn of this wears off
                                // immediately
                                set_monster_slow(
                                    cave_m_idx[m_ptr->fy][m_ptr->fx],
                                    m_ptr->slowed + crit_bonus_dice + 1, false);
                            }
                        }
                    }

                    /* Stop looking if a monster was hit but not pierced */
                    if (!pierce)
                    {
                        // continue checking trajectory, but without affecting
                        // things
                        ghost_arrow = true;

                        // record resting place of arrow
                        final_y = y;
                        final_x = x;
                    }
                    else
                    {
                        pierce = false;
                    }
                }

                // if it misses the monster...
                else
                {
                    // there is at least one target left on the trajectory
                    targets_remaining = true;
                }

                /* we have missed a target, but could still hit something (with
                 * a penalty) */
                missed_monsters++;
            }
        }

        if (!object_known_p(j_ptr) && noticed_radiance)
        {
            char j_short_name[80];
            char j_full_name[80];

            object_desc(j_short_name, sizeof(j_short_name), j_ptr, false, 0);
            object_aware(j_ptr);
            object_known(j_ptr);
            object_desc(j_full_name, sizeof(j_full_name), j_ptr, true, 3);

            msg_print("The arrow leaves behind a trail of light!");
            msg_format(
                "You recognize your %s to be %s", j_short_name, j_full_name);
        }

        // Break the truce if creatures see
        break_truce(false);

        /* Drop (or break) near that location */
        if (!returning_arrow)
            drop_near(i_ptr, breakage_chance(i_ptr, hit_wall), final_y, final_x);
    }

    /* Have to set this here as well, just in case... */
    /* Monsters might notice */
    player_attacked = true;

    p_ptr->redraw |= (PR_ARC | PR_QUIVER);

    // provoke attacks of opportunity
    if (p_ptr->active_ability[S_ARC][ARC_POINT_BLANK])
        attacks_of_opportunity(first_y, first_x);
    else
        attacks_of_opportunity(0, 0);
}

/*handle special effects of throwing certain potions*/
static bool thrown_potion_effects(object_type* o_ptr, bool* is_dead, int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    int y = m_ptr->fy;
    int x = m_ptr->fx;

    bool ident = false;

    bool un_confuse = false;
    bool un_stun = false;

    bool used_potion = true;

    /* Hold the monster name */
    char m_name[80];
    char m_poss[80];

    /* Get the monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Get the monster possessive ("his"/"her"/"its") */
    monster_desc(m_poss, sizeof(m_name), m_ptr, 0x22);

    /* Analyze the potion */
    switch (o_ptr->sval)
    {
    case SV_POTION_SLOWNESS:
    {
        /*slowness explosion at the site, radius 0*/
        ident = explosion(-1, 0, y, x, 0, 0, 10, GF_SLOW);
        break;
    }

    case SV_POTION_CONFUSION:
    {
        /*confusion explosion at the site, radius 0*/
        ident = explosion(-1, 0, y, x, 0, 0, 10, GF_CONFUSION);
        break;
    }

    case SV_POTION_true_SIGHT:
    {
        if ((!m_ptr->ml) && (r_ptr->flags2 & (RF2_INVISIBLE)))
        {
            /* Mark as visible */
            m_ptr->ml = true;

            /*re-draw the spot*/
            lite_spot(y, x);

            /* Update the monster name*/
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /*monster forgets player history*/
            msg_format("%^s appears for an instant!", m_name);

            /*update the lore*/
            l_ptr->flags2 |= (RF2_INVISIBLE);

            ident = true;
        }

        /* Potion isn't idntified */
        else
            used_potion = false;

        break;
    }

    case SV_POTION_QUICKNESS:
    {
        /*speed explosion at the site, radius 0*/
        ident = explosion(-1, 0, y, x, 1, 4, -1, GF_SPEED);
        break;
    }

    /*potion just gets thrown as normal object*/
    default:
    {
        used_potion = false;

        break;
    }
    }

    /*monster is now dead, skip messages below*/
    if (cave_m_idx[y][x] == 0)
    {
        un_confuse = false;
        un_stun = false;
        *is_dead = true;
    }

    if (un_confuse)
    {
        if (m_ptr->confused)
        {
            /* No longer confused */
            m_ptr->confused = 0;

            /* Dump a message */
            if (m_ptr->ml)
            {
                msg_format("%^s is no longer confused.", m_name);

                ident = true;
            }
        }
    }

    if (un_stun)
    {
        if (m_ptr->stunned)
        {
            /* No longer confused */
            m_ptr->stunned = 0;

            /* Dump a message */
            if (m_ptr->ml)
            {
                msg_format("%^s is no longer stunned.", m_name);

                ident = true;
            }
        }
    }

    /*inform them of the potion, mark it as known*/
    if ((ident) && (!(k_info[o_ptr->k_idx].aware)))
    {
        char o_name[80];

        /* Identify it fully */
        object_aware(o_ptr);
        object_known(o_ptr);

        /* Description */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Describe the potion */
        msg_format("You threw %s.", o_name);

        /* Combine / Reorder the pack (later) */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    }

    /* Redraw if necessary*/
    if (used_potion)
        p_ptr->redraw |= (PR_HEALTHBAR);

    /* Handle stuff */
    handle_stuff();

    return (used_potion);
}

/*
 * Throw an object from the pack or floor.
 *
 * Note: "unseen" monsters are very hard to hit.
 *
 * Should throwing a weapon do full damage?  Should it allow the magic
 * to hit bonus of the weapon to have an effect?  Should it ever cause
 * the item to be destroyed?  Should it do any damage at all?
 */
void do_cmd_throw(bool automatic)
{
    int dir, item;
    int i, j, y, x, ty, tx;
    int ty2,
        tx2; // dummy variables needed to pass to the path projection function
    int tdis;
    u32b f1, f2, f3;

    int attack_mod = 0, total_attack_mod = 0;
    int total_evasion_mod = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0, slay_bonus_dice = 0;
    int total_bonus_dice = 0;
    int total_ds = 0;
    int dam = 0, prt = 0, prt_percent = 100;
    int net_dam = 0;

    monster_type* m_ptr;
    monster_race* r_ptr;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    bool hit_body = false;
    bool hit_wall = false;
    bool treat_as_throwing = false;

    int missed_monsters = 0;

    bool spatial_target = false;

    byte missile_attr;
    char missile_char;

    char o_name[80];
    char punctuation[20];

    int path_n;
    u16b path_g[256];

    cptr q, s;

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    u32b noticed_flag
        = 0; // if a slay is noticed it is recorded here and the item identified

    int preset_item = throw_pending_slot;
    bool preset = (preset_item != THROW_PENDING_NONE);

    if (preset)
    {
        item = preset_item;
        automatic = false;
    }

    throw_pending_slot = THROW_PENDING_NONE;

    if (!preset && automatic)
    {
        bool found = false;

        /* Scan the inventory */
        for (i = 0; i < INVEN_PACK; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (player_can_treat_as_throwing_flags(o_ptr, f3))
            {
                item = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            msg_print("You don't have anything designed for throwing in your "
                      "inventory.");
            return;
        }
    }
    else if (!preset)
    {
        /* Get an item */
        q = "Throw which item? ";
        s = "You have nothing to throw.";
        if (!get_item(&item, q, s, (USE_INVEN | USE_FLOOR | USE_EQUIP)))
            return;
    }

    /* Get the object */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }
    else
    {
        o_ptr = &o_list[0 - item];
    }

    if (!o_ptr->k_idx)
    {
        if (preset)
            msg_print("You have nothing ready to throw.");
        return;
    }

    /* Hack -- Cannot remove cursed items */
    if ((item >= INVEN_WIELD) && cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return;
        }
    }

    // Determine throwing range
    tdis = throwing_range(o_ptr);

    /* Examine the item */
    object_flags(o_ptr, &f1, &f2, &f3);

    // Aim automatically if asked
    if (automatic)
    {
        if (target_okay(tdis))
            dir = 5;

        else
        {
            /* Prepare the "temp" array */
            get_sorted_target_list(TARGET_KILL, tdis);

            /* Monster */
            if (temp_n)
            {
                target_set_monster(cave_m_idx[temp_y[0]][temp_x[0]]);
                health_track(cave_m_idx[temp_y[0]][temp_x[0]]);
                dir = 5;
            }

            else
            {
                msg_print("No clear target for automatic throwing.");
                return;
            }
        }

        if (p_ptr->confused)
        {
            dir = ddd[rand_int(8)];
        }
    }

    // Otherwise get a direction (or cancel) */
    else if (!get_aim_dir(&dir, tdis))
        return;

    int original_slot = (item >= INVEN_WIELD) ? item : -1;

    /* If we're throwing from equipment (including quivers), set redraw flag */
    bool throwing_from_equipment = (original_slot >= INVEN_WIELD);
    if (throwing_from_equipment && (original_slot == INVEN_QUIVER1 || original_slot == INVEN_QUIVER2))
    {
        p_ptr->redraw |= (PR_QUIVER);
    }

    /* Start at the player */
    y = p_ptr->py;
    x = p_ptr->px;

    /* Predict the "target" location */
    ty = p_ptr->py + 99 * ddy[dir];
    tx = p_ptr->px + 99 * ddx[dir];

    /* Check for "target request" */
    if (dir == 5)
    {
        log_debug("do_cmd_throw: dir=5, checking target_okay(tdis=%d)", tdis);
        log_debug("do_cmd_throw: BEFORE target_okay: target_row=%d target_col=%d target_who=%d",
            p_ptr->target_row, p_ptr->target_col, p_ptr->target_who);
        
        if (!target_okay(tdis))
        {
            msg_print("You have no target.");
            return;
        }

        log_debug("do_cmd_throw: AFTER target_okay: target_row=%d target_col=%d target_who=%d",
            p_ptr->target_row, p_ptr->target_col, p_ptr->target_who);

        ty = p_ptr->target_row;
        tx = p_ptr->target_col;

        /* If we're targeting a location (not a monster), stop the throw there. */
        spatial_target = (p_ptr->target_who == 0);

        /* Oath of Mercy may block throwing at an explicit monster target. */
        if (!spatial_target && (p_ptr->target_who > 0))
        {
            monster_type* target_m_ptr = &mon_list[p_ptr->target_who];
            if (abort_for_mercy(target_m_ptr))
            {
                return;
            }
        }
    }

    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        ty = p_ptr->py;
        tx = p_ptr->px;
    }

    /* Clamp directional throws to the actual range before indexing grids */
    if ((dir != 5) && (dir != DIRECTION_UP) && (dir != DIRECTION_DOWN))
    {
        ty = p_ptr->py + tdis * ddy[dir];
        tx = p_ptr->px + tdis * ddx[dir];

        if (ty < 0)
            ty = 0;
        else if (ty >= p_ptr->cur_map_hgt)
            ty = p_ptr->cur_map_hgt - 1;

        if (tx < 0)
            tx = 0;
        else if (tx >= p_ptr->cur_map_wid)
            tx = p_ptr->cur_map_wid - 1;
    }

    /* Warn before ranged attacks that might hit fleeing enemies (Oath of Valor) */
    if (abort_for_valorous_ranged_path(tdis, ty, tx))
        return;

    /* Handle player fear */
    if (p_ptr->afraid)
    {
        /* Message */
        msg_print("You are too afraid to aim properly!");

        /* Done */
        return;
    }

    if (cave_m_idx[ty][tx] > 0)
    {
        m_ptr = &mon_list[cave_m_idx[ty][tx]];
        r_ptr = &r_info[m_ptr->r_idx];

        if (r_ptr->flags1 & (RF1_PEACEFUL))
        {
            char m_name[80];
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            msg_format("You stop before you hit %s.", m_name);

            return;
        }

        if (abort_for_mercy(m_ptr))
        {
            return;
        }
    }

    /* Set dummy variables to pass to project_path (so it doesn't clobber the real ones). */
    ty2 = ty;
    tx2 = tx;

    u32b path_flg = PROJECT_THRU;
    if (spatial_target)
        path_flg = 0;

    /* DEBUG: Log throw parameters before path calculation */
    log_debug("do_cmd_throw: dir=%d py=%d px=%d ty=%d tx=%d tdis=%d spatial=%d",
        dir, p_ptr->py, p_ptr->px, ty, tx, tdis, spatial_target ? 1 : 0);

    /* Calculate the path */
    path_n = project_path(
        path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, path_flg);
    path_n = ABS(path_n);

    /* DEBUG: Log path result */
    log_debug("do_cmd_throw: path_n=%d after project_path", path_n);
    if (path_n > 0)
    {
        log_debug("do_cmd_throw: first grid=(%d,%d) last grid=(%d,%d)",
            GRID_Y(path_g[0]), GRID_X(path_g[0]),
            GRID_Y(path_g[path_n-1]), GRID_X(path_g[path_n-1]));
    }

    if (path_n <= 0)
    {
        msg_print("You cannot throw there.");
        return;
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain a local object */
    object_copy(i_ptr, o_ptr);

    /* Single object */
    i_ptr->number = 1;

    /* Set pickup on thrown item */
    i_ptr->pickup = true;
    if ((original_slot == INVEN_QUIVER1) || (original_slot == INVEN_QUIVER2))
        i_ptr->pickup_slot = original_slot;
    else
        i_ptr->pickup_slot = -1;

    /* Reduce and describe inventory */
    if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
    }

    /* Reduce and describe floor item */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_optimize(0 - item);
    }

    /* Description */
    object_desc(o_name, sizeof(o_name), i_ptr, false, 3);

    /* Find the color and symbol for the object for throwing */
    missile_attr = object_attr(i_ptr);
    missile_char = object_char(i_ptr);
    treat_as_throwing = player_can_treat_as_throwing_flags(i_ptr, f3);

    attack_mod = p_ptr->skill_use[S_MEL] + i_ptr->att;

    // subtract out the melee weapon's bonus (as we had already accounted for
    // it)
    attack_mod -= (&inventory[INVEN_WIELD])->att;
    attack_mod -= axe_bonus(&inventory[INVEN_WIELD]);
    attack_mod -= polearm_bonus(&inventory[INVEN_WIELD]);

    /* Weapons that are not good for throwing are much less accurate */
    if (!treat_as_throwing)
    {
        attack_mod -= 5;
    }

    // give people their weapon affinity bonuses if the weapon is thrown
    attack_mod += axe_bonus(i_ptr);
    attack_mod += polearm_bonus(i_ptr);

    if (p_ptr->active_ability[S_MEL][MEL_THROWING] && treat_as_throwing)
        attack_mod += 1;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Project along the path */
    for (i = 0; i < path_n; ++i)
    {
        int ny = GRID_Y(path_g[i]);
        int nx = GRID_X(path_g[i]);

        log_debug("do_cmd_throw: loop i=%d ny=%d nx=%d cave_floor=%d cave_m_idx=%d",
            i, ny, nx, cave_floor_bold(ny, nx) ? 1 : 0, cave_m_idx[ny][nx]);

        /* Hack -- Stop before hitting walls */
        if (!cave_floor_bold(ny, nx))
        {
            hit_wall = true;
            log_debug("do_cmd_throw: hit wall at i=%d, breaking loop before updating y,x");

            // Show collision
            /* Only do visuals if the player can "see" the missile */
            if (panel_contains(ny, nx))
            {
                /* Visual effects */
                print_rel('*', TERM_L_WHITE, ny, nx);
                move_cursor_relative(ny, nx);
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
                lite_spot(ny, nx);
                Term_fresh();
            }

            /* Delay anyway for consistency */
            else
            {
                /* Pause anyway, for consistancy */
                Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
            }
            break;
        }

        /* Advance */
        x = nx;
        y = ny;

        /* Only do visuals if the player can "see" the missile */
        if (panel_contains(y, x) && player_can_see_bold(y, x))
        {
            /* Visual effects */
            print_rel(missile_char, missile_attr, y, x);
            move_cursor_relative(y, x);
            Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, msec);
            lite_spot(y, x);
            Term_fresh();
        }

        /* Delay anyway for consistency */
        else
        {
            /* Pause anyway, for consistancy */
            Term_xtra(TERM_XTRA_DELAY, msec);
        }

        /* Handle monster */
        if (cave_m_idx[y][x] > 0)
        {
            m_ptr = &mon_list[cave_m_idx[y][x]];
            r_ptr = &r_info[m_ptr->r_idx];

            bool potion_effect = false;
            int pdam = 0;
            bool fatal_blow = false;

            // Determine the player's attack score after all modifiers
            int stealth_bonus = stealth_melee_bonus(m_ptr, true);
            total_attack_mod = total_player_attack(m_ptr, attack_mod + stealth_bonus);
            if (p_ptr->active_ability[S_MEL][MEL_THROWING] && treat_as_throwing)
            {
                int dist = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
                total_attack_mod += dist / 10;
            }

            /* Monsters might notice */
            player_attacked = true;

            // Modifications for shots that go past the target or strike things
            // before the target...
            if ((dir == 5) && target_okay(tdis))
            {
                // if there is a specific target and this is not it, then
                // massively penalise
                if ((ty != y) || (tx != x))
                {
                    total_attack_mod = 0;
                }
            }
            // if it is just a shot in a direction and has already missed
            // something, then massively penalise
            else if (missed_monsters > 0)
            {
                total_attack_mod = 0;
            }
            // if it is a shot in a direction and this is the first monster
            else
            {
                /* Hack -- Track this monster race */
                if (m_ptr->ml)
                    monster_race_track(m_ptr->r_idx);

                /* Hack -- Track this monster */
                if (m_ptr->ml)
                    health_track(cave_m_idx[y][x]);

                /* Hack -- Target this monster */
                if (m_ptr->ml)
                    target_set_monster(cave_m_idx[y][x]);
            }

            // Determine the monster's evasion after all modifiers
            total_evasion_mod = total_monster_evasion(m_ptr, false);

            // No killing peaceful creatures
            if (r_ptr->flags1 & (RF1_PEACEFUL))
            {
                hit_result = 0;
            }
            else
            {
                /* Test for hit */
                hit_result = hit_roll(
                    total_attack_mod, total_evasion_mod, PLAYER, m_ptr, true);
            }

            /* If it hit... */
            if (hit_result > 0)
            {
                /* Note the collision */
                hit_body = true;

                /* Assume a default death */
                cptr note_dies = " dies.";

                /*Mark the monster as attacked by the player*/
                m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

                /* Some monsters get "destroyed" */
                if (monster_nonliving(r_ptr))
                {
                    /* Special note at death */
                    note_dies = " is destroyed.";
                }

                /* Apply special damage XXX XXX XXX */
                crit_bonus_dice = crit_bonus(
                    hit_result, i_ptr->weight, r_ptr, S_MEL, true, NULL, i_ptr);

                if (f3 & TR3_CUMBERSOME)
                {
                    crit_bonus_dice = 0;
                }

                slay_bonus_dice = slay_bonus(i_ptr, m_ptr, &noticed_flag);

                /* Calculate the damage from the thrown object */
                total_bonus_dice = crit_bonus_dice + slay_bonus_dice;
                total_ds = strength_modified_ds(i_ptr, 0);

                /* Penalise items that aren't made to be thrown */
                if (!treat_as_throwing)
                    total_ds /= 2;

                /* Can't have a negative number of sides */
                if (total_ds < 0)
                    total_ds = 0;

                dam = damroll(i_ptr->dd + total_bonus_dice, total_ds);
                
                /* Apply armor dice/sides curses/blessings */
                int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
                if (armor_dice_base < 0)
                    armor_dice_base = 0;
                int armor_dice = armor_dice_base + curse_flag_count_cur(CUR_MON_ARM_DICE);
                int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_count_cur(CUR_MON_ARM_SIDE);
                if (armor_dice < 0) armor_dice = 0;
                if (armor_sides < 1) armor_sides = 1;
                prt = damroll(armor_dice, armor_sides);

                prt_percent = prt_after_sharpness(i_ptr, &noticed_flag);
                prt = (prt * prt_percent) / 100;

                net_dam = dam - prt;

                // no negative damage
                if (net_dam < 0)
                    net_dam = 0;

                break_mercy_oath(m_ptr, net_dam);
                break_valorous_oath(m_ptr, net_dam, ATT_MAIN, -1);  // Direct thrown weapon

                /* Handle unseen monster */
                if (!(m_ptr->ml))
                {
                    /* Invisible monster */
                    msg_format("The %s finds a mark.", o_name);
                }

                /* Handle visible monster */
                else
                {
                    char m_name[80];

                    /* Get "the monster" or "it" */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                    if (p_ptr->active_ability[S_STL][STL_CRUEL_BLOW]
                        && (crit_bonus_dice > 0) && (net_dam > 0)
                        && !(r_ptr->flags1 & (RF1_RES_CRIT)))
                    {
                        int cruel_blow_multiplier
                            = (30 - (60 / (crit_bonus_dice + 2)));
                        if (skill_check(PLAYER, cruel_blow_multiplier,
                                monster_skill(m_ptr, S_WIL), m_ptr)
                            > 0)
                        {
                            msg_format("%s reels in pain!", m_name);

                            if (!(r_ptr->flags3 & (RF3_NO_CONF)))
                                m_ptr->confused += crit_bonus_dice + 1;

                            scare_onlooking_friends(m_ptr, -20);
                        }
                    }

                    // determine the punctuation for the attack ("...", ".", "!"
                    // etc)
                    attack_punctuation(punctuation, net_dam, crit_bonus_dice);

                    /* Message */
                    msg_format("The %s hits %s%s", o_name, m_name, punctuation);

                    /* Hack -- Track this monster race */
                    if (m_ptr->ml)
                        monster_race_track(m_ptr->r_idx);

                    /* Hack -- Track this monster */
                    if (m_ptr->ml)
                        health_track(cave_m_idx[y][x]);

                    /* Hack -- Target this monster */
                    if (m_ptr->ml)
                        target_set_monster(cave_m_idx[y][x]);
                }

                /*special effects sometimes reveal the kind of potion*/
                if (i_ptr->tval == TV_POTION)
                {
                    /*record monster hit points*/
                    pdam = m_ptr->hp;

                    msg_print("The bottle breaks.");

                    /*returns true if the damage has already been handled*/
                    potion_effect = (thrown_potion_effects(
                        i_ptr, &fatal_blow, cave_m_idx[y][x]));

                    /*check the change in monster hp*/
                    pdam -= m_ptr->hp;

                    /*monster could have been healed*/
                    if (pdam < 0)
                        pdam = 0;
                }

                // if a slay was noticed, then identify the weapon
                if (noticed_flag)
                {
                    ident_weapon_by_use(i_ptr, m_ptr, noticed_flag);
                }

                /* No negative net damage */
                if (net_dam < 0)
                    net_dam = 0;

                update_combat_rolls2(i_ptr->dd + total_bonus_dice, total_ds,
                    dam, armor_dice, armor_sides, prt, prt_percent, GF_HURT,
                    false);

                /* Hit the monster, unless a potion effect has already been done
                 */
                if (!potion_effect)
                {
                    fatal_blow = (mon_take_hit(
                        cave_m_idx[y][x], net_dam, note_dies, -1));
                }

                display_hit(y, x, net_dam, GF_HURT, fatal_blow);

                /* Still alive */
                if (!fatal_blow)
                {
                    // alert the monster, even if no damage was done
                    // (if damage was done, then it was alerted by
                    // mon_take_hit() )
                    if (net_dam == 0)
                    {
                        make_alert(m_ptr);
                    }

                    // Morgoth drops his iron crown if he is hit for 10 or more
                    // net damage twice
                    if ((m_ptr->r_idx == R_IDX_MORGOTH)
                        && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
                    {
                        if (net_dam >= 10)
                        {
                            if (p_ptr->morgoth_hits == 0)
                            {
                                msg_print("The force of your blow knocks the "
                                          "Iron Crown off "
                                          "balance.");
                                p_ptr->morgoth_hits++;
                            }
                            else if (p_ptr->morgoth_hits == 1)
                            {
                                drop_iron_crown(m_ptr,
                                    "You knock his crown from off his brow, "
                                    "and it "
                                    "falls to the ground nearby.");
                                p_ptr->morgoth_hits++;
                            }
                        }
                    }

                    /* Message if applicable*/
                    if ((!potion_effect) || (pdam > 0))
                        message_pain(cave_m_idx[y][x], (pdam ? pdam : net_dam));
                }
                /* Stop looking if a monster was hit */
                break;
            }
            else
            {
                /* we have missed a target, but could still hit something (with
                 * a penalty) */
                missed_monsters++;
            }
        }
    }

    // need to print this message even if the potion missed
    if (!hit_body && (i_ptr->tval == TV_POTION))
        msg_print("The bottle breaks.");

    /* Have to set this here as well, just in case... */
    /* Monsters might notice */
    player_attacked = true;

    /* Chance of breakage (during attacks) */
    j = breakage_chance(i_ptr, hit_wall);

    /* throwing weapons have a lesser chance */
    if (treat_as_throwing)
        j /= 4;

    /* DEBUG: Log final drop position */
    log_debug("do_cmd_throw: dropping at y=%d x=%d (player at %d,%d) hit_body=%d",
        y, x, p_ptr->py, p_ptr->px, hit_body ? 1 : 0);

    /* Drop (or break) near that location */
    drop_near(i_ptr, j, y, x);

    // Break the truce if creatures see
    break_truce(false);
}

/*
 * Throw the item currently stored in the supplied slot.
 */
void do_cmd_throw_from_slot(int slot)
{
    throw_pending_slot = slot;
    do_cmd_throw(false);
}
