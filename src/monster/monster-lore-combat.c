/* File: monster-lore-combat.c */

#include "monster-internal.h"

void describe_monster_spells(int r_idx, const monster_lore* l_ptr)
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

void describe_monster_drop(int r_idx, const monster_lore* l_ptr)
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
            /* Territorial monsters do not carry loot off; they keep it.
             * An artefact-bearer hoards its treasure rather than guarding
             * a scattering of lesser finds. */
            if (l_ptr->flags3 & (RF3_DROP_ARTEFACT))
                text_out(format("%^s hoards", wd_he[msex]));
            else
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

void describe_monster_attack(
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

void describe_monster_abilities(int r_idx, const monster_lore* l_ptr)
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

