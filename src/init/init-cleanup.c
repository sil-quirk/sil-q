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
void cleanup_angband(void)
{
    /* Free the macros */
    macro_free();

    /* Free the macro triggers */
    macro_trigger_free();

    /* Free the allocation tables */
    mem_free_null(alloc_ego_table);
    mem_free_null(alloc_race_table);
    mem_free_null(alloc_kind_table);

    /* Free the player inventory */
    mem_free_null(inventory);

    /*Clean the Autoinscribe*/
    autoinscribe_clean();

    /* Free the lore, monster, and object lists */
    mem_free_null(l_list);
    mem_free_null(mon_list);
    mem_free_null(o_list);

    /* Flow arrays */
    mem_free_null(cave_when);

    /* Free the cave */
    mem_free_null(cave_o_idx);
    mem_free_null(cave_m_idx);
    mem_free_null(cave_feat);
    mem_free_null(cave_color);
    mem_free_null(cave_natural);
    mem_free_null(cave_rewired);
    mem_free_null(cave_info);
    mem_free_null(cave_light);

    /* Free the "update_view()" array */
    mem_free_null(view_g);

    /* Free the temp arrays */
    mem_free_null(temp_g);
    mem_free_null(temp_y);
    mem_free_null(temp_x);

    /* Free the messages */
    messages_free();

    /* Free the "quarks" */
    quarks_free();

    /*free the randart arrays*/
    free_randart_tables();

    /* Free the info, name, and text arrays */
    free_info(&flavor_head);
    free_info(&g_head);
    free_info(&b_head);
    free_info(&c_head);
    free_info(&p_head);
    free_info(&h_head);
    free_info(&v_head);
    free_info(&r_head);
    free_info(&e_head);
    free_info(&a_head);
    free_info(&k_head);
    free_info(&f_head);
    free_info(&z_head);
    free_info(&n_head);
    free_info(&style_head);
    free_info(&skeleton_note_head);

    /* Note: format() now uses a static buffer, no cleanup needed */

    /* Free the directories */
    str_free(ANGBAND_DIR);
    str_free(ANGBAND_DIR_APEX);
    str_free(ANGBAND_DIR_METARUN);
    str_free(ANGBAND_DIR_BONE);
    str_free(ANGBAND_DIR_DATA);
    str_free(ANGBAND_DIR_EDIT);
    str_free(ANGBAND_DIR_FILE);
    str_free(ANGBAND_DIR_HELP);
    str_free(ANGBAND_DIR_INFO);
    str_free(ANGBAND_DIR_SAVE);
    str_free(ANGBAND_DIR_PREF);
    str_free(ANGBAND_DIR_USER);
    str_free(ANGBAND_DIR_XTRA);
    str_free(ANGBAND_DIR_SCRIPT);
}
