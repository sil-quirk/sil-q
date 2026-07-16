#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "ui/status/status-internal.h"

/*
 *  Represents the different levels of health.
 *  Note that it is a bit odd with fewer health levels in the SOMEWHAT_WOUNDED
 * category. This is due to a rounding off tension between the natural way to do
 * the colours (perfect having its own) and the natural way to do the stars for
 * the health bar (zero having its own). It should be unnoticeable to the
 * player.
 */
int health_level(int current, int max)
{
    int level;

    if (current == max)
    {
        level = HEALTH_UNHURT; // 100%
    }

    else
    {
        switch ((4 * current + max - 1) / max)
        {
        case 4:
            level = HEALTH_SOMEWHAT_WOUNDED;
            break; //  76% - 99%
        case 3:
            level = HEALTH_WOUNDED;
            break; //  51% - 75%
        case 2:
            level = HEALTH_BADLY_WOUNDED;
            break; //  26% - 50%
        case 1:
            level = HEALTH_ALMOST_DEAD;
            break; //   1% - 25%
        default:
            level = HEALTH_DEAD;
            break; //   0%
        }
    }

    return (level);
}

/*
 *  Assigns colours to the health levels.
 */
byte health_attr(int current, int max)
{
    byte a;

    switch (health_level(current, max))
    {
    case HEALTH_UNHURT:
        a = TERM_L_GREEN;
        break; // 100%
    case HEALTH_SOMEWHAT_WOUNDED:
        a = TERM_YELLOW;
        break; //  76% - 99%
    case HEALTH_WOUNDED:
        a = TERM_ORANGE;
        break; //  51% - 75%
    case HEALTH_BADLY_WOUNDED:
        a = TERM_L_RED;
        break; //  26% - 50%
    case HEALTH_ALMOST_DEAD:
        a = TERM_RED;
        break; //   1% - 25%
    default:
        a = TERM_RED;
        break; //   0%
    }

    return (a);
}

/*
 * Gets a text string denoting the alertness level / stance into a buffer, along
 * with the associated colour.
 */
bool get_alertness_text(
    monster_type* m_ptr, int text_size, char* text, int* color)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (m_ptr->alertness < ALERTNESS_UNWARY)
    {
        SDL_strlcpy(text, "Sleeping", text_size);
        *color = TERM_L_BLUE;
    }
    else if (m_ptr->alertness < ALERTNESS_ALERT)
    {
        SDL_strlcpy(text, "Unwary", text_size);
        *color = TERM_L_BLUE;
    }
    else
    {
        if (r_ptr->flags2 & (RF2_MINDLESS))
        {
            SDL_strlcpy(text, "Mindless", text_size);
            *color = TERM_L_DARK;
        }
        else
        {
            char morale_buf[8];

            if (m_ptr->stance == STANCE_FLEEING)
            {
                SDL_strlcpy(text, "Fleeing", text_size);
                *color = TERM_L_RED;
            }
            else if (m_ptr->stance == STANCE_CONFIDENT)
            {
                SDL_strlcpy(text, "Confident", text_size);
                *color = TERM_L_WHITE;
            }
            else if (m_ptr->stance == STANCE_AGGRESSIVE)
            {
                SDL_strlcpy(text, "Aggress", text_size);
                *color = TERM_L_WHITE;
            }

            // sometimes (only in debugging?) we are looking at a monster before
            // it has a stance in this case return false so we don't print the
            // strings
            else
            {
                return false;
            }

            if (m_ptr->morale >= 0)
                sprintf(morale_buf, " %d", (m_ptr->morale + 9) / 10);
            else
                sprintf(morale_buf, " %d", m_ptr->morale / 10);

            strncat(text, morale_buf, text_size - strlen(text));
        }
    }

    return true;
}

/*
 * Redraw the "monster health bar"
 *
 * The "monster health bar" provides visual feedback on the "health"
 * of the monster currently being "tracked".  There are several ways
 * to "track" a monster, including targetting it, attacking it, and
 * affecting it (and nobody else) with a ranged attack.  When nothing
 * is being tracked, we clear the health bar.  If the monster being
 * tracked is not currently visible, a special health bar is shown.
 */
void health_redraw(void)
{
    if (ui_hide_left_panel())
        return;

    /* Not tracking */
    if (!p_ptr->health_who)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking an unseen monster */
    else if (!mon_list[p_ptr->health_who].ml)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a hallucinatory monster */
    else if (p_ptr->image)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a dead monster (?) */
    else if (mon_list[p_ptr->health_who].hp <= 0)
    {
        /* Erase the health bar */
        Term_erase(COL_INFO, ROW_INFO, 12);
        /* Erase the morale bar */
        Term_erase(COL_INFO, ROW_INFO + 1, 12);
    }

    /* Tracking a visible monster */
    else
    {
        int color;
        char buf[20];

        monster_type* m_ptr = &mon_list[p_ptr->health_who];

        /* Afraid */
        // if (m_ptr->stance == STANCE_FLEEING) attr = TERM_VIOLET;

        Term_erase(COL_INFO, ROW_INFO, 12);
        if (monster_health_bar_allowed(m_ptr))
        {
            /* Default to "unknown" */
            Term_putstr(COL_INFO, ROW_INFO, 12, TERM_L_DARK,
                "  --------  ");

            /* Dump the current health (including confusion/stun labels). */
            Term_gotoxy(COL_INFO + 2, ROW_INFO);
            monster_health_bar_put(m_ptr, 8);
        }

        Term_erase(COL_INFO, ROW_INFO + 1, 12);

        if (!get_alertness_text(m_ptr, sizeof(buf), buf, &color))
            return;

        int buf_len = (int)strlen(buf);
        int display_len = MIN(buf_len, LEFT_PANEL_CONTENT_WID);
        int display_col = COL_INFO
            + MAX(0, (LEFT_PANEL_CONTENT_WID - display_len) / 2);

        Term_putstr(display_col, ROW_INFO + 1, display_len, color, buf);
    }
}
