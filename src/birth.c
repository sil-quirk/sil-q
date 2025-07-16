/* File: birth.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "z-term.h"
#include "metarun.h"

/* Locations of the tables on the screen */
#define HEADER_ROW 1
#define QUESTION_ROW 2
#define TABLE_ROW 3
#define DESCRIPTION_ROW 16
#define INSTRUCT_ROW 23

#define QUESTION_COL 2
#define RACE_COL 2
#define RACE_AUX_COL 19
#define CLASS_COL 19
#define CLASS_AUX_COL 31
#define TOTAL_AUX_COL 35
#define INVALID_CHOICE 255

/*
 * Forward declare
 */
typedef struct birther birther;
typedef struct birth_menu birth_menu;

/*
 * A structure to hold "rolled" information
 */
struct birther
{
    s16b age;
    s16b wt;
    s16b ht;
    s16b sc;

    s16b stat[A_MAX];

    char history[250];
};

/*
 * A structure to hold the menus
 */
struct birth_menu
{
    bool ghost;
    cptr name;
    cptr text;
};

// s16b adj_c[A_MAX];

static int get_start_xp(void)
{
    if (birth_fixed_exp)
    {
        return PY_FIXED_EXP;
    }
    else
    {
        return PY_START_EXP;
    }
}

/* -----------------------------------------------------------
 * new: delegate to the (i386-safe) 2-bit accessor in metarun.h
 * --------------------------------------------------------- */
static int curse_count(int id)           /* 0-31 */
{
    return CURSE_GET(id);
}


/* Return net adjustment for a primary stat from EVERY active metarun curse */
static int curses_stat_adj(int s)   /* s = 0-3  (STR-DEX-CON-GRA) */
{
    int delta = 0;
    for (int bit = 0; bit < z_info->cu_max; bit++) {
        int cnt = curse_count(bit);
        if (cnt)
            delta += cnt * cu_info[bit].cu_adj[s];
    }
    return delta;
}



/*
 * Generate some info that the auto-roller ignores
 */
static void get_extra(void)
{
    p_ptr->new_exp = p_ptr->exp = get_start_xp();

    /* Player is not singing */
    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
}

/*
 * Get the racial history, and social class, using the "history charts".
 */
// static void get_history_aux(void)
// {
//     int i, chart, roll;

//     /* Clear the previous history strings */
//     p_ptr->history[0] = '\0';

//     /* Starting place */
//     chart = rp_ptr->hist;

//     /* Process the history */
//     while (chart)
//     {
//         /* Start over */
//         i = 0;

//         /* Roll for nobility */
//         roll = dieroll(100);

//         /* Get the proper entry in the table */
//         while ((chart != h_info[i].chart))
//             i++;

//         if (h_info[i].house)
//         {
//             while ((p_ptr->phouse != h_info[i].house) && h_info[i].house)
//                 i++;
//         }

//         while (roll > h_info[i].roll)
//             i++;

//         /* Get the textual history */
//         my_strcat(
//             p_ptr->history, (h_text + h_info[i].text), sizeof(p_ptr->history));

//         /* Add a space */
//         my_strcat(p_ptr->history, " ", sizeof(p_ptr->history));

//         /* Enter the next chart */
//         chart = h_info[i].next;
//     }
// }

// Get the character description

static void get_history_aux1(void)
{
    /* Clear the previous history strings */
p_ptr->history[0] = '\0';
my_strcat(
            p_ptr->history, (c_text + c_info[p_ptr->phouse].text), sizeof(p_ptr->history));   
}

/*
 * Get the racial history, and social class, using the "history charts".
 */
static bool get_history(void)
{
    int i;
    char line[70];
    char query2;
    int loopagain = TRUE;

    // hack to see whether we are opening an old player file
    bool roll_history = !(p_ptr->history[0]);

    while (loopagain == TRUE)
    {
        if (roll_history)
        {
            /*get the random history, display for approval. */
            get_history_aux1();
        }
        else
        {
            roll_history = TRUE;
        }

        /* Display the player */
        display_player(0);

        // Highlight relevant info
        display_player_xtra_info(2);

        /* Prompt */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            "Enter accept history");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
            "Space reroll history");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 3, -1, TERM_SLATE,
            "    m manually enter history");

        /* Hack - highlight the key names */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Enter");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Space");
        Term_putstr(QUESTION_COL + 4, INSTRUCT_ROW + 3, -1, TERM_L_WHITE, "m");

        /* Move the cursor */
        Term_gotoxy(0, INSTRUCT_ROW + 1);

        /* Query */
        query2 = inkey();

        if ((query2 == '\r') || (query2 == '\n'))
        {
            /* got a history*/
            loopagain = FALSE;

            p_ptr->redraw |= (PR_MISC);
        }

        else if ((query2 == 'm') || (query2 == 'M'))
        {
            /* don't want a random history */

            /* Clear the previous history strings */
            p_ptr->history[0] = '\0';

            /* Display the player */
            display_player(0);

            for (i = 1; i <= 3; i++)
            {
                // clear line
                line[0] = '\0';

                Term_gotoxy(1, 15 + i);

                /* Prompt for a new history */
                if (askfor_aux(line, sizeof(line)))
                {
                    /* Get the textual history */
                    my_strcat(p_ptr->history, line, sizeof(p_ptr->history));

                    /* Add a space */
                    my_strcat(p_ptr->history, "\n", sizeof(p_ptr->history));

                    p_ptr->redraw |= (PR_MISC);

                    /* Display the player */
                    display_player(0);
                }
                else
                {
                    return (FALSE);
                }
            }

            // confirm the choices
            if (!get_history())
                return (FALSE);

            loopagain = FALSE;
        }

        else if (query2 == ESCAPE)
            return (FALSE);

        else if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }

    return (TRUE);
}

/*
 * Computes character's age, height, and weight
 */
static void get_ahw_aux(void)
{
    /* Calculate the age */
    p_ptr->age = rand_range(rp_ptr->b_age, rp_ptr->m_age);

    /* Calculate the height/weight */
    p_ptr->ht = Rand_normal(rp_ptr->b_ht, rp_ptr->m_ht);
    p_ptr->wt = Rand_normal(rp_ptr->b_wt, rp_ptr->m_wt);

    // Make weight a bit proportional to height
    p_ptr->wt += p_ptr->ht / 5;
}

/*
 * Get the Age, Height and Weight.
 */
static bool get_ahw(void)
{
    char prompt[50];
    char line[70];
    char query2;
    int loopagain = TRUE;

    // hack to see whether we are opening an old player file
    bool roll_ahw = !p_ptr->age;

    // put_str("(a)ccept age/height/weight, (r)eroll, (m)anually enter ", 0, 0);

    while (loopagain == TRUE)
    {
        if (roll_ahw)
        {
            /*get the random age/height/weight, display for approval. */
            get_ahw_aux();
        }
        else
        {
            roll_ahw = TRUE;
        }

        /* Display the player */
        display_player(0);

        // Highlight relevant info
        display_player_xtra_info(1);

        /* Prompt */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            "Enter accept age/height/weight");
        Term_putstr(
            QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE, "Space reroll");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 3, -1, TERM_SLATE,
            "    m manually enter");

        /* Hack - highlight the key names */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Enter");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Space");
        Term_putstr(QUESTION_COL + 4, INSTRUCT_ROW + 3, -1, TERM_L_WHITE, "m");

        /* Move the cursor */
        Term_gotoxy(0, INSTRUCT_ROW + 1);

        /* Query */
        query2 = inkey();

        if ((query2 == '\r') || (query2 == '\n'))
        {
            /* got ahw*/
            loopagain = FALSE;

            p_ptr->redraw |= (PR_MISC);
        }

        else if ((query2 == 'm') || (query2 == 'M'))
        {
            /* don't want random stats */
            int age = 0;
            int height = 0;
            int weight = 0;
            int age_l, age_h, height_l, height_h, weight_l, weight_h;

            age_l = rp_ptr->b_age;
            age_h = rp_ptr->m_age;

            height_l = rp_ptr->b_ht - 5 * (rp_ptr->m_ht);
            height_h = rp_ptr->b_ht + 5 * (rp_ptr->m_ht);
            weight_l = rp_ptr->b_wt / 2;
            weight_h = rp_ptr->b_wt * 2;

            // clear line
            line[0] = '\0';

            while ((age < age_l) || (age > age_h))
            {
                sprintf(prompt, "Enter age (%d-%d): ", age_l, age_h);
                if (!term_get_string(prompt, line, sizeof(line)))
                    return (FALSE);
                age = atoi(line);
                p_ptr->age = age;
            }

            /* Display the player */
            p_ptr->redraw |= (PR_MISC);
            display_player(0);

            // clear line
            line[0] = '\0';

            while ((height < height_l) || (height > height_h))
            {
                sprintf(prompt, "Enter height in inches (%d-%d): ", height_l,
                    height_h);
                if (!term_get_string(prompt, line, sizeof(line)))
                    return (FALSE);
                height = atoi(line);
                p_ptr->ht = height;
            }

            /* Display the player */
            p_ptr->redraw |= (PR_MISC);
            display_player(0);

            // clear line
            line[0] = '\0';

            while ((weight < weight_l) || (weight > weight_h))
            {
                sprintf(prompt, "Enter weight in pounds (%d-%d): ", weight_l,
                    weight_h);
                if (!term_get_string(prompt, line, sizeof(line)))
                    return (FALSE);
                weight = atoi(line);
                p_ptr->wt = weight;
            }

            /* Display the player */
            p_ptr->redraw |= (PR_MISC);
            display_player(0);

            // confirm the choices
            if (!get_ahw())
                return (FALSE);

            loopagain = FALSE;
        }

        else if (query2 == ESCAPE)
            return (FALSE);

        else if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }

    return (TRUE);
}

/*
 * Clear all the global "character" data
 */
static void player_wipe(void)
{
    int i;
    char history[250];
    int stat[A_MAX];

    /* Backup the player choices */
    // Initialized to soothe compilation warnings
    byte prace = 0;
    byte phouse = 0;
    int age = 0;
    int height = 0;
    int weight = 0;

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        /* Backup the player choices */
        prace = p_ptr->prace;
        phouse = p_ptr->phouse;
        age = p_ptr->age;
        height = p_ptr->ht;
        weight = p_ptr->wt;
        sprintf(history, "%s", p_ptr->history);

        for (i = 0; i < A_MAX; i++)
        {
            if (!(p_ptr->noscore & 0x0008))
                stat[i] = p_ptr->stat_base[i]
                    - (rp_ptr->r_adj[i] + hp_ptr->h_adj[i]);
            else
                stat[i] = 0;
        }
    }

    /* Wipe the player */
    (void)WIPE(p_ptr, player_type);

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        /* Restore the choices */
        p_ptr->prace = prace;
        p_ptr->phouse = phouse;
        p_ptr->game_type = 0;
        p_ptr->age = age;
        p_ptr->ht = height;
        p_ptr->wt = weight;
        sprintf(p_ptr->history, "%s", history);
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = stat[i];
        }
    }
    else
    {
        /* Reset */
        p_ptr->prace = 0;
        p_ptr->phouse = 0;
        p_ptr->game_type = 0;
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        p_ptr->history[0] = '\0';
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }

    /* Clear the inventory */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_wipe(&inventory[i]);
    }

    /* Start with no artefacts made yet */
    /* and clear the slots for in-game randarts */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        a_ptr->cur_num = 0;
        a_ptr->found_num = 0;
    }

    /*re-set the object_level*/
    object_level = 0;

    /* Reset the "objects" */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Reset "tried" */
        k_ptr->tried = FALSE;

        /* Reset "aware" */
        k_ptr->aware = FALSE;
    }

    /* Reset the "monsters" */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Hack -- Reset the counter */
        r_ptr->cur_num = 0;

        /* Hack -- Reset the max counter */
        r_ptr->max_num = 100;

        /* Hack -- Reset the max counter */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            r_ptr->max_num = 1;

        /* Clear player sights/kills */
        l_ptr->psights = 0;
        l_ptr->pkills = 0;
    }

    /*No current player ghosts*/
    bones_selector = 0;

    // give the player the most food possible without a message showing
    p_ptr->food = PY_FOOD_FULL - 1;

    // reset the stair info
    p_ptr->stairs_taken = 0;
    p_ptr->staircasiness = 0;

    // reset the forge info
    p_ptr->fixed_forge_count = 0;
    p_ptr->forge_count = 0;

    // No vengeance at birth
    p_ptr->vengeance = 0;

    // Morgoth unhurt
    p_ptr->morgoth_state = 0;

    p_ptr->killed_enemy_with_arrow = FALSE;

    p_ptr->oath_type = 0;
    p_ptr->oaths_broken = 0;

    p_ptr->thrall_quest = QUEST_NOT_STARTED;

    p_ptr->unused2 = 0;
    p_ptr->unused3 = 0;

    /*re-set the thefts counter*/
    recent_failed_thefts = 0;

    /*re-set the altered inventory counter*/
    allow_altered_inventory = 0;

    // reset some unique flags
    p_ptr->unique_forge_made = FALSE;
    p_ptr->unique_forge_seen = FALSE;
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        p_ptr->greater_vaults[i] = 0;
    }
}

/*
 * Init players with some belongings
 *
 * Having an item identifies it and makes the player "aware" of its purpose.
 */
/* ------------------------------------------------------------------
 * Public wrapper: let other modules reset the player safely.
 * Keeps player_wipe() private while exporting a thin façade.
 * ---------------------------------------------------------------- */
void character_wipe(void)
{
    player_wipe();
}

/* ------------------------------------------------------------------
 * Hand out one start-item list (race or house).
 * ------------------------------------------------------------------ */
static void give_start_items(const start_item *list)
{
    int i, slot, inven_slot;
    object_type object_type_body, *i_ptr, *o_ptr;

    for (i = 0; i < MAX_START_ITEMS && list[i].tval; i++)
    {
        const start_item *e_ptr = &list[i];

        /* Look up kind */
        s16b k_idx = lookup_kind(e_ptr->tval, e_ptr->sval);
        if (!k_idx) continue;

        object_kind *k_ptr = &k_info[k_idx];
        i_ptr = &object_type_body;

        /* Prepare object */
        object_prep(i_ptr, k_idx);
        i_ptr->number = (byte)rand_range(e_ptr->min, e_ptr->max);
        i_ptr->weight = k_ptr->weight;

        /* Where would this be wielded? */
        slot = wield_slot(i_ptr);

        /* Light sources start with fuel */
        if (slot == INVEN_LITE) i_ptr->timeout = 2000;

        object_known(i_ptr);

        /* Carry it */
        inven_slot = inven_carry(i_ptr, TRUE);

        /* Auto-wield if slot empty */
        if (slot >= INVEN_WIELD && inventory[slot].tval == 0)
        {
            o_ptr = &inventory[slot];
            object_copy(o_ptr, i_ptr);

            if (o_ptr->tval != TV_ARROW) o_ptr->number = 1;

            inven_item_increase(inven_slot, -(o_ptr->number));
            inven_item_optimize(inven_slot);
            p_ptr->equip_cnt++;
        }

        object_wipe(i_ptr); /* avoid dupes */
    }
}

static void player_outfit(void)
{
    /* ---------- locals ---------- */
    time_t      c;
    struct tm  *tp;

    /* skip all starting‐gear on load */
    if (character_loaded) return;

    /* ---------- escape-curse check ---------- */
    if (curse_flag_count(CUR_NOSTART)) return;

    /* ---------- pointers into info arrays ---------- */
    player_race  *rp_ptr = &p_info[p_ptr->prace];
    player_house *hp_ptr = &c_info[p_ptr->phouse];

    /* ---------- hand out gear ---------- */
    give_start_items(rp_ptr->start_items);   /* race first  */
    give_start_items(hp_ptr->start_items);   /* house next  */

    /* ---------- Christmas present (unchanged) ---------- */
    c  = time((time_t*)0);
    tp = localtime(&c);
    if ((tp->tm_mon == 11) && (tp->tm_mday >= 25))
    {
        object_type object_type_body, *i_ptr = &object_type_body;

        s16b k_idx = lookup_kind(TV_CHEST, SV_CHEST_PRESENT);
        object_prep(i_ptr, k_idx);
        i_ptr->number = 1;
        i_ptr->pval   = -20;

        (void)inven_carry(i_ptr, TRUE);
    }

    /* ---------- bookkeeping ---------- */
    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
}

/*
 * Clear the previous question
 */
static void clear_question(void)
{
    int i;

    for (i = QUESTION_ROW; i < TABLE_ROW; i++)
    {
        /* Clear line, position cursor */
        Term_erase(0, i, 255);
    }
}

/*
 * Generic "get choice from menu" function
 */
static int get_player_choice(birth_menu* choices, int num, int def, int col,
    int wid, void (*hook)(birth_menu))
{
    int top = 0, next;
    int i, dir;
    char c;
    char buf[300];
    bool done = FALSE;
    int hgt;
    byte attr;
    int cur = (def) ? def : 0;

    /* Autoselect if able */
    // if (num == 1) done = TRUE;

    /* Clear */
    for (i = TABLE_ROW; i < DESCRIPTION_ROW + 4; i++)
    {
        /* Clear */
        Term_erase(col, i, Term->wid - wid);
    }

    /* Choose */
    while (TRUE)
    {
        hgt = Term->hgt - TABLE_ROW - 1;

        /* Redraw the list */
        for (i = 0; ((i + top < num) && (i <= hgt)); i++)
        {
            if (i + top < 26)
            {
                strnfmt(buf, sizeof(buf), "%c) %s", I2A(i + top),
                    choices[i + top].name);
            }
            else
            {
                /* ToDo: Fix the ASCII dependency */
                strnfmt(buf, sizeof(buf), "%c) %s", 'A' + (i + top - 26),
                    choices[i + top].name);
            }

            /* Clear */
            Term_erase(col, i + TABLE_ROW, wid);

            /* Display */
            if (i == (cur - top))
            {
                /* Highlight the current selection */
                if (choices[i + top].ghost)
                    attr = TERM_BLUE;
                else
                    attr = TERM_L_BLUE;
            }
            else
            {
                if (choices[i + top].ghost)
                    attr = TERM_SLATE;
                else
                    attr = TERM_WHITE;
            }

            Term_putstr(col, i + TABLE_ROW, wid, attr, buf);
        }

        Term_erase(0, DESCRIPTION_ROW + 0, 255);
        Term_erase(0, DESCRIPTION_ROW + 1, 255);
        Term_erase(0, DESCRIPTION_ROW + 2, 255);
        Term_erase(0, DESCRIPTION_ROW + 3, 255);
        Term_erase(0, DESCRIPTION_ROW + 4, 255);
        Term_erase(0, DESCRIPTION_ROW + 5, 255);
        Term_erase(0, DESCRIPTION_ROW + 6, 255);
        Term_erase(0, DESCRIPTION_ROW + 7, 255);

        if (choices[cur + top].text != NULL)
        {
            /* Indent output by 2 character, and wrap at column 70 */
            text_out_wrap = 70;
            text_out_indent = 2;

            /* History */
            Term_gotoxy(text_out_indent, DESCRIPTION_ROW);
            text_out_to_screen(TERM_L_WHITE, choices[cur + top].text);

            /* Reset text_out() vars */
            text_out_wrap = 0;
            text_out_indent = 0;
        }

        if (done)
            return (cur);

        /* Display auxiliary information if any is available. */
        if (hook)
            hook(choices[cur]);

        /* Move the cursor */
        put_str("", TABLE_ROW + cur - top, col);

        hide_cursor = TRUE;
        c = inkey();
        hide_cursor = FALSE;

        /* Exit the game */
        if ((c == 'Q') || (c == 'q'))
            quit(NULL);

        /* Hack - go back */
        if ((c == ESCAPE) || (c == '4'))
            return (INVALID_CHOICE);

        /* Make a choice */
        if ((c == '\n') || (c == '\r') || (c == '6')) {
            if (choices[cur].ghost)
                bell("Your race cannot choose that house.");
            else
                return (cur);
        }
        //Show scores 
        if (c == 's')
            {
                show_scores();
            }

        /* Random choice */
        if (c == '*')
        {
            /* Ensure legal choice */
            do
            {
                cur = rand_int(num);
            } while (choices[cur].ghost);

            /* Done */
            done = TRUE;
        }

        /* Alphabetic choice */
        else if (isalpha(c))
        {
            /* Options */
            if ((c == 'O') || (c == 'o'))
            {
                do_cmd_options();
            }

            else
            {
                int choice;

                if (islower(c))
                    choice = A2I(c);
                else
                    choice = c - 'A' + 26;

                /* Validate input */
                if ((choice > -1) && (choice < num) && !(choices[choice].ghost))
                {
                    cur = choice;

                    /* Done */
                    done = TRUE;
                }
                else if (choices[choice].ghost)
                {
                    bell("Your race cannot choose that house.");
                }
                else
                {
                    bell("Illegal response to question!");
                }
            }
        }

        /* Move */
        else if (isdigit(c))
        {
            /* Get a direction from the key */
            dir = target_dir(c);

            /* Going up? */
            if (dir == 8)
            {
                next = -1;
                for (i = 0; i < cur; i++)
                {
                    // if (!(choices[i].ghost))
                    // {
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
                /* if (cur != 0) cur--; */

                /* Scroll up */
                if ((top > 0) && ((cur - top) < 4))
                    top--;
            }

            /* Going down? */
            if (dir == 2)
            {
                next = -1;
                for (i = num - 1; i > cur; i--)
                {
                    // if (!(choices[i].ghost))
                    // 
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
                /* if (cur != (num - 1)) cur++; */

                /* Scroll down */
                if ((top + hgt < (num - 1)) && ((top + hgt - cur) < 4))
                    top++;
            }
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");

        /* If choice is off screen, move it to the top */
        if ((cur < top) || (cur > top + hgt))
            top = cur;
    }

    return (INVALID_CHOICE);
}

/* OR of every flag carried by the active metarun curses */
u32b curse_flag_mask(void)
{
    u32b m = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_GET(id)) m |= cu_info[id].flags;
    }
    return m;
}

/*
 * Count how many active curses carry the given flag bit
 * (scans both cu_info[].flags and cu_info[].flags_u).
 */
int curse_flag_count(u32b rhf_flag)
{
    int count = 0;

    /* Iterate over every defined curse */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        /* Only consider curses the player actually has */
        if (curse_count(i) > 0)
        {
            /* If either flag field contains our target bit, tally it */
            if ((cu_info[i].flags   & rhf_flag) ||
                (cu_info[i].flags_u & rhf_flag))
            {
                count++;
            }
        }
    }

    return count;
}


/*
 * Show race/house flags in priority order.
 * Masteries first, then single-side affinities, then penalties,
 * and finally any “headline / unique” flags.
 */
static void print_rh_flags(int race, int house, int col, int row)
{
    int flags_left  = 0;
    int flags_right = 0;

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery  = TERM_L_GREEN;
    byte attr_penalty  = TERM_RED;
    byte attr_gr_penalty  = TERM_L_RED;

    const int col_pen = col + 20;

    // Updated struct to support side
    typedef struct {
        const char *txt;
        byte col;
        int side;  // 0 = left, 1 = right
    } skill_line;

    skill_line mastery_buf [16], affinity_buf[16], penalty_buf[16], unique_buf[8];
    int mastery_n = 0, affinity_n = 0, penalty_n = 0, unique_n = 0;

/*
 * Show one skill line according to the new ±2 ↔ mastery / grand-penalty rule.
 *
 *   +1 for every …_AFFINITY bit, −1 for every …_PENALTY bit.
 *
 *        score   meaning            colour / buffer
 *        =====   ===============    =========================
 *          +2    mastery            mastery_buf  / attr_mastery
 *          +1    affinity           affinity_buf / attr_affinity
 *           0    (omit line)        —
 *          −1    penalty            penalty_buf  / attr_penalty
 *          −2    grand penalty      penalty_buf  / attr_penalty
 */
/* Show one skill line according to the new ±2 rule,
 * now counting curse affinities / penalties too.
 */
#define HANDLE_SKILL_EX(label, AFF_FLAG, PEN_FLAG)                          \
    do {                                                                    \
        int score = 0;                                                      \
                                                                            \
        /* race + house bits */                                             \
        if (p_info[race].flags  & (AFF_FLAG)) score++;                      \
        if (c_info[house].flags & (AFF_FLAG)) score++;                      \
        if (p_info[race].flags  & (PEN_FLAG)) score--;                      \
        if (c_info[house].flags & (PEN_FLAG)) score--;                      \
                                                                            \
        /* every copy of the same curse flag */                             \
        score += curse_flag_count(AFF_FLAG);                                \
        score -= curse_flag_count(PEN_FLAG);                                \
                                                                            \
        /* clamp so the UI never shows >mastery or >grand-penalty */        \
        if (score >  2) score =  2;                                         \
        if (score < -2) score = -2;                                         \
                                                                            \
        if (score ==  2) {                                                  \
            mastery_buf[mastery_n].txt = label " mastery";                  \
            mastery_buf[mastery_n++].col = attr_mastery;                    \
        } else if (score == 1) {                                            \
            affinity_buf[affinity_n].txt = label " affinity";               \
            affinity_buf[affinity_n++].col = attr_affinity;                 \
        } else if (score == -1) {                                           \
            penalty_buf[penalty_n].txt = label " penalty";                  \
            penalty_buf[penalty_n++].col = attr_penalty;                    \
        } else if (score == -2) {                                           \
            penalty_buf[penalty_n].txt = label " grand penalty";            \
            penalty_buf[penalty_n++].col = attr_gr_penalty;                    \
        }                                                                   \
    } while (0)


// New: (label, FLAG, COLOR, SIDE) where SIDE = 0 (left) or 1 (right)
#define HANDLE_UNIQUE(label, FLAG, COLOR, SIDE)                             \
    do {                                                                    \
        int race_has  = p_info[race].flags  & (FLAG);                       \
        int house_has = c_info[house].flags & (FLAG);                       \
        if (race_has || house_has) {                                        \
            unique_buf[unique_n].txt  = label;                              \
            unique_buf[unique_n].col  = (COLOR);                            \
            unique_buf[unique_n++].side = (SIDE);                           \
        }                                                                   \
    } while (0)

// New: (label, FLAG, COLOR, SIDE) where SIDE = 0 (left) or 1 (right)
#define HANDLE_UNIQUE_U(label, FLAG, COLOR, SIDE)                             \
    do {                                                                    \
        int house_has = c_info[house].flags_u & (FLAG);                       \
        if (house_has) {                                                    \
            unique_buf[unique_n].txt  = label;                              \
            unique_buf[unique_n].col  = (COLOR);                            \
            unique_buf[unique_n++].side = (SIDE);                           \
        }                                                                   \
    } while (0)

    // Skills
    HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

    // Unique skills: SIDE = 0 (left), 1 (right)
    HANDLE_UNIQUE_U("Master Artisan",   UNQ_SMT_FEANOR,     TERM_BLUE,     0);
    HANDLE_UNIQUE("Kinslayer",   RHF_KINSLAYER, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Treacherous",   RHF_TREACHERY, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Doom of Mandos",   RHF_CURSE, TERM_UMBER,   1); // right
    HANDLE_UNIQUE_U("Indominable WILL",   UNQ_EARENDIL, TERM_BLUE,   0); // right

    // Left column
    for (int i = 0; i < mastery_n;  ++i)
        Term_putstr(col, row + flags_left++, -1, mastery_buf[i].col, mastery_buf[i].txt);
    for (int i = 0; i < affinity_n; ++i)
        Term_putstr(col, row + flags_left++, -1, affinity_buf[i].col, affinity_buf[i].txt);
    for (int i = 0; i < unique_n; ++i)
        if (unique_buf[i].side == 0)
            Term_putstr(col, row + flags_left++, -1, unique_buf[i].col, unique_buf[i].txt);

    // Right column
    for (int i = 0; i < penalty_n; ++i)
        Term_putstr(col_pen, row + flags_right++, -1, penalty_buf[i].col, penalty_buf[i].txt);
    for (int i = 0; i < unique_n; ++i)
        if (unique_buf[i].side == 1)
            Term_putstr(col_pen, row + flags_right++, -1, unique_buf[i].col, unique_buf[i].txt);

#undef HANDLE_SKILL_EX
#undef HANDLE_UNIQUE

Term_erase(col +7, row - 5, 30);

if (house) {
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_POWER)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Power");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_FINESSE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Finesse");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_KNOCK_BACK)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Knock Back");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_POLEARMS)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Polearm Mastery");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_CHARGE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Charge");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_FOLLOW_THROUGH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Follow-Through");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_IMPALE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Impale");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_CONTROL)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Subtlety");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_WHIRLWIND_ATTACK)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Whirlwind Attack");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_ZONE_OF_CONTROL)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Zone of Control");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_SMITE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Smite");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_TWO_WEAPON)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Two Weapon Fighting");
    if (c_info[house].a_adj[0] == S_MEL && c_info[house].a_adj[1] == MEL_RAPID_ATTACK)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Rapid Attack");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_ROUT)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Rout");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_FLETCHERY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Fletchery");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_POINT_BLANK)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Point Blank Archery");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_PUNCTURE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Puncture");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_AMBUSH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Ambush");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_VERSATILITY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Versatility");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_CRIPPLING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Crippling Shot");
    if (c_info[house].a_adj[0] == S_ARC && c_info[house].a_adj[1] == ARC_DEADLY_HAIL)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Deadly Hail");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_DODGING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Dodging");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_BLOCKING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Blocking");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_PARRY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Parry");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_CROWD_FIGHTING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Crowd Fighting");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_LEAPING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Leaping");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_SPRINTING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Sprinting");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_FLANKING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Flanking");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_HEAVY_ARMOUR)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Heavy Armour Use");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_RIPOSTE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Riposte");
    if (c_info[house].a_adj[0] == S_EVN && c_info[house].a_adj[1] == EVN_CONTROLLED_RETREAT)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Controlled Retreat");
    if (c_info[house].a_adj[0] == S_STL && c_info[house].a_adj[1] == STL_DISGUISE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Disguise");
    if (c_info[house].a_adj[0] == S_STL && c_info[house].a_adj[1] == STL_ASSASSINATION)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Assassination");
    if (c_info[house].a_adj[0] == S_STL && c_info[house].a_adj[1] == STL_CRUEL_BLOW)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Cruel Blow");
    if (c_info[house].a_adj[0] == S_STL && c_info[house].a_adj[1] == STL_EXCHANGE_PLACES)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Exchange Places");
    if (c_info[house].a_adj[0] == S_STL && c_info[house].a_adj[1] == STL_OPPORTUNIST)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Opportunist");
    if (c_info[house].a_adj[0] == S_STL && c_info[house].a_adj[1] == STL_VANISH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Vanish");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_QUICK_STUDY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Quick Study");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_FOCUSED_ATTACK)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Focused attack");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_KEEN_SENSES)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Keen Senses");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_CONCENTRATION)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Concentration");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_ALCHEMY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Alchemy");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_BANE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Bane");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_OUTWIT)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Outwit");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_LISTEN)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Listen");
    if (c_info[house].a_adj[0] == S_PER && c_info[house].a_adj[1] == PER_MASTER_HUNTER)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Master Hunter");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_CURSE_BREAKING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Curse Breaking");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_CHANNELING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Channeling");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_STRENGTH_IN_ADVERSITY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Strength in Adversity");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_FORMIDABLE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Formidable");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_INNER_LIGHT)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Inner Light");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_INDOMITABLE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Indomitable");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_OATH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Oath");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_POISON_RESISTANCE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Poison Resistance");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_VENGEANCE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Vengeance");
    if (c_info[house].a_adj[0] == S_WIL && c_info[house].a_adj[1] == WIL_MAJESTY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Majesty");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_WEAPONSMITH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Weaponsmith");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_ARMOURSMITH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Armoursmith");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_JEWELLER)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Jeweller");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_ENCHANTMENT)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Enchantment");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_EXPERTISE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Expertise");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_ARTEFACT)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Artifice");
    if (c_info[house].a_adj[0] == S_SMT && c_info[house].a_adj[1] == SMT_MASTERPIECE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Masterpiece");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_ELBERETH)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Elbereth");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_CHALLENGE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Challenge");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_DELVINGS)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Delvings");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_FREEDOM)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Freedom");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_SILENCE)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Silence");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_STAUNCHING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Staunching");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_THRESHOLDS)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Thresholds");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_TREES)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of the Trees");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_SLAYING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Slaying");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_STAYING)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Staying");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_LORIEN)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Lorien");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_MASTERY)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Song of Mastery");
    if (c_info[house].a_adj[0] == S_SNG && c_info[house].a_adj[1] == SNG_WOVEN_THEMES)
        Term_putstr(col + 7, row -5, -1, TERM_BLUE, "Woven Themes");
    }
}

/*
 * Display additional information about each race during the selection.
 */
static void race_aux_hook(birth_menu r_str)
{
    int race, i, adj;
    char s[50];
    byte attr;

    /* Extract the proper race index from the string. */
    for (race = 0; race < z_info->p_max; race++)
    {
        if (!strcmp(r_str.name, p_name + p_info[race].name))
            break;
    }

    if (race == z_info->p_max)
        return;

    /* Display the stats */
    for (i = 0; i < A_MAX; i++)
    {
        /*dump the stats*/
        strnfmt(s, sizeof(s), "%s", stat_names[i]);
        Term_putstr(RACE_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

        adj = p_info[race].r_adj[i];
        strnfmt(s, sizeof(s), "%+d", adj);

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 0)
            attr = TERM_L_DARK;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else
            attr = TERM_L_BLUE;

        Term_putstr(RACE_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
    }

    /* Display the race flags */

    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE,
        "                    ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE,
        "                    ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE,
        "                    ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE,
        "                    ");

    // print_rh_flags(race, 0, RACE_AUX_COL, TABLE_ROW + A_MAX + 1);
}

/*
 * Player race
 */
static bool get_player_race(void)
{
    int i;
    birth_menu* races;
    int race;

    C_MAKE(races, z_info->p_max, birth_menu);

    /* Extra info */
    // Term_putstr(QUESTION_COL, QUESTION_ROW, -1, TERM_YELLOW,
    //	"Your race affects your ability scores and other factors.");

    /* Tabulate races */
    for (i = 0; i < z_info->p_max; i++)
    {
        races[i].name = p_name + p_info[i].name;
        races[i].ghost = FALSE;
        races[i].text = p_text + p_info[i].text;
    }

    race = get_player_choice(
        races, z_info->p_max, p_ptr->prace, RACE_COL, 15, race_aux_hook);

    /* No selection? */
    if (race == INVALID_CHOICE)
    {
        return (FALSE);
    }

    // if different race to last time, then wipe the history, age, height,
    // weight
    if (race != p_ptr->prace)
    {
        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }
    p_ptr->prace = race;

    /* Save the race pointer */
    rp_ptr = &p_info[p_ptr->prace];

    FREE(races);

    /* Success */
    return (TRUE);
}

/*
 * Display additional information about each house during the selection.
 */

static void house_aux_hook(birth_menu c_str)
{
    int house_idx, i, adj;
    char s[128];
    byte attr;



    /* Extract the proper house index from the string. */
    for (house_idx = 0; house_idx < z_info->c_max; house_idx++)
    {
        if (!strcmp(c_str.name, c_name + c_info[house_idx].name))
            break;
    }

    if (house_idx == z_info->c_max)
        return;

    /* Display relevant details. */
    for (i = 0; i < A_MAX; i++)
    {
        /*dump potential total stats*/
        strnfmt(s, sizeof(s), "%s", stat_names[i]);
        Term_putstr(TOTAL_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

        adj = c_info[house_idx].h_adj[i] + rp_ptr->r_adj[i] + curses_stat_adj(i);
        strnfmt(s, sizeof(s), "%+d", adj);

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 0)
            attr = TERM_L_DARK;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else
            attr = TERM_L_BLUE;

        Term_putstr(TOTAL_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
    }

    /* Display the race flags */

    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 5, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 6, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 7, -1, TERM_WHITE,
        "                                         ");
    // Check dead   
    if (c_str.ghost) Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 7, -1, TERM_RED,
        "Dead");
    else Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX +7, -1, TERM_L_BLUE,
        "Alive");
    
    // /* ---- red line of active curses ----------------------------------- */
    // {
    //     char buf[80] = "";
    //     bool first = TRUE;
    //     for (int bit = 0; bit < z_info->cu_max; bit++) {
    //         int cnt = curse_count(bit);
    //         if (!cnt) continue;

    //         if (!first) my_strcat(buf, ", ", sizeof(buf));
    //         my_strcat(buf, cu_name + cu_info[bit].name, sizeof(buf));
    //         if (cnt > 1) {
    //             char t[6];  /* room for “ ×15” */
    //             strnfmt(t, sizeof(t), " ×%d", cnt);
    //             my_strcat(buf, t, sizeof(buf));
    //         }
    //         first = FALSE;
    //     }

    //     if (first) my_strcpy(buf, "<no curses>", sizeof(buf));

    //     /* one row above the flag block */
    //     Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX, -1, TERM_L_RED, buf);
    // }
        
    print_rh_flags(
        p_ptr->prace, house_idx, TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1);
    
}
// Check house flags
static int is_set(int bit) {
    if (bit < 0 || bit >= FLAG_COUNT) return 0;  // Out of bounds
    int word = bit / 32;
    int shift = bit % 32;
    return (rp_ptr->choice[word] & (1U << shift)) != 0;
}
/*
 * Player house
 */
static bool get_player_house(void)
{
    int i;
    int house = 0;
    int house_choice;
    int old_house_choice = 0;
    char buf[1024];

    birth_menu* houses;

    int housless=1;
    for (int i = 0; i < FLAG_WORDS; ++i) {
        if (rp_ptr->choice[i] != 0) {
            housless=0;
            break;  // At least one flag is set
        }
    }
    // select 'houseless' automatically if there are no available houses
    if (housless)
    {
        p_ptr->phouse = 0;
        hp_ptr = &c_info[p_ptr->phouse];
        return (TRUE);
    }

    C_MAKE(houses, z_info->c_max, birth_menu);

    /* Extra info */
    // Term_putstr(QUESTION_COL, QUESTION_ROW, -1, TERM_YELLOW,
    //	"Your house modifies your race bonuses.");

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "scores.raw");

    /* Grab permissions */
    safe_setuid_grab();

    /* Open the high score file, for reading/writing */
    highscore_fd = fd_open(buf, O_RDWR);

    /* Drop permissions */
    safe_setuid_drop();

    /* Tabulate houses */

    for (i = 0; i < z_info->c_max; i++)
    {

        /* Analyze */
        if (is_set(i))
        {
            if (highscore_dead(c_name + c_info[i].name)) houses[house].ghost = TRUE;
            else houses[house].ghost = FALSE;
            houses[house].name = c_name + c_info[i].name;
            houses[house].text = c_text + c_info[i].text;
            if (p_ptr->phouse == i)
                old_house_choice = house;
            house++;
        }
    }

    /* Shut the high score file */
    fd_close(highscore_fd);

    /* Forget the high score fd */
    highscore_fd = -1;

    house_choice = get_player_choice(
        houses, house, old_house_choice, CLASS_COL, 22, house_aux_hook);

    /* No selection? */
    if (house_choice == INVALID_CHOICE)
    {
        return (FALSE);
    }

    /* Get house from choice number */
    house = 0;
    for (i = 0; i < z_info->c_max; i++)
    {
        if (is_set(i))
        {
            if (house_choice == house)
            {
                // if different house to last time, then wipe the history, age,
                // height, weight
                if (i != p_ptr->phouse)
                {
                    int j;

                    p_ptr->history[0] = '\0';
                    p_ptr->age = 0;
                    p_ptr->ht = 0;
                    p_ptr->wt = 0;
                    for (j = 0; j < A_MAX; j++)
                    {
                        p_ptr->stat_base[j] = 0;
                    }
                }
                p_ptr->phouse = i;
            }
            house++;
        }
    }

    /* Set house */
    hp_ptr = &c_info[p_ptr->phouse];

    FREE(houses);

    return (TRUE);
}

/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a race, and house, and
 * modify options (including the birth options).
 */
static bool player_birth_aux_1(void)
{
    int i, j;

    int phase = 1;

    /*** Instructions ***/

    /* Clear screen */
    Term_clear();

    /* Display some helpful information */
    Term_putstr(
        QUESTION_COL, HEADER_ROW, -1, TERM_L_BLUE, "Character Selection:");

    Term_putstr(QUESTION_COL, INSTRUCT_ROW, -1, TERM_SLATE,
        "Arrow keys navigate the menu    Enter select the current menu item");
    Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
        "         * random menu item       ESC restart the character");
    Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
        "         = game options             s scores q quit");

    /* Hack - highlight the key names */
    Term_putstr(QUESTION_COL + 0, INSTRUCT_ROW, -1, TERM_L_WHITE, "Arrow keys");
    Term_putstr(QUESTION_COL + 32, INSTRUCT_ROW, -1, TERM_L_WHITE, "Enter");
    Term_putstr(QUESTION_COL + 9, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "*");
    Term_putstr(QUESTION_COL + 34, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "ESC");
    Term_putstr(QUESTION_COL + 9, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "O");
    Term_putstr(QUESTION_COL + 36, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "s");
    Term_putstr(QUESTION_COL + 45, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "q");

    while (phase <= 2)
    {
        clear_question();

        if (phase == 1)
        {
            /* Choose the player's race */
            if (!get_player_race())
            {
                continue;
            }

            /* Clean up */
            clear_question();

            phase++;
        }

        if (phase == 2)
        {
            /* Choose the player's house */
            if (!get_player_house())
            {
                phase--;
                continue;
            }

            /* Clean up */
            clear_question();

            phase++;
        }
    }

    //Check if savefile in progress
    path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, c_name + hp_ptr->name);

    /* Clear the base values of the skills */
    for (i = 0; i < A_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities and add bonus ability*/
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = FALSE;
            p_ptr->active_ability[i][j] = FALSE;
        }
    }

    p_ptr->innate_ability[hp_ptr->a_adj[0]][hp_ptr->a_adj[1]] = TRUE;
    p_ptr->active_ability[hp_ptr->a_adj[0]][hp_ptr->a_adj[1]] = TRUE;

    /* Set adult options from birth options */
    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
    {
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];
    }

    /* Reset score options from cheat options */
    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
    {
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];
    }

    // Set a default value for hitpoint warning / delay factor unless this is an
    // old game file
    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->hitpoint_warn = 3;
        op_ptr->delay_factor = 5;
    }

    /* reset squelch bits */

    for (i = 0; i < z_info->k_max; i++)
    {
        k_info[i].squelch = SQUELCH_NEVER;
    }
    /*Clear the squelch bytes*/
    for (i = 0; i < SQUELCH_BYTES; i++)
    {
        squelch_level[i] = SQUELCH_NONE;
    }
    /* Clear the special item squelching flags */
    for (i = 0; i < z_info->e_max; i++)
    {
        e_info[i].aware = FALSE;
        e_info[i].squelch = FALSE;
    }

    /* Clear */
    Term_clear();

    /* Done */
    return (TRUE);

}

/*
 * Initial stat costs.
 */
static const int birth_stat_costs[11]
    = { -4, -3, -2, -1, 0, 1, 3, 6, 10, 15, 21 };

#define MAX_COST 13

/*
 * Helper function for 'player_birth()'.
 */
static bool player_birth_aux_2(void)
{
    int i;

    int row = 2;
    int col = 42;

    int stat = 0;

    int stats[A_MAX];

    int cost;

    char ch;

    char buf[80];

    /* Initialize stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Initial stats */
        stats[i] = p_ptr->stat_base[i];
    }

    /* Determine experience and things */
    get_extra();

    /* Interact */
    while (1)
    {
        /* Reset cost */
        cost = 0;

        /* Process stats */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain a "bonus" for "race" */
            int bonus = rp_ptr->r_adj[i] + hp_ptr->h_adj[i] + curses_stat_adj(i);

            /* Apply the racial bonuses */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;

            /* Total cost */
            cost += birth_stat_costs[stats[i] + 4];
        }

        /* Restrict cost */
        if (cost > MAX_COST)
        {
            /* Warning */
            bell("Excessive stats!");

            /* Reduce stat */
            stats[stat]--;

            /* Recompute costs */
            continue;
        }

        p_ptr->new_exp = p_ptr->exp = get_start_xp();

        /* Calculate the bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);

        /* Update stuff */
        update_stuff();

        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;

        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        /* Display the player */
        display_player(0);

        /* Display the costs header */
        c_put_str(TERM_WHITE, "Points Left:", 0, col + 21);
        strnfmt(buf, sizeof(buf), "%2d", MAX_COST - cost);
        c_put_str(TERM_L_GREEN, buf, 0, col + 34);

        /* Display the costs */
        for (i = 0; i < A_MAX; i++)
        {
            if (i == stat)
            {
                byte attr = TERM_L_BLUE;
#ifndef MONOCHROME_MODE
                strnfmt(
                    buf, sizeof(buf), "%4d", birth_stat_costs[stats[i] + 4]);
                c_put_str(attr, buf, row + i, col + 32);
#else
                strnfmt(
                    buf, sizeof(buf), "%4d*", birth_stat_costs[stats[i] + 4]);
                c_put_str(attr, buf, row + i, col + 32);
                c_put_str(attr, "*", row + i, col - 2);
#endif
            }
            else
            {
                byte attr = TERM_L_WHITE;
                strnfmt(
                    buf, sizeof(buf), "%4d", birth_stat_costs[stats[i] + 4]);
                c_put_str(attr, buf, row + i, col + 32);
            }
            byte attr = (i == stat) ? TERM_L_BLUE : TERM_L_WHITE;

            /* Display cost */
            strnfmt(buf, sizeof(buf), "%4d", birth_stat_costs[stats[i] + 4]);
            c_put_str(attr, buf, row + i, col + 32);
        }

        /* Prompt */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            "Arrow keys allocate points to stats");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
            "     Enter accept current values");

        /* Hack - highlight the key names */
        Term_putstr(
            QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Arrow keys");
        Term_putstr(
            QUESTION_COL + 5, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Enter");

        /* Get key */
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        /* Quit */
        if ((ch == 'Q') || (ch == 'q'))
            quit(NULL);

        /* Start over */
        if (ch == ESCAPE)
            return (FALSE);

        /* Done */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Prev stat */
        if (ch == '8')
        {
            stat = (stat + A_MAX - 1) % A_MAX;
        }

        /* Next stat */
        if (ch == '2')
        {
            stat = (stat + 1) % A_MAX;
        }

        /* Decrease stat */
        if ((ch == '4') && (stats[stat] > 0))
        {
            stats[stat]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            stats[stat]++;
        }
    }

    /* Done */
    return (TRUE);
}

/*
 * Skill point costs.
 *
 * The nth skill point costs (100*n) experience points
 */
static int skill_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = (base) * (base + 1) / 2;
    return ((total_cost - prev_cost) * 100);
}

/*
 * Increase your skills by spending experience points
 */
extern bool gain_skills(void)
{
    int i;

    int row = 7;
    int col = 42;

    int skill = 0;

    int old_base[S_MAX];
    int skill_gain[S_MAX];

    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;

    // int old_csp = p_ptr->csp;
    // int old_msp = p_ptr->msp;

    char ch;

    char buf[80];

    bool accepted;

    int tab = 0;

    // hack global variable
    skill_gain_in_progress = TRUE;

    /* save the old skills */
    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    /* initialise the skill gains */
    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    /* Interact */
    while (1)
    {
        // reset the total cost
        total_cost = 0;

        /* Process skills */
        for (i = 0; i < S_MAX; i++)
        {
            /* Total cost */
            total_cost += skill_cost(old_base[i], skill_gain[i]);
        }

        // set the new experience pool total
        p_ptr->new_exp = old_new_exp - total_cost;

        /* Restrict cost */
        if (p_ptr->new_exp < 0)
        {
            /* Warning */
            bell("Excessive skills!");

            /* Reduce stat */
            skill_gain[skill]--;

            /* Recompute costs */
            continue;
        }

        /* Calculate the bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Set the redraw flag for everything */
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        /* update the skills */
        for (i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        /* Update stuff */
        update_stuff();

        /* Update voice level */
        // if (p_ptr->msp == 0) p_ptr->csp = 0;
        // else                 p_ptr->csp = (old_csp * p_ptr->msp) / old_msp;

        /* Display the player */
        display_player(0);

        /* Display the costs header */
        if (!character_dungeon)
        {
            if (p_ptr->new_exp >= 10000)
                tab = 0;
            else if (p_ptr->new_exp >= 1000)
                tab = 1;
            else if (p_ptr->new_exp >= 100)
                tab = 2;
            else if (p_ptr->new_exp >= 10)
                tab = 3;
            else
                tab = 4;

            strnfmt(buf, sizeof(buf), "%6d", p_ptr->new_exp);
            c_put_str(TERM_L_GREEN, buf, row - 2, col + 30);
            c_put_str(TERM_WHITE, "Points Left:", row - 2, col + 17 + tab);
        }

        /* Display the costs */
        for (i = 0; i < S_MAX; i++)
        {
            if (i == skill)
            {
                byte attr = TERM_L_BLUE;
#ifndef MONOCHROME_MODE
                strnfmt(buf, sizeof(buf), "%6d",
                    skill_cost(old_base[i], skill_gain[i]));
                c_put_str(attr, buf, row + i, col + 30);
#else
                strnfmt(buf, sizeof(buf), "%6d*",
                    skill_cost(old_base[i], skill_gain[i]));
                c_put_str(attr, buf, row + i, col + 30);
                c_put_str(attr, "*", row + i, col - 2);
#endif
            }
            else
            {
                byte attr = TERM_L_WHITE;
                strnfmt(buf, sizeof(buf), "%6d",
                    skill_cost(old_base[i], skill_gain[i]));
                c_put_str(attr, buf, row + i, col + 30);
            }
        }

        /* Special Prompt? */
        if (character_dungeon)
        {
            Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
                "ESC abort skill increases                  ");

            /* Hack - highlight the key names */
            Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1,
                TERM_L_WHITE, "ESC");
        }

        /* Prompt */
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            "Arrow keys allocate points to skills");
        Term_putstr(QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE,
            "     Enter accept current values");

        /* Hack - highlight the key names */
        Term_putstr(
            QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Arrow keys");
        Term_putstr(
            QUESTION_COL + 5, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Enter");

        /* Get key */
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        /* Quit (only if haven't begun game yet) */
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0))
            quit(NULL);

        /* Done */
        if ((ch == '\r') || (ch == '\n'))
        {
            accepted = TRUE;
            break;
        }

        /* Abort */
        if (ch == ESCAPE)
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++)
                p_ptr->skill_base[i] = old_base[i];
            // p_ptr->csp = old_csp;
            accepted = FALSE;
            break;
        }

        /* Prev skill */
        if (ch == '8')
        {
            skill = (skill + S_MAX - 1) % S_MAX;
        }

        /* Next skill */
        if (ch == '2')
        {
            skill = (skill + 1) % S_MAX;
        }

        /* Decrease skill */
        if ((ch == '4') && (skill_gain[skill] > 0))
        {
            skill_gain[skill]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            skill_gain[skill]++;
        }
    }

    // reset hack global variable
    skill_gain_in_progress = FALSE;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff();

    /* Done */
    return (accepted);
}

#define BASE_COLUMN 7
#define STAT_TITLE_ROW 14
#define BASE_STAT_ROW 16

/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static bool player_birth_aux(void)
{
    /* Ask questions */
    if (!player_birth_aux_1())
        return (FALSE);
    if (!load_player())  {
    /* Point-based stats */
    if (!player_birth_aux_2())
        return (FALSE);

    /* Point-based skills */
    if (!gain_skills())
        return (FALSE);

    /* Roll for history */
    if (!get_history())
        return (FALSE);

    /* Roll for age/height/weight */
    if (!get_ahw())
        return (FALSE);

    /* Get a name, prepare savefile */
    if (!get_name())
        return (FALSE);

    // Reset the number of artefacts
    p_ptr->artefacts = 0;
}
    /* Accept */
    return (TRUE);
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
void player_birth()
{
    int i;

    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    /* Create a new character */
    while (1)
    {
        /* Wipe the player */
        player_wipe();

        /* Roll up a new character */
        if (player_birth_aux())
            break;
    }

    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    /* Get date */
    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    sprintf(month, "%.2s", raw_date + 5);
    atomonth(atoi(month), month);

    if (*(raw_date + 7) == '0')
        sprintf(
            clean_date, "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
    else
        sprintf(
            clean_date, "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);

    /* Add in "character start" information */
    my_strcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    my_strcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    my_strcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    /* Note player birth in the message recall */
    message_add(" ", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add("====================", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add(" ", MSG_GENERIC);

    /* Hack -- outfit the player */
    player_outfit();
}
