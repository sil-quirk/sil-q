/* File: spell/spell-visuals.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

void stun_monster(monster_type* m_ptr, int stun)
{
    if (monster_race_is_vala(m_ptr->r_idx))
    {
        monster_clear_vala_state(m_ptr);
        return;
    }

    int new_stun = m_ptr->stunned + stun;
    m_ptr->stunned = MIN(new_stun, 255);
}

/*
 * Return a color to use for the bolt/ball spells
 */
static byte spell_color(int type)
{
    /* Analyze */
    switch (type)
    {
    case GF_ARROW:
        return (TERM_L_UMBER);
    case GF_BOULDER:
        return (TERM_SLATE);
    case GF_ACID:
        return (TERM_SLATE);
    case GF_ELEC:
        return (TERM_BLUE);
    case GF_FIRE:
        return (TERM_RED);
    case GF_COLD:
        return (TERM_WHITE);
    case GF_POIS:
        return (TERM_GREEN);
    case GF_CONFUSION:
        return (TERM_L_UMBER);
    case GF_SOUND:
        return (TERM_L_WHITE);
    case GF_LIGHT:
        return (TERM_WHITE);
    case GF_DARK_WEAK:
        return (TERM_L_DARK);
    case GF_DARK:
        return (TERM_L_DARK);
    case GF_IDENTIFY:
        return (TERM_WHITE);
    case GF_EARTHQUAKE:
        return (TERM_SLATE);
    case GF_WEB:
        return (TERM_L_UMBER);
    }

    /* Standard "color" */
    return (TERM_L_WHITE);
}

/*
 * Find the attr/char pair to use for a spell effect
 *
 * It is moving (or has moved) from (x,y) to (nx,ny).
 *
 * If the distance is not "one", we (may) return "*".
 */
u16b bolt_pict(int y, int x, int ny, int nx, int typ)
{
    int base;

    byte k;

    byte a;
    char c;

    /* No motion (*) */
    if ((ny == y) && (nx == x))
        base = 0x30;

    /* Vertical (|) */
    else if (nx == x)
        base = 0x40;

    /* Horizontal (-) */
    else if (ny == y)
        base = 0x50;

    /* Diagonal (/) */
    else if ((ny - y) == (x - nx))
        base = 0x60;

    /* Diagonal (\) */
    else if ((ny - y) == (nx - x))
        base = 0x70;

    /* Weird (*) */
    else
        base = 0x30;

    if (typ == GF_LIGHT && use_graphics == GRAPHICS_MICROCHASM)
    {
        a = misc_to_attr[ICON_GLOW];
        c = misc_to_char[ICON_GLOW];
    }
    else
    {
        /* Basic spell color */
        k = spell_color(typ);

        /* Obtain attr/char */
        a = misc_to_attr[base + k];
        c = misc_to_char[base + k];
    }

    /* Create pict */
    return (PICT(a, c));
}

