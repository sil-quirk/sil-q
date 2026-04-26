/* File: xtra2.c */

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

static void look_prt(bool use_story_font, cptr text, int row, int col)
{
    /* When story font is enabled, use story_print_text which handles proportional rendering */
    if (use_story_font) {
        log_debug("look_prt: Using story_print_text for: '%.50s'", text);
        story_print_text(row, col, 0, TERM_WHITE, text);
    } else {
        log_debug("look_prt: Using prt (mono) for: '%.50s'", text);
        prt(text, row, col);
    }
}

/*
 * The saving throw is a will skill check.
 *
 * Note that the player is resisting and thus wins ties.
 */
extern bool saving_throw(monster_type* m_ptr, int resistance)
{
    int player_score = p_ptr->skill_use[S_WIL];
    int difficulty;

    if (m_ptr == NULL)
        difficulty = 10;
    else
        difficulty = monster_skill(m_ptr, S_WIL);

    int original_difficulty = difficulty;
    difficulty -= 10 * resistance;

    log_trace("SAVING THROW DEBUG: original_difficulty=%d, resistance=%d, adjusted_difficulty=%d, player_will=%d", 
              original_difficulty, resistance, difficulty, player_score);

    if (skill_check(m_ptr, difficulty, player_score, PLAYER) > 0)
    {
        log_trace("SAVING THROW DEBUG: SAVE FAILED - skill check result > 0");
        return (false);
    }
    else
    {
        log_trace("SAVING THROW DEBUG: SAVE SUCCEEDED - skill check result <= 0");
        return (true);
    }
}

// Auxilliary function for the allow_player functions
bool allow_player_aux(monster_type* m_ptr, int player_flag, u32b ident_flag)
{
    int resistance = 0;  // Fixed: Changed from bool to int  //XXX is it correct. need to analyze logic

    if (player_flag > 0)
    {
        // possibly identify relevant items
        ident_resist(ident_flag);

        // makes things much easier
        resistance = player_flag;
    }

    log_trace("RESIST DEBUG: allow_player_aux called with player_flag=%d, resistance=%d", player_flag, resistance);

    bool save_result = saving_throw(m_ptr, resistance);
    log_trace("RESIST DEBUG: saving_throw returned %s", save_result ? "SUCCESS" : "FAILED");

    if (save_result)
        return (false);

    // Don't have the right resists or failed the save
    return (true);
}

/* Turin character resistance function - 70% chance to resist bad effects but becomes raged */
bool turin_resist_bad_effect(void)
{
    /* Check if player has Turin character flag */
    if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_WIL_TURIN))
        return (false);
    
    /* 70% chance to resist */
    if (one_in_(10) || one_in_(10) || one_in_(10))
        return (false);
    
    /* Turin resists! Apply rage */
    msg_print("Your will hardens against adversity!");
    (void)set_rage(p_ptr->rage + damroll(5, 4));
    
    /* 50% chance to become hallucinated when raged */
    if (one_in_(2))
    {
        msg_print("The rage clouds your vision!");
        (void)set_image(p_ptr->image + damroll(3, 4));
    }
    
    return (true);
}

/* Players with blindness resistance or who make their saving throw don't get
 * blinded */
bool allow_player_blind(monster_type* m_ptr)
{
    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
        return (false);
    
    return (allow_player_aux(m_ptr, p_ptr->resist_blind, TR2_RES_BLIND));
}

/*
 * Set "p_ptr->blind", notice observable changes
 *
 * Note the use of "PU_FORGET_VIEW" and "PU_UPDATE_VIEW", which are needed
 * because "p_ptr->blind" affects the "CAVE_SEEN" flag, and "PU_MONSTERS",
 * because "p_ptr->blind" affects monster visibility, and "PU_MAP", because
 * "p_ptr->blind" affects the way in which many cave grids are displayed.
 */
bool set_blind(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->blind)
        {
            msg_print("You are blind!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->blind)
        {
            msg_print("You can see again.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->blind = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw map and refresh side panels that lose their contents when the
     * screen is cleared by hallucination messages. */
    p_ptr->redraw |= (PR_MAP | PR_EXTRA | PR_STATE | PR_BASIC | PR_MISC);

    /* Redraw the "blind" */
    p_ptr->redraw |= (PR_BLIND);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/* Players with confusion resistance or who make their saving throw don't get
 * confused */
bool allow_player_confusion(monster_type* m_ptr)
{
    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
    {
        log_trace("CONFUSION DEBUG: Turin character resisted confusion");
        return (false);
    }
    
    bool result = allow_player_aux(m_ptr, p_ptr->resist_confu, TR2_RES_CONFU);
    log_trace("CONFUSION DEBUG: allow_player_confusion called with resist_confu=%d, result=%s", 
              p_ptr->resist_confu, result ? "ALLOW CONFUSION" : "RESIST CONFUSION");
    return result;
}

/*
 * Set "p_ptr->confused", notice observable changes
 */
bool set_confused(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->confused)
        {
            msg_print("You are confused!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->confused)
        {
            msg_print("You feel less confused now.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->confused = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw the "confused" */
    p_ptr->redraw |= (PR_CONFUSED);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->poisoned", notice observable changes
 *
 */
bool set_poisoned(int v)
{
    int new;
    int change;

    bool notice = false;

    /* Hack -- Force good values */
    new = (v > 100) ? 100 : (v < 0) ? 0 : v;

    change = new - p_ptr->poisoned;

    /* Increase poison */
    if (change > 0)
    {
        if (p_ptr->poisoned == 0)
        {
            if (change >= 20)
            {
                msg_print("You have been severely poisoned.");
                notice = true;
            }
            else if (change >= 10)
            {
                msg_print("You have been badly poisoned.");
                notice = true;
            }
            else
            {
                msg_print("You have been poisoned.");
                notice = true;
            }
        }
        else
        {
            if (change >= 20)
            {
                msg_print("You have been severely poisoned.");
                notice = true;
            }
            else if (change >= 10)
            {
                msg_print("You have been badly poisoned.");
                notice = true;
            }
            else
            {
                msg_print("You have been further poisoned.");
                notice = true;
            }
        }
    }
    /* Decrease poison */
    if (change < 0)
    {
        if ((new == 0) && (p_ptr->chp > 0))
        {
            msg_print("You recover from the poisoning.");
            disturb(0, 0);
            notice = true;
        }
        else if (-change > (p_ptr->poisoned + 4) / 5)
        {
            msg_print("You can feel the poison weakening.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->poisoned = new;

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Redraw the "poison" */
    p_ptr->redraw |= (PR_POISONED);

    /* Handle stuff */
    handle_stuff();

    /* No change */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Result */
    return (true);
}

/* Players with fear resistance or who make their saving throw don't get
 * terrified */
bool allow_player_fear(monster_type* m_ptr)
{
    // rage is incompatible with fear -- more than just a resistance
    if (p_ptr->rage)
        return (false);
    
    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
        return (false);
    
    return (allow_player_aux(m_ptr, p_ptr->resist_fear, TR2_RES_FEAR));
}

/*
 * Set "p_ptr->afraid", notice observable changes
 */
bool set_afraid(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->afraid)
        {
            msg_print("You are terrified!");
            notice = true;
            if (singing(SNG_CHALLENGE))
            {
                /* Stop singing */
                change_song(SNG_NOTHING);
            }
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->afraid)
        {
            msg_print("You feel bolder now.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->afraid = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw the "afraid" */
    p_ptr->redraw |= (PR_AFRAID);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/* Players with free action or who make their saving throw don't get entranced
 */
bool allow_player_entrancement(monster_type* m_ptr)
{
    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
        return (false);
    
    return (allow_player_aux(m_ptr, p_ptr->free_act, TR2_FREE_ACT));
}

/*
 * Set "p_ptr->entranced", notice observable changes
 */
bool set_entranced(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->entranced)
        {
            msg_print("You fall into a deep trance!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->entranced)
        {
            msg_print("The trance is broken!");
            p_ptr->was_entranced = true;
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->entranced = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw the state */
    p_ptr->redraw |= (PR_STATE);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/* Players with resist hallucination or who make their saving throw don't
 * hallucinate */
bool allow_player_image(monster_type* m_ptr)
{
    return (allow_player_aux(m_ptr, p_ptr->resist_hallu, TR2_RES_HALLU));
}

/*
 * Set "p_ptr->image", notice observable changes
 *
 * Note the use of "PR_MAP", which is needed because "p_ptr->image" affects
 * the way in which monsters, objects, and some normal grids, are displayed.
 */
bool set_image(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->image)
        {
            msg_print("Fantastic visions appear before your eyes.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->image)
        {
            msg_print("You can see clearly again.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->image = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->fast", notice observable changes
 */
bool set_fast(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->fast)
        {
            msg_print("You feel yourself moving faster!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->fast)
        {
            msg_print("You feel yourself slow down.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->fast = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/* Players with free action or who make their saving throw don't get slowed */
bool allow_player_slow(monster_type* m_ptr)
{
    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
        return (false);
    
    return (allow_player_aux(m_ptr, p_ptr->free_act, TR2_FREE_ACT));
}

/*
 * Set "p_ptr->slow", notice observable changes
 */
bool set_slow(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->slow)
        {
            msg_print("You feel yourself moving slower!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->slow)
        {
            msg_print("You feel yourself speed up.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->slow = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->rage", notice observable changes
 */
bool set_rage(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    bool was_raging = (p_ptr->rage > 0);
    bool will_rage = (v > 0);

    /* Apply the new value before kicking off redraws so map_info sees it */
    p_ptr->rage = v;

    /* Rage just started */
    if (will_rage && !was_raging)
    {
        msg_print("You burst into a furious rage!");
        notice = true;

        p_ptr->redraw |= (PR_MAP | PR_STATE);
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS
            | PU_DISTANCE);

        if (p_ptr->stealth_mode)
        {
            msg_print("Your fury shatters your stealth!");
            p_ptr->stealth_mode = false;
            stop_stealth_mode = false;
            p_ptr->update |= (PU_BONUS);
            p_ptr->redraw |= (PR_SPEED);
        }

        /* Turin character has 50% chance to become hallucinated when raged */
        if ((c_info[p_ptr->pcharacter].flags_u & UNQ_WIL_TURIN) && one_in_(2))
        {
            msg_print("The rage clouds your vision!");
            (void)set_image(p_ptr->image + damroll(3, 4));
        }
    }

    /* Rage just ended */
    else if (!will_rage && was_raging)
    {
        msg_print("Your rage subsides.");
        notice = true;

        // do_res_stat(A_STR, 1);
        // do_res_stat(A_CON, 1);

        p_ptr->redraw |= (PR_MAP | PR_STATE);
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS
            | PU_DISTANCE);
    }

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->tmp_str", notice observable changes
 */
bool set_tmp_str(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->tmp_str)
        {
            msg_print("You feel stronger.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->tmp_str)
        {
            msg_print("Your strength returns to normal.");
            do_res_stat(A_STR, 3);
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->tmp_str = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->tmp_dex", notice observable changes
 */
bool set_tmp_dex(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->tmp_dex)
        {
            msg_print("You feel more agile.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->tmp_dex)
        {
            msg_print("Your dexterity returns to normal.");
            do_res_stat(A_DEX, 3);
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->tmp_dex = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->tmp_con", notice observable changes
 */
bool set_tmp_con(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->tmp_con)
        {
            msg_print("You feel more resilient.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->tmp_con)
        {
            msg_print("Your constitution returns to normal.");
            do_res_stat(A_CON, 3);
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->tmp_con = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->tmp_gra", notice observable changes
 */
bool set_tmp_gra(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->tmp_gra)
        {
            msg_print("You feel more attuned to the world.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->tmp_gra)
        {
            msg_print("Your grace returns to normal.");
            do_res_stat(A_GRA, 3);
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->tmp_gra = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->tmp_per", notice observable changes
 */
bool set_tmp_per(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->tmp_per)
        {
            msg_print("You feel your perceptions sharpen.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->tmp_per)
        {
            msg_print("Your perception returns to normal.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->tmp_per = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->tim_invis", notice observable changes
 *
 * Note the use of "PU_MONSTERS", which is needed because
 * "p_ptr->tim_image" affects monster visibility.
 */
bool set_tim_invis(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        if (!p_ptr->tim_invis)
        {
            msg_print("Your vision sharpens.");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->tim_invis)
        {
            msg_print("Your eyes feel less sensitive.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->tim_invis = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update the monsters XXX */
    p_ptr->update |= (PU_MONSTERS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 *
 * Set "p_ptr->darkened", notice observable changes
 *
 * Note the use of "PU_MONSTERS", which is needed because
 * "p_ptr->darkened" affects monster visibility.
 */
bool set_darkened(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* If Increasing */
    if (v > p_ptr->darkened)
    {
        if (!p_ptr->blind)
        {
            msg_print("Your light dims.");
            notice = true;
        }
    }

    /* If Finished */
    if (v == 0)
    {
        if (p_ptr->darkened && !p_ptr->blind)
        {
            msg_print("Your light grows brighter again.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->darkened = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update the monsters XXX */
    p_ptr->update |= (PU_MONSTERS);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->oppose_fire", notice observable changes
 */
bool set_oppose_fire(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        /*then check if player has permanent resist to fire*/
        if ((p_ptr->resist_fire > 1) && (!p_ptr->oppose_fire))
        {
            msg_print("You feel more resistant to fire!");
            notice = true;
        }

        /*if player has neither*/
        else if (!p_ptr->oppose_fire)
        {
            msg_print("You feel resistant to fire!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->oppose_fire)
        {
            msg_print("You feel less resistant to fire.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->oppose_fire = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw resistances */
    p_ptr->redraw |= (PR_RESIST);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->oppose_cold", notice observable changes
 */
bool set_oppose_cold(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        /*then check if player has permanent resist to cold*/
        if ((p_ptr->resist_cold > 1) && (!p_ptr->oppose_cold))
        {
            msg_print("You feel more resistant to cold!");
            notice = true;
        }
        /*if player has neither*/
        else if (!p_ptr->oppose_cold)
        {
            msg_print("You feel resistant to cold!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->oppose_cold)
        {
            msg_print("You feel less resistant to cold.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->oppose_cold = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw resistances */
    p_ptr->redraw |= (PR_RESIST);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->oppose_pois", notice observable changes
 */
bool set_oppose_pois(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

    /* Open */
    if (v)
    {
        /*Then check if player has permanent resist to poison*/
        if ((p_ptr->resist_pois > 1) && (!p_ptr->oppose_pois))
        {
            msg_print("You feel more resistant to poison!");
            notice = true;
        }

        /*if player doesn't have permanent resistance to poison*/
        else if (!p_ptr->oppose_pois)
        {
            msg_print("You feel resistant to poison!");
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->oppose_pois)
        {
            msg_print("You feel less resistant to poison.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->oppose_pois = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Redraw resistances */
    p_ptr->redraw |= (PR_RESIST);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/* Players with stun resistance or who make their saving throw don't get stunned
 */
bool allow_player_stun(monster_type* m_ptr)
{
    return (allow_player_aux(m_ptr, p_ptr->resist_stun, TR2_RES_STUN));
}

/*
 * Set "p_ptr->stun", notice observable changes
 *
 * Note the special code to only notice "range" changes.
 */
bool set_stun(int v)
{
    int old_aux, new_aux;

    bool notice = false;

    /*  Don't increase stunning if stunning value is greater than 100.
     *  this is an effort to eliminate the "knocked out" instadeath.
     */
    if ((p_ptr->stun > 100) && (v > p_ptr->stun))
        return (false);

    /* Hack -- Force sane values */
    v = (v > 105) ? 105 : (v < 0) ? 0 : v;

    /* Knocked out */
    if (p_ptr->stun > 100)
    {
        old_aux = 3;
    }

    /* Heavy stun */
    else if (p_ptr->stun > 50)
    {
        old_aux = 2;
    }

    /* Stun */
    else if (p_ptr->stun > 0)
    {
        old_aux = 1;
    }

    /* None */
    else
    {
        old_aux = 0;
    }

    /* Knocked out */
    if (v > 100)
    {
        p_ptr->blind = MAX(p_ptr->blind, 2);
        new_aux = 3;
    }

    /* Heavy stun */
    else if (v > 50)
    {
        new_aux = 2;
    }

    /* Stun */
    else if (v > 0)
    {
        new_aux = 1;
    }

    /* None */
    else
    {
        new_aux = 0;
    }

    /* Increase stun */
    if (new_aux > old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
        /* Stun */
        case 1:
        {
            msg_print("You have been stunned.");
            break;
        }

        /* Heavy stun */
        case 2:
        {
            msg_print("You have been heavily stunned.");
            break;
        }

        /* Knocked out */
        case 3:
        {
            msg_print("You have been knocked out.");
            break;
        }
        }

        /* Notice */
        notice = true;
    }

    /* Decrease stun */
    else if (new_aux < old_aux)
    {
        // waking up from Knock Out
        if (old_aux == 3)
        {
            msg_print("You wake up.");

            // undo the temporary blinding if waking up from KO
            p_ptr->blind = MAX(p_ptr->blind - 1, 0);
        }

        /* Describe the state */
        switch (new_aux)
        {
        /* None */
        case 0:
        {
            msg_print("You are no longer stunned.");
            disturb(0, 0);
            break;
        }
        }

        /* Notice */
        notice = true;
    }

    /* Use the value */
    p_ptr->stun = v;

    /* No change */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Redraw the "stun" */
    p_ptr->redraw |= (PR_STUN);

    /* Redraw resistances */
    p_ptr->redraw |= (PR_RESIST);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->cut", notice observable changes
 *
 * Note the special code to only notice "range" changes.
 */
bool set_cut(int v)
{
    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 100) ? 100 : (v < 0) ? 0 : v;

    /* Increase cut */
    if (v > p_ptr->cut)
    {
        if (v - p_ptr->cut >= 20)
        {
            msg_print("You have been given a severe cut.");
            notice = true;
        }
        else if (v - p_ptr->cut >= 10)
        {
            msg_print("You have been given a deep cut.");
            notice = true;
        }
        else
        {
            msg_print("You have been given a cut.");
            notice = true;
        }
    }
    /* Decrease cut */
    if (v < p_ptr->cut)
    {
        if ((v == 0) && (p_ptr->chp > 0))
        {
            msg_print("The bleeding stops.");
            disturb(0, 0);
            notice = true;
        }
        else if ((p_ptr->cut - v) > (p_ptr->cut + 4) / 5)
        {
            msg_print("The bleeding slows.");
            notice = true;
        }
    }

    /* Use the value */
    p_ptr->cut = v;

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Redraw the "cut" */
    p_ptr->redraw |= (PR_CUT);

    /* Handle stuff */
    handle_stuff();

    /* No change */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Result */
    return (true);
}

/*
 * Set "p_ptr->food", notice observable changes
 */
bool set_food(int v)
{
    int old_aux, new_aux;

    bool notice = false;

    /* Hack -- Force good values */
    v = (v > 20000) ? 20000 : (v < 0) ? 0 : v;

    /* Starving */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        old_aux = 0;
    }

    /* Weak */
    else if (p_ptr->food < PY_FOOD_WEAK)
    {
        old_aux = 1;
    }

    /* Hungry */
    else if (p_ptr->food < PY_FOOD_ALERT)
    {
        old_aux = 2;
    }

    /* Normal */
    else if (p_ptr->food < PY_FOOD_FULL)
    {
        old_aux = 3;
    }

    /* Full */
    else if (p_ptr->food < PY_FOOD_MAX)
    {
        old_aux = 4;
    }

    /* Replete */
    else
    {
        old_aux = 5;
    }

    /* Starving */
    if (v < PY_FOOD_STARVE)
    {
        new_aux = 0;
    }

    /* Weak */
    else if (v < PY_FOOD_WEAK)
    {
        new_aux = 1;
    }

    /* Hungry */
    else if (v < PY_FOOD_ALERT)
    {
        new_aux = 2;
    }

    /* Normal */
    else if (v < PY_FOOD_FULL)
    {
        new_aux = 3;
    }

    /* Full */
    else if (v < PY_FOOD_MAX)
    {
        new_aux = 4;
    }

    /* Replete */
    else
    {
        new_aux = 5;
    }

    /* Food increase */
    if (new_aux > old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
        /* Weak */
        case 1:
        {
            msg_print("You are still weak.");
            break;
        }

        /* Hungry */
        case 2:
        {
            msg_print("You are still hungry.");
            break;
        }

        /* Normal */
        case 3:
        {
            msg_print("You are no longer hungry.");
            break;
        }

        /* Full */
        case 4:
        {
            msg_print("You are full!");
            break;
        }

        /* Replete */
        case 5:
        {
            msg_print("You are as full as you can be.");
            break;
        }
        }

        /* Change */
        notice = true;
    }

    /* Food decrease */
    else if (new_aux < old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
        /* Starving */
        case 0:
        {
            msg_print("You are beginning to starve!");
            break;
        }

        /* Weak */
        case 1:
        {
            msg_print("You are getting weak from hunger!");
            break;
        }

        /* Hungry */
        case 2:
        {
            msg_print("You are getting hungry.");
            break;
        }

        /* Normal */
        case 3:
        {
            msg_print("You are no longer full.");
            break;
        }

        /* Full */
        case 4:
        {
            msg_print("You feel comfortably full.");
            break;
        }
        }

        /* Change */
        notice = true;

        // maybe identify hunger / sustenance
        ident_hunger();
    }

    /* Use the value */
    p_ptr->food = v;

    /* Nothing to notice */
    if (!notice)
        return (false);

    /* Disturb */
    disturb(0, 0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Redraw hunger */
    p_ptr->redraw |= (PR_HUNGER);

    /* Handle stuff */
    handle_stuff();

    /* Result */
    return (true);
}

/*
 * Falling damage. 3d4 for one floor, 6d4 for two floors.
 */
void falling_damage(bool stun)
{
    int dice = 0;
    int dam;

    cptr message;

    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)
    {
        if (p_ptr->depth >= MORGOTH_DEPTH - 1)
            dice = 3; // as this means you will only fall one floor
        else
            dice = 6;
        message = "falling down a chasm";
    }
    else if (cave_stair_bold(p_ptr->py, p_ptr->px))
    {
        dice = 3;
        message = "a collapsing stair";
    }
    else
    {
        dice = 3;
        message = "a collapsing floor";
    }

    // calculate the damage
    dam = damroll(dice, 4);

    if (dice > 0)
    {
        // update the combat rolls window
        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, message);
    }

    if (stun && allow_player_stun(NULL))
    {
        set_stun(p_ptr->stun + dam * 5);
    }

    // reset staircasiness
    p_ptr->staircasiness = 0;
}

/*
 * Advance experience levels and print experience
 */
void check_experience(void)
{
    /* Hack -- lower limit */
    if (p_ptr->exp < 0)
        p_ptr->exp = 0;

    /* Hack -- lower limit */
    if (p_ptr->new_exp < 0)
        p_ptr->new_exp = 0;

    /* Hack -- upper limit */
    if (p_ptr->exp > PY_MAX_EXP)
        p_ptr->exp = PY_MAX_EXP;

    /* Hack -- upper limit */
    if (p_ptr->new_exp > PY_MAX_EXP)
        p_ptr->new_exp = PY_MAX_EXP;

    /* Hack -- maintain "max" experience */
    if (p_ptr->new_exp > p_ptr->exp)
        p_ptr->new_exp = p_ptr->exp;

    /* Redraw experience */
    p_ptr->redraw |= (PR_EXP);

    /* Redraw stuff */
    redraw_stuff();
}

/*
 * Gain experience
 */
void gain_exp(s32b amount)
{
    if (birth_fixed_exp)
    {
        return;
    }

    /* Gain some experience */
    p_ptr->exp += amount;
    p_ptr->new_exp += amount;

    /* Check Experience */
    check_experience();
}

/*
 * Lose experience
 */
void lose_exp(s32b amount)
{
    /* Never drop below zero experience */
    if (amount > p_ptr->new_exp)
        amount = p_ptr->new_exp;

    /* Lose some experience */
    p_ptr->new_exp -= amount;
    p_ptr->exp -= amount;

    /* Check Experience */
    check_experience();
}

/*
 *  Choose the location of a random staircase on the level
 */
bool random_stair_location(int* sy, int* sx)
{
    int stair_y[100];
    int stair_x[100];
    int stair_num = 0;
    int y, x;

    // Note all the stairs
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_stair_bold(y, x))
            {
                stair_y[stair_num] = y;
                stair_x[stair_num] = x;
                if (stair_num < 99)
                    stair_num++;
            }
        }
    }

    // If no valid stairs are found, then bail out (paranoia)
    if (stair_num == 0)
    {
        return (false);
    }

    // Choose a random stair
    stair_num = rand_int(stair_num);
    *sy = stair_y[stair_num];
    *sx = stair_x[stair_num];

    return (true);
}

/*
 * Break the truce in Morgoth's throne room
 */
extern void break_truce(bool obvious)
{
    int i;

    monster_type* m_ptr = NULL; // default to soothe compiler warnings

    char m_name[80];

    if (p_ptr->truce)
    {
        /* Scan all other monsters */
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore dead monsters */
            if (!m_ptr->r_idx)
                continue;

            // Ignore monsters out of line of sight
            if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
                continue;

            // Ignore unalert monsters
            if (m_ptr->alertness < ALERTNESS_ALERT)
                continue;

            /* Get the monster name (using 'something' for hidden creatures) */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

            p_ptr->truce = false;
        }

        if (obvious)
            p_ptr->truce = false;

        if (!p_ptr->truce)
        {
            if (!obvious)
            {
                msg_format(
                    "%^s lets out a cry! The tension is broken.", m_name);

                /* Make a lot of noise */
                update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
                monster_perception(false, false, -10);
            }
            else
            {
                msg_print("The tension is broken.");
            }

            /* Scan all other monsters */
            for (i = mon_max - 1; i >= 1; i--)
            {
                /* Access the monster */
                m_ptr = &mon_list[i];

                /* Ignore dead monsters */
                if (!m_ptr->r_idx)
                    continue;

                /* Mark minimum desired range for recalculation */
                m_ptr->min_range = 0;
            }
        }
    }
}

/*
 * Checks whether monsters on two separate coordinates are of the same type
 * (i.e. the same letter or share an RF3_ race flag)
 */
bool similar_monsters(int m1y, int m1x, int m2y, int m2x)
{
    monster_type* m_ptr;
    monster_race* r_ptr;
    monster_type* n_ptr;
    monster_race* nr_ptr;

    /*first check if there are monsters on both coordinates*/
    if (!(cave_m_idx[m1y][m1x] > 0))
        return (false);
    if (!(cave_m_idx[m2y][m2x] > 0))
        return (false);

    /* Access monster 1*/
    m_ptr = &mon_list[cave_m_idx[m1y][m1x]];
    r_ptr = &r_info[m_ptr->r_idx];

    /* Access monster 2*/
    n_ptr = &mon_list[cave_m_idx[m2y][m2x]];
    nr_ptr = &r_info[n_ptr->r_idx];

    /* Monsters have the same symbol */
    if (r_ptr->d_char == nr_ptr->d_char)
        return (true);

    /*
     * Same race (we are not checking all RF3 types
     * because that would be true at
     * the symbol check
     */
    if ((r_ptr->flags3 & (RF3_DRAGON)) && (nr_ptr->flags3 & (RF3_DRAGON)))
        return (true);
    if ((r_ptr->flags3 & (RF3_SERPENT)) && (nr_ptr->flags3 & (RF3_SERPENT)))
        return (true);
    if ((r_ptr->flags3 & (RF3_HORROR)) && (nr_ptr->flags3 & (RF3_HORROR)))
        return (true);

    /*Not the same*/
    return (false);
}

/*
 *  Cause a temporary penalty to morale in monsters of the same type who can see
 * the specified monster. (Used when it dies and for cruel blow).
 */
void scare_onlooking_friends(const monster_type* m_ptr, int amount)
{
    int i;
    int fy, fx, y, x;

    /* Location of main monster */
    fy = m_ptr->fy;
    fx = m_ptr->fx;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* n_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[n_ptr->r_idx];

        /* Skip dead monsters */
        if (!n_ptr->r_idx)
            continue;

        /* Location of other monster */
        y = n_ptr->fy;
        x = n_ptr->fx;

        // Only consider alert monsters of the same type in line of sight
        if ((n_ptr->alertness >= ALERTNESS_ALERT)
            && !(r_ptr->flags3 & (RF3_NO_FEAR))
            && similar_monsters(fy, fx, y, x) && los(y, x, fy, fx))
        {
            // cause a temporary morale penalty
            n_ptr->tmp_morale += amount;
        }
    }

    return;
}

/*
 * Create a chosen artefact (mainly for the death of a particular unique)
 */
static u32b monster_drop_source_ident(const monster_race* r_ptr)
{
    u32b source_ident = 0;

    if (!r_ptr)
        return 0;

    if (r_ptr->flags3 & RF3_DRAGON)
        source_ident |= IDENT_DRAGON_DROP;
    if (r_ptr->flags1 & RF1_UNIQUE)
        source_ident |= IDENT_UNIQUE_DROP;

    return source_ident;
}

static void create_chosen_artefact_marked(
    byte name1, int y, int x, bool identify, u32b source_ident)
{
    object_type* i_ptr;
    object_type object_type_body;
    artefact_type* a_ptr = &a_info[name1];

    log_trace("create_chosen_artefact: Creating artifact %d at (%d,%d), identify=%s", name1, y, x, identify ? "true" : "false");

    // Don't generate it if one has already been generated
    if (a_ptr->cur_num > 0) {
        log_trace("create_chosen_artefact: Artifact %d already exists (cur_num=%d)", name1, a_ptr->cur_num);
        return;
    }

    // Don't generate it if it's reserved for Tulkas quest (unless this IS the quest reward)
    if (valar_reserved_artifacts && valar_reserved_artifacts[name1] && 
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE) {
        log_trace("create_chosen_artefact: Artifact %d is reserved for Tulkas quest", name1);
        return;
    }

    // Don't generate it in no-artefact games, with one obvious exception
    if (birth_no_artefacts && (name1 != ART_MORGOTH_3)) {
        log_trace("create_chosen_artefact: Skipping artifact %d due to no-artifacts birth option", name1);
        return;
    }

    log_trace("create_chosen_artefact: All checks passed, creating artifact %d", name1);

    /* Get local object */
    i_ptr = &object_type_body;

    log_trace("create_chosen_artefact: Preparing base object for artifact %d (tval=%d, sval=%d)", name1, a_ptr->tval, a_ptr->sval);

    /* Mega-Hack -- Prepare the base object for the artefact */
    object_prep(i_ptr, lookup_kind(a_ptr->tval, a_ptr->sval));

    log_trace("create_chosen_artefact: Base object prepared, applying artifact magic");

    /* Mega-Hack -- Mark this item as the artefact */
    i_ptr->name1 = name1;

    /* Mega-Hack -- Actually create the artefact */
    apply_magic(i_ptr, -1, true, true, true, true);

    if (!character_dungeon)
        source_ident |= IDENT_HOARD_DROP;
    i_ptr->ident |= (source_ident & (IDENT_DRAGON_DROP | IDENT_UNIQUE_DROP));
    i_ptr->ident |= (source_ident & IDENT_HOARD_DROP);

    log_trace("create_chosen_artefact: Magic applied successfully");

    // Identify it if desired
    if (identify)
    {
        log_trace("create_chosen_artefact: Identifying artifact %d", name1);
        object_aware(i_ptr);
        object_known(i_ptr);
        log_trace("create_chosen_artefact: Artifact identified");
    }

    log_trace("create_chosen_artefact: About to drop artifact %d at (%d,%d)", name1, y, x);

    /* Drop it in the dungeon */
    drop_near(i_ptr, -1, y, x);
    
    log_trace("create_chosen_artefact: Successfully created and dropped artifact %d", name1);
}

extern void create_chosen_artefact(byte name1, int y, int x, bool identify)
{
    create_chosen_artefact_marked(name1, y, x, identify, 0);
}

/*
 * Drops the objects
 */
int drop_loot(monster_type* m_ptr)
{
    int j, y, x;

    int dump_item = 0;

    int number = 0;

    s16b this_o_idx, next_o_idx = 0;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    bool visible = (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)));

    bool chest = (r_ptr->flags1 & (RF1_DROP_CHEST)) ? true : false;
    bool good = false;
    bool great = false;
    bool superb = false;
    bool artefact = false;

    object_type* i_ptr;
    object_type object_type_body;
    u32b source_ident = monster_drop_source_ident(r_ptr);

    int original_object_level = object_level;

    if (r_ptr->flags1 & (RF1_DROP_GOOD))
    {
        good = true;
    }
    if (r_ptr->flags1 & (RF1_DROP_GREAT))
    {
        great = true;
    }
    if (r_ptr->flags2 & (RF2_DROP_SUPERB))
    {
        superb = true;
    }
    if (r_ptr->flags3 & (RF3_DROP_ARTEFACT))
    {
        artefact = true;
    }

    /* Get the location */
    y = m_ptr->fy;
    x = m_ptr->fx;

    /* Stone creatures turn into rubble */
    if ((r_ptr->flags3 & (RF3_STONE)) && !cave_stair_bold(y, x))
    {
        cave_set_feat(y, x, FEAT_RUBBLE);
    }

    /* Drop objects being carried */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /*Remove the mark to hide when monsters carry this object*/
        o_ptr->ident &= ~(IDENT_HIDE_CARRY);

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Paranoia */
        o_ptr->held_m_idx = 0;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Copy the object */
        object_copy(i_ptr, o_ptr);
        i_ptr->ident |= source_ident;

        /* Delete the object */
        delete_object_idx(this_o_idx);

        /* Drop it */
        drop_near(i_ptr, -1, y, x);
    }

    /* Forget objects */
    m_ptr->hold_o_idx = 0;

    /* Mega-Hack -- drop special treasures */
    if (r_ptr->flags1 & (RF1_DROP_CHOSEN))
    {
        /* Drop Morgoth's treasures */
        if (m_ptr->r_idx == R_IDX_MORGOTH)
        {
            // create the Massive Hammer 'Grond'
            create_chosen_artefact_marked(ART_GROND, y, x, true, source_ident);

            // create the Iron Crown of Morgoth
            create_chosen_artefact_marked(
                ART_MORGOTH_3, y, x, true, source_ident);
        }
        // Drop Calris from Gothmog
        else if (m_ptr->r_idx == R_IDX_GOTHMOG)
        {
            // create the Greatsword 'Calris'
            create_chosen_artefact_marked(
                ART_CALRIS, y, x, false, source_ident);
        }
        // Drop Galvorn Armour of Maeglin
        else if (r_ptr->d_char == '@')
        {
            // create the Armour of Maeglin
            create_chosen_artefact_marked(
                ART_MAEGLIN, y, x, false, source_ident);
        }
        // Drop Iron Spear of Boldog
        else if (r_ptr->d_char == 'o')
        {
            // create the Armour of Maeglin
            create_chosen_artefact_marked(
                ART_BOLDOG, y, x, false, source_ident);
        }
        // Drop Glend
        else if (r_ptr->d_char == 'G')
        {
            // create the Greatsword 'Glend'
            create_chosen_artefact_marked(
                ART_GLEND, y, x, false, source_ident);
        }
        // Drop Wolf-Hame of Drauglin
        else if (r_ptr->d_char == 'C')
        {
            // create the Wolf-Hame of Draugluin
            create_chosen_artefact_marked(
                ART_DRAUGLUIN, y, x, false, source_ident);
        }
        // Drop Bat-Fell of Thuringwethil
        else if (r_ptr->d_char == 'v')
        {
            // create the Bet-Fell of Thuringwethil
            create_chosen_artefact_marked(
                ART_THURINGWETHIL, y, x, false, source_ident);
        }
    }

    /* Determine how much we can drop */
    if ((r_ptr->flags1 & (RF1_DROP_33)) && percent_chance(33))
        number++;
    if (r_ptr->flags1 & (RF1_DROP_100))
        number += 1;
    if (r_ptr->flags1 & (RF1_DROP_1D2))
        number += damroll(1, 2);
    if (r_ptr->flags1 & (RF1_DROP_2D2))
        number += damroll(2, 2);
    if (r_ptr->flags1 & (RF1_DROP_3D2))
        number += damroll(3, 2);
    if (r_ptr->flags1 & (RF1_DROP_4D2))
        number += damroll(4, 2);
    if (r_ptr->flags3 & (RF3_DROP_1D3))
        number += damroll(1, 3);

    /* DROP_ARTEFACT must always yield at least one drop slot. */
    if (artefact && number < 1)
        number = 1;

    // Favoured drops 1: arrows from archers
    if ((number > 0)
        && ((r_ptr->flags4 & (RF4_ARROW1)) || (r_ptr->flags4 & (RF4_ARROW2)))
        && percent_chance(r_ptr->freq_ranged / 2))
    {
        int depth_cap = player_generation_depth();
        int gen_depth = MIN(r_ptr->level, depth_cap);
        drop_profile arrow_profile;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        drop_profile_default(&arrow_profile);
        arrow_profile.weight_weapon = 0;
        arrow_profile.weight_armor = 0;
        arrow_profile.weight_jewelry = 0;
        arrow_profile.weight_supply = 100;
        arrow_profile.supply_potion = 0;
        arrow_profile.supply_herb = 0;
        arrow_profile.supply_gem = 0;
        arrow_profile.supply_staff = 0;
        arrow_profile.supply_light = 0;
        arrow_profile.supply_arrows = 100;
        arrow_profile.supply_tunneling = 0;

        if (!drop_generate_object_profiled(gen_depth, DROP_QUALITY_NORMAL,
                DROP_TYPE_NOT_DAMAGED, 0, false, &arrow_profile, i_ptr))
        {
            s16b k_idx = lookup_kind(TV_ARROW, SV_NORMAL_ARROW);
            object_prep(i_ptr, k_idx);
        }
        else if (i_ptr->tval != TV_ARROW)
        {
            s16b k_idx = lookup_kind(TV_ARROW, SV_NORMAL_ARROW);
            object_prep(i_ptr, k_idx);
        }

        i_ptr->number = damroll(2, 8);

        object_known(i_ptr);
        i_ptr->ident |= source_ident;

        /* Assume seen XXX XXX XXX */
        dump_item++;

        /* Drop it in the dungeon */
        drop_near(i_ptr, -1, y, x);

        // Drop one fewer object to compensate
        number--;
    }

    // Favoured drops 2: torches from easterlings
    if ((number > 0) && (r_ptr->d_char == '@') && (r_ptr->light == 1)
        && (easter_time() || one_in_(3)))
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        // Special treat for Easter time
        if (easter_time())
        {
            /* Hack	-- Give the player an object */
            /* Get the object_kind */
            s16b k_idx = lookup_kind(TV_FOOD, rand_int(9));

            /* Prepare the item */
            object_prep(i_ptr, k_idx);
        }

        // Normally just go for a torch
        else
        {
            /* Use unified light-source drop logic (A: schedule gating + min-depth penalty). */
            int depth_cap = player_generation_depth();
            int gen_depth = MIN(r_ptr->level, depth_cap);
            if (!drop_generate_object(gen_depth, DROP_QUALITY_NORMAL, DROP_TYPE_TORCHES,
                    false, i_ptr))
            {
                /* Fallback: always try to give a basic torch */
                s16b k_idx = lookup_kind(TV_LIGHT, SV_LIGHT_TORCH);
                object_prep(i_ptr, k_idx);
                apply_magic(i_ptr, gen_depth, false, false, false, false);
            }
        }

        i_ptr->ident |= source_ident;

        /* Assume seen XXX XXX XXX */
        dump_item++;

        /* Drop it in the dungeon */
        drop_near(i_ptr, -1, y, x);

        // Drop one fewer object to compensate
        number--;
    }

    /* Use the monster's level, but cap to dungeon depth so A: schedule gates
     * are enforced by the current level (prevents early lantern/jewel drops). */
    int depth_cap = player_generation_depth();
    object_level = MIN(r_ptr->level, depth_cap);
    drop_quality quality = artefact ? DROP_QUALITY_ARTEFACT
                                    : drop_quality_from_flags(good, great, superb);
    level_partition_kind part_kind = level_partition_kind_for_point(y, x);
    drop_profile monster_profile;
    int normal_drop_type;
    drop_profile_for_partition_kind_source(
        part_kind, PARTITION_DROP_SOURCE_MONSTER, &monster_profile);
    normal_drop_type = monster_profile.allow_damaged
        ? DROP_TYPE_UNTHEMED
        : DROP_TYPE_NOT_DAMAGED;

    byte old_gen_mode = object_generation_mode;
    object_generation_mode = OB_GEN_MODE_MONSTER_DROP;

    /* Drop some objects */
    for (j = 0; j < number; j++)
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        if (artefact && (j == 0))
        {
            if (!make_guaranteed_artefact_with_profile(
                    i_ptr, quality, normal_drop_type, &monster_profile))
            {
                log_warn(
                    "drop_loot: DROP_ARTEFACT failed to find an eligible artefact for '%s' (r_idx=%d, depth=%d); falling back to normal loot",
                    r_name + r_ptr->name, m_ptr->r_idx, p_ptr ? p_ptr->depth : 0);

                /* Only fall back once the legal artefact pool is exhausted. */
                if (chest)
                {
                    if (!make_object_with_profile(
                            i_ptr, quality, DROP_TYPE_CHEST, &monster_profile))
                        continue;
                    if (i_ptr->tval == TV_CHEST)
                    {
                        i_ptr->xtra1 =
                            (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
                    }
                }
                else if (!make_object_with_profile(
                             i_ptr, quality, normal_drop_type,
                             &monster_profile))
                {
                    continue;
                }
            }
        }
        else if (chest)
        {
            if (!make_object_with_profile(
                    i_ptr, quality, DROP_TYPE_CHEST, &monster_profile))
                continue;
            if (i_ptr->tval == TV_CHEST)
                i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
        }

        /* Make an object */
        else if (!make_object_with_profile(
                     i_ptr, quality, normal_drop_type, &monster_profile))
            continue;

        i_ptr->ident |= source_ident;

        /* Assume seen XXX XXX XXX */
        dump_item++;

        /* Drop it in the dungeon */
        drop_near(i_ptr, -1, y, x);
    }

    object_generation_mode = old_gen_mode;

    /* Reset the object level */
    object_level = original_object_level;

    /* Take note of any dropped treasure */
    if (visible && (dump_item))
    {
        /* Take notes on treasure */
        lore_treasure(cave_m_idx[m_ptr->fy][m_ptr->fx], dump_item);
    }

    return dump_item;
}

static int drop_orcish_liquor(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    s16b k_idx;

    if (!(r_ptr->flags3 & (RF3_ORC)) || !percent_chance(10))
        return 0;

    k_idx = lookup_kind(TV_POTION, SV_POTION_ORCISH_LIQUOR);
    if (!k_idx)
    {
        log_warn("drop_orcish_liquor: missing Orcish Liquor kind");
        return 0;
    }

    object_wipe(i_ptr);
    object_prep(i_ptr, k_idx);
    drop_near(i_ptr, -1, m_ptr->fy, m_ptr->fx);
    return 1;
}

static const char morgoth_second_wind_text[][100]
    = { { "I am the Elder King: Melkor, first and mightiest of all the Valar," },
        { "who was before the world, and made it." },
        { "The shadow of my purpose lies upon Arda, and all that is in it" },
        { "bends slowly and surely to my will." },
        { "Think not that I shall go down so easily, nor that your stroke is the end." },

        { "" } };

static void morgoth_second_wind_message(void)
{
    flush();
    pause_with_text(morgoth_second_wind_text, 4, 8, NULL, 0);
}

/*
 * Makes Morgoth progressively more dangerous.
 */
void anger_morgoth(int level)
{
    monster_race* r_ptr = &r_info[R_IDX_MORGOTH];
    
    log_debug("anger_morgoth: called with level=%d, current morgoth_state=%d", 
              level, p_ptr->morgoth_state);
    
    if (p_ptr->morgoth_state >= level)
    {
        log_debug("anger_morgoth: no change, already at or above level %d", level);
        return;
    }

    log_debug("anger_morgoth: transitioning from state %d to state %d", 
              p_ptr->morgoth_state, level);
    log_debug("anger_morgoth: BEFORE - att=%d dd=%dd%d evn=%d pd=%d wil=%d per=%d light=%d",
              r_ptr->blow[0].att, r_ptr->blow[0].dd, r_ptr->blow[0].ds,
              r_ptr->evn, r_ptr->pd, r_ptr->wil, r_ptr->per, r_ptr->light);

    /* Apply all changes cumulatively up to the target level */
    /* This ensures stats are correct even when skipping intermediate states */
    
    /* State 0: Base values (for reference) */
    if (level >= 0)
    {
        r_ptr->evn = 20;
        r_ptr->blow[0].att = 20;
        r_ptr->blow[0].dd = 6; /* 6d10 */
        r_ptr->pd = 5;        /* 5d4 */
        r_ptr->wil = 25;
        r_ptr->per = 10;
        r_ptr->light = 7;
    }
    
    /* State 1: Crown lost */
    if (level >= 1)
    {
        r_ptr->evn = 25;
        r_ptr->pd = 6;        /* 6d4 */
        r_ptr->light = 0;
        r_ptr->per = 15;
        log_debug("anger_morgoth: applying state 1 changes - crown lost");
    }
    
    /* State 2: Hurt or 1st Silmaril stolen */
    if (level >= 2)
    {
        r_ptr->blow[0].att = 30;
        r_ptr->blow[0].dd = 7; /* 7d10 */
        r_ptr->wil = 30;
        r_ptr->per = 20;
        r_ptr->evn = 30;
        r_ptr->pd = 7;        /* 7d4 */
        log_debug("anger_morgoth: applying state 2 changes - hurt or silmaril");
    }
    
    /* State 3: Furious or 2nd Silmaril stolen */
    if (level >= 3)
    {
        r_ptr->pd = 8;        /* 8d4 */
        r_ptr->evn = 35;
        r_ptr->wil = 35;
        r_ptr->per = 25;
        log_debug("anger_morgoth: applying state 3 changes - badly hurt/furious");
    }
    
    /* State 4: Apoplectic or 3rd Silmaril stolen */
    if (level >= 4)
    {
        r_ptr->evn = 40;
        r_ptr->pd = 9;        /* 9d4 */
        r_ptr->blow[0].att = 40;
        r_ptr->blow[0].dd = 8; /* 8d10 */
        r_ptr->wil = 40;
        r_ptr->per = 30;
        log_debug("anger_morgoth: applying state 4 changes - desperate/apoplectic");
    }

    /* State 5: Desperate (final) */
    if (level >= 5)
    {
        r_ptr->evn = 60;
        r_ptr->pd = 12; 
        r_ptr->ps = 5;      /* 12d5 */
        r_ptr->blow[0].att = 60;
        r_ptr->blow[0].dd = 10;
        r_ptr->blow[0].ds = 10; /* 10d10 */
        r_ptr->wil = 50;
        r_ptr->per = 40;
        log_debug("anger_morgoth: applying state 5 changes - final desperate");
    }

    /* State 6: God (final desperate, but will and perception maxed) */
    if (level >= 6)
    {
        r_ptr->wil = 100;
        r_ptr->per = 100;
        log_debug("anger_morgoth: applying state 6 changes - god");
    }

    p_ptr->morgoth_state = level;
    
    log_debug("anger_morgoth: AFTER - att=%d dd=%dd%d evn=%d pd=%d wil=%d per=%d light=%d",
              r_ptr->blow[0].att, r_ptr->blow[0].dd, r_ptr->blow[0].ds,
              r_ptr->evn, r_ptr->pd, r_ptr->wil, r_ptr->per, r_ptr->light);
    log_debug("anger_morgoth: state successfully changed to %d", p_ptr->morgoth_state);
}

static int morgoth_hp_based_state(const monster_type* m_ptr)
{
    long hp;
    long maxhp;

    if (m_ptr == NULL)
        return 0;

    hp = (long)m_ptr->hp;
    maxhp = (long)m_ptr->maxhp;

    if (hp <= 0 || maxhp <= 0)
        return 0;

    /* HP thresholds (inclusive): 80/60/40/20% -> states 2/3/4/5 */
    if ((hp * 100L) <= (maxhp * 20L))
        return 5;
    if ((hp * 100L) <= (maxhp * 40L))
        return 4;
    if ((hp * 100L) <= (maxhp * 60L))
        return 3;
    if ((hp * 100L) <= (maxhp * 80L))
        return 2;

    return 0;
}

void maybe_update_morgoth_state_from_hp(monster_type* m_ptr)
{
    int target_state;

    if (m_ptr == NULL)
        return;
    if (m_ptr->r_idx != R_IDX_MORGOTH)
        return;

    target_state = morgoth_hp_based_state(m_ptr);
    if (target_state <= 0)
        return;

    if (target_state <= p_ptr->morgoth_state)
        return;

    switch (target_state)
    {
    case 2:
        msg_print("Morgoth grows angry.");
        break;
    case 3:
        msg_print("Morgoth grows furious.");
        break;
    case 4:
        msg_print("Morgoth grows apoplectic.");
        break;
    case 5:
        msg_print("Morgoth grows desperate.");
        break;
    default:
        break;
    }

    message_flush();
    anger_morgoth(target_state);
}

static void fail_niena_quest(void)
{
    p_ptr->niena_quest = NIENA_QUEST_FAILED;
    p_ptr->niena_level = 0;

    log_trace("Niena quest failed after a kill (seen=%d, killed=%d)",
              p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);

    msg_print("A long, sorrowful silence settles over you.");
    msg_print("You have taken a life and failed Niena's mercy quest.");
}

/*
 * Handle the "death" of a monster.
 *
 * Disperse treasures centered at the monster location based on the
 * various flags contained in the monster flags fields.
 *
 * Note that only the player can induce "monster_death()" on Uniques.
 *
 * Note that monsters can now carry objects, and when a monster dies,
 * it drops all of its objects, which may disappear in crowded rooms.
 */
void monster_death(int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    s32b new_exp;

    int multiplier = 1;

    /* Track monster death for Niena mercy quest */
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE && m_ptr->r_idx != R_IDX_NIENA) {
        p_ptr->niena_monsters_killed++;
        log_trace("Niena quest: Monster killed (total killed=%d, seen=%d)", 
                 p_ptr->niena_monsters_killed, p_ptr->niena_monsters_seen);
        fail_niena_quest();
    }

    /* Track monster death for Orome hunting quest - global kill counting */
    if (p_ptr->orome_quest >= OROME_QUEST_ACTIVE) {
        bool target_killed = false;
        
        /* Check if killed monster matches any hunt target type */
        if (r_ptr->flags3 & RF3_WOLF) {
            p_ptr->orome_wolves_killed++;
            target_killed = true;
            log_trace("Orome quest: Wolf killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)", 
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed, 
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
        /* Spider check: exclude trivial 'hatchling' variants from the Orome count */
        if (r_ptr->flags3 & RF3_SPIDER) {
            bool is_hatchling = false;
            if (r_ptr->name)
            {
                const char* rname = r_name + r_ptr->name;
                /* Case-insensitive substring search for "hatchling" */
                for (const char* p = rname; *p; p++)
                {
                    if (SDL_strncasecmp(p, "hatchling", 9) == 0)
                    {
                        is_hatchling = true;
                        break;
                    }
                }
            }

            if (!is_hatchling)
            {
                p_ptr->orome_spiders_killed++;
                target_killed = true;
                log_trace("Orome quest: Spider killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)", 
                         p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed, 
                         p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
            }
            else
            {
                log_trace("Orome quest: Spider hatchling excluded from spider count (name='%s')", (r_ptr->name ? (r_name + r_ptr->name) : "<unnamed>"));
            }
        }
        if (r_ptr->flags3 & RF3_SERPENT) {
            p_ptr->orome_serpents_killed++;
            target_killed = true;
            log_trace("Orome quest: Serpent killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)", 
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed, 
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
        if (r_ptr->flags3 & RF3_VAMPIRE) {
            p_ptr->orome_vampires_killed++;
            target_killed = true;
            log_trace("Orome quest: Vampire killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)", 
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed, 
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
        
        if (target_killed) {
            log_trace("Orome quest: Hunt target monster killed, checking for completion thresholds...");
        }
    }

    /*
     *   1. General monster death things
     */

    // Special message and flag setting for killing Morgoth
    if (m_ptr->r_idx == R_IDX_MORGOTH)
    {
        p_ptr->morgoth_slain = true;
        log_info("Morgoth slain by player; initiating Morgoth victory sequence.");
        msg_print("Morgoth's form shudders, radiant fissures racing across his iron crown!");
        msg_print("From beyond the West, the Valar proclaim your impossible triumph.");
        message_flush();
        do_cmd_morgoth_victory();
    }

    /* If the player kills a Unique, write a note. */
    if (r_ptr->flags1 & RF1_UNIQUE)
    {
        char note2[120];
        char real_name[120];

        /* Get the monster's real name for the notes file */
        monster_desc_race(real_name, sizeof(real_name), m_ptr->r_idx);

        /* Write note */
        if (monster_nonliving(r_ptr))
            SDL_strlcpy(note2, format("Destroyed %s", real_name), sizeof(note2));
        else
            SDL_strlcpy(note2, format("Slew %s", real_name), sizeof(note2));

        do_cmd_note(note2, p_ptr->depth);
    }

    /* Update monster list window */
    p_ptr->window |= PW_MONLIST;

    /* Check for Tulkas quest completion */
    check_tulkas_quest_completion(m_ptr->r_idx);
    
    /* Check for Mandos quest completion */
    check_mandos_quest_completion(m_ptr->r_idx);
    
    /* Check for Varda quest completion */
    check_varda_quest_completion(m_ptr->r_idx);
    
    /* Check for Orome quest completion */
    check_orome_quest_completion();

    /* Give some experience for the kill */
    new_exp = adjusted_mon_exp(r_ptr, true);
    gain_exp(new_exp);
    p_ptr->kill_exp += new_exp;

    /* Uniques stay dead */
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        r_ptr->max_num = 0;
    }

    /* Count kills this life */
    if (l_ptr->pkills < MAX_SHORT)
        l_ptr->pkills++;

    /* Count kills in all lives */
    if (l_ptr->tkills < MAX_SHORT)
        l_ptr->tkills++;

    // since it was killed, it was definitely encountered
    if (!m_ptr->encountered)
    {
        int new_exp;

        new_exp = adjusted_mon_exp(r_ptr, false);

        // gain experience for encounter
        gain_exp(new_exp);
        p_ptr->encounter_exp += new_exp;

        // update stats
        m_ptr->encountered = true;
        l_ptr->psights++;
        if (l_ptr->tsights < MAX_SHORT)
            l_ptr->tsights++;
    }

    /*
     *   2. Lower the morale of similar monsters that can see the deed.
     */

    // double effect for creatures with escorts
    if ((r_ptr->flags1 & (RF1_ESCORT)) || (r_ptr->flags1 & (RF1_ESCORTS)))
        multiplier = 4;

    scare_onlooking_friends(m_ptr, -40 * multiplier);

    /*
     *   3. Dropping items
     */

    // monsters who fell into chasms also don't generate loot...
    if (!((cave_feat[m_ptr->fy][m_ptr->fx] == FEAT_CHASM)
            && !(r_ptr->flags2 & (RF2_FLYING))))
    {
        int normal_loot_count = 0;
        int bonus_loot_count = 0;

        // drop the normal loot for non-territorial monsters
        if (!(r_ptr->flags2 & (RF2_TERRITORIAL)))
        {
            normal_loot_count = drop_loot(m_ptr);
        }

        // Every orc has an additional independent 10% chance to drop Orcish Liquor.
        bonus_loot_count = drop_orcish_liquor(m_ptr);

        if (bonus_loot_count && (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE))))
        {
            lore_treasure(cave_m_idx[m_ptr->fy][m_ptr->fx],
                normal_loot_count + bonus_loot_count);
        }
    }

    return;
}

/*
 * This adjusts a monster's raw experience point value according to the number
 * killed so far The formula is:
 *
 * (depth*10) / (kills+1)
 *
 * This is doubled for uniques.
 *
 * ((depth*25) * 4) / (kills + 4)  <- previous version
 *
 * 100 90 83 76 71 66 62  (10,10)   <- earliest version?
 * 100 80 66 57 50 44 40  (4,4)     <- this is the previous version (without
 * the 1.5 multiplier) 100 66 50 40 33 28 25  (2,2) 100 50 33 25 20 16 14  (1,1)
 * <- this is the current version
 *
 * 100 90 81 72 65 59 53  (10%)     <- exponential alternatives
 * 100 80 64 51 40 32 25  (20%)
 *
 * This function is called when gaining experience and when displaying it in
 * monster recall.
 */
s32b adjusted_mon_exp(const monster_race* r_ptr, bool kill)
{
    s32b exp;
    int mexp = r_ptr->level * 10;
    int mkills = l_list[r_ptr - r_info].pkills;
    int msights = l_list[r_ptr - r_info].psights;
    int repeats = kill ? mkills : msights;

    if (kill)
    {
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            exp = mexp * 2;
        }
        else
        {
            exp = (repeats >= 30) ? 0 : (mexp >> repeats);
        }

        if (r_ptr->flags1 & RF1_PEACEFUL)
        {
            exp = 0;
        }
    }
    else
    {
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            exp = mexp * 2;
        }
        else
        {
            exp = (repeats >= 30) ? 0 : (mexp >> repeats);
        }
    }

    return (exp);
}

/*
 * Decrease a monster's hit points, handle monster death.
 *
 * We return true if the monster has been killed (and deleted).
 *
 * We announce monster death (using an optional "death message"
 * if given, and a otherwise a generic killed/destroyed message).
 *
 * Only "physical attacks" can induce the "You have slain" message.
 * Missile and Spell attacks will induce the "dies" message, or
 * various "specialized" messages.  Note that "You have destroyed"
 * and "is destroyed" are synonyms for "You have slain" and "dies".
 *
 * Invisible monsters induce a special "You have killed it." message.
 */
bool mon_take_hit(int m_idx, int dam, cptr note, int who)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Redraw (later) if needed */
    if (p_ptr->health_who == m_idx)
        p_ptr->redraw |= (PR_HEALTHBAR);

    /* Hurt it */
    m_ptr->hp -= dam;

    if (dam > 0)
        maybe_update_morgoth_state_from_hp(m_ptr);

    /* It is dead now */
    if (m_ptr->hp <= 0)
    {
        if (m_ptr->r_idx == R_IDX_MORGOTH && !p_ptr->morgoth_second_wind)
        {
            int restored = (int)((long)m_ptr->maxhp * 20L / 100L);

            if (restored < 1)
                restored = 1;

            m_ptr->hp = restored;
            p_ptr->morgoth_second_wind = 1;

            log_info("Morgoth reached 0 HP; restoring to %d/%d and entering god state.", 
                     m_ptr->hp, m_ptr->maxhp);
            anger_morgoth(6);
            morgoth_second_wind_message();
            set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
            m_ptr->mflag |= (MFLAG_ACTV);
            m_ptr->min_range = 0;

            return (false);
        }

        char m_name[80];

        /* Extract monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        /* Death by Missile/Spell attack */
        if (note)
        {
            /* Hack -- allow message suppression */
            if (strlen(note) <= 1)
            {
                /* Be silent */
            }

            else
            {
                message_format(MSG_KILL, m_ptr->r_idx, "%^s%s", m_name, note);
            }
        }

        /* Death by physical attack -- invisible monster */
        else if (!m_ptr->ml)
        {
            // You only get messages for unseen monsters if you kill them
            if ((who < 0)
                && (distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) == 1))
                message_format(
                    MSG_KILL, m_ptr->r_idx, "You have killed %s.", m_name);
            // else			message_format(MSG_KILL, m_ptr->r_idx,
            // "%^s has been killed.", m_name);
        }

        /* Death by Physical attack -- non-living monster */
        else if (monster_nonliving(r_ptr))
        {
            if (who < 0)
                message_format(
                    MSG_KILL, m_ptr->r_idx, "You have destroyed %s.", m_name);
            else
                message_format(
                    MSG_KILL, m_ptr->r_idx, "%^s has been destroyed.", m_name);
        }

        /* Death by Physical attack -- living monster */
        else
        {
            if (who < 0)
                message_format(
                    MSG_KILL, m_ptr->r_idx, "You have slain %s.", m_name);
            else
                message_format(
                    MSG_KILL, m_ptr->r_idx, "%^s has been slain.", m_name);
        }

        /* Generate treasure */
        monster_death(m_idx);

        /* Auto-recall only if visible or unique */
        if (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)))
        {
            monster_race_track(m_ptr->r_idx);
        }

        /* Delete the monster */
        delete_monster_idx(m_idx);

        /* Monster is dead */
        return (true);
    }

    // Wake it up if there was real damage dealt
    if (dam > 0)
    {
        int random_level = rand_range(ALERTNESS_ALERT, ALERTNESS_QUITE_ALERT);
        set_alertness(m_ptr, MAX(m_ptr->alertness + dam, random_level + dam));
    }

    /* Monster will always go active */
    m_ptr->mflag |= (MFLAG_ACTV);

    /* Recalculate desired minimum range */
    if (dam > 0)
        m_ptr->min_range = 0;

    /* Not dead yet */
    return (false);
}

/*
 * Modify the current panel to the given coordinates, adjusting only to
 * ensure the coordinates are legal, and return true if anything done.
 *
 * Hack -- The surface should never be scrolled around.
 *
 * Note that monsters are no longer affected in any way by panel changes.
 *
 * As a total hack, whenever the current panel changes, we assume that
 * the "overhead view" window should be updated.
 */
bool modify_panel(int wy, int wx)
{
    /* Verify wy, adjust if needed */
    if (p_ptr->cur_map_hgt < SCREEN_HGT)
        wy = 0;
    else if (wy > p_ptr->cur_map_hgt - SCREEN_HGT)
        wy = p_ptr->cur_map_hgt - SCREEN_HGT;
    if (wy < 0)
        wy = 0;

    /* Verify wx, adjust if needed */
    if (p_ptr->cur_map_wid < SCREEN_WID)
        wx = 0;
    else if (wx > p_ptr->cur_map_wid - SCREEN_WID)
        wx = p_ptr->cur_map_wid - SCREEN_WID;
    if (wx < 0)
        wx = 0;

    /* React to changes */
    if ((p_ptr->wy != wy) || (p_ptr->wx != wx))
    {
        /* Save wy, wx */
        p_ptr->wy = wy;
        p_ptr->wx = wx;

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Hack -- Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        /* Changed */
        return (true);
    }

    /* No change */
    return (false);
}

/*
 * Perform the minimum "whole panel" adjustment to ensure that the given
 * location is contained inside the current panel, and return true if any
 * such adjustment was performed.
 */
bool adjust_panel(int y, int x)
{
    int wy = p_ptr->wy;
    int wx = p_ptr->wx;

    /* Adjust as needed */
    while (y >= wy + SCREEN_HGT)
        wy += SCREEN_HGT;
    while (y < wy)
        wy -= SCREEN_HGT;

    /* Adjust as needed */
    while (x >= wx + SCREEN_WID)
        wx += SCREEN_WID;
    while (x < wx)
        wx -= SCREEN_WID;

    /* Use "modify_panel" */
    return (modify_panel(wy, wx));
}

/*
 * Change the current panel to the panel lying in the given direction.
 *
 * Return true if the panel was changed.
 */
bool change_panel(int dir)
{
    int wy = p_ptr->wy + ddy[dir] * PANEL_HGT;
    int wx = p_ptr->wx + ddx[dir] * PANEL_WID;

    /* Use "modify_panel" */
    return (modify_panel(wy, wx));
}

/*
 * Verify the current panel (relative to the player location).
 *
 * By default, when the player gets "too close" to the edge of the current
 * panel, the map scrolls one panel in that direction so that the player
 * is no longer so close to the edge.
 *
 * The "center_player" option allows the current panel to always be centered
 * around the player, which is very expensive, and also has some interesting
 * gameplay ramifications.
 */
void verify_panel(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int banner_rows = active_narrative_banner_rows();

    int wy = p_ptr->wy;
    int wx = p_ptr->wx;

    bool compact_panel_y = (SCREEN_HGT < PANEL_HGT * 2);
    bool compact_panel_x = (SCREEN_WID < PANEL_WID * 2);

    int v_margin = compact_panel_y ? (SCREEN_HGT / 4) : 13;
    int h_margin = compact_panel_x ? (SCREEN_WID / 4) : 17;

    if (v_margin < 2)
        v_margin = 2;
    if (h_margin < 4)
        h_margin = 4;

    if (v_margin > (SCREEN_HGT - 1) / 2)
        v_margin = (SCREEN_HGT - 1) / 2;
    if (h_margin > (SCREEN_WID - 1) / 2)
        h_margin = (SCREEN_WID - 1) / 2;

    int v_step = compact_panel_y ? (SCREEN_HGT - 2 * v_margin) : PANEL_HGT;
    int h_step = compact_panel_x ? (SCREEN_WID - 2 * h_margin) : PANEL_WID;

    if (v_step < 1)
        v_step = 1;
    if (h_step < 1)
        h_step = 1;

    /* Scroll screen vertically when off-center */
    if (center_player && (!p_ptr->running || !run_avoid_center))
    {
        wy = py - SCREEN_HGT / 2;
    }

    // Sil-y: make this an option
    // by default it is 2 for vertical and 4 for hor
    // needs to be programmed better
    // this doesn't do quite what it says on bigscreen
    // it can end up assymmetric up/down or l/r due to panels

    /* Scroll screen vertically when near top/bottom edge */
    else if (py < wy + v_margin)
    {
        if (compact_panel_y)
            wy -= v_step;
        else
            wy = ((py - PANEL_HGT / 2) / PANEL_HGT) * PANEL_HGT;
    }
    else if (py >= wy + SCREEN_HGT - v_margin)
    {
        if (compact_panel_y)
            wy += v_step;
        else
            wy = ((py - PANEL_HGT / 2) / PANEL_HGT) * PANEL_HGT;
    }

    if (banner_rows >= SCREEN_HGT)
        banner_rows = SCREEN_HGT - 1;
    if ((banner_rows > 0) && (py < wy + banner_rows))
        wy = py - banner_rows;

    /* Scroll screen horizontally when off-center */
    if (center_player && (!p_ptr->running || !run_avoid_center)
        && (px != wx + SCREEN_WID / 2))
    {
        wx = px - SCREEN_WID / 2;
    }

    /* Scroll screen horizontally when near left/right edge */
    else if (px < wx + h_margin)
    {
        if (compact_panel_x)
            wx -= h_step;
        else
            wx = ((px - PANEL_WID / 2) / PANEL_WID) * PANEL_WID;
    }
    else if (px >= wx + SCREEN_WID - h_margin)
    {
        if (compact_panel_x)
            wx += h_step;
        else
            wx = ((px - PANEL_WID / 2) / PANEL_WID) * PANEL_WID;
    }

    /* Scroll if needed */
    bool panel_changed = modify_panel(wy, wx);

    /* Safety net: never allow the player to remain outside the visible panel. */
    if (!panel_contains(py, px))
    {
        if (adjust_panel(py, px))
            panel_changed = true;
    }

    if (panel_changed)
    {
        /* Optional disturb on "panel change" */
        if (!center_player)
            disturb(0, 0);
    }
}

/*
 * Monster health description.
 */
static void look_mon_desc(char* buf, size_t max, int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];

    // start the string empty
    SDL_strlcpy(buf, "(", max);

    if (p_ptr->wizard)
    {
        if (m_ptr->alertness < ALERTNESS_UNWARY)
            SDL_strlcat(buf, format("asleep (%d), ", m_ptr->alertness), max);
        else if (m_ptr->alertness < ALERTNESS_ALERT)
            SDL_strlcat(buf, format("unwary (%d), ", m_ptr->alertness), max);
        else
            SDL_strlcat(buf, format("alert (%d), ", m_ptr->alertness), max);
    }

    if (m_ptr->confused)
        SDL_strlcat(buf, "confused, ", max);
    if (m_ptr->stunned)
        SDL_strlcat(buf, "stunned, ", max);
    if ((m_ptr->slowed) && (!m_ptr->hasted))
        SDL_strlcat(buf, "slowed, ", max);
    if ((!m_ptr->slowed) && (m_ptr->hasted))
        SDL_strlcat(buf, "hasted, ", max);

    // If nothing is going to be written, wipe the string
    if (strlen(buf) == 1)
    {
        buf[0] = '\0';
    }
    // Otherwise finish it
    else
    {
        // trim the final ", " first
        buf[strlen(buf) - 2] = '\0';
        SDL_strlcat(buf, ") ", max);
    }
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort_aux(void* u, void* v, int p, int q)
{
    int z, a, b;

    /* Done sort */
    if (p >= q)
        return;

    /* Pivot */
    z = p;

    /* Begin */
    a = p;
    b = q;

    /* Partition */
    while (true)
    {
        /* Slide i2 */
        while (!(*ang_sort_comp)(u, v, b, z))
            b--;

        /* Slide i1 */
        while (!(*ang_sort_comp)(u, v, z, a))
            a++;

        /* Done partition */
        if (a >= b)
            break;

        /* Swap */
        (*ang_sort_swap)(u, v, a, b);

        /* Advance */
        a++, b--;
    }

    /* Recurse left side */
    ang_sort_aux(u, v, p, b);

    /* Recurse right side */
    ang_sort_aux(u, v, b + 1, q);
}

/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort(void* u, void* v, int n)
{
    /* Sort the array */
    ang_sort_aux(u, v, 0, n - 1);
}

/*** Targetting Code ***/

/*
 * Given a "source" and "target" location, extract a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 */
int motion_dir(int y1, int x1, int y2, int x2)
{
    /* No movement required */
    if ((y1 == y2) && (x1 == x2))
        return (5);

    /* South or North */
    if (x1 == x2)
        return ((y1 < y2) ? 2 : 8);

    /* East or West */
    if (y1 == y2)
        return ((x1 < x2) ? 6 : 4);

    /* South-east or South-west */
    if (y1 < y2)
        return ((x1 < x2) ? 3 : 1);

    /* North-east or North-west */
    if (y1 > y2)
        return ((x1 < x2) ? 9 : 7);

    /* Paranoia */
    return (5);
}

/*
 * Extract a direction (or zero) from a character
 */
int target_dir(char ch)
{
    int d = 0;

    int mode;

    cptr act;

    cptr s;

    /* Already a direction? */
    if (isdigit((unsigned char)ch))
    {
        d = D2I(ch);
    }
    else
    {
        // allow 'z' to indicate center direction
        if (ch == 'z')
            d = 5;

        else
        {
            // Determine the keyset
            if (!hjkl_movement && !angband_keyset)
                mode = KEYMAP_MODE_SIL;
            else if (hjkl_movement && !angband_keyset)
                mode = KEYMAP_MODE_SIL_HJKL;
            else if (!hjkl_movement && angband_keyset)
                mode = KEYMAP_MODE_ANGBAND;
            else
                mode = KEYMAP_MODE_ANGBAND_HJKL;

            /* Extract the action (if any) */
            act = keymap_act[mode][(byte)(ch)];

            /* Analyze */
            if (act)
            {
                /* Convert to a direction */
                for (s = act; *s; ++s)
                {
                    /* Use any digits in keymap */
                    if (isdigit((unsigned char)*s))
                        d = D2I(*s);
                }
            }
        }
    }

    /* Return direction */
    return (d);
}

/*
 * Determine is a monster makes a reasonable target
 *
 * The concept of "targetting" was stolen from "Morgul" (?)
 *
 * The player can target any location, or any "target-able" monster.
 *
 * Currently, a monster is "target_able" if it is visible, and if
 * the player can hit it with a projection, and the player is not
 * hallucinating.  This allows use of "use closest target" macros.
 */
bool target_able(int m_idx)
{
    monster_type* m_ptr;

    /* No monster */
    if (m_idx <= 0)
        return (false);

    /* Get monster */
    m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Monster must be alive */
    if (!m_ptr->r_idx)
        return (false);

    /* Monster must be visible */
    if (!m_ptr->ml)
        return (false);

    /* Monster must not be peaceful */
    if (r_ptr->flags1 & (RF1_PEACEFUL))
        return (false);

    /* Monster must be projectable */
    if (!player_can_fire_bold(m_ptr->fy, m_ptr->fx))
        return (false);

    /* Hack -- no targeting hallucinations */
    if (p_ptr->image)
        return (false);

    /* Rage and labyrinth partitions both suppress remembered-grid targeting. */
    if (!grid_info_is_available(m_ptr->fy, m_ptr->fx))
        return (false);

    /* Hack -- Never target trappers XXX XXX XXX */
    /* if (CLEAR_ATTR && (CLEAR_CHAR)) return (false); */

    /* Assume okay */
    return (true);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * We return true if the target is "okay" and false otherwise.
 */
bool target_okay(int range)
{
    /* No target */
    if (!p_ptr->target_set)
        return (false);

    /* Accept some "location" targets */
    if (p_ptr->target_who == 0)
    {
        /* Never "target" the player's own grid */
        if ((p_ptr->target_row == p_ptr->py) && (p_ptr->target_col == p_ptr->px))
            return (false);

        // reject things beyond range
        if ((range > 0)
            && (distance(
                    p_ptr->py, p_ptr->px, p_ptr->target_row, p_ptr->target_col)
                > range))
            return (false);

        // accept things in LOF
        if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_FIRE))
            return (true);

        // accept walls (for horn of blasting stuff)
        else if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_WALL))
            return (true);

        // reject others
        else
            return (false);
    }

    /* Check "monster" targets */
    if (p_ptr->target_who > 0)
    {
        int m_idx = p_ptr->target_who;

        /* Accept reasonable targets */
        if (target_able(m_idx))
        {
            monster_type* m_ptr = &mon_list[m_idx];

            /* Get the monster location */
            p_ptr->target_row = m_ptr->fy;
            p_ptr->target_col = m_ptr->fx;

            // reject things beyond range
            if ((range > 0)
                && (distance(p_ptr->py, p_ptr->px, p_ptr->target_row,
                        p_ptr->target_col)
                    > range))
                return (false);

            /* Good target */
            return (true);
        }
    }

    /* Assume no target */
    return (false);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * Very similar to target_okay, but does not require projectibility, just line
 * of sight
 *
 * We return true if the target is "okay" and false otherwise.
 */
bool target_sighted(void)
{
    /* No target */
    if (!p_ptr->target_set)
        return (false);

    /* Accept "location" targets */
    if (p_ptr->target_who == 0)
        return (true);

    /* Check "monster" targets */
    if (p_ptr->target_who > 0)
    {
        int m_idx = p_ptr->target_who;
        monster_type* m_ptr = &mon_list[m_idx];

        /* Accept reasonable targets */
        if (player_can_see_bold(m_ptr->fy, m_ptr->fx) && m_ptr->ml)
        {
            /* Get the monster location */
            p_ptr->target_row = m_ptr->fy;
            p_ptr->target_col = m_ptr->fx;

            /* Good target */
            return (true);
        }
    }

    /* Assume no target */
    return (false);
}

/*
 * Set the target to a monster (or nobody)
 */
void target_set_monster(int m_idx)
{
    /* Acceptable target */
    if ((m_idx > 0) && target_able(m_idx))
    {
        monster_type* m_ptr = &mon_list[m_idx];

        /* Save target info */
        p_ptr->target_set = true;
        p_ptr->target_who = m_idx;
        p_ptr->target_row = m_ptr->fy;
        p_ptr->target_col = m_ptr->fx;
    }

    /* Clear target */
    else
    {
        /* Reset target info */
        p_ptr->target_set = false;
        p_ptr->target_who = 0;
        p_ptr->target_row = 0;
        p_ptr->target_col = 0;
    }
}

/*
 * Set the target to a location
 */
void target_set_location(int y, int x)
{
    /* Legal target */
    if (in_bounds_fully(y, x))
    {
        /* Save target info */
        p_ptr->target_set = true;
        p_ptr->target_who = 0;
        p_ptr->target_row = y;
        p_ptr->target_col = x;
    }

    /* Clear target */
    else
    {
        /* Reset target info */
        p_ptr->target_set = false;
        p_ptr->target_who = 0;
        p_ptr->target_row = 0;
        p_ptr->target_col = 0;
    }
}

/*
 * Sorting hook -- comp function -- by "monster priority"
 *
 * Sorts monsters by: 1) Uniques first, 2) Then by depth (higher depth first), 3) Then by distance
 * We use "u" and "v" to point to arrays of "x" and "y" positions.
 */
static bool ang_sort_comp_monster_priority(const void* u, const void* v, int a, int b)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    int m_idx_a, m_idx_b;
    monster_type* m_ptr_a;
    monster_type* m_ptr_b;
    monster_race* r_ptr_a;
    monster_race* r_ptr_b;
    
    /* Get monster indices */
    m_idx_a = cave_m_idx[y[a]][x[a]];
    m_idx_b = cave_m_idx[y[b]][x[b]];
    
    /* Safety check */
    if (!m_idx_a && !m_idx_b) return false;
    if (!m_idx_a) return false; /* b comes first */
    if (!m_idx_b) return true;  /* a comes first */
    
    /* Get monster pointers */
    m_ptr_a = &mon_list[m_idx_a];
    m_ptr_b = &mon_list[m_idx_b];
    r_ptr_a = &r_info[m_ptr_a->r_idx];
    r_ptr_b = &r_info[m_ptr_b->r_idx];
    
    /* Check if either is unique */
    bool unique_a = (r_ptr_a->flags1 & RF1_UNIQUE) != 0;
    bool unique_b = (r_ptr_b->flags1 & RF1_UNIQUE) != 0;
    
    /* Uniques always come first */
    if (unique_a && !unique_b) return true;  /* a comes first */
    if (!unique_a && unique_b) return false; /* b comes first */
    
    /* Both unique or both non-unique, sort by depth (higher depth first) */
    if (r_ptr_a->level != r_ptr_b->level)
    {
        return (r_ptr_a->level >= r_ptr_b->level);
    }
    
    /* Same depth, sort by distance (closer first) */
    int da, db, kx, ky;

    /* Absolute distance components for a */
    kx = x[a] - px;
    kx = ABS(kx);
    ky = y[a] - py;
    ky = ABS(ky);
    da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Absolute distance components for b */
    kx = x[b] - px;
    kx = ABS(kx);
    ky = y[b] - py;
    ky = ABS(ky);
    db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Compare the distances */
    return (da <= db);
}

/*
 * Sorting hook -- comp function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by double-distance to the player.
 */
static bool ang_sort_comp_distance(const void* u, const void* v, int a, int b)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    int da, db, kx, ky;

    /* Absolute distance components */
    kx = x[a];
    kx -= px;
    kx = ABS(kx);
    ky = y[a];
    ky -= py;
    ky = ABS(ky);

    /* Approximate Double Distance to the first point */
    da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Absolute distance components */
    kx = x[b];
    kx -= px;
    kx = ABS(kx);
    ky = y[b];
    ky -= py;
    ky = ABS(ky);

    /* Approximate Double Distance to the first point */
    db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

    /* Compare the distances */
    return (da <= db);
}

/*
 * Sorting hook -- swap function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by distance to the player.
 */
static void ang_sort_swap_distance(void* u, void* v, int a, int b)
{
    byte* x = (byte*)(u);
    byte* y = (byte*)(v);

    byte temp;

    /* Swap "x" */
    temp = x[a];
    x[a] = x[b];
    x[b] = temp;

    /* Swap "y" */
    temp = y[a];
    y[a] = y[b];
    y[b] = temp;
}

/*
 * Hack -- help "select" a location (see below)
 */
static s16b target_pick(int y1, int x1, int dy, int dx)
{
    int i, v;

    int x2, y2, x3, y3, x4, y4;

    int b_i = -1, b_v = 9999;

    /* Scan the locations */
    for (i = 0; i < temp_n; i++)
    {
        /* Point 2 */
        x2 = temp_x[i];
        y2 = temp_y[i];

        /* Directed distance */
        x3 = (x2 - x1);
        y3 = (y2 - y1);

        /* Verify quadrant */
        if (dx && (x3 * dx <= 0))
            continue;
        if (dy && (y3 * dy <= 0))
            continue;

        /* Absolute distance */
        x4 = ABS(x3);
        y4 = ABS(y3);

        /* Verify quadrant */
        if (dy && !dx && (x4 > y4))
            continue;
        if (dx && !dy && (y4 > x4))
            continue;

        /* Approximate Double Distance */
        v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));

        /* Penalize location XXX XXX XXX */

        /* Track best */
        if ((b_i >= 0) && (v >= b_v))
            continue;

        /* Track best */
        b_i = i;
        b_v = v;
    }

    /* Result */
    return (b_i);
}

/*
 * Hack -- determine if a given location is "interesting"
 */
static bool determine_location_is_interesting(int y, int x)
{
    object_type* o_ptr;

    /* Player grids are always interesting */
    if (cave_m_idx[y][x] < 0)
        return (true);

    /* Handle hallucination */
    if (p_ptr->image)
        return (false);

    /* Rage and labyrinth partitions both suppress remembered-grid look data. */
    if (!grid_info_is_available(y, x))
        return (false);

    /* Check for objects first (only shown when on floors, not when in rubble) */
    /* This is checked BEFORE monsters to prevent showing unmarked objects under detected monsters */
    if (cave_floorlike_bold(y, x) || (cave_feat[y][x] == FEAT_SUNLIGHT))
    {
        /* Scan all objects in the grid */
        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Memorized object - this makes the location interesting */
            if (o_ptr->marked && !object_is_searched_skeleton(o_ptr))
                return (true);
        }
    }

    /* Visible monsters (checked AFTER objects) */
    /* This ensures that a location with a monster but no marked objects */
    /* is interesting for monster targeting but NOT for object listing */
    if (cave_m_idx[y][x] > 0)
    {
        monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

        /* Visible monsters */
        if (m_ptr->ml)
            return (true);
    }

    /* Interesting memorized features */
    if (cave_info[y][x] & (CAVE_MARK))
    {
        /* Notice chasms */
        if (cave_feat[y][x] == FEAT_CHASM)
            return (true);

        /* Notice glyphs */
        if (cave_glyph(y, x))
            return (true);

        /* Notice forges */
        if (cave_forge_bold(y, x))
            return (true);

        /* Notice doors */
        if (cave_feat[y][x] == FEAT_OPEN)
            return (true);
        if (cave_feat[y][x] == FEAT_BROKEN)
            return (true);

        /* Notice stairs */
        if (cave_stair_bold(y, x))
            return (true);

        /* Notice traps */
        if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
            return (true);

        /* Notice doors */
        if (cave_known_closed_door_bold(y, x))
            return (true);

        /* Notice rubble */
        if (cave_feat[y][x] == FEAT_RUBBLE)
            return (true);
    }

    /* Nope */
    return (false);
}

/*
 * Prepare the "temp" array for "target_interactive_set"
 *
 * Return the number of target_able monsters in the set.
 */
void get_sorted_target_list(int mode, int range)
{
    int y, x;

    /* Reset "temp" array */
    temp_n = 0;

    /* Scan the current panel */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            // Previously required LOS, but this is now ignored...

            /* Require "interesting" contents */
            if (!determine_location_is_interesting(y, x))
                continue;

            /* Special mode */
            if (mode & (TARGET_KILL))
            {
                /* Must contain a monster */
                if (!(cave_m_idx[y][x] > 0))
                    continue;

                /* Must be a targetable monster */
                if (!target_able(cave_m_idx[y][x]))
                    continue;

                // possibly restrict the distance from the player
                if ((range > 0)
                    && (distance(p_ptr->py, p_ptr->px, y, x) > range))
                    continue;
            }
            else if (mode & (TARGET_LIST_MONSTER))
            {
                /* Must contain a monster */
                if (!(cave_m_idx[y][x] > 0))
                    continue;
            }
            else if (mode & (TARGET_LIST_OBJECT))
            {
                if (!(cave_o_idx[y][x] > 0))
                    continue;
            }

            /* Save the location */
            temp_x[temp_n] = x;
            temp_y[temp_n] = y;
            temp_n++;
        }
    }

    /* Set the sort hooks */
    if (mode & (TARGET_LIST_MONSTER))
    {
        /* Use monster priority sorting (uniques first, then by depth, then by distance) */
        ang_sort_comp = ang_sort_comp_monster_priority;
    }
    else
    {
        /* Use distance sorting for objects and other targets */
        ang_sort_comp = ang_sort_comp_distance;
    }
    ang_sort_swap = ang_sort_swap_distance;

    /* Sort the positions */
    ang_sort(temp_x, temp_y, temp_n);
}

/*
 * Examine a grid, return a keypress.
 *
 * The "mode" argument contains the "TARGET_LOOK" bit flag, which
 * indicates that the "space" key should scan through the contents
 * of the grid, instead of simply returning immediately.  This lets
 * the "look" command get complete information, without making the
 * "target" command annoying.
 *
 * The "info" argument contains the "commands" which should be shown
 * inside the "[xxx]" text.  This string must never be empty, or grids
 * containing monsters will be displayed with an extra comma.
 *
 * Note that if a monster is in the grid, we update both the monster
 * recall info and the health bar info to track that monster.
 *
 * This function correctly handles multiple objects per grid, and objects
 * and terrain features in the same grid, though the latter never happens.
 *
 * This function must handle blindness/hallucination.
 */
static int target_set_interactive_aux(int y, int x, int mode, cptr info, bool use_story_font)
{
    s16b this_o_idx, next_o_idx = 0;

    cptr s1, s2, s3;

    bool boring;

    bool floored;

    int feat;

    int query;

    char out_val[256];

    /* Repeat forever */
    while (1)
    {
        char more[8];
        // reset the 'more' buffer
        strnfmt(more, 1, "");

        /* Paranoia */
        query = ' ';

        /* Assume boring */
        boring = true;

        /* Default */
        s1 = "You see ";
        s2 = "";
        s3 = "";

        /* The player */
        if (cave_m_idx[y][x] < 0)
        {
            /* Description */
            s1 = "You are ";

            /* Preposition */
            s2 = "on ";
        }

        /* Hack -- hallucination */
        if (p_ptr->image)
        {
            /* Display a message */
            strnfmt(out_val, sizeof(out_val),
                "What you see is not to be believed.  [%s]", info);

            look_prt(use_story_font, out_val, 0, 0);
            move_cursor_relative(y, x);
            query = inkey();

            /* Stop on everything but "return" */
            if ((query != '\n') && (query != '\r'))
                break;

            /* Repeat forever */
            continue;
        }

        /* Actual monsters */
        if ((cave_m_idx[y][x] > 0) && grid_info_is_available(y, x))
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Visible */
            if (m_ptr->ml)
            {
                bool recall = false;

                char m_name[80];

                bool show_more = false;

                /* Not boring */
                boring = false;

                if (p_ptr->rage)
                {
                    SDL_strlcpy(m_name, "an enemy", sizeof(m_name));
                }
                else
                {
                    /* Get the monster name ("a kobold") */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
                }

                /* Hack -- track this monster race */
                monster_race_track(m_ptr->r_idx);

                /* Hack -- health bar for this monster */
                health_track(cave_m_idx[y][x]);

                /* Hack -- handle stuff */
                handle_stuff();

                /* Interact */
                while (1)
                {
                    /* Recall, but not when raging */
                    if ((recall) && !p_ptr->rage)
                    {
                        /* Save screen */
                        screen_save();

                        /* Recall on screen */
                        screen_roff(m_ptr->r_idx, m_ptr);

                        /* Hack -- Complete the prompt (again) */
                        Term_addstr(
                            -1, TERM_WHITE, format("  [(r)ecall, %s]", info));

                        /* Command */
                        query = inkey();

                        /* Load screen */
                        screen_load();
                    }

                    /* Normal */
                    else
                    {
                        /* Describe the monster, unless a mimic */
                        char buf[80];

                        look_mon_desc(buf, sizeof(buf), cave_m_idx[y][x]);

                        // determine if there is more info to display...

                        // visible squares with monsters holding things
                        if ((cave_info[y][x] & (CAVE_SEEN))
                            && m_ptr->hold_o_idx)
                        {
                            show_more = true;
                        }

                        // known objects on the floor
                        else if (grid_info_is_available(y, x)
                            && (cave_floorlike_bold(y, x)
                                || (cave_feat[y][x] == FEAT_SUNLIGHT))
                            && cave_o_idx[y][x]
                            && (&o_list[cave_o_idx[y][x]])->marked)
                        {
                            show_more = true;
                        }

                        // standing in a known unusual terrain such as wall or
                        // door
                        else if (!cave_floorlike_bold(y, x)
                            && (cave_info[y][x] & (CAVE_MARK)))
                        {
                            show_more = true;
                        }

                        if (show_more)
                        {
                            strnfmt(more, 8, "-more- ");
                        }

                        /* Describe, and prompt for recall */
                        if (p_ptr->wizard)
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s] (%d:%d)", s1, s2,
                                s3, m_name, buf, more, info, y, x);
                        }

                        else
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s]", s1, s2, s3,
                                m_name, buf, more, info);
                        }

                        look_prt(use_story_font, out_val, 0, 0);

                        /* Place cursor */
                        move_cursor_relative(y, x);

                        /* Command */
                        query = inkey();
                    }

                    /* Normal commands */
                    if (query != 'r')
                        break;

                    /* Toggle recall */
                    recall = !recall;
                }

                /* Stop on everything but "return"/"space" */
                if ((query != '\n') && (query != '\r') && (query != ' '))
                    break;

                /* Sometimes stop at "space" key */
                if ((query == ' ') && !(mode & (TARGET_LOOK)))
                    break;

                /* Stop if not asked to continue */
                if (!show_more)
                    break;

                /* Change the intro */
                s1 = "It is ";

                /* Hack -- take account of gender */
                if (r_ptr->flags1 & (RF1_FEMALE))
                    s1 = "She is ";
                else if (r_ptr->flags1 & (RF1_MALE))
                    s1 = "He is ";

                /* Use a preposition */
                s2 = "carrying ";

                /* Scan all objects being carried */
                for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
                     this_o_idx = next_o_idx)
                {
                    char o_name[80];

                    object_type* o_ptr;

                    /* Get the object */
                    o_ptr = &o_list[this_o_idx];

                    /* Get the next object */
                    next_o_idx = o_ptr->next_o_idx;

                    /*Don't let the player see certain objects (used for vault
                     * treasure)*/
                    if ((o_ptr->ident & (IDENT_HIDE_CARRY)) && (!p_ptr->wizard)
                        && (!cheat_peek))
                        continue;

                    /* Obtain an object description */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                    /* Describe the object */
                    if (p_ptr->wizard)
                    {
                        strnfmt(out_val, sizeof(out_val),
                            "%s%s%s%s %s [%s] (%d:%d)", s1, s2, s3, o_name,
                            more, info, y, x);
                    }
                    else
                    {
                        strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]",
                            s1, s2, s3, o_name, more, info);
                    }

                    look_prt(use_story_font, out_val, 0, 0);
                    move_cursor_relative(y, x);
                    query = inkey();

                    /* Stop on everything but "return"/"space" */
                    if ((query != '\n') && (query != '\r') && (query != ' '))
                        break;

                    /* Sometimes stop at "space" key */
                    if ((query == ' ') && !(mode & (TARGET_LOOK)))
                        break;

                    /* Change the intro */
                    s2 = "also carrying ";
                }

                /* Double break */
                if (this_o_idx)
                    break;

                /* Use a preposition */
                s2 = "on ";
            }
        }
        // if the square doesn't include a monster...
        else
        {
            // cancel health tracking
            health_track(0);

            /* Hack -- handle stuff */
            handle_stuff();
        }

        /* Assume not floored */
        floored = false;

        /* Scan all objects in the grid */
        for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Skip objects if floored */
            if (floored)
                continue;

            /* Objects (only shown when on floors, not when in rubble) */
            if (cave_floorlike_bold(y, x) || (cave_feat[y][x] == FEAT_SUNLIGHT))
            {
                /* Describe it */
                if (o_ptr->marked && grid_info_is_available(y, x))
                {
                    char o_name[80];

                    /* Not boring */
                    boring = false;

                    /* Obtain an object description */
                    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

                    /* Describe the object */
                    if (p_ptr->wizard)
                    {
                        strnfmt(out_val, sizeof(out_val),
                            "%s%s%s%s %s [%s] (%d:%d)", s1, s2, s3, o_name,
                            more, info, y, x);
                    }
                    else
                    {
                        strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]",
                            s1, s2, s3, o_name, more, info);
                    }

                    look_prt(use_story_font, out_val, 0, 0);
                    move_cursor_relative(y, x);
                    query = inkey();

                    /* Stop on everything but "return"/"space" */
                    if ((query != '\n') && (query != '\r') && (query != ' '))
                        break;

                    /* Sometimes stop at "space" key */
                    if ((query == ' ') && !(mode & (TARGET_LOOK)))
                        break;

                    /* Change the intro */
                    s1 = "It is ";

                    /* Plurals */
                    if (o_ptr->number != 1)
                        s1 = "They are ";

                    /* Preposition */
                    s2 = "on ";
                }
            }
        }

        /* Double break */
        if (this_o_idx)
            break;

        /* Feature (apply "mimic") */
        feat = f_info[cave_feat[y][x]].mimic;

        /* Require knowledge about grid, or ability to see grid */
        if ((!grid_info_is_available(y, x)
                || (!(cave_info[y][x] & (CAVE_MARK))
                    && !player_can_see_bold(y, x)))
            && (distance(p_ptr->py, p_ptr->px, y, x) > 0))
        {
            /* Forget feature */
            feat = FEAT_NONE;
        }

        /* Terrain feature if needed */
        if (boring || !cave_floorlike_bold(y, x))
        {
            cptr name = f_name + f_info[feat].name;

            /* Hack -- handle unknown grids */
            if (feat == FEAT_NONE)
                name = "unknown square";

            /* Pick a prefix */
            if (*s2 && (feat >= FEAT_DOOR_HEAD))
                s2 = "in ";

            /* Use the definite article for the unique forge */
            if ((feat >= FEAT_FORGE_UNIQUE_HEAD)
                && (feat <= FEAT_FORGE_UNIQUE_TAIL))
            {
                s3 = "the ";
            }

            /* Pick proper indefinite article */
            else
            {
                s3 = (is_a_vowel(name[0])) ? "an " : "a ";
            }

            /* Display a message */
            if (p_ptr->wizard)
            {
                strnfmt(out_val, sizeof(out_val),
                    "%s%s%s%s (%d) %s [%s] (%d:%d)", s1, s2, s3, name,
                    cave_feat[y][x], more, info, y, x);
            }
            else
            {
                strnfmt(out_val, sizeof(out_val), "%s%s%s%s %s [%s]", s1, s2,
                    s3, name, more, info);
            }

            look_prt(use_story_font, out_val, 0, 0);
            move_cursor_relative(y, x);
            query = inkey();

            /* Stop on everything but "return"/"space" */
            if ((query != '\n') && (query != '\r') && (query != ' '))
                break;
        }

        /* Stop on everything but "return" */
        if ((query != '\n') && (query != '\r'))
            break;
    }

    // make sure the health tracking is sorted out
    if (p_ptr->target_who)
    {
        health_track(p_ptr->target_who);
    }
    else
    {
        health_track(0);
    }

    /* Keep going */
    return (query);
}

/*
 * Draw a visible path over the squares between (x1,y1) and (x2,y2).
 * The path consists of "*", which are white except where there is a
 * monster, object or feature in the grid.
 *
 * This routine has (at least) three weaknesses:
 * - remembered objects/walls which are no longer present are not shown,
 * - squares which (e.g.) the player has walked through in the dark are
 *   treated as unknown space.
 * - walls which appear strange due to hallucination aren't treated correctly.
 *
 * The first two result from information being lost from the dungeon arrays,
 * which requires changes elsewhere
 */
static int draw_path(
    u16b* path, int range, char* c, byte* a, int y1, int x1, int y2, int x2)
{
    int i;
    int max;
    bool on_screen;

    /* Find the path. */
    max = project_path(
        path, range, y1, x1, &y2, &x2, (PROJECT_THRU | PROJECT_INVISIPASS));

    /* No path, so do nothing. */
    if (max < 1)
        return 0;

    /* The starting square is never drawn, but notice if it is being
     * displayed. In theory, it could be the last such square.
     */
    on_screen = panel_contains(y1, x1);

    /* Draw the path. */
    for (i = 0; i < max; i++)
    {
        byte colour;

        /* Find the co-ordinates on the level. */
        int y = GRID_Y(path[i]);
        int x = GRID_X(path[i]);
        /*
         * As path[] is a straight line and the screen is oblong,
         * there is only section of path[] on-screen.
         * If the square being drawn is visible, this is part of it.
         * If none of it has been drawn, continue until some of it
         * is found or the last square is reached.
         * If some of it has been drawn, finish now as there are no
         * more visible squares to draw.
         */

        if (panel_contains(y, x))
            on_screen = true;
        else if (on_screen)
            break;
        else
            continue;

        /* Find the position on-screen */
        move_cursor_relative(y, x);

        /* This square is being overwritten, so save the original. */
        Term_what(Term->scr->cx, Term->scr->cy, a + i, c + i);

        /* Choose a colour. */
        /* Visible monsters are red. */
        if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
        {
            colour = TERM_L_RED;
        }

        /* Known objects are yellow. */
        else if (cave_o_idx[y][x] && o_list[cave_o_idx[y][x]].marked)
        {
            colour = TERM_YELLOW;
        }

        /* Known walls are blue. */
        else if (!cave_floor_bold(y, x)
            && (cave_info[y][x] & (CAVE_MARK) || player_can_see_bold(y, x)))
        {
            colour = TERM_BLUE;
        }
        /* Unknown squares are grey. */
        else if (!(cave_info[y][x] & (CAVE_MARK)) && !player_can_see_bold(y, x))
        {
            colour = TERM_L_DARK;
        }
        /* Unoccupied squares are white. */
        else
        {
            colour = TERM_WHITE;
        }

        /* Draw the path segment */
        (void)Term_addch(colour, '*');
    }
    return i;
}

/*
 * Load the attr/char at each point along "path" which is on screen from
 * "a" and "c". This was saved in draw_path().
 */
static void load_path(int max, u16b* path, char* c, byte* a)
{
    int i;
    for (i = 0; i < max; i++)
    {
        if (!panel_contains(GRID_Y(path[i]), GRID_X(path[i])))
            continue;

        move_cursor_relative(GRID_Y(path[i]), GRID_X(path[i]));

        (void)Term_addch(a[i], c[i]);
    }

    Term_fresh();
}

/*
 * Handle "target" and "look".
 *
 * Note that this code can be called from "get_aim_dir()".
 *
 * Currently, when "flag" is true, that is, when
 * "interesting" grids are being used, and a directional key is used, we
 * only scroll by a single panel, in the direction requested, and check
 * for any interesting grids on that panel.  The "correct" solution would
 * actually involve scanning a larger set of grids, including ones in
 * panels which are adjacent to the one currently scanned, but this is
 * overkill for this function.  XXX XXX
 *
 * Hack -- targetting/observing an "outer border grid" may induce
 * problems, so this is not currently allowed.
 *
 * The player can use the direction keys to move among "interesting"
 * grids in a heuristic manner, or the "space", "+", and "-" keys to
 * move through the "interesting" grids in a sequential manner, or
 * can enter "location" mode, and use the direction keys to move one
 * grid at a time in any direction.  The "t" (set target) command will
 * only target a monster (as opposed to a location) if the monster is
 * target_able and the "interesting" mode is being used.
 *
 * The current grid is described using the "look" method above, and
 * a new command may be entered at any time, but note that if the
 * "TARGET_LOOK" bit flag is set (or if we are in "location" mode,
 * where "space" has no obvious meaning) then "space" will scan
 * through the description of the current grid until done, instead
 * of immediately jumping to the next "interesting" grid.  This
 * allows the "target" command to retain its old semantics.
 *
 * The "*", "+", and "-" keys may always be used to jump immediately
 * to the next (or previous) interesting grid, in the proper mode.
 *
 * The "return" key may always be used to scan through a complete
 * grid description (forever).
 *
 * if the range variable is 0, there is no range limit
 *
 * This command will cancel any old target, even if used from
 * inside the "look" command.
 */
bool target_set_interactive(int mode, int range)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, d, m, t, bd;

    int y = py;
    int x = px;

    int y2; // these dummy variables are needed in path determination stuff
    int x2;

    int adjusted_range;

    bool done = false;

    bool flag = true;

    bool valid_target;

    bool new_target = false;

    char query;

    char info[80];

    bool use_story_look = story_look_enabled() && (mode & TARGET_LOOK);

    /* These are used for displaying the path to the target */
    u16b path[MAX_RANGE];
    char path_char[MAX_RANGE];
    byte path_attr[MAX_RANGE];
    int max;

    bool wiz = mode & (TARGET_WIZ);

    // turn off auto if doing wizard mode dungeon modification
    if (wiz)
        flag = false;

    if (range == 0)
        adjusted_range = MAX_RANGE;
    else
        adjusted_range = range;

    /* Prepare the "temp" array */
    get_sorted_target_list(mode, range);

    /* Start near the player */
    m = 0;

    /* Interact */
    while (!done)
    {
        max = 0;

        /* Interesting grids */
        if (flag && temp_n)
        {
            y = temp_y[m];
            x = temp_x[m];

            y2 = y;
            x2 = x;

            // need to compute 'max' whether or not we are in 'target mode'
            // in order to correctly determine if a square is targetable
            // taking player's limited knowledge into account

            max = project_path(path, adjusted_range, py, px, &y2, &x2,
                (PROJECT_THRU | PROJECT_INVISIPASS));

            /* Draw the path in "target" mode. If there is one */
            if (mode & (TARGET_KILL))
                (void)draw_path(
                    path, adjusted_range, path_char, path_attr, py, px, y, x);

            // Check whether the target location is valid (ie within the path)
            if ((max == 0)
                || ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                        || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
                    && (((GRID_X(path[max - 1]) <= x) && (x <= px))
                        || ((GRID_X(path[max - 1]) >= x) && (x >= px)))))
            {
                valid_target = true;
            }
            else
            {
                valid_target = false;
            }

            // prepare the relevant prompt
            if (valid_target)
            {
                SDL_strlcpy(info, "(t)arget, (m)anual, <dir>", sizeof(info));
            }
            else
            {
                SDL_strlcpy(info, "(m)anual, <dir>", sizeof(info));
            }

            /* Describe and Prompt */
            if (use_story_look)
                sdl_story_font_enable();
            query = target_set_interactive_aux(y, x, mode, info, use_story_look);
            if (use_story_look)
                sdl_story_font_disable();

            /* Remove the path */
            if (mode & (TARGET_KILL))
                load_path(max, path, path_char, path_attr);

            /* Assume no "direction" */
            d = 0;

            /* Analyze */
            switch (query)
            {
            case ESCAPE:
            case 'q':
            {
                done = true;
                break;
            }

            case ' ':
            case '*':
            case '+':
            {
                if (++m == temp_n)
                {
                    m = 0;
                }
                break;
            }

            case '-':
            {
                if (m-- == 0)
                {
                    m = temp_n - 1;
                }
                break;
            }

            case 'p':
            {
                /* Recenter around player */
                verify_panel();

                /* Handle stuff */
                handle_stuff();

                y = py;
                x = px;
                __attribute__((fallthrough));
            }

            case 'm':
            {
                flag = false;
                break;
            }

            case 't':
            case '5':
            case 'z':
            case '\n':
            case '\r':
            {
                int m_idx = cave_m_idx[y][x];

                if ((p_ptr->py == y) && (p_ptr->px == x))
                {
                    done = true;
                }
                else if ((m_idx > 0) && target_able(m_idx))
                {
                    health_track(m_idx);
                    target_set_monster(m_idx);
                    new_target = true;
                    done = true;
                }
                else if (valid_target)
                {
                    target_set_location(y, x);
                    health_track(0);
                    new_target = true;
                    done = true;
                }
                else
                {
                    bell("Illegal target.");
                }
                break;
            }

            default:
            {
                /* Extract direction */
                d = target_dir(query);

                /* Oops */
                if (!d)
                    bell("Illegal command for target mode!");

                break;
            }
            }

            /* Hack -- move around */
            if (d)
            {
                int old_y = temp_y[m];
                int old_x = temp_x[m];

                /* Find a new monster */
                i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                /* Scroll to find interesting grid */
                if (i < 0)
                {
                    int old_wy = p_ptr->wy;
                    int old_wx = p_ptr->wx;

                    /* Change if legal */
                    if (change_panel(d))
                    {
                        /* Recalculate interesting grids */
                        get_sorted_target_list(mode, range);

                        /* Find a new monster */
                        i = target_pick(old_y, old_x, ddy[d], ddx[d]);

                        /* Restore panel if needed */
                        if ((i < 0) && modify_panel(old_wy, old_wx))
                        {
                            /* Recalculate interesting grids */
                            get_sorted_target_list(mode, range);
                        }

                        /* Handle stuff */
                        handle_stuff();
                    }
                }

                /* Use interesting grid if found */
                if (i >= 0)
                    m = i;
            }
        }

        /* Arbitrary grids */
        else if (!wiz)
        {
            y2 = y;
            x2 = x;

            // need to compute 'max' whether or not we are in 'target mode'
            // in order to correctly determine if a square is targetable
            // taking player's limited knowledge into account
            max = project_path(path, adjusted_range, py, px, &y2, &x2,
                (PROJECT_THRU | PROJECT_INVISIPASS));

            /* Draw the path in "target" mode. If there is one */
            if (mode & (TARGET_KILL))
                (void)draw_path(
                    path, adjusted_range, path_char, path_attr, py, px, y, x);

            // Check whether the target location is valid (ie within the path)
            if ((max == 0)
                || ((((GRID_Y(path[max - 1]) <= y) && (y <= py))
                        || ((GRID_Y(path[max - 1]) >= y) && (y >= py)))
                    && (((GRID_X(path[max - 1]) <= x) && (x <= px))
                        || ((GRID_X(path[max - 1]) >= x) && (x >= px)))))
            {
                valid_target = true;
            }
            else
            {
                valid_target = false;
            }

            // prepare the relevant prompt
            if (valid_target || p_ptr->wizard)
            {
                SDL_strlcpy(info, "(t)arget, (a)uto, <dir>", sizeof(info));
            }
            else
            {
                SDL_strlcpy(info, "(a)uto, <dir>", sizeof(info));
            }

            /* Describe and Prompt (enable "TARGET_LOOK") */
            if (use_story_look)
                sdl_story_font_enable();
            query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info, use_story_look);
            if (use_story_look)
                sdl_story_font_disable();

            /* Remove the path */
            if (mode & (TARGET_KILL))
                load_path(max, path, path_char, path_attr);

            /* Assume no direction */
            d = 0;

            /* Analyze the keypress */
            switch (query)
            {
            case ESCAPE:
            case 'q':
            {
                done = true;
                break;
            }

            case 'p':
            {
                /* Recenter around player */
                verify_panel();

                /* Handle stuff */
                handle_stuff();

                y = py;
                x = px;
                __attribute__((fallthrough));
            }

            case 'a':
            {
                flag = true;

                m = 0;
                bd = 999;

                /* Pick a nearby monster */
                for (i = 0; i < temp_n; i++)
                {
                    t = distance(y, x, temp_y[i], temp_x[i]);

                    /* Pick closest */
                    if (t < bd)
                    {
                        m = i;
                        bd = t;
                    }
                }

                /* Nothing interesting */
                if (bd == 999)
                    flag = false;

                break;
            }

            case 't':
            case '5':
            case 'z':
            case '\n':
            case '\r':
            {
                if ((p_ptr->py == y) && (p_ptr->px == x))
                {
                    done = true;
                }
                else if (valid_target || p_ptr->wizard)
                {
                    target_set_location(y, x);
                    health_track(0);
                    new_target = true;
                    done = true;
                }
                else
                {
                    bell("Illegal target.");
                }
                break;
            }

            default:
            {
                /* Extract a direction */
                d = target_dir(query);

                /* Oops */
                if (!d)
                    bell("Illegal command for target mode!");

                break;
            }
            }

            /* Handle "direction" */
            if (d)
            {
                /* Move */
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    /* Handle stuff */
                    handle_stuff();

                    /* Recalculate interesting grids */
                    get_sorted_target_list(mode, range);
                }
            }
        }

        /* Wizard dungeon modification */
        else
        {
            bool inc_monster = false;
            bool inc_object = false;
            bool inc_terrain = false;
            bool reroll_monster = false;
            bool reroll_object = false;
            bool found = false;

            y2 = y;
            x2 = x;

            // prepare the relevant prompt
            SDL_strlcpy(info, "<space>, <tab>, <dir>", sizeof(info));

            /* Describe and Prompt (enable "TARGET_LOOK") */
            query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info, use_story_look);

            /* Remove the path */
            if (mode & (TARGET_KILL))
                load_path(max, path, path_char, path_attr);

            /* Assume no direction */
            d = 0;

            // space increments (and is handled specially)
            if (query == ' ')
            {
                // increment a monster race
                if (cave_m_idx[y][x])
                    inc_monster = true;
                // increment an object kind
                else if (cave_o_idx[y][x])
                    inc_object = true;
                // increment a terrain type
                else
                    inc_terrain = true;
            }

            // tab rerolls (and is handled specially)
            if (query == '\t')
            {
                // reroll a monster race
                if (cave_m_idx[y][x])
                    reroll_monster = true;
                // reroll an object kind
                else if (cave_o_idx[y][x])
                    reroll_object = true;
            }

            // escape exits
            if (query == ESCAPE)
            {
                done = true;
            }

            // backspace changes the light level (and is handled specially)
            else if (query == '\b')
            {
                // toggle the cave_glow value
                if (cave_info[y][x] & (CAVE_GLOW))
                {
                    cave_info[y][x] &= ~(CAVE_GLOW);
                    if (cave_floorlike_bold(y, x))
                    {
                        cave_info[y][x] &= ~(CAVE_MARK);
                    }
                }
                else
                {
                    cave_info[y][x] |= (CAVE_GLOW);
                }

                update_view();
            }

            // numbers move
            else if (strchr("12346789", query))
            {
                /* Extract a direction */
                d = target_dir(query);
            }

            // summon a creature
            else if (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX"
                            "YZ&@",
                         query)
                || inc_monster || reroll_monster)
            {
                monster_race* r_ptr;
                monster_race* old_r_ptr;
                monster_type* m_ptr;

                // recreate a monster of the same type.
                if (reroll_monster)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    i = m_ptr->r_idx;
                    found = true;
                }

                // go through monster race list and find next monster with that
                // symbol.
                else if (inc_monster)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    old_r_ptr = &r_info[m_ptr->r_idx];

                    for (i = 1; i < z_info->r_max; i++)
                    {
                        r_ptr = &r_info[(i + m_ptr->r_idx) % z_info->r_max];

                        // stop when you find one
                        if ((r_ptr->d_char == old_r_ptr->d_char)
                            && (r_ptr->cur_num < r_ptr->max_num)
                            && (r_ptr->level <= 25))
                        {
                            found = true;
                            i = (i + m_ptr->r_idx) % z_info->r_max;
                            break;
                        }
                    }
                }

                // go through monster race list and find first monster with that
                // symbol.
                else
                {
                    for (i = 1; i < z_info->r_max; i++)
                    {
                        r_ptr = &r_info[i];

                        // stop when you find one
                        if (r_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing monster
                    if (cave_m_idx[y][x])
                    {
                        delete_monster_idx(cave_m_idx[y][x]);
                    }
                    // place the new one
                    place_monster_one(y, x, i, true, true, NULL);
                }
            }

            // create an object
            else if (strchr("([)|/\\]}-~*\"=_?!~,", query) || inc_object
                || reroll_object)
            {
                object_kind* old_k_ptr;
                object_type* o_ptr;
                object_kind* k_ptr;
                object_type* i_ptr;
                object_type object_type_body;

                // recreate an object of the same type.
                if (reroll_object)
                {
                    o_ptr = &o_list[cave_o_idx[y][x]];
                    i = o_ptr->k_idx;
                    found = true;
                }

                // go through object kind list and find next object kind with
                // that symbol.
                else if (inc_object)
                {
                    o_ptr = &o_list[cave_o_idx[y][x]];
                    old_k_ptr = &k_info[o_ptr->k_idx];

                    for (i = 1; i < z_info->k_max; i++)
                    {
                        k_ptr = &k_info[(i + o_ptr->k_idx) % z_info->k_max];

                        // stop when you find one
                        if (k_ptr->d_char == old_k_ptr->d_char)
                        {
                            found = true;
                            i = (i + o_ptr->k_idx) % z_info->k_max;
                            break;
                        }
                    }
                }

                // go through object kind list and find first object kind with
                // that symbol.
                else
                {
                    for (i = 1; i < z_info->k_max; i++)
                    {
                        k_ptr = &k_info[i];

                        // stop when you find one
                        if (k_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing item
                    if (cave_o_idx[y][x])
                    {
                        delete_object_idx(cave_o_idx[y][x]);
                    }

                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Create the item */
                    object_prep(i_ptr, i);

                    /* Apply magic (no messages, no artefacts) */
                    apply_magic(
                        i_ptr, p_ptr->depth, false, false, false, false);

                    // apply the autoinscription (if any)
                    apply_autoinscription(i_ptr);

                    if (i_ptr->tval == TV_ARROW)
                        i_ptr->number = 24;

                    /* Drop the object from heaven */
                    drop_near(i_ptr, -1, y, x);
                }
            }

            // change the terrain
            else if (strchr(".;'^+#:%0<>", query) || inc_terrain)
            {
                feature_type* f_ptr;
                feature_type* old_f_ptr;

                // go through terrain list and find next terrain type with that
                // symbol.
                if (inc_terrain)
                {
                    old_f_ptr = &f_info[cave_feat[y][x]];

                    for (i = 1; i < z_info->f_max; i++)
                    {
                        f_ptr = &f_info[(i + cave_feat[y][x]) % z_info->f_max];

                        // stop when you find one
                        if (f_ptr->d_char == old_f_ptr->d_char)
                        {
                            found = true;
                            i = (i + cave_feat[y][x]) % z_info->f_max;
                            break;
                        }
                    }
                }

                // go through terrain list and find first terrain type with that
                // symbol.
                else
                {
                    for (i = 1; i < z_info->f_max; i++)
                    {
                        f_ptr = &f_info[i];

                        // stop when you find one
                        if (f_ptr->d_char == query)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // if one was found, then place it
                if (found)
                {
                    // delete any existing monster
                    if (cave_m_idx[y][x])
                    {
                        delete_monster_idx(cave_m_idx[y][x]);
                    }
                    // delete any existing item
                    if (cave_o_idx[y][x])
                    {
                        delete_object_idx(cave_o_idx[y][x]);
                    }
                    // place the new terrain
                    cave_info[y][x] &= ~(CAVE_MARK);
                    cave_set_feat(y, x, i);
                    update_view();
                }
            }

            // unexpected symbol
            else if ((query != ' ') && (query != '\t'))
            {
                bell("Illegal command for target mode!");
            }

            /* Handle "direction" */
            if (d)
            {
                /* Move */
                x += ddx[d];
                y += ddy[d];

                /* Slide into legality */
                if (x >= p_ptr->cur_map_wid - 1)
                    x--;
                else if (x <= 0)
                    x++;

                /* Slide into legality */
                if (y >= p_ptr->cur_map_hgt - 1)
                    y--;
                else if (y <= 0)
                    y++;

                /* Adjust panel if needed */
                if (adjust_panel(y, x))
                {
                    /* Handle stuff */
                    handle_stuff();

                    /* Recalculate interesting grids */
                    get_sorted_target_list(mode, range);
                }
            }
        }
    }

    /* Forget */
    temp_n = 0;

    /* Clear the top line */
    prt("", 0, 0);

    /* Recenter around player */
    verify_panel();

    /* Handle stuff */
    handle_stuff();

    /* Failure to set target */
    if (!new_target)
    {
        // if we did not select a new target and were in targetting mode, then
        // abort target
        if (mode & (TARGET_KILL))
        {
            target_set_monster(0);
            health_track(0);
        }
        return (false);
    }

    /* Success */
    return (true);
}

/*
 * Takes a delta coordinates and returns a direction.
 * e.g. (1,0) is south, which is direction 2.
 */
int dir_from_delta(int deltay, int deltax)
{
    s16b dird[3][3] = { { 7, 8, 9 }, { 4, 5, 6 }, { 1, 2, 3 } };

    return (dird[deltay + 1][deltax + 1]);
}

/*
 * Gives the overall direction from point 1 to point 2.
 * Uses orthogonals when breaking ties.
 */
int rough_direction(int y1, int x1, int y2, int x2)
{
    int deltay = y2 - y1; // these represent the displacement
    int deltax = x2 - x1;

    int dy, dx; // these represent the direction

    // determine the main direction from the source to the target
    if (deltay == 0)
        dy = 0;
    else
        dy = (deltay > 0) ? 1 : -1;

    if (deltax == 0)
        dx = 0;
    else
        dx = (deltax > 0) ? 1 : -1;

    if ((deltax != 0) && (ABS(deltay) / ABS(deltax) >= 2))
        dx = 0;
    if ((deltay != 0) && (ABS(deltax) / ABS(deltay) >= 2))
        dy = 0;

    return (dir_from_delta(dy, dx));
}

/*
 * Get an "aiming direction" (1,2,3,4,6,7,8,9 or 5) from the user.
 *
 * Return true if a direction was chosen, otherwise return false.
 *
 * The direction "5" is special, and means "use current target".
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 *
 * Note that "Force Target", if set, will pre-empt user interaction,
 * if there is a usable target already set.
 *
 * If the range variable is 0, there is no range limit.
 *
 * Currently this function applies confusion directly.
 */
static void get_aim_prompt_label(
    int binding, cptr fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void get_aim_prompt(char* buf, size_t buflen, bool has_target)
{
    char fire_label[24];
    char select_label[24];
    char cancel_label[24];

    if (!buf || !buflen)
        return;

    if (!steamdeck_controls_active())
    {
        if (has_target)
        {
            SDL_strlcpy(buf,
                "Direction ('f'=target, 's'=select target, ESC)? ", buflen);
        }
        else
        {
            SDL_strlcpy(buf,
                "Direction ('f'=closest, 's'=select target, ESC)? ", buflen);
        }
        return;
    }

    get_aim_prompt_label('f', "B", fire_label, sizeof(fire_label));
    get_aim_prompt_label('s', "Y", select_label, sizeof(select_label));
    get_aim_prompt_label(ESCAPE, "Start", cancel_label, sizeof(cancel_label));

    if (has_target)
    {
        strnfmt(buf, buflen, "Dir (%s target, %s select target, %s cancel)? ",
            fire_label, select_label, cancel_label);
        if (strlen(buf) > 49)
            strnfmt(buf, buflen, "Dir (%s target, %s select, %s cancel)? ",
                fire_label, select_label, cancel_label);
    }
    else
    {
        strnfmt(buf, buflen, "Dir (%s closest, %s select target, %s cancel)? ",
            fire_label, select_label, cancel_label);
        if (strlen(buf) > 49)
            strnfmt(buf, buflen, "Dir (%s closest, %s select, %s cancel)? ",
                fire_label, select_label, cancel_label);
    }
}

bool get_aim_dir(int* dp, int range)
{
    int dir;

    char ch;

    char prompt[80];

#ifdef ALLOW_REPEAT

    if (repeat_pull(dp))
    {
        /* Verify */
        if (!(*dp == 5 && !target_okay(range)))
        {
            return (true);
        }
        else
        {
            /* Invalid repeat - reset it */
            repeat_clear();
        }
    }

#endif /* ALLOW_REPEAT */

    /* Initialize */
    (*dp) = 0;

    /* Global direction */
    dir = p_ptr->command_dir;
    if ((dir == 5) && !target_okay(range))
        dir = 0;
    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
        dir = 0;

    /* Hack -- auto-target if requested */
    //	if (use_old_target && target_okay(range)) dir = 5;

    /* Ask until satisfied */
    while (!dir)
    {
        bool has_target = target_okay(range);

        /* Choose a prompt */
        get_aim_prompt(prompt, sizeof(prompt), has_target);

        /* Get a command (or Cancel) */
        if (!get_com(prompt, &ch))
            break;

        /* Analyze */
        switch (ch)
        {
        /* Set new target, use target if legal */
        case 's':
        case '*':
        {
            if (target_set_interactive(TARGET_KILL, range))
                dir = 5;
            break;
        }

        /* Use current target, if set and legal, otherwise pick next target */
        case 'f':
        case 'F':
        case 't':
        case '5':
        case 'z':
        {
            if (target_okay(range))
                dir = 5;
            else
            {
                /* Prepare the "temp" array */
                get_sorted_target_list(TARGET_KILL, range);

                /* Monster */
                if (temp_n)
                {
                    target_set_monster(cave_m_idx[temp_y[0]][temp_x[0]]);
                    health_track(cave_m_idx[temp_y[0]][temp_x[0]]);
                    dir = 5;
                }
            }
            break;
        }

            // Sil-y: there is some chance that these UP and DOWN things
            //        will cause trouble elsewhere

        case '>':
        {
            dir = DIRECTION_DOWN;
            break;
        }
        case '<':
        {
            dir = DIRECTION_UP;
            break;
        }

        /* Possible direction */
        default:
        {
            dir = target_dir(ch);
            if ((dir == 5) && !target_okay(range))
                dir = 0;
            break;
        }
        }

        /* Error */
        if (!dir)
            bell("Illegal aim direction!");
    }

    /* No direction */
    if (!dir)
    {
        return (false);
    }

    /* Save the direction */
    p_ptr->command_dir = dir;

    /* Check for confusion */
    // Sil-y: Doesn't use the new confusion method, but might be difficult to
    // use it
    if ((dir != DIRECTION_UP) && (dir != DIRECTION_DOWN) && p_ptr->confused)
    {
        /* Randomish direction */
        dir = ddd[rand_int(8)];
    }

    /* Notice confusion */
    if (p_ptr->command_dir != dir)
    {
        /* Warn the user */
        msg_print("You are confused.");
    }

    /* Save direction */
    (*dp) = dir;

#ifdef ALLOW_REPEAT

    repeat_push(dir);

#endif /* ALLOW_REPEAT */

    /* A "valid" direction was entered */
    return (true);
}

/*
 * Request a "movement" direction (1,2,3,4,5,6,7,8,9) from the user.
 *
 * Return true if a direction was chosen, otherwise return false.
 *
 * Direction "0" is illegal and will not be accepted.
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 */
bool get_rep_dir(int* dp)
{
    int dir;

    char ch;

    cptr p;

#ifdef ALLOW_REPEAT

    if (repeat_pull(dp))
    {
        return (true);
    }

#endif /* ALLOW_REPEAT */

    /* Initialize */
    (*dp) = 0;

    /* Global direction */
    dir = p_ptr->command_dir;

    /* Get a direction */
    while (!dir)
    {
        /* Choose a prompt */
        p = "Direction (Escape to cancel)? ";

        /* Get a command (or Cancel) */
        if (!get_com(p, &ch))
            break;

        /* Convert keypress into a direction */
        dir = target_dir(ch);

        /* Oops */
        if (!dir)
            bell("Illegal repeatable direction!");
    }

    /* Aborted */
    if (!dir)
        return (false);

    /* Save desired direction */
    p_ptr->command_dir = dir;

    /* Save direction */
    (*dp) = dir;

#ifdef ALLOW_REPEAT

    repeat_push(dir);

#endif /* ALLOW_REPEAT */

    /* Success */
    return (true);
}

/*
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return true if direction changes.
 */
bool confuse_dir(int* dp)
{
    int dir;
    int i;

    /* Default */
    dir = (*dp);

    /* Apply "confusion" */
    if (p_ptr->confused)
    {
        /* If no direction given, then completely randomise it */
        if (dir == 5)
        {
            /* Random direction */
            dir = ddd[rand_int(8)];
        }
        else
        {
            // gives 3 chances to be turned left and 3 chances to be turned
            // right leads to a binomial distribution of direction around the
            // intended one:
            //
            // 15 20 15
            //  6     6   (chances are all out of 64)
            //  1  0  1

            i = damroll(3, 2) - damroll(3, 2);

            dir = cycle[chome[*dp] + i];
        }
    }

    /* Notice confusion */
    if ((*dp) != dir)
    {
        /* Warn the user */
        msg_print("You are confused.");

        /* Save direction */
        (*dp) = dir;

        /* Confused */
        return (true);
    }

    /* Not confused */
    return (false);
}

const char entry_poetry[][100] = { { "Into the vast and echoing gloom," },
    { "more dread than many-tunnelled tomb" },
    //	{ "in labyrinthine pyramid" },
    //	{ "where everlasting death is hid," },
    { "  down awful corridors that wind" },
    { "    down to a menace dark enshrined;" },
    { "      down to the mountain's roots profound," },
    { "devoured, tormented, bored and ground" },
    { "by seething vermin spawned of stone;" },
    { "  down to the depths they went alone..." },

    { "" } };

const char tutorial_leave_text[][100] = {
    { "You have finished the first half of the tutorial and are ready" },
    { "to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_win_text[][100] = {
    { "Congratulations. You have survived a fire-drake (usually found" },
    { "at 900 ft!), and have finished the tutorial in fine form." },
    { "You are more than ready to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_early_death_text[][100] = { { "You have been slain." },
    { " " },
    { "A key feature of Sil (and all Roguelike games) is that you cannot" },
    { "use savepoints: if you die, that's it!" },
    { "It is thus a challenging game where you need to really *think*." },
    { " " },
    { "However, it is a bit frustrating to die before the end of the" },
    { "tutorial, so we evidentally made it a bit too deadly." }, { " " },
    { "Just restart the tutorial and you should be back to where you" },
    { "were in a couple of minutes. Remember that if combat is not going" },
    { "your way, you can try to escape and heal, then either come back" },
    { "and again to defeat your adversary, or simply ignore it." },

    { "" } };

const char tutorial_late_death_text[][100] = {
    { "Congratulations: you have finished the tutorial." }, { " " },
    { "You have also just been through a rite of passage: dying." },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },
    { " " },
    { "You are now more than ready to create a character and start playing." },
    { " " }, { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." },

    { "" }
};

const char throne_poetry[][100] = { { "Loud rose a din of laughter hoarse," },
    { "  self-loathing yet without remorse;" },
    { "    loud came a singing harsh and fierce" },
    { "      like swords of terror souls to pierce." },
    { "Red was the glare through open doors" },
    { "  of firelight mirrored on brazen floors," },
    { "    and up the arches towering clomb" },
    { "      to glooms unguessed, to vaulted dome" },
    { "        swathed in wavering smokes and steams" },
    { "          stabbed with flickering lightning-gleams." },

    { "" } };

/*
const char throne_poetry2[][100] =
{
        { "To Morgoth's hall, where dreadful feast" },
        { "he held, and drank the blood of beast" },
        { "and lives of Men, she stumbling came:" },
        { "her eyes were dazed with smoke and flame." },
        { "The pillars, reared like monstrous shores" },
        { "to bear earth's overwhelming floors," },
        { "were devil-carven, shaped with skill" },
        { "such as unholy dreams doth fill:" },
        { "they towered like trees into the air," },
        { "whose trunks are rooted in despair," },
        { "whose shade is death, whose fruit is bane," },
        { "whose boughs like serpents writhe in pain." },
        { "Beneath them ranged with spear and sword" },
        { "stood Morgoth's sable-armoured horde:" },
        { "the fire on blade and boss of shield" },
        { "was red as blood on stricken field." },
        { "Beneath a monstrous column loomed" },
        { "the throne of Morgoth, and the doomed" },
        { "and dying gasped upon the floor:" },
        { "his hideous footstool, rape of war." },

        { "" }
};
*/

const char ultimate_bug_text[][100]
    = { { "Against all hope, you defeated the Dark Enemy," },
          { "  and destroyed his physical form." },
          { "    For the rest of this age at least," },
          { "      Arda shall be free from the tyrant's shadow." },
          { "But there will be time later for reflection" },
          { "  on this great change to Arda's fate." },
          { "    You are buried still in Angband's vaults" },
          { "      -- make quick your bold escape!" },

          { "" } };

static int pause_with_text_print_wrapped_segment(int row, int col, byte attr,
                                                 cptr text, int delay_msec)
{
    int term_wid = 80;
    int term_hgt = 24;
    int max_cols;
    int wrap_col;
    int rows_used = 1;

    if (!text)
        text = "";

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    if (row < 0 || row >= term_hgt)
        return 0;

    if (col < 0)
        col = 0;
    if (col >= term_wid)
        col = term_wid - 1;

    max_cols = term_wid - col - 2;
    if (max_cols < 1)
        max_cols = 1;

    wrap_col = col + max_cols;

    if (*text)
    {
        if (sdl_is_story_font_enabled())
            rows_used = count_wrapped_lines_story(text, wrap_col, col);
        else
            rows_used = count_wrapped_lines(text, wrap_col, col);

        if (rows_used < 1)
            rows_used = 1;
    }

    story_print_text(row, col, max_cols, attr, text);
    Term_fresh();

    if (delay_msec > 0)
        Term_xtra(TERM_XTRA_DELAY, delay_msec);

    return rows_used;
}

static int pause_with_text_count_wrapped_segment(int col, cptr text)
{
    int term_wid = 80;
    int term_hgt = 24;
    int max_cols;
    int wrap_col;
    int rows_used = 1;

    if (!text)
        text = "";

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    (void)term_hgt;

    if (col < 0)
        col = 0;
    if (col >= term_wid)
        col = term_wid - 1;

    max_cols = term_wid - col - 2;
    if (max_cols < 1)
        max_cols = 1;

    wrap_col = col + max_cols;

    if (*text)
    {
        if (sdl_is_story_font_enabled())
            rows_used = count_wrapped_lines_story(text, wrap_col, col);
        else
            rows_used = count_wrapped_lines(text, wrap_col, col);

        if (rows_used < 1)
            rows_used = 1;
    }

    return rows_used;
}

/* pause_with_text: prints name+alt, explicit blank line, then wrapped start splits */
void pause_with_text(const char desc[][100], int row, int col,
                     const char extra[][100], byte extra_attr)
{
    int i_main = 0, msec = 50;
    int banner_lines = 0;
    int main_rows = 0;
    int term_wid = 80;
    int term_hgt = 24;
    int banner_col;
    int tail_col;
    bool show_banner_gap = true;
    bool show_stanza_gap = true;

    /* 0. save & clear screen */
    screen_save();
    Term_clear();
    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    sdl_story_font_enable();
    log_debug("Banner: story font enabled");

    banner_col = col - 5;
    if (banner_col < 0)
        banner_col = 0;
    tail_col = banner_col + 4;
    if (tail_col < 0)
        tail_col = 0;

    if (extra)
    {
        int n_extra = 0;

        banner_lines += pause_with_text_count_wrapped_segment(banner_col, extra[0]);
        while (extra[n_extra][0])
            n_extra++;

        if (n_extra > 1)
        {
            for (int i = 1; i < n_extra; ++i)
            {
                int segment_col = (i == n_extra - 1) ? tail_col : banner_col;
                banner_lines += pause_with_text_count_wrapped_segment(segment_col, extra[i]);
            }
        }
        else
        {
            show_banner_gap = false;
            show_stanza_gap = false;
        }
    }
    else
    {
        show_banner_gap = false;
        show_stanza_gap = false;
    }

    if (show_banner_gap)
        banner_lines++;
    if (show_stanza_gap)
        banner_lines++;

    while (desc && desc[i_main][0])
    {
        main_rows += pause_with_text_count_wrapped_segment(col, desc[i_main]);
        ++i_main;
    }

    if ((banner_lines + main_rows) > term_hgt && show_stanza_gap)
    {
        banner_lines--;
        show_stanza_gap = false;
    }

    if ((banner_lines + main_rows) > term_hgt && show_banner_gap)
    {
        banner_lines--;
        show_banner_gap = false;
    }

    {
        int total_rows = banner_lines + main_rows;

        if (extra)
        {
            row = (term_hgt - total_rows) / 2;
        }
        else
        {
            int slack = term_hgt - total_rows;

            if (row < 0)
                row = 0;
            if (slack < 0)
                slack = 0;
            if (row > slack)
                row = slack;
        }

        if (row < 0)
            row = 0;
    }

    banner_lines = 0;
    main_rows = 0;

    /* 1. optional banner */
    if (extra) {
        int n_extra = 0;

        while (extra[n_extra][0]) n_extra++;

        /* Line 1: name+alt */
        banner_lines += pause_with_text_print_wrapped_segment(
            row + banner_lines, banner_col, extra_attr, extra[0], msec);

        /* Line 2: blank line */
        if (show_banner_gap)
        {
            banner_lines += pause_with_text_print_wrapped_segment(
                row + banner_lines, banner_col, extra_attr, "", msec);
        }

        /* Lines 3+: start splits, last one shifted further right */
        for (int i = 1; i < n_extra; ++i) {
            int shift = (i == n_extra - 1) ? tail_col : banner_col;
            banner_lines += pause_with_text_print_wrapped_segment(
                row + banner_lines, shift, extra_attr, extra[i], msec);
        }

        /* separator before stanza */
        if (show_stanza_gap)
            banner_lines++;
    }

    /* 2. main stanza */
    i_main = 0;
    while (desc && desc[i_main][0]) {
        main_rows += pause_with_text_print_wrapped_segment(
            row + banner_lines + main_rows, col, TERM_WHITE, desc[i_main], msec);
        ++i_main;
    }

    log_debug("Banner: story font disabled");
    sdl_story_font_disable();

    /* 3. wait for key */
    hide_cursor = true;
    (void)inkey();
    hide_cursor = false;

    /* 4. wipe the area used */
    int total = banner_lines + main_rows;
    int max_row = MIN(row + total, term_hgt);
    for (int j = row; j < max_row; ++j) {
        Term_erase(0, j, 255);
    }

    screen_load();
}

/*
 * Select a suitable unique monster for the Tulkas quest
 */
static bool tulkas_target_valid(int r_idx, const monster_race* r_ptr, int depth)
{
    if (!r_ptr) return false;

    if (!(r_ptr->flags1 & RF1_UNIQUE)) return false;
    if (r_ptr->max_num <= 0) return false;
    if (r_ptr->level < depth) return false;
    if (r_ptr->level > MORGOTH_DEPTH) return false;
    if (r_idx == R_IDX_TULKAS || r_idx == R_IDX_MORGOTH) return false;

    return true;
}

static bool tulkas_has_valid_target(int depth)
{
    int i;

    if (!z_info) return false;

    for (i = 1; i < z_info->r_max; i++)
    {
        if (tulkas_target_valid(i, &r_info[i], depth)) return true;
    }

    return false;
}

static int select_tulkas_quest_target(void)
{
    int i;
    int valid_targets[50];
    int count = 0;
    
    log_trace("select_tulkas_quest_target: z_info=%p, r_max=%d", z_info, z_info ? z_info->r_max : -1);
    
    if (!z_info) 
    {
        log_trace("z_info is NULL!");
        return 0;
    }
    
    /* Look for unique monsters at current depth or deeper */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        
        if (!r_ptr) 
        {
            log_trace("r_info[%d] returned NULL", i);
            continue;
        }
        
        /* Must be unique, alive (max_num > 0), and at appropriate depth */
        /* Exclude Tulkas himself and Morgoth from being targets */
        if (tulkas_target_valid(i, r_ptr, p_ptr->depth))
        {
            valid_targets[count] = i;
            count++;
            if (count >= 50) break; /* Safety limit */
        }
    }
    
    if (count == 0) {
        log_trace("select_tulkas_quest_target: No valid unique targets found");
        return 0; /* No valid targets */
    }
    
    log_trace("select_tulkas_quest_target: Found %d valid unique targets", count);
    return valid_targets[rand_int(count)];
}

/*
 * Select a suitable artifact prize for the Tulkas quest
 */
static int select_tulkas_quest_prize(int target_level)
{
    int i;
    int valid_prizes[100];
    int count = 0;
    int max_artifact_level = target_level + 6; /* Not more than 6 levels deeper */
    
    log_trace("select_tulkas_quest_prize: target_level=%d, max_artifact_level=%d, z_info=%p, art_max=%d", 
              target_level, max_artifact_level, z_info, z_info ? z_info->art_max : -1);
    
    if (!z_info) 
    {
        log_trace("z_info is NULL!");
        return 0;
    }
    
    if (!valar_reserved_artifacts)
    {
        log_trace("valar_reserved_artifacts is NULL! Initializing...");
        
        /* Initialize the array if it doesn't exist */
        if (z_info && z_info->art_max > 0) {
            valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
            for (int j = 0; j < z_info->art_max; j++) {
                valar_reserved_artifacts[j] = false;
            }
            log_trace("Initialized valar_reserved_artifacts with %d entries", z_info->art_max);
        } else {
            log_error("Cannot initialize valar_reserved_artifacts: z_info=%p, art_max=%d", 
                     z_info, z_info ? z_info->art_max : -1);
            return 0;
        }
    }
    
    /* First pass: Look for artifacts with rarity >= 10 within depth constraint */
    for (i = 1; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        
        if (!a_ptr) 
        {
            log_trace("a_info[%d] returned NULL", i);
            continue;
        }
        
        /* Must be high rarity, within depth constraint, and not yet created */
        if ((a_ptr->rarity >= 10) &&
            (a_ptr->level >= target_level) &&
            (a_ptr->level <= max_artifact_level) &&
            (a_ptr->cur_num == 0) &&
            !valar_reserved_artifacts[i])
        {
            valid_prizes[count] = i;
            count++;
            if (count >= 100) break; /* Safety limit */
        }
    }
    
    /* If no suitable artifacts found with rarity >= 10, increase level requirement */
    if (count == 0)
    {
        log_trace("No artifacts found with rarity >= 10 within depth constraint, relaxing requirements");
        max_artifact_level = MORGOTH_DEPTH; /* Remove depth constraint */
        
        /* Second pass: Look for any artifacts with rarity >= 10 regardless of depth */
        for (i = 1; i < z_info->art_max; i++)
        {
            artefact_type* a_ptr = &a_info[i];
            
            if (!a_ptr) continue;
            
            /* Must be high rarity, appropriate level, and not yet created */
            if ((a_ptr->rarity >= 10) &&
                (a_ptr->level >= target_level) &&
                (a_ptr->cur_num == 0) &&
                !valar_reserved_artifacts[i])
            {
                valid_prizes[count] = i;
                count++;
                if (count >= 100) break; /* Safety limit */
            }
        }
        
        /* Third pass: If still no artifacts, take highest level artifact available */
        if (count == 0)
        {
            log_trace("No artifacts found with rarity >= 10, looking for highest level artifact");
            int best_artifact = 0;
            int best_level = 0;
            
            for (i = 1; i < z_info->art_max; i++)
            {
                artefact_type* a_ptr = &a_info[i];
                
                if (!a_ptr) continue;
                
                /* Must be high rarity and not yet created, ignore level constraint */
                if ((a_ptr->rarity >= 10) &&
                    (a_ptr->cur_num == 0) &&
                    !valar_reserved_artifacts[i] &&
                    (a_ptr->level > best_level))
                {
                    best_artifact = i;
                    best_level = a_ptr->level;
                }
            }
            
            if (best_artifact > 0)
            {
                valid_prizes[0] = best_artifact;
                count = 1;
                log_trace("Selected highest level artifact: %d (level %d)", best_artifact, best_level);
            }
        }
        
        if (count > 0)
        {
            log_trace("Found %d artifacts after relaxing depth constraint", count);
        }
    }
    else
    {
        log_trace("Found %d artifacts with rarity >= 10 within depth constraint", count);
    }
    
    if (count == 0) return 0; /* No valid prizes */
    
    return valid_prizes[rand_int(count)];
}

/*
 * Get metarun quest flag from quest index by looking up the M: field in quest.txt
 * Returns 0 if quest has no metarun tracking or quest_idx is invalid
 */
static u32b get_metarun_quest_flag(int quest_idx)
{
    quest_type* q_ptr;
    const char* metarun_id;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return 0;
    
    q_ptr = &quest_info[quest_idx];
    
    /* Get the metarun quest ID string from the M: field */
    if (q_ptr->metarun_quest_id == 0) return 0;
    metarun_id = quest_name_text + q_ptr->metarun_quest_id;
    
    /* Map the string to the corresponding flag value */
    if (streq(metarun_id, "METARUN_QUEST_TULKAS")) return METARUN_QUEST_TULKAS;
    if (streq(metarun_id, "METARUN_QUEST_AULE")) return METARUN_QUEST_AULE;
    if (streq(metarun_id, "METARUN_QUEST_MANDOS")) return METARUN_QUEST_MANDOS;
    if (streq(metarun_id, "METARUN_QUEST_NIENA")) return METARUN_QUEST_NIENA;
    if (streq(metarun_id, "METARUN_QUEST_OROME")) return METARUN_QUEST_OROME;
    if (streq(metarun_id, "METARUN_QUEST_VARDA")) return METARUN_QUEST_VARDA;
    
    /* Unknown or future quest */
    log_debug("get_metarun_quest_flag: Unknown metarun_quest_id '%s' for quest_idx %d", metarun_id, quest_idx);
    return 0;
}

/*
 * Apply quest rewards (stats, skills, abilities) based on quest.txt data
 */
void apply_quest_rewards(int quest_idx)
{
    quest_type* q_ptr;
    
    /* Validate quest index */
    if (!p_ptr || quest_idx <= 0 || quest_idx >= z_info->quest_max) return;
    
    q_ptr = &quest_info[quest_idx];
    
    /* Apply stat bonuses */
    for (int i = 0; i < 4; i++) {
        if (q_ptr->stat_bonuses[i] > 0) {
            /* stat_bonuses array: [str, dex, con, gra] */
            int stat_idx = i; /* A_STR=0, A_DEX=1, A_CON=2, A_GRA=3 */
            
            for (int j = 0; j < q_ptr->stat_bonuses[i]; j++) {
                if (p_ptr->stat_base[stat_idx] < BASE_STAT_MAX) {
                    p_ptr->stat_base[stat_idx]++;
                }
            }
            
            log_trace("Applied %s bonus: +%d", 
                     (i == 0 ? "STR" : i == 1 ? "DEX" : i == 2 ? "CON" : "GRA"), 
                     q_ptr->stat_bonuses[i]);
        }
    }
    
    /* Apply skill bonus */
    if (q_ptr->skill_bonus > 0 && q_ptr->skill_type < S_MAX) {
        p_ptr->skill_base[q_ptr->skill_type] += q_ptr->skill_bonus;

        switch (q_ptr->skill_type) {
            case S_MEL:
                log_trace("Applied Melee bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_ARC:
                log_trace("Applied Archery bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_EVN:
                log_trace("Applied Evasion bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_STL:
                log_trace("Applied Stealth bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_PER:
                log_trace("Applied Perception bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_WIL:
                log_trace("Applied Will bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_SMT:
                log_trace("Applied Smithing bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_SNG:
                log_trace("Applied Song bonus: +%d", q_ptr->skill_bonus);
                break;
            default:
                log_trace("Applied skill bonus: skill=%d bonus=+%d",
                          q_ptr->skill_type, q_ptr->skill_bonus);
                break;
        }
    }
    
    /* Apply special ability */
    if (q_ptr->ability_type > 0 && q_ptr->ability_id < ABILITIES_MAX) {
        /* ability_type 8 is the Special skill type (S_SPC) based on quest.txt */
        if (q_ptr->ability_type == 8) {
            /* Grant the special ability using the ability_id from quest.txt */
            if (!p_ptr->have_ability[S_SPC][q_ptr->ability_id]) {
                p_ptr->have_ability[S_SPC][q_ptr->ability_id] = true;
                p_ptr->innate_ability[S_SPC][q_ptr->ability_id] = true;
                p_ptr->active_ability[S_SPC][q_ptr->ability_id] = true;
                ability_log_record_gain(S_SPC, q_ptr->ability_id);
                
                /* Get the ability name for the message */
                ability_type* b_ptr = &b_info[ability_index(S_SPC, q_ptr->ability_id)];
                if (b_ptr && b_ptr->name && b_name) {
                    msg_format("You have learned %s!", b_name + b_ptr->name);
                    log_trace("Applied special ability: %s (skill=%d, ability=%d)", 
                             b_name + b_ptr->name, q_ptr->ability_type, q_ptr->ability_id);
                } else {
                    msg_print("You have gained a new special ability!");
                    log_trace("Applied special ability: Unknown name (skill=%d, ability=%d)", 
                             q_ptr->ability_type, q_ptr->ability_id);
                }
            } else {
                /* Already have this ability */
                ability_type* b_ptr = &b_info[ability_index(S_SPC, q_ptr->ability_id)];
                if (b_ptr && b_ptr->name && b_name) {
                    msg_format("You already possess %s.", b_name + b_ptr->name);
                } else {
                    msg_print("You already possess this special ability.");
                }
                log_trace("Special ability already granted: skill=%d, ability=%d", 
                         q_ptr->ability_type, q_ptr->ability_id);
            }
        }
        else {
            log_trace("Unknown ability type: %d (not implemented)", q_ptr->ability_type);
        }
    }
    
    /* Recalculate bonuses and redraw */
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATS);
}

/*
 * Get oath ID from quest data
 */
int get_quest_oath_id(int quest_idx)
{
    quest_type* q_ptr;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return 0; /* No oath */
    
    q_ptr = &quest_info[quest_idx];
    return q_ptr->oath_id;
}

/*
 * Check quest eligibility based on E: field data and standard quest requirements
 * Includes metarun completion checks and quest state checks
 */
bool check_quest_eligibility(int quest_idx, int depth)
{
    quest_type* q_ptr;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return false;
    
    q_ptr = &quest_info[quest_idx];
    
    /* Debug quest 2 specifically - show what was actually loaded */
    if (quest_idx == 2) {
        log_trace("Quest %d (Aule) LOADED DATA: eligibility_type=%d, eligibility_skill=%d, eligibility_value=%d", 
                 quest_idx, q_ptr->eligibility_type, q_ptr->eligibility_skill, q_ptr->eligibility_value);
        log_trace("Quest %d (Aule) LOADED DATA: This data was loaded from save file, not parsed from quest.txt", quest_idx);
    }
    
    /* Check standard requirements that apply to all quests */
    /* 1. Quest not already completed in current metarun unless oath override applies */
    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int metarun_count = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    bool oath_override = false;
    if (q_ptr->oath_id > 0 && p_ptr && p_ptr->oath_type == q_ptr->oath_id && !oath_invalid(q_ptr->oath_id)) {
        oath_override = true;
    }
    
    if (metarun_flag) {
        if (metarun_count >= METARUN_QUEST_COMPLETION_CAP) {
            log_trace("Quest %d eligibility: METARUN_CAP (%d/%d) = FAIL", quest_idx, metarun_count, METARUN_QUEST_COMPLETION_CAP);
            return false;
        }
        if (metarun_count > 0 && !oath_override) {
            log_trace("Quest %d eligibility: METARUN_COMPLETED (count=%d) without oath override = FAIL", quest_idx, metarun_count);
            return false;
        }
        if (metarun_count > 0 && oath_override) {
            log_trace("Quest %d eligibility: metarun completion count=%d overridden by active oath %d", quest_idx, metarun_count, q_ptr->oath_id);
        }
    }
    
    /* 2. Check quest-specific state (must not be started for roulette quests) */
    switch (quest_idx) {
        case 1: /* Tulkas */
            if (p_ptr->tulkas_quest != TULKAS_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: TULKAS_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            if (!tulkas_has_valid_target(depth)) {
                log_trace("Quest %d eligibility: TULKAS_NO_TARGETS = FAIL (depth=%d)", quest_idx, depth);
                return false;
            }
            break;
        case 4: /* Niena */
            if (p_ptr->niena_quest != NIENA_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: NIENA_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
    }
    
    /* 3. Only one quest per run (for roulette quests) */
    if (q_ptr->quest_type == 1 && p_ptr->quest_reserved[0]) { /* Y:1 = roulette quest */
        log_trace("Quest %d eligibility: QUEST_RESERVED = FAIL", quest_idx);
        return false;
    }
    
    /* Check eligibility type from E: field */
    switch (q_ptr->eligibility_type) {
        case 0: /* No requirements */
            log_trace("Quest %d eligibility: NO_REQUIREMENTS = PASS", quest_idx);
            return true;
            
        case 1: /* SKILL_MIN - minimum skill requirement */
            if (q_ptr->eligibility_skill < S_MAX) {
                int player_skill = p_ptr->skill_base[q_ptr->eligibility_skill];
                bool meets_req = (player_skill >= q_ptr->eligibility_value);
                
                /* Enhanced logging for debugging smith skill issues */
                if (q_ptr->eligibility_skill == S_SMT) {
                    log_trace("SMT SKILL DEBUG: Quest %d eligibility check:", quest_idx);
                    log_trace("  skill_base[S_SMT] = %d", p_ptr->skill_base[S_SMT]);
                    log_trace("  skill_use[S_SMT] = %d", p_ptr->skill_use[S_SMT]);
                    log_trace("  required value = %d", q_ptr->eligibility_value);
                    log_trace("  meets requirement = %s", meets_req ? "YES" : "NO");
                }
                
                log_trace("Quest %d eligibility: SKILL_MIN (skill=%d, player=%d, required=%d) = %s",
                         quest_idx, q_ptr->eligibility_skill, player_skill, q_ptr->eligibility_value,
                         meets_req ? "PASS" : "FAIL");
                return meets_req;
            }
            return false;
            
        case 2: /* SKILL_RANGE - skill requirement within depth range */
            if (q_ptr->eligibility_skill < S_MAX && 
                depth >= q_ptr->eligibility_depth_min && 
                depth <= q_ptr->eligibility_depth_max) {
                int player_skill = p_ptr->skill_base[q_ptr->eligibility_skill];
                bool meets_req = (player_skill >= q_ptr->eligibility_value);
                log_trace("Quest %d eligibility: SKILL_RANGE (skill=%d, player=%d, required=%d, depth=%d in %d-%d) = %s",
                         quest_idx, q_ptr->eligibility_skill, player_skill, q_ptr->eligibility_value,
                         depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max,
                         meets_req ? "PASS" : "FAIL");
                return meets_req;
            }
            log_trace("Quest %d eligibility: SKILL_RANGE (depth=%d NOT in %d-%d) = FAIL",
                     quest_idx, depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max);
            return false;
            
        case 3: /* DEPTH_RANGE - must be within depth range */
            {
                bool in_range = (depth >= q_ptr->eligibility_depth_min && depth <= q_ptr->eligibility_depth_max);
                log_trace("Quest %d eligibility: DEPTH_RANGE (depth=%d in %d-%d) = %s",
                         quest_idx, depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max,
                         in_range ? "PASS" : "FAIL");
                return in_range;
            }
            
        default:
            log_trace("Quest %d eligibility: Unknown type %d = FAIL", quest_idx, q_ptr->eligibility_type);
            return false;
    }
}

/*
 * Extract quest initialization texts from quest data
 * Returns array of text strings split by paragraph breaks
 */
cptr* extract_quest_init_texts(int quest_idx, int* count)
{
    quest_type* q_ptr;
    cptr full_text;
    cptr* texts;
    char* text_copy;
    char* line_start;
    char* line_end;
    int text_count = 0;
    int max_texts = 20; /* Maximum expected paragraphs */
    int len;
    
    /* Initialize count */
    if (count) *count = 0;
    else return NULL;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return NULL;
    
    q_ptr = &quest_info[quest_idx];
    if (!q_ptr->init_text) return NULL;
    
    /* Get the initialization text */
    full_text = q_text + q_ptr->init_text;
    if (!full_text || strlen(full_text) == 0) return NULL;
    
    /* Allocate text array */
    texts = mem_alloc_array(max_texts, cptr);
    if (!texts) return NULL;
    
    /* Create a working copy of the text */
    len = strlen(full_text);
    text_copy = mem_alloc_array(len + 1, char);
    if (!text_copy) {
        mem_free_null(texts);
        return NULL;
    }
    SDL_strlcpy(text_copy, full_text, len + 1);
    
    /* Split text by single newlines (each I: line becomes an entry) */
    line_start = text_copy;
    while (line_start && *line_start && text_count < max_texts - 1) {
        /* Find the end of this line */
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }
        
        /* Store the line (even if empty - empty lines become paragraph breaks) */
        texts[text_count] = str_dup(line_start);
        if (texts[text_count]) text_count++;
        
        /* Move to next line */
        if (line_end) {
            line_start = line_end + 1;
        } else {
            /* No more lines to process */
            break;
        }
    }
    
    /* Clean up */
    mem_free_null(text_copy);
    
    *count = text_count;
    return texts;
}

/*
 * Extract quest completion texts from quest data
 * Returns array of text strings split by paragraph breaks
 */
cptr* extract_quest_completion_texts(int quest_idx, int* count)
{
    quest_type* q_ptr;
    cptr full_text;
    cptr* texts;
    char* text_copy;
    char* line_start;
    char* line_end;
    int text_count = 0;
    int max_texts = 20; /* Maximum expected paragraphs */
    int len;
    
    /* Initialize count */
    if (count) *count = 0;
    else return NULL;
    
    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return NULL;
    
    q_ptr = &quest_info[quest_idx];
    if (!q_ptr->completion_text) return NULL;
    
    /* Get the completion text */
    full_text = q_text + q_ptr->completion_text;
    if (!full_text || strlen(full_text) == 0) return NULL;
    
    /* Allocate text array */
    texts = mem_alloc_array(max_texts, cptr);
    if (!texts) return NULL;
    
    /* Create a working copy of the text */
    len = strlen(full_text);
    text_copy = mem_alloc_array(len + 1, char);
    if (!text_copy) {
        mem_free_null(texts);
        return NULL;
    }
    SDL_strlcpy(text_copy, full_text, len + 1);
    
    /* Split text by single newlines (each W: line becomes an entry) */
    line_start = text_copy;
    while (line_start && text_count < max_texts - 1) {
        /* Find the end of this line */
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }
        
        /* Store the line (even if empty - empty lines become paragraph breaks) */
        texts[text_count] = str_dup(line_start);
        if (texts[text_count]) text_count++;
        
        /* Move to next line */
        if (line_end) {
            line_start = line_end + 1;
        } else {
            /* This was the last line, we're done */
            break;
        }
    }
    
    /* Clean up */
    mem_free_null(text_copy);
    
    *count = text_count;
    return texts;
}

/* Forward declarations for quest text helpers used before their definitions */
static cptr get_quest_title(int quest_idx);
static cptr get_oath_name_from_id(byte oath_id);

/* Prepend a repeat-attempt context line when returning to a Valar quest under an oath */
static cptr* prepend_repeat_context(int quest_idx, cptr* texts, int* count, bool is_completion)
{
    if (!texts || !count || quest_idx <= 0 || quest_idx >= z_info->quest_max) return texts;

    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr || q_ptr->oath_id <= 0) return texts;
    if (!p_ptr || p_ptr->oath_type != q_ptr->oath_id || oath_invalid(q_ptr->oath_id)) return texts;

    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int previous = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    if (previous <= 0) return texts; /* First attempt - no alternate text */

    cptr quest_title = get_quest_title(quest_idx);
    cptr oath_name = get_oath_name_from_id(q_ptr->oath_id);

    char repeat_line[180];
    strnfmt(repeat_line, sizeof(repeat_line),
            is_completion
                ? "%s honors your %s oath after %d earlier success%s."
                : "%s returns under your %s oath; you have succeeded %d time%s before.",
            quest_title ? quest_title : "This quest",
            oath_name ? oath_name : "oath",
            previous,
            (previous == 1 ? "" : "s"));

    cptr* new_texts = mem_alloc_array(*count + 1, cptr);
    if (!new_texts) return texts;
    new_texts[0] = str_dup(repeat_line);
    for (int i = 0; i < *count; i++) new_texts[i + 1] = texts[i];
    mem_free_null(texts); /* free only the array container; strings move to new_texts */
    (*count)++;
    return new_texts;
}

/*
 * Get quest title from quest data
 */
static cptr get_quest_title(int quest_idx)
{
    log_trace("QUEST TITLE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST TITLE: Invalid bounds check, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST TITLE: q_ptr is NULL, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    if (q_ptr->title_text && q_text) {
        log_trace("QUEST TITLE: Using title_text");
        return q_text + q_ptr->title_text;
    }
    
    /* Fallback to quest name */
    if (q_ptr->name && quest_name_text) {
        log_trace("QUEST TITLE: Using quest name fallback");
        return quest_name_text + q_ptr->name;
    }
    
    log_trace("QUEST TITLE: No valid text found, returning Unknown Quest");
    return "Unknown Quest";
}

/*
 * Get quest challenge description from quest data
 */
static cptr get_quest_challenge(int quest_idx)
{
    log_trace("QUEST CHALLENGE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST CHALLENGE: Invalid bounds check, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST CHALLENGE: q_ptr is NULL, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    if (q_ptr->challenge_text && q_text) {
        log_trace("QUEST CHALLENGE: Using challenge_text");
        return q_text + q_ptr->challenge_text;
    }
    
    log_trace("QUEST CHALLENGE: No valid text found, returning default");
    return "Face the unknown challenge";
}

/*
 * Get oath name from oath ID using oath_info data
 */
static cptr get_oath_name_from_id(byte oath_id)
{
    if (oath_id <= 0 || oath_id >= z_info->oath_max) return "No oath";
    
    oath_type* o_ptr = &oath_info[oath_id];
    if (o_ptr->name) {
        return oath_name_text + o_ptr->name;
    }
    
    /* Fallback to hardcoded names if oath_info not loaded */
    switch(oath_id) {
        case 0: return "No oath";
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";  
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

/*
 * Display wrapped text for quest status - simple word wrapping
 */
static void display_wrapped_text(int col, int *row, cptr text, byte color, int max_width)
{
    char line_buf[256];
    int line_pos = 0;
    int effective_width = max_width - col - 4; /* Leave margin for indentation */
    int text_len = strlen(text);
    int word_start = 0;
    int i = 0;
    int loop_count = 0; /* Safety counter for this function call */
    
    if (effective_width < 20) effective_width = 20; /* Minimum width */
    
    line_buf[0] = '\0';
    
    while (i <= text_len) {
        /* Safety check to prevent infinite loop */
        loop_count++;
        if (loop_count > 1000) {
            log_warn("display_wrapped_text: safety break, possible infinite loop (text_len=%d, i=%d)", text_len, i);
            break;
        }
        
        /* End of string or found a space */
        if (i == text_len || text[i] == ' ') {
            /* Extract the current word */
            int word_len = i - word_start;
            char word[128];
            
            if (word_len > 0 && word_len < (int)sizeof(word)) {
                /* Copy the word manually to avoid buffer issues */
                int copy_len = word_len;
                if (copy_len >= (int)sizeof(word)) copy_len = (int)sizeof(word) - 1;
                
                /* Manual copy to avoid strncpy issues */
                int j;
                for (j = 0; j < copy_len; j++) {
                    word[j] = text[word_start + j];
                }
                word[copy_len] = '\0';
                
                /* Check if adding this word would exceed the line width */
                int new_line_len = line_pos + (line_pos > 0 ? 1 : 0) + copy_len;
                
                if (new_line_len > effective_width && line_pos > 0) {
                    /* Output current line and start new line with this word */
                    Term_putstr(col + 2, (*row)++, -1, color, line_buf);
                    
                    /* Check if the word itself is too long for a line */
                    if (copy_len > effective_width) {
                        /* Break the word across multiple lines */
                        int word_pos = 0;
                        while (word_pos < copy_len) {
                            int chunk_len = effective_width;
                            if (word_pos + chunk_len > copy_len) {
                                chunk_len = copy_len - word_pos;
                            }
                            
                            /* Extract chunk of the word */
                            char chunk[256];
                            int k;
                            for (k = 0; k < chunk_len && word_pos + k < copy_len; k++) {
                                chunk[k] = word[word_pos + k];
                            }
                            chunk[k] = '\0';
                            
                            /* Output this chunk */
                            Term_putstr(col + 2, (*row)++, -1, color, chunk);
                            word_pos += chunk_len;
                        }
                        
                        /* Reset line buffer */
                        line_buf[0] = '\0';
                        line_pos = 0;
                    } else {
                        /* Word fits on a new line */
                        SDL_strlcpy(line_buf, word, sizeof(line_buf));
                        line_pos = copy_len;
                    }
                } else {
                    /* Add word to current line */
                    if (line_pos > 0) {
                        SDL_strlcat(line_buf, " ", sizeof(line_buf));
                        line_pos++;
                    }
                    SDL_strlcat(line_buf, word, sizeof(line_buf));
                    line_pos += copy_len;
                }
            }
            
            /* Skip spaces and move to next word */
            while (i < text_len && text[i] == ' ') {
                i++;
            }
            word_start = i;
        } else {
            i++;
        }
    }
    
    /* Output any remaining text in the buffer */
    if (line_pos > 0) {
        Term_putstr(col + 2, (*row)++, -1, color, line_buf);
    }
}

static int quest_wrapped_rows(int col, cptr text, int max_width)
{
    int indent = col + 2;
    int wrap_width = max_width - 2;
    int rows_used = 1;

    if (!text || !text[0])
        return 1;

    if (wrap_width <= indent)
        wrap_width = indent + 1;

    if (sdl_is_story_font_enabled())
        rows_used = count_wrapped_lines_story(text, wrap_width, indent);
    else
        rows_used = count_wrapped_lines(text, wrap_width, indent);

    if (rows_used < 1)
        rows_used = 1;

    return rows_used;
}

static void quest_status_draw_header(int col)
{
    Term_putstr(col, 1, -1, TERM_YELLOW, "=== Quest Status ===");
}

static void quest_status_reset_page(int col, int *row)
{
    Term_clear();
    quest_status_draw_header(col);
    *row = 3;
}

static void quest_status_wait_for_next_page(int col, int hgt, int *row)
{
    int prompt_row = MAX(0, hgt - 1);

    Term_erase(0, prompt_row, 255);
    Term_putstr(col, prompt_row, -1, TERM_L_WHITE, "Press any key to continue.");
    Term_fresh();
    (void)inkey();

    quest_status_reset_page(col, row);
}

static void quest_status_ensure_rows(int col, int hgt, int *row, int needed_rows)
{
    if (needed_rows < 1)
        needed_rows = 1;

    if ((*row + needed_rows) <= (hgt - 1))
        return;

    quest_status_wait_for_next_page(col, hgt, row);
}

static void quest_status_put_line(int col, int hgt, int *row, byte color, cptr text)
{
    quest_status_ensure_rows(col, hgt, row, 1);
    Term_putstr(col, (*row)++, -1, color, text ? text : "");
}

static void quest_status_put_wrapped(int col, int wid, int hgt, int *row,
    byte color, cptr text)
{
    int rows_needed = quest_wrapped_rows(col, text, wid);

    quest_status_ensure_rows(col, hgt, row, rows_needed);
    display_wrapped_text(col, row, text, color, wid);
}

/*
 * Simple string search function - finds needle in haystack
 * Returns pointer to first occurrence, or NULL if not found
 */
static char* my_strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    
    int needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char* p = haystack; *p; p++) {
        int i;
        for (i = 0; i < needle_len && p[i] && p[i] == needle[i]; i++);
        if (i == needle_len) {
            return (char*)p;
        }
    }
    return NULL;
}

/*
 * Process placeholders in quest text (challenge, etc.) with actual values
 */
static cptr process_quest_placeholders(cptr text, int quest_idx)
{
    static char processed_buf[256];

    if (!text) {
        return "";
    }

    SDL_strlcpy(processed_buf, text, sizeof(processed_buf));
    
    if (quest_idx == QUEST_ID_TULKAS) {
        /* Replace [monster name] with actual monster name */
        char* monster_pos = my_strstr(processed_buf, "[monster name]");
        if (monster_pos && p_ptr->tulkas_target_r_idx > 0 && p_ptr->tulkas_target_r_idx < z_info->r_max) {
            monster_race* r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
            char before[128], after[128];
            int before_len = monster_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, r_name + r_ptr->name, after);
        }
        
        /* Replace [artifact name] with actual artifact name */
        char* artifact_pos = my_strstr(processed_buf, "[artifact name]");
        if (artifact_pos && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
            char before[128], after[128];
            int before_len = artifact_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, artifact_pos + 15, sizeof(after)); /* 15 = strlen("[artifact name]") */
            
            /* Get proper artifact name using object_desc */
            char artifact_name[120];
            if (a_ptr->name[0] != '\0') {
                /* Create a temporary object to get proper description */
                object_type temp_obj;
                object_wipe(&temp_obj);
                
                /* Set up the object as the artifact */
                s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
                if (k_idx > 0) {
                    object_prep(&temp_obj, k_idx);
                    temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                    temp_obj.ident |= IDENT_KNOWN;
                    
                    /* Get the full artifact description */
                    object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                } else {
                    SDL_strlcpy(artifact_name, a_ptr->name, sizeof(artifact_name));
                }
            } else {
                SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
            }
            
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, artifact_name, after);
        }
    }
    
    return processed_buf;
}

/*
 * Get quest reward description for status display using actual quest data
 */
static cptr get_quest_reward_text(int quest_idx)
{
    static char reward_buf[200];
    char temp_buf[100];
    
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return "Unknown reward";
    
    quest_type* q_ptr = &quest_info[quest_idx];
    reward_buf[0] = '\0';
    
    /* Handle special Tulkas artifact reward */
    if (quest_idx == QUEST_ID_TULKAS && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
        artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
        if (a_ptr->name[0] != '\0') {
            /* Create a temporary object to get proper description */
            object_type temp_obj;
            object_wipe(&temp_obj);
            
            /* Set up the object as the artifact */
            s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (k_idx > 0) {
                object_prep(&temp_obj, k_idx);
                temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                temp_obj.ident |= IDENT_KNOWN;
                
                /* Get the full artifact description */
                object_desc(reward_buf, sizeof(reward_buf), &temp_obj, true, 0);
                return reward_buf;
            } else {
                SDL_strlcpy(reward_buf, a_ptr->name, sizeof(reward_buf));
                return reward_buf;
            }
        }
    }
    
    /* Varda reward description */
    if (quest_idx == QUEST_ID_VARDA) {
        SDL_strlcpy(reward_buf, "Choose one radiant artefact and unlock the Oath of Light (+1 light radius)", sizeof(reward_buf));
        return reward_buf;
    }
    
    /* Build reward description from quest data */
    bool has_rewards = false;
    
    /* Check stat bonuses */
    if (q_ptr->stat_bonuses[0] || q_ptr->stat_bonuses[1] || q_ptr->stat_bonuses[2] || q_ptr->stat_bonuses[3]) {
        has_rewards = true;
        SDL_strlcat(reward_buf, "Stats: ", sizeof(reward_buf));
        
        if (q_ptr->stat_bonuses[0]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Str ", q_ptr->stat_bonuses[0]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[1]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Dex ", q_ptr->stat_bonuses[1]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[2]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Con ", q_ptr->stat_bonuses[2]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[3]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Gra ", q_ptr->stat_bonuses[3]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
    }
    
    /* Check skill bonuses */
    if (q_ptr->skill_type && q_ptr->skill_bonus) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Convert skill type to name */
        cptr skill_name = "Unknown";
        switch (q_ptr->skill_type) {
            case 0: skill_name = "Melee"; break;
            case 1: skill_name = "Archery"; break;
            case 2: skill_name = "Evasion"; break;
            case 3: skill_name = "Stealth"; break;
            case 4: skill_name = "Perception"; break;
            case 5: skill_name = "Will"; break;
            case 6: skill_name = "Smithing"; break;
            case 7: skill_name = "Song"; break;
        }
        strnfmt(temp_buf, sizeof(temp_buf), "+%d %s ", q_ptr->skill_bonus, skill_name);
        SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
    }
    
    /* Check special abilities */
    if (q_ptr->ability_type && q_ptr->ability_id < ABILITIES_MAX) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Get ability name from ability database */
        cptr ability_name = "Special ability";
        if (q_ptr->ability_type == S_SPC) { /* Special abilities type */
            /* Use ability_index to find the ability and get its name */
            int idx = ability_index(S_SPC, q_ptr->ability_id);
            if (idx >= 0 && idx < z_info->b_max) {
                ability_type* b_ptr = &b_info[idx];
                if (b_ptr->name) {
                    ability_name = b_name + b_ptr->name;
                }
            }
        }
        
        SDL_strlcat(reward_buf, ability_name, sizeof(reward_buf));
    }
    
    /* Check oath association */
    if (q_ptr->oath_id) {
        if (has_rewards) SDL_strlcat(reward_buf, " | ", sizeof(reward_buf));
        has_rewards = true;
        SDL_strlcat(reward_buf, get_oath_name_from_id(q_ptr->oath_id), sizeof(reward_buf));
    }
    
    if (!has_rewards) {
        SDL_strlcpy(reward_buf, "Unknown reward", sizeof(reward_buf));
    }
    
    return reward_buf;
}

/*
 * Free quest text array returned by extract functions
 */
void free_quest_texts(cptr* texts, int count)
{
    if (!texts) return;

    if (count < 0) count = 0;
    if (count > 50) count = 50; /* hard cap safety */

    for (int i = 0; i < count; i++) {
        if (texts[i]) {
            str_free((char*)texts[i]);
        }
    }

    mem_free_null(texts);
}

/*
 * Show quest status for current metarun - only active and completed quests
 * Now uses quest.txt data instead of hardcoded values
 */
void do_cmd_quest_status(void)
{
    char buf[128];
    int row = 1;
    int col = 2;
    bool any_quests = false;
    int wid, hgt;

    log_trace("QUEST STATUS: do_cmd_quest_status() called");

    /* Safety check: ensure we have a valid player and metarun */
    if (!p_ptr) {
        log_trace("QUEST STATUS: No player data available");
        msg_print("No character data available.");
        return;
    }

    log_trace("QUEST STATUS: Player exists, quest states - Tulkas: %d, Aule: %d, Mandos: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Get terminal size for wrapping */
    Term_get_size(&wid, &hgt);
    quest_status_reset_page(col, &row);

    /* Check Tulkas quest */
    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr tulkas_status;
        byte color;
        
        log_trace("QUEST STATUS: Getting title and challenge for Tulkas quest");
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_TULKAS);
        log_trace("QUEST STATUS: Got title='%s', challenge='%s'", quest_title ? quest_title : "NULL", quest_challenge ? quest_challenge : "NULL");
        
        if (!quest_title) quest_title = "Tulkas Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";
        
        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);
        
        switch (p_ptr->tulkas_quest) {
            case TULKAS_QUEST_GIVER_PRESENT:
                log_trace("QUEST STATUS: Tulkas GIVER_PRESENT case");
                tulkas_status = "Available - Tulkas awaits";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
                {
                    log_trace("QUEST STATUS: About to call process_quest_placeholders");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, processed_challenge);
                }
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (GIVER_PRESENT)");
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case TULKAS_QUEST_ACTIVE:
                log_trace("QUEST STATUS: Tulkas ACTIVE case");
                {
                    /* Use processed challenge text instead of hardcoded status */
                    log_trace("QUEST STATUS: About to call process_quest_placeholders for ACTIVE");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_WHITE, processed_challenge);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                    break;
                }
            case TULKAS_QUEST_COMPLETE:
                log_trace("QUEST STATUS: Tulkas COMPLETE case");
                tulkas_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
                break;
            case TULKAS_QUEST_REWARDED:
                /* For Tulkas quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                tulkas_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (REWARDED)");
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                tulkas_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Aule quest */
    if (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr aule_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_AULE);
        
        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);
        
        switch (p_ptr->aule_quest) {
            case AULE_QUEST_FORGE_PRESENT:
                aule_status = "Available - Enter the forge";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case AULE_QUEST_ACTIVE:
                /* Use challenge text instead of hardcoded status */
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_WHITE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case AULE_QUEST_SUCCESS:
                aule_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case AULE_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                aule_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                aule_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Mandos quest */
    if (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr mandos_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_MANDOS);
        
        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);
        
        switch (p_ptr->mandos_quest) {
            case MANDOS_QUEST_GIVER_PRESENT:
                mandos_status = "Available - Enter the tomb";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_ACTIVE:
                mandos_status = "Active";
                color = TERM_WHITE;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_SUCCESS:
                mandos_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                mandos_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                mandos_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Niena quest */
    if (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr niena_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_NIENA);
        
        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);
        
        switch (p_ptr->niena_quest) {
            case NIENA_QUEST_GIVER_PRESENT:
                niena_status = "Available - Niena offers mercy";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_ACTIVE:
                strnfmt(buf, sizeof(buf), "Active: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                niena_status = buf;
                color = TERM_WHITE;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_SUCCESS:
                niena_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                niena_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_FAILED:
                strnfmt(buf, sizeof(buf), "Failed: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                color = TERM_RED;
                quest_status_put_line(col + 2, hgt, &row, color, buf);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE,
                    "You took a life and lost Niena's mercy.");
                break;
            default:
                niena_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Orome quest */
    if (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr orome_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_OROME);
        
        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);
        
        switch (p_ptr->orome_quest) {
            case OROME_QUEST_GIVER_PRESENT:
                orome_status = "Available - Orome awaits";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case OROME_QUEST_ACTIVE:
                {
                    strnfmt(buf, sizeof(buf), "Active: Hunt the fell kindreds");
                    orome_status = buf;
                    color = TERM_WHITE;
                    quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                    
                    /* Show current kill counts for all monster types */
                    strnfmt(buf, sizeof(buf), "Wolves killed: %d/100", p_ptr->orome_wolves_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_wolves_killed >= 100 ? TERM_L_GREEN : TERM_SLATE, buf);
                    strnfmt(buf, sizeof(buf), "Spiders killed: %d/80", p_ptr->orome_spiders_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_spiders_killed >= 80 ? TERM_L_GREEN : TERM_SLATE, buf);
                    strnfmt(buf, sizeof(buf), "Serpents killed: %d/60", p_ptr->orome_serpents_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_serpents_killed >= 60 ? TERM_L_GREEN : TERM_SLATE, buf);
                    strnfmt(buf, sizeof(buf), "Vampires killed: %d/30", p_ptr->orome_vampires_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_vampires_killed >= 30 ? TERM_L_GREEN : TERM_SLATE, buf);
                    
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                }
                break;
            case OROME_QUEST_SUCCESS:
                orome_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case OROME_QUEST_REWARDED:
                /* For Orome quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                orome_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_OROME));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                orome_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Varda quest */
    if (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr varda_status;
        byte color;
        bool on_bastion_level = varda_quest_bastion_level_active();

        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_VARDA);
        if (!quest_title) quest_title = "Varda Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->varda_quest) {
            case VARDA_QUEST_GIVER_PRESENT:
                varda_status = "Available - Varda waits in sunlight";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_ACTIVE:
                if (on_bastion_level) {
                    varda_status = "Active - Duruin's Bastion is on this level";
                    color = TERM_ORANGE;
                } else {
                    varda_status = "Active - Seek Duruin's Bastion";
                    color = TERM_WHITE;
                }
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                if (on_bastion_level) {
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_ORANGE,
                        "This is the first level you have reached after 500 ft. Slay Duruin before leaving, or Varda's quest is lost.");
                } else {
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE,
                        "Duruin's Bastion will appear on the first level you reach after 500 ft.");
                }
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_SUCCESS:
                varda_status = "Complete - Claim Varda's blessing";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_REWARDED:
                varda_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_FAILED:
                varda_status = "Failed - Duruin's Bastion was left behind";
                color = TERM_RED;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE,
                    "Varda's trial was on the first level you reached after 500 ft; leaving it without slaying Duruin ended the quest.");
                break;
            default:
                varda_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Show previous metarun completions */
    bool has_previous_completions = false;
    int tulkas_completed = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    if (tulkas_completed > 0 && p_ptr->tulkas_quest != TULKAS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr oath_name = get_oath_name_from_id(quest_info[1].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, tulkas_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int aule_completed = metarun_quest_completion_count(METARUN_QUEST_AULE);
    if (aule_completed > 0 && p_ptr->aule_quest != AULE_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr oath_name = get_oath_name_from_id(quest_info[2].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, aule_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int mandos_completed = metarun_quest_completion_count(METARUN_QUEST_MANDOS);
    if (mandos_completed > 0 && p_ptr->mandos_quest != MANDOS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr oath_name = get_oath_name_from_id(quest_info[3].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, mandos_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int niena_completed = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    if (niena_completed > 0 && p_ptr->niena_quest != NIENA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr oath_name = get_oath_name_from_id(quest_info[4].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, niena_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int orome_completed = metarun_quest_completion_count(METARUN_QUEST_OROME);
    if (orome_completed > 0 && p_ptr->orome_quest != OROME_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr oath_name = get_oath_name_from_id(quest_info[5].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, orome_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int varda_completed = metarun_quest_completion_count(METARUN_QUEST_VARDA);
    if (varda_completed > 0 && p_ptr->varda_quest != VARDA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr oath_name = get_oath_name_from_id(quest_info[6].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, varda_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    
    if (has_previous_completions) {
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* If no quests are active or completed */
    if (!any_quests) {
        quest_status_put_line(col, hgt, &row, TERM_SLATE,
            "No active or completed quests this run.");
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
        quest_status_put_line(col, hgt, &row, TERM_L_DARK,
            "Quest vaults may appear as you delve deeper...");
    }

    quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    quest_status_ensure_rows(col, hgt, &row, 1);
    Term_putstr(col, row, -1, TERM_L_WHITE, "Press any key to return.");
    inkey();
    
    screen_pop_supporting_panes_hidden();
    screen_load();
}

static int quest_typewriter_text_start_row(int hgt)
{
    if (hgt <= 3)
        return MAX(0, hgt - 2);

    return 3;
}

static int quest_typewriter_last_text_row(int hgt)
{
    int start_row = quest_typewriter_text_start_row(hgt);
    int last_row = hgt - 2;

    if (last_row < start_row)
        last_row = start_row;

    return last_row;
}

static void quest_typewriter_draw_title(cptr title, byte title_color, int wid)
{
    int title_col = MAX(0, (wid - (int)strlen(title)) / 2);

    Term_putstr(title_col, 1, -1, title_color, title);
}

static bool quest_typewriter_next_page(cptr title, byte title_color, int wid,
    int hgt, int *row, int *col)
{
    cptr prompt = "(press any key to continue)";
    int prompt_row = MAX(0, hgt - 1);
    int prompt_col = MAX(0, (wid - (int)strlen(prompt)) / 2);
    char k;

    Term_erase(0, prompt_row, 255);
    Term_putstr(prompt_col, prompt_row, -1, TERM_L_WHITE, prompt);
    Term_fresh();

    k = inkey();
    if (k == 'Q' || k == 'q')
        return false;

    Term_clear();
    quest_typewriter_draw_title(title, title_color, wid);
    *row = quest_typewriter_text_start_row(hgt);
    *col = 0;
    return true;
}

static bool quest_typewriter_ensure_row(cptr title, byte title_color, int wid,
    int hgt, int *row, int *col)
{
    if (*row <= quest_typewriter_last_text_row(hgt))
        return true;

    return quest_typewriter_next_page(title, title_color, wid, hgt, row, col);
}

/*
 * Quest typewriter menu function - displays quest dialog with typewriter effect
 * Based on print_story_intro() style
 */
void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color)
{
    int wid, h;
    const int indent = 2;
    bool skipped = false;
    
    /* Get terminal size */
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent * 2;
    int row = quest_typewriter_text_start_row(h), col = 0;
    
    /* Save screen and start fresh */
    screen_save();
    Term_clear();
    Term_flush();
    
    /* Display title */
    quest_typewriter_draw_title(title, title_color, wid);
    
    for (int idx = 0; idx < total_texts; idx++) {
        const char *s = texts[idx];
        
        /* Handle empty lines as paragraph breaks */
        if (!s || strlen(s) == 0) {
            /* Empty line - just advance row for paragraph break */
            row++;
            col = 0;
            if ((idx + 1 < total_texts)
                && !quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
                Term_clear();
                screen_load();
                return;
            }
            /* Short pause for empty line */
            if (!skipped) Term_xtra(TERM_XTRA_DELAY, 200);
            continue;
        }
        
        col = 0;
        
        /* Print this string with proper word wrapping and typewriter effect */
        int i = 0;
        while (s[i]) {
            /* Handle explicit newlines */
            if (s[i] == '\n') {
                row++;
                col = 0;
                i++;
                if (!quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
                    Term_clear();
                    screen_load();
                    return;
                }
                continue;
            }
            
            /* Find the end of the current word (or until we hit wrap width) */
            int word_start = i;
            int word_len = 0;
            bool has_space_after = false;
            
            /* Build the current word/phrase until we hit whitespace, newline, or exceed reasonable length */
            while (s[i] && s[i] != '\n' && word_len < wrap_width) {
                if (s[i] == ' ' || s[i] == '\t') {
                    has_space_after = true;
                    break;
                }
                word_len++;
                i++;
            }
            
            log_trace("WRAP DEBUG: word='%.*s', word_len=%d, col=%d, wrap_width=%d", word_len, &s[word_start], word_len, col, wrap_width);
            
            /* Check if this word fits on the current line */
            if (col + word_len > wrap_width && col > 0) {
                /* Word doesn't fit, wrap to next line */
                log_trace("WRAP DEBUG: Wrapping word to next line (col=%d + word_len=%d > wrap_width=%d)", col, word_len, wrap_width);
                row++;
                col = 0;
                if (!quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
                    Term_clear();
                    screen_load();
                    return;
                }
            }
            
            /* Print the word character by character with typewriter effect */
            if (skipped) {
                /* Skip mode: print entire word instantly */
                for (int j = word_start; j < word_start + word_len; j++) {
                    Term_putch(indent + col, row, text_color, s[j]);
                    col++;
                }
            }
            else {
                /* Normal mode: typewriter effect with character-by-character */
                for (int j = word_start; j < word_start + word_len; j++) {
                    /* Check for ESC or Enter key press to skip typewriter effect */
                    char check_key;
                    if (Term_inkey(&check_key, false, false) == 0) {
                        /* Only respond to ESC or Enter - consume and check */
                        Term_inkey(&check_key, false, true);
                        if (check_key == ESCAPE || check_key == '\n' || check_key == '\r') {
                            skipped = true;
                            /* Print rest of current word instantly */
                            for (int k = j; k < word_start + word_len; k++) {
                                Term_putch(indent + col, row, text_color, s[k]);
                                col++;
                            }
                            break; /* Exit to continue with rest of text in skip mode */
                        }
                        /* Other keys are ignored (already consumed) */
                    }
                    
                    /* Print character with typewriter effect */
                    Term_putch(indent + col, row, text_color, s[j]);
                    Term_fresh();
                    col++;
                    
                    /* Delay 25 ms after each character for typewriter effect */
                    Term_xtra(TERM_XTRA_DELAY, 25);
                }
            }
            
            /* Handle the space/whitespace after the word */
            if (has_space_after) {
                if (s[i] == ' ') {
                    /* Only print space if we're not at the end of a line */
                    if (col < wrap_width) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;
                        
                        /* Delay for space too (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the space */
                }
                else if (s[i] == '\t') {
                    /* Handle tab - convert to spaces but respect wrap width */
                    int tab_spaces = 4 - (col % 4);
                    for (int t = 0; t < tab_spaces && col < wrap_width; t++) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;
                        
                        /* Delay for tab spaces (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the tab */
                }
            }
        }
        
        /* Move to next line after text */
        row++;
        col = 0;
        if ((idx + 1 < total_texts)
            && !quest_typewriter_ensure_row(title, title_color, wid, h, &row, &col)) {
            Term_clear();
            screen_load();
            return;
        }
        
        /* 400ms pause after each line of text (unless skipped) */
        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 400);
    }
    
    /* Refresh screen to show all text if skipped */
    if (skipped) Term_fresh();
    
    /* Final prompt */
    Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key to continue)");
    inkey();
    
    /* Flush any queued keypresses that accumulated during the typewriter effect */
    Term_flush();
    
    Term_clear();
    screen_load();
}

/*
 * Remove quest giver monster by R_IDX without messaging
 */
static void remove_quest_giver_silent(int quest_giver_r_idx)
{
    int i;

    log_trace("Attempting to remove quest giver silently with R_IDX: %d", quest_giver_r_idx);

    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];

        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;

        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            log_trace("Found quest giver at (%d, %d), removing silently", m_ptr->fy, m_ptr->fx);
            delete_monster_idx(i);
            return;
        }
    }

    log_trace("Quest giver with R_IDX %d not found on current level (silent remove)", quest_giver_r_idx);
}

/*
 * Remove quest giver monster by R_IDX
 */
void remove_quest_giver(int quest_giver_r_idx)
{
    int i;
    
    log_trace("Attempting to remove quest giver with R_IDX: %d", quest_giver_r_idx);
    
    /* Find and remove the quest giver */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        
        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;
        
        /* Check if this is our quest giver */
        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            log_trace("Found quest giver at (%d, %d), removing", m_ptr->fy, m_ptr->fx);
            
            /* Add a message about the quest giver departing */
            msg_print("The quest giver nods approvingly and fades away, their task complete.");
            
            /* Remove the monster */
            delete_monster_idx(i);
            
            log_trace("Quest giver successfully removed");
            return;
        }
    }
    
    log_trace("Quest giver with R_IDX %d not found on current level", quest_giver_r_idx);
}

/*
 * Check if a quest giver is present on the current level
 */
bool is_quest_giver_present(int quest_giver_r_idx)
{
    int i;
    
    /* Find the quest giver */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        
        /* Skip empty slots */
        if (!m_ptr->r_idx) continue;
        
        /* Check if this is our quest giver */
        if (m_ptr->r_idx == quest_giver_r_idx)
        {
            return true;
        }
    }
    
    return false;
}

/*
 * Spawn a quest giver near the player (for debug completion)
 */
bool spawn_quest_giver_near_player(int quest_giver_r_idx)
{
    int y, x;
    
    log_trace("Attempting to spawn quest giver R_IDX %d near player", quest_giver_r_idx);
    
    /* Try to find a suitable spot near the player */
    for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
    {
        for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
        {
            if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
            {
                if (place_monster_one(y, x, quest_giver_r_idx, true, true, NULL))
                {
                    msg_print("A quest giver materializes nearby!");
                    log_trace("Successfully spawned quest giver at (%d, %d)", y, x);
                    return true;
                }
            }
        }
    }
    
    log_trace("Failed to spawn quest giver near player");
    return false;
}

static void tulkas_quest_decline(cptr message)
{
    if (message) {
        msg_print(message);
    }

    p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    p_ptr->tulkas_target_r_idx = 0;
    p_ptr->tulkas_prize_a_idx = 0;
    p_ptr->tulkas_quest_complete = 0;

    remove_quest_giver_silent(R_IDX_TULKAS);
}

/*
 * Handle Tulkas interaction
 */
void tulkas_quest_interaction(void)
{
    int target_r_idx, prize_a_idx;
    monster_race* r_ptr;
    artefact_type* a_ptr;
    
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_GIVER_PRESENT && 
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE)
    {
        log_trace("tulkas_quest_interaction called with invalid quest state: %d", p_ptr->tulkas_quest);
        return;
    }
    
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Tulkas quest interaction - assigning target and prize");
        
        /* Assign quest target and prize */
        target_r_idx = select_tulkas_quest_target();
        if (target_r_idx == 0 || target_r_idx >= z_info->r_max)
        {
            log_trace("Invalid target_r_idx: %d", target_r_idx);
            tulkas_quest_decline("Tulkas nods thoughtfully and fades away, finding no worthy challenge for you at this time.");
            return;
        }
        
        r_ptr = &r_info[target_r_idx];
        if (target_r_idx <= 0 || target_r_idx >= z_info->r_max || r_ptr->name == 0)
        {
            log_trace("Invalid monster race data for r_idx: %d", target_r_idx);
            tulkas_quest_decline("Tulkas nods thoughtfully and fades away, finding no worthy challenge for you at this time.");
            return;
        }
        
        /* Validate that the target unique is still alive (double-check after selection) */
        if (r_ptr->max_num == 0)
        {
            log_trace("Target unique %d (%s) has already been killed (max_num=0)", target_r_idx, r_name + r_ptr->name);
            tulkas_quest_decline("Tulkas frowns. 'The foe I had in mind has already fallen. Impressive, but I have no new challenge for you now.'");
            return;
        }
        
        /* Select prize artifact that is 5 levels higher than the target monster */
        prize_a_idx = select_tulkas_quest_prize(r_ptr->level);
        if (prize_a_idx == 0 || prize_a_idx >= z_info->art_max)
        {
            log_trace("Invalid prize_a_idx: %d", prize_a_idx);
            tulkas_quest_decline("Tulkas frowns and fades away, having no suitable reward to offer.");
            return;
        }
        
        a_ptr = &a_info[prize_a_idx];
        if (prize_a_idx <= 0 || prize_a_idx >= z_info->art_max)
        {
            log_trace("Invalid artifact data for a_idx: %d", prize_a_idx);
            tulkas_quest_decline("Tulkas frowns and fades away, having no suitable reward to offer.");
            return;
        }
        
        /* Store quest data */
        p_ptr->tulkas_target_r_idx = target_r_idx;
        p_ptr->tulkas_prize_a_idx = prize_a_idx;
        p_ptr->tulkas_quest = TULKAS_QUEST_ACTIVE;
        
        /* Remove the quest giver now that quest is accepted without
         * showing the generic reward/departure message before the quest text.
         */
        remove_quest_giver_silent(R_IDX_TULKAS);
        
        /* Reserve the artifact */
        valar_reserved_artifacts[prize_a_idx] = true;
        
        log_trace("Quest assigned: target=%d (%s), prize=%d (%s)", 
                 target_r_idx, r_name + r_ptr->name, prize_a_idx, a_ptr->name);
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(1, &text_count); /* Tulkas is quest index 1 */
        init_texts = prepend_repeat_context(QUEST_ID_TULKAS, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            /* Substitute [monster name] and [artifact name] in the texts */
            cptr* processed_texts = mem_alloc_array(text_count, cptr);
            for (int i = 0; i < text_count; i++) {
                char temp_text[1024];
                SDL_strlcpy(temp_text, init_texts[i], sizeof(temp_text));
                
                /* Replace [monster name] with actual monster name */
                char* monster_pos = my_strstr(temp_text, "[monster name]");
                if (monster_pos) {
                    char before[512], after[512];
                    int before_len = monster_pos - temp_text;
                    SDL_strlcpy(before, temp_text, before_len + 1);
                    before[before_len] = '\0';
                    SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
                    strnfmt(temp_text, sizeof(temp_text), "%s%s%s", before, r_name + r_ptr->name, after);
                }
                
                /* Replace [artifact name] with actual artifact name */
                char* artifact_pos = my_strstr(temp_text, "[artifact name]");
                if (artifact_pos) {
                    char before[512], after[512];
                    int before_len = artifact_pos - temp_text;
                    SDL_strlcpy(before, temp_text, before_len + 1);
                    before[before_len] = '\0';
                    SDL_strlcpy(after, artifact_pos + 15, sizeof(after)); /* 15 = strlen("[artifact name]") */
                    
                    /* Get proper artifact name using object_desc */
                    char artifact_name[120];
                    if (a_ptr->name[0] != '\0') {
                        /* Create a temporary object to get proper description */
                        object_type temp_obj;
                        object_wipe(&temp_obj);
                        
                        /* Set up the object as the artifact */
                        s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
                        object_prep(&temp_obj, k_idx);
                        temp_obj.name1 = prize_a_idx;
                        temp_obj.ident |= IDENT_KNOWN;
                        
                        /* Get the full artifact description */
                        object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                    } else {
                        SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
                    }
                    
                    strnfmt(temp_text, sizeof(temp_text), "%s%s%s", before, artifact_name, after);
                }
                
                processed_texts[i] = str_dup(temp_text);
            }
            
            /* Display typewriter quest dialog */
            quest_typewriter_menu("Quest of Tulkas the Strong", processed_texts, text_count, TERM_YELLOW, TERM_WHITE);
            
            /* Clean up */
            for (int i = 0; i < text_count; i++) {
                if (processed_texts[i]) str_free((char*)processed_texts[i]);
            }
            mem_free_null(processed_texts);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            msg_print("Tulkas the Strong speaks of a great quest, but the words are lost in thunder.");
        }
    }
    else if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE)
    {
        log_trace("Completing Tulkas quest - giving reward artifact %d", p_ptr->tulkas_prize_a_idx);
        
        /* Safety check for valid artifact index */
        if (p_ptr->tulkas_prize_a_idx <= 0 || p_ptr->tulkas_prize_a_idx >= z_info->art_max)
        {
            log_trace("Invalid prize artifact index: %d", p_ptr->tulkas_prize_a_idx);
            msg_print("Tulkas appears but looks puzzled about your reward.");
            return;
        }
        
        log_trace("About to create artifact %d at position (%d,%d)", p_ptr->tulkas_prize_a_idx, p_ptr->py, p_ptr->px);
        
        /* Give the artifact reward */
        create_chosen_artefact(p_ptr->tulkas_prize_a_idx, p_ptr->py, p_ptr->px, true);
        
        log_trace("Successfully created artifact %d", p_ptr->tulkas_prize_a_idx);
        
        /* Clear quest state */
        if (valar_reserved_artifacts && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            valar_reserved_artifacts[p_ptr->tulkas_prize_a_idx] = false;
            log_trace("Cleared reservation for artifact %d", p_ptr->tulkas_prize_a_idx);
        } else {
            log_trace("Skipping reservation clear: valar_reserved_artifacts=%p, artifact_idx=%d, art_max=%d", 
                     valar_reserved_artifacts, p_ptr->tulkas_prize_a_idx, z_info->art_max);
        }
        p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
        p_ptr->tulkas_target_r_idx = 0;
        p_ptr->tulkas_prize_a_idx = 0;
        p_ptr->tulkas_quest_complete = 0;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(1); /* Tulkas is quest index 1 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(1); /* Tulkas is quest index 1 */
        
        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(1, &completion_count); /* Tulkas is quest index 1 */
        completion_texts = prepend_repeat_context(QUEST_ID_TULKAS, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            /* Display typewriter completion dialog */
            quest_typewriter_menu("Quest Complete: Tulkas Rewards Your Valor", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Tulkas appears with a great laugh of triumph!",
                "'Well fought, warrior! You have proven your valor in battle.'"
            };
            quest_typewriter_menu("Quest Complete: Tulkas Rewards Your Valor", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }

        metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_TULKAS);
        
        log_trace("Tulkas quest completed and rewarded");
    }
}

/*
 * Check if player is adjacent to Tulkas and handle interaction
 */
void check_tulkas_quest_interaction(void)
{
    int i, y, x;
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_GIVER_PRESENT && 
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE)
    {
        return;
    }
    
    log_trace("check_tulkas_quest_interaction: checking adjacency, quest state: %d", p_ptr->tulkas_quest);
    
    /* Check all adjacent squares for Tulkas */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];
        
        /* Check bounds */
        if (!in_bounds(y, x)) continue;
        
        /* Check for monster */
        if (cave_m_idx[y][x] > 0)
        {
            int m_idx = cave_m_idx[y][x];
            
            if (m_idx >= mon_max) continue;
            
            monster_type* m_ptr = &mon_list[m_idx];
            
            /* Check if it's Tulkas */
            if (m_ptr->r_idx == R_IDX_TULKAS)
            {
                log_trace("Found Tulkas adjacent, calling interaction");
                tulkas_quest_interaction();
                return;
            }
        }
    }
}

/*
 * Handle monster death for Tulkas quest
 */
void check_tulkas_quest_completion(int r_idx)
{
    if (p_ptr->tulkas_quest == TULKAS_QUEST_ACTIVE && 
        r_idx == p_ptr->tulkas_target_r_idx)
    {
        p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
        p_ptr->tulkas_quest_complete = 1;
        
        msg_print("The deed is done! Seek out Tulkas Unclad to claim your reward.");
        
        /* Spawn Tulkas in the same room */
        int y, x;
        
        /* Try to find a suitable spot near the player */
        for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                    cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
                {
                    place_monster_one(y, x, R_IDX_TULKAS, true, true, NULL);
                    msg_print("Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!");
                    return;
                }
            }
        }
    }
}

/*
 * Validate Tulkas quest target on game load
 * Auto-completes the quest if the assigned target is already dead
 * This fixes stuck saves where players have dead targets assigned
 */
void validate_tulkas_quest_on_load(void)
{
    monster_race* r_ptr;
    
    /* Only validate if quest is in ACTIVE state */
    if (p_ptr->tulkas_quest != TULKAS_QUEST_ACTIVE)
    {
        return;
    }
    
    /* Check if we have a valid target assigned */
    if (p_ptr->tulkas_target_r_idx <= 0 || p_ptr->tulkas_target_r_idx >= z_info->r_max)
    {
        log_trace("validate_tulkas_quest_on_load: Invalid target r_idx=%d, skipping", p_ptr->tulkas_target_r_idx);
        return;
    }
    
    r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
    
    /* Check if the target unique is already dead (max_num == 0) */
    if (r_ptr->max_num == 0)
    {
        log_trace("validate_tulkas_quest_on_load: Target unique %d (%s) is dead (max_num=0), auto-completing quest",
                 p_ptr->tulkas_target_r_idx, r_name + r_ptr->name);
        
        /* Validate we have a valid artifact prize */
        if (p_ptr->tulkas_prize_a_idx <= 0 || p_ptr->tulkas_prize_a_idx >= z_info->art_max)
        {
            log_trace("validate_tulkas_quest_on_load: Invalid prize artifact index: %d, clearing quest", p_ptr->tulkas_prize_a_idx);
            
            /* Clear quest state without reward */
            p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
            p_ptr->tulkas_target_r_idx = 0;
            p_ptr->tulkas_prize_a_idx = 0;
            p_ptr->tulkas_quest_complete = 0;
            return;
        }
        
        /* Set quest to COMPLETE state - this will trigger normal completion flow */
        /* Tulkas will spawn near player on next level generation/turn */
        p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
        p_ptr->tulkas_quest_complete = 1;
        
        log_trace("validate_tulkas_quest_on_load: Quest set to COMPLETE state, Tulkas will spawn on next turn");
    }
    else
    {
        log_trace("validate_tulkas_quest_on_load: Target unique %d (%s) is still alive (max_num=%d)",
                 p_ptr->tulkas_target_r_idx, r_name + r_ptr->name, r_ptr->max_num);
    }
}

/* -----------------------------
 * Varda quest helpers and flow
 * ----------------------------- */
static bool artefact_is_radiant_candidate(artefact_type* a_ptr)
{
    if (!a_ptr) return false;
    if (!(a_ptr->flags2 & (TR2_LIGHT | TR2_RADIANCE))) return false;
    if (a_ptr->flags2 & TR2_DARKNESS) return false;
    if (a_ptr->flags4 & TR4_UNLIGHT) return false;
    if (a_ptr->flags3 & TR3_LIGHT_CURSE) return false;
    return true;
}

static int build_varda_reward_options(int* choices, int max_choices)
{
    if (!choices || max_choices <= 0 || !z_info) return 0;

    /* Ensure reservation table exists so we can filter reserved artefacts */
    if (!valar_reserved_artifacts && z_info && z_info->art_max > 0) {
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
        for (int j = 0; j < z_info->art_max; j++) {
            valar_reserved_artifacts[j] = false;
        }
        log_trace("Varda reward: initialized valar_reserved_artifacts for %d artefacts", z_info->art_max);
    }

    int candidates[256];
    int candidate_count = 0;

    /* Early-light reward: bias toward items not far above current depth */
    int depth_cap = 25;
    if (p_ptr) {
        depth_cap = MIN(p_ptr->depth + 6, depth_cap);
    }
    depth_cap = MAX(depth_cap, 15); /* keep at least mid-depth options */

    for (int i = 1; i < z_info->art_max && candidate_count < (int)N_ELEMENTS(candidates); i++) {
        artefact_type* a_ptr = &a_info[i];

        if (!a_ptr || a_ptr->tval == 0) continue;
        if (a_ptr->name[0] == '\0') continue;
        if (!artefact_is_radiant_candidate(a_ptr)) continue;
        if (a_ptr->cur_num != 0) continue; /* already created */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;
        if (a_ptr->level > depth_cap) continue;

        candidates[candidate_count++] = i;
    }

    /* Relax depth cap if we still need more options */
    if (candidate_count < max_choices) {
        for (int i = 1; i < z_info->art_max && candidate_count < (int)N_ELEMENTS(candidates); i++) {
            artefact_type* a_ptr = &a_info[i];

            if (!a_ptr || a_ptr->tval == 0) continue;
            if (a_ptr->name[0] == '\0') continue;
            if (!artefact_is_radiant_candidate(a_ptr)) continue;
            if (a_ptr->cur_num != 0) continue;
            if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

            candidates[candidate_count++] = i;
        }
    }

    if (candidate_count == 0) return 0;

    /* Shuffle candidates */
    for (int i = candidate_count - 1; i > 0; i--) {
        int swap_idx = rand_int(i + 1);
        int tmp = candidates[i];
        candidates[i] = candidates[swap_idx];
        candidates[swap_idx] = tmp;
    }

    int final_count = MIN(max_choices, candidate_count);
    for (int i = 0; i < final_count; i++) {
        choices[i] = candidates[i];
    }
    return final_count;
}

static void describe_varda_choice(int a_idx, char* buf, size_t buf_len)
{
    artefact_type* a_ptr = &a_info[a_idx];
    object_type temp_obj;
    object_wipe(&temp_obj);

    s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (k_idx > 0) {
        object_prep(&temp_obj, k_idx);
        temp_obj.name1 = a_idx;
        temp_obj.ident |= IDENT_KNOWN;
        object_desc(buf, buf_len, &temp_obj, true, 0);
    } else if (a_ptr->name[0] != '\0') {
        SDL_strlcpy(buf, a_ptr->name, buf_len);
    } else {
        SDL_strlcpy(buf, "a radiant artefact", buf_len);
    }
}

/*
 * Display Varda's reward selection in a scrollable menu integrated with quest completion text.
 * Returns the selected artefact index, or 0 if cancelled.
 */
static int prompt_varda_reward_choice_menu(const int* choices, int choice_count, cptr* completion_texts, int text_count)
{
    int wid, hgt;
    Term_get_size(&wid, &hgt);
    bool compact = (wid < 60) || (hgt < 20);
    bool steamdeck = steamdeck_controls_active();
    
    int selection = 0;
    bool done = false;
    int selected_artifact = 0;

    if (compact && completion_texts && text_count > 0) {
        quest_typewriter_menu("Starlight Triumph", completion_texts, text_count,
            TERM_L_GREEN, TERM_WHITE);
    }
    
    /* Save screen once */
    screen_save();

    while (!done) {
        /* Clear screen */
        Term_clear();
        
        /* Display title */
        int row = 1;
        cptr title = "Starlight Triumph";
        Term_putstr((wid - strlen(title)) / 2, row, -1, TERM_L_GREEN, title);
        row += 2;
        
        /* Display completion text */
        if (!compact) {
            int reserved_rows = choice_count + 5;

            for (int i = 0; i < text_count && row < hgt - reserved_rows; i++) {
                if (completion_texts[i] && completion_texts[i][0] != '\0') {
                    display_wrapped_text(2, &row, completion_texts[i], TERM_WHITE, wid);
                } else {
                    row++; /* Empty line for paragraph break */
                }
            }
        }
        
        row++;
        Term_putstr(2, row++, -1, TERM_L_BLUE, "Choose your radiant gift:");
        row++;
        
        /* Display reward choices with highlighting */
        int choice_start_row = row;
        char desc[120];
        for (int i = 0; i < choice_count && row < hgt - 3; i++) {
            describe_varda_choice(choices[i], desc, sizeof(desc));
            
            byte attr = (i == selection) ? TERM_YELLOW : TERM_L_WHITE;
            char marker = (i == selection) ? '>' : ' ';
            
            char line_buf[140];
            if (steamdeck)
                strnfmt(line_buf, sizeof(line_buf), "%c   %s", marker, desc);
            else
                strnfmt(line_buf, sizeof(line_buf), "%c %c) %s", marker,
                    'a' + i, desc);
            Term_putstr(2, row++, -1, attr, line_buf);
        }
        
        /* Display controls */
        row = hgt - 2;
        Term_putstr(2, row, -1, TERM_L_DARK,
            steamdeck
                ? (compact ? "D-pad move  X inspect  A choose"
                           : "D-pad navigate   X Inspect   A accept")
                : (compact ? "8/2 move  x inspect  Enter choose"
                           : "Arrows navigate   'x' Inspect   Space/Enter accept   Letter select"));
        
        /* Position cursor at selection */
        Term_gotoxy(2, MIN(choice_start_row + selection, hgt - 3));
        Term_fresh();
        
        /* Get input */
        char key = inkey();
        
        /* Handle input */
        if (key == '\r' || key == '\n' || key == ' ' || key == '6'
            || (steamdeck && key == steamdeck_confirm_key())) {
            /* Accept current selection */
            selected_artifact = choices[selection];
            done = true;
        } else if ((!steamdeck && (key == 'x' || key == 'X'))
                   || key == '?'
                   || (steamdeck && key == steamdeck_alt_action_key())) {
            /* Inspect selection */
            Term_clear();
            desc_art_fake(choices[selection]);
        } else if (key == '8' || key == 'k' || key == '-') {
            /* Move up */
            selection = (selection + choice_count - 1) % choice_count;
        } else if (key == '2' || key == 'j' || key == '+') {
            /* Move down */
            selection = (selection + 1) % choice_count;
        } else if (!steamdeck && key >= 'a' && key < 'a' + choice_count) {
            /* Letter selection */
            selected_artifact = choices[key - 'a'];
            done = true;
        } else if (!steamdeck && key >= 'A' && key < 'A' + choice_count) {
            /* Capital letter selection */
            selected_artifact = choices[key - 'A'];
            done = true;
        }
    }
    
    screen_load();
    return selected_artifact;
}

static bool grant_varda_reward(cptr* completion_texts, int completion_count)
{
    int choices[3] = {0};
    int available = build_varda_reward_options(choices, (int)N_ELEMENTS(choices));
    if (available <= 0) {
        msg_print("Varda has no radiant artefacts left to offer.");
        log_trace("Varda reward: no available radiant artefacts");
        return false;
    }

    /* Use the integrated scrollable menu with completion text */
    int selected = prompt_varda_reward_choice_menu(choices, available, completion_texts, completion_count);
    if (selected <= 0) {
        msg_print("The starlight gifts wait until you are ready to choose.");
        return false;
    }

    create_chosen_artefact(selected, p_ptr->py, p_ptr->px, true);
    msg_print("Starlight gathers at your feet, coalescing into a shining relic.");
    p_ptr->varda_quest = VARDA_QUEST_REWARDED;
    p_ptr->quest_reserved[0] = 1;
    p_ptr->varda_vault_ready = 0;
    p_ptr->varda_vault_placed = 1;

    metarun_mark_quest_completed(METARUN_QUEST_VARDA);
    metarun_unlock_oath(OATH_LIGHT);
    do_cmd_note("Varda blessed me with a radiant artefact and the Oath of Light.", p_ptr->depth);

    return true;
}

static void varda_make_light_pool(int y, int x)
{
    for (int ny = y - 1; ny <= y + 1; ny++) {
        for (int nx = x - 1; nx <= x + 1; nx++) {
            if (!in_bounds(ny, nx)) continue;
            if (cave_info[ny][nx] & CAVE_ICKY) continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR && cave_feat[ny][nx] != FEAT_RAGE_FLOOR
                && cave_feat[ny][nx] != FEAT_SUNLIGHT) continue;
            cave_set_feat(ny, nx, FEAT_SUNLIGHT);
        }
    }
}

static void try_place_varda_near_player(void)
{
    /* Avoid double-spawning */
    for (int i = 1; i < mon_max; i++) {
        monster_type* m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_VARDA) {
            log_trace("Varda reward: quest giver already exists on level");
            return;
        }
    }

    for (int y = p_ptr->py - 2; y <= p_ptr->py + 2; y++) {
        for (int x = p_ptr->px - 2; x <= p_ptr->px + 2; x++) {
            if (!in_bounds(y, x)) continue;
            if (y == p_ptr->py && x == p_ptr->px) continue;
            if (distance(p_ptr->py, p_ptr->px, y, x) < 2) continue;
            if (!cave_floor_bold(y, x)) continue;
            if (cave_m_idx[y][x] != 0) continue;

            if (place_monster_one(y, x, R_IDX_VARDA, true, true, NULL)) {
                varda_make_light_pool(y, x);
                p_ptr->varda_level = p_ptr->depth;
                log_trace("Varda reward: placed quest giver at (%d,%d)", y, x);
                return;
            }
        }
    }

    /* Mark this depth as attempted even if placement failed, to avoid infinite spawn loops */
    p_ptr->varda_level = p_ptr->depth;
    log_trace("Varda reward: failed to place quest giver near player (no valid space), will retry on new depth");
}

static bool varda_quest_duruin_present(void)
{
    for (int i = 1; i < mon_max; i++) {
        monster_type* m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_DURUIN) return true;
    }

    return false;
}

bool varda_quest_bastion_level_active(void)
{
    if (!p_ptr) return false;
    if (p_ptr->varda_quest != VARDA_QUEST_ACTIVE) return false;
    if (!p_ptr->varda_vault_placed) return false;
    if (p_ptr->varda_level != p_ptr->depth) return false;

    return varda_quest_duruin_present();
}

void varda_quest_notice_bastion_level_entry(void)
{
    if (!varda_quest_bastion_level_active()) return;

    msg_print("Varda's quest presses upon you.");
    msg_print("This is the first level you have reached after 500 ft.");
    msg_print("Duruin, least of the Balrogs, waits here.");
    msg_print("His Bastion is on this level.");
    msg_print("Leave without slaying him and the quest is lost.");
}

bool varda_quest_confirm_leave_bastion(void)
{
    if (!varda_quest_bastion_level_active()) return true;

    msg_print("Duruin's Bastion lies on this level.");
    msg_print("It is the first level you reached after 500 ft.");
    msg_print("Leaving now will fail Varda's quest.");

    return get_check("Leave Duruin's Bastion and fail Varda's quest? ");
}

void varda_quest_fail_if_bastion_missed(void)
{
    if (!varda_quest_bastion_level_active()) return;

    p_ptr->varda_quest = VARDA_QUEST_FAILED;
    p_ptr->varda_vault_ready = 0;
    p_ptr->quest_reserved[0] = 1;

    msg_print("You have left Duruin's Bastion behind.");
    msg_print("Varda's quest is lost.");
    do_cmd_note("Failed Varda's quest by leaving Duruin's Bastion behind.", p_ptr->depth);
    log_trace("Varda quest: FAILED - player left Duruin's Bastion at depth %d", p_ptr->depth);
}

void check_varda_quest_completion(int r_idx)
{
    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE && r_idx == R_IDX_DURUIN) {
        p_ptr->varda_quest = VARDA_QUEST_SUCCESS;
        p_ptr->varda_vault_ready = 0;
        p_ptr->varda_level = p_ptr->depth;
        msg_print("Duruin falls. The Bastion's shadows unravel under starlight!");
        try_place_varda_near_player();
    }
}

void varda_quest_interaction(void)
{
    static s32b last_interaction_turn = -1;
    if (last_interaction_turn == turn) return;
    last_interaction_turn = turn;

    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT) {
        log_trace("Varda quest: accepting quest");
        p_ptr->varda_quest = VARDA_QUEST_ACTIVE;
        p_ptr->quest_reserved[0] = 1;
        p_ptr->varda_level = p_ptr->depth;

        /* Remove quest giver for roulette quests without a generic
         * completion-style departure message during quest acceptance.
         */
        remove_quest_giver_silent(R_IDX_VARDA);

        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_VARDA, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_VARDA, init_texts, &text_count, false);
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Varda, Lady of the Stars", init_texts, text_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Varda's voice is clear as starlight:",
                "\"Seek Duruin's bastion of shadow and break it open to the Sun.\""
            };
            quest_typewriter_menu("Varda, Lady of the Stars", fallback, 2, TERM_WHITE, TERM_L_BLUE);
        }

        do_cmd_note("Varda sent me to destroy Duruin and cleanse his bastion.", p_ptr->depth);
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
        msg_print("Varda's whisper:");
        msg_print("\"Find Duruin's Bastion on the first level");
        msg_print("you reach after 500 ft.\"");
        msg_print("\"Leave it behind and the quest is lost.\"");
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_SUCCESS) {
        log_trace("Varda quest: delivering reward");
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA, completion_texts, &completion_count, true);
        
        /* Fallback texts if quest data not available */
        cptr fallback[] = {
            "Varda inclines her head in silent approval:",
            "\"The stolen light is free. Choose your blessing.\""
        };
        cptr* texts_to_use = (completion_texts && completion_count > 0) ? completion_texts : fallback;
        int text_count = (completion_texts && completion_count > 0) ? completion_count : 2;
        
        /* Display quest completion and reward selection in one integrated menu */
        bool rewarded = grant_varda_reward(texts_to_use, text_count);
        
        if (completion_texts) {
            free_quest_texts(completion_texts, completion_count);
        }
        
        if (rewarded) {
            remove_quest_giver(R_IDX_VARDA);
        }
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_REWARDED) {
        msg_print("Varda's blessing still follows the path you walk.");
    }
}

void check_varda_quest_interaction(void)
{
    int i, y, x;

    if (p_ptr->varda_quest < VARDA_QUEST_GIVER_PRESENT || p_ptr->varda_quest > VARDA_QUEST_SUCCESS) return;

    for (i = 1; i < 9; i++) {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];

        if (!in_bounds(y, x)) continue;
        if (cave_m_idx[y][x] <= 0) continue;

        int m_idx = cave_m_idx[y][x];
        if (m_idx >= mon_max) continue;

        monster_type* m_ptr = &mon_list[m_idx];
        if (m_ptr->r_idx == R_IDX_VARDA) {
            varda_quest_interaction();
            return;
        }
    }

    /* If Varda should be present for a reward but isn't adjacent, try to place her */
    /* Only attempt placement once per depth to avoid infinite spawn loops */
    if (p_ptr->varda_quest == VARDA_QUEST_SUCCESS && p_ptr->varda_level != p_ptr->depth) {
        log_trace("Varda quest: success state without nearby quest giver - attempting to place Varda");
        try_place_varda_near_player();
    }
}

/*
 * Count hostile monsters in Mandos vault area using proper vault boundaries
 */
/*
 * Check if Brodda (formerly Aldor) has been killed for Mandos quest
 */
static bool is_brodda_dead(void)
{
    int i;
    
    /* Check if Brodda is still alive on the level */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_ALDOR) /* Brodda uses the same monster index as Aldor */
        {
            log_trace("Brodda is still alive at (%d, %d)", m_ptr->fy, m_ptr->fx);
            return false;
        }
    }
    
    log_trace("Brodda has been slain");
    return true;
}

/*
 * Check if player is adjacent to Aule
 */
void check_aule_quest_interaction(void)
{
    int i, y, x;
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->aule_quest != AULE_QUEST_NOT_STARTED &&
        p_ptr->aule_quest != AULE_QUEST_FORGE_PRESENT && 
        p_ptr->aule_quest != AULE_QUEST_SUCCESS)
    {
        return;
    }
    
    /* Skip interaction if quest already rewarded */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED)
    {
        return;
    }
    
    log_trace("check_aule_quest_interaction: checking adjacency, quest state: %d", p_ptr->aule_quest);
    
    /* Check all adjacent squares for Aule */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];
        
        /* Check bounds */
        if (!in_bounds(y, x)) continue;
        
        /* Check for monster */
        if (cave_m_idx[y][x] > 0)
        {
            int m_idx = cave_m_idx[y][x];
            
            if (m_idx >= mon_max) continue;
            
            monster_type* m_ptr = &mon_list[m_idx];
            
            /* Check if it's Aule */
            if (m_ptr->r_idx == R_IDX_AULE)
            {
                log_trace("Found Aule adjacent, calling interaction");
                aule_quest_interaction();
                return;
            }
        }
    }
}

/*
 * Handle Aule interaction
 */
void aule_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Skip interaction if quest already rewarded */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED)
    {
        log_trace("Quest already rewarded, no interaction");
        return;
    }
    
    /* Handle first encounter - initialize quest */
    if (p_ptr->aule_quest == AULE_QUEST_NOT_STARTED)
    {
        log_trace("First encounter with Aule - setting to FORGE_PRESENT");
        p_ptr->aule_quest = AULE_QUEST_FORGE_PRESENT;
        p_ptr->aule_level = p_ptr->depth;
        /* Don't start the actual quest conversation yet, let them talk again */
        msg_print("You encounter Aule the Smith, Maker of Mountains.");
        msg_print("'Speak with me again to learn of the challenges that await.'");
        return;
    }
    
    /* Handle quest explanation */
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        log_trace("Aule quest explanation - setting to ACTIVE");
        p_ptr->aule_quest = AULE_QUEST_ACTIVE;
        
        /* Only remove quest giver for roulette-based quests (Y:1) */
        quest_type* q_ptr = &quest_info[2]; /* Aule is quest index 2 */
        if (q_ptr->quest_type == 1) { /* Y:1 = roulette-based */
            remove_quest_giver_silent(R_IDX_AULE);
            log_trace("Aule quest giver removed silently (roulette-based quest)");
        } else {
            log_trace("Aule quest giver NOT removed (vault-based quest)");
        }
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(2, &text_count); /* Aule is quest index 2 */
        init_texts = prepend_repeat_context(QUEST_ID_AULE, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Aule the Smith", init_texts, text_count, TERM_YELLOW, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Aule speaks in a voice like hammer on anvil:",
                "'Find my forge and create something worthy of my attention.'"
            };
            quest_typewriter_menu("Aule the Smith", fallback_texts, 2, TERM_YELLOW, TERM_WHITE);
        }
        
        /* Mark in the notes */
        do_cmd_note("Aule has challenged me to use his forge to create an item.", p_ptr->depth);
        return;
    }
    
    /* Handle quest completion */
    if (p_ptr->aule_quest == AULE_QUEST_SUCCESS)
    {
        log_trace("Aule quest completed - giving special ability reward");
        
        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(2, &completion_count); /* Aule is quest index 2 */
        completion_texts = prepend_repeat_context(QUEST_ID_AULE, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Quest Complete!", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Aule nods with satisfaction:",
                "'Well done! Your skill at the forge shows promise.'"
            };
            quest_typewriter_menu("Quest Complete!", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }
        
        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_AULE);
        
        /* Change quest state to prevent repeated interactions */
        p_ptr->aule_quest = AULE_QUEST_REWARDED;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(2); /* Aule is quest index 2 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(2); /* Aule is quest index 2 */
        
        msg_print("Aule smiles with approval and returns to his eternal labors.");
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver_silent(R_IDX_AULE);
        
        return;
    }
    
    /* Handle other quest states */
    if (p_ptr->aule_quest == AULE_QUEST_ACTIVE)
    {
        msg_print("Aule watches you with eyes like glowing coals:");
        msg_print("'The forge awaits your skill. Show me what you can create.'");
        return;
    }
    
    /* Default message */
    msg_print("Aule the Smith regards you with interest.");
}

/*
 * Handle Mandos interaction
 */
void mandos_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Handle first encounter - initialize quest */
    if (p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED)
    {
        log_trace("First encounter with Mandos - setting to GIVER_PRESENT");
        p_ptr->mandos_quest = MANDOS_QUEST_GIVER_PRESENT;
        p_ptr->mandos_level = p_ptr->depth;
        /* Don't start the actual quest conversation yet, let them talk again */
        msg_print("You encounter Mandos, the Doomsman of the Valar.");
        msg_print("His stern gaze weighs upon your soul, as if judging your worth.");
        return;
    }
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->mandos_quest != MANDOS_QUEST_GIVER_PRESENT && 
        p_ptr->mandos_quest != MANDOS_QUEST_ACTIVE &&
        p_ptr->mandos_quest != MANDOS_QUEST_SUCCESS &&
        p_ptr->mandos_quest != MANDOS_QUEST_REWARDED)
    {
        log_trace("mandos_quest_interaction called with invalid quest state: %d", p_ptr->mandos_quest);
        return;
    }
    
    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Mandos quest interaction - assigning Brodda quest");
        
        /* Set quest state */
        p_ptr->mandos_quest = MANDOS_QUEST_ACTIVE;
        p_ptr->mandos_level = p_ptr->depth;
        
        /* Only remove quest giver for roulette-based quests (Y:1) */
        quest_type* q_ptr = &quest_info[3]; /* Mandos is quest index 3 */
        if (q_ptr->quest_type == 1) { /* Y:1 = roulette-based */
            remove_quest_giver_silent(R_IDX_MANDOS);
            log_trace("Mandos quest giver removed silently (roulette-based quest)");
        } else {
            log_trace("Mandos quest giver NOT removed (vault-based quest)");
        }
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(3, &text_count); /* Mandos is quest index 3 */
        init_texts = prepend_repeat_context(QUEST_ID_MANDOS, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Mandos the Doomsman", init_texts, text_count, TERM_L_DARK, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Mandos speaks with the authority of the Valar:",
                "'Slay Brodda the Easterling and prove your worth.'"
            };
            quest_typewriter_menu("Mandos the Doomsman", fallback_texts, 2, TERM_L_DARK, TERM_WHITE);
        }
        
        log_trace("Mandos quest activated - player must slay Brodda");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE)
    {
        /* Check if Brodda is dead */
        if (is_brodda_dead())
        {
            p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
            
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(3, &completion_count); /* Mandos is quest index 3 */
            completion_texts = prepend_repeat_context(QUEST_ID_MANDOS, completion_texts, &completion_count, true);
            
            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Justice Served", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Mandos nods with solemn approval:",
                    "'Justice has been served. The path forward opens.'"
                };
                quest_typewriter_menu("Justice Served", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
            }
            
            log_trace("Mandos quest completed successfully");
        }
        else
        {
            msg_print("Mandos gazes at you with penetrating eyes:");
            msg_print("'Brodda the Easterling still draws breath within these halls.");
            msg_print("Until his tyranny is ended, you may not pass beyond.'");
            msg_print("");
            msg_print("'Remember - he who ruled Dor-lomin with an iron fist");
            msg_print("must face the justice he denied to others.'");
        }
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_SUCCESS)
    {
        log_trace("Mandos quest already completed - giving special ability reward");
        
        /* We'll show the same completion texts again since this is the reward phase */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(3, &completion_count); /* Mandos is quest index 3 */
        completion_texts = prepend_repeat_context(QUEST_ID_MANDOS, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Quest Reward", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Mandos acknowledges you with respect:",
                "'Accept the gift of my protection from mortal fears.'"
            };
            quest_typewriter_menu("Quest Reward", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }
        
        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
        log_trace("Mandos quest: marked as completed in metarun");
        
        /* Change quest state to prevent repeated interactions */
        p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(3); /* Mandos is quest index 3 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(3); /* Mandos is quest index 3 */
        
        msg_print("Mandos bows deeply and fades into shadow, his task complete.");
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver_silent(R_IDX_MANDOS);
        
        log_trace("Mandos quest reward given");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED)
    {
        log_trace("Mandos quest already rewarded - giving acknowledgment");
        msg_print("Mandos nods with solemn respect:");
        msg_print("'The task is done, and the doom has been fulfilled.'");
        msg_print("'Your path continues ever deeper into the halls of Mandos.'");
    }
}

/*
 * Check if player is adjacent to Mandos and handle interaction
 */
void check_mandos_quest_interaction(void)
{
    int i, y, x;
    static s32b last_interaction_turn = -1;
    
    log_trace("check_mandos_quest_interaction called, quest state: %d, turn: %d", p_ptr->mandos_quest, turn);
    
    /* Prevent multiple interactions in the same turn */
    if (last_interaction_turn == turn)
    {
        log_trace("Already interacted this turn, skipping");
        return;
    }
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED &&
        p_ptr->mandos_quest != MANDOS_QUEST_GIVER_PRESENT && 
        p_ptr->mandos_quest != MANDOS_QUEST_ACTIVE &&
        p_ptr->mandos_quest != MANDOS_QUEST_SUCCESS &&
        p_ptr->mandos_quest != MANDOS_QUEST_REWARDED)
    {
        log_trace("Quest not in correct state (%d), returning", p_ptr->mandos_quest);
        return;
    }
    
    /* Check all adjacent squares for Mandos */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];
        
        if (in_bounds(y, x))
        {
            s16b m_idx = cave_m_idx[y][x];

            if (m_idx <= 0 || m_idx >= mon_max)
                continue;

            monster_type* m_ptr = &mon_list[m_idx];
            if (!m_ptr)
                continue;

            if (m_ptr->r_idx <= 0 || m_ptr->r_idx >= z_info->r_max)
                continue;

            if (m_ptr->r_idx == R_IDX_MANDOS)
            {
                log_trace("Found Mandos, calling interaction (turn %d)", turn);
                last_interaction_turn = turn;
                mandos_quest_interaction();
                return;
            }
        }
    }
    
    log_trace("No Mandos found adjacent");
}

/*
 * Handle monster death for Mandos quest
 */
void check_mandos_quest_completion(int r_idx)
{
    if (p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE)
    {
        log_trace("Mandos quest: Checking completion after death of r_idx %d", r_idx);
        
        /* Check if Brodda was killed */
        if (r_idx == R_IDX_ALDOR)  /* Brodda (formerly Aldor) */
        {
            p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
            
            msg_print("Brodda the Easterling falls! His tyranny is ended at last.");
            msg_print("The spirits of Dor-lomin can finally know peace.");
            msg_print("Return to Mandos the Doomsman to claim your reward.");
            
            log_trace("Mandos quest completed - Brodda slain");
        }
    }
}

/*
 * Handle quest completion checking for Orome hunting quest
 */
void check_orome_quest_completion(void)
{
    if (p_ptr->orome_quest == OROME_QUEST_ACTIVE) {
        /* Check thresholds for each monster type */
        bool quest_complete = false;
        cptr monster_name = "";
        int kill_count = 0;
        
        if (p_ptr->orome_wolves_killed >= 100) {
            quest_complete = true;
            monster_name = "wolves";
            kill_count = p_ptr->orome_wolves_killed;
        }
        else if (p_ptr->orome_spiders_killed >= 80) {
            quest_complete = true;
            monster_name = "spiders";
            kill_count = p_ptr->orome_spiders_killed;
        }
        else if (p_ptr->orome_serpents_killed >= 60) {
            quest_complete = true;
            monster_name = "serpents";
            kill_count = p_ptr->orome_serpents_killed;
        }
        else if (p_ptr->orome_vampires_killed >= 30) {
            quest_complete = true;
            monster_name = "vampires";
            kill_count = p_ptr->orome_vampires_killed;
        }
        
        if (quest_complete) {
            p_ptr->orome_quest = OROME_QUEST_SUCCESS;
            
            msg_format("The hunt is complete! You have slain %d %s, proving your prowess!", 
                       kill_count, monster_name);
            msg_print("Orome the Huntsman will be pleased with your mastery.");
            msg_print("Seek him out to claim your reward - the knowledge of Unique Bane!");
            
            log_trace("Orome quest completed - %d %s slain (wolves=%d, spiders=%d, serpents=%d, vampires=%d)", 
                     kill_count, monster_name,
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed, 
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
    }
}

/*
 * Handle interaction with Niena for the mercy quest
 */
void niena_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->niena_quest != NIENA_QUEST_GIVER_PRESENT && 
        p_ptr->niena_quest != NIENA_QUEST_SUCCESS)
    {
        log_trace("niena_quest_interaction called with invalid quest state: %d", p_ptr->niena_quest);
        return;
    }
    
    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Niena quest interaction - offering mercy quest");
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(4, &text_count); /* Nienna is quest index 4 */
        init_texts = prepend_repeat_context(QUEST_ID_NIENA, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Niena, Lady of Pity", init_texts, text_count, TERM_L_BLUE, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Niena, Lady of Pity, speaks with a voice full of sorrow and hope:",
                "'Show mercy to the creatures here and find the downward path.'"
            };
            quest_typewriter_menu("Niena, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
        }
        
        /* Accept the quest */
        p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
        p_ptr->niena_monsters_seen = 0;
        p_ptr->niena_monsters_killed = 0;
        p_ptr->niena_level = p_ptr->depth; /* Track where quest was started */
        
        /* Remove the quest giver now that quest is accepted without
         * showing the generic reward/departure message before the quest text.
         */
        remove_quest_giver_silent(R_IDX_NIENA);
        
        /* Make all stairs visible */
        int y, x;
        for (y = 0; y < p_ptr->cur_map_hgt; y++) {
            for (x = 0; x < p_ptr->cur_map_wid; x++) {
                if (cave_feat[y][x] == FEAT_MORE || cave_feat[y][x] == FEAT_MORE_SHAFT ||
                    cave_feat[y][x] == FEAT_LESS || cave_feat[y][x] == FEAT_LESS_SHAFT) {
                    cave_info[y][x] |= CAVE_MARK;
                    cave_info[y][x] |= CAVE_SEEN;
                }
            }
        }
        
        msg_print("The stairs throughout the level become clearly visible to you.");
        msg_print("Niena fades away, but her presence lingers in your heart.");
        
        /* Update display */
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
        p_ptr->redraw |= (PR_MAP);
        handle_stuff();
        
        log_trace("Niena quest started - all stairs revealed, monsters_seen=%d, monsters_killed=%d", 
                 p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
    }
    else if (p_ptr->niena_quest == NIENA_QUEST_SUCCESS)
    {
        log_trace("Completing Niena quest - giving enhanced stealth reward");
        
        /* Calculate the stealth bonus: 10 * (seen - killed) / seen, rounded up */
        int stealth_bonus = 0;
        if (p_ptr->niena_monsters_seen > 0) {
            /* Using ceiling division: (a + b - 1) / b */
            int mercy_ratio_times_10 = (10 * (p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed));
            stealth_bonus = (mercy_ratio_times_10 + p_ptr->niena_monsters_seen - 1) / p_ptr->niena_monsters_seen;
        }
        
        if (stealth_bonus > 0) {
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(4, &completion_count); /* Nienna is quest index 4 */
            completion_texts = prepend_repeat_context(QUEST_ID_NIENA, completion_texts, &completion_count, true);
            
            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Niena, Lady of Pity", completion_texts, completion_count, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Niena appears with tears of joy in her eyes!",
                    "'You have shown that true strength lies in restraint.'"
                };
                quest_typewriter_menu("Niena, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
            }
            
            /* Show the specific numbers and bonus after the main dialogue */
            msg_format("You encountered %d creatures but spared %d of them.", 
                      p_ptr->niena_monsters_seen, p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed);
            msg_format("You gain +%d effective stealth from your mercy.", stealth_bonus);
        } else {
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(4, &completion_count); /* Nienna is quest index 4 */
            completion_texts = prepend_repeat_context(QUEST_ID_NIENA, completion_texts, &completion_count, true);
            
            if (completion_texts && completion_count > 1) {
                /* Use alternate completion text if available */
                cptr alt_texts[] = {completion_texts[1]};
                quest_typewriter_menu("Niena, Lady of Pity", alt_texts, 1, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Niena appears, her expression neutral.",
                    "'You have completed the task, though perhaps not as I hoped.'"
                };
                quest_typewriter_menu("Niena, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
            }
            
            /* Show the specific numbers after the main dialogue */
            msg_format("You encountered %d creatures but spared %d of them.", 
                      p_ptr->niena_monsters_seen, p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed);
        }
        
        /* Clear quest state */
        p_ptr->niena_quest = NIENA_QUEST_REWARDED;
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(4); /* Nienna is quest index 4 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(4); /* Nienna is quest index 4 */
        
        /* Mark quest completion in metarun for score/persistence */
        metarun_mark_quest_completed(METARUN_QUEST_NIENA);
        log_trace("Niena quest marked complete in metarun and Mercy oath unlocked");
        
        msg_print("Niena smiles sadly and fades away, leaving you with her blessing.");
        
        /* Keep Niena's custom farewell message without appending the
         * generic quest giver departure line.
         */
        remove_quest_giver_silent(R_IDX_NIENA);
        
        /* Recalculate bonuses to apply the new stealth bonus */
        p_ptr->update |= (PU_BONUS);
        handle_stuff();
        
        log_trace("Niena quest completed and rewarded - stealth bonus: %d", stealth_bonus);
    }
}

/*
 * Check if player is adjacent to Niena and handle interaction
 */
void check_niena_quest_interaction(void)
{
    int y, x;
    monster_type* m_ptr;
    
    /* Only check if quest is in appropriate state */
    if (p_ptr->niena_quest != NIENA_QUEST_GIVER_PRESENT && 
        p_ptr->niena_quest != NIENA_QUEST_SUCCESS)
    {
        return;
    }
    
    log_trace("check_niena_quest_interaction: checking adjacency, quest state: %d", p_ptr->niena_quest);
    
    /* Look in adjacent squares for Niena */
    for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
    {
        for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
        {
            /* Skip the player's own square */
            if (y == p_ptr->py && x == p_ptr->px) continue;
            
            /* Skip invalid coordinates */
            if (!in_bounds(y, x)) continue;
            
            /* Check if there's a monster here */
            if (cave_m_idx[y][x] > 0)
            {
                m_ptr = &mon_list[cave_m_idx[y][x]];
                
                /* Is it Niena? */
                if (m_ptr->r_idx == R_IDX_NIENA)
                {
                    log_trace("Found Niena adjacent at (%d, %d), triggering interaction", y, x);
                    niena_quest_interaction();
                    return;
                }
            }
        }
    }
}

/*
 * Check if the player has completed the Niena mercy quest by reaching down stairs
 */
void check_niena_quest_completion(void)
{
    /* Only check if quest is active */
    if (p_ptr->niena_quest != NIENA_QUEST_ACTIVE) {
        return;
    }
    
    /* Check if player is on down stairs */
    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE || 
        cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT) {
        
        log_trace("Player reached down stairs during Niena quest - quest completed!");
        log_trace("Final counts: seen=%d, killed=%d", p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
        
        p_ptr->niena_quest = NIENA_QUEST_SUCCESS;
        
        msg_print("As you step onto the stairs, you feel Niena's presence return.");
        msg_print("'You have done well, showing mercy where others would show only violence.'");
        msg_print("Wait here a moment - she wishes to speak with you.");
        
        /* Make Niena appear near the player for the reward interaction */
        int attempts;
        bool niena_spawned = false;
        
        for (attempts = 0; attempts < 20 && !niena_spawned; attempts++)
        {
            int try_y = p_ptr->py + rand_range(-3, 3);
            int try_x = p_ptr->px + rand_range(-3, 3);
            
            /* Must be valid coordinates and floor */
            if (in_bounds(try_y, try_x) && cave_floor_bold(try_y, try_x) && 
                cave_m_idx[try_y][try_x] == 0 &&
                distance(p_ptr->py, p_ptr->px, try_y, try_x) >= 2)
            {
                if (place_monster_one(try_y, try_x, R_IDX_NIENA, true, true, NULL))
                {
                    niena_spawned = true;
                    log_trace("Niena spawned near stairs at (%d, %d) for quest completion", try_y, try_x);
                }
            }
        }
        
        if (!niena_spawned) {
            log_trace("Failed to spawn Niena for quest completion - will complete anyway");
            /* Complete the quest directly if spawning fails */
            niena_quest_interaction();
        }
    }
}

/*
 * Handle interaction with Orome for the hunting quest
 */
void orome_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;
    
    /* Safety check - ensure valid quest state */
    if (p_ptr->orome_quest != OROME_QUEST_GIVER_PRESENT && 
        p_ptr->orome_quest != OROME_QUEST_SUCCESS)
    {
        log_trace("orome_quest_interaction called with invalid quest state: %d", p_ptr->orome_quest);
        return;
    }
    
    if (p_ptr->orome_quest == OROME_QUEST_GIVER_PRESENT)
    {
        log_trace("Starting Orome quest interaction - offering hunting quest");
        
        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(5, &text_count); /* Orome is quest index 5 */
        init_texts = prepend_repeat_context(QUEST_ID_OROME, init_texts, &text_count, false);
        
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Orome the Huntsman", init_texts, text_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Orome the Huntsman regards you with keen eyes:",
                "'Prove your skill as a hunter. The dark creatures multiply.'"
            };
            quest_typewriter_menu("Orome the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
        }
        
        /* Determine hunt target based on dungeon depth */
        int depth = p_ptr->depth;
        cptr target_name;
        int target_count;
        
        if (depth <= 250) {
            p_ptr->orome_target_type = OROME_TARGET_WOLF;
            target_count = 100;
            target_name = "wolves";
        } else if (depth <= 500) {
            p_ptr->orome_target_type = OROME_TARGET_SPIDER;
            target_count = 80;
            target_name = "spiders";
        } else if (depth <= 750) {
            p_ptr->orome_target_type = OROME_TARGET_SERPENT;
            target_count = 60;
            target_name = "serpents";
        } else {
            p_ptr->orome_target_type = OROME_TARGET_VAMPIRE;
            target_count = 30;
            target_name = "vampires";
        }
        
        /* Accept the quest */
        p_ptr->orome_quest = OROME_QUEST_ACTIVE;
        p_ptr->orome_target_count = target_count;
        p_ptr->orome_killed_count = 0;
        
        /* Remove the quest giver now that quest is accepted without
         * showing the generic reward/departure message before the quest text.
         */
        remove_quest_giver_silent(R_IDX_OROME);
        
        msg_format("You must hunt and slay %d %s to prove your prowess.", target_count, target_name);
        msg_print("Return when the hunt is complete to claim your reward.");
        msg_print("Orome fades into the wild, but his presence lingers in your soul.");
        
        log_trace("Orome quest started - hunt %d %s at depth %d", 
                 target_count, target_name, depth);
    }
    else if (p_ptr->orome_quest == OROME_QUEST_SUCCESS)
    {
        log_trace("Completing Orome quest - giving Unique Bane reward");
        
        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(5, &completion_count); /* Orome is quest index 5 */
        completion_texts = prepend_repeat_context(QUEST_ID_OROME, completion_texts, &completion_count, true);
        
        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Orome the Huntsman", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Orome appears with a proud smile!",
                "'You have proven yourself a true hunter of the wild.'"
            };
            quest_typewriter_menu("Orome the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
        }
        
        /* Show the specific numbers after the main dialogue */
        cptr monster_names[] = {"wolves", "spiders", "serpents", "vampires"};
        cptr monster_name = (p_ptr->orome_target_type >= 1 && p_ptr->orome_target_type <= 4) 
                           ? monster_names[p_ptr->orome_target_type - 1] : "creatures";
        
        msg_format("You have slain %d %s as commanded.", 
                  p_ptr->orome_target_count, monster_name);
        msg_print("You learn the secret of hunting unique creatures!");
        
        /* Clear quest state */
        p_ptr->orome_quest = OROME_QUEST_REWARDED;
        log_trace("Orome reward: Quest state set to REWARDED (%d)", OROME_QUEST_REWARDED);
        
        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(5); /* Orome is quest index 5 */
        
        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_OROME);
        
        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(5); /* Orome is quest index 5 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
            msg_format("The %s is now available for future characters in this lineage!", 
                      get_oath_name_from_id(oath_id));
        }
        
        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_OROME);
        
        log_trace("Orome quest completed - Unique Bane granted, oath unlocked");
    }
}

/*
 * Check if player should interact with Orome
 */
void check_orome_quest_interaction(void)
{
    /* Only check if quest can be started or completed */
    if (p_ptr->orome_quest != OROME_QUEST_GIVER_PRESENT && 
        p_ptr->orome_quest != OROME_QUEST_SUCCESS) {
        return;
    }
    
    log_trace("check_orome_quest_interaction: checking adjacency, quest state: %d", p_ptr->orome_quest);
    
    /* Check for adjacent Orome */
    int y, x;
    for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
    {
        for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
        {
            if (in_bounds(y, x) && cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                if (m_ptr->r_idx == R_IDX_OROME)
                {
                    log_trace("Found Orome adjacent - triggering interaction");
                    orome_quest_interaction();
                    return;
                }
            }
        }
    }
    
    /* If quest is completed but Orome isn't adjacent, try to spawn him */
    if (p_ptr->orome_quest == OROME_QUEST_SUCCESS)
    {
        log_trace("Orome quest complete but no Orome found - trying to spawn");
        
        /* Try to find a suitable spot near the player */
        for (y = p_ptr->py - 3; y <= p_ptr->py + 3; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                    cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
                {
                    if (place_monster_one(y, x, R_IDX_OROME, true, true, NULL))
                    {
                        msg_print("Orome the Huntsman materializes nearby, ready to honor your success!");
                        log_trace("Successfully spawned Orome for quest completion");
                        return;
                    }
                }
            }
        }
        
        log_trace("Failed to spawn Orome for quest completion - will complete anyway");
        /* Complete the quest directly if spawning fails */
        orome_quest_interaction();
    }
}

/*
 * Grant the unique bane special ability to the player
 * This function can be called from quests, debug commands, or other rewards
 */
void grant_unique_bane_ability(void)
{
    log_trace("grant_unique_bane_ability: Function called, checking current state");
    log_trace("grant_unique_bane_ability: have_ability[S_SPC][SPC_UNIQUE_BANE] = %s",
             p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE");
    
    if (p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE])
    {
        log_trace("grant_unique_bane_ability: Player already has ability, showing message and returning");
        msg_print("You already possess the power to hunt unique creatures effectively.");
        return;
    }
    
    log_trace("grant_unique_bane_ability: Setting ability flags to TRUE");
    /* Grant the ability */
    p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] = true;
    p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE] = true;
    
    log_trace("grant_unique_bane_ability: After setting flags - have_ability=%s, active_ability=%s",
             p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE",
             p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE");
    
    msg_print("You have learned the art of Unique Bane!");
    msg_print("You gain significant advantages when fighting unique creatures.");
    
    log_trace("Granted Unique Bane special ability to player");
    
    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);
    handle_stuff();
}

