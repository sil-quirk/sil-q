#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "item_set.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "sdl-sound.h"
#include "init2-internal.h"
#include "init-lifecycle.h"
#include <SDL3/SDL_filesystem.h>
#include <stdio.h>
extern void autoinscribe_clean(void)
{
    if (inscriptions)
    {
        mem_free_null(inscriptions);
    }

    inscriptions = 0;
    inscriptionsCount = 0;
}

extern void autoinscribe_init(void)
{
    /* Paranoia */
    autoinscribe_clean();

    inscriptions = mem_alloc_array(AUTOINSCRIPTIONS_MAX, autoinscription);
}

/*
 * Reinitialize some things between games
 *
 * Needed because rerunning the whole of init_angband() causes crashes.
 */
extern void re_init_some_things(void)
{
    int i;

    run_mode_reset();
    ui_reset_transient_state_for_new_session();
    screen_set_startup_supporting_panes_hidden(true);

    /* Defensive: clear any quit-time presentation suppression so a session that
     * returns to the title (rather than exiting) keeps rendering normally. */
    sdl_set_present_suppressed(false);

    /* No live dungeon session exists while the between-games welcome flow is
     * active.  Leaving these set lets SDL's quit-transition guard consume all
     * input on the welcome screen after death or other end-of-run exits. */
    character_generated = false;
    character_dungeon = false;
    character_loaded = false;
    character_loaded_dead = false;

    // wipe the whole player structure
    memset(p_ptr, 0, sizeof(player_type));

    // reset global race/character profile pointers to valid defaults
    // (p_ptr->prace and p_ptr->pcharacter are now 0 after memset)
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];

    // reset dungeon-related static state for new game
    reset_dungeon_state();

    // reset hint/skeleton note state for new game
    reset_hint_skeleton_state();

    // clear some additional things
    savefile[0] = '\0';
    playerturn = 0;
    min_depth_counter = 0;
    op_ptr->full_name[0] = '\0';

    // clear the terms
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term* old = Term;

        /* Dead window */
        if (!angband_term[i])
            continue;

        /* Activate */
        Term_activate(angband_term[i]);

        /* Erase */
        Term_clear();

        /* Restore */
        Term_activate(old);
    }

    // Reset the autoinscriptions
    autoinscribe_clean();
    autoinscribe_init();

    // display the introduction message again
    screen_set_startup_touch_pane_hidden(true);
    sdl_story_font_enable();
    display_introduction();
    sdl_story_font_reset();

    /* Array of grids */
    mem_free_null(view_g);
    view_g = mem_alloc_array(VIEW_MAX, u16b);

    /* Array of grids */
    mem_free_null(temp_g);
    temp_g = mem_alloc_array(TEMP_MAX, u16b);

    /* has_lite patch causes both temp_g and temp_x/y to be used
    in targetting mode: can't use the same memory any more. */
    mem_free_null(temp_y);
    mem_free_null(temp_x);
    temp_y = mem_alloc_array(TEMP_MAX, byte);
    temp_x = mem_alloc_array(TEMP_MAX, byte);

    /*** Prepare dungeon arrays ***/

    /* Padded into array */
    mem_free_null(cave_info);
    cave_info = mem_alloc_array(MAX_DUNGEON_HGT, u16b_256);

    /* Feature array */
    mem_free_null(cave_feat);
    cave_feat = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Color array */
    mem_free_null(cave_color);
    cave_color = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Rewired-trap difficulty array */
    mem_free_null(cave_rewired);
    cave_rewired = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Light array */
    mem_free_null(cave_light);
    cave_light = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Entity arrays */
    mem_free_null(cave_o_idx);
    mem_free_null(cave_m_idx);
    cave_o_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_m_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Flow arrays */
    mem_free_null(cave_when);
    cave_when = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /*** Prepare "vinfo" array ***/

    /* Used by "update_view()" */
    (void)vinfo_init();

    /*** Prepare entity arrays ***/

    /* Objects */
    mem_free_null(o_list);
    o_list = mem_alloc_array(z_info->o_max, object_type);

    /* Monsters */
    mem_free_null(mon_list);
    mon_list = mem_alloc_array(MAX_MONSTERS, monster_type);

    /*** Prepare lore array ***/

    /* Lore */
    mem_free_null(l_list);
    l_list = mem_alloc_array(z_info->r_max, monster_lore);

    /*** Prepare the inventory ***/

    /* Allocate it */
    mem_free_null(inventory);
    inventory = mem_alloc_array(INVEN_TOTAL, object_type);

    /*** Prepare the options ***/

    /* Initialize the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        /* Default value */
        op_ptr->opt[i] = option_norm[i];
    }

    /* Initialize the window flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Assume no flags */
        op_ptr->window_flag[i] = 0L;
    }

    // Set some sensible defaults
    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS | PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE | PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);
    op_ptr->window_flag[WINDOW_SUPPLY] |= (PW_SUPPLY);

    /* Reapply app-wide options after resetting runtime defaults. */
    sdl_config_load_app_options(get_sdl_config_path());

    // re-initialize the objects and flavors
    if (init_k_info())
        quit("Cannot initialize objects");
    if (init_flavor_info())
        quit("Cannot initialize flavors");
    if (init_effect_info())
        quit("Cannot initialize effects");
    if (init_skeleton_note_info())
        quit("Cannot initialize skeleton notes");
    if (init_e_info())
        quit("Cannot initialize special items");
    /* Oath of Light crash guard: ensure oath text/info tables are fresh between games */
    if (init_oath_info())
        quit("Cannot initialize oaths");
}

/*
 * Initialize some other arrays
 */
errr init_other(void)
{
    int i;

    /*** Prepare the various "bizarre" arrays ***/

    /* Initialize the "macro" package */
    (void)macro_init();

    /* Initialize the "quark" package */
    (void)quarks_init();

    /* Initialize autoinscriptions */
    (void)autoinscribe_init();

    /* Initialize the "message" package */
    (void)messages_init();

    /*** Prepare grid arrays ***/

    /* Array of grids */
    view_g = mem_alloc_array(VIEW_MAX, u16b);

    /* Array of grids */
    temp_g = mem_alloc_array(TEMP_MAX, u16b);

    /* has_lite patch causes both temp_g and temp_x/y to be used
    in targetting mode: can't use the same memory any more. */
    temp_y = mem_alloc_array(TEMP_MAX, byte);
    temp_x = mem_alloc_array(TEMP_MAX, byte);

    /*** Prepare dungeon arrays ***/

    /* Padded into array */
    cave_info = mem_alloc_array(MAX_DUNGEON_HGT, u16b_256);

    /* Feature array */
    cave_feat = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Color array */
    cave_color = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Rewired-trap difficulty array */
    cave_rewired = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /* Light array */
    cave_light = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Entity arrays */
    cave_o_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);
    cave_m_idx = mem_alloc_array(MAX_DUNGEON_HGT, s16b_wid);

    /* Flow arrays */
    cave_when = mem_alloc_array(MAX_DUNGEON_HGT, byte_wid);

    /*** Prepare "vinfo" array ***/

    /* Used by "update_view()" */
    (void)vinfo_init();

    /*** Prepare entity arrays ***/

    /* Objects */
    o_list = mem_alloc_array(z_info->o_max, object_type);

    /* Monsters */
    mon_list = mem_alloc_array(MAX_MONSTERS, monster_type);

    /*** Prepare lore array ***/

    /* Lore */
    l_list = mem_alloc_array(z_info->r_max, monster_lore);

    /*** Prepare the inventory ***/

    /* Allocate it */
    inventory = mem_alloc_array(INVEN_TOTAL, object_type);

    /*** Prepare the options ***/

    /* Initialize the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        /* Default value */
        op_ptr->opt[i] = option_norm[i];
    }

    /* Initialize the window flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Assume no flags */
        op_ptr->window_flag[i] = 0L;
    }

    // Set some sensible defaults
    op_ptr->window_flag[WINDOW_INVEN] |= (PW_INVEN);
    op_ptr->window_flag[WINDOW_EQUIP] |= (PW_EQUIP);
    op_ptr->window_flag[WINDOW_COMBAT_ROLLS] |= (PW_COMBAT_ROLLS | PW_MESSAGE);
    op_ptr->window_flag[WINDOW_MONSTER] |= (PW_MONSTER);
    op_ptr->window_flag[WINDOW_PLAYER_0] |= (PW_PLAYER_0);
    op_ptr->window_flag[WINDOW_MESSAGE] |= (PW_MESSAGE | PW_COMBAT_ROLLS);
    op_ptr->window_flag[WINDOW_MONLIST] |= (PW_MONLIST);
    op_ptr->window_flag[WINDOW_SUPPLY] |= (PW_SUPPLY);

    /* Reapply app-wide options after initializing runtime defaults. */
    sdl_config_load_app_options(get_sdl_config_path());

    /*** Pre-allocate space for the "format()" buffer ***/

    /* Hack -- Just call the "format()" function */
    (void)format("%s", MAINTAINER);

    /* Success */
    return (0);
}

/*
 * Initialize some other arrays
 */
errr init_alloc(void)
{
    int i, j;

    object_kind* k_ptr;

    monster_race* r_ptr;

    ego_item_type* e_ptr;

    alloc_entry* table;

    s16b num[MAX_DEPTH];

    s16b aux[MAX_DEPTH];

    /*** Analyze object allocation info ***/

    /* Clear the "aux" array */
    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);

    /* Clear the "num" array */
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    /* Size of "alloc_kind_table" */
    alloc_kind_size = 0;

    /* Scan the objects */
    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /* Count the "legal" entries */
            if (k_ptr->chance[j])
            {
                /* Count the entries */
                alloc_kind_size++;

                /* Group by level */
                num[k_ptr->locale[j]]++;
            }
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Paranoia */
    // if (!num[0]) quit("No surface objects!");

    /*** Initialize object allocation info ***/

    /* Allocate the alloc_kind_table */
    alloc_kind_table = mem_alloc_array(alloc_kind_size, alloc_entry);

    /* Get the table entry */
    table = alloc_kind_table;

    /* Scan the objects */
    for (i = 1; i < z_info->k_max; i++)
    {
        k_ptr = &k_info[i];

        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /* Count the "legal" entries */
            if (k_ptr->chance[j])
            {
                int p, x, y, z;

                /* Extract the base level */
                x = k_ptr->locale[j];

                /* Extract the base probability (direct rarity weight) */
                p = k_ptr->chance[j];

                /* Skip entries preceding our locale */
                y = (x > 0) ? num[x - 1] : 0;

                /* Skip previous entries at this locale */
                z = y + aux[x];

                /* Load the entry */
                table[z].index = i;
                table[z].level = x;
                table[z].prob1 = p;
                table[z].prob2 = p;
                table[z].prob3 = p;

                /* Another entry complete for this locale */
                aux[x]++;
            }
        }
    }

    /*** Analyze monster allocation info ***/

    /* Clear the "aux" array */
    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);

    /* Clear the "num" array */
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    /* Size of "alloc_race_table" */
    alloc_race_size = 0;

    /* Scan the monsters*/
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Get the i'th race */
        r_ptr = &r_info[i];

        /* Legal monsters */
        if (r_ptr->rarity)
        {
            /* Count the entries */
            alloc_race_size++;

            /* Group by level */
            num[r_ptr->level]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /* Paranoia */
    // if (!num[0]) quit("No surface monsters!");

    /*** Initialize monster allocation info ***/

    /* Allocate the alloc_race_table */
    alloc_race_table = mem_alloc_array(alloc_race_size, alloc_entry);

    /* Get the table entry */
    table = alloc_race_table;

    /* Scan the monsters*/
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Get the i'th race */
        r_ptr = &r_info[i];

        /* Count valid pairs */
        if (r_ptr->rarity)
        {
            int p, x, y, z;

            /* Extract the base level */
            x = r_ptr->level;

            /* Extract the base probability (direct rarity weight) */
            p = r_ptr->rarity;

            /* Skip entries preceding our locale */
            y = (x > 0) ? num[x - 1] : 0;

            /* Skip previous entries at this locale */
            z = y + aux[x];

            /* Load the entry */
            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            /* Another entry complete for this locale */
            aux[x]++;
        }
    }

    /*** Analyze ego_item allocation info ***/

    /* Clear the "aux" array */
    memset(aux, 0, sizeof(s16b) * MAX_DEPTH);

    /* Clear the "num" array */
    memset(num, 0, sizeof(s16b) * MAX_DEPTH);

    /* Size of "alloc_ego_table" */
    alloc_ego_size = 0;

    /* Scan the ego items */
    for (i = 1; i < z_info->e_max; i++)
    {
        /* Get the i'th ego item */
        e_ptr = &e_info[i];

        /* Legal items */
        if (e_ptr->rarity)
        {
            /* Count the entries */
            alloc_ego_size++;

            /* Group by level */
            num[e_ptr->level]++;
        }
    }

    /* Collect the level indexes */
    for (i = 1; i < MAX_DEPTH; i++)
    {
        /* Group by level */
        num[i] += num[i - 1];
    }

    /*** Initialize special item allocation info ***/

    /* Allocate the alloc_ego_table */
    alloc_ego_table = mem_alloc_array(alloc_ego_size, alloc_entry);

    /* Get the table entry */
    table = alloc_ego_table;

    /* Scan the special items */
    for (i = 1; i < z_info->e_max; i++)
    {
        /* Get the i'th ego item */
        e_ptr = &e_info[i];

        /* Count valid pairs */
        if (e_ptr->rarity)
        {
            int p, x, y, z;

            /* Extract the base level */
            x = e_ptr->level;

            /* Extract the base probability (direct rarity weight) */
            p = e_ptr->rarity;

            /* Skip entries preceding our locale */
            y = (x > 0) ? num[x - 1] : 0;

            /* Skip previous entries at this locale */
            z = y + aux[x];

            /* Load the entry */
            table[z].index = i;
            table[z].level = x;
            table[z].prob1 = p;
            table[z].prob2 = p;
            table[z].prob3 = p;

            /* Another entry complete for this locale */
            aux[x]++;
        }
    }

    /* Success */
    return (0);
}
