#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "player/player-upkeep-internal.h"

/*
 * Calculate maximum voice.
 *
 * This function induces status messages.
 */
extern void calc_voice(void)
{
    int msp;
    int i;
    int tmp;

    /* Get voice value */
    // 20 + a compounding 20% bonus per point of gra

    tmp = 20 * 100;

    if (p_ptr->stat_use[A_GRA] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_GRA]; i++)
        {
            tmp = tmp * 12 / 10;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_GRA]); i++)
        {
            tmp = tmp * 10 / 12;
        }
    }
    msp = tmp / 100;

    /* New maximum hitpoints */
    if (p_ptr->msp != msp)
    {
        int i = 100;

        /* Get percentage of maximum sp */
        if (p_ptr->msp)
            i = ((100 * p_ptr->csp) / p_ptr->msp);

        /* Save new limit */
        p_ptr->msp = msp;

        /* Update current maximum sp */
        p_ptr->csp = ((i * p_ptr->msp) / 100)
            + (((i * p_ptr->msp) % 100 >= 50) ? 1 : 0);

        /* Hack - any change in max voice resets frac */
        p_ptr->csp_frac = 0;

        /* Display sp later */
        p_ptr->redraw |= (PR_VOICE);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }

    /* Hack -- handle "xtra" mode */
    if (character_xtra)
        return;
}

/*
 * Calculate the player's (maximal) hit points
 *
 * Adjust current hitpoints if necessary
 *
 * Sil - modified substantially to reflect absence of chance and fixed bonus,
 * not per level
 */
void calc_hitpoints(void)
{
    int mhp;
    int i;
    int tmp;

    /* Get hitpoint value */
    // 20 + a compounding 16% bonus per point of con, plus 5 HP flat bonus

    tmp = 20 * 100;
    if (p_ptr->stat_use[A_CON] >= 0)
    {
        for (i = 0; i < p_ptr->stat_use[A_CON]; i++)
        {
            tmp = tmp * 116 / 100;
        }
    }
    else
    {
        for (i = 0; i < -(p_ptr->stat_use[A_CON]); i++)
        {
            tmp = tmp * 100 / 116;
        }
    }
    mhp = tmp / 100 + 5;

    /* New maximum hitpoints */
    if (p_ptr->mhp != mhp)
    {
        int i = 100;

        /* Get percentage of maximum hp */
        if (p_ptr->mhp)
            i = ((100 * p_ptr->chp) / p_ptr->mhp);

        /* Save new limit */
        p_ptr->mhp = mhp;

        /* Update current maximum hp */
        p_ptr->chp = ((i * p_ptr->mhp) / 100)
            + (((i * p_ptr->mhp) % 100 >= 50) ? 1 : 0);

        /* Hack - any change in max hitpoint resets frac */
        p_ptr->chp_frac = 0;

        /* Display hp later */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}
