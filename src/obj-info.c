/* File: obj-info.c */

/*
 * Copyright (c) 2002 Andrew Sidwell, Robert Ruehlmann
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"

/* true if a paragraph break should be output before next p_text_out() */
static bool new_paragraph = false;

static void p_text_out(cptr str)
{
    if (new_paragraph)
    {
        text_out("\n\n   ");
        new_paragraph = false;
    }

    text_out(str);
}

/* Color-coded text output for object descriptions */
static void p_text_out_c(byte attr, cptr str)
{
    if (new_paragraph)
    {
        text_out("\n\n   ");
        new_paragraph = false;
    }

    text_out_c(attr, str);
}

static void output_list(cptr list[], int n)
{
    int i;

    char* conjunction = "and ";
    if (n < 0)
    {
        n = -n;
        conjunction = "or ";
    }

    for (i = 0; i < n; i++)
    {
        if (i != 0)

        {
            p_text_out((i == 1 && i == n - 1) ? " " : ", ");
            if (i == n - 1)
                p_text_out(conjunction);
        }
        p_text_out(list[i]);
    }
}

/* Colored version of output_list */
static void output_list_c(cptr list[], int n, byte attr)
{
    int i;

    char* conjunction = "and ";
    if (n < 0)
    {
        n = -n;
        conjunction = "or ";
    }

    for (i = 0; i < n; i++)
    {
        if (i != 0)
        {
            p_text_out((i == 1 && i == n - 1) ? " " : ", ");
            if (i == n - 1)
                p_text_out(conjunction);
        }
        p_text_out_c(attr, list[i]);
    }
}

static void output_desc_list(cptr intro, cptr list[], int n)
{
    if (n != 0)
    {
        /* Output intro */
        p_text_out(intro);

        /* Output list */
        output_list(list, n);

        /* Output end */
        p_text_out(".  ");
    }
}

/*
 * Describe stat modifications.
 */
static bool describe_stats(const object_type* o_ptr, u32b f1)
{
    cptr descs[A_MAX];
    int cnt = 0;
    int pval = (o_ptr->pval > 0 ? o_ptr->pval : -o_ptr->pval);

    /* Abort if the pval is zero */
    // if (!pval) return (false);

    /* Collect stat bonuses */
    if (f1 & (TR1_STR))
        descs[cnt++] = stat_names_full[A_STR];
    if (f1 & (TR1_DEX))
        descs[cnt++] = stat_names_full[A_DEX];
    if (f1 & (TR1_CON))
        descs[cnt++] = stat_names_full[A_CON];
    if (f1 & (TR1_GRA))
        descs[cnt++] = stat_names_full[A_GRA];

    /* Skip */
    if (cnt == 0)
        return (false);

    /* Shorten to "all stats", if appropriate. */
    if (cnt == A_MAX)
    {
        p_text_out("It ");
        p_text_out_c(o_ptr->pval >= 0 ? TERM_GREEN : TERM_L_RED,
            (o_ptr->pval >= 0 ? "increases" : "decreases"));
        p_text_out(" all your stats");
    }
    else
    {
        p_text_out("It ");
        p_text_out_c(o_ptr->pval >= 0 ? TERM_GREEN : TERM_L_RED,
            (o_ptr->pval >= 0 ? "increases" : "decreases"));
        p_text_out(" your ");

        /* Output list */
        output_list(descs, cnt);
    }

    /* Output end */
    p_text_out(" by ");
    p_text_out_c(TERM_UMBER, format("%i", pval));
    p_text_out(".  ");

    /* We found something */
    return (true);
}

/*
 * Describe reversed stat modifications.
 */
static bool describe_neg_stats(const object_type* o_ptr, u32b f1)
{
    cptr descs[A_MAX];
    int cnt = 0;
    int pval = (o_ptr->pval > 0 ? o_ptr->pval : -o_ptr->pval);

    /* Abort if the pval is zero */
    // if (!pval) return (false);

    /* Collect stat bonuses */
    if (f1 & (TR1_NEG_STR))
        descs[cnt++] = stat_names_full[A_STR];
    if (f1 & (TR1_NEG_DEX))
        descs[cnt++] = stat_names_full[A_DEX];
    if (f1 & (TR1_NEG_CON))
        descs[cnt++] = stat_names_full[A_CON];
    if (f1 & (TR1_NEG_GRA))
        descs[cnt++] = stat_names_full[A_GRA];

    /* Skip */
    if (cnt == 0)
        return (false);

    /* Shorten to "all stats", if appropriate. */
    if (cnt == A_MAX)
    {
        p_text_out(format("It %s all your stats",
            (o_ptr->pval < 0 ? "increases" : "decreases")));
    }
    else
    {
        p_text_out(format(
            "It %s your ", (o_ptr->pval < 0 ? "increases" : "decreases")));

        /* Output list */
        output_list(descs, cnt);
    }

    /* Output end */
    p_text_out(format(" by %i.  ", pval));

    /* We found something */
    return (true);
}

/*
 * Describe "secondary bonuses" of an item.
 */
static bool describe_secondary(const object_type* o_ptr, u32b f1)
{
    cptr descs[8];
    int cnt = 0;
    int pval = (o_ptr->pval > 0 ? o_ptr->pval : -o_ptr->pval);

    /* Collect */
    if (f1 & (TR1_MEL))
        descs[cnt++] = "melee";
    if (f1 & (TR1_ARC))
        descs[cnt++] = "archery";
    if (f1 & (TR1_STL))
        descs[cnt++] = "stealth";
    if (f1 & (TR1_PER))
        descs[cnt++] = "perception";
    if (f1 & (TR1_WIL))
        descs[cnt++] = "will";
    if (f1 & (TR1_SMT))
        descs[cnt++] = "smithing";
    if (f1 & (TR1_SNG))
        descs[cnt++] = "song";
    if (f1 & (TR1_TUNNEL))
        descs[cnt++] = "tunneling";
    if (f1 & (TR1_DAMAGE_SIDES))
        descs[cnt++] = "damage sides";

    /* Skip */
    if (!cnt)
        return (false);

    /* Start */
    p_text_out("It ");
    p_text_out_c(o_ptr->pval >= 0 ? TERM_GREEN : TERM_L_RED,
        (o_ptr->pval >= 0 ? "improves" : "worsens"));
    p_text_out(" your ");

    /* Output list */
    output_list(descs, cnt);

    /* Output end */
    p_text_out(" by ");
    p_text_out_c(TERM_UMBER, format("%i", pval));
    p_text_out(".  ");

    /* We found something */
    return (true);
}

/*
 * Describe the special slays and executes of an item.
 */
static bool describe_slay(const object_type* o_ptr, u32b f1)
{
    cptr slays[8];
    int slcnt = 0;

    /* Unused parameter */
    (void)o_ptr;

    /* Collect brands */
    if (f1 & (TR1_SLAY_WOLF))
        slays[slcnt++] = "wolves";
    if (f1 & (TR1_SLAY_ORC))
        slays[slcnt++] = "orcs";
    if (f1 & (TR1_SLAY_TROLL))
        slays[slcnt++] = "trolls";
    if (f1 & (TR1_SLAY_SPIDER))
        slays[slcnt++] = "spiders";
    if (f1 & (TR1_SLAY_DRAGON))
        slays[slcnt++] = "dragons";
    if (f1 & (TR1_SLAY_RAUKO))
        slays[slcnt++] = "raukar";
    if (f1 & (TR1_SLAY_UNDEAD))
        slays[slcnt++] = "undead";
    if (f1 & (TR1_SLAY_MAN_OR_ELF))
    {
        slays[slcnt++] = "men";
        slays[slcnt++] = "elves";
    }

    /* Describe */
    if (slcnt)
    {
        if (o_ptr->number == 1)
        {
            /* Output intro */
            p_text_out("It ");
            p_text_out_c(TERM_L_RED, "slays");
            p_text_out(" ");
        }
        else
        {
            /* Output intro */
            p_text_out("They ");
            p_text_out_c(TERM_L_RED, "slay");
            p_text_out(" ");
        }

        /* Output list in red */
        output_list_c(slays, slcnt, TERM_ORANGE);

        /* Output end */
        p_text_out(".  ");
    }

    /* We are done here */
    return ((slcnt) ? true : false);
}

/*
 * Describe elemental brands.
 */
static bool describe_brand(const object_type* o_ptr, u32b f1)
{
    cptr descs[5];
    byte colors[5];  /* Corresponding colors for each brand */
    int cnt = 0;

    /* Unused parameter */
    (void)o_ptr;

    /* Collect brands with element-specific colors */
    if (f1 & (TR1_BRAND_ELEC))
    {
        descs[cnt] = "lightning";
        colors[cnt] = TERM_YELLOW;  /* Yellow for lightning */
        cnt++;
    }
    if (f1 & (TR1_BRAND_FIRE))
    {
        descs[cnt] = "flame";
        colors[cnt] = TERM_L_RED;  /* Light red for fire */
        cnt++;
    }
    if (f1 & (TR1_BRAND_COLD))
    {
        descs[cnt] = "frost";
        colors[cnt] = TERM_L_BLUE;  /* Light blue for cold */
        cnt++;
    }
    if (f1 & (TR1_BRAND_POIS))
    {
        descs[cnt] = "venom";
        colors[cnt] = TERM_GREEN;  /* Green for poison */
        cnt++;
    }

    /* Describe brands with colors */
    if (cnt)
    {
        int i;
        
        if (o_ptr->number == 1)
        {
            p_text_out("It is ");
            p_text_out_c(TERM_ORANGE, "branded");
            p_text_out(" with ");
        }
        else
        {
            p_text_out("They are ");
            p_text_out_c(TERM_ORANGE, "branded");
            p_text_out(" with ");
        }
        
        for (i = 0; i < cnt; i++)
        {
            if (i != 0)
            {
                p_text_out((i == 1 && i == cnt - 1) ? " " : ", ");
                if (i == cnt - 1)
                    p_text_out("and ");
            }
            p_text_out_c(colors[i], descs[i]);
        }
        p_text_out(".  ");
    }

    /* We are done here */
    return (cnt ? true : false);
}

/*
 * Describe misc weapon attributes.
 */
static bool describe_misc_weapon_attributes(
    const object_type* o_ptr, u32b f1, u32b f3)
{
    bool message = false;

    if (f1 & (TR1_SHARPNESS))
    {
        if (o_ptr->number == 1)
            p_text_out("It cuts easily through armour.  ");
        else
            p_text_out("They cut easily through armour.  ");
        message = true;
    }
    if (f1 & (TR1_SHARPNESS2))
    {
        p_text_out("It cuts very easily through armour.  ");
        message = true;
    }
    if (f1 & (TR1_VAMPIRIC))
    {
        p_text_out("It drains life from your enemies.  ");
        message = true;
    }
    if (f3 & (TR3_ACCURATE))
    {
        if (o_ptr->tval == TV_BOW)
        {
            p_text_out("It fires arrows with unerring precision (misses are "
                       "rerolled).  ");
            message = true;
        }
        else
        {
            p_text_out(
                "It is unusually well balanced (misses are rerolled).  ");
            message = true;
        }
    }
    if (f3 & (TR3_CUMBERSOME))
    {
        p_text_out("It is cumbersome (it does not score critical hits).  ");
        message = true;
    }

    return (message);
}

/*
 * Describe resistances granted by an object.
 */
static bool describe_resist(const object_type* o_ptr, u32b f2)
{
    cptr vp[17];
    byte colors[17];  /* Corresponding colors for each resistance */
    int vn = 0;

    /* Unused parameter */
    (void)o_ptr;

    /* Collect resistances with element-specific colors */
    if (f2 & (TR2_RES_COLD))
    {
        vp[vn] = "cold";
        colors[vn] = TERM_L_BLUE;  /* Light blue for cold */
        vn++;
    }
    if (f2 & (TR2_RES_FIRE))
    {
        vp[vn] = "fire";
        colors[vn] = TERM_L_RED;  /* Light red for fire */
        vn++;
    }
    if (f2 & (TR2_RES_ELEC))
    {
        vp[vn] = "lightning";
        colors[vn] = TERM_YELLOW;  /* Yellow for lightning */
        vn++;
    }
    if (f2 & (TR2_RES_POIS))
    {
        vp[vn] = "poison";
        colors[vn] = TERM_GREEN;  /* Green for poison */
        vn++;
    }
    if (f2 & (TR2_RES_BLEED))
    {
        vp[vn] = "bleeding";
        colors[vn] = TERM_RED;  /* Red for bleeding */
        vn++;
    }

    if (f2 & (TR2_RES_FEAR))
    {
        vp[vn] = "fear";
        colors[vn] = TERM_VIOLET;  /* Violet for fear */
        vn++;
    }
    if (f2 & (TR2_RES_BLIND))
    {
        vp[vn] = "blindness";
        colors[vn] = TERM_L_DARK;  /* Dark for blindness */
        vn++;
    }
    if (f2 & (TR2_RES_CONFU))
    {
        vp[vn] = "confusion";
        colors[vn] = TERM_VIOLET;  /* Violet for confusion */
        vn++;
    }
    if (f2 & (TR2_RES_STUN))
    {
        vp[vn] = "stunning";
        colors[vn] = TERM_ORANGE;  /* Orange for stunning */
        vn++;
    }
    if (f2 & (TR2_RES_HALLU))
    {
        vp[vn] = "hallucination";
        colors[vn] = TERM_VIOLET;  /* Violet for hallucination */
        vn++;
    }

    /* Describe resistances with colors */
    if (vn)
    {
        int i;
        p_text_out("It provides ");
        p_text_out_c(TERM_L_BLUE, "resistance");
        p_text_out(" to ");
        
        for (i = 0; i < vn; i++)
        {
            if (i != 0)
            {
                p_text_out((i == 1 && i == vn - 1) ? " " : ", ");
                if (i == vn - 1)
                    p_text_out("and ");
            }
            p_text_out_c(colors[i], vp[i]);
        }
        p_text_out(".  ");
    }

    /* We are done here */
    return (vn ? true : false);
}

/*
 * Describe resistances granted by an object.
 */
static bool describe_vulnerability(const object_type* o_ptr, u32b f2)
{
    cptr vp[17];
    byte colors[17];  /* Corresponding colors for each vulnerability */
    int vn = 0;

    /* Unused parameter */
    (void)o_ptr;

    /* Collect vulnerabilities with element-specific colors */
    if (f2 & (TR2_VUL_COLD))
    {
        vp[vn] = "cold";
        colors[vn] = TERM_L_BLUE;  /* Light blue for cold */
        vn++;
    }
    if (f2 & (TR2_VUL_FIRE))
    {
        vp[vn] = "fire";
        colors[vn] = TERM_L_RED;  /* Light red for fire */
        vn++;
    }
    if (f2 & (TR2_VUL_POIS))
    {
        vp[vn] = "poison";
        colors[vn] = TERM_GREEN;  /* Green for poison */
        vn++;
    }

    /* Describe vulnerabilities with colors */
    if (vn)
    {
        int i;
        p_text_out("It makes you more ");
        p_text_out_c(TERM_RED, "vulnerable");
        p_text_out(" to ");
        
        for (i = 0; i < vn; i++)
        {
            if (i != 0)
            {
                p_text_out((i == 1 && i == vn - 1) ? " " : ", ");
                if (i == vn - 1)
                    p_text_out("and ");
            }
            p_text_out_c(colors[i], vp[i]);
        }
        p_text_out(".  ");
    }

    /* We are done here */
    return (vn ? true : false);
}

/*
 * Describe the 'handedness' of a weapon
 */
static bool describe_handedness(const object_type* o_ptr, u32b f3)
{
    int n = o_ptr->tval;

    if ((n == TV_DIGGING) || (n == TV_HAFTED) || (n == TV_POLEARM)
        || (n == TV_SWORD))
    {
        if (f3 & (TR3_HAND_AND_A_HALF))
            p_text_out("It does extra damage when wielded with both hands.  ");
        else if (f3 & (TR3_TWO_HANDED))
            p_text_out("It requires both hands to wield it properly.  ");
        else
            return (false);

        return (true);
    }

    return (false);
}

/*
 * Describe the 'polearmness' of a weapon
 */
static bool describe_polearmness(u32b f3)
{
    if (f3 & (TR3_POLEARM))
    {
        p_text_out("It counts as a type of polearm.  ");
        return (true);
    }

    return (false);
}

/*
 * Describe 'ignores' of an object.
 */
static bool describe_ignores(const object_type* o_ptr, u32b f3)
{
    //	cptr list[4];
    int n = 0;

    /* Unused parameter */
    (void)o_ptr;

    /* Collect the ignores */
    //	if ((f3 & (TR3_IGNORE_ACID)) && hates_acid(o_ptr)) list[n++] = "acid";
    //	if ((f3 & (TR3_IGNORE_ELEC)) && hates_elec(o_ptr)) list[n++] =
    //"electricity"; 	if ((f3 & (TR3_IGNORE_FIRE)) && hates_fire(o_ptr))
    // list[n++] = "fire"; 	if ((f3 & (TR3_IGNORE_COLD)) &&
    // hates_cold(o_ptr)) list[n++] = "cold";

    /* Describe ignores */
    if ((f3 & (TR3_IGNORE_ACID)) && (f3 & (TR3_IGNORE_FIRE))
        && (f3 & (TR3_IGNORE_COLD)))
    {
        p_text_out("It cannot be harmed by the elements.  ");

        return (true);
    }
    //	else
    //		output_desc_list("It cannot be harmed by ", list, - n);
    //
    return ((n > 0) ? true : false);
}

/*
 * Describe stat sustains.
 */
static bool describe_sustains(const object_type* o_ptr, u32b f2)
{
    cptr list[A_MAX];
    int n = 0;

    /* Unused parameter */
    (void)o_ptr;

    /* Collect the sustains */
    if (f2 & (TR2_SUST_STR))
        list[n++] = stat_names_full[A_STR];
    if (f2 & (TR2_SUST_DEX))
        list[n++] = stat_names_full[A_DEX];
    if (f2 & (TR2_SUST_CON))
        list[n++] = stat_names_full[A_CON];
    if (f2 & (TR2_SUST_GRA))
        list[n++] = stat_names_full[A_GRA];

    /* Describe immunities */
    if (n == A_MAX)
        p_text_out("It sustains all your stats.  ");
    else
        output_desc_list("It sustains your ", list, n);

    /* We are done here */
    return (n ? true : false);
}

/*
 * Describe miscellaneous powers such as see invisible, free action,
 * permanent light, etc; also note curses and penalties.
 */
static bool describe_misc_magic(const object_type* o_ptr, u32b f2, u32b f3)
{
    cptr good[7], bad[6];
    int gc = 0, bc = 0;
    bool something = false;

    /* Throwing weapons. */
    if (f3 & (TR3_THROWING))
    {
        good[gc++] = (format(
            "can be thrown effectively (%d squares)", throwing_range(o_ptr)));
        good[gc++] = "can be placed in quiver (passive abilities remain active for 2nd quiver)";
    }

    /* Collect stuff which can't be categorized */
    if (((o_ptr->tval == TV_LIGHT) && artefact_p(o_ptr))
        || ((o_ptr->tval != TV_LIGHT) && (f2 & (TR2_LIGHT))))
        good[gc++] = "lights the dungeon around you";
    if ((f2 & (TR2_LIGHT)) && (o_ptr->tval == TV_LIGHT))
        good[gc++] = "burns brightly, increasing your light radius by an "
                     "additional square";
    if (f2 & (TR2_SLOW_DIGEST))
        good[gc++] = "reduces your need for food";
    if ((f2 & (TR2_RADIANCE)) && (o_ptr->tval == TV_BOW))
        good[gc++] = "fires shining arrows";
    if ((f2 & (TR2_RADIANCE)) && (o_ptr->tval == TV_BOOTS))
        good[gc++] = "lights your path behind you";
    if (f2 & (TR2_REGEN))
        good[gc++] = "speeds your regeneration (which increases your hunger "
                     "while active)";
    if (f3 & (TR3_CHEAT_DEATH))
        good[gc++] = "preserves you from death once";
    if (f3 & (TR3_STAND_FAST))
        good[gc++] = "lets you stand fast against your foes";
    if (f3 & (TR3_AVOID_TRAPS))
        good[gc++] = "lets you step on traps without triggering them";
    if (f3 & (TR3_MEDIC))
        good[gc++] = "increases the health you get from healing items";

    /* Describe */
    output_desc_list("It ", good, gc);

    /* Set "something" */
    if (gc)
        something = true;

    /* Collect granted powers */
    gc = 0;
    if (f2 & (TR2_SPEED))
        good[gc++] = "great speed";
    if (f2 & (TR2_FREE_ACT))
        good[gc++] = "freedom of movement";
    if (f2 & (TR2_SEE_INVIS))
        good[gc++] = "the ability to see invisible creatures";

    /* Collect penalties */
    if (f2 & (TR2_DANGER))
        bad[bc++] = "makes you encounter more dangerous creatures (even when "
                    "not worn)";
    if (f2 & (TR2_FEAR))
        bad[bc++] = "causes you to panic in combat";
    if (f2 & (TR2_HUNGER))
        bad[bc++] = "increases your hunger";
    if (f2 & (TR2_DARKNESS))
        bad[bc++] = "creates an unnatural darkness";
    if (f2 & (TR2_SLOWNESS))
        bad[bc++] = "slows your movement";
    if (f2 & (TR2_AGGRAVATE))
        bad[bc++] = "enrages nearby creatures";
    if (f2 & (TR2_HAUNTED))
        bad[bc++] = "draws wraiths to your level";
    if (f2 & (TR2_TRAITOR))
        bad[bc++] = "may betray you when you need it most";

    /* Deal with cursed stuff */
    if (cursed_p(o_ptr))
    {
        if (f3 & (TR3_PERMA_CURSE))
            bad[bc++] = "permanently cursed";
        else if (f3 & (TR3_HEAVY_CURSE))
            bad[bc++] = "heavily cursed";
        else if (object_known_p(o_ptr))
            bad[bc++] = "cursed";
    }

    /* Describe */
    if (gc)
    {
        /* Output intro */
        p_text_out("It ");
        p_text_out_c(TERM_L_BLUE, "grants");
        p_text_out(" you ");

        /* Output list in light blue */
        output_list_c(good, gc, TERM_L_BLUE);

        /* Output end (if needed) */
        if (!bc)
            p_text_out(".  ");
    }

    if (bc)
    {
        /* Output intro */
        if (gc)
            p_text_out(", but it also ");
        else
            p_text_out("It ");
        
        /* Check if any curse-related items */
        bool has_curse = false;
        int i;
        for (i = 0; i < bc; i++)
        {
            if (strstr(bad[i], "cursed"))
            {
                has_curse = true;
                break;
            }
        }

        /* Output list in red for bad effects, violet for curses */
        if (has_curse)
        {
            /* Special handling for curse descriptions */
            for (i = 0; i < bc; i++)
            {
                if (i != 0)
                {
                    p_text_out((i == 1 && i == bc - 1) ? " " : ", ");
                    if (i == bc - 1)
                        p_text_out("and ");
                }
                
                /* Color curse-related text in violet */
                if (strstr(bad[i], "cursed"))
                {
                    if (strstr(bad[i], "permanently"))
                    {
                        p_text_out("is ");
                        p_text_out_c(TERM_VIOLET, "permanently cursed");
                    }
                    else if (strstr(bad[i], "heavily"))
                    {
                        p_text_out("is ");
                        p_text_out_c(TERM_VIOLET, "heavily cursed");
                    }
                    else
                    {
                        p_text_out("is ");
                        p_text_out_c(TERM_VIOLET, "cursed");
                    }
                }
                else
                {
                    /* Color darkness in dark gray/violet */
                    if (strstr(bad[i], "darkness"))
                    {
                        p_text_out("creates an unnatural ");
                        p_text_out_c(TERM_L_DARK, "darkness");
                    }
                    /* Color fear/panic in violet */
                    else if (strstr(bad[i], "panic"))
                    {
                        p_text_out("causes you to ");
                        p_text_out_c(TERM_VIOLET, "panic");
                        p_text_out(" in combat");
                    }
                    /* Default red for other bad effects */
                    else
                    {
                        p_text_out_c(TERM_L_RED, bad[i]);
                    }
                }
            }
        }
        else
        {
            /* No curses, handle special coloring for bad effects */
            int i;
            for (i = 0; i < bc; i++)
            {
                if (i != 0)
                {
                    p_text_out((i == 1 && i == bc - 1) ? " " : ", ");
                    if (i == bc - 1)
                        p_text_out("and ");
                }
                
                /* Color darkness in dark gray */
                if (strstr(bad[i], "darkness"))
                {
                    p_text_out("creates an unnatural ");
                    p_text_out_c(TERM_L_DARK, "darkness");
                }
                /* Color fear/panic in violet */
                else if (strstr(bad[i], "panic"))
                {
                    p_text_out("causes you to ");
                    p_text_out_c(TERM_VIOLET, "panic");
                    p_text_out(" in combat");
                }
                /* Default red for other bad effects */
                else
                {
                    p_text_out_c(TERM_L_RED, bad[i]);
                }
            }
        }

        /* Output end */
        p_text_out(".  ");
    }

    /* Set "something" */
    if (gc || bc)
        something = true;

    /* Return "something" */
    return (something);
}

static cptr act_description[ACT_MAX] = { "illumination", "magic mapping",
    "clairvoyance", "protection from evil", "dispel evil (x5)", "heal (500)",
    "heal (1000)", "cure wounds (4d7)", "haste self (20+d20 turns)",
    "haste self (75+d75 turns)", "fire bolt (9d8)", "fire ball (72)",
    "large fire ball (120)", "frost bolt (6d8)", "frost ball (48)",
    "frost ball (100)", "frost bolt (12d8)", "large frost ball (200)",
    "acid bolt (5d8)", "recharge item I", "sleep II", "lightning bolt (4d8)",
    "large lightning ball (250)", "banishment", "mass banishment", "*identify*",
    "drain life (90)", "drain life (120)", "bizarre things", "star ball (150)",
    "berserk rage, bless, and resistance", "phase door",
    "door and trap destruction", "detection", "resistance (20+d20 turns)",
    "teleport", "restore voice", "magic missile (2d6)", "a magical arrow (150)",
    "remove fear and cure poison", "stinking cloud (12)", "stone to mud",
    "teleport away", "word of recall", "confuse monster", "probing",
    "fire branding of bolts", "starlight (10d8)", "mana bolt (12d8)",
    "berserk rage (50+d50 turns)", "resist acid (20+d20 turns)",
    "resist electricity (20+d20 turns)", "resist fire (20+d20 turns)",
    "resist cold (20+d20 turns)", "resist poison (20+d20 turns)" };

/*
 * Determine the "Activation" (if any) for an artefact
 */
static void describe_item_activation(
    const object_type* o_ptr, char* random_name, size_t max)
{
    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Require activation ability */
    if (!(f3 & TR3_ACTIVATE))
        return;

    /* Artefact activations */
    if ((o_ptr->name1) && (o_ptr->name1 < z_info->art_norm_max))
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        /* Paranoia */
        if (a_ptr->activation >= ACT_MAX)
        {
            return;
        }

        /* Some artefacts can be activated */
        SDL_strlcat(random_name, act_description[a_ptr->activation], max);

        /* Output the number of turns */
        if (a_ptr->time && a_ptr->randtime)
            SDL_strlcat(random_name,
                format(" every %d+d%d turns", a_ptr->time, a_ptr->randtime),
                max);
        else if (a_ptr->time)
            SDL_strlcat(random_name, format(" every %d turns", a_ptr->time), max);
        else if (a_ptr->randtime)
            SDL_strlcat(
                random_name, format(" every d%d turns", a_ptr->randtime), max);

        return;
    }
}

/*
 * Describe an object's activation, if any.
 */
static bool describe_activation(const object_type* o_ptr, u32b f3)
{
    /* Check for the activation flag */
    if (f3 & TR3_ACTIVATE)
    {
        char act_desc[120];

        u16b size;

        SDL_strlcpy(act_desc, "It activates for ", sizeof(act_desc));

        /*get the size of the file*/
        size = strlen(act_desc);

        describe_item_activation(o_ptr, act_desc, sizeof(act_desc));

        /*if the previous function added length, we have an activation, so print
         * it out*/
        if (strlen(act_desc) > size)
        {
            SDL_strlcat(act_desc, format(".  "), sizeof(act_desc));

            /*print it out*/
            p_text_out(act_desc);

            return (true);
        }
    }

    /* No activation */
    return (false);
}

/*
 * Describe abilities granted by an object.
 */
static bool describe_abilities(const object_type* o_ptr)
{
    cptr ability[8];
    int ac = 0;
    ability_type* b_ptr;
    int i;

    // only describe when identified
    if (!object_known_p(o_ptr) && !(o_ptr->ident & (IDENT_SPOIL)))
        return (false);

    // check its abilities
    for (i = 0; i < o_ptr->abilities; i++)
    {
        b_ptr
            = &b_info[ability_index(o_ptr->skilltype[i], o_ptr->abilitynum[i])];
        ability[ac++] = b_name + b_ptr->name;
    }

    /* Describe */
    if (ac)
    {
        /* Output intro */
        if (ac == 1)
            p_text_out("It ");
        else
            p_text_out("It ");
        
        p_text_out_c(TERM_L_BLUE, "grants");
        p_text_out(" you the ");
        p_text_out_c(TERM_L_BLUE, (ac == 1 ? "ability" : "abilities"));
        p_text_out(": ");

        /* Output list in light blue */
        output_list_c(ability, ac, TERM_VIOLET);

        /* Output end (if needed) */
        p_text_out(".  ");

        /* It granted abilities */
        return (true);
    }

    /* No abilities granted */
    return (false);
}

/*
 * Describe attributes of bows and arrows.
 */
static bool describe_archery(const object_type* o_ptr)
{
    if (o_ptr->tval == TV_BOW)
    {
        p_text_out(format(
            "It can shoot arrows %d squares (with your current strength).",
            archery_range(o_ptr)));
        return (true);
    }
    if (o_ptr->tval == TV_ARROW)
    {
        if ((&inventory[INVEN_BOW])->k_idx)
        {
            if (o_ptr->number == 1)
            {
                p_text_out(format("It can be shot %d squares (with your "
                                  "current strength and bow).",
                    archery_range(&inventory[INVEN_BOW])));
            }
            else
            {
                p_text_out(format("They can be shot %d squares (with your "
                                  "current strength and bow).",
                    archery_range(&inventory[INVEN_BOW])));
            }
        }
        else
        {
            if (o_ptr->number == 1)
            {
                p_text_out("It can be shot by a bow.");
            }
            else
            {
                p_text_out("They can be shot by a bow.");
            }
        }
        return (true);
    }

    /* Not archery related */
    return (false);
}

/*
 * Describe weapon damage with current strength modifiers
 */
static bool describe_weapon_damage(const object_type* o_ptr)
{
    byte base_dd, base_ds, actual_dd, actual_ds;
    u32b f1, f2, f3;
    bool is_melee = false;
    bool is_bow = false;
    bool is_hand_and_half = false;
    
    /* Check if this is a melee weapon */
    if (o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM 
        || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING)
    {
        is_melee = true;
    }
    /* Check if this is a bow */
    else if (o_ptr->tval == TV_BOW)
    {
        is_bow = true;
    }
    else
    {
        return (false);
    }

    /* Get object flags to check for hand-and-a-half */
    object_flags(o_ptr, &f1, &f2, &f3);
    is_hand_and_half = (f3 & TR3_HAND_AND_A_HALF) ? true : false;

    if (is_melee)
    {
        /* Calculate melee damage with current strength */
        base_dd = o_ptr->dd;
        base_ds = o_ptr->ds;
        actual_dd = total_mdd(o_ptr);
        actual_ds = total_mds(o_ptr, 0);

        /* Special handling for hand-and-a-half weapons */
        if (is_hand_and_half)
        {
            int hand_half_bonus_equipped = hand_and_a_half_bonus(o_ptr);
            int hand_half_bonus_potential;
            int one_handed_ds_int, two_handed_ds_int;
            byte one_handed_ds, two_handed_ds;
            bool is_currently_equipped = (&inventory[INVEN_WIELD] == o_ptr);
            
            /* Determine potential hand-and-a-half bonus (when wielded two-handed) */
            if (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
            {
                hand_half_bonus_potential = 3;
            }
            else
            {
                hand_half_bonus_potential = 2;
            }
            
            if (is_currently_equipped)
            {
                /* For equipped weapons, use actual current damage */
                one_handed_ds_int = (int)actual_ds - hand_half_bonus_equipped;
                one_handed_ds = (one_handed_ds_int < 0) ? 0 : (byte)one_handed_ds_int;
                two_handed_ds = actual_ds;
            }
            else
            {
                /* For unequipped weapons, calculate hypothetical damage */
                /* One-handed: base + strength (capped by weight), no hand-and-half bonus */
                one_handed_ds_int = (int)actual_ds;
                one_handed_ds = (one_handed_ds_int < 0) ? 0 : (byte)one_handed_ds_int;
                
                /* Two-handed: base + strength (capped by weight) + hand-and-half bonus */
                two_handed_ds_int = (int)actual_ds + hand_half_bonus_potential;
                two_handed_ds = (two_handed_ds_int < 0) ? 0 : (byte)two_handed_ds_int;
            }

            /* Show distinction between one-handed and two-handed */
            if (base_dd != actual_dd || base_ds != one_handed_ds || base_ds != two_handed_ds)
            {
                p_text_out(format(
                    "It does %dd%d damage (%dd%d one-handed, %dd%d two-handed with your current strength and abilities).",
                    base_dd, base_ds, actual_dd, one_handed_ds, actual_dd, two_handed_ds));
            }
            else
            {
                p_text_out(format("It does %dd%d damage.", base_dd, base_ds));
            }
        }
        else
        {
            /* Regular melee weapons */
            if (base_dd != actual_dd || base_ds != actual_ds)
            {
                p_text_out(format(
                    "It does %dd%d damage (%dd%d with your current strength and abilities).",
                    base_dd, base_ds, actual_dd, actual_ds));
            }
            else
            {
                p_text_out(format("It does %dd%d damage.", base_dd, base_ds));
            }
        }
    }
    else if (is_bow)
    {
        /* Calculate bow damage with current strength */
        base_dd = o_ptr->dd;
        base_ds = o_ptr->ds;
        actual_dd = o_ptr->dd;  /* Bow dice don't change */
        actual_ds = total_ads(o_ptr);

        /* Show complete bow+arrow damage */
        if (base_ds != actual_ds)
        {
            p_text_out(format(
                "It shoots arrows for %dd%d damage (%dd%d with your current strength).",
                base_dd, base_ds, actual_dd, actual_ds));
        }
        else
        {
            p_text_out(format("It shoots arrows for %dd%d damage.", base_dd, base_ds));
        }
    }

    return (true);
}

/*
 * Output object information
 */
bool object_info_out(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    u32b ff1, ff2, ff3;
    bool something = false;

    /* Grab the object flags */
    object_info_out_flags(o_ptr, &f1, &f2, &f3);

    /* Hack - grab the ID-independent flags */
    /* Used to show handedness even when not ID'd */
    object_flags(o_ptr, &ff1, &ff2, &ff3);

    /* Describe the object */
    if (describe_stats(o_ptr, f1))
        something = true;
    if (describe_neg_stats(o_ptr, f1))
        something = true;
    if (describe_secondary(o_ptr, f1))
        something = true;
    if (describe_slay(o_ptr, f1))
        something = true;
    if (describe_brand(o_ptr, f1))
        something = true;
    if (describe_misc_weapon_attributes(o_ptr, f1, f3))
        something = true;
    if (describe_resist(o_ptr, f2))
        something = true;
    if (describe_vulnerability(o_ptr, f2))
        something = true;
    if (describe_sustains(o_ptr, f2))
        something = true;
    if (describe_misc_magic(o_ptr, f2, f3))
        something = true;
    if (describe_activation(o_ptr, f3))
        something = true;
    if (describe_ignores(o_ptr, f3))
        something = true;
    if (describe_abilities(o_ptr))
        something = true;

    if (describe_handedness(o_ptr, ff3))
        something = true;
    if (describe_polearmness(ff3))
        something = true;
    if (describe_archery(o_ptr))
        something = true;
    if (describe_weapon_damage(o_ptr))
        something = true;

    /* We are done. */
    return something;
}

/*
 * Header for additional information when printing to screen.
 *
 * Header for additional information when printing to screen.
 */
static bool screen_out_head(const object_type* o_ptr)
{
    char* o_name;
    int name_size = Term->wid;

    bool has_description = false;

    log_trace("screen_out_head: Starting, Term->wid=%d, Term->hgt=%d", Term->wid, Term->hgt);
    log_trace("screen_out_head: Current cursor position: x=%d, y=%d", Term->scr->cx, Term->scr->cy);

    /* Allocate memory to the size of the screen */
    o_name = mem_alloc_array(name_size, char);

    /* Description */
    object_desc(o_name, name_size, o_ptr, true, 3);

    log_trace("screen_out_head: About to print object name at current position");
    
    /* Use same color logic as inventory/equipment displays */
    byte base_color;
    
    /* Determine base color from item type */
    if (weapon_glows(o_ptr))
    {
        base_color = TERM_L_BLUE;
    }
    else
    {
        base_color = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];
    }
    
    /* Apply artifact/shade coloring using the same function as inventory */
    byte name_color = object_display_color(o_ptr, base_color);
    
    /* Print, in colour */
    text_out_c(name_color, format("%^s", o_name));

    /* Show weight information */
    {
        char weight_buf[64];
        int total_weight = o_ptr->weight * o_ptr->number;
        int each_weight = o_ptr->weight;
        if (o_ptr->number > 1) {
            strnfmt(weight_buf, sizeof(weight_buf), " %3d.%1d lb (%3d.%1d lb each)",
                total_weight / 10, total_weight % 10, each_weight / 10, each_weight % 10);
        } else {
            strnfmt(weight_buf, sizeof(weight_buf), " %3d.%1d lb", total_weight / 10, total_weight % 10);
        }
        text_out_c(TERM_L_UMBER, weight_buf);
    }

    log_trace("screen_out_head: After printing object name, cursor position: x=%d, y=%d", Term->scr->cx, Term->scr->cy);

    /* Free up the memory */
    mem_free_null(o_name);

    /* Display the known artefact description */
    if (!adult_rand_artefacts && o_ptr->name1 && object_known_p(o_ptr)
        && a_info[o_ptr->name1].text)

    {
        p_text_out("\n\n   ");
        p_text_out(a_text + a_info[o_ptr->name1].text);
        has_description = true;
    }
    /* Display the known object description */
    else if (object_aware_p(o_ptr) || object_known_p(o_ptr))
    {
        if (k_info[o_ptr->k_idx].text)
        {
            p_text_out("\n\n   ");
            p_text_out(k_text + k_info[o_ptr->k_idx].text);
            has_description = true;
        }

        /* Display an additional special item description */
        if (o_ptr->name2 && object_known_p(o_ptr) && e_info[o_ptr->name2].text)
        {
            p_text_out("\n\n   ");
            p_text_out(e_text + e_info[o_ptr->name2].text);
            has_description = true;
        }
    }

    return (has_description);
}

/*
 * Display the text from a note on the screen.
 */
void note_info_screen(const object_type* o_ptr)
{
    /* Redirect output to the screen */
    text_out_hook = text_out_to_screen;

    /* Save the screen */
    screen_save();

    /* Indent output by 14 characters, and wrap at column 60 */
    text_out_wrap = 60;
    text_out_indent = 14;

    /* Note intro */
    Term_gotoxy(text_out_indent, 0);
    text_out_c(TERM_L_WHITE + TERM_SHADE, "The note here reads:\n\n");

    /* Note text */
    Term_gotoxy(text_out_indent, 2);
    text_out_to_screen(TERM_WHITE, k_text + k_info[o_ptr->k_idx].text);

    /* Note outro */
    text_out_c(TERM_L_WHITE + TERM_SHADE, "\n\n(press any key)\n");

    /* Reset text_out() vars */
    text_out_wrap = 0;
    text_out_indent = 0;

    /* Wait for input */
    (void)inkey();

    /* Load the screen */
    screen_load();

    return;
}

/*
 * Place an item description on the screen.
 */
void object_info_screen(const object_type* o_ptr)
{
    bool has_description, has_info;

    log_trace("object_info_screen: Starting item description display");
    log_trace("object_info_screen: BEFORE reset - text_out_indent=%d, text_out_wrap=%d", text_out_indent, text_out_wrap);

    /* Redirect output to the screen */
    text_out_hook = text_out_to_screen;
    
    /* Reset text output positioning to ensure proper layout */
    text_out_wrap = 0;
    text_out_indent = 0;

    log_trace("object_info_screen: AFTER reset - text_out_indent=%d, text_out_wrap=%d", text_out_indent, text_out_wrap);

    /* Save the screen */
    screen_save();
    log_trace("object_info_screen: Screen saved");

    /* Ensure cursor starts at top-left for proper text layout */
    Term_gotoxy(0, 0);
    log_trace("object_info_screen: Reset cursor to (0,0), now at x=%d, y=%d", Term->scr->cx, Term->scr->cy);

    has_description = screen_out_head(o_ptr);

    object_info_out_flags = object_flags_known;

    /* Dump the info */
    new_paragraph = true;
    has_info = object_info_out(o_ptr);
    new_paragraph = false;

    if (!object_known_p(o_ptr))
    {
        p_text_out("\n\n   This item has not been identified.");
    }
    else if ((!has_description) && (!has_info))
    {
        p_text_out(
            "\n\n   This item does not seem to possess any special abilities.");
    }

    text_out_c(TERM_L_BLUE, "\n\n(press any key)\n");

    log_trace("object_info_screen: About to wait for input");

    /* Wait for input */
    (void)inkey();

    log_trace("object_info_screen: Input received, about to load screen");

    /* Load the screen */
    screen_load();
    
    /* Ensure text output variables are reset after use */
    text_out_wrap = 0;
    text_out_indent = 0;
    
    log_trace("object_info_screen: Screen loaded, exiting - text_out_indent=%d, text_out_wrap=%d", text_out_indent, text_out_wrap);

    return;
}

void object_info_screen_multi(const object_type** objects, const char** headings, int count)
{
    if (count <= 0 || objects == NULL)
        return;

    text_out_hook = text_out_to_screen;
    text_out_wrap = 0;
    text_out_indent = 0;

    screen_save();
    Term_gotoxy(0, 0);

    for (int i = 0; i < count; i++)
    {
        if (i > 0)
        {
            text_out_c(TERM_L_DARK, "\n----------------------------------------\n\n");
        }

        Term_erase(0, Term->scr->cy, 255);

        if (headings && headings[i] && headings[i][0])
        {
            text_out_c(TERM_L_BLUE, headings[i]);
            text_out_c(TERM_L_BLUE, "\n");
        }

        if (objects[i])
        {
            bool has_description = screen_out_head(objects[i]);

            object_info_out_flags = object_flags_known;

            new_paragraph = true;
            bool has_info = object_info_out(objects[i]);
            new_paragraph = false;

            if (!object_known_p(objects[i]))
            {
                p_text_out("\n\n   This item has not been identified.");
            }
            else if ((!has_description) && (!has_info))
            {
                p_text_out("\n\n   This item does not seem to possess any special abilities.");
            }
        }
        else
        {
            p_text_out("\n   (slot is empty)");
        }
    }

    text_out_c(TERM_L_BLUE, "\n\n(press any key)\n");
    (void)inkey();

    screen_load();

    text_out_wrap = 0;
    text_out_indent = 0;
}






