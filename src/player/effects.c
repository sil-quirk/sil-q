#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

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

/* Túrin character resistance function - 70% chance to resist bad effects but becomes raged */
bool turin_resist_bad_effect(void)
{
    /* Check if player has Túrin character flag */
    if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_WIL_TURIN))
        return (false);

    /* 70% chance to resist */
    if (one_in_(10) || one_in_(10) || one_in_(10))
        return (false);

    /* Túrin resists! Apply rage */
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
    /* Túrin character resistance check first */
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
    /* Túrin character resistance check first */
    if (turin_resist_bad_effect())
    {
        log_trace("CONFUSION DEBUG: Túrin character resisted confusion");
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

    /* Túrin character resistance check first */
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
    /* Túrin character resistance check first */
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
            hallucination_randomize_style_transitions();
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p_ptr->image)
        {
            msg_print("You can see clearly again.");
            hallucination_clear_style_transitions();
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
    /* Túrin character resistance check first */
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

        /* Túrin character has 50% chance to become hallucinated when raged */
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
