/* File: object/object-info.c */

#include "angband.h"
#include "externs.h"
#include "object/object-info.h"
#include "log/log.h"
#include "sdl-config.h"


/* true if a paragraph break should be output before next p_text_out() */
static bool new_paragraph = false;

#define OBJECT_INFO_CAPTURE_MIN_ROWS 255
#define OBJECT_INFO_CAPTURE_MAX_ROWS 2048
#define OBJECT_INFO_CAPTURE_ROWS_PER_OBJECT 64
#define OBJECT_INFO_CAPTURE_STORY_COLS 200
#define OBJECT_INFO_MAX_ACTIONS 8
#define OBJECT_INFO_UNDERSTANDING_KEY 'i'

typedef struct object_info_screen_capture
{
    int width;
    int height;
    int target_cols;
    int cursor_x;
    int cursor_y;
    bool cursor_useless;
    bool cursor_visible;
    byte* attrs;
    char* chars;
    byte* tattrs;
    char* tchars;
    byte* story;
} object_info_screen_capture;

static object_info_screen_capture object_info_overlay_capture;
static bool object_info_overlay_capture_active = false;

static void object_info_screen_multi_body(const object_type** objects,
    const char** headings, int count, bool clear_current_line);
static char object_info_screen_capture_view(
    const object_info_screen_capture* capture, cptr footer,
    const object_info_screen_action* actions, int action_count);

static int object_info_screen_preferred_capture_width(bool use_story_font)
{
    if (use_story_font)
        return OBJECT_INFO_CAPTURE_STORY_COLS;

    return sdl_description_overlay_max_cols();
}

static bool object_info_blocked_by_hallucination(void)
{
    if (!p_ptr || !p_ptr->image)
        return false;

    msg_print("Your vision is too distorted to examine items carefully.");
    window_stuff();
    Term_fresh();

    return true;
}

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

static void object_info_capitalize_first(char* str)
{
    if (!str)
        return;

    for (char* p = str; *p; p++)
    {
        if (!isspace((unsigned char)*p))
        {
            if (islower((unsigned char)*p))
                *p = (char)toupper((unsigned char)*p);
            return;
        }
    }
}

static bool describe_consumable_healing(const object_type* o_ptr)
{
    int potential, missing, actual;

    if (!object_aware_p(o_ptr) && !object_known_p(o_ptr)
        && !(o_ptr->ident & IDENT_SPOIL))
    {
        return false;
    }

    potential = consumable_healing_points(o_ptr);
    if (potential <= 0)
        return false;

    missing = p_ptr->mhp - p_ptr->chp;
    if (missing < 0)
        missing = 0;

    actual = MIN(potential, missing);

    p_text_out((o_ptr->number == 1) ? "It restores " : "Each one restores ");

    if (actual == potential)
    {
        p_text_out_c(TERM_L_GREEN, format("%d health", actual));
        p_text_out(" right now.  ");
    }
    else
    {
        p_text_out("up to ");
        p_text_out_c(TERM_L_GREEN, format("%d health", potential));

        if (actual > 0)
        {
            p_text_out(format(
                "; at your current wounds it would restore %d.  ", actual));
        }
        else
        {
            p_text_out("; you are already at full health.  ");
        }
    }

    return true;
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

static u32b stat_flag_for_bonus(int stat, bool negative)
{
    switch (stat)
    {
    case A_STR:
        return negative ? TR1_NEG_STR : TR1_STR;
    case A_DEX:
        return negative ? TR1_NEG_DEX : TR1_DEX;
    case A_CON:
        return negative ? TR1_NEG_CON : TR1_CON;
    case A_GRA:
        return negative ? TR1_NEG_GRA : TR1_GRA;
    default:
        return 0L;
    }
}

/*
 * Describe stat modifications.
 */
static bool describe_stats(const object_type* o_ptr, u32b f1)
{
    bool found = false;

    (void)f1;

    for (int stat = 0; stat < A_MAX; stat++)
    {
        const int bonus = o_ptr->stat_bonus[stat];
        if (bonus <= 0)
            continue;
        if (!(f1 & stat_flag_for_bonus(stat, false)))
            continue;

        p_text_out("It ");
        p_text_out_c(TERM_GREEN, "increases");
        p_text_out(" your ");
        p_text_out(stat_names_full[stat]);
        p_text_out(" by ");
        p_text_out_c(TERM_UMBER, format("%i", bonus));
        p_text_out(".  ");
        found = true;
    }

    return found;
}

/*
 * Describe reversed stat modifications.
 */
static bool describe_neg_stats(const object_type* o_ptr, u32b f1)
{
    bool found = false;

    (void)f1;

    for (int stat = 0; stat < A_MAX; stat++)
    {
        const int bonus = o_ptr->stat_bonus[stat];
        if (bonus >= 0)
            continue;
        if (!(f1 & stat_flag_for_bonus(stat, true)))
            continue;

        p_text_out("It ");
        p_text_out_c(TERM_L_RED, "decreases");
        p_text_out(" your ");
        p_text_out(stat_names_full[stat]);
        p_text_out(" by ");
        p_text_out_c(TERM_UMBER, format("%i", -bonus));
        p_text_out(".  ");
        found = true;
    }

    return found;
}

/*
 * Describe "secondary bonuses" of an item.
 */
static bool describe_secondary(const object_type* o_ptr, u32b f1)
{
    bool found = false;

    if (f1 & TR1_MEL)
    {
        int bonus = o_ptr->skill_bonus[S_MEL];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your melee by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_ARC)
    {
        int bonus = o_ptr->skill_bonus[S_ARC];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your archery by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_STL)
    {
        int bonus = o_ptr->skill_bonus[S_STL];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your stealth by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_PER)
    {
        int bonus = o_ptr->skill_bonus[S_PER];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your perception by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_WIL)
    {
        int bonus = o_ptr->skill_bonus[S_WIL];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your will by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_SMT)
    {
        int bonus = o_ptr->skill_bonus[S_SMT];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your smithing by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_SNG)
    {
        int bonus = o_ptr->skill_bonus[S_SNG];
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your song by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }

    if (f1 & TR1_TUNNEL)
    {
        int bonus = o_ptr->pval;
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your tunneling by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }
    if (f1 & TR1_DAMAGE_SIDES)
    {
        int bonus = o_ptr->pval;
        if (bonus != 0)
        {
            p_text_out("It ");
            p_text_out_c(bonus > 0 ? TERM_GREEN : TERM_L_RED,
                (bonus > 0 ? "improves" : "worsens"));
            p_text_out(" your damage sides by ");
            p_text_out_c(TERM_UMBER, format("%i", ABS(bonus)));
            p_text_out(".  ");
            found = true;
        }
    }

    return found;
}

/*
 * Describe the special slays and executes of an item.
 */
static bool describe_slay(const object_type* o_ptr, u32b f1, u32b f4)
{
    cptr slays[16];
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
    if (f4 & (TR4_SLAY_SERPENT))
        slays[slcnt++] = "serpents";
    if (f4 & (TR4_SLAY_VAMPIRE))
        slays[slcnt++] = "vampires";
    if (f4 & (TR4_SLAY_HORROR))
        slays[slcnt++] = "horrors";
    if (f4 & (TR4_SLAY_CAT))
        slays[slcnt++] = "cats";
    if (f4 & (TR4_SLAY_GIANT))
        slays[slcnt++] = "giants";
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

static void format_min_depth_bonus_depths(char* buf, size_t buflen, int units)
{
    int whole;
    int rem;

    if (!buf || buflen == 0)
        return;

    if (units < 0)
        units = 0;

    whole = units / MIN_DEPTH_BONUS_UNITS_PER_DEPTH;
    rem = units % MIN_DEPTH_BONUS_UNITS_PER_DEPTH;

    if (rem == 0)
        strnfmt(buf, buflen, "%d", whole);
    else if (MIN_DEPTH_BONUS_UNITS_PER_DEPTH == 2 && rem == 1)
        strnfmt(buf, buflen, "%d.5", whole);
    else
        strnfmt(buf, buflen, "%d+%d/%d", whole, rem,
            MIN_DEPTH_BONUS_UNITS_PER_DEPTH);
}

/*
 * Describe miscellaneous powers such as see invisible, free action,
 * permanent light, etc; also note curses and penalties.
 */
static bool describe_misc_magic(const object_type* o_ptr, u32b f2, u32b f3, u32b f4)
{
    cptr good[24], bad[14];
    char deep_call_desc[120];
    char deep_call_equipped_bonus[16];
    char deep_call_inventory_bonus[16];
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
    if (f3 & (TR3_OATH_BOOST))
        good[gc++] = "doubles the reward of your oath (or increases your light radius if oathless)";
    if (f4 & (TR4_ARMOR_SHATTER))
        good[gc++] = "can shatter the armor of your foes with each successful blow";
    if (f4 & (TR4_DEPTH_SCALE_PS))
        good[gc++] = "gains protection as you delve deeper";
    if (f3 & (TR3_WILL_DRAIN))
        good[gc++] = "drains the will of your enemies when you strike them";
    if (f4 & (TR4_PAIRED))
        good[gc++] = "is part of a matched pair of weapons";
    if (f4 & (TR4_SUBTLETY_THROW))
        good[gc++] = "lets you use Subtlety with thrown attacks";
    if (f4 & (TR4_BREAKS_PERMA_CURSE))
        good[gc++] = "can break the Oath of Fëanor on your equipped items";
    if (f4 & (TR4_DEEP_CALL))
    {
        format_min_depth_bonus_depths(deep_call_equipped_bonus,
            sizeof(deep_call_equipped_bonus),
            MIN_DEPTH_ITEM_BONUS_DEEP_CALL_EQUIPPED);
        format_min_depth_bonus_depths(deep_call_inventory_bonus,
            sizeof(deep_call_inventory_bonus),
            MIN_DEPTH_ITEM_BONUS_DEEP_CALL_INVENTORY);
        strnfmt(deep_call_desc, sizeof(deep_call_desc),
            "bears a Deep Call, speeding min depth (+%s equipped, +%s in inventory)",
            deep_call_equipped_bonus, deep_call_inventory_bonus);
        good[gc++] = deep_call_desc;
    }
    if ((f4 & (TR4_PROT_FIRE)) && (o_ptr->pd > 0))
        good[gc++] = "uses its protection against fire";
    if ((f4 & (TR4_PROT_COLD)) && (o_ptr->pd > 0))
        good[gc++] = "uses its protection against cold";
    if ((f4 & (TR4_PROT_POIS)) && (o_ptr->pd > 0))
        good[gc++] = "uses its protection against poison";
    if ((f4 & (TR4_PROT_DARK)) && (o_ptr->pd > 0))
        good[gc++] = "uses its protection against darkness";
    if ((f4 & (TR4_WEIGHT)) && !(f4 & (TR4_NEG_WEIGHT)))
        good[gc++] = "is unusually heavy for its kind";
    if ((f4 & (TR4_NEG_WEIGHT)) && !(f4 & (TR4_WEIGHT)))
        good[gc++] = "is unusually light for its kind";

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
        bad[bc++] = "reduces your light radius by 1, but concentrates the light that remains";
    if (f4 & (TR4_UNLIGHT))
        bad[bc++] = "reduces your light radius by 1 without concentrating the light that remains";
    if (f2 & (TR2_SLOWNESS))
        bad[bc++] = "slows your movement";
    if (f2 & (TR2_AGGRAVATE))
        bad[bc++] = "enrages nearby creatures";
    if (f2 & (TR2_HAUNTED))
        bad[bc++] = "draws wraiths to your level";
    if (f2 & (TR2_TRAITOR))
        bad[bc++] = "may betray you when you need it most";
    if (f3 & (TR3_OATH_NEGATE))
        bad[bc++] = "negates your oath bonuses (even when in inventory)";
    if (f4 & (TR4_JINX))
        bad[bc++] = "bears a jinx that sanctity can break with Curse Breaking";

    /* Deal with cursed stuff */
    if (cursed_p(o_ptr))
    {
        if (f3 & (TR3_PERMA_CURSE))
            bad[bc++] = "bound by the Oath of Fëanor (broken by holy light); the Silmarils are calling you, speeding the minimum depth timer as if you were five levels deeper even in your inventory";
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
            if (strstr(bad[i], "cursed") || strstr(bad[i], "Oath of Fëanor"))
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
                if (strstr(bad[i], "Oath of Fëanor"))
                {
                    p_text_out("is ");
                    p_text_out_c(TERM_VIOLET, "bound by the Oath of Fëanor");
                    p_text_out(" (broken by holy light); the Silmarils are calling you, speeding the minimum depth timer as if you were five levels deeper even in your inventory");
                }
                else if (strstr(bad[i], "cursed"))
                {
                    if (strstr(bad[i], "heavily"))
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
    static char ability_buf[8][80]; /* Static buffer for modified ability names */
    char cruel_blow_bonus[16];
    char cruel_blow_desc[160];
    int ac = 0;
    ability_type* b_ptr;
    int i;
    bool grants_cruel_blow = false;

    // only describe when identified
    if (!object_known_p(o_ptr) && !(o_ptr->ident & (IDENT_SPOIL)))
        return (false);

    // check its abilities
    for (i = 0; i < o_ptr->abilities; i++)
    {
        b_ptr
            = &b_info[ability_index(o_ptr->skilltype[i], o_ptr->abilitynum[i])];
        if (o_ptr->skilltype[i] == S_STL
            && o_ptr->abilitynum[i] == STL_CRUEL_BLOW)
        {
            grants_cruel_blow = true;
        }

        /* Check if this is a Bane ability with a specific type */
        if (o_ptr->skilltype[i] == S_PER && o_ptr->abilitynum[i] == PER_BANE
            && o_ptr->bane_type[i] > 0 && o_ptr->bane_type[i] < 9)
        {
            strnfmt(ability_buf[ac], 80, "%s-%s",
                bane_name[o_ptr->bane_type[i]], b_name + b_ptr->name);
            ability[ac] = ability_buf[ac];
            ac++;
        }
        else
        {
            ability[ac++] = b_name + b_ptr->name;
        }
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
        if (grants_cruel_blow)
        {
            format_min_depth_bonus_depths(cruel_blow_bonus,
                sizeof(cruel_blow_bonus),
                MIN_DEPTH_ITEM_BONUS_CRUEL_BLOW_EQUIPPED);
            strnfmt(cruel_blow_desc, sizeof(cruel_blow_desc),
                "When equipped, its Cruel Blow grant speeds min depth by the same amount as equipped Deep Call (+%s), even if disabled.  ",
                cruel_blow_bonus);
            p_text_out(cruel_blow_desc);
        }

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
/*
 * Describe what happens when a potion is hurled and shattered.  Only shown to
 * those who know Alchemy, who alone may throw potions.
 */
static bool describe_potion_throw(const object_type* o_ptr)
{
    cptr good[1];
    int gc = 0;

    if (!o_ptr || (o_ptr->tval != TV_POTION))
        return false;
    if (!p_ptr->active_ability[S_PER][PER_ALCHEMY]
        && !p_ptr->have_ability[S_PER][PER_ALCHEMY])
        return false;

    switch (o_ptr->sval)
    {
    case SV_POTION_SLOWNESS:
        good[gc++] = "bursts into a slowing vapour across the impact square and every adjacent square";
        break;
    case SV_POTION_CONFUSION:
        good[gc++] = "bursts into a bewildering haze across the impact square and every adjacent square";
        break;
    case SV_POTION_POISON:
        good[gc++] = "bursts into a cloud of poison across the impact square and every adjacent square";
        break;
    case SV_POTION_ORCISH_LIQUOR:
        good[gc++] = "bursts into flame across the impact square and every adjacent square";
        break;
    case SV_POTION_BLINDNESS:
        good[gc++] = "bursts into a blinding spray across the impact square and every adjacent square";
        break;
    case SV_POTION_DEC_DEX:
        good[gc++] = "bursts into a slick haze across the impact square and every adjacent square";
        break;
    case SV_POTION_DEC_GRA:
        good[gc++] = "bursts into a spirit-severing mist across the impact square and every adjacent square";
        break;
    case SV_POTION_true_SIGHT:
        good[gc++] = "bursts into a clarifying mist across the impact square and every adjacent square";
        break;
    default:
        good[gc++] = "shatters without any lasting effect";
        break;
    }

    output_desc_list("If thrown, it ", good, gc);
    return true;
}

bool object_info_out(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    u32b ff1, ff2, ff3, ff4;
    bool something = false;
    bool known_only = (object_info_out_flags == object_flags_known);

    /* Grab the object flags */
    object_info_out_flags(o_ptr, &f1, &f2, &f3);
    if (known_only)
        object_flags_known4(o_ptr, &ff1, &ff2, &ff3, &f4);
    else
        object_flags4(o_ptr, &ff1, &ff2, &ff3, &f4);

    /* Hack - grab the ID-independent flags */
    /* Used to show handedness even when not ID'd */
    object_flags4(o_ptr, &ff1, &ff2, &ff3, &ff4);

    /* Describe the object */
    if (describe_consumable_healing(o_ptr))
        something = true;
    if (describe_potion_throw(o_ptr))
        something = true;
    if (describe_stats(o_ptr, f1))
        something = true;
    if (describe_neg_stats(o_ptr, f1))
        something = true;
    if (describe_secondary(o_ptr, f1))
        something = true;
    if (describe_slay(o_ptr, f1, f4))
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
    if (describe_misc_magic(o_ptr, f2, f3, f4))
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

typedef struct object_lore_profile
{
    cptr keywords[10];
    int keyword_count;
} object_lore_profile;

typedef enum object_lore_alignment
{
    OBJECT_LORE_ALIGNMENT_NEUTRAL = 0,
    OBJECT_LORE_ALIGNMENT_NOBLE = 1,
    OBJECT_LORE_ALIGNMENT_EVIL = 2
} object_lore_alignment;

static void object_lore_add_keyword(object_lore_profile* profile, cptr keyword)
{
    int i;

    if (!profile || !keyword || !keyword[0])
        return;

    for (i = 0; i < profile->keyword_count; i++)
    {
        if (streq(profile->keywords[i], keyword))
            return;
    }

    if (profile->keyword_count >= (int)N_ELEMENTS(profile->keywords))
        return;

    profile->keywords[profile->keyword_count++] = keyword;
}

static void object_lore_trim_copy(const char* start, size_t len, char* out,
    size_t out_sz)
{
    if (!out || out_sz == 0)
        return;

    while (len > 0 && isspace((unsigned char)*start))
    {
        start++;
        len--;
    }

    while (len > 0 && isspace((unsigned char)start[len - 1]))
        len--;

    if (len >= out_sz)
        len = out_sz - 1;

    if (len > 0)
        memcpy(out, start, len);

    out[len] = '\0';
}

static int object_lore_keyword_rank(const object_lore_profile* profile,
    cptr keyword)
{
    int i;

    if (!profile || !keyword || !keyword[0])
        return -1;

    for (i = 0; i < profile->keyword_count; i++)
    {
        if (streq(profile->keywords[i], keyword))
            return i;
    }

    return -1;
}

static bool object_lore_text_is_structured(cptr raw)
{
    while (raw && *raw && isspace((unsigned char)*raw))
        raw++;

    return (raw && *raw == '[');
}

static object_lore_alignment object_lore_actual_alignment(
    const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    bool has_noble;
    bool has_evil;

    if (!o_ptr || !o_ptr->k_idx)
        return OBJECT_LORE_ALIGNMENT_NEUTRAL;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    has_noble = ((f4 & TR4_NOBLE_ITEM) != 0);
    has_evil = ((f4 & TR4_EVIL_ITEM) != 0);

    if (has_noble && !has_evil)
        return OBJECT_LORE_ALIGNMENT_NOBLE;
    if (has_evil && !has_noble)
        return OBJECT_LORE_ALIGNMENT_EVIL;

    return OBJECT_LORE_ALIGNMENT_NEUTRAL;
}

static object_lore_alignment object_lore_base_alignment(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return OBJECT_LORE_ALIGNMENT_NEUTRAL;

    /* Base lore should reflect what the item visibly is, even before the
     * player has identified the ego behind that appearance. */
    return object_lore_actual_alignment(o_ptr);
}

static void object_lore_add_alignment_keyword(object_lore_profile* profile,
    object_lore_alignment alignment)
{
    if (!profile)
        return;

    switch (alignment)
    {
    case OBJECT_LORE_ALIGNMENT_NOBLE:
        object_lore_add_keyword(profile, "noble");
        object_lore_add_keyword(profile, "good");
        break;

    case OBJECT_LORE_ALIGNMENT_EVIL:
        object_lore_add_keyword(profile, "evil");
        break;

    case OBJECT_LORE_ALIGNMENT_NEUTRAL:
    default:
        object_lore_add_keyword(profile, "neutral");
        break;
    }
}

static void object_lore_profile_for_object(const object_type* o_ptr,
    object_lore_profile* profile)
{
    if (!profile)
        return;

    memset(profile, 0, sizeof(*profile));

    if (!o_ptr || !o_ptr->k_idx)
        return;

    /* Add more specific item tags before broad category tags so keyed
     * lore in the data files can safely provide fallbacks like [shield]
     * or [light] without overriding [kite_shield] or [lantern]. */
    switch (o_ptr->tval)
    {
    case TV_SWORD:
        switch (o_ptr->sval)
        {
        case SV_DAGGER:
            object_lore_add_keyword(profile, "dagger");
            object_lore_add_keyword(profile, "knife");
            object_lore_add_keyword(profile, "small_blade");
            break;
        case SV_CURVED_SWORD:
            object_lore_add_keyword(profile, "curved_sword");
            object_lore_add_keyword(profile, "sword");
            break;
        case SV_SHORT_SWORD:
            object_lore_add_keyword(profile, "short_sword");
            object_lore_add_keyword(profile, "sword");
            break;
        case SV_LONG_SWORD:
        case SV_MITHRIL_LONG_SWORD:
            object_lore_add_keyword(profile, "long_sword");
            object_lore_add_keyword(profile, "sword");
            break;
        case SV_BASTARD_SWORD:
            object_lore_add_keyword(profile, "bastard_sword");
            object_lore_add_keyword(profile, "sword");
            break;
        case SV_GREAT_SWORD:
        case SV_STAR_IRON_GREAT_SWORD:
            object_lore_add_keyword(profile, "great_sword");
            object_lore_add_keyword(profile, "sword");
            break;
        default:
            object_lore_add_keyword(profile, "sword");
            break;
        }
        object_lore_add_keyword(profile, "blade");
        object_lore_add_keyword(profile, "melee_weapon");
        object_lore_add_keyword(profile, "weapon");
        break;

    case TV_POLEARM:
        switch (o_ptr->sval)
        {
        case SV_SPEAR:
            object_lore_add_keyword(profile, "spear");
            object_lore_add_keyword(profile, "polearm");
            break;
        case SV_GREAT_SPEAR:
            object_lore_add_keyword(profile, "great_spear");
            object_lore_add_keyword(profile, "polearm");
            break;
        case SV_GLAIVE:
            object_lore_add_keyword(profile, "glaive");
            object_lore_add_keyword(profile, "polearm");
            break;
        case SV_HAND_AXE:
            object_lore_add_keyword(profile, "hand_axe");
            object_lore_add_keyword(profile, "axe");
            break;
        case SV_BATTLE_AXE:
            object_lore_add_keyword(profile, "battle_axe");
            object_lore_add_keyword(profile, "axe");
            break;
        case SV_GREAT_AXE:
            object_lore_add_keyword(profile, "great_axe");
            object_lore_add_keyword(profile, "axe");
            break;
        default:
            object_lore_add_keyword(profile, "polearm");
            break;
        }
        object_lore_add_keyword(profile, "melee_weapon");
        object_lore_add_keyword(profile, "weapon");
        break;

    case TV_HAFTED:
        switch (o_ptr->sval)
        {
        case SV_QUARTERSTAFF:
            object_lore_add_keyword(profile, "quarterstaff");
            object_lore_add_keyword(profile, "staff");
            break;
        case SV_WAR_HAMMER:
            object_lore_add_keyword(profile, "war_hammer");
            object_lore_add_keyword(profile, "hammer");
            break;
        default:
            object_lore_add_keyword(profile, "hafted");
            break;
        }
        object_lore_add_keyword(profile, "melee_weapon");
        object_lore_add_keyword(profile, "weapon");
        break;

    case TV_BOW:
        switch (o_ptr->sval)
        {
        case SV_SHORT_BOW:
            object_lore_add_keyword(profile, "shortbow");
            break;
        case SV_LONG_BOW:
            object_lore_add_keyword(profile, "longbow");
            break;
        case SV_DH_LONG_BOW:
            object_lore_add_keyword(profile, "dragonhorn_bow");
            break;
        default:
            break;
        }
        object_lore_add_keyword(profile, "bow");
        object_lore_add_keyword(profile, "ranged_weapon");
        break;

    case TV_ARROW:
        object_lore_add_keyword(profile, "arrow");
        object_lore_add_keyword(profile, "missile");
        object_lore_add_keyword(profile, "ranged_weapon");
        break;

    case TV_SHIELD:
        switch (o_ptr->sval)
        {
        case SV_ROUND_SHIELD:
            object_lore_add_keyword(profile, "round_shield");
            break;
        case SV_KITE_SHIELD:
            object_lore_add_keyword(profile, "kite_shield");
            break;
        case SV_MITHRIL_SHIELD:
            object_lore_add_keyword(profile, "mithril_shield");
            break;
        default:
            break;
        }
        object_lore_add_keyword(profile, "shield");
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_HELM:
        switch (o_ptr->sval)
        {
        case SV_GREAT_HELM:
            object_lore_add_keyword(profile, "great_helm");
            break;
        case SV_MITHRIL_HELM:
            object_lore_add_keyword(profile, "mithril_helm");
            break;
        default:
            object_lore_add_keyword(profile, "headpiece");
            break;
        }
        object_lore_add_keyword(profile, "helm");
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_SOFT_ARMOR:
        switch (o_ptr->sval)
        {
        case SV_ROBE:
            object_lore_add_keyword(profile, "robe");
            break;
        case SV_LEATHER_ARMOR:
            object_lore_add_keyword(profile, "leather_armour");
            break;
        case SV_STUDDED_LEATHER:
            object_lore_add_keyword(profile, "studded_armour");
            break;
        default:
            break;
        }
        object_lore_add_keyword(profile, "soft_armour");
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_MAIL:
        switch (o_ptr->sval)
        {
        case SV_MAIL_CORSLET:
            object_lore_add_keyword(profile, "mail_corslet");
            break;
        case SV_LONG_CORSLET:
            object_lore_add_keyword(profile, "hauberk");
            break;
        case SV_MITHRIL_CORSLET:
            object_lore_add_keyword(profile, "mithril_mail");
            break;
        default:
            break;
        }
        object_lore_add_keyword(profile, "mail");
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_CLOAK:
        object_lore_add_keyword(profile, "cloak");
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_BOOTS:
        switch (o_ptr->sval)
        {
        case SV_PAIR_OF_STEEL_GREAVES:
        case SV_PAIR_OF_MITHRIL_GREAVES:
            object_lore_add_keyword(profile, "greaves");
            break;
        default:
            object_lore_add_keyword(profile, "boots");
            break;
        }
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_GLOVES:
        switch (o_ptr->sval)
        {
        case SV_SET_OF_GAUNTLETS:
            object_lore_add_keyword(profile, "gauntlets");
            break;
        default:
            object_lore_add_keyword(profile, "gloves");
            break;
        }
        object_lore_add_keyword(profile, "hands");
        object_lore_add_keyword(profile, "armour");
        break;

    case TV_LIGHT:
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
            object_lore_add_keyword(profile, "torch");
            object_lore_add_keyword(profile, "wooden_torch");
            break;
        case SV_LIGHT_MALLORN:
            object_lore_add_keyword(profile, "torch");
            object_lore_add_keyword(profile, "mallorn_torch");
            break;
        case SV_LIGHT_LANTERN:
            object_lore_add_keyword(profile, "lantern");
            break;
        case SV_LIGHT_LESSER_JEWEL:
            object_lore_add_keyword(profile, "lesser_jewel");
            break;
        case SV_LIGHT_FEANORIAN:
            object_lore_add_keyword(profile, "feanorian_lamp");
            break;
        default:
            break;
        }
        object_lore_add_keyword(profile, "light");
        break;

    case TV_RING:
        object_lore_add_keyword(profile, "ring");
        object_lore_add_keyword(profile, "jewelry");
        break;

    case TV_AMULET:
        object_lore_add_keyword(profile, "amulet");
        object_lore_add_keyword(profile, "jewelry");
        break;

    case TV_DIGGING:
        if (o_ptr->sval == SV_MATTOCK)
            object_lore_add_keyword(profile, "mattock");
        else
            object_lore_add_keyword(profile, "shovel");
        object_lore_add_keyword(profile, "digging_tool");
        object_lore_add_keyword(profile, "tool");
        break;

    default:
        object_lore_add_keyword(profile, "item");
        break;
    }

    object_lore_add_keyword(profile, "all");
}

static bool object_lore_select_segment(cptr raw,
    const object_lore_profile* profile, char* out, size_t out_sz)
{
    const char* cursor;
    const char* best_start = NULL;
    size_t best_len = 0;
    int best_rank = 9999;
    bool saw_structured = false;

    if (!raw || !profile || !out || out_sz == 0)
        return false;

    cursor = raw;

    while (*cursor)
    {
        const char* seg_end = strstr(cursor, "||");
        const char* seg_stop = seg_end ? seg_end : cursor + strlen(cursor);
        const char* seg_start = cursor;
        const char* tag_start;
        const char* tag_end;
        const char* body_start;
        char tags_buf[256];
        char token[64];
        size_t tags_len;
        bool segment_matched = false;

        while (seg_start < seg_stop && isspace((unsigned char)*seg_start))
            seg_start++;
        while (seg_stop > seg_start && isspace((unsigned char)seg_stop[-1]))
            seg_stop--;

        if (seg_start < seg_stop && *seg_start == '[')
        {
            saw_structured = true;
            tag_end = memchr(seg_start, ']', (size_t)(seg_stop - seg_start));
            if (tag_end)
            {
                tag_start = seg_start + 1;
                tags_len = (size_t)(tag_end - tag_start);
                if (tags_len >= sizeof(tags_buf))
                    tags_len = sizeof(tags_buf) - 1;
                memcpy(tags_buf, tag_start, tags_len);
                tags_buf[tags_len] = '\0';

                body_start = tag_end + 1;
                while (body_start < seg_stop
                    && isspace((unsigned char)*body_start))
                {
                    body_start++;
                }

                if (body_start < seg_stop)
                {
                    char* next = tags_buf;
                    while (*next)
                    {
                        char* comma = strchr(next, ',');
                        int rank = -1;

                        if (comma)
                            *comma = '\0';

                        object_lore_trim_copy(next, strlen(next), token,
                            sizeof(token));

                        if (streq(token, "default") || streq(token, "all"))
                            rank = 9000;
                        else
                            rank = object_lore_keyword_rank(profile, token);

                        if (rank >= 0 && rank < best_rank)
                        {
                            best_rank = rank;
                            best_start = body_start;
                            best_len = (size_t)(seg_stop - body_start);
                            segment_matched = true;
                        }

                        if (!comma)
                            break;

                        next = comma + 1;
                    }

                    if (!segment_matched && best_start == NULL && best_rank > 9000)
                    {
                        if (strstr(tags_buf, "default") || strstr(tags_buf, "all"))
                        {
                            best_rank = 9000;
                            best_start = body_start;
                            best_len = (size_t)(seg_stop - body_start);
                        }
                    }
                }
            }
        }

        if (!seg_end)
            break;
        cursor = seg_end + 2;
    }

    if (!saw_structured || !best_start)
        return false;

    object_lore_trim_copy(best_start, best_len, out, out_sz);
    return (out[0] != '\0');
}

cptr object_lore_select_base_text(const object_type* o_ptr, char* out,
    size_t out_sz)
{
    cptr raw;
    object_lore_profile profile;

    if (!o_ptr || !o_ptr->k_idx || !k_info[o_ptr->k_idx].text)
        return NULL;

    raw = k_text + k_info[o_ptr->k_idx].text;
    if (!object_lore_text_is_structured(raw))
        return raw;

    object_lore_profile_for_object(o_ptr, &profile);
    object_lore_add_alignment_keyword(&profile,
        object_lore_base_alignment(o_ptr));

    if (!object_lore_select_segment(raw, &profile, out, out_sz))
        return NULL;

    return out;
}

static bool screen_out_legacy_ego_lore(cptr raw_text)
{
    if (!raw_text || !raw_text[0])
        return false;

    p_text_out("\n\n   ");
    p_text_out(raw_text);
    return true;
}

static bool screen_out_ego_lore(const object_type* o_ptr)
{
    byte ego_pfx;
    byte ego_sfx;
    cptr prefix_text = NULL;
    cptr suffix_text = NULL;
    bool prefix_structured = false;
    bool suffix_structured = false;
    bool has_description = false;
    char prefix_buf[1024];
    char suffix_buf[1024];
    object_lore_profile profile;

    if (!o_ptr || !o_ptr->k_idx || !object_known_p(o_ptr))
        return false;

    ego_pfx = object_ego_prefix(o_ptr);
    ego_sfx = object_ego_suffix(o_ptr);

    if (ego_pfx && e_info[ego_pfx].text)
        prefix_text = e_text + e_info[ego_pfx].text;

    if (ego_sfx && (ego_sfx != ego_pfx) && e_info[ego_sfx].text)
        suffix_text = e_text + e_info[ego_sfx].text;

    if (!prefix_text && !suffix_text)
        return false;

    object_lore_profile_for_object(o_ptr, &profile);

    prefix_buf[0] = '\0';
    suffix_buf[0] = '\0';

    if (prefix_text && object_lore_text_is_structured(prefix_text))
    {
        prefix_structured = object_lore_select_segment(prefix_text, &profile,
            prefix_buf, sizeof(prefix_buf));
    }

    if (suffix_text && object_lore_text_is_structured(suffix_text))
    {
        suffix_structured = object_lore_select_segment(suffix_text, &profile,
            suffix_buf, sizeof(suffix_buf));
    }

    if (prefix_structured || suffix_structured)
    {
        p_text_out("\n\n   ");

        if (prefix_buf[0])
        {
            p_text_out(prefix_buf);
            if (suffix_buf[0])
                p_text_out(" ");
        }

        if (suffix_buf[0])
            p_text_out(suffix_buf);

        has_description = true;
    }

    if (prefix_text && !prefix_structured)
        has_description |= screen_out_legacy_ego_lore(prefix_text);

    if (suffix_text && !suffix_structured)
        has_description |= screen_out_legacy_ego_lore(suffix_text);

    return has_description;
}

/*
 * Header for additional information when printing to screen.
 *
 * Header for additional information when printing to screen.
 */
static bool screen_out_head(const object_type* o_ptr)
{
    char o_name[2048];
    char base_desc_buf[2048];
    cptr base_desc = NULL;

    bool has_description = false;

    log_trace("screen_out_head: Starting, Term->wid=%d, Term->hgt=%d", Term->wid, Term->hgt);
    log_trace("screen_out_head: Current cursor position: x=%d, y=%d", Term->scr->cx, Term->scr->cy);

    /* Description */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    object_info_capitalize_first(o_name);

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
    text_out_c(name_color, o_name);

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

    /* Debug: compact smithing difficulty + weight rarity */
    if (op_ptr->opt[OPT_show_smithing_difficulty] && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);
        text_out_c(TERM_SLATE, format(" {%d,%d}", sd, wr));
    }

    log_trace("screen_out_head: After printing object name, cursor position: x=%d, y=%d", Term->scr->cx, Term->scr->cy);

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
        base_desc = object_lore_select_base_text(o_ptr, base_desc_buf,
            sizeof(base_desc_buf));

        if (base_desc && base_desc[0])
        {
            p_text_out("\n\n   ");
            p_text_out(base_desc);
            has_description = true;
        }

        if (screen_out_ego_lore(o_ptr))
            has_description = true;
    }

    return (has_description);
}

/*
 * Display the text from a note on the screen.
 */
void note_info_screen(const object_type* o_ptr)
{
    if (object_info_blocked_by_hallucination())
        return;

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
    const object_type* objects[1];

    objects[0] = o_ptr;
    object_info_screen_multi(objects, NULL, 1);
}

static void object_info_screen_multi_body(const object_type** objects,
    const char** headings, int count, bool clear_current_line)
{
    for (int i = 0; i < count; i++)
    {
        if (i > 0)
        {
            text_out_c(TERM_L_DARK, "\n----------------------------------------\n\n");
        }

        if (clear_current_line && Term && Term->scr)
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
            {
                bool has_info = object_info_out(objects[i]);
                new_paragraph = false;

                if (!object_known_p(objects[i]))
                {
                    p_text_out("\n\n   This item has not been identified.");
                }
                else if ((!has_description) && (!has_info))
                {
                    p_text_out(
                        "\n\n   This item does not seem to possess any special abilities.");
                }
            }
        }
        else
        {
            p_text_out("\n   (slot is empty)");
        }
    }
}

static void object_info_screen_capture_free(
    object_info_screen_capture* capture)
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

static void object_info_overlay_capture_release(void)
{
    if (!object_info_overlay_capture_active)
        return;

    object_info_screen_capture_free(&object_info_overlay_capture);
    object_info_overlay_capture_active = false;
}

void object_info_overlay_clear(void)
{
    sdl_description_overlay_clear();
    object_info_overlay_capture_release();
}

static int object_info_screen_capture_used_rows(term* t)
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

static int object_info_screen_capture_used_cols(term* t, int rows)
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

static bool object_info_screen_capture_build(
    const object_type** objects, const char** headings, int count,
    object_info_screen_capture* capture, int preferred_width,
    bool use_story_font, bool interactive)
{
    term scratch;
    term* saved_term = Term;
    void (*old_hook)(byte, cptr) = text_out_hook;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    bool old_paragraph = new_paragraph;
    story_font_term_state story_state;
    bool story_pushed = false;
    bool scratch_ready = false;
    bool success = false;
    int term_wid = 80;
    int term_hgt = 24;
    int target_wid;
    int capture_hgt;
    int used_rows;
    int used_cols;

    if (!capture || !objects || count <= 0 || !saved_term)
        return false;

    SDL_memset(capture, 0, sizeof(*capture));
    SDL_memset(&scratch, 0, sizeof(scratch));

    Term_get_size(&term_wid, &term_hgt);
    target_wid = sdl_description_overlay_capture_cols(term_wid,
        interactive);
    if (target_wid < 20)
        target_wid = 20;
    term_wid = use_story_font ? preferred_width : target_wid;
    if (term_wid < target_wid)
        term_wid = target_wid;
    if (term_wid < 20)
        term_wid = 20;
    if (term_hgt < 24)
        term_hgt = 24;

    capture_hgt = MAX(OBJECT_INFO_CAPTURE_MIN_ROWS, term_hgt);
    capture_hgt =
        MAX(capture_hgt, count * OBJECT_INFO_CAPTURE_ROWS_PER_OBJECT);
    if (capture_hgt > OBJECT_INFO_CAPTURE_MAX_ROWS)
        capture_hgt = OBJECT_INFO_CAPTURE_MAX_ROWS;

    if (term_init(&scratch, term_wid, capture_hgt, 16) != 0)
        goto cleanup;
    scratch_ready = true;

    Term_activate(&scratch);
    story_font_term_push_slot(use_story_font, false,
        STORY_FONT_SLOT_SECONDARY, &story_state);
    story_pushed = true;

    text_out_hook = use_story_font
        ? text_out_to_screen_overlay_story : text_out_to_screen;
    text_out_wrap = use_story_font ? target_wid : 0;
    text_out_indent = 0;
    new_paragraph = false;

    Term_clear();
    Term_gotoxy(0, 0);
    object_info_screen_multi_body(objects, headings, count, true);

    used_rows = object_info_screen_capture_used_rows(Term);
    if (used_rows < 1)
        used_rows = 1;
    used_cols = object_info_screen_capture_used_cols(Term, used_rows);
    if (used_cols < 1)
        used_cols = 1;

    capture->width = used_cols;
    capture->height = used_rows;
    capture->target_cols = target_wid;
    capture->cursor_x = scratch.scr->cx;
    capture->cursor_y = scratch.scr->cy;
    capture->cursor_useless = scratch.scr->cu;
    capture->cursor_visible = scratch.scr->cv;
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
    new_paragraph = old_paragraph;

    if (story_pushed)
        story_font_term_pop(&story_state);

    if (saved_term && Term != saved_term)
        Term_activate(saved_term);

    if (scratch_ready)
        term_nuke(&scratch);

    if (!success)
        object_info_screen_capture_free(capture);

    return success;
}

bool object_info_overlay_show_multi(const object_type** objects,
    const char** headings, int count)
{
    object_info_screen_capture capture;
    int overlay_width;
    bool use_story_font;

    if (count <= 0 || objects == NULL)
        return false;

    if (object_info_blocked_by_hallucination())
        return false;

    SDL_memset(&capture, 0, sizeof(capture));
    use_story_font = story_object_desc_enabled();
    overlay_width = object_info_screen_preferred_capture_width(use_story_font);

    if (!object_info_screen_capture_build(objects, headings, count, &capture,
            overlay_width, use_story_font, false))
        return false;

    object_info_overlay_capture_release();
    object_info_overlay_capture = capture;
    object_info_overlay_capture_active = true;

    if (!sdl_description_overlay_present(object_info_overlay_capture.attrs,
            object_info_overlay_capture.chars,
            object_info_overlay_capture.tattrs,
            object_info_overlay_capture.tchars,
            object_info_overlay_capture.story,
            NULL,
            object_info_overlay_capture.width,
            object_info_overlay_capture.height,
            object_info_overlay_capture.target_cols, 0, false, NULL, NULL))
    {
        object_info_overlay_clear();
        return false;
    }

    return true;
}

static bool object_info_screen_key_is_action(int key,
    const object_info_screen_action* actions, int action_count)
{
    if (!actions || action_count <= 0)
        return false;

    for (int i = 0; i < action_count; i++)
    {
        if (actions[i].key == key)
            return true;
    }

    return false;
}

static bool object_info_screen_action_key_available(int key,
    const object_info_screen_action* actions, int action_count)
{
    if (!actions || action_count <= 0)
        return true;

    for (int i = 0; i < action_count; i++)
    {
        if (actions[i].key == key)
            return false;
    }

    return true;
}

static void object_info_understanding_action_token(char* buf, size_t buflen,
    int count)
{
    if (!buf || buflen == 0)
        return;

    if (count == 1)
    {
        strnfmt(buf, buflen, "i Identify (1 gem)");
    }
    else if (count > 999)
    {
        strnfmt(buf, buflen, "i Identify (999+ gems)");
    }
    else
    {
        strnfmt(buf, buflen, "i Identify (%d gems)", count);
    }
}

static void object_info_configure_footer(cptr footer,
    const object_info_screen_action* actions, int action_count)
{
    sdl_description_overlay_set_footer(footer, footer && footer[0]);
    sdl_description_overlay_clear_footer_actions();

    if (!actions || action_count <= 0)
        return;

    for (int i = 0; i < action_count; i++)
        sdl_description_overlay_add_footer_action(actions[i].key,
            actions[i].token);
}

static char object_info_screen_capture_view(
    const object_info_screen_capture* capture, cptr footer,
    const object_info_screen_action* actions, int action_count)
{
    int scroll = 0;
    bool overlay_active = false;
    char result = 0;

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
        object_info_configure_footer(footer, actions, action_count);
        if (!sdl_description_overlay_present(capture->attrs, capture->chars,
                capture->tattrs, capture->tchars, capture->story,
                NULL,
                capture->width, capture->height, capture->target_cols, scroll,
                true, &visible_rows, &max_scroll))
        {
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

        if (ch == UI_MENU_CLICK_WAKE_KEY)
        {
            continue;
        }
        else if (ch == ESCAPE)
        {
            break;
        }
        else if (object_info_screen_key_is_action(ch, actions, action_count))
        {
            result = ch;
            break;
        }
        else if ((ch == '8') || (ch == '='))
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
            break;
        }
    }

    if (overlay_active)
        sdl_description_overlay_clear();
    sdl_description_overlay_clear_footer_actions();
    sdl_description_overlay_set_footer(NULL, false);
    ui_scroll_area_clear();

    return result;
}

void object_info_screen_multi(const object_type** objects, const char** headings, int count)
{
    (void)object_info_screen_multi_with_actions(objects, headings, count,
        NULL, NULL, 0);
}

char object_info_screen_multi_with_actions(const object_type** objects,
    const char** headings, int count, cptr footer,
    const object_info_screen_action* actions, int action_count)
{
    object_info_screen_capture capture;
    object_info_screen_action effective_actions[OBJECT_INFO_MAX_ACTIONS];
    char effective_footer[160];
    char understanding_token[32];
    bool have_capture = false;
    bool use_understanding = false;
    char result = 0;
    int effective_action_count = 0;
    int understanding_count = 0;
    bool use_story_font;

    if (count <= 0 || objects == NULL)
        return 0;

    if (object_info_blocked_by_hallucination())
        return 0;

    SDL_memset(&capture, 0, sizeof(capture));
    use_story_font = story_object_desc_enabled();

    if (actions && action_count > 0)
    {
        effective_action_count = MIN(action_count, OBJECT_INFO_MAX_ACTIONS);
        for (int i = 0; i < effective_action_count; i++)
            effective_actions[i] = actions[i];
    }

    understanding_count =
        understanding_gem_count_for_item_description(objects[0]);
    if (understanding_count > 0
        && effective_action_count < OBJECT_INFO_MAX_ACTIONS
        && object_info_screen_action_key_available(
            OBJECT_INFO_UNDERSTANDING_KEY, effective_actions,
            effective_action_count))
    {
        object_info_understanding_action_token(understanding_token,
            sizeof(understanding_token), understanding_count);
        effective_actions[effective_action_count].key =
            OBJECT_INFO_UNDERSTANDING_KEY;
        effective_actions[effective_action_count].token = understanding_token;
        effective_action_count++;

        if (footer && footer[0])
        {
            strnfmt(effective_footer, sizeof(effective_footer), "%s  %s",
                understanding_token, footer);
        }
        else
        {
            strnfmt(effective_footer, sizeof(effective_footer), "%s",
                understanding_token);
        }
        footer = effective_footer;
    }

    character_icky++;

    have_capture =
        object_info_screen_capture_build(objects, headings, count, &capture,
            object_info_screen_preferred_capture_width(use_story_font),
            use_story_font, true);
    if (have_capture)
    {
        result = object_info_screen_capture_view(&capture, footer,
            effective_action_count ? effective_actions : NULL,
            effective_action_count);
        object_info_screen_capture_free(&capture);
    }
    else
    {
        msg_print("Could not render item description.");
    }

    character_icky--;

    text_out_hook = text_out_to_screen;
    text_out_wrap = 0;
    text_out_indent = 0;
    new_paragraph = false;

    use_understanding = (result == OBJECT_INFO_UNDERSTANDING_KEY
        && understanding_count > 0);
    if (use_understanding)
    {
        (void)do_cmd_use_understanding_gem_on_item(objects[0]);
        return 0;
    }

    return result;
}

