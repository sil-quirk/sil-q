/* File: tables.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

/*
 * Global array for looping through the "keypad directions".
 */
const s16b ddd[9] = { 2, 8, 6, 4, 3, 1, 9, 7, 5 };

/*
 * Global arrays for converting "keypad direction" into "offsets".
 */
const s16b ddx[10] = { 0, -1, 0, 1, -1, 0, 1, -1, 0, 1 };

const s16b ddy[10] = { 0, 1, 1, 1, 0, 0, 0, -1, -1, -1 };

/*
 * Global arrays for optimizing "ddx[ddd[i]]" and "ddy[ddd[i]]".
 */
const s16b ddx_ddd[9] = { 0, 0, 1, -1, 1, -1, 1, -1, 0 };

const s16b ddy_ddd[9] = { 1, -1, 0, 0, 1, 1, -1, -1, 0 };

/*
 * Global array for converting numbers to uppercase hecidecimal digit
 * This array can also be used to convert a number to an octal digit
 */
const char hexsym[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A',
    'B', 'C', 'D', 'E', 'F' };

/*
 * This table allows quick conversion from "speed" to "energy"
 * It used to be complex, but in Sil it is basically linear.
 * It is set up so that there are 10 game turns per player turn at normal speed
 *
 * Note that creatures should never have speed 0 in the first place
 */
const byte extract_energy[8] = {
    /* impossible */ 5,
    /* Slow */ 5,
    /* Normal */ 10,
    /* Fast */ 15,
    /* V Fast */ 20,
    /* X Fast */ 25,
    /* I Fast */ 30,
    /* A Fast */ 35,
};

/*
 * Each chest has a certain set of traps, determined by pval
 * Each chest has a "pval" from 1 to the chest level (max 55)
 * If the "pval" is negative then the trap has been disarmed
 * The "pval" of a chest determines the quality of its treasure
 * Note that disarming a trap on a chest also removes the lock.
 */

const byte chest_traps[25 + 1] = {
    0, /* 0 == empty */
    (CHEST_GAS_CONF), (CHEST_GAS_CONF), (CHEST_GAS_STUN), 0, (CHEST_GAS_STUN),
    (CHEST_GAS_POISON), (CHEST_GAS_POISON), 0, (CHEST_NEEDLE_ENTRANCE),
    (CHEST_NEEDLE_ENTRANCE), (CHEST_NEEDLE_HALLU), 0, (CHEST_NEEDLE_HALLU),
    (CHEST_NEEDLE_LOSE_STR), (CHEST_NEEDLE_LOSE_STR), 0,
    (CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
    (CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
    (CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR), 0,
    (CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR),
    (CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),
    (CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE), 0,
    (CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE), /* 25 == best */
};

/*
 * Hack -- the "basic" color names (see "TERM_xxx")
 */
cptr color_names[16] = {
    "Dark",
    "White",
    "Slate",
    "Orange",
    "Red",
    "Green",
    "Blue",
    "Umber",
    "LightDark",
    "LightSlate",
    "Violet",
    "Yellow",
    "LightRed",
    "LightGreen",
    "LightBlue",
    "LightUmber",
};

/*
 * Abbreviations of healthy stats
 */
cptr stat_names[A_MAX] = { "Str ", "Dex ", "Con ", "Gra " };

/*
 * Abbreviations of damaged stats
 */
cptr stat_names_reduced[A_MAX] = { "Str ", "Dex ", "Con ", "Gra " };

/*
 * Full stat names
 */
cptr stat_names_full[A_MAX]
    = { "strength", "dexterity", "constitution", "grace" };

/*
 * Abbreviations of skills
 */
cptr skill_names[S_MAX]
    = { "Mel", "Arc", "Evn", "Stl", "Per", "Wil", "Cmt", "Sng", "Spc" };

/*
 * Full skill names
 */
cptr skill_names_full[S_MAX] = { "Melee", "Archery", "Evasion", "Stealth",
    "Perception", "Will", "Smithing", "Song", "Special" };

/*
 * Certain "screens" always use the main screen, including News, Birth,
 * Dungeon, Tomb-stone, High-scores, Macros, Colors, Visuals, Options.
 *
 * Later, special flags may allow sub-windows to "steal" stuff from the
 * main window, including File dump (help), File dump (artefacts, uniques),
 * Character screen, Small scale map, Previous Messages, Store screen, etc.
 */
cptr window_flag_desc[32] = { "Display inven/equip", "Display equip/inven",
    "Display player (basic)", "Display player (extra)", "Display combat rolls",
    "Display monster recall", "Display object recall", "Display messages",
    "Display overhead view", "Display monster list", NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL };

/*
 * Options -- textual names (where defined)
 */
cptr option_text[OPT_MAX] = {
    "hjkl_movement", /* OPT_hjkl_movement */
    "quick_messages", /* OPT_quick_messages */
    "angband_keyset", /* OPT_angband_keyset */
    NULL, /* xxx carry_query */
    "stop_singing_on_rest", /* OPT_stop_singing_on_rest */
    NULL, /* xxx always_pickup */
    "forgo_attacking_unwary", /* OPT_forgo_attacking_unwary */
    NULL, /* xxx depth_in_feet */
    NULL, /* xxx stack_force_notes */
    NULL, /* xxx stack_force_costs */
    NULL, /* xxx show_labels */
    NULL, /* xxx show_weights */
    NULL, /* xxx show_choices */
    NULL, /* xxx show_details */
    "beep", /* OPT_system_beep */
    NULL, /* xxx flavors */
    NULL, /* xxx run_ignore_stairs */
    NULL, /* xxx run_ignore_doors */
    NULL, /* xxx run_cut_corners */
    NULL, /* xxx run_use_corners */
    NULL, /* xxx disturb_move */
    NULL, /* xxx disturb_near */
    NULL, /* xxx disturb_panel */
    NULL, /* xxx disturb_state */
    NULL, /* xxx disturb_minor */
    NULL, /* xxx disturb_wakeup */
    NULL, /* xxx alert_hitpoint */
    NULL, /* xxx alert_failure */
    NULL, /* xxx verify_destroy */
    NULL, /* xxx verify_special */
    NULL, /* xxx allow_quantity */
    NULL, /* xxx */
    NULL, /* xxx auto_haggle */
    NULL, /* xxx auto_scum */
    NULL, /* xxx allow_themed_levels */
    NULL, /* xxx testing_carry */
    NULL, /* xxx expand_look */
    NULL, /* xxx expand_list */
    NULL, /* xxx view_perma_grids */
    NULL, /* xxx view_torch_grids */
    NULL, /* xxx dungeon_align */
    NULL, /* xxx dungeon_stair */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx track_follow */
    NULL, /* xxx track_target */
    NULL, /* xxx track_target */
    NULL, /* xxx smart_cheat */
    NULL, /* xxx view_reduce_lite */
    NULL, /* xxx hidden_player */
    NULL, /* xxx avoid_abort */
    NULL, /* xxx avoid_other */
    NULL, /* xxx flush_failure */
    NULL, /* xxx flush_disturb */
    NULL, /* xxx flush_command */
    NULL, /* xxx fresh_before */
    NULL, /* xxx fresh_after */
    NULL, /* xxx fresh_message */
    NULL, /* xxx compress_savefile */
    "hilite_player", /* OPT_hilite_player */
    "hilite_target", /* OPT_hilite_target */
    "hilite_unwary", /* OPT_hilite_unwary */
    "solid_walls", /* OPT_solid_walls */
    "hybrid_walls", /* OPT_hybrid_walls */
    NULL, /* xxx easy_open */
    NULL, /* xxx easy_alter */
    NULL, /* xxx easy_floor */
    "instant_run", /* OPT_instant_run */
    "center_player", /* OPT_center_player */
    "run_avoid_center", /* OPT_run_avoid_center */
    NULL, /* xxx scroll_target */
    "auto_more", /* OPT_auto_more */
    "know_monster_info", /* OPT_know_monster_info */
    "always_show_list", /* OPT_always_show_list */
    "easy_main_menu", /* OPT_easy_main_menu */
    NULL, /* xxx verify_leave_quests*/
    NULL, /* xxx mark_squelch_items */
    "display_hits", /* OPT_display_hits */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx birth_point_based */
    NULL, /* xxx birth_auto_roller */
    NULL, /* xxx birth_maximize */
    "birth_discon_stair", /* OPT_birth_discon_stair */
    "birth_ironman", /* OPT_birth_ironman */
    NULL, "birth_no_artefacts", /* OPT_birth_no_artefacts */
    "birth_fixed_exp", /* OPT_birth_fixed_exp */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx birth_force_small_lev */
    NULL, /* xxx birth_retain_squelch */
    NULL, /* xxx birth_no_quests*/
    NULL, /* xxx birth_no_player ghosts*/
    NULL, /* xxx birth_no_store_services*/
    NULL, /* xxx birth_no_xtra_artefacts*/
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    "cheat_peek", /* OPT_cheat_peek */
    "cheat_hear", /* OPT_cheat_hear */
    "cheat_room", /* OPT_cheat_room */
    "cheat_xtra", /* OPT_cheat_xtra */
    "cheat_know", /* OPT_cheat_know */
    "cheat_live", /* OPT_cheat_live */
    "cheat_monsters", /* OPT_cheat_monsters */
    "cheat_noise", /* OPT_cheat_noise */
    "cheat_scent", /* OPT_cheat_scent */
    "cheat_light", /* OPT_cheat_light */
    "cheat_skill_rolls", /* OPT_cheat_skill_rolls */
    "cheat_timestop", /* OPT_cheat_timestop */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx adult_point_based */
    NULL, /* xxx adult_auto_roller */
    NULL, /* xxx adult_maximize */
    "adult_discon_stair", /* OPT_adult_discon_stair */
    "adult_ironman", /* OPT_adult_ironman */
    NULL, /* xxx adult_no_stores */
    "adult_no_artefacts", /* OPT_adult_no_artefacts */
    NULL, /* xxx adult_rand_artefacts */
    NULL, /* xxx adult_no_stacking */
    NULL, /* xxx adult_auto_notes */
    NULL, /* xxx adult_force_small_lev*/
    NULL, /* xxx adult_retain_squelch */
    NULL, /* xxx adult_no_quests*/
    NULL, /* xxx adult_no_player ghosts*/
    NULL, /* xxx adult_no_store_services*/
    NULL, /* xxx adult_no_xtra_artefacts*/
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    "score_peek", /* OPT_score_peek */
    "score_hear", /* OPT_score_hear */
    "score_room", /* OPT_score_room */
    "score_xtra", /* OPT_score_xtra */
    "score_know", /* OPT_score_know */
    "score_live", /* OPT_score_live */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL /* xxx */
};

/*
 * Options -- descriptions (where defined)
 */
cptr option_desc[OPT_MAX] = {
    "Move with hjkl etc. (use ^ for underlying keys)", /* OPT_hjkl_movement */
    "Dismiss '-more-' and 'y/n' prompts with any key", /* OPT_quick_messages */
    "Use a keyset more closely based on Angband", /* OPT_angband_keyset */
    NULL, /* xxx carry_query */
    "Stop singing when you use the rest command", /* OPT_stop_singing_on_rest */
    NULL, /* xxx always_pickup */
    "Forgo bonus attacks on non-alert enemies", /* OPT_forgo_attacking_unwary */
    NULL, /* xxx depth_in_feet */
    NULL, /* xxx stack_force_notes */
    NULL, /* xxx stack_force_costs */
    NULL, /* xxx show_labels */
    NULL, /* xxx show_weights */
    NULL, /* xxx show_choices */
    NULL, /* xxx show_details */
    "Audible beep (on errors/warnings)", /* OPT_system_beep */
    NULL, /* xxx show_flacors */
    NULL, /* xxx run_ignore_stairs */
    NULL, /* xxx run_ignore_doors */
    NULL, /* xxx run_cut_corners */
    NULL, /* xxx run_use_corners */
    NULL, /* xxx disturb_move */
    NULL, /* xxx disturb_near */
    NULL, /* xxx disturb_panel */
    NULL, /* xxx disturb_state */
    NULL, /* xxx disturb_minor */
    NULL, /* xxx disturb_wakeup */
    NULL, /* xxx alert_hitpoint */
    NULL, /* xxx alert_failure */
    NULL, /* xxx verify_destroy */
    NULL, /* xxx verify_special */
    NULL, /* xxx allow_quantity */
    NULL, /* xxx */
    NULL, /* xxx auto_haggle */
    NULL, /* xxx auto_scum */
    NULL, /* xxx allow_themed_levels */
    NULL, /* xxx testing_carry */
    NULL, /* xxx expand_look */
    NULL, /* xxx expand_list */
    NULL, /* xxx view_perma_grids */
    NULL, /* xxx view_torch_grids */
    NULL, /* xxx dungeon_align */
    NULL, /* xxx dungeon_stair */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx track_follow */
    NULL, /* xxx track_target */
    NULL, /* xxx track_target */
    NULL, /* xxx smart_cheat */
    NULL, /* xxx view_reduce_lite */
    NULL, /* xxx hidden_player */
    NULL, /* xxx avoid_abort */
    NULL, /* xxx avoid_other */
    NULL, /* xxx flush_failure */
    NULL, /* xxx flush_disturb */
    NULL, /* xxx */
    NULL, /* xxx fresh_before */
    NULL, /* xxx fresh_after */
    NULL, /* xxx */
    NULL, /* xxx compress_savefile */
    "Highlight the player with the cursor", /* OPT_hilite_player */
    "Highlight the target with the cursor", /* OPT_hilite_target */
    "Highlight sleeping and unwary creatures", /* OPT_hilite_unwary */
    "Display walls as solid blocks", /* OPT_solid_walls */
    "Display walls as semi-solid", /* OPT_hybrid_walls */
    NULL, /* xxx easy_open */
    NULL, /* xxx easy_alter */
    NULL, /* xxx easy_floor */
    "Faster display while running", /* OPT_instant_run */
    "Center map continuously (very slow)", /* OPT_center_player */
    "Avoid centering while running", /* OPT_run_avoid_center */
    NULL, /* xxx scroll_target */
    "Automatically dismiss '-more-' messages", /* OPT_auto_more */
    "Know all monster info", /* OPT_know_monster_info */
    "Automatically display drop-down lists", /* OPT_auto_display_lists */
    "Use the Escape key to access the main menu", /* OPT_easy_main_menu */
    NULL, /* xxx verify_leave_quest */
    NULL, /* xxx mark_squelch_items */
    "Display a mark when something gets hit", /* OPT_display_hits */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx birth_point_based */
    NULL, /* xxx birth_auto_roller */
    NULL, /* xxx birth_maximize */
    "Disconnected stairs", /* OPT_birth_discon_stair */
    "Straight down (no up stairs until endgame)", /* OPT_birth_ironman */
    NULL, "No artefacts", /* OPT_birth_no_artefacts */
    "Fixed XP - gain 50K at start and nothing after", /* OPT_birth_fixed_exp */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx birth_take_notes */
    NULL, /* xxx birth_force_small_lev */
    NULL, /* xxx birth_retain_squelch*/
    NULL, /* xxx birth_no_quests*/
    NULL, /* xxx birth_no_player ghosts*/
    NULL, /* xxx birth_no_store_services*/
    NULL, /* xxx birth_no_xtra_artefacts*/
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    "Debug: Peek into object creation", /* OPT_cheat_peek */
    "Debug: Peek into monster creation", /* OPT_cheat_hear */
    "Debug: Peek into dungeon creation", /* OPT_cheat_room */
    "Debug: Peek into something else", /* OPT_cheat_xtra */
    "Debug: Know complete monster info", /* OPT_cheat_know */
    "Debug: Allow player to avoid death", /* OPT_cheat_live */
    "Debug: Continually display all monsters", /* OPT_cheat_monsters */
    "Debug: Continually display noise levels", /* OPT_cheat_noise */
    "Debug: Continually display scent levels", /* OPT_cheat_scent */
    "Debug: Continually display light levels", /* OPT_cheat_light */
    "Debug: Show all skill rolls", /* OPT_cheat_skill_rolls */
    "Debug: Don't allow monsters to move", /* OPT_timestop */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx adult_point_based */
    NULL, /* xxx adult_auto_roller */
    NULL, /* xxx adult_maximize */
    "Disconnected stairs", /* OPT_adult_discon_stair */
    "Straight down (no up stairs until endgame)", /* OPT_adult_ironman */
    NULL, /* xxx adult_no_stores */
    "No artefacts", /* OPT_adult_no_artefacts */
    NULL, /* xxx adult_rand_artefacts */
    NULL, /* xxx adult_adult_no_stacking */
    NULL, /* xxx adult_take_notes */
    NULL, /* xxx adult_force_small_lev */
    NULL, /* xxx adult_retain_squelch*/
    NULL, /* xxx adult_no_quests*/
    NULL, /* xxx adult_no_player ghosts*/
    NULL, /* xxx adult_no_store_services*/
    NULL, /* xxx adult_no_xtra_artefacts*/
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    "Score: Peek into object creation", /* OPT_score_peek */
    "Score: Peek into monster creation", /* OPT_score_hear */
    "Score: Peek into dungeon creation", /* OPT_score_room */
    "Score: Peek into something else", /* OPT_score_xtra */
    "Score: Know complete monster info", /* OPT_score_know */
    "Score: Allow player to avoid death", /* OPT_score_live */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL, /* xxx */
    NULL /* xxx */
};

/*
 * Options -- normal values
 */
const bool option_norm[OPT_MAX] = {
    false, /* OPT_hjkl_movement */
    true, /* OPT_quick_messages */
    false, /* OPT_angband_keyset */
    false, /* xxx carry_query */
    true, /* OPT_stop_singing_on_rest */
    false, /* xxx always_pickup */
    true, /* OPT_forgo_attacking_unwary */
    false, /* xxx depth_in_feet */
    false, /* xxx stack_force_notes */
    false, /* xxx stack_force_costs */
    false, /* xxx show_labels */
    false, /* xxx show_weights */
    false, /* xxx show_choices */
    false, /* xxx show_details */
    true, /* OPT_system_beep */
    false, /* xxx show_flavors */
    false, /* xxx run_ignore_stairs */
    false, /* xxx run_ignore_doors */
    false, /* xxx run_cut_corners */
    false, /* xxx run_use_corners */
    false, /* xxx disturb_move */
    false, /* xxx disturb_near */
    false, /* xxx disturb_panel */
    false, /* xxx disturb_state */
    false, /* xxx disturb_minor */
    false, /* xxx disturb_wakeup */
    false, /* xxx alert_hitpoint */
    false, /* xxx alert_failure */
    false, /* xxx verify_destroy */
    false, /* xxx verify_special */
    false, /* xxx allow_quantity */
    false, /* xxx */
    false, /* xxx auto_haggle */
    false, /* xxx auto_scum */
    false, /* xxx allow_themed_levels */
    false, /* xxx */
    false, /* xxx expand_look */
    false, /* xxx expand_list */
    false, /* xxx view_perma_grids */
    false, /* xxx view_torch_grids */
    false, /* xxx dungeon_align */
    false, /* xxx dungeon_stair */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx track_follow */
    false, /* xxx track_target */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx view_reduce_lite */
    false, /* xxx hidden_player */
    false, /* xxx avoid_abort */
    false, /* xxx avoid_other */
    false, /* xxx flush_failure */
    false, /* xxx flush_disturb */
    false, /* xxx */
    false, /* xxx fresh_before */
    false, /* xxx fresh_after */
    false, /* xxx */
    false, /* xxx compress_savefile */
    false, /* OPT_hilite_player */
    true, /* OPT_hilite_target */
    true, /* OPT_hilite_unwary */
    true, /* OPT_solid_walls */
    false, /* OPT_hybrid_walls */
    false, /* xxx easy_open */
    false, /* xxx easy_alter */
    false, /* xxx easy_floor */
    false, /* OPT_instant_run */
    false, /* OPT_center_player */
    false, /* OPT_run_avoid_center */
    false, /* xxx scroll_target */
    false, /* OPT_auto_more */
    false, /* OPT_know_monster_info */
    false, /* OPT_auto_display_lists */
    true, /* OPT_easy_main_menu */
    false, /* xxx verify_quest_leave */
    false, /* xxx mark_squelch_items */
    true, /* OPT_display_hits */
    false, /* OPT_display_wakings */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx birth_point_based */
    false, /* xxx birth_auto_roller */
    false, /* xxx birth_maximize */
    false, /* OPT_birth_discon_stair */
    false, /* OPT_birth_ironman */
    false, /* xxx */
    false, /* OPT_birth_no_artefacts */
    false, /* OPT_birth_fixed_exp */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx birth_force_small_lev */
    false, /* xxx birth_retain_squelch */
    false, /* xxx OPT_birth_no_quests*/
    false, /* xxx birth_no_player ghosts*/
    false, /* xxx birth_no_store_services*/
    false, /* xxx birth_no_xtra_artefacts*/
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* OPT_cheat_peek */
    false, /* OPT_cheat_hear */
    false, /* OPT_cheat_room */
    false, /* OPT_cheat_xtra */
    false, /* OPT_cheat_know */
    false, /* OPT_cheat_live */
    false, /* OPT_cheat_monsters */
    false, /* OPT_cheat_noise */
    false, /* OPT_cheat_scent */
    false, /* OPT_cheat_light */
    false, /* OPT_cheat_skill_rolls */
    false, /* OPT_cheat_timestop */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx adult_point_based */
    false, /* xxx adult_auto_roller */
    false, /* xxx adult_maximize */
    false, /* OPT_adult_discon_stair */
    false, /* OPT_adult_ironman */
    false, /* xxx adult_no_stores */
    false, /* OPT_adult_no_artefacts */
    false, /* xxx adult_rand_artefacts */
    false, /* xxx adult_no_stacking */
    false, /* xxx adult_take_notes */
    false, /* xxx adult_force_small_lev*/
    false, /* xxx adult_retain_squelch */
    false, /* xxx OPT_adult_no_quests */
    false, /* xxx adult_no_player ghosts */
    false, /* xxx adult_no_store_services */
    false, /* xxx adult_no_xtra_artefacts */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* OPT_score_peek */
    false, /* OPT_score_hear */
    false, /* OPT_score_room */
    false, /* OPT_score_xtra */
    false, /* OPT_score_know */
    false, /* OPT_score_live */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false, /* xxx */
    false /* xxx */
};

/*
 * Option screen interface
 */
const byte option_page[OPT_PAGE_MAX][OPT_PAGE_PER] = {
    /*** User-Interface ***/

    { OPT_forgo_attacking_unwary, OPT_stop_singing_on_rest, OPT_system_beep,
        OPT_quick_messages, OPT_auto_more, OPT_easy_main_menu,
        OPT_hjkl_movement, OPT_angband_keyset, OPT_hitpoint_warning,
        OPT_know_monster_info, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Disturbance ***/

    { OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Game-Play ***/

    { OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Efficiency ***/

    { OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Display ***/

    { OPT_display_hits, OPT_auto_display_lists, OPT_instant_run,
        OPT_center_player, OPT_run_avoid_center, OPT_hilite_player,
        OPT_hilite_target, OPT_hilite_unwary, OPT_solid_walls, OPT_hybrid_walls,
        OPT_delay_factor, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Birth ***/

    { OPT_birth_discon_stair, OPT_birth_ironman, OPT_birth_no_artefacts,
        OPT_birth_fixed_exp, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Cheat ***/

    { OPT_cheat_peek, OPT_cheat_hear, OPT_cheat_room, OPT_cheat_xtra,
        OPT_cheat_know, OPT_cheat_live, OPT_cheat_monsters, OPT_cheat_noise,
        OPT_cheat_scent, OPT_cheat_light, OPT_cheat_skill_rolls,
        OPT_cheat_timestop, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE }
};

cptr inscrip_text[MAX_INSCRIP] = { NULL,
    "artefact, cursed", // old: terrible
    "special, cursed", // old: worthless
    "cursed", "broken",
    "average", // old: average
    "fine", // old: good
    "fine", // old: good
    "special", // old: excellent
    "artefact", // old: special
    "uncursed", "indestructible" };

/*
 * First column is Mana Cost
 * Second column is number of sides of damage
 * Third column is Optimal Ranges for various spells.
 *   - the degree of preference for a range is given in the next table
 */

// {Mana_cost,  dam_sides,  best_range}
byte spell_info_RF4[32][3] = {
    { 0, 7, 4 }, /* RF4_ARROW1 */
    { 0, 7, 4 }, /* RF4_ARROW2 */
    { 0, 4, 4 }, /* RF4_BOULDER */
    { MON_MANA_COST, 4, 2 }, /* RF4_BRTH_FIRE */
    { MON_MANA_COST, 4, 2 }, /* RF4_BRTH_COLD */
    { MON_MANA_COST, 4, 2 }, /* RF4_BRTH_POIS */
    { MON_MANA_COST, 4, 2 }, /* RF4_BRTH_DARK */
    { MON_MANA_COST, 4, 2 }, /* RF4_EARTHQUAKE */
    { MON_MANA_COST, 0, 0 }, /* RF4_SHRIEK */
    { 0, 1, 2 }, /* RF4_SCREECH */
    { MON_MANA_COST, 0, 0 }, /* RF4_DARKNESS */
    { MON_MANA_COST, 0, 0 }, /* RF4_FORGET */
    { MON_MANA_COST, 0, 0 }, /* RF4_SCARE */
    { MON_MANA_COST, 0, 0 }, /* RF4_CONF */
    { MON_MANA_COST, 0, 0 }, /* RF4_HOLD */
    { MON_MANA_COST, 0, 0 }, /* RF4_SLOW */
    { MON_MANA_COST, 0, 0 }, /* RF4_HATCH_SPIDER */
    { MON_MANA_COST, 0, 0 }, /* RF4_DIM */

    { MON_MANA_COST, 0, 0 }, /* RF4_SNG_BINDING */
    { MON_MANA_COST, 0, 0 }, /* RF4_SNG_PIERCING */
    { MON_MANA_COST, 0, 0 }, /* RF4_SNG_OATHS */

    { 0, 0, 0 }, /* RF4_XXX22 */
    { 0, 0, 0 }, /* RF4_XXX23 */
    { MON_MANA_COST, 0, 0 }, /* RF4_THROW_WEB */
    { MON_MANA_COST, 0, 0 }, /* RF4_RALLY */
    { 0, 0, 0 }, /* RF4_XXX26 */
    { 0, 0, 0 }, /* RF4_XXX27 */
    { 0, 0, 0 }, /* RF4_XXX28 */
    { 0, 0, 0 }, /* RF4_XXX29 */
    { 0, 0, 0 }, /* RF4_XXX30 */
    { 0, 0, 0 }, /* RF4_XXX31 */
    { 0, 0, 0 } /* RF4_XXX32 */
};

/*
 * desirability:			base desirability for AI.
 * usefulness past range:   % of spell desirability retained for each step past
 * 'range'
 */

byte spell_desire_RF4[32][2] = {
    /*  { desirability,  usefulness past range }  */
    { 100, 100 }, /* RF4_ARROW1	    */
    { 100, 100 }, /* RF4_ARROW2	    */
    { 50, 100 }, /* RF4_BOULDER   */
    { 100, 50 }, /* RF4_BRTH_FIRE */
    { 100, 50 }, /* RF4_BRTH_COLD */
    { 100, 50 }, /* RF4_BRTH_POIS */
    { 100, 50 }, /* RF4_BRTH_DARK */
    { 50, 0 }, /* RF4_EARTHQUAKE   */
    { 50, 100 }, /* RF4_SHRIEK    */
    { 50, 100 }, /* RF4_SCREECH    */
    { 50, 100 }, /* RF4_DARKNESS  */
    { 50, 100 }, /* RF4_FORGET    */
    { 50, 100 }, /* RF4_SCARE	 */
    { 50, 100 }, /* RF4_CONF	  */
    { 50, 100 }, /* RF4_HOLD	*/
    { 50, 100 }, /* RF4_SLOW	*/
    { 50, 100 }, /* RF4_HATCH_SPIDER */
    { 50, 100 }, /* RF4_DIM */

    { 50, 100 }, /* RF4_SNG_BINDING */
    { 0, 0 }, /* RF4_SNG_PIERCING */
    { 50, 100 }, /* RF4_SNG_OATHS */

    { 0, 100 }, /* RF4_XXX22 */
    { 0, 100 }, /* RF4_XXX23 */
    { 50, 100 }, /* RF4_THROW_WEB */
    { 50, 100 }, /* RF4_RALLY */
    { 0, 100 }, /* RF4_XXX26 */
    { 0, 100 }, /* RF4_XXX27 */
    { 0, 100 }, /* RF4_XXX28 */
    { 0, 100 }, /* RF4_XXX29 */
    { 0, 100 }, /* RF4_XXX30 */
    { 0, 100 }, /* RF4_XXX31 */
    { 0, 100 } /* RF4_XXX32 */
};
