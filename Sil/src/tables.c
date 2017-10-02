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
const s16b ddd[9] =
{ 2, 8, 6, 4, 3, 1, 9, 7, 5 };

/*
 * Global arrays for converting "keypad direction" into "offsets".
 */
const s16b ddx[10] =
{ 0, -1, 0, 1, -1, 0, 1, -1, 0, 1 };

const s16b ddy[10] =
{ 0, 1, 1, 1, 0, 0, 0, -1, -1, -1 };

/*
 * Global arrays for optimizing "ddx[ddd[i]]" and "ddy[ddd[i]]".
 */
const s16b ddx_ddd[9] =
{ 0, 0, 1, -1, 1, -1, 1, -1, 0 };

const s16b ddy_ddd[9] =
{ 1, -1, 0, 0, 1, 1, -1, -1, 0 };


/*
 * Global array for converting numbers to uppercase hecidecimal digit
 * This array can also be used to convert a number to an octal digit
 */
const char hexsym[16] =
{
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};



/*
 * This table allows quick conversion from "speed" to "energy"
 * It used to be complex, but in Sil it is basically linear.
 * It is set up so that there are 10 game turns per player turn at normal speed
 *
 * Note that creatures should never have speed 0 in the first place
 */
const byte extract_energy[8] =
{
	/* impossible */  5,
	/* Slow */        5,
	/* Normal */     10,
	/* Fast */       15,
	/* V Fast */     20,
	/* X Fast */     25,
	/* I Fast */     30,
	/* A Fast */     35,
};


/*
 * Player Sexes
 *
 *	Title,
 *	Winner
 */
const player_sex sex_info[MAX_SEXES+1] =
{
	{
		"Female",
		"Lady"
	},

	{
		"Male",
		"Lord"
	},
	
	{
		"",
		""
	}
};


/*
 * Each chest has a certain set of traps, determined by pval
 * Each chest has a "pval" from 1 to the chest level (max 55)
 * If the "pval" is negative then the trap has been disarmed
 * The "pval" of a chest determines the quality of its treasure
 * Note that disarming a trap on a chest also removes the lock.
 */

const byte chest_traps[25+1] =
{
	0,					/* 0 == empty */
	(CHEST_GAS_CONF),
	(CHEST_GAS_CONF),
	(CHEST_GAS_STUN),
	0,
	(CHEST_GAS_STUN),
	(CHEST_GAS_POISON),
	(CHEST_GAS_POISON),
	0,
	(CHEST_NEEDLE_ENTRANCE),
	(CHEST_NEEDLE_ENTRANCE),
	(CHEST_NEEDLE_HALLU),
	0,
	(CHEST_NEEDLE_HALLU),
	(CHEST_NEEDLE_LOSE_STR),
	(CHEST_NEEDLE_LOSE_STR),
	0,
	(CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
	(CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
	(CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR),
	0,
	(CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR),
	(CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),
	(CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),
	0,
	(CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),			/* 25 == best */
};



/*
 * Hack -- the "basic" color names (see "TERM_xxx")
 */
cptr color_names[16] =
{
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
cptr stat_names[A_MAX] =
{
	"Str ", "Dex ", "Con ", "Gra "
};

/*
 * Abbreviations of damaged stats
 */
cptr stat_names_reduced[A_MAX] =
{
	"Str ", "Dex ", "Con ", "Gra "
};

/*
 * Full stat names
 */
cptr stat_names_full[A_MAX] =
{
	"strength",
	"dexterity",
	"constitution",
	"grace"
};

/*
 * Abbreviations of skills
 */
cptr skill_names[S_MAX] =
{
	"Mel",
	"Arc",
	"Evn",
	"Stl",
	"Per",
	"Wil",
	"Cmt",
	"Sng"
};

/*
 * Full skill names
 */
cptr skill_names_full[S_MAX] =
{
	"Melee",
	"Archery",
	"Evasion",
	"Stealth",
	"Perception",
	"Will",
	"Smithing",
	"Song"
};



/*
 * Certain "screens" always use the main screen, including News, Birth,
 * Dungeon, Tomb-stone, High-scores, Macros, Colors, Visuals, Options.
 *
 * Later, special flags may allow sub-windows to "steal" stuff from the
 * main window, including File dump (help), File dump (artefacts, uniques),
 * Character screen, Small scale map, Previous Messages, Store screen, etc.
 */
cptr window_flag_desc[32] =
{
	"Display inven/equip",
	"Display equip/inven",
	"Display player (basic)",
	"Display player (extra)",
	"Display combat rolls",
	"Display monster recall",
	"Display object recall",
	"Display messages",
	"Display overhead view",
	"Display monster list",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};


/*
 * Options -- textual names (where defined)
 */
cptr option_text[OPT_MAX] =
{
	"hjkl_movement",			/* OPT_hjkl_movement */
	"quick_messages",			/* OPT_quick_messages */
	"angband_keyset",			/* OPT_angband_keyset */
	NULL,                       /* xxx carry_query */
	"stop_singing_on_rest",		/* OPT_stop_singing_on_rest */
	NULL,                       /* xxx always_pickup */
	"forgo_attacking_unwary",	/* OPT_forgo_attacking_unwary */
	NULL,						/* xxx depth_in_feet */
	NULL,						/* xxx stack_force_notes */
	NULL,						/* xxx stack_force_costs */
	NULL,						/* xxx show_labels */
	NULL,						/* xxx show_weights */
	NULL,						/* xxx show_choices */
	NULL,						/* xxx show_details */
	"beep",						/* OPT_system_beep */
	NULL,						/* xxx flavors */
	NULL,						/* xxx run_ignore_stairs */
	NULL,						/* xxx run_ignore_doors */
	NULL,						/* xxx run_cut_corners */
	NULL,						/* xxx run_use_corners */
	NULL,						/* xxx disturb_move */
	NULL,						/* xxx disturb_near */
	NULL,						/* xxx disturb_panel */
	NULL,						/* xxx disturb_state */
	NULL,						/* xxx disturb_minor */
	NULL,						/* xxx disturb_wakeup */
	NULL,						/* xxx alert_hitpoint */
	NULL,						/* xxx alert_failure */
	NULL,						/* xxx verify_destroy */
	NULL,						/* xxx verify_special */
	NULL,						/* xxx allow_quantity */
	NULL,						/* xxx */
	NULL,						/* xxx auto_haggle */
	NULL,						/* xxx auto_scum */
	NULL,						/* xxx allow_themed_levels */
	NULL,						/* xxx testing_carry */
	NULL,						/* xxx expand_look */
	NULL,						/* xxx expand_list */
	NULL,						/* xxx view_perma_grids */
	NULL,						/* xxx view_torch_grids */
	NULL,						/* xxx dungeon_align */
	NULL,						/* xxx dungeon_stair */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx track_follow */
	NULL,						/* xxx track_target */
	NULL,						/* xxx track_target */
	NULL,						/* xxx smart_cheat */
	NULL,						/* xxx view_reduce_lite */
	NULL,						/* xxx hidden_player */
	NULL,						/* xxx avoid_abort */
	NULL,						/* xxx avoid_other */
	NULL,						/* xxx flush_failure */
	NULL,						/* xxx flush_disturb */
	NULL,						/* xxx flush_command */
	NULL,						/* xxx fresh_before */
	NULL,						/* xxx fresh_after */
	NULL,						/* xxx fresh_message */
	NULL,						/* xxx compress_savefile */
	"hilite_player",			/* OPT_hilite_player */
	"hilite_target",			/* OPT_hilite_target */
	"hilite_unwary",			/* OPT_hilite_unwary */
	"solid_walls",				/* OPT_solid_walls */
	"hybrid_walls",				/* OPT_hybrid_walls */
	NULL,						/* xxx easy_open */
	NULL,						/* xxx easy_alter */
	NULL,						/* xxx easy_floor */
	"instant_run",				/* OPT_instant_run */
	"center_player",			/* OPT_center_player */
	"run_avoid_center",			/* OPT_run_avoid_center */
	NULL,						/* xxx scroll_target */
	"auto_more",				/* OPT_auto_more */
	NULL,						/* xxx toggle_exp*/
	"always_show_list",			/* OPT_always_show_list */
	"easy_main_menu",			/* OPT_easy_main_menu */
	NULL,						/* xxx verify_leave_quests*/
	NULL,						/* xxx mark_squelch_items */
	"display_hits",				/* OPT_display_hits */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx birth_point_based */
	NULL,						/* xxx birth_auto_roller */
	NULL,						/* xxx birth_maximize */
	"birth_discon_stair",	/* OPT_birth_discon_stair */
	"birth_ironman",			/* OPT_birth_ironman */
	NULL,						/* xxx birth_no_stores */
	"birth_no_artefacts",		/* OPT_birth_no_artefacts */
	NULL,						/* xxx birth_rand_artefacts */
	NULL,						/* xxx birth_no_stacking */
 	NULL,						/* xxx birth_auto_notes */
 	NULL,						/* xxx birth_force_small_lev */
	NULL,						/* xxx birth_retain_squelch */
	NULL,						/* xxx birth_no_quests*/
	NULL,						/* xxx birth_no_player ghosts*/
	NULL,						/* xxx birth_no_store_services*/
	NULL,						/* xxx birth_no_xtra_artefacts*/
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	"cheat_peek",				/* OPT_cheat_peek */
	"cheat_hear",				/* OPT_cheat_hear */
	"cheat_room",				/* OPT_cheat_room */
	"cheat_xtra",				/* OPT_cheat_xtra */
	"cheat_know",				/* OPT_cheat_know */
	"cheat_live",				/* OPT_cheat_live */
	"cheat_monsters",			/* OPT_cheat_monsters */
	"cheat_noise",				/* OPT_cheat_noise */
	"cheat_scent",				/* OPT_cheat_scent */
	"cheat_light",				/* OPT_cheat_light */
	"cheat_skill_rolls",		/* OPT_cheat_skill_rolls */
	"cheat_timestop",			/* OPT_cheat_timestop */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx adult_point_based */
	NULL,						/* xxx adult_auto_roller */
	NULL,						/* xxx adult_maximize */
	"adult_discon_stair",	/* OPT_adult_discon_stair */
	"adult_ironman",			/* OPT_adult_ironman */
	NULL,						/* xxx adult_no_stores */
	"adult_no_artefacts",		/* OPT_adult_no_artefacts */
	NULL,						/* xxx adult_rand_artefacts */
	NULL,						/* xxx adult_no_stacking */
	NULL,						/* xxx adult_auto_notes */
	NULL,						/* xxx adult_force_small_lev*/
	NULL,						/* xxx adult_retain_squelch */
	NULL,						/* xxx adult_no_quests*/
	NULL,						/* xxx adult_no_player ghosts*/
	NULL,						/* xxx adult_no_store_services*/
	NULL,						/* xxx adult_no_xtra_artefacts*/
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	"score_peek",				/* OPT_score_peek */
	"score_hear",				/* OPT_score_hear */
	"score_room",				/* OPT_score_room */
	"score_xtra",				/* OPT_score_xtra */
	"score_know",				/* OPT_score_know */
	"score_live",				/* OPT_score_live */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL,						/* xxx */
	NULL						/* xxx */
};


/*
 * Options -- descriptions (where defined)
 */
cptr option_desc[OPT_MAX] =
{
	"Move with hjkl etc. (use ^ for underlying keys)",	/* OPT_hjkl_movement */
	"Dismiss '-more-' and 'y/n' prompts with any key",	/* OPT_quick_messages */
	"Use a keyset more closely based on Angband",		/* OPT_angband_keyset */
	NULL,                                               /* xxx carry_query */
	"Stop singing when you use the rest command",       /* OPT_stop_singing_on_rest */
	NULL,                                               /* xxx always_pickup */
	"Forgo bonus attacks on non-alert enemies",         /* OPT_forgo_attacking_unwary */
	NULL,                                               /* xxx depth_in_feet */
	NULL,												/* xxx stack_force_notes */
	NULL,												/* xxx stack_force_costs */
	NULL,												/* xxx show_labels */
	NULL,												/* xxx show_weights */
	NULL,												/* xxx show_choices */
	NULL,												/* xxx show_details */
	"Audible beep (on errors/warnings)",				/* OPT_system_beep */
	NULL,										/* xxx show_flacors */
	NULL,										/* xxx run_ignore_stairs */
	NULL,										/* xxx run_ignore_doors */
	NULL,										/* xxx run_cut_corners */
	NULL,										/* xxx run_use_corners */
	NULL,										/* xxx disturb_move */
	NULL,										/* xxx disturb_near */
	NULL,										/* xxx disturb_panel */
	NULL,										/* xxx disturb_state */
	NULL,										/* xxx disturb_minor */
	NULL,										/* xxx disturb_wakeup */
	NULL,										/* xxx alert_hitpoint */
	NULL,										/* xxx alert_failure */
	NULL,										/* xxx verify_destroy */
	NULL,										/* xxx verify_special */
	NULL,										/* xxx allow_quantity */
	NULL,										/* xxx */
	NULL,										/* xxx auto_haggle */
	NULL,										/* xxx auto_scum */
	NULL,										/* xxx allow_themed_levels */
	NULL,										/* xxx testing_carry */
	NULL,										/* xxx expand_look */
	NULL,										/* xxx expand_list */
	NULL,										/* xxx view_perma_grids */
	NULL,										/* xxx view_torch_grids */
	NULL,										/* xxx dungeon_align */
	NULL,										/* xxx dungeon_stair */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx track_follow */
	NULL,										/* xxx track_target */
	NULL,										/* xxx track_target */
	NULL,										/* xxx smart_cheat */
	NULL,										/* xxx view_reduce_lite */
	NULL,										/* xxx hidden_player */
	NULL,										/* xxx avoid_abort */
	NULL,										/* xxx avoid_other */
	NULL,										/* xxx flush_failure */
	NULL,										/* xxx flush_disturb */
	NULL,										/* xxx */
	NULL,										/* xxx fresh_before */
	NULL,										/* xxx fresh_after */
	NULL,										/* xxx */
	NULL,										/* xxx compress_savefile */
	"Highlight the player with the cursor",		/* OPT_hilite_player */
	"Highlight the target with the cursor",		/* OPT_hilite_target */
	"Highlight sleeping and unwary creatures",	/* OPT_hilite_unwary */
	"Display walls as solid blocks",			/* OPT_solid_walls */
	"Display walls as semi-solid",				/* OPT_hybrid_walls */
	NULL,										/* xxx easy_open */
	NULL,										/* xxx easy_alter */
	NULL,										/* xxx easy_floor */
	"Faster display while running",				/* OPT_instant_run */
	"Center map continuously (very slow)",		/* OPT_center_player */
	"Avoid centering while running",			/* OPT_run_avoid_center */
	NULL,										/* xxx scroll_target */
	"Automatically dismiss '-more-' messages",	/* OPT_auto_more */
	NULL,										/* xxx toggle_xp */
	"Automatically display drop-down lists",	/* OPT_auto_display_lists */
	"Use the Escape key to access the main menu",	/* OPT_easy_main_menu */
	NULL,										/* xxx verify_leave_quest */
	NULL,										/* xxx mark_squelch_items */
	"Display a mark when something gets hit",	/* OPT_display_hits */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx birth_point_based */
	NULL,										/* xxx birth_auto_roller */
	NULL,										/* xxx birth_maximize */
	"Disconnected stairs",						/* OPT_birth_discon_stair */
	"Straight down (no up stairs until endgame)",/* OPT_birth_ironman */
	NULL,										/* xxx birth_no_stores */
	"No artefacts",								/* OPT_birth_no_artefacts */
	NULL,										/* xxx birth_rand_artefacts */
	NULL,										/* xxx birth_no_stacking */
 	NULL,										/* xxx birth_take_notes */
 	NULL,										/* xxx birth_force_small_lev */
	NULL,										/* xxx birth_retain_squelch*/
	NULL,										/* xxx birth_no_quests*/
	NULL,										/* xxx birth_no_player ghosts*/
	NULL,										/* xxx birth_no_store_services*/
	NULL,										/* xxx birth_no_xtra_artefacts*/
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	"Debug: Peek into object creation",			/* OPT_cheat_peek */
	"Debug: Peek into monster creation",		/* OPT_cheat_hear */
	"Debug: Peek into dungeon creation",		/* OPT_cheat_room */
	"Debug: Peek into something else",			/* OPT_cheat_xtra */
	"Debug: Know complete monster info",		/* OPT_cheat_know */
	"Debug: Allow player to avoid death",		/* OPT_cheat_live */
	"Debug: Continually display all monsters",	/* OPT_cheat_monsters */
	"Debug: Continually display noise levels",	/* OPT_cheat_noise */
	"Debug: Continually display scent levels",	/* OPT_cheat_scent */
	"Debug: Continually display light levels",	/* OPT_cheat_light */
	"Debug: Show all skill rolls",				/* OPT_cheat_skill_rolls */
	"Debug: Don't allow monsters to move",		/* OPT_timestop */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx adult_point_based */
	NULL,										/* xxx adult_auto_roller */
	NULL,										/* xxx adult_maximize */
	"Disconnected stairs",						/* OPT_adult_discon_stair */
	"Straight down (no up stairs until endgame)",/* OPT_adult_ironman */
	NULL,										/* xxx adult_no_stores */
	"No artefacts",								/* OPT_adult_no_artefacts */
	NULL,										/* xxx adult_rand_artefacts */
	NULL,										/* xxx adult_adult_no_stacking */
	NULL,										/* xxx adult_take_notes */
	NULL,										/* xxx adult_force_small_lev */
	NULL,										/* xxx adult_retain_squelch*/
	NULL,										/* xxx adult_no_quests*/
	NULL,										/* xxx adult_no_player ghosts*/
	NULL,										/* xxx adult_no_store_services*/
	NULL,										/* xxx adult_no_xtra_artefacts*/
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	"Score: Peek into object creation",			/* OPT_score_peek */
	"Score: Peek into monster creation",		/* OPT_score_hear */
	"Score: Peek into dungeon creation",		/* OPT_score_room */
	"Score: Peek into something else",			/* OPT_score_xtra */
	"Score: Know complete monster info",		/* OPT_score_know */
	"Score: Allow player to avoid death",		/* OPT_score_live */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL,										/* xxx */
	NULL										/* xxx */
};


/*
 * Options -- normal values
 */
const bool option_norm[OPT_MAX] =
{
	FALSE,		/* OPT_hjkl_movement */
	TRUE,		/* OPT_quick_messages */
	FALSE,		/* OPT_angband_keyset */
	FALSE,		/* xxx carry_query */
	TRUE,		/* OPT_stop_singing_on_rest */
	FALSE,		/* xxx always_pickup */
	TRUE,		/* OPT_forgo_attacking_unwary */
	FALSE,		/* xxx depth_in_feet */
	FALSE,		/* xxx stack_force_notes */
	FALSE,		/* xxx stack_force_costs */
	FALSE,		/* xxx show_labels */
	FALSE,		/* xxx show_weights */
	FALSE,		/* xxx show_choices */
	FALSE,		/* xxx show_details */
	TRUE,		/* OPT_system_beep */
	FALSE,		/* xxx show_flavors */
	FALSE,		/* xxx run_ignore_stairs */
	FALSE,		/* xxx run_ignore_doors */
	FALSE,		/* xxx run_cut_corners */
	FALSE,		/* xxx run_use_corners */
	FALSE,		/* xxx disturb_move */
	FALSE,		/* xxx disturb_near */
	FALSE,		/* xxx disturb_panel */
	FALSE,		/* xxx disturb_state */
	FALSE,		/* xxx disturb_minor */
	FALSE,		/* xxx disturb_wakeup */
	FALSE,		/* xxx alert_hitpoint */
	FALSE,		/* xxx alert_failure */
	FALSE,		/* xxx verify_destroy */
	FALSE,		/* xxx verify_special */
	FALSE,		/* xxx allow_quantity */
	FALSE,		/* xxx */
	FALSE,		/* xxx auto_haggle */
	FALSE,		/* xxx auto_scum */
	FALSE,		/* xxx allow_themed_levels */
	FALSE,		/* xxx */
	FALSE,		/* xxx expand_look */
	FALSE,		/* xxx expand_list */
	FALSE,		/* xxx view_perma_grids */
	FALSE,		/* xxx view_torch_grids */
	FALSE,		/* xxx dungeon_align */
	FALSE,		/* xxx dungeon_stair */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx track_follow */
	FALSE,		/* xxx track_target */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx view_reduce_lite */
	FALSE,		/* xxx hidden_player */
	FALSE,		/* xxx avoid_abort */
	FALSE,		/* xxx avoid_other */
	FALSE,		/* xxx flush_failure */
	FALSE,		/* xxx flush_disturb */
	FALSE,		/* xxx */
	FALSE,		/* xxx fresh_before */
	FALSE,		/* xxx fresh_after */
	FALSE,		/* xxx */
	FALSE,		/* xxx compress_savefile */
	FALSE,		/* OPT_hilite_player */
	TRUE,		/* OPT_hilite_target */
	TRUE,		/* OPT_hilite_unwary */
	TRUE,		/* OPT_solid_walls */
	FALSE,		/* OPT_hybrid_walls */
	FALSE,		/* xxx easy_open */
	FALSE,		/* xxx easy_alter */
	FALSE,		/* xxx easy_floor */
	FALSE,		/* OPT_instant_run */
	FALSE,		/* OPT_center_player */
	FALSE,		/* OPT_run_avoid_center */
	FALSE,		/* xxx scroll_target */
	FALSE,		/* OPT_auto_more */
	FALSE,		/* xxx toggle_xp */
	FALSE,		/* OPT_auto_display_lists */
	TRUE,		/* OPT_easy_main_menu */
	FALSE,		/* xxx verify_quest_leave */
	FALSE,		/* xxx mark_squelch_items */
	TRUE,		/* OPT_display_hits */
	FALSE,		/* OPT_display_wakings */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx birth_point_based */
	FALSE,		/* xxx birth_auto_roller */
	FALSE,		/* xxx birth_maximize */
	FALSE,		/* OPT_birth_discon_stair */
	FALSE,		/* OPT_birth_ironman */
	FALSE,		/* xxx birth_no_stores */
	FALSE,		/* OPT_birth_no_artefacts */
	FALSE,		/* xxx birth_rand_artefacts */
	FALSE,		/* xxx birth_no_stacking */
	FALSE,		/* xxx birth_take_notes */
	FALSE,		/* xxx birth_force_small_lev */
	FALSE,		/* xxx birth_retain_squelch */
	FALSE,		/* xxx OPT_birth_no_quests*/
	FALSE,		/* xxx birth_no_player ghosts*/
	FALSE,		/* xxx birth_no_store_services*/
	FALSE,		/* xxx birth_no_xtra_artefacts*/
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* OPT_cheat_peek */
	FALSE,		/* OPT_cheat_hear */
	FALSE,		/* OPT_cheat_room */
	FALSE,		/* OPT_cheat_xtra */
	FALSE,		/* OPT_cheat_know */
	FALSE,		/* OPT_cheat_live */
	FALSE,		/* OPT_cheat_monsters */
	FALSE,		/* OPT_cheat_noise */
	FALSE,		/* OPT_cheat_scent */
	FALSE,		/* OPT_cheat_light */
	FALSE,		/* OPT_cheat_skill_rolls */
	FALSE,		/* OPT_cheat_timestop */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx adult_point_based */
	FALSE,		/* xxx adult_auto_roller */
	FALSE,		/* xxx adult_maximize */
	FALSE,		/* OPT_adult_discon_stair */
	FALSE,		/* OPT_adult_ironman */
	FALSE,		/* xxx adult_no_stores */
	FALSE,		/* OPT_adult_no_artefacts */
	FALSE,		/* xxx adult_rand_artefacts */
	FALSE,		/* xxx adult_no_stacking */
	FALSE,		/* xxx adult_take_notes */
	FALSE,		/* xxx adult_force_small_lev*/
	FALSE,		/* xxx adult_retain_squelch */
	FALSE,		/* xxx OPT_adult_no_quests */
	FALSE,		/* xxx adult_no_player ghosts */
	FALSE,		/* xxx adult_no_store_services */
	FALSE,		/* xxx adult_no_xtra_artefacts */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* OPT_score_peek */
	FALSE,		/* OPT_score_hear */
	FALSE,		/* OPT_score_room */
	FALSE,		/* OPT_score_xtra */
	FALSE,		/* OPT_score_know */
	FALSE,		/* OPT_score_live */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE,		/* xxx */
	FALSE		/* xxx */
};


/*
 * Option screen interface
 */
const byte option_page[OPT_PAGE_MAX][OPT_PAGE_PER] =
{
	/*** User-Interface ***/

	{
		OPT_forgo_attacking_unwary,
		OPT_stop_singing_on_rest,
		OPT_system_beep,
		OPT_quick_messages,
		OPT_auto_more,
		OPT_easy_main_menu,
		OPT_hjkl_movement,
		OPT_angband_keyset,
		OPT_hitpoint_warning,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	},

	/*** Disturbance ***/

	{
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	},

	/*** Game-Play ***/

	{
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	},

	/*** Efficiency ***/

	{
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	},

	/*** Display ***/

	{
		OPT_display_hits,
		OPT_auto_display_lists,
		OPT_instant_run,
 		OPT_center_player,
 		OPT_run_avoid_center,
		OPT_hilite_player,
		OPT_hilite_target,
		OPT_hilite_unwary,
		OPT_solid_walls,
		OPT_hybrid_walls,
		OPT_delay_factor,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	},

	/*** Birth ***/

	{
		OPT_birth_discon_stair,
		OPT_birth_ironman,
		OPT_birth_no_artefacts,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	},

	/*** Cheat ***/

	{
		OPT_cheat_peek,
		OPT_cheat_hear,
		OPT_cheat_room,
		OPT_cheat_xtra,
		OPT_cheat_know,
		OPT_cheat_live,
		OPT_cheat_monsters,
		OPT_cheat_noise,
		OPT_cheat_scent,
		OPT_cheat_light,
		OPT_cheat_skill_rolls,
		OPT_cheat_timestop,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE,
		OPT_NONE
	}
};


cptr inscrip_text[MAX_INSCRIP] =
{
	NULL,
	"artefact, cursed",			// old: terrible
	"special, cursed",			// old: worthless
	"cursed",
	"broken",
	"average",			// old: average
	"fine",				// old: good
	"fine",				// old: good
	"special",			// old: excellent
	"artefact",			// old: special
	"uncursed",
	"indestructible"
};



/*
 * First column is Mana Cost
 * Second column is number of sides of damage
 * Third column is Optimal Ranges for various spells.
 *   - the degree of preference for a range is given in the next table
 */

// {Mana_cost,  dam_sides,  best_range}
byte spell_info_RF4[32][3]=
{
	{             0,     7,     4},        /* RF4_ARROW1 */
	{             0,     7,     4},        /* RF4_ARROW2 */
	{             0,     4,     4},        /* RF4_BOULDER */
	{ MON_MANA_COST,     4,     2},        /* RF4_BRTH_FIRE */
	{ MON_MANA_COST,     4,     2},        /* RF4_BRTH_COLD */
	{ MON_MANA_COST,     4,     2},        /* RF4_BRTH_POIS */
	{ MON_MANA_COST,     4,     2},        /* RF4_BRTH_DARK */
	{ MON_MANA_COST,     4,     2},        /* RF4_EARTHQUAKE */
	{ MON_MANA_COST,     0,     0},        /* RF4_SHRIEK */
	{             0,     1,     2},        /* RF4_SCREECH */
	{ MON_MANA_COST,     0,     0},        /* RF4_DARKNESS */
	{ MON_MANA_COST,     0,     0},        /* RF4_FORGET */
	{ MON_MANA_COST,     0,     0},        /* RF4_SCARE */
	{ MON_MANA_COST,     0,     0},        /* RF4_CONF */
	{ MON_MANA_COST,     0,     0},        /* RF4_HOLD */
	{ MON_MANA_COST,     0,     0},        /* RF4_SLOW */
	
	{ MON_MANA_COST,     0,     0},        /* RF4_SNG_BINDING */
	{ MON_MANA_COST,     0,     0},        /* RF4_SNG_PIERCING */
    { MON_MANA_COST,     0,     0},        /* RF4_SNG_OATHS */
    
	{             0,     0,     0},        /* RF4_XXX20 */
	{             0,     0,     0},        /* RF4_XXX21 */
	{             0,     0,     0},        /* RF4_XXX22 */
	{             0,     0,     0},        /* RF4_XXX23 */
	{             0,     0,     0},        /* RF4_XXX24 */
	{             0,     0,     0},        /* RF4_XXX25 */
	{             0,     0,     0},        /* RF4_XXX26 */
	{             0,     0,     0},        /* RF4_XXX27 */
	{             0,     0,     0},        /* RF4_XXX28 */
	{             0,     0,     0},        /* RF4_XXX29 */
	{             0,     0,     0},        /* RF4_XXX30 */
	{             0,     0,     0},        /* RF4_XXX31 */
	{             0,     0,     0}         /* RF4_XXX32 */
};	


/*
 * desirability:			base desirability for AI.
 * usefulness past range:   % of spell desirability retained for each step past 'range'
 */

byte spell_desire_RF4[32][2] =
{
/*  { desirability,  usefulness past range }  */
	{ 100,  100}, /* RF4_ARROW1	    */
	{ 100,  100}, /* RF4_ARROW2	    */
	{  50,  100}, /* RF4_BOULDER   */
	{ 100,   50}, /* RF4_BRTH_FIRE */
	{ 100,   50}, /* RF4_BRTH_COLD */
	{ 100,   50}, /* RF4_BRTH_POIS */
	{ 100,   50}, /* RF4_BRTH_DARK */
	{  50,    0}, /* RF4_EARTHQUAKE   */
	{  50,  100}, /* RF4_SHRIEK    */
	{  50,  100}, /* RF4_SCREECH    */
	{  50,  100}, /* RF4_DARKNESS  */
	{  50,  100}, /* RF4_FORGET    */
	{  50,  100}, /* RF4_SCARE	 */
	{  50,  100}, /* RF4_CONF	  */
	{  50,  100}, /* RF4_HOLD	*/
	{  50,  100}, /* RF4_SLOW	*/
	
	{  50,  100}, /* RF4_SNG_BINDING */
	{   0,    0}, /* RF4_SNG_PIERCING */
	{  50,  100}, /* RF4_SNG_OATHS */
    
	{ 0,   100}, /* RF4_XXX20 */
	{ 0,   100}, /* RF4_XXX21 */
	{ 0,   100}, /* RF4_XXX22 */
	{ 0,   100}, /* RF4_XXX23 */
	{ 0,   100}, /* RF4_XXX24 */
	{ 0,   100}, /* RF4_XXX25 */
	{ 0,   100}, /* RF4_XXX26 */
	{ 0,   100}, /* RF4_XXX27 */
	{ 0,   100}, /* RF4_XXX28 */
	{ 0,   100}, /* RF4_XXX29 */
	{ 0,   100}, /* RF4_XXX30 */
	{ 0,   100}, /* RF4_XXX31 */
	{ 0,   100}  /* RF4_XXX32 */
};

