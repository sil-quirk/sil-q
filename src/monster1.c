/* File: monster1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "sdl-config.h"

/*
 * Pronoun arrays, by gender.
 */
static cptr wd_he[3] = { "it", "he", "she" };
static cptr wd_his[3] = { "its", "his", "her" };
static cptr wd_him[3] = { "it", "him", "her" };

/*
 * Pluralizer.  Args(count, singular, plural)
 */
#define plural(c, s, p) (((c) == 1) ? (s) : (p))

#define PLAYER_GHOST_TRIES_MAX 30

#define MANY_MANY_KILLS 10000

/*
 * Determine if the "armour" is known
 * One kill is needed.
 */
static bool know_armour(s32b kills)
{
    /* Normal monsters */
    if (kills > 0)
        return (true);

    /* Assume false */
    return (false);
}

/*
 * Determine if the "damage" of the given attack is known.
 * One attack is needed.
 */
static bool know_damage(const monster_lore* l_ptr, int i)
{
    /* Unique monsters */
    if (l_ptr->blows[i])
        return (true);

    /* Assume false */
    return (false);
}

static bool lore_knows_contest_stats(const monster_lore* l_ptr)
{
    return ((l_ptr->song_lore_flags & MONSTER_LORE_SONG_CONTEST) != 0);
}

static bool lore_knows_lament_stats(const monster_lore* l_ptr)
{
    return ((l_ptr->song_lore_flags & MONSTER_LORE_SONG_LAMENT) != 0);
}

static int monster_lore_live_index(const monster_type* m_ptr)
{
    if (!m_ptr)
        return 0;

    for (int i = 1; i < mon_max; i++)
    {
        if (&mon_list[i] == m_ptr)
            return i;
    }

    return 0;
}

static bool monster_lore_is_current_duel_target(
    const monster_type* m_ptr, int song)
{
    int m_idx = monster_lore_live_index(m_ptr);

    return (m_idx > 0) && (p_ptr->song_target_idx == m_idx)
        && (p_ptr->song_target_song == song);
}

static bool monster_lore_has_damage_dice_reduction(const monster_type* m_ptr)
{
    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        if (m_ptr->blow_dd_reduction[i] > 0)
            return true;
    }

    return false;
}

static void monster_lore_learn_live_song_duels(
    monster_lore* l_ptr, const monster_type* m_ptr)
{
    if (!m_ptr)
        return;

    if (monster_lore_is_current_duel_target(m_ptr, SNG_CONTEST)
        || (m_ptr->song_contest_stacks > 0) || m_ptr->song_contest_completed
        || (m_ptr->song_stealth_penalty > 0) || (m_ptr->song_evasion_penalty > 0)
        || (m_ptr->song_armor_dice_penalty > 0))
    {
        l_ptr->song_lore_flags |= MONSTER_LORE_SONG_CONTEST;
    }

    if (monster_lore_is_current_duel_target(m_ptr, SNG_LAMENT)
        || (m_ptr->song_lament_stacks > 0) || m_ptr->song_lament_completed
        || (monster_song_hp_loss(m_ptr) > 0)
        || monster_lore_has_damage_dice_reduction(m_ptr))
    {
        l_ptr->song_lore_flags |= MONSTER_LORE_SONG_LAMENT;
    }
}

static void describe_monster_desc(int r_idx)
{
    const monster_race* r_ptr = &r_info[r_idx];
    char buf[2048];

    /* Simple method */
    SDL_strlcpy(buf, r_text + r_ptr->text, sizeof(buf));

    /* Dump it */
    text_out(buf);
    text_out("\n");
}

static void describe_monster_spells(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    int m, n;
    int msex = 0;
    int spower;
    int vn;
    cptr vp[64];
    int attack = -1;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Get spell power */
    spower = r_ptr->spell_power;

    /* Collect innate attacks */
    vn = 0;

    if (l_ptr->flags4 & (RF4_ARROW1))
    {
        vp[vn++] = "fire a shortbow";
        attack = 96 + 0;
    }

    if (l_ptr->flags4 & (RF4_ARROW2))
    {
        vp[vn++] = "fire a longbow";
        attack = 96 + 1;
    }

    if (l_ptr->flags4 & (RF4_BOULDER))
    {
        vp[vn++] = "throw boulders";
        attack = 96 + 2;
    }

    if (l_ptr->flags4 & (RF4_THROW_WEB))
    {
        vp[vn++] = "throw web";
        attack = 96 + 23;
    }

    /* Describe innate attacks */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
            {
                text_out(" may ");
            }

            else if (n < vn - 1)
                text_out(", ");
            else if (n == 1)
                text_out(" or ");
            else
                text_out(", or ");

            /* Dump */
            text_out_c(TERM_L_RED, vp[n]);
        }

        /* Damage */
        if (attack > -1)
        {
            // RF4_ARROW1
            if (attack == 96 + 0)
            {
                text_out_c(TERM_UMBER,
                    format(
                        " (%+d, 1d%d)", r_ptr->spell_power, get_sides(attack)));
            }
            // RF4_ARROW2
            else if (attack == 96 + 1)
            {
                text_out_c(TERM_UMBER,
                    format(
                        " (%+d, 2d%d)", r_ptr->spell_power, get_sides(attack)));
            }
            // GF_BOULDER
            else if (attack == 96 + 2)
            {
                text_out_c(TERM_UMBER,
                    format(
                        " (%+d, 6d%d)", r_ptr->spell_power, get_sides(attack)));
            }
            else if (attack == 96 + 23)
            {
                text_out_c(TERM_UMBER, format(" (%+d)", r_ptr->spell_power));
            }
        }

        /* End */
        text_out(".  ");
    }

    /* Collect breaths */
    vn = 0;

    if (l_ptr->flags4 & (RF4_BRTH_FIRE))
        vp[vn++] = "fire";
    if (l_ptr->flags4 & (RF4_BRTH_COLD))
        vp[vn++] = "frost";
    if (l_ptr->flags4 & (RF4_BRTH_POIS))
        vp[vn++] = "poison";
    if (l_ptr->flags4 & (RF4_BRTH_DARK))
        vp[vn++] = "darkness";

    /* Describe breaths */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(" may breathe ");
            else if (n < vn - 1)
                text_out(", ");
            else if (n == 1)
                text_out(" or ");
            else
                text_out(", or ");

            /* Dump */
            text_out_c(TERM_L_RED, vp[n]);
        }

        text_out(format(" (%dd%d). ", spower, 4));
    }

    /* Collect spells */
    vn = 0;

    if (l_ptr->flags4 & (RF4_EARTHQUAKE))
        vp[vn++] = "cause earthquakes";
    if (l_ptr->flags4 & (RF4_SHRIEK))
        vp[vn++] = "call for help";
    if (l_ptr->flags4 & (RF4_SCREECH))
        vp[vn++] = "cause a hideous screeching";
    if (l_ptr->flags4 & (RF4_DARKNESS))
        vp[vn++] = "create darkness";
    if (l_ptr->flags4 & (RF4_FORGET))
        vp[vn++] = "remove memories";
    if (l_ptr->flags4 & (RF4_SCARE))
        vp[vn++] = "terrify";
    if (l_ptr->flags4 & (RF4_CONF))
        vp[vn++] = "confuse";
    if (l_ptr->flags4 & (RF4_HOLD))
        vp[vn++] = "entrance";
    if (l_ptr->flags4 & (RF4_SLOW))
        vp[vn++] = "slow";
    if (l_ptr->flags4 & (RF4_DIM))
        vp[vn++] = "dim torches and lanterns";

    m = vn;

    /* Describe spells */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s may ", wd_he[msex]));

        /* Normal spells */
        for (n = 0; n < m; n++)
        {
            if (n == 0)
                text_out("attempt to ");
            else if (n < m - 1)
                text_out(", ");
            else if (n != 1)
                text_out(", or ");
            else
                text_out(" or ");

            /* Dump */
            text_out_c(TERM_ORANGE, vp[n]);
        }

        /* End this sentence */
        text_out(".  ");
    }
}

static void describe_monster_drop(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    bool sin = false;

    int n;

    cptr p;

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Drops items */
    if (l_ptr->drop_item)
    {
        if (r_ptr->flags2 & (RF2_TERRITORIAL))
        {
            /* Intro */
            text_out(format("%^s may be found with", wd_he[msex]));
        }
        else
        {
            /* Intro */
            text_out(format("%^s may carry", wd_he[msex]));
        }

        /* Count maximum drop */
        n = l_ptr->drop_item;

        /* One drop (may need an "n") */
        if (n == 1)
        {
            text_out(" a");
            sin = true;
        }

        /* Two drops */
        else if (n == 2)
        {
            text_out(" one or two");
        }

        /* Many drops */
        else
        {
            text_out(format(" up to %d", n));
        }

        if (l_ptr->flags3 & RF3_DROP_ARTEFACT)
        {
            p = " legendary";
            sin = false;
        }

        /* Chests are not noted as good or great
         * (no "n" needed)
         */
        else if (l_ptr->flags1 & RF1_DROP_CHEST)
        {
            p = NULL;
            sin = false;
        }

        /* Superb (no "n" needed) */
        else if (l_ptr->flags2 & RF2_DROP_SUPERB)
        {
            p = " superb";
            sin = false;
        }

        /* Great */
        else if (l_ptr->flags1 & RF1_DROP_GREAT)
        {
            p = " exceptional";
        }

        /* Good (no "n" needed) */
        else if (l_ptr->flags1 & RF1_DROP_GOOD)
        {
            p = " good";
            sin = false;
        }

        /* Okay */
        else
        {
            p = NULL;
        }

        /* Objects */
        if (l_ptr->drop_item)
        {
            /* Handle singular "an" */
            if (sin)
                text_out("n");
            sin = false;

            /* Dump "object(s)" */
            if (p)
                text_out(p);

            /*specify chests where needed*/
            if (l_ptr->flags1 & RF1_DROP_CHEST)
                text_out(" chest");
            else
                text_out(" object");
            if (n != 1)
                text_out("s");

            if (l_ptr->flags3 & RF3_DROP_ARTEFACT)
                text_out(", including an artefact");
        }

        /* End this sentence */
        text_out(".  ");
    }
}

static void describe_monster_attack(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    int m, r, n;
    cptr p, q;

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Count the number of "known" attacks */
    for (n = 0, m = 0; m < MONSTER_BLOW_MAX; m++)
    {
        /* Skip non-attacks */
        if (!r_ptr->blow[m].method)
            continue;

        /* Count known attacks */
        if ((l_ptr->blows[m]) || lore_knows_lament_stats(l_ptr)
            || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR))
            n++;
    }

    /* Examine (and count) the actual attacks */
    for (r = 0, m = 0; m < MONSTER_BLOW_MAX; m++)
    {
        int method, effect, att, d1, d2;

        /* Skip non-attacks */
        if (!r_ptr->blow[m].method)
            continue;

        /* Skip unknown attacks */
        if (!l_ptr->blows[m] && !lore_knows_lament_stats(l_ptr))
            continue;

        /* Extract the attack info */
        method = r_ptr->blow[m].method;
        effect = r_ptr->blow[m].effect;
        att = r_ptr->blow[m].att;
        d1 = r_ptr->blow[m].dd;
        d2 = r_ptr->blow[m].ds;

        if (m_ptr)
        {
            monster_type* live = (monster_type*)m_ptr;
            int dd_reduction = live->blow_dd_reduction[m];
            int ds_reduction = live->blow_ds_reduction[m];

            if (d1 > 0 && dd_reduction > 0)
            {
                if (dd_reduction >= d1)
                    d1 = 1;
                else
                    d1 = MAX(1, d1 - dd_reduction);
            }
            if (d2 > 0 && ds_reduction > 0)
            {
                if (ds_reduction >= d2)
                    d2 = 1;
                else
                    d2 = MAX(1, d2 - ds_reduction);
            }

            att = total_monster_attack(live, att);
        }

        /* No method yet */
        p = NULL;

        /* Get the method */
        switch (method)
        {
        case RBM_HIT:
            p = "hit";
            break;
        case RBM_TOUCH:
            p = "touch (ignoring armour)";
            break;
        case RBM_CLAW:
            p = "claw";
            break;
        case RBM_BITE:
            p = "bite";
            break;
        case RBM_PECK:
            p = "peck";
            break;
        case RBM_STING:
            p = "sting";
            break;
        case RBM_WHIP:
            p = "whip";
            break;
        case RBM_CRUSH:
            p = "crush";
            break;
        case RBM_ENGULF:
            p = "engulf";
            break;
        case RBM_CRAWL:
            p = "crawl on you";
            break;
        case RBM_THORN:
            p = "pierce you with thorns";
            break;
        case RBM_SPORE:
            p = "release spores (ignoring evasion and armour)";
            break;
        }

        /* Default effect */
        q = NULL;

        /* Get the effect */
        switch (effect)
        {
        case RBE_HURT:
            q = "attack";
            break;
        case RBE_WOUND:
            q = "wound";
            break;
        case RBE_BATTER:
            q = "stun";
            break;
        case RBE_SHATTER:
            q = "cause earthquakes";
            break;

        case RBE_UN_BONUS:
            q = "disenchant";
            break;
        case RBE_UN_POWER:
            q = "drain charges";
            break;
        case RBE_LOSE_MANA:
            q = "drain mana";
            break;
        case RBE_SLOW:
            q = "slow";
            break;
        case RBE_EAT_ITEM:
            q = "steal items";
            break;
        case RBE_EAT_FOOD:
            q = "eat your food";
            break;
        case RBE_HUNGER:
            q = "cause hunger";
            break;

        case RBE_POISON:
            q = "poison";
            break;
        case RBE_ACID:
            q = "corrode";
            break;
        case RBE_ELEC:
            q = "shock";
            break;
        case RBE_FIRE:
            q = "burn";
            break;
        case RBE_COLD:
            q = "freeze";
            break;
        case RBE_DARK:
            q = "darken";
            break;

        case RBE_BLIND:
            q = "blind";
            break;
        case RBE_CONFUSE:
            q = "confuse";
            break;
        case RBE_TERRIFY:
            q = "terrify";
            break;
        case RBE_ENTRANCE:
            q = "entrance";
            break;
        case RBE_HALLU:
            q = "induce hallucinations";
            break;
        case RBE_DISEASE:
            q = "cause disease";
            break;

        case RBE_LOSE_STR:
            q = "reduce strength";
            break;
        case RBE_LOSE_DEX:
            q = "reduce dexterity";
            break;
        case RBE_LOSE_CON:
            q = "reduce constitution";
            break;
        case RBE_LOSE_GRA:
            q = "reduce grace";
            break;
        case RBE_LOSE_STR_CON:
            q = "reduce strength and constitution";
            break;
        case RBE_LOSE_ALL:
            q = "reduce all stats";
            break;

        case RBE_DISARM:
            q = "disarm";
            break;
        }

        /* Introduce the attack description */
        if (!r)
        {
            text_out(format("%^s can ", wd_he[msex]));
        }
        else if (r < n - 1)
        {
            text_out(", ");
        }
        else
        {
            text_out(", or ");
        }

        /* Hack -- force a method */
        if (!p)
            p = "do something weird";

        /* Describe the method */
        text_out(p);

        /* Describe the effect (if any) */
        if (q)
        {
            /* Describe the attack type */
            text_out(" to ");
            text_out_c(TERM_L_RED, q);

            /* Describe damage (if known) */
            if ((know_damage(l_ptr, m)) || lore_knows_lament_stats(l_ptr)
                || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR))
            {
                if (d1 > 0 && d2 > 0)
                {
                    text_out_c(
                        TERM_L_WHITE, format(" (%+d, %dd%d)", att, d1, d2));
                }
                else
                {
                    /* Display the attack rating */
                    text_out_c(TERM_L_WHITE, format(" (%+d)", att));
                }
            }
        }

        /* Count the attacks as printed */
        r++;
    }

    /* Finish sentence above */
    if (r)
    {
        text_out(".  ");
    }

    /* Notice lack of attacks */
    else if (l_ptr->flags1 & RF1_NEVER_BLOW)
    {
        text_out(format("%^s has no physical attacks.  ", wd_he[msex]));
    }

    /* Or describe the lack of knowledge */
    else
    {
        text_out(format("Nothing is known about %s attack.  ", wd_his[msex]));
    }
}

static void describe_monster_abilities(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int n;

    int vn;
    cptr vp[64];

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Collect Abilities */
    vn = 0;
    if (r_ptr->flags2 & (RF2_ELFBANE))
        vp[vn++] = "elf-bane"; // elf-bane is obvious
    if (r_ptr->flags4 & (RF4_NOLDORBANE))
        vp[vn++] = "noldor-bane"; // noldor-bane is obvious
    if (r_ptr->flags4 & (RF4_SINDARBANE))
        vp[vn++] = "sindar-bane"; // sindar-bane is obvious
    if (r_ptr->flags4 & (RF4_DWARFBANE))
        vp[vn++] = "dwarf-bane"; // dwarf-bane is obvious
    if (r_ptr->flags4 & (RF4_EDAINBANE))
        vp[vn++] = "edain-bane"; // edain-bane is obvious
    if (l_ptr->flags2 & (RF2_CHARGE))
        vp[vn++] = "charge";
    if (l_ptr->flags2 & (RF2_KNOCK_BACK))
        vp[vn++] = "knock back";
    if (l_ptr->flags2 & (RF2_CRIPPLING))
        vp[vn++] = "crippling shot";
    if (l_ptr->flags2 & (RF2_CRUEL_BLOW))
        vp[vn++] = "cruel blow";
    if (l_ptr->flags2 & (RF2_OPPORTUNIST))
        vp[vn++] = "opportunist";
    if (l_ptr->flags2 & (RF2_ZONE_OF_CONTROL))
        vp[vn++] = "zone of control";
    if (l_ptr->flags2 & (RF2_EXCHANGE_PLACES))
        vp[vn++] = "exchange places";
    if (l_ptr->flags2 & (RF2_RIPOSTE))
        vp[vn++] = "riposte";
    if (l_ptr->flags2 & (RF2_FLANKING))
        vp[vn++] = "flanking";
    if (l_ptr->flags4 & (RF4_SNG_BINDING))
        vp[vn++] = "song of binding";
    if (l_ptr->flags4 & (RF4_SNG_PIERCING))
        vp[vn++] = "song of piercing";
    if (l_ptr->flags4 & (RF4_SNG_OATHS))
        vp[vn++] = "song of oaths";
    if (l_ptr->flags4 & (RF4_HATCH_SPIDER))
        vp[vn++] = "hatch spider";
    if (l_ptr->flags4 & (RF4_RALLY))
        vp[vn++] = "rally foes";

    /* Describe Abilities */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if ((n == 0) && (vn > 1))
                text_out(" has the abilities: ");
            else if (n == 0)
                text_out(" has the ability: ");
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(" and ");

            /* Dump */
            text_out_c(TERM_RED, vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Collect special abilities. */
    vn = 0;
    if (r_ptr->light > 0)
    {
        /*humaniods carry torches, others glow*/
        if (!strchr("@G", r_ptr->d_char))
            vp[vn++] = "radiate light";
        else
            vp[vn++] = "use a light source";
    }
    if (r_ptr->light < 0)
        vp[vn++] = "produce an unnatural darkness";
    if (l_ptr->flags2 & RF2_FLYING)
        vp[vn++] = "fly";
    if (l_ptr->flags2 & RF2_OPEN_DOOR)
        vp[vn++] = "open doors";
    if (l_ptr->flags2 & RF2_PASS_DOOR)
        vp[vn++] = "pass through doors";
    if (l_ptr->flags2 & RF2_UNLOCK_DOOR)
        vp[vn++] = "unlock doors";
    if (l_ptr->flags2 & RF2_BASH_DOOR)
        vp[vn++] = "bash down doors";
    if (l_ptr->flags2 & RF2_PASS_WALL)
        vp[vn++] = "pass through walls";
    if (l_ptr->flags2 & RF2_KILL_WALL)
        vp[vn++] = "bore through walls";
    if (l_ptr->flags2 & RF2_TUNNEL_WALL)
        vp[vn++] = "tunnel through walls";
    if (l_ptr->flags2 & RF2_KILL_BODY)
        vp[vn++] = "destroy weaker monsters";
    if (l_ptr->flags2 & RF2_TAKE_ITEM)
        vp[vn++] = "pick up objects";
    if (l_ptr->flags2 & RF2_KILL_ITEM)
        vp[vn++] = "destroy objects";

    /* Describe special abilities. */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(" can ");
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(" and ");

            /* Dump */
            text_out(vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Describe special abilities. */
    if (l_ptr->flags2 & RF2_INVISIBLE)
    {
        text_out(format("%^s is very difficult to see.  ", wd_he[msex]));
    }
    if (l_ptr->flags2 & RF2_MULTIPLY)
    {
        text_out(format("%^s breeds explosively.  ", wd_he[msex]));
    }
    if (l_ptr->flags2 & RF2_REGENERATE)
    {
        text_out(format("%^s regenerates quickly.  ", wd_he[msex]));
    }
    if (l_ptr->flags1 & RF1_NO_CRIT)
    {
        text_out(format("%^s cannot be critically hit.  ", wd_he[msex]));
    }
    if (l_ptr->flags1 & RF1_RES_CRIT)
    {
        text_out(format("%^s is resistant to critical hits.  ", wd_he[msex]));
    }

    if (l_ptr->flags2 & (RF2_CLOUD_SURROUND))
    {
        int typ = 0, dd = 0, ds = 0, rad = 0;

        /* Get type of cloud */
        cloud_surround(r_idx, &typ, &dd, &ds, &rad);

        /*hack - alter type for char-attr monster*/

        if ((r_ptr->flags4 & (RF4_BRTH_FIRE))
            && (r_ptr->flags4 & (RF4_BRTH_POIS))
            && (r_ptr->flags4 & (RF4_BRTH_DARK))
            && (r_ptr->flags4 & (RF4_BRTH_COLD)))
        {
            text_out(
                format("%^s is surrounded by shimmering haze of ever changing "
                       "elements (%dd%d).  ",
                    wd_he[msex], dd, ds));
        }

        /* We emit something */
        else if (typ)
        {
            text_out(format(
                "%^s is surrounded by a shimmering haze of ", wd_he[msex]));

            /* Describe cloud */
            if (typ == GF_FIRE)
                text_out_c(TERM_L_RED, "fire");
            else if (typ == GF_COLD)
                text_out_c(TERM_L_RED, "frost");
            else if (typ == GF_POIS)
                text_out_c(TERM_L_RED, "poison");
            else if (typ == GF_DARK)
                text_out_c(TERM_L_RED, "darkness");
            text_out(format(" (%dd%d).  ", dd, ds));
        }
    }

    /* Describe intelligence. */
    if (r_ptr->flags2 & RF2_MINDLESS)
    {
        text_out(format("%^s is mindless.  ", wd_he[msex]));
    }
    else if (r_ptr->flags2 & RF2_SMART)
    {
        // text_out(format("%^s is a servant of the dark.  ", wd_he[msex]));
    }
    else
    {
        text_out(format("%^s is unintelligent.  ", wd_he[msex]));
    }

    /* Collect susceptibilities */
    vn = 0;
    if (l_ptr->flags3 & RF3_STONE)
        vp[vn++] = "shattering";
    if (l_ptr->flags3 & RF3_HURT_LITE)
        vp[vn++] = "bright light";
    if (l_ptr->flags3 & RF3_HURT_FIRE)
        vp[vn++] = "fire";
    if (l_ptr->flags3 & RF3_HURT_COLD)
        vp[vn++] = "cold";

    /* Describe susceptibilities */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(" is vulnerable to ");
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(" and ");

            /* Dump */
            text_out_c(TERM_L_BLUE, vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Collect resistances */
    vn = 0;
    if (l_ptr->flags3 & RF3_RES_ELEC)
        vp[vn++] = "lightning";
    if (l_ptr->flags3 & RF3_RES_FIRE)
        vp[vn++] = "fire";
    if (l_ptr->flags3 & RF3_RES_COLD)
        vp[vn++] = "cold";
    if (l_ptr->flags3 & RF3_RES_POIS)
        vp[vn++] = "poison";

    /* Describe immunities */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(" resists ");
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(" and ");

            /* Dump */
            text_out(vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Collect non-effects */
    vn = 0;
    if (l_ptr->flags3 & RF3_NO_SLOW)
        vp[vn++] = "slowed";
    if (l_ptr->flags3 & RF3_NO_STUN)
        vp[vn++] = "stunned";
    if (l_ptr->flags3 & RF3_NO_FEAR)
        vp[vn++] = "frightened";
    if (l_ptr->flags3 & RF3_NO_CONF)
        vp[vn++] = "confused";
    if (l_ptr->flags3 & RF3_NO_SLEEP)
        vp[vn++] = "put to sleep";

    /* Describe non-effects */
    if (vn)
    {
        /* Intro */
        text_out(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++)
        {
            /* Intro */
            if (n == 0)
                text_out(" cannot be ");
            else if (n < vn - 1)
                text_out(", ");
            else
                text_out(" or ");

            /* Dump */
            text_out_c(TERM_YELLOW, vp[n]);
        }

        /* End */
        text_out(".  ");
    }

    /* Describe escort */
    if (l_ptr->flags1 & RF1_ESCORT)
    {
        text_out(format("%^s usually appears with escorts.  ", wd_he[msex]));
    }

    /* Describe escorts */
    if (l_ptr->flags1 & RF1_ESCORTS)
    {
        text_out(
            format("%^s usually appears with many escorts.  ", wd_he[msex]));
    }

    /* Describe friend */
    else if (l_ptr->flags1 & RF1_FRIEND)
    {
        text_out(format("%^s sometimes appears in groups.  ", wd_he[msex]));
    }

    /* Describe friends */
    else if (l_ptr->flags1 & RF1_FRIENDS)
    {
        text_out(format("%^s usually appears in groups.  ", wd_he[msex]));
    }
}

static void describe_monster_kills(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int msex = 0;

    bool out = true;

    int real_tkills;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    // determine the real number of ancestor kills for printing purposes
    if (l_ptr->tkills < MANY_MANY_KILLS)
        real_tkills = l_ptr->tkills;
    else
        real_tkills = l_ptr->tkills - MANY_MANY_KILLS;

    /* Treat uniques differently */
    if (l_ptr->flags1 & RF1_UNIQUE)
    {
        /* Determine if the unique is "dead" */
        bool dead = (r_ptr->max_num == 0) ? true : false;

        /* We've been killed... */
        if (l_ptr->deaths)
        {
            /* Killed predecessors */
            text_out(format("%^s has slain %d of your predecessors",
                wd_he[msex], l_ptr->deaths));

            /* But we've also killed it */
            if (dead)
            {
                text_out(", but you have taken revenge!  ");
            }

            /* Unavenged (ever) */
            else
            {
                text_out(format(", who %s unavenged.  ",
                    plural(l_ptr->deaths, "remains", "remain")));
            }
        }

        /* Dead unique who never hurt us */
        else if (dead)
        {
            text_out("You have slain this foe.  ");
        }

        if (!dead)
        {
            if (l_ptr->psights)
            {
                text_out("You have encountered this foe.  ");
            }
            else
            {
                text_out("You are yet to encounter this foe.  ");
            }
        }
    }

    /* Not unique, but killed us */
    else if (l_ptr->deaths)
    {
        /* Dead predecessors */
        text_out(
            format("%d of your predecessors %s been killed by this creature, ",
                l_ptr->deaths, plural(l_ptr->deaths, "has", "have")));

        /* Some kills this life */
        if (l_ptr->pkills)
        {
            text_out(format(
                "and you have slain %d of the %d you have encountered.  ",
                l_ptr->pkills, l_ptr->psights));
        }

        /* Some kills past lives */
        else if (real_tkills)
        {
            text_out(format("and %s have slain %d in return.  ",
                "your predecessors", real_tkills));

            if (l_ptr->psights)
            {
                text_out(format("You have encountered %d.  ", l_ptr->psights));
            }
            else
            {
                text_out("You are yet to encounter one.  ");
            }
        }

        /* No kills */
        else
        {
            text_out_c(TERM_RED,
                format("and it is not known to have been defeated.  "));

            if (l_ptr->psights)
            {
                text_out(format("You have encountered %d.  ", l_ptr->psights));
            }
            else
            {
                text_out("You are yet to encounter one.  ");
            }
        }
    }

    /* Normal monsters */
    else
    {
        /* Encountered some this life */
        if (l_ptr->psights && !l_ptr->pkills)
        {
            text_out(format("You have encountered %d of these creatures, ",
                l_ptr->psights));

            /* Killed some last life */
            if (real_tkills)
            {
                text_out(format(
                    "and your predecessors have slain %d.  ", real_tkills));
            }

            /* Killed none */
            else
            {
                text_out("but no battles to the death are recalled.  ");
            }
        }

        /* Killed some this life */
        else if (l_ptr->pkills)
        {
            text_out(
                format("You have slain %d of the %d you have encountered.  ",
                    l_ptr->pkills, l_ptr->psights));
        }

        /* Encountered some this life */
        else
        {
            text_out(format("You have encountered none of these creatures, ",
                l_ptr->psights));

            /* Killed some this life -- should be impossible to reach here */
            if (l_ptr->pkills)
            {
                text_out(format("but slain %d.  ", l_ptr->pkills));
            }

            /* Killed some last life */
            else if (real_tkills)
            {
                text_out(format(
                    "but your predecessors have slain %d.  ", real_tkills));
            }

            /* Killed none */
            else
            {
                text_out("and no battles to the death are recalled.  ");
            }
        }
    }

    /* Separate */
    if (out)
        text_out("\n");
}

static void describe_monster_toughness(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];
    bool knows_full_toughness = ((know_armour(l_ptr->tkills))
        || (l_ptr->tsights == MAX_SHORT) || (l_ptr->ranged == MAX_UCHAR));
    bool knows_hp = knows_full_toughness || lore_knows_lament_stats(l_ptr);
    bool knows_defence = knows_full_toughness || lore_knows_contest_stats(l_ptr);

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Describe monster "toughness" */
    if (knows_hp || knows_defence)
    {
        int evn = r_ptr->evn;
        int prot_dice = r_ptr->pd;
        int prot_sides = r_ptr->ps;

        /* Health */
        char hp_text[32];
        if (r_ptr->hdice > 0 && r_ptr->hside > 0)
            strnfmt(hp_text, sizeof(hp_text), "%dd%d", r_ptr->hdice, r_ptr->hside);
        else if (r_ptr->hdice > 0)
            strnfmt(hp_text, sizeof(hp_text), "%d", r_ptr->hdice);
        else
            strnfmt(hp_text, sizeof(hp_text), "%d", r_ptr->hside);

        if (knows_hp)
        {
            text_out(format("%^s has ", wd_he[msex]));
            text_out_c(TERM_GREEN, hp_text);
            if (m_ptr)
            {
                int hp_loss = monster_song_hp_loss(m_ptr);
                if (hp_loss > 0)
                    text_out_c(TERM_L_RED, format("-%d", hp_loss));
            }
            text_out(" hp");
        }

        /* Defence */
        if (knows_defence && m_ptr)
        {
            monster_type* live = (monster_type*)m_ptr;
            evn = total_monster_evasion(live, false);

            int base_dice = r_ptr->pd - m_ptr->song_armor_dice_penalty;
            if (base_dice < 0)
                base_dice = 0;
            prot_dice = base_dice + curse_flag_delta_cur(CUR_MON_ARM_DICE);
            if (prot_dice < 0)
                prot_dice = 0;

            int base_sides = monster_base_armour_sides(live);
            base_sides += curse_flag_delta_cur(CUR_MON_ARM_SIDE);
            if (base_sides < 0)
                base_sides = 0;
            prot_sides = base_sides;

            if (prot_dice > 0 && prot_sides < 1)
                prot_sides = 1;
        }

        if (knows_hp && knows_defence)
            text_out(" and a defence of ");
        else if (knows_defence)
            text_out(format("%^s has a defence of ", wd_he[msex]));

        if (knows_defence && (prot_dice > 0) && (prot_sides > 0))
        {
            text_out_c(
                TERM_SLATE, format("[%+d, %dd%d]", evn, prot_dice, prot_sides));
        }
        else if (knows_defence)
        {
            text_out_c(TERM_SLATE, format("[%+d]", evn));
        }

        text_out(".  ");
    }
}

static void describe_monster_skills(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int msex = 0;
    cptr act;
    bool knows_normal_skills;
    bool knows_will;
    bool knows_stealth;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    knows_normal_skills = ((l_ptr->ranged == MAX_UCHAR)
        || ((l_ptr->tsights > 1)
            && (10 - l_ptr->tsights < p_ptr->skill_use[S_PER])));
    knows_will = knows_normal_skills || lore_knows_contest_stats(l_ptr)
        || lore_knows_lament_stats(l_ptr);
    knows_stealth = (knows_normal_skills
        && p_ptr->active_ability[S_PER][PER_LISTEN])
        || lore_knows_contest_stats(l_ptr);

    /* Describe experience if known */
    if (knows_will || knows_stealth || knows_normal_skills)
    {
        int will = r_ptr->wil;
        int stealth = r_ptr->stl;
        int perception = r_ptr->per;
        bool printed = false;

        if (m_ptr)
        {
            monster_type* live = (monster_type*)m_ptr;
            will = monster_skill(live, S_WIL);
            stealth = monster_skill(live, S_STL);
            perception = monster_skill(live, S_PER);
        }

        text_out(format("%^s has ", wd_he[msex]));
        if (knows_will)
        {
            text_out_c(TERM_L_GREEN, format("%d", will));
            text_out(" Will");
            printed = true;
        }
        if (knows_stealth)
        {
            if (printed)
                text_out(", ");
            text_out_c(TERM_L_GREEN, format("%d", stealth));
            text_out(" Stealth");
            printed = true;
        }
        if (knows_normal_skills)
        {
            if (printed)
                text_out(", ");
            text_out_c(TERM_L_GREEN, format("%d", perception));
            text_out(" Perception");
        }

        if (r_ptr->sleep > 20) // 21 +
        {
            act = "is usually found asleep";
        }
        else if (r_ptr->sleep > 15) // 16 to 20
        {
            act = "is often found asleep";
        }
        else if (r_ptr->sleep > 10) // 11 to 15
        {
            act = "is sometimes found asleep";
        }
        else if (r_ptr->sleep > 5) // 6 to 10
        {
            act = "is never found asleep";
        }
        else if (r_ptr->sleep > 1) // 2 to 5
        {
            act = "is quick to notice intruders";
        }
        else if (r_ptr->sleep > 0) // 1
        {
            act = "is very quick to notice intruders";
        }
        else // 0
        {
            act = "is ever vigilant";
        }

        if (!knows_normal_skills || (r_ptr->flags2 & (RF2_MINDLESS)))
        {
            text_out(".  ");
        }
        else
        {
            text_out(format(", and %s.  ", act));
        }
    }
}

static void describe_monster_song_duel_progress(
    const monster_lore* l_ptr, const monster_type* m_ptr)
{
    bool printed = false;

    if (!m_ptr)
        return;

    if (lore_knows_contest_stats(l_ptr) && !m_ptr->song_contest_completed)
    {
        text_out_c(TERM_YELLOW,
            format("Song of Contest pressure: %d/%d",
                m_ptr->song_contest_stacks, SONG_DUEL_STACK_LIMIT));
        printed = true;
    }

    if (lore_knows_lament_stats(l_ptr) && !m_ptr->song_lament_completed)
    {
        if (printed)
            text_out("; ");
        text_out_c(TERM_YELLOW,
            format("Song of Lament pressure: %d/%d",
                m_ptr->song_lament_stacks, SONG_DUEL_STACK_LIMIT));
        printed = true;
    }

    if (printed)
        text_out(".  ");
}

static void describe_monster_exp(int r_idx, const monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    long i;

    int msex = 0;

    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE)
        msex = 2;
    else if (r_ptr->flags1 & RF1_MALE)
        msex = 1;

    /* Describe experience if known */
    if (l_ptr->tkills || l_ptr->tsights)
    {
        /* Introduction for Encounters */
        if (l_ptr->psights)
        {
            if (l_ptr->flags1 & RF1_UNIQUE)
            {
                text_out(format("Encountering %s was worth", wd_him[msex]));
            }
            else
                text_out("Encountering another would be worth");
        }
        else
        {
            if (l_ptr->flags1 & RF1_UNIQUE)
                text_out(
                    format("Encountering %s would be worth", wd_him[msex]));
            else
                text_out("Encountering one would be worth");
        }

        /* calculate the integer exp part */
        i = adjusted_mon_exp(r_ptr, false);

        /* Mention the experience */
        text_out(format(" %ld experience.  ", (long)i));

        /* Introduction for Kills */
        if (!(r_ptr->flags1 & (RF1_PEACEFUL)))
        {
            if (l_ptr->pkills)
            {
                if (l_ptr->flags1 & RF1_UNIQUE)
                    text_out(format("Killing %s was worth", wd_him[msex]));
                else
                    text_out("Killing another would be worth");
            }
            else
            {
                if (l_ptr->flags1 & RF1_UNIQUE)
                    text_out(format("Killing %s would be worth", wd_him[msex]));
                else
                    text_out("Killing one would be worth");
            }

            /* calculate the integer exp part */
            i = adjusted_mon_exp(r_ptr, true);

            /* Mention the experience */
            text_out(format(" %ld.  ", (long)i));
        }
    }
}

static void describe_monster_movement(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    bool old = false;

    int display_speed = r_ptr->speed;
    bool is_hasted = false;
    bool is_slowed = false;

    if (m_ptr)
    {
        if (m_ptr->mspeed)
            display_speed = m_ptr->mspeed;
        is_hasted = (m_ptr->hasted > 0);
        is_slowed = (m_ptr->slowed > 0);
    }

    text_out("This");

    if (l_ptr->flags3 & RF3_UNDEAD)
        text_out_c(TERM_L_BLUE, " undead");

    if (l_ptr->flags3 & RF3_DRAGON)
        text_out_c(TERM_L_BLUE, " dragon");
    else if (l_ptr->flags3 & RF3_SERPENT)
        text_out_c(TERM_L_BLUE, " serpent");
    else if (l_ptr->flags3 & RF3_VAMPIRE)
        text_out_c(TERM_L_BLUE, " vampire");
    else if (l_ptr->flags3 & RF3_RAUKO)
        text_out_c(TERM_L_BLUE, " rauko");
    else if (l_ptr->flags3 & RF3_TROLL)
        text_out_c(TERM_L_BLUE, " troll");
    else if (l_ptr->flags3 & RF3_ORC)
        text_out_c(TERM_L_BLUE, " orc");
    else if (l_ptr->flags3 & RF3_GIANT)
        text_out_c(TERM_L_BLUE, " giant");
    else if (l_ptr->flags3 & RF3_WOLF)
        text_out_c(TERM_L_BLUE, " wolf");
    else if (l_ptr->flags3 & RF3_SPIDER)
        text_out_c(TERM_L_BLUE, " spider");
    else if (l_ptr->flags3 & RF3_CAT)
        text_out_c(TERM_L_BLUE, " cat");
    else if (l_ptr->flags3 & RF3_HORROR)
        text_out_c(TERM_L_BLUE, " horror");
    else if (l_ptr->flags3 & RF3_MAN)
        text_out_c(TERM_L_BLUE, " man");
    else if (l_ptr->flags3 & RF3_ELF)
        text_out_c(TERM_L_BLUE, " elf");
    else
        text_out(" creature");

    /* Describe location */
    if (r_ptr->level == 0)
    {
        text_out_c(TERM_YELLOW, " dwells at the gates of Angband");
        old = true;
    }
    else
    {
        if (l_ptr->flags1 & RF1_FORCE_DEPTH)
            text_out(" is found ");
        else
            text_out(" is normally found ");

        if (r_idx == R_IDX_CARCHAROTH)
        {
            text_out_c(TERM_YELLOW, "guarding the gates of Angband");
        }
        else if (r_ptr->level < MORGOTH_DEPTH)
        {
            text_out("at depths of ");
            text_out_c(TERM_YELLOW, format("%d feet", r_ptr->level * 50));
        }
        else
        {
            text_out("at depths of ");
            text_out_c(TERM_YELLOW, format("%d feet", MORGOTH_DEPTH * 50));
        }
        old = true;
    }

    /* Deal with non-moving creatures */
    if (l_ptr->flags1 & RF1_NEVER_MOVE)
    {
        if (old)
            text_out(", and");
        text_out(" cannot move");
    }

    /* Deal with non-moving creatures */
    else if (l_ptr->flags1 & RF1_HIDDEN_MOVE)
    {
        if (old)
            text_out(", and");
        text_out(" never moves when you are looking");
    }

    /* most other creatures display their speed */
    else if ((display_speed != 2) || (l_ptr->flags1 & RF1_RAND_50)
        || (l_ptr->flags1 & RF1_RAND_25))
    {
        if (old)
            text_out(", and");
        text_out(" moves");

        /* Random-ness */
        if ((l_ptr->flags1 & RF1_RAND_50) || (l_ptr->flags1 & RF1_RAND_25))
        {
            /* Adverb */
            if ((l_ptr->flags1 & RF1_RAND_50) && (l_ptr->flags1 & RF1_RAND_25))
            {
                text_out(" extremely");
            }
            else if (l_ptr->flags1 & RF1_RAND_50)
            {
                text_out(" somewhat");
            }
            else if (l_ptr->flags1 & RF1_RAND_25)
            {
                text_out(" a bit");
            }

            /* Adjective */
            text_out(" erratically");

            /* Hack -- Occasional conjunction */
            if (display_speed != 2)
                text_out(", and");
        }

        /* Speed */
        if (display_speed > 2)
        {
            if (display_speed > 5)
                text_out_c(TERM_L_GREEN, " incredibly");
            else if (display_speed > 4)
                text_out_c(TERM_L_GREEN, " extremely");
            else if (display_speed > 3)
                text_out_c(TERM_L_GREEN, " very");
            text_out_c(TERM_L_GREEN, " quickly");
        }
        else if (display_speed < 2)
        {
            text_out_c(TERM_L_UMBER, " slowly");
        }

        if (m_ptr)
        {
            text_out_c(
                TERM_L_GREEN, format(" (speed %d)", display_speed));
            if (is_hasted && is_slowed)
                text_out(" while hasted and slowed");
            else if (is_hasted)
                text_out(" while hasted");
            else if (is_slowed)
                text_out(" while slowed");
        }
    }

    /* End this sentence */
    text_out(".  ");

    /*note if this monster does not pursue you*/
    if (l_ptr->flags2 & (RF2_TERRITORIAL))
    {
        int msex = 0;

        /* Extract a gender (if applicable) */
        if (r_ptr->flags1 & RF1_FEMALE)
            msex = 2;
        else if (r_ptr->flags1 & RF1_MALE)
            msex = 1;

        text_out(format("%^s does not deign to pursue you.  ", wd_he[msex]));
    }
}

/*
 * Learn everything about a monster (by cheating)
 */
static void cheat_monster_lore(int r_idx, monster_lore* l_ptr)
{
    const monster_race* r_ptr = &r_info[r_idx];

    int i;

    /* Hack -- Maximal predecessors kills */
    if (l_ptr->tkills < MANY_MANY_KILLS)
        l_ptr->tkills += MANY_MANY_KILLS;

    /* Hack -- Maximal info */
    l_ptr->notice = l_ptr->ignore = MAX_UCHAR;

    /* Observe "maximal" attacks */
    for (i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        /* Examine "actual" blows */
        if (r_ptr->blow[i].effect || r_ptr->blow[i].method)
        {
            /* Hack -- maximal observations */
            l_ptr->blows[i] = MAX_UCHAR;
        }
    }

    /* Hack -- maximal drops */
    l_ptr->drop_item = (((r_ptr->flags1 & RF1_DROP_4D2) ? 8 : 0)
        + ((r_ptr->flags1 & RF1_DROP_3D2) ? 6 : 0)
        + ((r_ptr->flags1 & RF1_DROP_2D2) ? 4 : 0)
        + ((r_ptr->flags1 & RF1_DROP_1D2) ? 2 : 0)
        + ((r_ptr->flags1 & RF1_DROP_100) ? 1 : 0)
        + ((r_ptr->flags1 & RF1_DROP_33) ? 1 : 0)
        + ((r_ptr->flags3 & RF3_DROP_1D3) ? 3 : 0));

    /* Hack -- observe many spells */
    l_ptr->ranged = MAX_UCHAR;

    /* Hack -- know all the flags */
    l_ptr->flags1 = r_ptr->flags1;
    l_ptr->flags2 = r_ptr->flags2;
    l_ptr->flags3 = r_ptr->flags3;
    l_ptr->flags4 = r_ptr->flags4;
}

/*
 * Hack -- display monster information using "roff()"
 *
 * Note that there is now a compiler option to only read the monster
 * descriptions from the raw file when they are actually needed, which
 * saves about 60K of memory at the cost of disk access during monster
 * recall, which is optional to the user.
 *
 * This function should only be called with the cursor placed at the
 * left edge of the screen, on a cleared line, in which the recall is
 * to take place.  One extra blank line is left after the recall.
 */
static bool monster_recall_blocked_by_hallucination(void)
{
    return (p_ptr && p_ptr->image);
}

static void monster_recall_show_hallucination_block(void)
{
    msg_print("Your vision is too distorted to study monsters carefully.");
    window_stuff();
    Term_fresh();
}

void describe_monster(int r_idx, bool spoilers, const monster_type* m_ptr)
{
    monster_lore lore;

    monster_lore save_mem;

    /* Get the race and lore */
    const monster_race* r_ptr = &r_info[r_idx];
    monster_lore* l_ptr = &l_list[r_idx];

    monster_lore_learn_live_song_duels(l_ptr, m_ptr);

    /* Cheat -- know everything */
    if ((cheat_know) || know_monster_info)
    {
        /* XXX XXX XXX */

        /* Hack -- save memory */
        memcpy(&save_mem, l_ptr, sizeof(monster_lore));
    }

    /* Hack -- create a copy of the monster-memory */
    memcpy(&lore, l_ptr, sizeof(monster_lore));

    /* Assume some "obvious" flags */
    lore.flags1 |= (r_ptr->flags1 & RF1_OBVIOUS_MASK);

    /* Killing a monster reveals some properties */
    if (lore.tkills)
    {
        /* Know "race" flags */
        lore.flags3 |= (r_ptr->flags3 & RF3_RACE_MASK);

        /* Know "forced" flags */
        lore.flags1 |= (r_ptr->flags1 & (RF1_FORCE_DEPTH));
    }

    /* Cheat -- know everything */
    if ((cheat_know) || know_monster_info || spoilers)
    {
        cheat_monster_lore(r_idx, &lore);
    }

    /* Show kills of monster vs. player(s) */
    if (!spoilers)
        describe_monster_kills(r_idx, &lore);

    /* Describe experience */
    if (!spoilers)
        describe_monster_exp(r_idx, &lore);

    /* Monster description */
    describe_monster_desc(r_idx);

    /* Describe the movement and level of the monster */
    describe_monster_movement(r_idx, &lore, m_ptr);

    /* Describe spells and innate attacks */
    describe_monster_spells(r_idx, &lore);

    /* Describe the abilities of the monster */
    describe_monster_abilities(r_idx, &lore);

    /* Describe the known attacks */
    describe_monster_attack(r_idx, &lore, m_ptr);

    /* Describe monster "toughness" */
    describe_monster_toughness(r_idx, &lore, m_ptr);

    /* Describe the known skills */
    describe_monster_skills(r_idx, &lore, m_ptr);

    /* Describe duel-song pressure on this individual monster */
    describe_monster_song_duel_progress(&lore, m_ptr);

    /* Describe the monster drop */
    describe_monster_drop(r_idx, &lore);

    /* All done */
    text_out("\n");

    /* Cheat -- know everything */
    if ((cheat_know) || know_monster_info)
    {
        /* Hack -- restore memory */
        memcpy(l_ptr, &save_mem, sizeof(monster_lore));
    }
}

static void roff_top_live(int r_idx, const monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[r_idx];

    byte a1;
    char c1;

    /* Get the chars and attrs using graphics-aware macros */
    c1 = monster_char(r_ptr);
    a1 = monster_attr(r_ptr);

    /* Clear the top line */
    Term_erase(0, 0, 255);

    /* Reset the cursor */
    Term_gotoxy(0, 0);

    /* A title (use "The" for non-uniques) */
    if (!(r_ptr->flags1 & RF1_UNIQUE))
    {
        Term_addstr(-1, TERM_WHITE, "The ");
    }

    /* Dump the name */
    Term_addstr(-1, TERM_WHITE, (r_name + r_ptr->name));

    /* Append the "standard" attr/char info */
    Term_addstr(-1, TERM_WHITE, " - ");
    Term_addch(a1, c1);
    if (use_bigtile)
    {
        Term_addch(255, -1);
    }

    if (m_ptr && (m_ptr->maxhp > 0))
    {
        char hp_bar[10];
        byte attr = health_attr(m_ptr->hp, m_ptr->maxhp);

        monster_health_bar_text(m_ptr, hp_bar, sizeof(hp_bar), 8);

        Term_addstr(-1, TERM_WHITE, " ");
        Term_addstr(-1, attr, hp_bar[0] ? hp_bar : "-");
    }

    Term_addstr(-1, TERM_SLATE, "");
}

/*
 * Hack -- Display the "name" and "attr/chars" of a monster race
 */
void roff_top(int r_idx)
{
    roff_top_live(r_idx, NULL);
}

typedef struct monster_recall_screen_capture
{
    int width;
    int height;
    byte* attrs;
    char* chars;
    byte* tattrs;
    char* tchars;
    byte* story;
} monster_recall_screen_capture;

static void monster_recall_screen_capture_free(
    monster_recall_screen_capture* capture)
{
    if (!capture)
        return;

    mem_free_null(capture->attrs);
    mem_free_null(capture->chars);
    mem_free_null(capture->tattrs);
    mem_free_null(capture->tchars);
    mem_free_null(capture->story);

    capture->width = 0;
    capture->height = 0;
}

static int monster_recall_screen_capture_used_rows(term* t)
{
    if (!t || !t->scr)
        return 0;

    for (int y = t->hgt - 1; y >= 0; y--)
    {
        for (int x = 0; x < t->wid; x++)
        {
            if ((t->scr->c[y][x] != ' ')
                || (t->scr->a[y][x] != t->attr_blank)
                || (t->scr->story[y][x] != 0))
            {
                return y + 1;
            }
        }
    }

    return 0;
}

static int monster_recall_screen_capture_used_cols(term* t, int rows)
{
    int used_cols = 0;

    if (!t || !t->scr || rows <= 0)
        return 0;

    if (rows > t->hgt)
        rows = t->hgt;

    for (int y = 0; y < rows; y++)
    {
        for (int x = t->wid - 1; x >= 0; x--)
        {
            if ((t->scr->c[y][x] != ' ')
                || (t->scr->a[y][x] != t->attr_blank)
                || (t->scr->story[y][x] != 0))
            {
                if (x + 1 > used_cols)
                    used_cols = x + 1;
                break;
            }
        }
    }

    return used_cols;
}

static bool monster_recall_screen_capture_build(
    int r_idx, const monster_type* m_ptr,
    monster_recall_screen_capture* capture)
{
    term scratch;
    term* saved_term = Term;
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    story_font_term_state story_state;
    bool story_pushed = false;
    bool scratch_ready = false;
    bool success = false;
    bool use_story_font = story_monster_desc_enabled();
    int term_wid = 80;
    int term_hgt = 24;
    int used_rows;
    int used_cols;

    if (!capture || !saved_term)
        return false;

    SDL_memset(capture, 0, sizeof(*capture));
    SDL_memset(&scratch, 0, sizeof(scratch));

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 20)
        term_wid = 20;
    (void)term_hgt;

    if (term_init(&scratch, term_wid, 255, 16) != 0)
        goto cleanup;
    scratch_ready = true;

    Term_activate(&scratch);
    story_font_term_push(use_story_font, false, &story_state);
    story_pushed = true;

    text_out_hook = text_out_to_screen;
    text_out_wrap = (term_wid > 2) ? (term_wid - 1) : 0;
    text_out_indent = 0;

    Term_clear();
    roff_top_live(r_idx, m_ptr);
    Term_gotoxy(0, 1);
    Term_erase(0, 1, 255);
    describe_monster(r_idx, false, m_ptr);

    used_rows = monster_recall_screen_capture_used_rows(Term);
    if (used_rows < 1)
        used_rows = 1;
    used_cols = monster_recall_screen_capture_used_cols(Term, used_rows);
    if (used_cols < 1)
        used_cols = 1;

    capture->width = used_cols;
    capture->height = used_rows;
    capture->attrs = mem_alloc_array(capture->width * capture->height, byte);
    capture->chars = mem_alloc_array(capture->width * capture->height, char);
    capture->tattrs = mem_alloc_array(capture->width * capture->height, byte);
    capture->tchars = mem_alloc_array(capture->width * capture->height, char);
    capture->story = mem_alloc_array(capture->width * capture->height, byte);

    for (int y = 0; y < capture->height; y++)
    {
        for (int x = 0; x < capture->width; x++)
        {
            int idx = y * capture->width + x;
            capture->attrs[idx] = scratch.scr->a[y][x];
            capture->chars[idx] = scratch.scr->c[y][x];
            capture->tattrs[idx] = scratch.scr->ta[y][x];
            capture->tchars[idx] = scratch.scr->tc[y][x];
            capture->story[idx] = scratch.scr->story[y][x];
        }
    }

    success = true;

cleanup:
    text_out_hook = old_hook;
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    if (story_pushed)
        story_font_term_pop(&story_state);

    if (saved_term && Term != saved_term)
        Term_activate(saved_term);

    if (scratch_ready)
        term_nuke(&scratch);

    if (!success)
        monster_recall_screen_capture_free(capture);

    return success;
}

static int monster_recall_screen_capture_view(
    const monster_recall_screen_capture* capture)
{
    int scroll = 0;
    int exit_key = ESCAPE;
    bool overlay_active = false;

    if (!capture)
        return 0;

    while (true)
    {
        int term_hgt = 24;
        int visible_rows = 1;
        int max_scroll = 0;
        int dir;
        char ch;

        Term_get_size(NULL, &term_hgt);
        if (!sdl_description_overlay_present(capture->attrs, capture->chars,
                capture->tattrs, capture->tchars, capture->story,
                capture->width, capture->height, scroll, &visible_rows,
                &max_scroll))
        {
            exit_key = 0;
            break;
        }
        overlay_active = true;

        if (scroll > max_scroll)
            scroll = max_scroll;

        ui_scroll_area_begin(0, MAX(0, term_hgt - 1),
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        ui_scroll_area_set_tap_key(ESCAPE);

        ch = inkey();
        dir = target_dir(ch);
        if ((dir == 8) || (dir == 2))
            ch = I2D(dir);

        if ((ch == '8') || (ch == '='))
        {
            if (scroll > 0)
                scroll--;
        }
        else if ((ch == '2') || (ch == '\n') || (ch == '\r'))
        {
            if (scroll < max_scroll)
                scroll++;
        }
        else if ((ch == '3') || (ch == ' '))
        {
            scroll += visible_rows;
            if (scroll > max_scroll)
                scroll = max_scroll;
        }
        else if ((ch == '9') || (ch == '-'))
        {
            scroll -= visible_rows;
            if (scroll < 0)
                scroll = 0;
        }
        else
        {
            exit_key = ch;
            break;
        }
    }

    if (overlay_active)
        sdl_description_overlay_clear();
    ui_scroll_area_clear();
    return exit_key;
}

static void monster_recall_screen_draw_plain(
    int r_idx, const monster_type* m_ptr)
{
    bool use_story_font = story_monster_desc_enabled();
    story_font_term_state story_state;
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_indent = text_out_indent;
    int old_wrap = text_out_wrap;
    int wid = 0;
    int hgt = 0;

    story_font_term_push(use_story_font, false, &story_state);

    /* Begin recall */
    Term_erase(0, 1, 255);
    Term_gotoxy(0, 1);

    Term_get_size(&wid, &hgt);
    (void)hgt;
    text_out_indent = 0;
    text_out_wrap = (wid > 2) ? (wid - 1) : 0;
    text_out_hook = text_out_to_screen;

    /* Recall monster */
    describe_monster(r_idx, false, m_ptr);

    /* Describe monster */
    roff_top_live(r_idx, m_ptr);

    if (Term == term_screen)
        ui_key_wait_dismiss_begin('\r');

    text_out_hook = old_hook;
    text_out_indent = old_indent;
    text_out_wrap = old_wrap;

    story_font_term_pop(&story_state);
}

/*
 * Hack -- describe the given monster race at the top of the screen
 *
 * Returns the key that dismissed the scrollable recall view, or zero when
 * the legacy one-screen display was used and the caller should still wait.
 */
int screen_roff(int r_idx, const monster_type* m_ptr)
{
    monster_recall_screen_capture capture;
    bool have_capture;

    if (monster_recall_blocked_by_hallucination())
    {
        monster_recall_show_hallucination_block();
        return ESCAPE;
    }

    /* Flush messages */
    message_flush();

    have_capture = monster_recall_screen_capture_build(r_idx, m_ptr, &capture);
    if (have_capture)
    {
        int exit_key = monster_recall_screen_capture_view(&capture);

        monster_recall_screen_capture_free(&capture);
        if (exit_key)
            return exit_key;
    }

    monster_recall_screen_draw_plain(r_idx, m_ptr);

    return 0;
}

/*
 * Hack -- describe the given monster race in the current "term" window
 */
void display_roff(int r_idx, const monster_type* m_ptr)
{
    int y;

    if (monster_recall_blocked_by_hallucination())
    {
        for (y = 0; y < Term->hgt; y++)
        {
            Term_erase(0, y, 255);
        }
        return;
    }

    bool use_story_font = story_monster_desc_enabled();
    story_font_term_state story_state;
    story_font_term_push(use_story_font, false, &story_state);

    /* Erase the window */
    for (y = 0; y < Term->hgt; y++)
    {
        /* Erase the line */
        Term_erase(0, y, 255);
    }

    /* Begin recall */
    Term_gotoxy(0, 1);

    /* Output to the screen */
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_indent = text_out_indent;
    int old_wrap = text_out_wrap;

    int wid = 0, hgt = 0;
    Term_get_size(&wid, &hgt);
    text_out_indent = 0;
    text_out_wrap = (wid > 2) ? (wid - 1) : 0;
    text_out_hook = text_out_to_screen;

    /* Recall monster */
    describe_monster(r_idx, false, m_ptr);

    /* Describe monster */
    roff_top_live(r_idx, m_ptr);

    text_out_hook = old_hook;
    text_out_indent = old_indent;
    text_out_wrap = old_wrap;

    story_font_term_pop(&story_state);
}
