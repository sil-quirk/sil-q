/* File: xtra2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * The saving throw is a will skill check.
 *
 * Note that the player is resisting and thus wins ties.
 */
extern bool saving_throw(monster_type *m_ptr, int resistance)
{
	int player_score = p_ptr->skill_use[S_WIL];
	int difficulty;
	
	if (m_ptr == NULL)	difficulty = 10;
	else				difficulty = monster_skill(m_ptr, S_WIL);
	
	difficulty -= 10 * resistance;
	
	if (skill_check(m_ptr, difficulty, player_score, PLAYER) > 0)
	{
		return (FALSE);
	}
	else
	{
		return (TRUE);
	}
}


// Auxilliary function for the allow_player functions
bool allow_player_aux(monster_type *m_ptr, int player_flag, u32b ident_flag)
{
	bool resistance = 0;
	
	if (player_flag > 0)
	{ 
		// possibly identify relevant items
		ident_resist(ident_flag);
		
		// makes things much easier
		resistance = player_flag;
	}
	
	if (saving_throw(m_ptr, resistance)) return (FALSE);
	   
	// Don't have the right resists or failed the save
	return (TRUE);
}



/* Players with blindness resistance or who make their saving throw don't get blinded */
bool allow_player_blind(monster_type *m_ptr)
{
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
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->blind)
		{
			msg_print("You are blind!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->blind)
		{
			msg_print("You can see again.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->blind = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Fully update the visuals */
	p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Redraw the "blind" */
	p_ptr->redraw |= (PR_BLIND);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/* Players with confusion resistance or who make their saving throw don't get confused */
bool allow_player_confusion(monster_type *m_ptr)
{
	return (allow_player_aux(m_ptr, p_ptr->resist_confu, TR2_RES_CONFU));
}


/*
 * Set "p_ptr->confused", notice observable changes
 */
bool set_confused(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->confused)
		{
			msg_print("You are confused!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->confused)
		{
			msg_print("You feel less confused now.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->confused = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw the "confused" */
	p_ptr->redraw |= (PR_CONFUSED);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->poisoned", notice observable changes
 *
 */
bool set_poisoned(int v)
{
	int new;
	int change;
	
	bool notice = FALSE;
	
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
				notice = TRUE;
			}
			else if (change >= 10)
			{
				msg_print("You have been badly poisoned.");
				notice = TRUE;
			}
			else
			{
				msg_print("You have been poisoned.");
				notice = TRUE;
			}
		}
		else
		{
			if (change >= 20)
			{
				msg_print("You have been severely poisoned.");
				notice = TRUE;
			}
			else if (change >= 10)
			{
				msg_print("You have been badly poisoned.");
				notice = TRUE;
			}
			else
			{
				msg_print("You have been further poisoned.");
				notice = TRUE;
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
			notice = TRUE;
		}
		else if (-change > (p_ptr->poisoned + 4) / 5)
		{
			msg_print("You can feel the poison weakening.");
			notice = TRUE;
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
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Result */
	return (TRUE);
}


/* Players with fear resistance or who make their saving throw don't get terrified */
bool allow_player_fear(monster_type *m_ptr)
{
    // rage is incompatible with fear -- more than just a resistance
    if (p_ptr->rage)    return (FALSE);
    else                return (allow_player_aux(m_ptr, p_ptr->resist_fear, TR2_RES_FEAR));
}


/*
 * Set "p_ptr->afraid", notice observable changes
 */
bool set_afraid(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->afraid)
		{
			msg_print("You are terrified!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->afraid)
		{
			msg_print("You feel bolder now.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->afraid = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw the "afraid" */
	p_ptr->redraw |= (PR_AFRAID);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/* Players with free action or who make their saving throw don't get entranced */
bool allow_player_entrancement(monster_type *m_ptr)
{
	return (allow_player_aux(m_ptr, p_ptr->free_act, TR2_FREE_ACT));
}


/*
 * Set "p_ptr->entranced", notice observable changes
 */
bool set_entranced(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->entranced)
		{
			msg_print("You fall into a deep trance!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->entranced)
		{
			msg_print("The trance is broken!");
			p_ptr->was_entranced = TRUE;
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->entranced = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw the state */
	p_ptr->redraw |= (PR_STATE);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);
		
	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/* Players with resist hallucination or who make their saving throw don't hallucinate */
bool allow_player_image(monster_type *m_ptr)
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
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->image)
		{
			msg_print("Fantastic visions appear before your eyes.");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->image)
		{
			msg_print("You can see clearly again.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->image = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->fast", notice observable changes
 */
bool set_fast(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->fast)
		{
			msg_print("You feel yourself moving faster!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->fast)
		{
			msg_print("You feel yourself slow down.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->fast = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/* Players with free action or who make their saving throw don't get slowed */
bool allow_player_slow(monster_type *m_ptr)
{
	return (allow_player_aux(m_ptr, p_ptr->free_act, TR2_FREE_ACT));
}


/*
 * Set "p_ptr->slow", notice observable changes
 */
bool set_slow(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->slow)
		{
			msg_print("You feel yourself moving slower!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->slow)
		{
			msg_print("You feel yourself speed up.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->slow = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->rage", notice observable changes
 */
bool set_rage(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->rage)
		{
			msg_print("You burst into a furious rage!");
			notice = TRUE;
			
			/* Redraw map */
			p_ptr->redraw |= (PR_MAP);
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->rage)
		{
			msg_print("Your rage subsides.");
			notice = TRUE;

			//do_res_stat(A_STR, 1);
			//do_res_stat(A_CON, 1);

			/* Redraw map */
			p_ptr->redraw |= (PR_MAP);
		}
	}

	/* Use the value */
	p_ptr->rage = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}

/*
 * Set "p_ptr->tmp_str", notice observable changes
 */
bool set_tmp_str(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->tmp_str)
		{
			msg_print("You feel stronger.");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->tmp_str)
		{
			msg_print("Your strength returns to normal.");
			do_res_stat(A_STR, 3);
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->tmp_str = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->tmp_dex", notice observable changes
 */
bool set_tmp_dex(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->tmp_dex)
		{
			msg_print("You feel more agile.");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->tmp_dex)
		{
			msg_print("Your dexterity returns to normal.");
			do_res_stat(A_DEX, 3);
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->tmp_dex = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->tmp_con", notice observable changes
 */
bool set_tmp_con(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->tmp_con)
		{
			msg_print("You feel more resilient.");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->tmp_con)
		{
			msg_print("Your constitution returns to normal.");
			do_res_stat(A_CON, 3);
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->tmp_con = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->tmp_gra", notice observable changes
 */
bool set_tmp_gra(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->tmp_gra)
		{
			msg_print("You feel more attuned to the world.");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->tmp_gra)
		{
			msg_print("Your grace returns to normal.");
			do_res_stat(A_GRA, 3);
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->tmp_gra = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->tim_invis", notice observable changes
 *
 * Note the use of "PU_MONSTERS", which is needed because
 * "p_ptr->tim_image" affects monster visibility.
 */
bool set_tim_invis(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		if (!p_ptr->tim_invis)
		{
			msg_print("Your vision sharpens.");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->tim_invis)
		{
			msg_print("Your eyes feel less sensitive.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->tim_invis = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Update the monsters XXX */
	p_ptr->update |= (PU_MONSTERS);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
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
	bool notice = FALSE;
	
	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;
	
	/* If Increasing */
	if (v > p_ptr->darkened)
	{
		if (!p_ptr->blind)
		{
			msg_print("Your light dims.");
			notice = TRUE;
		}
	}
	
	/* If Finished */
	if (v == 0)
	{
		if (p_ptr->darkened && !p_ptr->blind)
		{
			msg_print("Your light grows brighter again.");
			notice = TRUE;
		}
	}
	
	/* Use the value */
	p_ptr->darkened = v;
	
	/* Nothing to notice */
	if (!notice) return (FALSE);
	
	/* Disturb */
	disturb(0, 0);
	
	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);
	
	/* Update the monsters XXX */
	p_ptr->update |= (PU_MONSTERS);

	/* Handle stuff */
	handle_stuff();
	
	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->oppose_fire", notice observable changes
 */
bool set_oppose_fire(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		/*then check if player has permanent resist to fire*/
		if ((p_ptr->resist_fire > 1) && (!p_ptr->oppose_fire))
		{
			msg_print("You feel more resistant to fire!");
			notice = TRUE;
		}

		/*if player has neither*/
		else if (!p_ptr->oppose_fire)
		{
			msg_print("You feel resistant to fire!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->oppose_fire)
		{
			msg_print("You feel less resistant to fire.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->oppose_fire = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw resistances */
	p_ptr->redraw |= (PR_RESIST);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->oppose_cold", notice observable changes
 */
bool set_oppose_cold(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		/*then check if player has permanent resist to cold*/
		if ((p_ptr->resist_cold > 1) && (!p_ptr->oppose_cold))
		{
			msg_print("You feel more resistant to cold!");
			notice = TRUE;
		}
		/*if player has neither*/
		else if (!p_ptr->oppose_cold)
		{
			msg_print("You feel resistant to cold!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->oppose_cold)
		{
			msg_print("You feel less resistant to cold.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->oppose_cold = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw resistances */
	p_ptr->redraw |= (PR_RESIST);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->oppose_pois", notice observable changes
 */
bool set_oppose_pois(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Open */
	if (v)
	{
		/*Then check if player has permanent resist to poison*/
		if ((p_ptr->resist_pois > 1) && (!p_ptr->oppose_pois))
		{
			msg_print("You feel more resistant to poison!");
			notice = TRUE;
		}

		/*if player doesn't have permanent resistance to poison*/
		else if (!p_ptr->oppose_pois)
		{
			msg_print("You feel resistant to poison!");
			notice = TRUE;
		}
	}

	/* Shut */
	else
	{
		if (p_ptr->oppose_pois)
		{
			msg_print("You feel less resistant to poison.");
			notice = TRUE;
		}
	}

	/* Use the value */
	p_ptr->oppose_pois = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Redraw resistances */
	p_ptr->redraw |= (PR_RESIST);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
}


/* Players with stun resistance or who make their saving throw don't get stunned */
bool allow_player_stun(monster_type *m_ptr)
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

	bool notice = FALSE;
	
	/*  Don't increase stunning if stunning value is greater than 100.
	 *  this is an effort to eliminate the "knocked out" instadeath.
	 */
	if ((p_ptr->stun > 100) && (v > p_ptr->stun)) return (FALSE);

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
		p_ptr->blind = MAX(p_ptr->blind,2);
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
		notice = TRUE;
	}

	/* Decrease stun */
	else if (new_aux < old_aux)
	{
		// waking up from Knock Out
		if (old_aux == 3)
		{
			msg_print("You wake up.");
			
			// undo the temporary blinding if waking up from KO
			p_ptr->blind = MAX(p_ptr->blind-1,0);
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
		notice = TRUE;
	}

	/* Use the value */
	p_ptr->stun = v;

	/* No change */
	if (!notice) return (FALSE);

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
	return (TRUE);
}


/*
 * Set "p_ptr->cut", notice observable changes
 *
 * Note the special code to only notice "range" changes.
 */
bool set_cut(int v)
{
	bool notice = FALSE;

	/* Hack -- Force good values */
	v = (v > 100) ? 100 : (v < 0) ? 0 : v;

	/* Increase cut */
	if (v > p_ptr->cut)
	{
		if (v - p_ptr->cut >= 20)
		{
			msg_print("You have been given a severe cut.");
			notice = TRUE;
		}
		else if (v - p_ptr->cut >= 10)
		{
			msg_print("You have been given a deep cut.");
			notice = TRUE;
		}
		else
		{
			msg_print("You have been given a cut.");
			notice = TRUE;
		}
	}
	/* Decrease cut */
	if (v < p_ptr->cut)
	{
		if ((v == 0) && (p_ptr->chp > 0))
		{
			msg_print("The bleeding stops.");
			disturb(0, 0);
			notice = TRUE;
		}
		else if ((p_ptr->cut - v) > (p_ptr->cut + 4) / 5)
		{
			msg_print("The bleeding slows.");
			notice = TRUE;
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
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Result */
	return (TRUE);
}


/*
 * Set "p_ptr->food", notice observable changes
 */
bool set_food(int v)
{
	int old_aux, new_aux;

	bool notice = FALSE;

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

	/* Gorged */
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

	/* Gorged */
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

			/* Bloated */
			case 5:
			{
				msg_print("You have gorged yourself!");
				msg_print("You can't eat or drink any more until you recover.");
				break;
			}
		}

		/* Change */
		notice = TRUE;
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
				msg_print("You are no longer gorged.");
				break;
			}
		}

		/* Change */
		notice = TRUE;
        
		// maybe identify hunger / sustenance
		ident_hunger();
	}

	/* Use the value */
	p_ptr->food = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	/* Disturb */
	disturb(0, 0);

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Redraw hunger */
	p_ptr->redraw |= (PR_HUNGER);

	/* Handle stuff */
	handle_stuff();

	/* Result */
	return (TRUE);
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
        if (p_ptr->depth == MORGOTH_DEPTH - 2)  dice = 3; // as this means you will only fall one floor
        else                                    dice = 6;
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
		update_combat_rolls1b(NULL, PLAYER, TRUE);
		update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, FALSE);
		
		/* Take the damage */
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
	if (p_ptr->exp < 0) p_ptr->exp = 0;

	/* Hack -- lower limit */
	if (p_ptr->new_exp < 0) p_ptr->new_exp = 0;

	/* Hack -- upper limit */
	if (p_ptr->exp > PY_MAX_EXP) p_ptr->exp = PY_MAX_EXP;

	/* Hack -- upper limit */
	if (p_ptr->new_exp > PY_MAX_EXP) p_ptr->new_exp = PY_MAX_EXP;

	/* Hack -- maintain "max" experience */
	if (p_ptr->new_exp > p_ptr->exp) p_ptr->new_exp = p_ptr->exp;

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
	if (amount > p_ptr->new_exp) amount = p_ptr->new_exp;

	/* Lose some experience */
	p_ptr->new_exp -= amount;
	p_ptr->exp -= amount;

	/* Check Experience */
	check_experience();
}


/*
 *  Choose the location of a random staircase on the level
 */
bool random_stair_location(int *sy, int *sx)
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
			if (cave_stair_bold(y,x))
			{
				stair_y[stair_num] = y;
				stair_x[stair_num] = x;
				if (stair_num < 99) stair_num++;
			}
		}
	}
	
	// If no valid stairs are found, then bail out (paranoia)
	if (stair_num == 0)
	{
		return (FALSE);
	}
	
	// Choose a random stair
	stair_num = rand_int(stair_num);
	*sy = stair_y[stair_num];
	*sx = stair_x[stair_num];
	
	return (TRUE);
}


/*
 * Break the truce in Morgoth's throne room
 */
extern void break_truce(bool obvious)
{
	int i;
	
	monster_type *m_ptr = NULL; // default to soothe compiler warnings
	monster_race *r_ptr;
	
	char m_name[80];
	
	if (p_ptr->truce)
	{
		/* Scan all other monsters */
		for (i = mon_max - 1; i >= 1; i--)
		{
			/* Access the monster */
			m_ptr = &mon_list[i];
			r_ptr = &r_info[m_ptr->r_idx];
			
			/* Ignore dead monsters */
			if (!m_ptr->r_idx) continue;
			
			// Ignore monsters out of line of sight
			if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px)) continue;
			
			// Ignore unalert monsters
			if (m_ptr->alertness < ALERTNESS_ALERT) continue;
			
			/* Get the monster name (using 'something' for hidden creatures) */
			monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);
			
			p_ptr->truce = FALSE;
		}
		
		if (obvious) p_ptr->truce = FALSE;
		
		if (!p_ptr->truce)
		{
			if (!obvious)
			{
				msg_format("%^s lets out a cry! The tension is broken.", m_name);

				/* Make a lot of noise */
				update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
				monster_perception(FALSE, FALSE, -10);
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
				if (!m_ptr->r_idx) continue;
				
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
	monster_type *m_ptr;
	monster_race *r_ptr;
	monster_type *n_ptr;
	monster_race *nr_ptr;
	
	/*first check if there are monsters on both coordinates*/
	if (!(cave_m_idx[m1y][m1x] > 0)) return(FALSE);
	if (!(cave_m_idx[m2y][m2x] > 0)) return(FALSE);
	
	/* Access monster 1*/
	m_ptr = &mon_list[cave_m_idx[m1y][m1x]];
	r_ptr = &r_info[m_ptr->r_idx];
	
	/* Access monster 2*/
	n_ptr = &mon_list[cave_m_idx[m2y][m2x]];
	nr_ptr = &r_info[n_ptr->r_idx];
	
	/* Monsters have the same symbol */
	if (r_ptr->d_char == nr_ptr->d_char) return(TRUE);
	
	/*
	 * Same race (we are not checking all RF3 types
	 * because that would be true at
	 * the symbol check
	 */
	if ((r_ptr->flags3 & (RF3_DRAGON)) && (nr_ptr->flags3 & (RF3_DRAGON))) return(TRUE);
	if ((r_ptr->flags3 & (RF3_SERPENT)) && (nr_ptr->flags3 & (RF3_SERPENT))) return(TRUE);
	
	/*Not the same*/
	return(FALSE);
}


/*
 *  Cause a temporary penalty to morale in monsters of the same type who can see the 
 *  specified monster. (Used when it dies and for cruel blow).
 */
void scare_onlooking_friends(const monster_type *m_ptr, int amount)
{
	int i;
	int fy, fx, y, x;
	
	/* Location of main monster */
	fy = m_ptr->fy;
	fx = m_ptr->fx;
	
	/* Scan monsters */
	for (i = 1; i < mon_max; i++)
	{
		monster_type *n_ptr = &mon_list[i];
		monster_race *r_ptr = &r_info[n_ptr->r_idx];
		
		/* Skip dead monsters */
		if (!n_ptr->r_idx) continue;
		
		/* Location of other monster */
		y = n_ptr->fy;
		x = n_ptr->fx;
		
		// Only consider alert monsters of the same type in line of sight
		if ((n_ptr->alertness >= ALERTNESS_ALERT) && !(r_ptr->flags3 & (RF3_NO_FEAR)) &&
		    similar_monsters(fy, fx, y, x) && los(y, x, fy, fx))
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

extern void create_chosen_artefact(byte name1, int y, int x, bool identify)
{
	object_type *i_ptr;
	object_type object_type_body;
	artefact_type *a_ptr = &a_info[name1];
	
	// Don't generate it if one has already been generated
	if (a_ptr->cur_num > 0) return;
	
	// Don't generate it in no-artefact games, with one obvious exception
	if (birth_no_artefacts && (name1 != ART_MORGOTH_3)) return;
	
	/* Get local object */
	i_ptr = &object_type_body;
		
	/* Mega-Hack -- Prepare the base object for the artefact */
	object_prep(i_ptr, lookup_kind(a_ptr->tval, a_ptr->sval));

	/* Mega-Hack -- Mark this item as the artefact */
	i_ptr->name1 = name1;

	/* Mega-Hack -- Actually create the artefact */
	apply_magic(i_ptr, -1, TRUE, TRUE, TRUE, TRUE);
	
	// Identify it if desired
	if (identify)
	{
		object_aware(i_ptr);
		object_known(i_ptr);
	}

	/* Drop it in the dungeon */
	drop_near(i_ptr, -1, y, x);
	

}

/*
 * Drops the objects
 */
void drop_loot(monster_type *m_ptr)
{
	int j, y, x;
	
	int dump_item = 0;
	
	int number = 0;
		
	s16b this_o_idx, next_o_idx = 0;
		
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	
	bool visible = (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)));
	
	bool chest = (r_ptr->flags1 & (RF1_DROP_CHEST)) ? TRUE : FALSE;
	bool good = FALSE;
	bool great = FALSE;
	
	object_type *i_ptr;
	object_type object_type_body;
		
	int original_object_level = object_level;

	if (r_ptr->flags1 & (RF1_DROP_GOOD))
	{
		good = TRUE;
	}
	if (r_ptr->flags1 & (RF1_DROP_GREAT))
	{
		great = TRUE;
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
		object_type *o_ptr;
		
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
			create_chosen_artefact(ART_GROND, y, x, TRUE);
			
			// create the Iron Crown of Morgoth
			create_chosen_artefact(ART_MORGOTH_3, y, x, TRUE);
		}
		
		// Drop Deathblade treasures
		else if (r_ptr->d_char == '|')
		{
			if (!birth_no_artefacts && (r_ptr->flags1 & (RF1_UNIQUE)))
			{
				// create the Deathblade 'Delmereth'
				create_chosen_artefact(ART_DELMERETH, y, x, TRUE);
			}
			else
			{
				/* Get local object */
				i_ptr = &object_type_body;
				
				/* Mega-Hack -- Prepare deathblade */
				object_prep(i_ptr, lookup_kind(TV_SWORD, SV_DEATHBLADE));
				
				// otherwise just apply_magic to give a chance of being fine or special (not artefact)
				apply_magic(i_ptr, object_level, FALSE, FALSE, FALSE, FALSE);
				
				/* Drop it in the dungeon */
				drop_near(i_ptr, -1, y, x);
			}
			
		}
		// Drop Galvorn Armour of Maeglin
		else if (r_ptr->d_char == '@')
		{
			// create the Armour of Maeglin
			create_chosen_artefact(ART_MAEGLIN, y, x, FALSE);
		}
		// Drop Iron Spear of Boldog
		else if (r_ptr->d_char == 'o')
		{
			// create the Armour of Maeglin
			create_chosen_artefact(ART_BOLDOG, y, x, FALSE);
		}
		// Drop Glend
		else if (r_ptr->d_char == 'G')
		{
			// create the Greatsword 'Glend'
			create_chosen_artefact(ART_GLEND, y, x, FALSE);
		}
		// Drop Wolf-Hame of Drauglin
		else if (r_ptr->d_char == 'C')
		{
			// create the Wolf-Hame of Draugluin
			create_chosen_artefact(ART_DRAUGLUIN, y, x, FALSE);
		}
		// Drop Bat-Fell of Thuringwethil
		else if (r_ptr->d_char == 'v')
		{
			// create the Bet-Fell of Thuringwethil
			create_chosen_artefact(ART_THURINGWETHIL, y, x, FALSE);
		}
	}
	
	/* Determine how much we can drop */
	if ((r_ptr->flags1 & (RF1_DROP_33)) && percent_chance(33)) number++;
	if (r_ptr->flags1 & (RF1_DROP_100)) number += 1;
	if (r_ptr->flags1 & (RF1_DROP_1D2)) number += damroll(1, 2);
	if (r_ptr->flags1 & (RF1_DROP_2D2)) number += damroll(2, 2);
	if (r_ptr->flags1 & (RF1_DROP_3D2)) number += damroll(3, 2);
	if (r_ptr->flags1 & (RF1_DROP_4D2)) number += damroll(4, 2);
	
	// Favoured drops 1: arrows from archers
	if ((number > 0) && ((r_ptr->flags4 & (RF4_ARROW1)) || (r_ptr->flags4 & (RF4_ARROW2))) && percent_chance(r_ptr->freq_ranged / 2))
	{
		/* Get local object */
		i_ptr = &object_type_body;
		
		/* Wipe the object */
		object_wipe(i_ptr);
		
		/* Hack	-- Give the player an object */
		/* Get the object_kind */
		s16b k_idx = lookup_kind(TV_ARROW, SV_NORMAL_ARROW);
		
		/* Prepare the item */
		object_prep(i_ptr, k_idx);
		
		i_ptr->number = damroll(2,8);
		
		object_known(i_ptr);
		
		/* Assume seen XXX XXX XXX */
		dump_item++;
		
		/* Drop it in the dungeon */
		drop_near(i_ptr, -1, y, x);
		
		// Drop one fewer object to compensate
		number--;
	}
	
	// Favoured drops 2: torches from easterlings
	if ((number > 0) && (r_ptr->d_char == '@') && (r_ptr->light == 1) && (easter_time() || one_in_(3)))
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
			s16b k_idx  = lookup_kind(TV_FOOD, rand_int(9));
			
			/* Prepare the item */
			object_prep(i_ptr, k_idx);
		}
		
		// Normally just go for a torch
		else
		{
			/* Hack	-- Give the player an object */
			/* Get the object_kind */
			s16b k_idx = lookup_kind(TV_LIGHT, SV_LIGHT_TORCH);
			
			/* Prepare the item */
			object_prep(i_ptr, k_idx);
			
			i_ptr->timeout = rand_range(500,3000);
		}
		
		/* Assume seen XXX XXX XXX */
		dump_item++;
		
		/* Drop it in the dungeon */
		drop_near(i_ptr, -1, y, x);
		
		// Drop one fewer object to compensate
		number--;
	}
	
	/* Use the monster's level */
	object_level = r_ptr->level;
	
	/* Drop some objects */
	for (j = 0; j < number; j++)
	{		
		/* Get local object */
		i_ptr = &object_type_body;
		
		/* Wipe the object */
		object_wipe(i_ptr);
		
		/* Make Object */
		if (chest)
		{
			if (!make_object(i_ptr, good, great, DROP_TYPE_CHEST)) continue;
		}
		
		/* Make an object */
		else if (!make_object(i_ptr, good, great, DROP_TYPE_NOT_USELESS)) continue;
		
		/* Assume seen XXX XXX XXX */
		dump_item++;
		
		/* Drop it in the dungeon */
		drop_near(i_ptr, -1, y, x);
	}
	
	/* Reset the object level */
	object_level = original_object_level;
	
	/* Take note of any dropped treasure */
	if (visible && (dump_item))
	{
		/* Take notes on treasure */
		lore_treasure(cave_m_idx[m_ptr->fy][m_ptr->fx], dump_item);
	}
	
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
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	monster_lore *l_ptr = &l_list[m_ptr->r_idx];
    
	s32b new_exp;
    
	int multiplier = 1;


	/*
	 *   1. General monster death things
	 */

	// Special message and flag setting for killing Morgoth
	if (m_ptr->r_idx == R_IDX_MORGOTH)
	{
		p_ptr->morgoth_slain = TRUE;
		msg_print("BUG: Morgoth has been defeated in combat.");
		msg_print("But this is not possible within the fates Illuvatar has decreed.");
		msg_print("Please post an 'ultimate bug-report' on http://angband.oook.cz/forum/ explaining how this happened.");
		msg_print("But for now, let's run with it, since it's undeniably impressive.");

		// display the ultimate bug text
		pause_with_text(ultimate_bug_text, 5, 15);
	}
	
	/* If the player kills a Unique, write a note. */
   	if (r_ptr->flags1 & RF1_UNIQUE)
	{
		char note2[120];
 		char real_name[120];
		
		/* Get the monster's real name for the notes file */
		monster_desc_race(real_name, sizeof(real_name), m_ptr->r_idx);
			
		/* Write note */
		if (monster_nonliving(r_ptr) && (r_ptr->d_char == '|'))	my_strcpy(note2, format("Subdued %s", real_name), sizeof (note2));
		else if (monster_nonliving(r_ptr))						my_strcpy(note2, format("Destroyed %s", real_name), sizeof (note2));
		else													my_strcpy(note2, format("Slew %s", real_name), sizeof (note2));
		
 		do_cmd_note(note2, p_ptr->depth);
	}
	
	/* Update monster list window */
	p_ptr->window |= PW_MONLIST;
	
    /* Give some experience for the kill */
    new_exp = adjusted_mon_exp(r_ptr, TRUE);
    gain_exp(new_exp);
    p_ptr->kill_exp += new_exp;
    
    /* Uniques stay dead */
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        r_ptr->max_num = 0;
    }
    
    /* Count kills this life */
    if (l_ptr->pkills < MAX_SHORT) l_ptr->pkills++;
    
    /* Count kills in all lives */
    if (l_ptr->tkills < MAX_SHORT) l_ptr->tkills++;
    
    // since it was killed, it was definitely encountered
    if (!m_ptr->encountered)
    {
        int new_exp;
        
        new_exp = adjusted_mon_exp(r_ptr, FALSE);
        
        // gain experience for encounter
        gain_exp(new_exp);
        p_ptr->encounter_exp += new_exp;
        
        // update stats
        m_ptr->encountered = TRUE;
        l_ptr->psights++;
        if (l_ptr->tsights < MAX_SHORT) l_ptr->tsights++;
    }

    
	/*
	 *   2. Lower the morale of similar monsters that can see the deed.
	 */
	 
	// double effect for creatures with escorts
	if ((r_ptr->flags1 & (RF1_ESCORT)) || (r_ptr->flags1 & (RF1_ESCORTS)))	multiplier = 4;
	
	scare_onlooking_friends(m_ptr, -40 * multiplier);
	
	
	/*
	 *   3. Dropping items
	 */
	
	// drop the loot for non-territorial monsters
	if (!(r_ptr->flags2 & (RF2_TERRITORIAL)))
	{
        // monsters who fell into chasms also don't generate loot...
        if (!((cave_feat[m_ptr->fy][m_ptr->fx] == FEAT_CHASM) && !(r_ptr->flags2 & (RF2_FLYING))))
        {
            drop_loot(m_ptr);
        }
	}

	return;
}

/*
 * This adjusts a monster's raw experience point value according to the number killed so far
 * The formula is:
 *
 * (depth*10) / (kills+1)
 *
 * This is doubled for uniques.
 *
 * ((depth*25) * 4) / (kills + 4)  <- previous version
 *
 * 100 90 83 76 71 66 62  (10,10)   <- earliest version?
 * 100 80 66 57 50 44 40  (4,4)     <- this is the previous version (without the 1.5 multiplier)
 * 100 66 50 40 33 28 25  (2,2)
 * 100 50 33 25 20 16 14  (1,1)     <- this is the current version
 *
 * 100 90 81 72 65 59 53  (10%)     <- exponential alternatives
 * 100 80 64 51 40 32 25  (20%)     
 *
 * This function is called when gaining experience and when displaying it in monster recall.
 */
s32b adjusted_mon_exp(const monster_race *r_ptr, bool kill)
{
	s32b exp;
	
	int mexp = r_ptr->level * 10;
	
	if (kill)
	{
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			exp = mexp;
		}
		else
		{
			exp = (mexp) / (l_list[r_ptr - r_info].pkills + 1);
		}
	}
	else
	{
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			exp = mexp;
		}
		else
		{
			exp = (mexp) / (l_list[r_ptr - r_info].psights + 1);
		}
	}
			
	return (exp);
}


/*
 * Decrease a monster's hit points, handle monster death.
 *
 * We return TRUE if the monster has been killed (and deleted).
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
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	/* Redraw (later) if needed */
	if (p_ptr->health_who == m_idx) p_ptr->redraw |= (PR_HEALTHBAR);

	/* Hurt it */
	m_ptr->hp -= dam;

	/* It is dead now */
	if (m_ptr->hp <= 0)
	{
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
			if ((who < 0) && (distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) == 1))
				message_format(MSG_KILL, m_ptr->r_idx, "You have killed %s.", m_name);
			//else			message_format(MSG_KILL, m_ptr->r_idx, "%^s has been killed.", m_name);
		}

		/* Death by Physical attack -- non-living monster */
		else if (monster_nonliving(r_ptr))
		{
			// special message for deathblades
			if (r_ptr->d_char == '|')
			{
				if (who < 0)	message_format(MSG_KILL, m_ptr->r_idx, "You have subdued %s.", m_name);
				else			message_format(MSG_KILL, m_ptr->r_idx, "%^s has been subdued.", m_name);
			}
			else
			{
				if (who < 0)	message_format(MSG_KILL, m_ptr->r_idx, "You have destroyed %s.", m_name);
				else			message_format(MSG_KILL, m_ptr->r_idx, "%^s has been destroyed.", m_name);
			}
		}

		/* Death by Physical attack -- living monster */
		else
		{
			if (who < 0)	message_format(MSG_KILL, m_ptr->r_idx, "You have slain %s.", m_name);
			else			message_format(MSG_KILL, m_ptr->r_idx, "%^s has been slain.", m_name);
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
		return (TRUE);
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
	if (dam > 0) m_ptr->min_range = 0;

	/* Not dead yet */
	return (FALSE);
}


/*
 * Modify the current panel to the given coordinates, adjusting only to
 * ensure the coordinates are legal, and return TRUE if anything done.
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
	if (p_ptr->cur_map_hgt < SCREEN_HGT) wy = 0;
	else if (wy > p_ptr->cur_map_hgt - SCREEN_HGT) wy = p_ptr->cur_map_hgt - SCREEN_HGT;
	if (wy < 0) wy = 0;

	/* Verify wx, adjust if needed */
	if (p_ptr->cur_map_wid < SCREEN_WID) wx = 0;
	else if (wx > p_ptr->cur_map_wid - SCREEN_WID) wx = p_ptr->cur_map_wid - SCREEN_WID;
	if (wx < 0) wx = 0;

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
		return (TRUE);
	}

	/* No change */
	return (FALSE);
}


/*
 * Perform the minimum "whole panel" adjustment to ensure that the given
 * location is contained inside the current panel, and return TRUE if any
 * such adjustment was performed.
 */
bool adjust_panel(int y, int x)
{
	int wy = p_ptr->wy;
	int wx = p_ptr->wx;

	/* Adjust as needed */
	while (y >= wy + SCREEN_HGT) wy += SCREEN_HGT;
	while (y < wy) wy -= SCREEN_HGT;

	/* Adjust as needed */
	while (x >= wx + SCREEN_WID) wx += SCREEN_WID;
	while (x < wx) wx -= SCREEN_WID;

	/* Use "modify_panel" */
	return (modify_panel(wy, wx));
}


/*
 * Change the current panel to the panel lying in the given direction.
 *
 * Return TRUE if the panel was changed.
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

	int wy = p_ptr->wy;
	int wx = p_ptr->wx;


	/* Scroll screen vertically when off-center */
	if (center_player && (!p_ptr->running || !run_avoid_center) &&
	    (py != wy + SCREEN_HGT / 2))
	{
		wy = py - SCREEN_HGT / 2;
	}


	// Sil-y: make this an option
	//by default it is 2 for vertical and 4 for hor
	//needs to be programmed better
	//this doesn't do quite what it says on bigscreen
	//it can end up assymmetric up/down or l/r due to panels
	
	
	/* Scroll screen vertically when 2 grids from top/bottom edge */
	else if ((py < wy + 13) || (py >= wy + SCREEN_HGT - 13))
	{
		wy = ((py - PANEL_HGT / 2) / PANEL_HGT) * PANEL_HGT;
	}


	/* Scroll screen horizontally when off-center */
	if (center_player && (!p_ptr->running || !run_avoid_center) &&
	    (px != wx + SCREEN_WID / 2))
	{
		wx = px - SCREEN_WID / 2;
	}

	/* Scroll screen horizontally when 4 grids from left/right edge */
	else if ((px < wx + 17) || (px >= wx + SCREEN_WID - 17))
	{
		wx = ((px - PANEL_WID / 2) / PANEL_WID) * PANEL_WID;
	}


	/* Scroll if needed */
	if (modify_panel(wy, wx))
	{
		/* Optional disturb on "panel change" */
		if (!center_player) disturb(0, 0);
	}
}



/*
 * Monster health description.
 */
static void look_mon_desc(char *buf, size_t max, int m_idx)
{
	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	bool living = TRUE;

	/* Determine if the monster is "living" (vs "undead") */
	if (monster_nonliving(r_ptr)) living = FALSE;

	/* Healthy monsters */
	//if (m_ptr->hp >= m_ptr->maxhp)
	//{
	//	/* No damage */
	//	my_strcpy(buf, (living ? "unhurt" : "undamaged"), max);
	//}
	//else
	//{
	//	/* Calculate a health "percentage" */
	//	int perc = 100L * m_ptr->hp / m_ptr->maxhp;

	//	if (perc >= 75)
	//		my_strcpy(buf, (living ? "somewhat wounded" : "somewhat damaged"), max);
	//	else if (perc >= 50)
	//		my_strcpy(buf, (living ? "wounded" : "damaged"), max);
	//	else if (perc >= 25)
	//		my_strcpy(buf, (living ? "badly wounded" : "badly damaged"), max);
	//	else
	//		my_strcpy(buf, (living ? "almost dead" : "almost destroyed"), max);
	//}
	
	// start the string empty
	my_strcpy(buf, "(", max);
	
	if (p_ptr->wizard)
	{
		if (m_ptr->alertness < ALERTNESS_UNWARY)          my_strcat(buf, format("asleep (%d), ", m_ptr->alertness), max);
		else if (m_ptr->alertness < ALERTNESS_ALERT)      my_strcat(buf, format("unwary (%d), ", m_ptr->alertness), max);
		else											  my_strcat(buf, format("alert (%d), ", m_ptr->alertness), max);
	}
	
	//else
	//{
	//	if (m_ptr->alertness < ALERTNESS_UNWARY)          my_strcat(buf, ", asleep", max);
	//	else if (m_ptr->alertness < ALERTNESS_ALERT)      my_strcat(buf, ", unwary", max);
	//}
	//if (!(r_ptr->flags2 & (RF2_MINDLESS)) && (m_ptr->alertness >= ALERTNESS_ALERT))
	//{
	//	if (m_ptr->stance == STANCE_FLEEING)          my_strcat(buf, format(", afraid (%d)",m_ptr->morale), max);
	//	else if (m_ptr->stance == STANCE_CONFIDENT)   my_strcat(buf, format(", confident (%d)",m_ptr->morale), max);
	//	else if (m_ptr->stance == STANCE_AGGRESSIVE)  my_strcat(buf, format(", aggressive (%d)",m_ptr->morale), max);
	//}
	//else if (m_ptr->stance == STANCE_FLEEING) my_strcat(buf, ", afraid", max);
	
	if (m_ptr->confused) my_strcat(buf, "confused, ", max);
	if (m_ptr->stunned) my_strcat(buf, "stunned, ", max);
	if ((m_ptr->slowed) && (!m_ptr->hasted)) my_strcat(buf, "slowed, ", max);
	if ((!m_ptr->slowed) && (m_ptr->hasted)) my_strcat(buf, "hasted, ", max);
	
	// If nothing is going to be written, wipe the string
	if (strlen(buf) == 1)
	{
		buf[0] = '\0';
	}
	// Otherwise finish it
	else
	{
		// trim the final ", " first
		buf[strlen(buf)-2] = '\0';
		my_strcat(buf, ") ", max);
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
void ang_sort_aux(void *u, void *v, int p, int q)
{
	int z, a, b;

	/* Done sort */
	if (p >= q) return;

	/* Pivot */
	z = p;

	/* Begin */
	a = p;
	b = q;

	/* Partition */
	while (TRUE)
	{
		/* Slide i2 */
		while (!(*ang_sort_comp)(u, v, b, z)) b--;

		/* Slide i1 */
		while (!(*ang_sort_comp)(u, v, z, a)) a++;

		/* Done partition */
		if (a >= b) break;

		/* Swap */
		(*ang_sort_swap)(u, v, a, b);

		/* Advance */
		a++, b--;
	}

	/* Recurse left side */
	ang_sort_aux(u, v, p, b);

	/* Recurse right side */
	ang_sort_aux(u, v, b+1, q);
}


/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort(void *u, void *v, int n)
{
	/* Sort the array */
	ang_sort_aux(u, v, 0, n-1);
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
	if ((y1 == y2) && (x1 == x2)) return (5);

	/* South or North */
	if (x1 == x2) return ((y1 < y2) ? 2 : 8);

	/* East or West */
	if (y1 == y2) return ((x1 < x2) ? 6 : 4);

	/* South-east or South-west */
	if (y1 < y2) return ((x1 < x2) ? 3 : 1);

	/* North-east or North-west */
	if (y1 > y2) return ((x1 < x2) ? 9 : 7);

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
		if (ch == 'z')	d = 5;

		else
		{
			// Determine the keyset
			if (!hjkl_movement && !angband_keyset)		mode = KEYMAP_MODE_SIL;
			else if (hjkl_movement && !angband_keyset)	mode = KEYMAP_MODE_SIL_HJKL;
			else if (!hjkl_movement && angband_keyset)	mode = KEYMAP_MODE_ANGBAND;
			else										mode = KEYMAP_MODE_ANGBAND_HJKL;
			
			/* Extract the action (if any) */
			act = keymap_act[mode][(byte)(ch)];
			
			/* Analyze */
			if (act)
			{
				/* Convert to a direction */
				for (s = act; *s; ++s)
				{
					/* Use any digits in keymap */
					if (isdigit((unsigned char)*s)) d = D2I(*s);
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
	monster_type *m_ptr;

	/* No monster */
	if (m_idx <= 0) return (FALSE);

	/* Get monster */
	m_ptr = &mon_list[m_idx];

	/* Monster must be alive */
	if (!m_ptr->r_idx) return (FALSE);

	/* Monster must be visible */
	if (!m_ptr->ml) return (FALSE);

	/* Monster must be projectable */
	if (!player_can_fire_bold(m_ptr->fy, m_ptr->fx)) return (FALSE);

	/* Hack -- no targeting hallucinations */
	if (p_ptr->image) return (FALSE);

	/* Hack -- no targeting things out of sight when raging */
	if (!(p_ptr->is_dead) && (p_ptr->rage) && !(cave_info[m_ptr->fy][m_ptr->fx] & (CAVE_SEEN))) return (FALSE);

	/* Hack -- Never target trappers XXX XXX XXX */
	/* if (CLEAR_ATTR && (CLEAR_CHAR)) return (FALSE); */

	/* Assume okay */
	return (TRUE);
}




/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * We return TRUE if the target is "okay" and FALSE otherwise.
 */
bool target_okay(int range)
{
	/* No target */
	if (!p_ptr->target_set) return (FALSE);

	/* Accept some "location" targets */
	if (p_ptr->target_who == 0)
    {
        // reject things beyond range
        if ((range > 0) && (distance(p_ptr->py, p_ptr->px, p_ptr->target_row, p_ptr->target_col) > range))  return (FALSE);
        
        // accept things in LOF
        if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_FIRE))          return (TRUE);
        
        // accept walls (for horn of blasting stuff)
        else if (cave_info[p_ptr->target_row][p_ptr->target_col] & (CAVE_WALL))     return (TRUE);
        
        // reject others
        else                                                                        return (FALSE);
    }

	/* Check "monster" targets */
	if (p_ptr->target_who > 0)
	{
		int m_idx = p_ptr->target_who;

		/* Accept reasonable targets */
		if (target_able(m_idx))
		{
			monster_type *m_ptr = &mon_list[m_idx];

			/* Get the monster location */
			p_ptr->target_row = m_ptr->fy;
			p_ptr->target_col = m_ptr->fx;

            // reject things beyond range
            if ((range > 0) && (distance(p_ptr->py, p_ptr->px, p_ptr->target_row, p_ptr->target_col) > range))  return (FALSE);
            
			/* Good target */
			return (TRUE);
		}
	}

	/* Assume no target */
	return (FALSE);
}

/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * Very similar to target_okay, but does not require projectibility, just line of sight
 *
 * We return TRUE if the target is "okay" and FALSE otherwise.
 */
bool target_sighted(void)
{
	/* No target */
	if (!p_ptr->target_set) return (FALSE);
	
	/* Accept "location" targets */
	if (p_ptr->target_who == 0) return (TRUE);
	
	/* Check "monster" targets */
	if (p_ptr->target_who > 0)
	{
		int m_idx = p_ptr->target_who;
		monster_type *m_ptr = &mon_list[m_idx];
		
		/* Accept reasonable targets */
		if (player_can_see_bold(m_ptr->fy, m_ptr->fx) && m_ptr->ml)
		{
			
			/* Get the monster location */
			p_ptr->target_row = m_ptr->fy;
			p_ptr->target_col = m_ptr->fx;
			
			/* Good target */
			return (TRUE);
		}
	}
	
	/* Assume no target */
	return (FALSE);
}



/*
 * Set the target to a monster (or nobody)
 */
void target_set_monster(int m_idx)
{
	/* Acceptable target */
	if ((m_idx > 0) && target_able(m_idx))
	{
		monster_type *m_ptr = &mon_list[m_idx];

		/* Save target info */
		p_ptr->target_set = TRUE;
		p_ptr->target_who = m_idx;
		p_ptr->target_row = m_ptr->fy;
		p_ptr->target_col = m_ptr->fx;
	}

	/* Clear target */
	else
	{
		/* Reset target info */
		p_ptr->target_set = FALSE;
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
		p_ptr->target_set = TRUE;
		p_ptr->target_who = 0;
		p_ptr->target_row = y;
		p_ptr->target_col = x;
	}

	/* Clear target */
	else
	{
		/* Reset target info */
		p_ptr->target_set = FALSE;
		p_ptr->target_who = 0;
		p_ptr->target_row = 0;
		p_ptr->target_col = 0;
	}
}


/*
 * Sorting hook -- comp function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by double-distance to the player.
 */
static bool ang_sort_comp_distance(const void *u, const void *v, int a, int b)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	byte *x = (byte*)(u);
	byte *y = (byte*)(v);

	int da, db, kx, ky;

	/* Absolute distance components */
	kx = x[a]; kx -= px; kx = ABS(kx);
	ky = y[a]; ky -= py; ky = ABS(ky);

	/* Approximate Double Distance to the first point */
	da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

	/* Absolute distance components */
	kx = x[b]; kx -= px; kx = ABS(kx);
	ky = y[b]; ky -= py; ky = ABS(ky);

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
static void ang_sort_swap_distance(void *u, void *v, int a, int b)
{
	byte *x = (byte*)(u);
	byte *y = (byte*)(v);

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
		if (dx && (x3 * dx <= 0)) continue;
		if (dy && (y3 * dy <= 0)) continue;

		/* Absolute distance */
		x4 = ABS(x3);
		y4 = ABS(y3);

		/* Verify quadrant */
		if (dy && !dx && (x4 > y4)) continue;
		if (dx && !dy && (y4 > x4)) continue;

		/* Approximate Double Distance */
		v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));

		/* Penalize location XXX XXX XXX */

		/* Track best */
		if ((b_i >= 0) && (v >= b_v)) continue;

		/* Track best */
		b_i = i; b_v = v;
	}

	/* Result */
	return (b_i);
}


/*
 * Hack -- determine if a given location is "interesting"
 */
static bool target_set_interactive_accept(int y, int x)
{
	object_type *o_ptr;


	/* Player grids are always interesting */
	if (cave_m_idx[y][x] < 0) return (TRUE);

	/* Handle hallucination */
	if (p_ptr->image) return (FALSE);

	/* No looking at things out of sight when raging */
	if (!(p_ptr->is_dead) && (p_ptr->rage) && !(cave_info[y][x] & (CAVE_SEEN))) return (FALSE);

	/* Visible monsters */
	if (cave_m_idx[y][x] > 0)
	{
		monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];

		/* Visible monsters */
		if (m_ptr->ml) return (TRUE);
	}

	/* Objects (only shown when on floors, not when in rubble) */
	if (cave_floorlike_bold(y,x))
	{
		/* Scan all objects in the grid */
		for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
		{
			/* Memorized object */
			if (o_ptr->marked) return (TRUE);
		}
	}

	/* Interesting memorized features */
	if (cave_info[y][x] & (CAVE_MARK))
	{
		/* Notice chasms */
		if (cave_feat[y][x] == FEAT_CHASM) return (TRUE);
        
		/* Notice glyphs */
		if (cave_feat[y][x] == FEAT_GLYPH) return (TRUE);

		/* Notice forges */
		if (cave_forge_bold(y,x)) return (TRUE);

		/* Notice doors */
		if (cave_feat[y][x] == FEAT_OPEN) return (TRUE);
		if (cave_feat[y][x] == FEAT_BROKEN) return (TRUE);

		/* Notice stairs */
		if (cave_stair_bold(y,x)) return (TRUE);

		/* Notice traps */
		if (cave_trap_bold(y,x) && !cave_floorlike_bold(y,x)) return (TRUE);

		/* Notice doors */
		if (cave_known_closed_door_bold(y,x)) return (TRUE);

		/* Notice rubble */
		if (cave_feat[y][x] == FEAT_RUBBLE) return (TRUE);
	}

	/* Nope */
	return (FALSE);
}


/*
 * Prepare the "temp" array for "target_interactive_set"
 *
 * Return the number of target_able monsters in the set.
 */
void target_set_interactive_prepare(int mode, int range)
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
			if (!in_bounds_fully(y, x)) continue;

			// Previously required LOS, but this is now ignored...

			/* Require "interesting" contents */
			if (!target_set_interactive_accept(y, x)) continue;

			/* Special mode */
			if (mode & (TARGET_KILL))
			{
				/* Must contain a monster */
				if (!(cave_m_idx[y][x] > 0)) continue;

				/* Must be a targetable monster */
			 	if (!target_able(cave_m_idx[y][x])) continue;
				
				// possibly restrict the distance from the player
				if ((range > 0) && (distance(p_ptr->py, p_ptr->px, y, x) > range)) continue;
			}

			/* Save the location */
			temp_x[temp_n] = x;
			temp_y[temp_n] = y;
			temp_n++;
		}
	}

	/* Set the sort hooks */
	ang_sort_comp = ang_sort_comp_distance;
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
static int target_set_interactive_aux(int y, int x, int mode, cptr info)
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
		boring = TRUE;

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

			prt(out_val, 0, 0);
			move_cursor_relative(y, x);
			query = inkey();

			/* Stop on everything but "return" */
			if ((query != '\n') && (query != '\r')) break;

			/* Repeat forever */
			continue;
		}


		/* Actual monsters */
		if (cave_m_idx[y][x] > 0)
		{
			monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];
			monster_race *r_ptr = &r_info[m_ptr->r_idx];

			/* Visible */
			if (m_ptr->ml)
			{
				bool recall = FALSE;

				char m_name[80];
                
                bool show_more = FALSE;
				
				/* Not boring */
				boring = FALSE;

                if (p_ptr->rage)
                {
                    my_strcpy(m_name, "an enemy", sizeof(m_name));
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
						screen_roff(m_ptr->r_idx);

						/* Hack -- Complete the prompt (again) */
						Term_addstr(-1, TERM_WHITE, format("  [(r)ecall, %s]", info));

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
                        if ((cave_info[y][x] & (CAVE_SEEN)) && m_ptr->hold_o_idx)
                        {
                            show_more = TRUE;
                        }
                        
                        // known objects on the floor
                        else if (cave_floorlike_bold(y,x) && cave_o_idx[y][x] && (&o_list[cave_o_idx[y][x]])->marked)
                        {
                            show_more = TRUE;
                        }
                        
                        // standing in a known unusual terrain such as wall or door
                        else if (!cave_floorlike_bold(y,x) && (cave_info[y][x] & (CAVE_MARK)))
                        {
                            show_more = TRUE;
                        }
                        
                        if (show_more)
                        {
                            strnfmt(more, 8, "-more- ");
                        }

                        /* Describe, and prompt for recall */
                        if (p_ptr->wizard)
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s] (%d:%d)",
                                s1, s2, s3, m_name, buf, more, info, y, x);
                        }
                        
                        else
                        {
                            strnfmt(out_val, sizeof(out_val),
                                "%s%s%s%s %s%s [(r)ecall, %s]",
                                s1, s2, s3, m_name, buf, more, info);
                        }

						prt(out_val, 0, 0);

						/* Place cursor */
						move_cursor_relative(y, x);

						/* Command */
						query = inkey();
					}

					/* Normal commands */
					if (query != 'r') break;

					/* Toggle recall */
					recall = !recall;
				}

				/* Stop on everything but "return"/"space" */
				if ((query != '\n') && (query != '\r') && (query != ' ')) break;

				/* Sometimes stop at "space" key */
				if ((query == ' ') && !(mode & (TARGET_LOOK))) break;
                
				/* Stop if not asked to continue */
                if (!show_more) break;
                
				/* Change the intro */
				s1 = "It is ";

				/* Hack -- take account of gender */
				if (r_ptr->flags1 & (RF1_FEMALE)) s1 = "She is ";
				else if (r_ptr->flags1 & (RF1_MALE)) s1 = "He is ";

				/* Use a preposition */
				s2 = "carrying ";

				/* Scan all objects being carried */
				for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
				{
					char o_name[80];

					object_type *o_ptr;

					/* Get the object */
					o_ptr = &o_list[this_o_idx];

					/* Get the next object */
					next_o_idx = o_ptr->next_o_idx;

					/*Don't let the player see certain objects (used for vault treasure)*/
					if ((o_ptr->ident & (IDENT_HIDE_CARRY)) && (!p_ptr->wizard) &&
						(!cheat_peek))	 continue;

					/* Obtain an object description */
					object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

					/* Describe the object */
					if (p_ptr->wizard)
					{
						strnfmt(out_val, sizeof(out_val),
					        "%s%s%s%s %s [%s] (%d:%d)",
					        s1, s2, s3, o_name, more, info, y, x);
					}
					else
					{
						strnfmt(out_val, sizeof(out_val),
					        "%s%s%s%s %s [%s]", s1, s2, s3, o_name, more, info);
					}

					prt(out_val, 0, 0);
					move_cursor_relative(y, x);
					query = inkey();

					/* Stop on everything but "return"/"space" */
					if ((query != '\n') && (query != '\r') && (query != ' ')) break;

					/* Sometimes stop at "space" key */
					if ((query == ' ') && !(mode & (TARGET_LOOK))) break;

					/* Change the intro */
					s2 = "also carrying ";
				}

				/* Double break */
				if (this_o_idx) break;

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
		floored = FALSE;


		/* Scan all objects in the grid */
		for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
		{
			object_type *o_ptr;

			/* Get the object */
			o_ptr = &o_list[this_o_idx];

			/* Get the next object */
			next_o_idx = o_ptr->next_o_idx;

			/* Skip objects if floored */
			if (floored) continue;

			/* Objects (only shown when on floors, not when in rubble) */
			if (cave_floorlike_bold(y,x))
			{
				
				/* Describe it */
				if (o_ptr->marked)
				{
					char o_name[80];

					/* Not boring */
					boring = FALSE;

					/* Obtain an object description */
					object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

					/* Describe the object */
					if (p_ptr->wizard)
					{
						strnfmt(out_val, sizeof(out_val),
								"%s%s%s%s %s [%s] (%d:%d)",
								s1, s2, s3, o_name, more, info, y, x);
					}
					else
					{
						strnfmt(out_val, sizeof(out_val),
								"%s%s%s%s %s [%s]", s1, s2, s3, o_name, more, info);
					}

					prt(out_val, 0, 0);
					move_cursor_relative(y, x);
					query = inkey();

					/* Stop on everything but "return"/"space" */
					if ((query != '\n') && (query != '\r') && (query != ' ')) break;

					/* Sometimes stop at "space" key */
					if ((query == ' ') && !(mode & (TARGET_LOOK))) break;

					/* Change the intro */
					s1 = "It is ";

					/* Plurals */
					if (o_ptr->number != 1) s1 = "They are ";

					/* Preposition */
					s2 = "on ";
				}
			}
		}

		/* Double break */
		if (this_o_idx) break;


		/* Feature (apply "mimic") */
		feat = f_info[cave_feat[y][x]].mimic;

		/* Require knowledge about grid, or ability to see grid */
		if (!(cave_info[y][x] & (CAVE_MARK)) && !player_can_see_bold(y,x) && (distance(p_ptr->py, p_ptr->px, y, x) > 0))
		{
			/* Forget feature */
			feat = FEAT_NONE;
		}

		/* Terrain feature if needed */
		if (boring || !cave_floorlike_bold(y,x))
		{
			cptr name = f_name + f_info[feat].name;

			/* Hack -- handle unknown grids */
			if (feat == FEAT_NONE) name = "unknown square";

			/* Pick a prefix */
			if (*s2 && (feat >= FEAT_DOOR_HEAD)) s2 = "in ";

			/* Use the definite article for the unique forge */
			if ((feat >= FEAT_FORGE_UNIQUE_HEAD) && (feat <= FEAT_FORGE_UNIQUE_TAIL))
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
						"%s%s%s%s (%d) %s [%s] (%d:%d)", s1, s2, s3, name, cave_feat[y][x], more, info, y, x);
			}
			else
			{
				strnfmt(out_val, sizeof(out_val),
						"%s%s%s%s %s [%s]", s1, s2, s3, name, more, info);
			}

			prt(out_val, 0, 0);
			move_cursor_relative(y, x);
			query = inkey();

			/* Stop on everything but "return"/"space" */
			if ((query != '\n') && (query != '\r') && (query != ' ')) break;
		}

		/* Stop on everything but "return" */
		if ((query != '\n') && (query != '\r')) break;
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
static int draw_path(u16b *path, int range, char *c, byte *a,
    int y1, int x1, int y2, int x2)
{
	int i;
	int max;
	bool on_screen;

	/* Find the path. */
	max = project_path(path, range, y1, x1, &y2, &x2, (PROJECT_THRU | PROJECT_INVISIPASS));

	/* No path, so do nothing. */
	if (max < 1) return 0;

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
		 		 
		if (panel_contains(y,x)) on_screen = TRUE;
		else if (on_screen) break;
		else continue;

	 	/* Find the position on-screen */
		move_cursor_relative(y,x);

		/* This square is being overwritten, so save the original. */
		Term_what(Term->scr->cx, Term->scr->cy, a+i, c+i);

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
		else if (!cave_floor_bold(y,x) && (cave_info[y][x] & (CAVE_MARK) ||
					player_can_see_bold(y,x)))
		{
			colour = TERM_BLUE;
		}
		/* Unknown squares are grey. */
		else if (!(cave_info[y][x] & (CAVE_MARK)) && !player_can_see_bold(y,x))
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
static void load_path(int max, u16b *path, char *c, byte *a)
{
	int i;
	for (i = 0; i < max; i++)
	{
		if (!panel_contains(GRID_Y(path[i]), GRID_X(path[i]))) continue;

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

	bool done = FALSE;

	bool flag = TRUE;
	
	bool valid_target;
	
	bool new_target = FALSE;

	char query;

	char info[80];
	
	/* These are used for displaying the path to the target */
	u16b path[MAX_RANGE];
	char path_char[MAX_RANGE];
	byte path_attr[MAX_RANGE];
	int max;

	bool wiz = mode & (TARGET_WIZ);

	// turn off auto if doing wizard mode dungeon modification
	if (wiz) flag = FALSE;

	if (range == 0) adjusted_range = MAX_RANGE;
	else			adjusted_range = range;

	
	/* Prepare the "temp" array */
	target_set_interactive_prepare(mode, range);

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
			
			max = project_path(path, adjusted_range, py, px, &y2, &x2, (PROJECT_THRU | PROJECT_INVISIPASS));

			/* Draw the path in "target" mode. If there is one */
			if (mode & (TARGET_KILL))
				(void) draw_path(path, adjusted_range, path_char, path_attr, py, px, y, x);
								
			// Check whether the target location is valid (ie within the path)
			if ((max == 0) ||
				((((GRID_Y(path[max-1]) <= y) && (y <= py)) || ((GRID_Y(path[max-1]) >= y) && (y >= py))) &&
			     (((GRID_X(path[max-1]) <= x) && (x <= px)) || ((GRID_X(path[max-1]) >= x) && (x >= px)))))
			{
				valid_target = TRUE;
			}
			else
			{
				valid_target = FALSE;
			}
			
			// prepare the relevant prompt
			if (valid_target)
			{
				my_strcpy(info, "(t)arget, (m)anual, <dir>", sizeof(info));
			}
			else
			{
				my_strcpy(info, "(m)anual, <dir>", sizeof(info));
			}

			/* Describe and Prompt */
			query = target_set_interactive_aux(y, x, mode, info);

			/* Remove the path */
			if (mode & (TARGET_KILL))	load_path (max, path, path_char, path_attr);

			/* Assume no "direction" */
			d = 0;

			/* Analyze */
			switch (query)
			{
				case ESCAPE:
				case 'q':
				{
					done = TRUE;
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
				}

				case 'm':
				{
					flag = FALSE;
					break;
				}

				case 't':
				case '5':
				case 'z':
				{
					int m_idx = cave_m_idx[y][x];

					if ((p_ptr->py == y) && (p_ptr->px == x))
					{
						done = TRUE;
					}
					else if ((m_idx > 0) && target_able(m_idx))
					{
						health_track(m_idx);
						target_set_monster(m_idx);
						new_target = TRUE;
						done = TRUE;
					}
					else if (valid_target)
					{
						target_set_location(y, x);
						health_track(0);
						new_target = TRUE;
						done = TRUE;
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
					if (!d) bell("Illegal command for target mode!");

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
						target_set_interactive_prepare(mode, range);

						/* Find a new monster */
						i = target_pick(old_y, old_x, ddy[d], ddx[d]);

						/* Restore panel if needed */
						if ((i < 0) && modify_panel(old_wy, old_wx))
						{
							/* Recalculate interesting grids */
							target_set_interactive_prepare(mode, range);
						}

						/* Handle stuff */
						handle_stuff();
					}
				}

				/* Use interesting grid if found */
				if (i >= 0) m = i;
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
			max = project_path(path, adjusted_range, py, px, &y2, &x2, (PROJECT_THRU | PROJECT_INVISIPASS));
			
			/* Draw the path in "target" mode. If there is one */
			if (mode & (TARGET_KILL))
				(void) draw_path(path, adjusted_range, path_char, path_attr, py, px, y, x);

			// Check whether the target location is valid (ie within the path)
			if ((max == 0) ||
				((((GRID_Y(path[max-1]) <= y) && (y <= py)) || ((GRID_Y(path[max-1]) >= y) && (y >= py))) &&
			     (((GRID_X(path[max-1]) <= x) && (x <= px)) || ((GRID_X(path[max-1]) >= x) && (x >= px)))))
			{
				valid_target = TRUE;
			}
			else
			{
				valid_target = FALSE;
			}
			
			// prepare the relevant prompt
			if (valid_target || p_ptr->wizard)
			{
				my_strcpy(info, "(t)arget, (a)uto, <dir>", sizeof(info));
			}
			else
			{
				my_strcpy(info, "(a)uto, <dir>", sizeof(info));
			}
			
			/* Describe and Prompt (enable "TARGET_LOOK") */
			query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info);

			/* Remove the path */
			if (mode & (TARGET_KILL))  load_path (max, path, path_char, path_attr);

			/* Assume no direction */
			d = 0;

			/* Analyze the keypress */
			switch (query)
			{
				case ESCAPE:
				case 'q':
				{
					done = TRUE;
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
				}

				case 'a':
				{
					flag = TRUE;

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
					if (bd == 999) flag = FALSE;

					break;
				}

				case 't':
				case '5':
				case 'z':
				{
					if ((p_ptr->py == y) && (p_ptr->px == x))
					{
						done = TRUE;
					}
					else if (valid_target || p_ptr->wizard)
					{
						target_set_location(y, x);
						health_track(0);
						new_target = TRUE;
						done = TRUE;
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
					if (!d) bell("Illegal command for target mode!");

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
				if (x >= p_ptr->cur_map_wid - 1) x--;
				else if (x <= 0) x++;

				/* Slide into legality */
				if (y >= p_ptr->cur_map_hgt - 1) y--;
				else if (y <= 0) y++;

				/* Adjust panel if needed */
				if (adjust_panel(y, x))
				{
					/* Handle stuff */
					handle_stuff();

					/* Recalculate interesting grids */
					target_set_interactive_prepare(mode, range);
				}
			}
		}

		/* Wizard dungeon modification */
		else
		{
			bool inc_monster = FALSE;
			bool inc_object = FALSE;
			bool inc_terrain = FALSE;
			bool reroll_monster = FALSE;
			bool reroll_object = FALSE;
			bool found = FALSE;
			
			y2 = y;
			x2 = x;
			
			// prepare the relevant prompt
			my_strcpy(info, "<space>, <tab>, <dir>", sizeof(info));
			
			/* Describe and Prompt (enable "TARGET_LOOK") */
			query = target_set_interactive_aux(y, x, mode | TARGET_LOOK, info);
			
			/* Remove the path */
			if (mode & (TARGET_KILL))  load_path (max, path, path_char, path_attr);
			
			/* Assume no direction */
			d = 0;
			
			// space increments (and is handled specially)
			if (query == ' ')
			{
				// increment a monster race
				if (cave_m_idx[y][x])		inc_monster = TRUE;
				// increment an object kind
				else if (cave_o_idx[y][x])	inc_object = TRUE;
				// increment a terrain type
				else						inc_terrain = TRUE;
			}

			// tab rerolls (and is handled specially)
			if (query == '\t')
			{
				// reroll a monster race
				if (cave_m_idx[y][x])		reroll_monster = TRUE;
				// reroll an object kind
				else if (cave_o_idx[y][x])	reroll_object = TRUE;
			}

			// escape exits
			if (query == ESCAPE)
			{
				done = TRUE;
			}

			// backspace changes the light level (and is handled specially)
			else if (query == '\b')
			{
				// toggle the cave_glow value
				if (cave_info[y][x] & (CAVE_GLOW))
				{
					cave_info[y][x] &= ~(CAVE_GLOW);
					if (cave_floorlike_bold(y,x))
					{
						cave_info[y][x] &= ~(CAVE_MARK);
					}
				}
				else
				{
					cave_info[y][x] |=  (CAVE_GLOW);
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
			else if (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&@", query) || inc_monster || reroll_monster)
			{
				monster_race *r_ptr;
				monster_race *old_r_ptr;
				monster_type *m_ptr;
				
				// recreate a monster of the same type.
				if (reroll_monster)
				{
					m_ptr = &mon_list[cave_m_idx[y][x]];
					i = m_ptr->r_idx;
					found = TRUE;
				}
				
				// go through monster race list and find next monster with that symbol.
				else if (inc_monster)
				{
					m_ptr = &mon_list[cave_m_idx[y][x]];
					old_r_ptr = &r_info[m_ptr->r_idx];
					
					for (i = 1; i < z_info->r_max; i++)
					{
						r_ptr = &r_info[(i+m_ptr->r_idx) % z_info->r_max];
						
						// stop when you find one
						if ((r_ptr->d_char == old_r_ptr->d_char) && (r_ptr->cur_num < r_ptr->max_num) && (r_ptr->level <= 25))
						{
							found = TRUE;
							i = (i+m_ptr->r_idx) % z_info->r_max;
							break;
						}
					}
				}
				
				// go through monster race list and find first monster with that symbol.
				else
				{
					for (i = 1; i < z_info->r_max; i++)
					{
						r_ptr = &r_info[i];
						
						// stop when you find one
						if (r_ptr->d_char == query)\
						{
							found = TRUE;
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
					place_monster_one(y, x, i, TRUE, TRUE, NULL);
				}
			}

			// create an object
			else if (strchr("([)|/\\]}-~*\"=_?!~,", query) || inc_object || reroll_object)
			{		
				object_kind *old_k_ptr;
				object_type *o_ptr;
				object_kind *k_ptr;
				object_type *i_ptr;
				object_type object_type_body;
				
				// recreate an object of the same type.
				if (reroll_object)
				{
					o_ptr = &o_list[cave_o_idx[y][x]];
					i = o_ptr->k_idx;
					found = TRUE;
				}
				
				// go through object kind list and find next object kind with that symbol.
				else if (inc_object)
				{
					o_ptr = &o_list[cave_o_idx[y][x]];
					old_k_ptr = &k_info[o_ptr->k_idx];
					
					for (i = 1; i < z_info->k_max; i++)
					{
						k_ptr = &k_info[(i+o_ptr->k_idx) % z_info->k_max];
						
						// stop when you find one
						if (k_ptr->d_char == old_k_ptr->d_char)
						{
							found = TRUE;
							i = (i+o_ptr->k_idx) % z_info->k_max;
							break;
						}
					}
				}
				
				// go through object kind list and find first object kind with that symbol.
				else
				{
					for (i = 1; i < z_info->k_max; i++)
					{
						k_ptr = &k_info[i];
						
						// stop when you find one
						if (k_ptr->d_char == query) 
						{
							found = TRUE;
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
					apply_magic(i_ptr, p_ptr->depth, FALSE, FALSE, FALSE, FALSE);
					
					// apply the autoinscription (if any)
					apply_autoinscription(i_ptr);
					
					if (i_ptr->tval == TV_ARROW) i_ptr->number = 24;
					
					/* Drop the object from heaven */
					drop_near(i_ptr, -1, y, x);
				}
			}
						
			// change the terrain
			else if (strchr(".;'^+#:%0<>", query) || inc_terrain)
			{		
				feature_type *f_ptr;
				feature_type *old_f_ptr;
				
				// go through terrain list and find next terrain type with that symbol.
				if (inc_terrain)
				{
					old_f_ptr = &f_info[cave_feat[y][x]];
					
					for (i = 1; i < z_info->f_max; i++)
					{
						f_ptr = &f_info[(i+cave_feat[y][x]) % z_info->f_max];
						
						// stop when you find one
						if (f_ptr->d_char == old_f_ptr->d_char)
						{
							found = TRUE;
							i = (i+cave_feat[y][x]) % z_info->f_max;
							break;
						}
					}
				}

				// go through terrain list and find first terrain type with that symbol.
				else
				{
					for (i = 1; i < z_info->f_max; i++)
					{
						f_ptr = &f_info[i];
						
						// stop when you find one
						if (f_ptr->d_char == query)
						{
							found = TRUE;
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
				if (x >= p_ptr->cur_map_wid - 1) x--;
				else if (x <= 0) x++;
				
				/* Slide into legality */
				if (y >= p_ptr->cur_map_hgt - 1) y--;
				else if (y <= 0) y++;
				
				/* Adjust panel if needed */
				if (adjust_panel(y, x))
				{
					/* Handle stuff */
					handle_stuff();
					
					/* Recalculate interesting grids */
					target_set_interactive_prepare(mode, range);
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
		// if we did not select a new target and were in targetting mode, then abort target
		if (mode & (TARGET_KILL))
		{
			target_set_monster(0);
			health_track(0);
		}
		return (FALSE);
	}

	/* Success */
	return (TRUE);
}

/*
 * Takes a delta coordinates and returns a direction.
 * e.g. (1,0) is south, which is direction 2.
 */
int dir_from_delta(int deltay, int deltax)
{
	s16b dird[3][3] =	{ { 7, 8, 9 },
						  { 4, 5, 6 },
						  { 1, 2, 3 } };
	
	return (dird[deltay+1][deltax+1]);
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
    if (deltay == 0) dy = 0;
    else             dy = (deltay > 0) ? 1 : -1;
    
    if (deltax == 0) dx = 0;
    else             dx = (deltax > 0) ? 1 : -1;
    
    if ((deltax != 0) && (ABS(deltay) / ABS(deltax) >= 2)) dx = 0;
    if ((deltay != 0) && (ABS(deltax) / ABS(deltay) >= 2)) dy = 0;
    
    return (dir_from_delta(dy, dx));
}


/*
 * Get an "aiming direction" (1,2,3,4,6,7,8,9 or 5) from the user.
 *
 * Return TRUE if a direction was chosen, otherwise return FALSE.
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
bool get_aim_dir(int *dp, int range)
{
	int dir;

	char ch;

	cptr p;

#ifdef ALLOW_REPEAT

	if (repeat_pull(dp))
	{
		/* Verify */
		if (!(*dp == 5 && !target_okay(range)))
		{
			return (TRUE);
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

	/* Hack -- auto-target if requested */
//	if (use_old_target && target_okay(range)) dir = 5;

	/* Ask until satisfied */
	while (!dir)
	{
		/* Choose a prompt */
		if (!target_okay(range))
		{
			p = "Direction ('f' for closest, '*' to choose a target, ESC to cancel)? ";
		}
		else
		{
			p = "Direction ('f' for target, '*' to re-target, ESC to cancel)? ";
		}

		/* Get a command (or Cancel) */
		if (!get_com(p, &ch)) break;

		/* Analyze */
		switch (ch)
		{
			/* Set new target, use target if legal */
			case '*':
			{
				if (target_set_interactive(TARGET_KILL, range)) dir = 5;
				break;
			}

			/* Use current target, if set and legal, otherwise pick next target */
			case 'f':
			case 'F':
			case 't':
			case '5':
			case 'z':
			{
				if (target_okay(range)) dir = 5;
				else
				{
					/* Prepare the "temp" array */
					target_set_interactive_prepare(TARGET_KILL, range);
					
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
				break;
			}
		}

		/* Error */
		if (!dir) bell("Illegal aim direction!");
	}

	/* No direction */
	if (!dir) return (FALSE);

	/* Save the direction */
	p_ptr->command_dir = dir;

	/* Check for confusion */
	// Sil-y: Doesn't use the new confusion method, but might be difficult to use it
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
	return (TRUE);
}



/*
 * Request a "movement" direction (1,2,3,4,5,6,7,8,9) from the user.
 *
 * Return TRUE if a direction was chosen, otherwise return FALSE.
 *
 * Direction "0" is illegal and will not be accepted.
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 */
bool get_rep_dir(int *dp)
{
	int dir;

	char ch;

	cptr p;

#ifdef ALLOW_REPEAT

	if (repeat_pull(dp))
	{
		return (TRUE);
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
		if (!get_com(p, &ch)) break;

		/* Convert keypress into a direction */
		dir = target_dir(ch);

		/* Oops */
		if (!dir) bell("Illegal repeatable direction!");
	}

	/* Aborted */
	if (!dir) return (FALSE);

	/* Save desired direction */
	p_ptr->command_dir = dir;

	/* Save direction */
	(*dp) = dir;

#ifdef ALLOW_REPEAT

	repeat_push(dir);

#endif /* ALLOW_REPEAT */

	/* Success */
	return (TRUE);
}


/*
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return TRUE if direction changes.
 */
bool confuse_dir(int *dp)
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
			// gives 3 chances to be turned left and 3 chances to be turned right
			// leads to a binomial distribution of direction around the intended one:
			//
			// 15 20 15
			//  6     6   (chances are all out of 64)
			//  1  0  1
			
			i = damroll(3,2) - damroll(3,2);
			
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
		return (TRUE);
	}

	/* Not confused */
	return (FALSE);
}


const char female_entry_poetry[][100] =
{
	{ "Into the vast and echoing gloom," },
	{ "more dread than many-tunnelled tomb" },
//	{ "in labyrinthine pyramid" },
//	{ "where everlasting death is hid," },
	{ "  down awful corridors that wind" },
	{ "    down to a menace dark enshrined;" },
	{ "      down to the mountain's roots profound," },
	{ "devoured, tormented, bored and ground" },
	{ "by seething vermin spawned of stone;" },
	{ "  down to the depths she went alone..." },
	
	{ "" }
};


const char male_entry_poetry[][100] =
{
	{ "Into the vast and echoing gloom," },
	{ "more dread than many-tunnelled tomb" },
//	{ "in labyrinthine pyramid" },
//	{ "where everlasting death is hid," },
	{ "  down awful corridors that wind" },
	{ "    down to a menace dark enshrined;" },
	{ "      down to the mountain's roots profound," },
	{ "devoured, tormented, bored and ground" },
	{ "by seething vermin spawned of stone;" },
	{ "  down to the depths he went alone..." },
	
	{ "" }
};


const char tutorial_leave_text[][100] =
{
	{ "You have finished the first half of the tutorial and are ready" },
	{ "to create a new character." },
	{ " " },
	{ "Don't let the choices overwhelm you the first time." },
	{ "Just start with the default Race and House, then invest most" },
	{ "of your starting experience in Melee and Evasion." },
	{ "Once the game begins, finding some weapons and armour should" },
	{ "be your top priority." },
	{ " " },
	{ "Remember that a key feature of Sil (and all Roguelike games)" },
	{ "is that you cannot use savepoints: if you die, that's it." },
	{ "It is thus a challenging game where you need to really *think*." },
	{ "You will die many times. When you do: reflect on what to learn" },
	{ "from that death, see if you set a high score, then think about" },
	{ "all the things you want to do differently with the next character..." },
	
	{ "" }
};


const char tutorial_win_text[][100] =
{
	{ "Congratulations. You have survived a fire-drake (usually found" },
	{ "at 900 ft!), and have finished the tutorial in fine form." },
	{ "You are more than ready to create a new character." },
	{ " " },
	{ "Don't let the choices overwhelm you the first time." },
	{ "Just start with the default Race and House, then invest most" },
	{ "of your starting experience in Melee and Evasion." },
	{ "Once the game begins, finding some weapons and armour should" },
	{ "be your top priority." },
	{ " " },
	{ "Remember that a key feature of Sil (and all Roguelike games)" },
	{ "is that you cannot use savepoints: if you die, that's it." },
	{ "It is thus a challenging game where you need to really *think*." },
	{ "You will die many times. When you do: reflect on what to learn" },
	{ "from that death, see if you set a high score, then think about" },
	{ "all the things you want to do differently with the next character..." },
	
	{ "" }
};


const char tutorial_early_death_text[][100] =
{
	{ "You have been slain." },
	{ " " },
	{ "A key feature of Sil (and all Roguelike games) is that you cannot" },
	{ "use savepoints: if you die, that's it!" },
	{ "It is thus a challenging game where you need to really *think*." },
	{ " " },
	{ "However, it is a bit frustrating to die before the end of the" },
	{ "tutorial, so we evidentally made it a bit too deadly." },
	{ " " },
	{ "Just restart the tutorial and you should be back to where you" },
	{ "were in a couple of minutes. Remember that if combat is not going" },
	{ "your way, you can try to escape and heal, then either come back" },
	{ "and again to defeat your adversary, or simply ignore it." },
	
	{ "" }
};


const char tutorial_late_death_text[][100] =
{
	{ "Congratulations: you have finished the tutorial." },
	{ " " },
	{ "You have also just been through a rite of passage: dying." },
	{ "Remember that a key feature of Sil (and all Roguelike games)" },
	{ "is that you cannot use savepoints: if you die, that's it." },
	{ "It is thus a challenging game where you need to really *think*." },
	{ "You will die many times. When you do: reflect on what to learn" },
	{ "from that death, see if you set a high score, then think about" },
	{ "all the things you want to do differently with the next character..." },
	{ " " },
	{ "You are now more than ready to create a character and start playing." },
	{ " " },
	{ "Don't let the choices overwhelm you the first time." },
	{ "Just start with the default Race and House, then invest most" },
	{ "of your starting experience in Melee and Evasion." },
	{ "Once the game begins, finding some weapons and armour should" },
	{ "be your top priority." },
	
	{ "" }
};


const char throne_poetry[][100] =
{
	{ "Loud rose a din of laughter hoarse," },
	{ "  self-loathing yet without remorse;" },
	{ "    loud came a singing harsh and fierce" },
	{ "      like swords of terror souls to pierce." },
	{ "Red was the glare through open doors" },
	{ "  of firelight mirrored on brazen floors," },
	{ "    and up the arches towering clomb" },
	{ "      to glooms unguessed, to vaulted dome" },
	{ "        swathed in wavering smokes and steams" },
	{ "          stabbed with flickering lightning-gleams." },

	{ "" }
};

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

const char ultimate_bug_text[][100] =
{
	{ "Against all hope, you defeated the Dark Enemy," },
	{ "  and destroyed his physical form." },
	{ "    For the rest of this age at least," },
	{ "      Arda shall be free from the tyrant's shadow." },
	{ "But there will be time later for reflection" },
	{ "  on this great change to Arda's fate." },
	{ "    You are buried still in Angband's vaults" },
	{ "      -- make quick your bold escape!" },
	
	{ "" }
};




/*
 * Show the poetry on entering Morgoth's Throne Room
 */
void pause_with_text(const char desc[][100], int row, int col)
{
	char ch;
	int i = 0;
	int j = 0;
	int msec = 50;
	
	/* Save screen */
	screen_save();
	
	/* Clear screen */
	Term_clear();
		
	while (strlen(desc[i]) != 0)
	{
		c_put_str(TERM_WHITE, desc[i], row + i, col);
		Term_xtra(TERM_XTRA_DELAY, msec);
		Term_fresh();
		i++;
	}

	/* Interact until done */
	while (1)
	{
		/* Prompt */
		hide_cursor = TRUE;
		ch = inkey();
		hide_cursor = FALSE;
		
		/* Press any key */
		if (ch != EOF)	break;
		
		/* Flush messages */
		message_flush();
	}	
	
	for (j = row; j <= row; j++)
	{
		c_put_str(TERM_WHITE, "                                                                 ", j, 1);
		Term_xtra(TERM_XTRA_DELAY, msec);
		Term_fresh();
	}

	/* Load screen */
	screen_load();
}





