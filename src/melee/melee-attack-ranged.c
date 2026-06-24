#include "angband.h"
#include "externs.h"
#include "melee/melee-attack.h"

static int ranged_attack_sound(int attack)
{
    switch (attack)
    {
    case 96 + 0:  /* RF4_ARROW1 */
    case 96 + 1:  /* RF4_ARROW2 */
    case 96 + 2:  /* RF4_BOULDER */
    case 96 + 23: /* RF4_THROW_WEB */
        return MSG_MONSTER_ATTACK_RANGED;
    case 96 + 3:  /* RF4_BRTH_FIRE */
    case 96 + 4:  /* RF4_BRTH_COLD */
    case 96 + 5:  /* RF4_BRTH_POIS */
    case 96 + 6:  /* RF4_BRTH_DARK */
        return MSG_MONSTER_ATTACK_BREATH;
    default:
        return -1;
    }
}

/*********************************************************************/
/*                                                                   */
/*                      Monster Ranged Attacks                       */
/*                                                                   */
/*********************************************************************/

/*
 * Gets the number of sides used in the monster attack
 */
int get_sides(int attack)
{
    int sides;

    if (attack >= 128)
        return (false);
    else if (attack >= 96)
    {
        sides = spell_info_RF4[attack - 96][COL_SPELL_SIDES];
    }
    else
        return (false);

    return (sides);
}

/*
 * Cast a bolt at the player
 * Stop if we hit a monster
 * Affect monsters and the player
 */
static void mon_bolt(int m_idx, int typ, int dd, int ds, int dif)
{
    monster_type* m_ptr = &mon_list[m_idx];
    int py = p_ptr->py;
    int px = p_ptr->px;
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    u32b flg = PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY;

    /* Target the player with a bolt attack */
    (void)project(m_idx, 0, fy, fx, py, px, dd, ds, dif, typ, flg, 0, false);
}

/*
 * Cast a beam at the player, sometimes with limited range.
 * Do not stop if we hit a monster
 * Affect grids, monsters, and the player
 */
/*
static void mon_beam(int m_idx, int typ, int dd, int ds, int dif, int range)
{
        monster_type *m_ptr = &mon_list[m_idx];
        int py = p_ptr->py;
        int px = p_ptr->px;
        int fy = m_ptr->fy;
        int fx = m_ptr->fx;

        u32b flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL |
                                PROJECT_PLAY;

        // Target the player with a beam attack
        (void)project(m_idx, range, fy, fx, py, px, dd, ds, dif, typ, flg, 0,
true);
}
*/

/*
 * Release a cloud, which is a ball centered on the monster that does not
 * affect other monsters (mostly to avoid annoying messages).
 *
 */
void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad)
{
    monster_type* m_ptr = &mon_list[m_idx];
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    // u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_PLAY |
    // PROJECT_HIDE;
    u32b flg = PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM | PROJECT_PLAY
        | PROJECT_KILL | PROJECT_HIDE;

    /* Surround the monster with a cloud */
    project(m_idx, rad, fy, fx, fy, fx, dd + 2, ds, dif, typ, flg, 0, 0);
}

/*
 * Breathe or cast an arc-shaped spell at the player.
 * Use an arc spell of specified range and width.
 * Optionally, do not harm monsters with the same r_idx.
 * Affect grids, objects, monsters, and (specifically) the player
 *
 * Monster breaths do not lose strength with distance at the same rate
 * that normal arc spells do.  If the monster is "powerful", they lose
 * less strength; otherwise, they lose more.
 */
static void mon_arc(int m_idx, int typ, bool noharm, int dd, int ds, int dif,
    int rad, int degrees_of_arc)
{
    monster_type* m_ptr = &mon_list[m_idx];

    int py = p_ptr->py;
    int px = p_ptr->px;
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    u32b flg = PROJECT_ARC | PROJECT_BOOM | PROJECT_GRID | PROJECT_ITEM
        | PROJECT_KILL | PROJECT_PLAY;

    /*unused variable*/
    (void)noharm;

    /* Radius of zero means no fixed limit. */
    if (rad == 0)
        rad = MAX_SIGHT;

    /* Target the player with an arc-shaped attack. */
    (void)project(m_idx, rad, fy, fx, py, px, dd + 2, ds, dif, typ, flg,
        degrees_of_arc, false);
}

// a monster calls for help

void shriek(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    char m_name[80];

    /* Get the monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

    if (m_ptr->ml)
    {
        if (singing(SNG_SILENCE))
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_format("%^s lets out a muffled shout for help.", m_name);
            else
                msg_format("%^s lets out a muffled shriek.", m_name);
        }
        else
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_format("%^s shouts for help.", m_name);
            else
                msg_format("%^s makes a high pitched shriek.", m_name);
        }
    }
    else
    {
        if (singing(SNG_SILENCE))
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_print("You hear a muffled shout for help.");
            else
                msg_print("You hear a muffled shriek.");
        }
        else
        {
            if (r_ptr->flags2 & (RF2_SMART))
                msg_print("You hear a shout for help.");
            else
                msg_print("You hear a shriek.");
        }
    }

    // disturb the player
    disturb(0, 0);

    /* Make a lot of noise */
    update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
    monster_perception(false, false, -10);

    // makes monster noise too
    m_ptr->noise += 10;
}

/*
 * Monster attempts to make a ranged (non-melee) attack.
 *
 * Determine if monster can attack at range, then see if it will.  Use
 * the helper function "choose_attack_spell()" to pick a physical ranged
 * attack, magic spell, or summon.  Execute the attack chosen.  Process
 * its effects, and update character knowledge of the monster.
 *
 * Perhaps monsters should breathe at locations *near* the player,
 * since this would allow them to inflict "partial" damage.
 */
bool make_attack_ranged(monster_type* m_ptr, int attack)
{
    int spower, manacost;

    int m_idx = cave_m_idx[m_ptr->fy][m_ptr->fx];

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    char m_name[80];
    char m_poss[80];

    char ddesc[80];

    /* Is the player blind? */
    bool blind = (p_ptr->blind ? true : false);

    /* Can the player see the monster casting the spell? */
    bool seen = (!blind && m_ptr->ml);

    /* Determine mana cost */
    if (attack >= 128)
        return (false);
    else if (attack >= 96)
        manacost = spell_info_RF4[attack - 96][COL_SPELL_MANA_COST];
    else
        return (false);

    /* Spend mana (for non-songs) */
    if (attack < 96 + RF4_SNG_HEAD)
        m_ptr->mana -= manacost; // Sil-x: this is a hack to only have you pay
                                 // mana for things other than songs

    monster_set_visual_facing_target_immediate(m_ptr, p_ptr->py, p_ptr->px);

    /*** Get some info. ***/

    /* Extract the monster's spell power.  Must be at least 1. */
    spower = MAX(1, r_ptr->spell_power);

    /* Get the monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

    /* Get the monster possessive ("his"/"her"/"its") */
    monster_desc(m_poss, sizeof(m_name), m_ptr, 0x22);

    /* Hack -- Get the "died from" name */
    monster_desc(ddesc, sizeof(m_name), m_ptr, 0x88);

    // Sil-y: no chance of spell failure anymore

    /*Monster has cast a spell*/
    m_ptr->mflag &= ~(MFLAG_ALWAYS_CAST);

    {
        int attack_sound = ranged_attack_sound(attack);
        if (attack_sound >= 0)
            sound(attack_sound);
    }

    /*** Execute the ranged attack chosen. ***/
    switch (attack)
    {
    /* RF4_ARROW1, RF4_ARROW2 */
    case 96 + 0:
    case 96 + 1:
    {
        int dd = (attack == 96 + 0) ? 1 : 2;

        disturb(1, 0);
        if (spower < 2)
        {
            if (blind)
                msg_print("You hear a twang.");
            else
                msg_format("%^s fires an arrow.", m_name);
        }
        else
        {
            if (blind)
                msg_print("You hear a loud thwang.");
            else
                msg_format("%^s fires an arrow.", m_name);
        }

        mon_bolt(m_idx, GF_ARROW, dd, get_sides(attack), -1);

        break;
    }

    /* RF4_BOULDER */
    case 96 + 2:
    {
        disturb(1, 0);
        if (blind)
            msg_print("You hear something grunt with exertion.");
        else if (spower < 8)
            msg_format("%^s hurls a rock at you.", m_name);
        else
            msg_format("%^s hurls a boulder at you.", m_name);

        mon_bolt(m_idx, GF_BOULDER, 6, get_sides(attack), -1);

        break;
    }

    /* RF4_BRTH_FIRE */
    case 96 + 3:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        else
            msg_format("%^s breathes fire.", m_name);
        mon_arc(m_idx, GF_FIRE, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 60);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_BRTH_COLD */
    case 96 + 4:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        else
            msg_format("%^s breathes frost.", m_name);
        mon_arc(m_idx, GF_COLD, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 60);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_BRTH_POIS */
    case 96 + 5:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        else
            msg_format("%^s breathes poisonous gas.", m_name);
        mon_arc(m_idx, GF_POIS, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 90);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_BRTH_DARK */
    case 96 + 6:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s breathes.", m_name);
        msg_format("%^s breathes darkness.", m_name);
        mon_arc(m_idx, GF_DARK, true, r_ptr->spell_power, get_sides(attack), -1,
            r_ptr->spell_power / 2, 60);

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -10);

        break;
    }

    /* RF4_EARTHQUAKE */
    case 96 + 7:
    {
        int pit_y, pit_x, dy, dx;

        dy = (m_ptr->fy > p_ptr->py) ? -1 : ((m_ptr->fy < p_ptr->py) ? 1 : 0);
        dx = (m_ptr->fx > p_ptr->px) ? -1 : ((m_ptr->fx < p_ptr->px) ? 1 : 0);
        pit_y = m_ptr->fy + dy;
        pit_x = m_ptr->fx + dx;

        msg_format("%^s slams his hammer into the ground.", m_name);

        earthquake(m_ptr->fy, m_ptr->fx, pit_y, pit_x, 5,
            cave_m_idx[m_ptr->fy][m_ptr->fx]);
        break;
    }

    /* RF4_SHRIEK */
    case 96 + 8:
    {
        disturb(0, 0);

        shriek(m_ptr);
        break;
    }

    /* RF4_SCREECH */
    case 96 + 9:
    {
        disturb(1, 0);
        if (p_ptr->stun || !seen)
        {
            if (singing(SNG_SILENCE))
            {
                msg_print("The air is filled with a muffled screeching.");
            }
            else
            {
                msg_print("The air is filled with an unearthly screeching.");
            }
        }
        else
        {
            if (singing(SNG_SILENCE))
            {
                msg_format("%^s fixes its malevolent gaze upon you and lets "
                           "out a muffled "
                           "screech.",
                    m_name);
            }
            else
            {
                msg_format("%^s fixes its malevolent gaze upon you and lets "
                           "out a terrible "
                           "screech.",
                    m_name);
            }
        }

        if (allow_player_stun(m_ptr))
        {
            if (p_ptr->stun < 100)
            {
                msg_print("Your mind reels.");

                set_stun(p_ptr->stun + 20);
            }
        }

        if (allow_player_fear(m_ptr))
        {
            (void)set_afraid(p_ptr->afraid + damroll(2, 4));
        }

        /* Make a lot of noise */
        update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
        monster_perception(false, false, -20);

        break;
    }

    /* RF4_DARKNESS */
    case 96 + 10:
    {
        disturb(0, 0);

        if (blind)
            msg_format("%^s mutters.", m_name);
        else
            msg_format("%^s gestures in shadow.", m_name);

        (void)darken_area(0, 0, 3);
        break;
    }

    /* RF4_FORGET */
    case 96 + 11:
    {
        disturb(0, 0);

        msg_format("%^s tries to blank your mind.", m_name);

        if (saving_throw(m_ptr, 0))
        {
            msg_print("You resist!");
        }
        else
        {
            msg_print("Your memories fade away.");
            wiz_dark();
        }
        break;
    }

    /* RF4_SCARE */
    case 96 + 12:
    {
        disturb(1, 0);
        if (!m_ptr->ml || one_in_(2))
        {
            msg_format("%^s lets out a terrible cry.", m_name);

            /* Make a lot of noise */
            update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
            monster_perception(false, false, -10);
        }
        else
        {
            msg_format("%^s looks into your eyes.", m_name);
        }
        if (!allow_player_fear(m_ptr) && !(p_ptr->afraid))
        {
            msg_print("You are unafraid.");
        }
        else
        {
            (void)set_afraid(p_ptr->afraid + damroll(3, 4));
        }
        break;
    }

    /* RF4_CONF */
    case 96 + 13:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s mutters.", m_name);
        else
            msg_format("%^s glares at you.", m_name);
        if (allow_player_confusion(m_ptr))
        {
            (void)set_confused(p_ptr->confused + damroll(2, 4));
        }
        break;
    }

    /* RF4_HOLD */
    case 96 + 14:
    {
        disturb(1, 0);
        if (blind)
            msg_format("%^s mutters.", m_name);
        else
            msg_format("%^s stares deep into your eyes.", m_name);

        if (!allow_player_entrancement(m_ptr))
        {
            if (!p_ptr->entranced)
                msg_print("You stare back unafraid!");
        }
        // Must not already be entranced or entranced last round, as chaining
        // entrancement is too nasty
        else if (!p_ptr->entranced && !p_ptr->was_entranced)
        {
            (void)set_entranced(damroll(4, 4));
        }
        break;
    }

        /* RF4_SLOW */
    case 96 + 15:
    {
        disturb(1, 0);
        msg_format("%^s whispers of fading and decay.", m_name);

        if (!allow_player_slow(m_ptr))
        {
            msg_print("You resist.");
        }
        else
        {
            (void)set_slow(p_ptr->slow + damroll(2, 4));
        }
        break;
    }

        /* RF4_HATCH_SPIDER */
    case 96 + 16:
    {
        hatch_spider(m_ptr);

        break;
    }

        /* RF4_DIM */
    case 96 + 17:
    {
        object_type* o_ptr = &inventory[INVEN_LITE];
        int roll = dieroll(4);
        disturb(0, 0);

        switch (roll)
        {
        case 1:
            msg_format("%^s whispers of the cold beneath the earth.", m_name);
            break;
        case 2:
            msg_format("%^s whispers of dusk turning into night.", m_name);
            break;
        case 3:
            msg_format(
                "%^s whispers of flames burning low in a gathering darkness.",
                m_name);
            break;
        default:
            msg_format("%^s whispers of an ancient gloom.", m_name);
        }

        if (o_ptr->tval == TV_LIGHT && player_light_has_fuel(o_ptr))
        {
            if (o_ptr->sval == SV_LIGHT_TORCH
                || o_ptr->sval == SV_LIGHT_MALLORN)
                msg_print("Your torch sputters.");
            else if (o_ptr->sval == SV_LIGHT_LANTERN)
                msg_print("Your lantern sputters.");
            message_flush();

            player_light_add_fuel(o_ptr, -damroll(20, 20));
            if (player_light_fuel(o_ptr) < 1)
                player_light_set_fuel(o_ptr, 1);
        }

        break;
    }

        // Sil-x: only songs after this point as 96+RF4_SNG_HEAD is used in the
        // spell code to distinguish songs from non-songs

        /* RF4_SNG_BINDING */
    case 96 + 18:
    {
        song_of_binding(m_ptr);

        break;
    }

        /* RF4_SNG_PIERCING */
    case 96 + 19:
    {
        song_of_piercing(m_ptr);

        break;
    }

        /* RF4_SNG_OATHS */
    case 96 + 20:
    {
        song_of_oaths(m_ptr);

        break;
    }

    /* RF4_THROW_WEB */
    case 96 + 23:
    {
        if (blind)
            msg_print("You hear a whispering movement.");
        else
            msg_format("%^s tosses strands of sticky web at you.", m_name);

        mon_bolt(m_idx, GF_WEB, 0, 0, -1);

        break;
    }

    /* RF4_RALLY */
    case 96 + 24:
    {
        if (blind)
            msg_print("You hear a rallying cry.");
        else
            msg_format("%^s shouts a rallying cry.", m_name);

        for (int i = mon_max - 1; i >= 1; i--)
        {
            monster_type* target = &mon_list[i];
            monster_race* r_ptr = &r_info[target->r_idx];

            // Rally works on living monsters which are orcs, men, or raukar
            if (!target->r_idx || target == m_ptr
                || (!(r_ptr->flags3 & (RF3_ORC)) && !(r_ptr->flags3 & (RF3_MAN))
                    && !(r_ptr->flags3 & (RF3_RAUKO))))
            {
                continue;
            }

            int d = distance(m_ptr->fx, m_ptr->fy, target->fx, target->fy);
            target->tmp_morale += ((spower * 10 / (d + 4)) * 10);
        }

        break;
    }

        /* Paranoia */
    default:
    {
        msg_print(
            "A monster tried to cast a spell that has not yet been defined.");
    }
    }

    /* Mark minimum desired range for recalculation */
    m_ptr->min_range = 0;

    /* Remember what the monster did to us */
    if (seen)
    {
        /* Innate spell */
        if (attack < 32 * 4)
        {
            l_ptr->flags4 |= (1L << (attack - 32 * 3));
            if (l_ptr->ranged < MAX_UCHAR)
                l_ptr->ranged++;
        }
    }

    // if (seen && p_ptr->wizard)
    //	msg_format("%^s has %i mana remaining.", m_name, m_ptr->mana);

    /* Always take note of monsters that kill you */
    if (p_ptr->is_dead && (l_ptr->deaths < MAX_SHORT))
    {
        l_ptr->deaths++;
    }

    /* A spell was cast */
    return (true);
}

/*
 * Some monsters are surrounded by poison gas, terrible heat, cold, darkness etc
 * Process any such affects.
 */
void cloud_surround(int r_idx, int* typ, int* dd, int* ds, int* rad)
{
    monster_race* r_ptr = &r_info[r_idx];

    *typ = 0;
    *dd = r_ptr->spell_power / 4;
    *ds = 4;
    *rad = 1;

    /*** Determine the kind of cloud we're supposed to be giving off ***/

    /* If breaths and attrs match, the choice is clear. */
    if (r_ptr->flags4)
    {
        /* This is mostly for the serpents */
        if ((r_ptr->flags4 & (RF4_BRTH_FIRE))
            && (r_ptr->flags4 & (RF4_BRTH_POIS))
            && (r_ptr->flags4 & (RF4_BRTH_COLD))
            && (r_ptr->flags4 & (RF4_BRTH_DARK)))
        {
            int rand_num = dieroll(4);

            switch (rand_num)
            {
            case 1:
                *typ = GF_COLD;
                break;
            case 2:
                *typ = GF_FIRE;
                break;
            case 3:
                *typ = GF_POIS;
                break;
            case 4:
                *typ = GF_DARK;
                break;
            }
        }
        else if (r_ptr->flags4 & (RF4_BRTH_POIS))
            *typ = GF_POIS;
        else if (r_ptr->flags4 & (RF4_BRTH_FIRE))
            *typ = GF_FIRE;
        else if (r_ptr->flags4 & (RF4_BRTH_COLD))
            *typ = GF_COLD;
        else if (r_ptr->flags4 & (RF4_BRTH_DARK))
            *typ = GF_DARK;
    }
}
