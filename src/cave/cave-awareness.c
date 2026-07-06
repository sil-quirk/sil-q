/* File: cave-awareness.c */

#include "cave-internal.h"

/*
 * Track a new monster
 */
void health_track(int m_idx)
{
    /* Track a new guy */
    p_ptr->health_who = m_idx;

    /* Redraw (later) */
    p_ptr->redraw |= (PR_HEALTHBAR);
    if (styled_monster_health_bars)
        p_ptr->window |= PW_MONSTER;
}

/*
 * Hack -- track the given monster race
 */
void monster_race_track(int r_idx)
{
    // don't track when hallucinating
    if (p_ptr->image)
        return;

    // don't track when raging
    if (p_ptr->rage)
        return;

    /* Save this monster ID */
    p_ptr->monster_race_idx = r_idx;

    /* Window stuff */
    p_ptr->window |= (PW_MONSTER);
}

/*
 * Hack -- track the given object kind
 */
void object_kind_track(int k_idx)
{
    // don't track when hallucinating
    if (p_ptr->image)
        return;

    /* Save this object ID */
    p_ptr->object_kind_idx = k_idx;

    /* Window stuff */
    p_ptr->window |= (PW_OBJECT);
}

/*
 * Something has happened to disturb the player.
 *
 * The first arg indicates a major disturbance, which affects stealth mode.
 *
 * The second arg is currently unused, but could induce output flush.
 *
 * All disturbance cancels repeated commands, resting, and running.
 */
void disturb(int stop_stealth, int unused_flag)
{
    /* Unused parameter */
    (void)unused_flag;

    /* Cancel SDL mouse auto-walk paths along with other auto-actions. */
    sdl_mouse_path_cancel();

    /* Cancel auto-commands */
    /* p_ptr->command_new = 0; */

    /* Cancel repeated commands */
    if (p_ptr->command_rep)
    {
        /* Cancel */
        p_ptr->command_rep = 0;

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel Resting */
    if (p_ptr->resting)
    {
        /* Cancel */
        p_ptr->resting = 0;

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel Smithing */
    if (p_ptr->smithing)
    {
        /* Cancel */
        p_ptr->smithing = 0;

        // Display a message
        msg_print("Your work is interrupted!");

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel Smithing */
    if (p_ptr->fletching)
    {
        // Display a message
        msg_print("Your work is interrupted!");
        log_debug("fletchery:interrupt turns_left=%d fletch_item=%d",
            p_ptr->fletching, p_ptr->fletch_item);

        finish_fletching(p_ptr->fletching);

        /* Cancel */
        p_ptr->fletching = 0;

        /* Redraw the state (later) */
        p_ptr->redraw |= (PR_STATE);
    }

    /* Cancel running */
    if (p_ptr->running)
    {
        /* Cancel */
        p_ptr->running = 0;

        /* Check for new panel if appropriate */
        if (center_player && run_avoid_center)
            verify_panel();
    }

    /* Cancel stealth if requested */
    if (stop_stealth && p_ptr->stealth_mode)
    {
        // signal that it will be stopped at the end of the turn
        stop_stealth_mode = true;
    }

    /* Flush the input */
    flush();
}
