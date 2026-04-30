/* File: wizard2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"

#ifdef ALLOW_DEBUG

/*
 * Debug function declarations
 */
static void do_cmd_debug_complete_quest(void);
static void do_cmd_debug_orome_status(void);
static void do_cmd_debug_spawn_quest_valar(void);
static void do_cmd_debug_identify_all_items(void);

/*
 * Display the dungeon light levels.
 */

void display_light_map(void)
{
    int y, x;

    /* Redraw map */
    prt_map();

    /* Update map */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            byte a;

            int light = cave_light[y][x];

            /* Pretty colors by level */
            if (light < 0)
                a = TERM_L_DARK;
            else if (light == 0)
                a = TERM_SLATE;
            else
                a = TERM_WHITE;

            /* Display light */
            if (light >= 0)
            {
                print_rel('0' + (light % 10), a, y, x);
            }
            else
            {
                print_rel('0' + (-light % 10), a, y, x);
            }
        }
    }
}

/*
 * Display the dungeon scent levels.
 */

void display_scent_map(void)
{
    int y, x;
    byte a;
    int age;

    /* Redraw map */
    prt_map();

    /* Update map */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            age = get_scent(y, x);

            /* Must have scent */
            if (age == -1)
                continue;

            /* Pretty colors by age */
            if (age < 10)
                a = TERM_RED;
            else if (age < 20)
                a = TERM_L_RED;
            else if (age < 30)
                a = TERM_ORANGE;
            else if (age < 40)
                a = TERM_YELLOW;
            else if (age < 50)
                a = TERM_L_GREEN;
            else if (age < 60)
                a = TERM_GREEN;
            else if (age < 70)
                a = TERM_L_BLUE;
            else
                a = TERM_BLUE;

            /* Display player/floors/walls */
            if ((y == p_ptr->py) && (x == p_ptr->px))
            {
                // do nothing
            }
            // ignore closed doors
            else if (cave_any_closed_door_bold(y, x))
            {
                // do nothing
            }
            // ignore visible monsters
            else if ((cave_m_idx[y][x] > 0)
                && (&mon_list[cave_m_idx[y][x]])->ml)
            {
                // do nothing
            }
            else
            {
                print_rel('0' + (age % 10), a, y, x);
            }
        }
    }
}

/*
 * Display the dungeon noise levels.
 */

void display_noise_map(void)
{
    int y, x;
    byte a = TERM_DARK; // default to soothe compilation warnings
    int dist;
    int d;

    /* Redraw map */
    prt_map();

    /* Update map */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            if (!in_bounds(y, x))
                continue;

            // default to player noise (i.e. the distance from the player in
            // terms of how much sound decays)
            dist = flow_dist(FLOW_PLAYER_NOISE, y, x);

            // if a monster is targetted, then use a monster relevant flow
            if (p_ptr->health_who > 0)
            {
                monster_type* m_ptr = &mon_list[p_ptr->health_who];

                // if it is unwary, use its distance in turns to its wandering
                // monster destination
                if (m_ptr->alertness < ALERTNESS_ALERT)
                    dist = flow_dist(m_ptr->wandering_idx, y, x);

                // otherwise, use its distance in turns to the player
                else
                    dist = flow_dist(p_ptr->health_who, y, x);
            }

            if (dist <= 0)
                continue;

            d = (dist % 100) / 10;

            switch (d)
            {
            case 0:
                a = TERM_RED;
                break;
            case 1:
                a = TERM_RED + TERM_SHADE;
                break;
            case 2:
                a = TERM_ORANGE;
                break;
            case 3:
                a = TERM_YELLOW;
                break;
            case 4:
                a = TERM_L_GREEN;
                break;
            case 5:
                a = TERM_GREEN;
                break;
            case 6:
                a = TERM_L_BLUE;
                break;
            case 7:
                a = TERM_BLUE + TERM_SHADE;
                break;
            case 8:
                a = TERM_BLUE;
                break;
            case 9:
                a = TERM_VIOLET + TERM_SHADE;
                break;
            }

            if (dist < FLOW_MAX_DIST)
            {
                /* Display player/floors/walls */
                if ((y == p_ptr->py) && (x == p_ptr->px))
                {
                    // do nothing
                }
                // ignore closed doors
                else if (cave_any_closed_door_bold(y, x))
                {
                    // do nothing
                }
                // ignore visible monsters
                else if ((cave_m_idx[y][x] > 0)
                    && (&mon_list[cave_m_idx[y][x]])->ml)
                {
                    // do nothing
                }
                else
                {
                    print_rel('0' + (dist % 10), a, y, x);
                }
            }
        }
    }
}

/*
 * Output a long int in binary format.
 */
static void prt_binary(u32b flags, int row, int col)
{
    int i;
    u32b bitmask;

    /* Scan the flags */
    for (i = bitmask = 1; i <= 32; i++, bitmask *= 2)
    {
        /* Dump set bits */
        if (flags & bitmask)
        {
            Term_putch(col++, row, TERM_BLUE, '*');
        }

        /* Dump unset bits */
        else
        {
            Term_putch(col++, row, TERM_WHITE, '-');
        }
    }
}

/*
 * Hack -- Teleport to the target
 */
static void do_cmd_wiz_bamf(void)
{
    /* Must have a target */
    if (target_okay(0))
    {
        /* Teleport to the target */
        teleport_player_to(p_ptr->target_row, p_ptr->target_col);
    }
}

/*
 * Aux function for "do_cmd_wiz_change()"
 */
static void do_cmd_wiz_change_aux(void)
{
    int i;

    int tmp_int;

    long tmp_long;

    char tmp_val[160];

    char ppp[80];

    /* Query the stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Prompt */
        strnfmt(ppp, sizeof(ppp), "%s (%d to %d): ", stat_names[i],
            BASE_STAT_MIN, BASE_STAT_MAX);

        /* Default */
        sprintf(tmp_val, "%d", p_ptr->stat_base[i]);

        /* Query */
        if (!term_get_string(ppp, tmp_val, 4))
            return;

        /* Extract */
        tmp_int = atoi(tmp_val);

        /* Verify */
        if (tmp_int > BASE_STAT_MAX)
            tmp_int = BASE_STAT_MAX;
        else if (tmp_int < BASE_STAT_MIN)
            tmp_int = BASE_STAT_MIN;

        /* Save it */
        p_ptr->stat_base[i] = tmp_int;
        p_ptr->stat_drain[i] = 0;
    }

    /* Default */
    sprintf(tmp_val, "%ld", (long)(p_ptr->new_exp));

    /* Query */
    if (!term_get_string("Experience Pool: ", tmp_val, 10))
        return;

    /* Extract */
    tmp_long = atol(tmp_val);

    /* Verify */
    if (tmp_long < 0)
        tmp_long = 0L;

    /* Update total Exp */
    p_ptr->exp += tmp_long - p_ptr->new_exp;

    /* Save */
    p_ptr->new_exp = tmp_long;

    /* Update */
    check_experience();

    /* Default */
    sprintf(tmp_val, "%ld", (long)(p_ptr->game_type));

    /* Query */
    if (!term_get_string("Game Type: ", tmp_val, 10))
        return;

    /* Extract */
    tmp_long = atol(tmp_val);

    /* Update game type */
    p_ptr->game_type = tmp_long;
}

/*
 * Change various "permanent" player variables.
 */
static void do_cmd_wiz_change(void)
{
    /* Interact */
    do_cmd_wiz_change_aux();

    /* Redraw everything */
    do_cmd_redraw();
}

/*
 * Wizard routines for creating objects and modifying them
 *
 * This has been rewritten to make the whole procedure
 * of debugging objects much easier and more comfortable.
 *
 * Here are the low-level functions
 *
 * - wiz_display_item()
 *     display an item's debug-info
 * - wiz_create_itemtype()
 *     specify tval and sval (type and subtype of object)
 * - wiz_tweak_item()
 *     specify pval, +AC, +tohit, +todam
 *     Note that the wizard can leave this function anytime,
 *     thus accepting the default-values for the remaining values.
 *     pval comes first now, since it is most important.
 * - wiz_reroll_item()
 *     apply some magic to the item or turn it into an artefact.
 * - wiz_roll_item()
 *     Get some statistics about the rarity of an item:
 *     We create a lot of fake items and see if they are of the
 *     same type (tval and sval), then we compare pval and +AC.
 *     If the fake-item is better or equal it is counted.
 *     Note that cursed items that are better or equal (absolute values)
 *     are counted, too.
 *     HINT: This is *very* useful for balancing the game!
 * - wiz_quantity_item()
 *     change the quantity of an item, but be sane about it.
 *
 * And now the high-level functions
 * - do_cmd_wiz_play()
 *     play with an existing object
 * - wiz_create_item()
 *     create a new object
 *
 * Note -- You do not have to specify "pval" and other item-properties
 * directly. Just apply magic until you are satisfied with the item.
 *
 * Note -- For some items (such as wands, staffs, some rings, etc), you
 * must apply magic, or you will get "broken" or "uncharged" objects.
 *
 * Note -- Redefining artefacts via "do_cmd_wiz_play()" may destroy
 * the artefact.  Be careful.
 *
 * Hack -- this function will allow you to create multiple artefacts.
 * This "feature" may induce crashes or other nasty effects.
 */

/*
 * Display an item's properties
 */
static void wiz_display_item(const object_type* o_ptr)
{
    int j = 0;

    u32b f1, f2, f3;

    char buf[256];

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Clear screen */
    Term_clear();

    /* Describe fully */
    object_desc_spoil(buf, sizeof(buf), o_ptr, true, 3);

    prt(buf, 2, j);

    prt(format("kind = %-5d  level = %-4d  tval = %-5d  sval = %-5d",
            o_ptr->k_idx, k_info[o_ptr->k_idx].level, o_ptr->tval, o_ptr->sval),
        4, j);

    prt(format("number = %-3d  wgt = %-6d  prt = %dd%d    damage = %dd%d",
            o_ptr->number, o_ptr->weight, o_ptr->pd, o_ptr->ps, o_ptr->dd,
            o_ptr->ds),
        5, j);

    prt(format("pval = %-5d  toev = %-5d  tohit = %-4d", o_ptr->pval,
            o_ptr->evn, o_ptr->att),
        6, j);

    prt(format("name1 = %-4d  egoP = %-4d  egoS = %-4d  cost = %ld", o_ptr->name1,
            object_ego_prefix(o_ptr), object_ego_suffix(o_ptr),
            (long)object_value(o_ptr)),
        7, j);

    prt(format("ident = %04x  timeout = %-d", o_ptr->ident, o_ptr->timeout), 8,
        j);

    prt("+------------FLAGS1------------+", 10, j);
    prt("AFFECT..........SLAY.......BRAND", 11, j);
    prt("                ae      xxxaefcp", 12, j);
    prt("siwdcc  ssitsasmnvudotgddduclioo", 13, j);
    prt("tnieoh  trnupthgiinmrrnrrmnierli", 14, j);
    prt("rtsxna..lcfndkttmldncltggnddceds", 15, j);
    prt_binary(f1, 16, j);

    prt("+------------FLAGS2------------+", 17, j);
    prt("SUST.......IMM..RESIST.........", 18, j);
    prt("           afecpaefcpfldbc s n  ", 19, j);
    prt("siwdcc     ciloocliooeialoshnecd", 20, j);
    prt("tnieoh     ireliierliatrnnnrethi", 21, j);
    prt("rtsxna.....decdsdcedsrekdfddxhss", 22, j);
    prt_binary(f2, 23, j);

    prt("+------------FLAGS3------------+", 10, j + 32);
    prt("s   ts h     tadiiii   aiehs  hp", 11, j + 32);
    prt("lf  eefoni   egrgggg  bcnaih  vr", 12, j + 32);
    prt("we  lerler  ilgannnn  ltssdo  ym", 13, j + 32);
    prt("da reiedvo  merirrrr  eityew ccc", 14, j + 32);
    prt("itlepnelpn  ppanaefc  svaktm uuu", 15, j + 32);
    prt("ghigavaiim  aoveclio  saanyo rrr", 16, j + 32);
    prt("seteticfca  craxierl  etropd sss", 17, j + 32);
    prt("trenhstekn  tttpdced  detwes eee", 18, j + 32);
    prt_binary(f3, 19, j + 32);
}

/*
 * A structure to hold a tval and its description
 */
typedef struct tval_desc
{
    int tval;
    cptr desc;
} tval_desc;

/*
 * A list of tvals and their textual names
 */
static const tval_desc tvals[] = { { TV_SWORD, "Sword" },
    { TV_POLEARM, "Axe or Polearm" }, { TV_HAFTED, "Blunt Weapon" },
    { TV_DIGGING, "Digger" }, { TV_BOW, "Bow" }, { TV_ARROW, "Arrows" },
    { TV_RING, "Ring" }, { TV_AMULET, "Amulet" }, { TV_LIGHT, "Light" },
    { TV_SOFT_ARMOR, "Soft Armour" }, { TV_MAIL, "Mail" },
    { TV_CLOAK, "Cloak" }, { TV_SHIELD, "Shield" }, { TV_HELM, "Helm" },
    { TV_CROWN, "Crown" }, { TV_GLOVES, "Gloves" }, { TV_BOOTS, "Boots" },
    { TV_FOOD, "Food" }, { TV_POTION, "Potion" }, { TV_STAFF, "Staff" },
    { TV_HORN, "Horn" }, { TV_CHEST, "Chest" }, { TV_FLASK, "Flask" },
    { TV_SKELETON, "Skeleton" }, { TV_METAL, "Piece of Metal" },
    { TV_NOTE, "Note" }, { 0, NULL } };

/*
 * Get an object kind for creation (or zero)
 *
 * List up to 60 choices in three columns
 */
static int wiz_create_itemtype(void)
{
    int i, num, max_num;
    int col, row;
    int tval;

    cptr tval_desc;
    char ch;

    int choice[60];
    static const char choice_name[] = "abcdefghijklmnopqrst"
                                      "ABCDEFGHIJKLMNOPQRST"
                                      "0123456789:;<=>?@%&*";
    const char* cp;

    char buf[160];

    /* Clear screen */
    Term_clear();

    /* Print all tval's and their descriptions */
    for (num = 0; (num < 60) && tvals[num].tval; num++)
    {
        row = 2 + (num % 20);
        col = 30 * (num / 20);
        ch = choice_name[num];
        prt(format("[%c] %s", ch, tvals[num].desc), row, col);
    }

    /* We need to know the maximal possible tval_index */
    max_num = num;

    /* Choose! */
    if (!get_com("Create what type of object? ", &ch))
        return (0);

    /* Analyze choice */
    num = -1;
    if ((cp = strchr(choice_name, ch)) != NULL)
        num = cp - choice_name;

    /* Bail out if choice is illegal */
    if ((num < 0) || (num >= max_num))
        return (0);

    /* Base object type chosen, fill in tval */
    tval = tvals[num].tval;
    tval_desc = tvals[num].desc;

    /*** And now we go for k_idx ***/

    /* Clear screen */
    Term_clear();

    /* We have to search the whole itemlist. */
    for (num = 0, i = 1; (num < 60) && (i < z_info->k_max); i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Analyze matching items */
        if (k_ptr->tval == tval)
        {
            /* Hack -- Skip instant artefacts */
            if (k_ptr->flags3 & (TR3_INSTA_ART))
                continue;

            /* Prepare it */
            row = 2 + (num % 20);
            col = 30 * (num / 20);
            ch = choice_name[num];

            /* Get the "name" of object "i" */
            strip_name(buf, i);

            /* Print it */
            prt(format("[%c] %s", ch, buf), row, col);

            /* Remember the object index */
            choice[num++] = i;
        }
    }

    /* Me need to know the maximal possible remembered object_index */
    max_num = num;

    /* Choose! */
    if (!get_com(format("What Kind of %s? ", tval_desc), &ch))
        return (0);

    /* Analyze choice */
    num = -1;
    if ((cp = strchr(choice_name, ch)) != NULL)
        num = cp - choice_name;

    /* Bail out if choice is "illegal" */
    if ((num < 0) || (num >= max_num))
        return (0);

    /* And return successful */
    return (choice[num]);
}

/*
 * Tweak an item
 */
static void wiz_tweak_item(object_type* o_ptr)
{
    cptr p;
    char tmp_val[80];

    /* Hack -- leave artefacts alone */
    if (artefact_p(o_ptr))
        return;

    p = "Enter new 'att' setting: ";
    sprintf(tmp_val, "%d", o_ptr->att);
    if (!term_get_string(p, tmp_val, 6))
        return;
    o_ptr->att = atoi(tmp_val);
    wiz_display_item(o_ptr);

    p = "Enter new 'evn' setting: ";
    sprintf(tmp_val, "%d", o_ptr->evn);
    if (!term_get_string(p, tmp_val, 6))
        return;
    o_ptr->evn = atoi(tmp_val);
    wiz_display_item(o_ptr);

    p = "Enter new 'pval' setting: ";
    sprintf(tmp_val, "%d", o_ptr->pval);
    if (!term_get_string(p, tmp_val, 6))
        return;
    o_ptr->pval = atoi(tmp_val);
    wiz_display_item(o_ptr);

    p = "Enter new weight: ";
    sprintf(tmp_val, "%d", o_ptr->weight);
    if (!term_get_string(p, tmp_val, 6))
        return;
    o_ptr->weight = atoi(tmp_val);
    wiz_display_item(o_ptr);
}

/*
 * Apply magic to an item or turn it into an artefact. -Bernd-
 */
static void wiz_reroll_item(object_type* o_ptr)
{
    object_type* i_ptr;
    object_type object_type_body;

    char ch;

    bool changed = false;

    /* Hack -- leave artefacts alone */
    if (artefact_p(o_ptr))
        return;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Copy the object */
    object_copy(i_ptr, o_ptr);

    /* Main loop. Ask for magification and artefactification */
    while (true)
    {
        /* Display full item debug information */
        wiz_display_item(i_ptr);

        /* Ask wizard what to do. */
        if (!get_com("[a]ccept, [n]ormal, [g]ood, [e]xcellent? ", &ch))
            break;

        /* Create/change it! */
        if (ch == 'A' || ch == 'a')
        {
            changed = true;
            break;
        }

        /* Apply normal magic, but first clear object */
        else if (ch == 'n' || ch == 'N')
        {
            object_prep(i_ptr, o_ptr->k_idx);
            apply_magic(i_ptr, p_ptr->depth, false, false, false, false);
        }

        /* Apply good magic, but first clear object */
        else if (ch == 'g' || ch == 'g')
        {
            object_prep(i_ptr, o_ptr->k_idx);
            apply_magic(i_ptr, p_ptr->depth, false, true, false, false);
        }

        /* Apply great magic, but first clear object */
        else if (ch == 'e' || ch == 'e')
        {
            object_prep(i_ptr, o_ptr->k_idx);
            apply_magic(i_ptr, p_ptr->depth, false, true, true, false);
        }
    }

    /* Notice change */
    if (changed)
    {
        /* Restore the position information */
        i_ptr->iy = o_ptr->iy;
        i_ptr->ix = o_ptr->ix;
        i_ptr->next_o_idx = o_ptr->next_o_idx;
        i_ptr->marked = o_ptr->marked;

        /* Apply changes */
        object_copy(o_ptr, i_ptr);

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Combine / Reorder the pack (later) */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    }
}

/*
 * Maximum number of rolls
 */
#define TEST_ROLL 100000

/*
 * Try to create an item again. Output some statistics.    -Bernd-
 *
 * The statistics are correct now.  We acquire a clean grid, and then
 * repeatedly place an object in this grid, copying it into an item
 * holder, and then deleting the object.  We fiddle with the artefact
 * counter flags to prevent weirdness.  We use the items to collect
 * statistics on item creation relative to the initial item.
 */
static void wiz_statistics(object_type* o_ptr)
{
    long i, matches, better, worse, other;

    char ch;
    cptr quality;

    bool good, great;

    object_type* i_ptr;
    object_type object_type_body;

    cptr q = "Rolls: %ld, Matches: %ld, Better: %ld, Worse: %ld, Other: %ld";

    /* Mega-Hack -- allow multiple artefacts XXX XXX XXX */
    if (artefact_p(o_ptr))
        a_info[o_ptr->name1].cur_num = 0;

    /* Interact */
    while (true)
    {
        cptr pmt = "Roll for [n]ormal, [g]ood, or [e]xcellent treasure? ";

        /* Display item */
        wiz_display_item(o_ptr);

        /* Get choices */
        if (!get_com(pmt, &ch))
            break;

        if (ch == 'n' || ch == 'N')
        {
            good = false;
            great = false;
            quality = "normal";
        }
        else if (ch == 'g' || ch == 'G')
        {
            good = true;
            great = false;
            quality = "good";
        }
        else if (ch == 'e' || ch == 'E')
        {
            good = false;
            great = true;
            quality = "excellent";
        }
        else
        {
            break;
        }

        /* Let us know what we are doing */
        msg_format("Creating a lot of %s items. Base level = %d.", quality,
            p_ptr->depth);
        message_flush();

        /* Set counters to zero */
        matches = better = worse = other = 0;

        /* Let's rock and roll */
        for (i = 0; i <= TEST_ROLL; i++)
        {
            /* Output every few rolls */
            if ((i < 100) || (i % 100 == 0))
            {
                /* Do not wait */
                inkey_scan = true;

                /* Allow interupt */
                if (inkey())
                {
                    /* Flush */
                    flush();

                    /* Stop rolling */
                    break;
                }

                /* Dump the stats */
                prt(format(q, i, matches, better, worse, other), 0, 0);
                Term_fresh();
            }

            /* Get local object */
            i_ptr = &object_type_body;

            /* Wipe the object */
            object_wipe(i_ptr);

            drop_quality quality = drop_quality_from_flags(good, great, false);

            /* Create an object */
            make_object(i_ptr, quality, DROP_TYPE_UNTHEMED);

            /* Mega-Hack -- allow multiple artefacts XXX XXX XXX */
            if (artefact_p(i_ptr))
                a_info[i_ptr->name1].cur_num = 0;

            /* Test for the same tval and sval. */
            if ((o_ptr->tval) != (i_ptr->tval))
                continue;
            if ((o_ptr->sval) != (i_ptr->sval))
                continue;

            /* Check for match */
            if ((i_ptr->pval == o_ptr->pval) && (i_ptr->evn == o_ptr->evn)
                && (i_ptr->att == o_ptr->att))
            {
                matches++;
            }

            /* Check for better */
            else if ((i_ptr->pval >= o_ptr->pval) && (i_ptr->evn >= o_ptr->evn)
                && (i_ptr->att >= o_ptr->att))
            {
                better++;
            }

            /* Check for worse */
            else if ((i_ptr->pval <= o_ptr->pval) && (i_ptr->evn <= o_ptr->evn)
                && (i_ptr->att <= o_ptr->att))
            {
                worse++;
            }

            /* Assume different */
            else
            {
                other++;
            }
        }

        /* Final dump */
        msg_format(q, i, matches, better, worse, other);
        message_flush();
    }

    /* Hack -- Normally only make a single artefact */
    if (artefact_p(o_ptr))
        a_info[o_ptr->name1].cur_num = 1;
}

/*
 * Change the quantity of a the item
 */
static void wiz_quantity_item(object_type* o_ptr)
{
    int tmp_int;

    char tmp_val[8]; /* Increased buffer size to prevent truncation warnings */

    /* Never duplicate artefacts */
    if (artefact_p(o_ptr))
        return;

    /* Default */
    snprintf(tmp_val, sizeof(tmp_val), "%d", o_ptr->number);

    /* Query */
    if (term_get_string("Quantity: ", tmp_val, sizeof(tmp_val) - 1))
    {
        /* Extract */
        tmp_int = atoi(tmp_val);

        /* Paranoia */
        if (tmp_int < 1)
            tmp_int = 1;
        if (tmp_int > 99)
            tmp_int = 99;

        /* Accept modifications */
        o_ptr->number = tmp_int;
    }
}

/*
 * Play with an item. Options include:
 *   - Output statistics (via wiz_roll_item)
 *   - Reroll item (via wiz_reroll_item)
 *   - Change properties (via wiz_tweak_item)
 *   - Change the number of items (via wiz_quantity_item)
 */
static void do_cmd_wiz_play(void)
{
    int item;

    object_type* i_ptr;
    object_type object_type_body;

    object_type* o_ptr;

    char ch;

    cptr q, s;

    bool changed = false;

    /* Get an item */
    q = "Play with which object? ";
    s = "You have nothing to play with.";
    if (!get_item(&item, q, s, (USE_EQUIP | USE_INVEN | USE_FLOOR)))
        return;

    /* Get the item (in the pack) */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Get the item (on the floor) */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    /* Save screen */
    screen_save();

    /* Get local object */
    i_ptr = &object_type_body;

    /* Copy object */
    object_copy(i_ptr, o_ptr);

    /* The main loop */
    while (true)
    {
        /* Display the item */
        wiz_display_item(i_ptr);

        /* Get choice */
        if (!get_com(
                "[a]ccept [s]tatistics [r]eroll [t]weak [q]uantity? ", &ch))
            break;

        if (ch == 'A' || ch == 'a')
        {
            changed = true;
            break;
        }

        if (ch == 's' || ch == 'S')
        {
            wiz_statistics(i_ptr);
        }

        if (ch == 'r' || ch == 'r')
        {
            wiz_reroll_item(i_ptr);
        }

        if (ch == 't' || ch == 'T')
        {
            wiz_tweak_item(i_ptr);
        }

        if (ch == 'q' || ch == 'Q')
        {
            wiz_quantity_item(i_ptr);
        }
    }

    /* Load screen */
    screen_load();

    /* Accept change */
    if (changed)
    {
        /* Message */
        msg_print("Changes accepted.");

        /* Change */
        object_copy(o_ptr, i_ptr);

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Combine / Reorder the pack (later) */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    }

    /* Ignore change */
    else
    {
        msg_print("Changes ignored.");
    }
}

/*
 * Auxilliary Wizard routine for creating objects
 *
 * Hack -- this routine always makes a "dungeon object", and applies
 * magic to it, and attempts to decline cursed items. XXX XXX XXX
 */
static void wiz_create_item_aux(int k_idx, int y, int x)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Create the item */
    object_prep(i_ptr, k_idx);

    /* Apply magic (no messages, no artefacts) */
    apply_magic(i_ptr, p_ptr->depth, false, false, false, false);

    // apply the autoinscription (if any)
    apply_autoinscription(i_ptr);

    if (i_ptr->tval == TV_ARROW)
        i_ptr->number = 24;

    /* Drop the object from heaven */
    drop_near(i_ptr, -1, y, x);
}

/*
 * Wizard routine for creating objects
 *
 * Note that wizards cannot create objects on top of other objects.
 *
 * Hack -- this routine always makes a "dungeon object", and applies
 * magic to it, and attempts to decline cursed items. XXX XXX XXX
 */
static void wiz_create_item(int num)
{
    int k_idx;

    int i;

    /* Save screen */
    screen_save();

    /* Get object base type */
    k_idx = wiz_create_itemtype();

    /* Load screen */
    screen_load();

    /* Return if failed */
    if (!k_idx)
        return;

    for (i = 0; i < num; i++)
    {
        wiz_create_item_aux(k_idx, p_ptr->py, p_ptr->px);
    }

    /* All done */
    msg_print("Allocated.");
}

/*
 * Create the artefact with the specified number
 */
static void wiz_create_artefact(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];

    /* Ignore "empty" artefacts */
    if (a_ptr->tval + a_ptr->sval == 0)
        return;

    create_chosen_artefact(a_idx, p_ptr->py, p_ptr->px, false);

    return;
}

/*
 * Cure everything instantly
 */
static void do_cmd_wiz_cure_all(void)
{
    /* Remove curses */
    (void)remove_curse(true);

    /* Restore stats */
    (void)res_stat(A_STR, 20);
    (void)res_stat(A_CON, 20);
    (void)res_stat(A_DEX, 20);
    (void)res_stat(A_GRA, 20);

    /* Heal the player */
    p_ptr->chp = p_ptr->mhp;
    p_ptr->chp_frac = 0;

    /* Restore mana */
    p_ptr->csp = p_ptr->msp;
    p_ptr->csp_frac = 0;

    /* Cure stuff */
    (void)set_blind(0);
    (void)set_confused(0);
    (void)set_poisoned(0);
    (void)set_afraid(0);
    (void)set_entranced(0);
    (void)set_image(0);
    (void)set_stun(0);
    (void)set_cut(0);
    (void)set_slow(0);

    /* No longer hungry */
    (void)set_food(PY_FOOD_FULL - 1);

    /* Redraw everything */
    do_cmd_redraw();
}

/*
 * Go to any level
 */
static void do_cmd_wiz_jump(void)
{
    /* Ask for level */
    if (p_ptr->command_arg <= 0)
    {
        char ppp[80];

        char tmp_val[160];

        /* Prompt */
        sprintf(ppp, "Jump to level (0-%d): ", MORGOTH_DEPTH);

        /* Default */
        sprintf(tmp_val, "%d", p_ptr->depth);

        /* Ask for a level */
        if (!term_get_string(ppp, tmp_val, 11))
            return;

        /* Extract request */
        p_ptr->command_arg = atoi(tmp_val);
    }

    /* Paranoia */
    if (p_ptr->command_arg < 0)
        p_ptr->command_arg = 0;

    /* Paranoia */
    if (p_ptr->command_arg > MORGOTH_DEPTH)
        p_ptr->command_arg = MORGOTH_DEPTH;

    /* Accept request */
    msg_format("You jump to dungeon level %d.", p_ptr->command_arg);

    // make a note if the player loses a greater vault
    note_lost_greater_vault();

    /* New depth */
    p_ptr->depth = p_ptr->command_arg;

    /* Leaving */
    p_ptr->leaving = true;
}

/*
 * Tile test.
 */
static void do_cmd_wiz_tile_test(void)
{
    int item_index = 1;
    int monster_index = 1;
    int artefact_index = 1;
    int forge_count = FEAT_FORGE_TAIL - FEAT_FORGE_HEAD + 1;

    /* Accept request */
    msg_format("Clearing level to display tiles");

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            cave_feat[y][x] = FEAT_FLOOR;
            cave_info[y][x] = CAVE_GLOW | CAVE_ROOM | CAVE_MARK;
        }
    }

    for (int i = 0; i < forge_count; ++i)
    {
        cave_feat[2][2 + i] = FEAT_FORGE_HEAD + i;
    }

    cave_feat[3][2] = FEAT_SECRET;
    cave_feat[3][3] = FEAT_RUBBLE;
    cave_feat[3][4] = FEAT_QUARTZ;
    cave_feat[3][5] = FEAT_WALL_EXTRA;
    cave_feat[3][6] = FEAT_WALL_PERM;

    cave_feat[3][7] = FEAT_LESS;
    cave_feat[3][8] = FEAT_MORE;
    cave_feat[3][9] = FEAT_LESS_SHAFT;
    cave_feat[3][10] = FEAT_MORE_SHAFT;

    for (int i = 2; i < 7; ++i)
        cave_info[3][i] = CAVE_WALL;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        delete_object_idx(i);
    }

    for (int o_idx = 1; o_idx < 500; o_idx++)
    {
        object_kind* k_ptr = &k_info[o_idx];

        if (k_ptr && k_ptr->tval > 0)
        {
            item_index++;

            int y = item_index / 30 + 5;
            int x = item_index % 30 + 2;

            wiz_create_item_aux(o_idx, y, x);
        }
    }

    for (int r_idx = 1; r_idx < 500; r_idx++)
    {
        monster_race* r_ptr = &r_info[r_idx];

        if (r_ptr && r_ptr->name)
        {
            r_ptr->light = 0;
            monster_index++;

            int y = monster_index / 30 + 13;
            int x = monster_index % 30 + 2;

            place_monster_one(y, x, r_idx, false, false, NULL);
        }
    }

    for (int a_idx = 1; a_idx < 180; a_idx++)
    {
        artefact_type* a_ptr = &a_info[a_idx];

        if (a_ptr->tval + a_ptr->sval > 0)
        {
            artefact_index++;

            int y = artefact_index / 30 + 19;
            int x = artefact_index % 30 + 2;

            create_chosen_artefact(a_idx, y, x, false);
        }
    }

    // Identify all new items we've created
    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        object_aware(o_ptr);
        object_known(o_ptr);
    }

    map_area();

    teleport_player_to(14, 16);

    /* Fully update the visuals */
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
}

static bool debug_grid_already_used(
    int y, int x, const int* used_y, const int* used_x, int used_n)
{
    for (int i = 0; i < used_n; i++)
    {
        if ((used_y[i] == y) && (used_x[i] == x))
            return true;
    }

    return false;
}

static bool debug_quest_vala_grid_ok(
    int y, int x, const int* used_y, const int* used_x, int used_n)
{
    if (!in_bounds_fully(y, x))
        return false;

    if ((y == p_ptr->py) && (x == p_ptr->px))
        return false;

    if (cave_m_idx[y][x] < 0)
        return false;

    return !debug_grid_already_used(y, x, used_y, used_x, used_n);
}

static bool debug_find_quest_vala_grid(
    int preferred_y, int preferred_x, const int* used_y, const int* used_x,
    int used_n, int* out_y, int* out_x)
{
    if (debug_quest_vala_grid_ok(preferred_y, preferred_x, used_y, used_x,
            used_n))
    {
        *out_y = preferred_y;
        *out_x = preferred_x;
        return true;
    }

    for (int radius = 2; radius <= 8; radius++)
    {
        int y_min = MAX(1, p_ptr->py - radius);
        int y_max = MIN(p_ptr->cur_map_hgt - 2, p_ptr->py + radius);
        int x_min = MAX(1, p_ptr->px - radius);
        int x_max = MIN(p_ptr->cur_map_wid - 2, p_ptr->px + radius);

        for (int y = y_min; y <= y_max; y++)
        {
            for (int x = x_min; x <= x_max; x++)
            {
                int dy = (y > p_ptr->py) ? (y - p_ptr->py) : (p_ptr->py - y);
                int dx = (x > p_ptr->px) ? (x - p_ptr->px) : (p_ptr->px - x);

                if ((dy <= 1) && (dx <= 1))
                    continue;

                if (!debug_quest_vala_grid_ok(y, x, used_y, used_x, used_n))
                    continue;

                *out_y = y;
                *out_x = x;
                return true;
            }
        }
    }

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (!debug_quest_vala_grid_ok(y, x, used_y, used_x, used_n))
                continue;

            *out_y = y;
            *out_x = x;
            return true;
        }
    }

    return false;
}

/*
 * Force-place one monster for debug tile inspection.
 *
 * This deliberately bypasses the normal monster placement path, including
 * depth, quest, special-generation, unique count, and terrain checks.
 */
static bool debug_force_place_monster(int y, int x, int r_idx)
{
    monster_type monster_type_body;
    monster_type* n_ptr = &monster_type_body;
    monster_race* r_ptr;
    s16b m_idx;

    if (!in_bounds_fully(y, x))
        return false;

    if ((r_idx <= 0) || (r_idx >= z_info->r_max))
        return false;

    r_ptr = &r_info[r_idx];
    if (!r_ptr->name)
        return false;

    if (cave_m_idx[y][x] > 0)
        delete_monster_idx(cave_m_idx[y][x]);

    if (cave_m_idx[y][x] != 0)
        return false;

    cave_set_feat(y, x, FEAT_FLOOR);
    cave_info[y][x] = CAVE_GLOW | CAVE_ROOM | CAVE_MARK;

    memset(n_ptr, 0, sizeof(monster_type));

    n_ptr->r_idx = r_idx;
    n_ptr->image_r_idx = r_idx;
    n_ptr->maxhp = MAX(1, r_ptr->hdice * (1 + r_ptr->hside) / 2);
    n_ptr->hp = n_ptr->maxhp;
    n_ptr->alertness = ALERTNESS_ALERT;
    n_ptr->mana = MON_MANA_MAX;
    n_ptr->song = SNG_NOTHING;
    n_ptr->energy = 0;
    n_ptr->stance = STANCE_CONFIDENT;
    n_ptr->mflag = MFLAG_MARK | MFLAG_SHOW;

    m_idx = monster_place(y, x, n_ptr);
    if (!m_idx)
        return false;

    n_ptr = &mon_list[m_idx];
    n_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

    repair_mflag_mark = true;
    repair_mflag_show = true;

    calc_monster_speed(y, x);
    update_mon(m_idx, true);
    lite_spot(y, x);

    log_debug("Debug: force-spawned quest Vala '%s' r_idx=%d at (%d,%d)",
        r_name + r_ptr->name, r_idx, y, x);

    return true;
}

static void do_cmd_debug_spawn_quest_valar(void)
{
    static const int quest_valar_r_idx[] = {
        R_IDX_TULKAS,
        R_IDX_AULE,
        R_IDX_MANDOS,
        R_IDX_NIENA,
        R_IDX_OROME,
        R_IDX_VARDA,
    };

    int used_y[N_ELEMENTS(quest_valar_r_idx)] = {0};
    int used_x[N_ELEMENTS(quest_valar_r_idx)] = {0};
    int total = (int)N_ELEMENTS(quest_valar_r_idx);
    int placed = 0;
    int row = p_ptr->py - 2;
    int max_start_x = p_ptr->cur_map_wid - 1 - total;
    int start_x;

    if (max_start_x < 1)
        max_start_x = 1;

    start_x = MAX(1, MIN(p_ptr->px - (total / 2), max_start_x));

    if (!in_bounds_fully(row, start_x))
        row = p_ptr->py + 2;

    if (!in_bounds_fully(row, start_x))
        row = MAX(1, MIN(p_ptr->py, p_ptr->cur_map_hgt - 2));

    for (int i = 0; i < total; i++)
    {
        int y;
        int x;

        if (!debug_find_quest_vala_grid(row, start_x + i, used_y, used_x,
                placed, &y, &x))
        {
            log_warn("Debug: failed to find a grid for quest Vala r_idx=%d",
                quest_valar_r_idx[i]);
            continue;
        }

        if (!debug_force_place_monster(y, x, quest_valar_r_idx[i]))
        {
            log_warn("Debug: failed to force-spawn quest Vala r_idx=%d at (%d,%d)",
                quest_valar_r_idx[i], y, x);
            continue;
        }

        used_y[placed] = y;
        used_x[placed] = x;
        placed++;
    }

    if (placed == total)
        msg_format("Spawned %d quest Valar for tile inspection.", placed);
    else if (placed > 0)
        msg_format("Spawned %d of %d quest Valar for tile inspection.", placed,
            total);
    else
        msg_print("Failed to spawn quest Valar.");

    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
    p_ptr->redraw |= (PR_MAP);
    p_ptr->window |= (PW_MONSTER);
}

/*
 * Become unaware of objects, monster memory, and the map
 */
static void do_cmd_wiz_forget(void)
{
    int i;

    /* Forget info about objects on the map */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        object_kind* k_ptr = &k_info[o_ptr->k_idx];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        // identify non-special non-artefact weapons/armour
        switch (o_ptr->tval)
        {
        case TV_DIGGING:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        case TV_BOW:
        case TV_ARROW:
        case TV_MAIL:
        case TV_SOFT_ARMOR:
        case TV_SHIELD:
        case TV_HELM:
        case TV_CROWN:
        case TV_CLOAK:
        case TV_GLOVES:
        case TV_BOOTS:
        case TV_LIGHT:
        {
            if (!o_ptr->name1 && !object_has_ego(o_ptr))
            {
                /* Identify it */
                object_aware(o_ptr);
                object_known(o_ptr);
                break;
            }
            __attribute__((fallthrough));
        }
        default:
        {
            if (!(k_ptr->flags3 & (TR3_EASY_KNOW)))
            {
                o_ptr->ident &= ~(IDENT_KNOWN);
            }
        }
        }

        /* Hack -- Clear the "empty" flag */
        o_ptr->ident &= ~(IDENT_EMPTY);

        // re pseudo id
        pseudo_id(o_ptr);
    }

    /* Forget info about carried objects */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        object_kind* k_ptr = &k_info[o_ptr->k_idx];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        // identify non-special non-artefact weapons/armour
        switch (o_ptr->tval)
        {
        case TV_DIGGING:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        case TV_BOW:
        case TV_ARROW:
        case TV_MAIL:
        case TV_SOFT_ARMOR:
        case TV_SHIELD:
        case TV_HELM:
        case TV_CROWN:
        case TV_CLOAK:
        case TV_GLOVES:
        case TV_BOOTS:
        case TV_LIGHT:
        {
            if (!o_ptr->name1 && !object_has_ego(o_ptr))
            {
                /* Identify it */
                object_aware(o_ptr);
                object_known(o_ptr);
                break;
            }
            __attribute__((fallthrough));
        }
        default:
        {
            if (!(k_ptr->flags3 & (TR3_EASY_KNOW)))
            {
                o_ptr->ident &= ~(IDENT_KNOWN);
            }
        }
        }

        /* Hack -- Clear the "empty" flag */
        o_ptr->ident &= ~(IDENT_EMPTY);

        // re pseudo id
        pseudo_id(o_ptr);
    }

    /* Reset the object kinds */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Reset "tried" */
        k_ptr->tried = false;

        /* Reset "aware" */
        k_ptr->aware = false;
    }

    /* Reset the special objects */
    for (i = 1; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];

        /* Reset "aware" */
        e_ptr->aware = false;
    }

    /* Forget encountered monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        m_ptr->encountered = false;
    }

    /* Reset the monster memory */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_lore* l_ptr = &l_list[i];

        memset(l_ptr, 0, sizeof(monster_lore));
    }

    /* Mega-Hack -- Forget the map */
    wiz_dark();

    // lose all spare experience
    // lose_exp(10000);

    // clear the cheat flags
    p_ptr->noscore = 0x0000;

    /* Forget turns */
    turn = 1;
    playerturn = 1;
    min_depth_counter = 0;

    // forget all messages -- currently a bit buggy
    messages_init();

    // clear target
    target_set_monster(0);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

/*
 * Summon some creatures
 */
static void do_cmd_wiz_summon(int num)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i;

    for (i = 0; i < num; i++)
    {
        (void)summon_specific(py, px, p_ptr->depth, 0);
    }
}

/*
 * Summon a creature of the specified type
 *
 * This function is rather dangerous XXX XXX XXX
 */
static void do_cmd_wiz_named(int r_idx, bool slp)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, x, y;

    /* Paranoia */
    if (!r_idx)
        return;
    if (r_idx >= z_info->r_max - 1)
        return;

    /* Try 10 times */
    for (i = 0; i < 10; i++)
    {
        int d = 1;

        /* Pick a location */
        scatter(&y, &x, py, px, d, 0);

        /* Require empty grids */
        if (!cave_empty_bold(y, x))
            continue;

        /* Place it (allow groups) */
        if (place_monster_aux(y, x, r_idx, slp, true))
            break;
    }
}

/*
 * Hack -- Delete all nearby monsters
 */
static void do_cmd_wiz_zap(int d)
{
    int i;

    /* Banish everyone nearby */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Skip uniques */
        // if ((&r_info[m_ptr->r_idx])->flags1 & (RF1_UNIQUE)) continue;

        /* Skip distant monsters */
        if (m_ptr->cdis > d)
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }
}

/*
 * Un-hide all monsters
 */
extern void do_cmd_wiz_unhide(int d)
{
    int i;

    /* Process monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Skip distant monsters */
        if (m_ptr->cdis > d)
            continue;

        /* Optimize -- Repair flags */
        repair_mflag_mark = true;
        repair_mflag_show = true;

        /* Detect the monster */
        m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

        /* Update the monster */
        update_mon(i, false);
    }
}

/*
 * Query the dungeon
 */
static void do_cmd_wiz_query(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    char cmd;

    u16b mask = 0x00;

    /* Get a "debug command" */
    if (!get_com("Debug Command Query: ", &cmd))
        return;

    /* Extract a flag */
    switch (cmd)
    {
    case '0':
        mask = (1 << 0);
        break;
    case '1':
        mask = (1 << 1);
        break;
    case '2':
        mask = (1 << 2);
        break;
    case '3':
        mask = (1 << 3);
        break;
    case '4':
        mask = (1 << 4);
        break;
    case '5':
        mask = (1 << 5);
        break;
    case '6':
        mask = (1 << 6);
        break;
    case '7':
        mask = (1 << 7);
        break;

    case 'm':
        mask |= (CAVE_MARK);
        break;
    case 'g':
        mask |= (CAVE_GLOW);
        break;
    case 'r':
        mask |= (CAVE_ROOM);
        break;
    case 'i':
        mask |= (CAVE_ICKY);
        break;
    case 'h':
        mask |= (CAVE_HIDDEN);
        break;
    case 's':
        mask |= (CAVE_SEEN);
        break;
    case 'v':
        mask |= (CAVE_VIEW);
        break;
    case 't':
        mask |= (CAVE_TEMP);
        break;
    case 'w':
        mask |= (CAVE_WALL);
        break;
    case 'f':
        mask |= (CAVE_FIRE);
        break;
    case 'V':
        mask |= (CAVE_G_VAULT);
        break;
    }

    /* Scan map */
    for (y = p_ptr->wy; y < p_ptr->wy + SCREEN_HGT; y++)
    {
        for (x = p_ptr->wx; x < p_ptr->wx + SCREEN_WID; x++)
        {
            byte a = TERM_RED;

            if (!in_bounds_fully(y, x))
                continue;

            /* Given mask, show only those grids */
            if (mask && !(cave_info[y][x] & mask))
                continue;

            /* Given no mask, show unknown grids */
            if (!mask && (cave_info[y][x] & (CAVE_MARK)))
                continue;

            /* Color */
            if (cave_floor_bold(y, x))
                a = TERM_YELLOW;

            /* Display player/floors/walls */
            if ((y == py) && (x == px))
            {
                print_rel('@', a, y, x);
            }
            else if (cave_floor_bold(y, x))
            {
                print_rel('*', a, y, x);
            }
            else
            {
                print_rel('#', a, y, x);
            }
        }
    }

    /* Get keypress */
    msg_print("Press any key.");
    message_flush();

    /* Redraw map */
    prt_map();
}

/*
 * Unlock all oaths for the current metarun
 */
static void do_cmd_wiz_unlock_all_oaths(void)
{
    int i;
    int count = 0;
    int already_unlocked = 0;
    
    /* Unlock oaths 1 through oath_max-1 (OATH_MERCY, OATH_SILENCE, OATH_IRON, OATH_SMITH, OATH_VALOROUS) */
    for (i = 1; z_info && i < z_info->oath_max; i++)
    {
        if (!oath_unlocked(i))
        {
            metarun_unlock_oath(i);
            count++;
        }
        else
        {
            already_unlocked++;
        }
    }
    
    /* Give feedback to the user */
    if (count > 0)
    {
        if (already_unlocked > 0)
        {
            msg_format("Unlocked %d oath%s (%d already unlocked).", 
                      count, (count == 1) ? "" : "s", already_unlocked);
        }
        else
        {
            msg_format("Unlocked all %d oaths for this metarun.", count);
        }
    }
    else
    {
        msg_print("All oaths were already unlocked for this metarun.");
    }
}

/*
 * Modify the dungeon
 */
void do_cmd_wiz_look(void)
{
    /* Look around and modify things */
    target_set_interactive(TARGET_WIZ, 0);
}

/*
 * Debug function: Complete current active quest
 */
static void do_cmd_debug_complete_quest(void)
{
    bool quest_found = false;
    
    /* Check for active quests and complete them */
    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED && p_ptr->tulkas_quest < TULKAS_QUEST_REWARDED) {
        msg_print("Completing Tulkas quest...");
        
        /* If quest is already in COMPLETE state, just give reward */
        if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE) {
            /* Tulkas is spawn-based (Y:1) - spawn quest giver near player for reward */
            if (!is_quest_giver_present(R_IDX_TULKAS)) {
                if (!spawn_quest_giver_near_player(R_IDX_TULKAS)) {
                    msg_print("Warning: Could not spawn Tulkas for reward - completing anyway.");
                }
            }
            /* Trigger quest interaction to give reward */
            tulkas_quest_interaction();
            quest_found = true;
            log_debug("Debug: Triggered Tulkas quest reward interaction");
        } else {
            /* Quest not completed yet - mark as complete and spawn giver */
            p_ptr->tulkas_quest = TULKAS_QUEST_COMPLETE;
            if (!is_quest_giver_present(R_IDX_TULKAS)) {
                if (!spawn_quest_giver_near_player(R_IDX_TULKAS)) {
                    msg_print("Warning: Could not spawn Tulkas for reward - completing anyway.");
                }
            }
            /* Trigger proper quest interaction instead of just applying rewards */
            tulkas_quest_interaction(); 
            quest_found = true;
            log_debug("Debug: Completed Tulkas quest with full interaction");
        }
    }
    
    if (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED && p_ptr->aule_quest < AULE_QUEST_REWARDED) {
        msg_print("Completing Aule quest...");
        
        /* If quest is already in SUCCESS state, just give reward */
        if (p_ptr->aule_quest == AULE_QUEST_SUCCESS) {
            /* Aule is vault-based (Y:0) - check if quest giver is present */
            if (!is_quest_giver_present(R_IDX_AULE)) {
                msg_print("Warning: Aule is not present in this vault. Go to Aule's forge vault to receive your reward.");
                quest_found = true;
                log_debug("Debug: Aule quest ready for reward but Aule not present - go to vault");
            } else {
                /* Trigger quest interaction to give reward */
                aule_quest_interaction();
                quest_found = true;
                log_debug("Debug: Triggered Aule quest reward interaction");
            }
        } else {
            /* Quest not completed yet - mark as complete and check for giver */
            if (!is_quest_giver_present(R_IDX_AULE)) {
                msg_print("Warning: Aule is not present in this vault. Go to Aule's forge vault to receive your reward.");
                /* Still mark quest as complete for the challenge completion */
                p_ptr->aule_quest = AULE_QUEST_SUCCESS;
                quest_found = true;
                log_debug("Debug: Marked Aule quest complete but Aule not present - go to vault for reward");
            } else {
                p_ptr->aule_quest = AULE_QUEST_SUCCESS;
                /* Trigger proper quest interaction instead of just applying rewards */
                aule_quest_interaction();
                quest_found = true;
                log_debug("Debug: Completed Aule quest with full interaction");
            }
        }
    }
    
    if (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED && p_ptr->mandos_quest < MANDOS_QUEST_REWARDED) {
        msg_print("Completing Mandos quest...");
        
        /* If quest is already in SUCCESS state, just give reward */
        if (p_ptr->mandos_quest == MANDOS_QUEST_SUCCESS) {
            /* Mandos is vault-based (Y:0) - check if quest giver is present */
            if (!is_quest_giver_present(R_IDX_MANDOS)) {
                msg_print("Warning: Mandos is not present in this vault. Go to Mandos' throne vault to receive your reward.");
                quest_found = true;
                log_debug("Debug: Mandos quest ready for reward but Mandos not present - go to vault");
            } else {
                /* Trigger quest interaction to give reward */
                mandos_quest_interaction();
                quest_found = true;
                log_debug("Debug: Triggered Mandos quest reward interaction");
            }
        } else {
            /* Quest not completed yet - mark as complete and check for giver */
            if (!is_quest_giver_present(R_IDX_MANDOS)) {
                msg_print("Warning: Mandos is not present in this vault. Go to Mandos' throne vault to receive your reward.");
                /* Still mark quest as complete for the challenge completion */
                p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
                quest_found = true;
                log_debug("Debug: Marked Mandos quest complete but Mandos not present - go to vault for reward");
            } else {
                p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;
                /* Trigger proper quest interaction instead of just applying rewards */
                mandos_quest_interaction();
                quest_found = true;
                log_debug("Debug: Completed Mandos quest with full interaction");
            }
        }
    }
    
    if (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED && p_ptr->niena_quest < NIENA_QUEST_REWARDED) {
        msg_print("Completing Niena quest...");
        
        /* If quest is already in SUCCESS state, just give reward */
        if (p_ptr->niena_quest == NIENA_QUEST_SUCCESS) {
            /* Niena is spawn-based (Y:1) - spawn quest giver near player for reward */
            if (!is_quest_giver_present(R_IDX_NIENA)) {
                if (!spawn_quest_giver_near_player(R_IDX_NIENA)) {
                    msg_print("Warning: Could not spawn Niena for reward - completing anyway.");
                }
            }
            /* Trigger quest interaction to give reward */
            niena_quest_interaction();
            quest_found = true;
            log_debug("Debug: Triggered Niena quest reward interaction");
        } else {
            /* Quest not completed yet - mark as complete and spawn giver */
            p_ptr->niena_quest = NIENA_QUEST_SUCCESS;
            if (!is_quest_giver_present(R_IDX_NIENA)) {
                if (!spawn_quest_giver_near_player(R_IDX_NIENA)) {
                    msg_print("Warning: Could not spawn Niena for reward - completing anyway.");
                }
            }
            /* Trigger proper quest interaction */
            niena_quest_interaction();
            quest_found = true;
            log_debug("Debug: Completed Niena quest with full interaction");
        }
    }
    
    if (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED && p_ptr->orome_quest < OROME_QUEST_REWARDED) {
        msg_print("Completing Orome quest...");
        
        /* If quest is already in SUCCESS state, just give reward */
        if (p_ptr->orome_quest == OROME_QUEST_SUCCESS) {
            /* Orome is spawn-based (Y:1) - spawn quest giver near player for reward */
            if (!is_quest_giver_present(R_IDX_OROME)) {
                if (!spawn_quest_giver_near_player(R_IDX_OROME)) {
                    msg_print("Warning: Could not spawn Orome for reward - completing anyway.");
                }
            }
            /* Trigger quest interaction to give reward */
            orome_quest_interaction();
            quest_found = true;
            log_debug("Debug: Triggered Orome quest reward interaction");
        } else {
            /* Quest not completed yet - mark as complete and spawn giver */
            p_ptr->orome_quest = OROME_QUEST_SUCCESS;
            /* Set kill count to target to simulate completion */
            p_ptr->orome_killed_count = p_ptr->orome_target_count;
            if (!is_quest_giver_present(R_IDX_OROME)) {
                if (!spawn_quest_giver_near_player(R_IDX_OROME)) {
                    msg_print("Warning: Could not spawn Orome for reward - completing anyway.");
                }
            }
            /* Trigger proper quest interaction */
            orome_quest_interaction();
            quest_found = true;
            log_debug("Debug: Completed Orome quest with full interaction");
        }
    }
    
    if (!quest_found) {
        msg_print("No active quests found to complete.");
    } else {
        msg_print("Quest(s) completed! Check your quest log and abilities menu.");
    }
}

/*
 * Debug function: Show Orome quest status and spawn probability
 */
static void do_cmd_debug_orome_status(void)
{
    char buf[1024];
    quest_type *q_ptr = &quest_info[5]; /* Orome is quest 5 */
    
    /* Show Orome quest state */
    strnfmt(buf, sizeof(buf), "Orome Quest Status: %d (0=NOT_STARTED, 1=GIVEN, 2=ACTIVE, 3=COMPLETE, 4=REWARDED)", 
            p_ptr->orome_quest);
    msg_print(buf);
    
    /* Show current depth */
    strnfmt(buf, sizeof(buf), "Current depth: %d (Orome depth range: %d-%d)", 
            p_ptr->depth, q_ptr->depth_min, q_ptr->depth_max);
    msg_print(buf);
    
    /* Show formula details */
    strnfmt(buf, sizeof(buf), "Formula type: %d (4=LINEAR_INTERPOLATE), params=[%.3f, %.3f, %.3f, %.3f]",
            q_ptr->formula_type,
            q_ptr->formula_params[0], q_ptr->formula_params[1], 
            q_ptr->formula_params[2], q_ptr->formula_params[3]);
    msg_print(buf);
    
    /* Check if we're at the right depth for Orome */
    if (p_ptr->depth >= q_ptr->depth_min && p_ptr->depth <= q_ptr->depth_max) {
        msg_print("Depth is in valid range for Orome spawning.");
    } else {
        msg_print("Depth is NOT in valid range for Orome spawning.");
    }
    
    /* Manually trigger quest lottery to see what happens */
    msg_print("Triggering quest lottery manually...");
    debug_run_quest_roulette();
    int winner = debug_get_quest_lottery_winner();
    strnfmt(buf, sizeof(buf), "Quest lottery result: %d (0=none, 1=Tulkas, 4=Niena, 5=Orome)", winner);
    msg_print(buf);
}

/*
 * Identify all items on the dungeon floor
 */
static void do_cmd_debug_identify_all_items(void)
{
    int i;
    int count = 0;
    
    /* Iterate through all floor objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        
        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;
        
        /* Skip held objects (in monster inventory) */
        if (o_ptr->held_m_idx)
            continue;
        
        /* Identify the object */
        object_aware(o_ptr);
        object_known(o_ptr);
        
        count++;
    }
    
    /* Report result */
    if (count > 0)
    {
        msg_format("Identified %d item%s on the dungeon floor.", count, (count != 1) ? "s" : "");
    }
    else
    {
        msg_print("No items found on the dungeon floor.");
    }
    
    /* Redraw map to show identified items */
    p_ptr->redraw |= (PR_MAP);
    
    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
}

/*
 * Ask for and parse a "debug command"
 *
 * The "p_ptr->command_arg" may have been set.
 */
void do_cmd_debug(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    char cmd;

    /* Get a "debug command" */
    if (!get_com("Debug Command: ", &cmd))
        return;

    /* Trace which debug command was requested and current flags */
    log_debug("do_cmd_debug: received '%c' (wizard=%d, noscore=0x%04X)",
              cmd, p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore);

    /* Analyze the command */
    switch (cmd)
    {
    /* Ignore */
    case ESCAPE:
    case ' ':
    case '\n':
    case '\r':
    {
        break;
    }

#ifdef ALLOW_SPOILERS

    /* Hack -- Generate Spoilers */
    case '"':
    {
        do_cmd_spoilers();
        break;
    }

#endif

    /* Hack -- Help */
    case '?':
    {
        do_cmd_help();
        break;
    }

    /* Cure all maladies */
    case 'a':
    {
        do_cmd_wiz_cure_all();
        break;
    }

    /* Teleport to target */
    case 'b':
    {
        do_cmd_wiz_bamf();
        break;
    }

    /* Create any object */
    case 'c':
    {
        if (p_ptr->command_arg <= 0)
            p_ptr->command_arg = 1;
        wiz_create_item(p_ptr->command_arg);
        break;
    }

    /* Create an artefact */
    case 'C':
    {
        wiz_create_artefact(p_ptr->command_arg);
        break;
    }

    /* Detect everything */
    case 'd':
    {
        detect_all_doors_traps();
        detect_all();
        break;
    }

    /* Edit character */
    case 'e':
    {
    log_debug("debug: invoking character edit (before: wizard=%d, noscore=0x%04X)",
         p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore);
        do_cmd_wiz_change();
    log_debug("debug: finished character edit (after: wizard=%d, noscore=0x%04X)",
         p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore);
        break;
    }

    /* Forget items and map and monster memory */
    case 'f':
    {
        do_cmd_wiz_forget();
        break;
    }

    /* Good Objects */
    case 'g':
    {
        if (p_ptr->command_arg <= 0)
            p_ptr->command_arg = 1;
        acquirement(py, px, p_ptr->command_arg, DROP_QUALITY_GOOD);
        break;
    }

    /* Identify */
    case 'i':
    {
        (void)ident_spell(true);
        break;
    }

    /* Identify all floor items */
    case 'I':
    {
        do_cmd_debug_identify_all_items();
        break;
    }

    /* Go up or down in the dungeon */
    case 'j':
    {
        do_cmd_wiz_jump();
        break;
    }

    /* Self-Knowledge */
    case 'k':
    {
        self_knowledge();
        break;
    }

    /* Wizard Look */
    case 'l':
    {
        do_cmd_wiz_look();
        break;
    }

    /* Magic Mapping */
    case 'm':
    {
        map_area();
        break;
    }

    /* Summon Named Monster */
    case 'n':
    {
        do_cmd_wiz_named(p_ptr->command_arg, true);
        break;
    }

    /* Object playing routines */
    case 'o':
    {
        do_cmd_wiz_play();
        break;
    }

    /* Debug Options */
    case 'O':
    {
        screen_save();
        do_cmd_options_aux(6, "Debug Options");
        screen_load();
        break;
    }

    /* Phase Door */
    case 'p':
    {
        teleport_player(10);
        break;
    }

    /* Query the dungeon */
    case 'q':
    {
        do_cmd_wiz_query();
        break;
    }

    /* Summon Random Monster(s) */
    case 's':
    {
        if (p_ptr->command_arg <= 0)
            p_ptr->command_arg = 1;
        do_cmd_wiz_summon(p_ptr->command_arg);
        break;
    }

    /* Teleport */
    case 't':
    {
        teleport_player(100);
        break;
    }

    /* Test tiles */
    case 'T':
    {
        do_cmd_wiz_tile_test();
        break;
    }

    /* Un-hide all monsters */
    case 'u':
    {
        if (p_ptr->command_arg <= 0)
            p_ptr->command_arg = 255;
        do_cmd_wiz_unhide(p_ptr->command_arg);
        break;
    }

    /* Unlock all oaths for this metarun */
    case 'U':
    {
        do_cmd_wiz_unlock_all_oaths();
        break;
    }

    /* Very Good Objects */
    case 'v':
    {
        if (p_ptr->command_arg <= 0)
            p_ptr->command_arg = 1;
        acquirement(py, px, p_ptr->command_arg, DROP_QUALITY_GREAT);
        break;
    }

    /* Wizard Light the Level */
    case 'w':
    {
        wiz_light();
        break;
    }

    /* Increase Experience */
    case 'x':
    {
        if (p_ptr->command_arg)
        {
            gain_exp(p_ptr->command_arg);
        }
        else
        {
            gain_exp(p_ptr->exp);
        }
        break;
    }

    /* Grant Unique Bane ability */
    case 'y':
    {
        grant_unique_bane_ability();
        break;
    }

    /* Zap Monsters (Banishment) */
    case 'z':
    {
        if (p_ptr->command_arg <= 0)
            p_ptr->command_arg = MAX_SIGHT;
        do_cmd_wiz_zap(p_ptr->command_arg);
        break;
    }

    /* Complete current quest */
    case '2':
    {
        do_cmd_debug_complete_quest();
        break;
    }

    /* Check Orome quest status */
    case '3':
    {
        do_cmd_debug_orome_status();
        break;
    }

    /* Spawn all quest Valar for tile inspection */
    case '4':
    {
        do_cmd_debug_spawn_quest_valar();
        break;
    }

    /* Oops */
    default:
    {
        msg_print("That is not a valid debug command.");
        break;
    }
    }
}

#else

#endif

