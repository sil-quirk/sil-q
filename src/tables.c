/* File: tables.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"

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
    (CHEST_NEEDLE_LOSE_STR), (CHEST_NEEDLE_LOSE_STR), (CHEST_FLAME),
    (CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
    (CHEST_GAS_CONF | CHEST_NEEDLE_HALLU),
    (CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR), (CHEST_FLAME),
    (CHEST_GAS_STUN | CHEST_NEEDLE_LOSE_STR),
    (CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE),
    (CHEST_GAS_POISON | CHEST_NEEDLE_ENTRANCE), (CHEST_FLAME),
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
    "Display overhead view", "Display monster list", "Display supplies", NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL };

/*
 * Options -- textual names (where defined)
 */
cptr option_text[OPT_MAX] = {
    "hjkl_movement", /* OPT_hjkl_movement */
    NULL, /* obsolete 0.9.7: quick_messages */
    "angband_keyset", /* OPT_angband_keyset */
    NULL, /* reserved legacy slot: carry_query */
    "stop_singing_on_rest", /* OPT_stop_singing_on_rest */
    NULL, /* reserved legacy slot: always_pickup */
    "forgo_attacking_unwary", /* OPT_forgo_attacking_unwary */
    NULL, /* reserved legacy slot: depth_in_feet */
    NULL, /* reserved legacy slot: stack_force_notes */
    NULL, /* reserved legacy slot: stack_force_costs */
    NULL, /* reserved legacy slot: show_labels */
    NULL, /* reserved legacy slot: show_weights */
    NULL, /* obsolete 0.9.7: main_combat_rolls */
    NULL, /* reserved legacy slot: show_details */
    NULL, /* obsolete 0.9.7: system_beep */
    NULL, /* reserved legacy slot: flavors */
    NULL, /* reserved legacy slot: run_ignore_stairs */
    NULL, /* reserved legacy slot: run_ignore_doors */
    NULL, /* reserved legacy slot: run_cut_corners */
    NULL, /* reserved legacy slot: run_use_corners */
    NULL, /* reserved legacy slot: disturb_move */
    NULL, /* reserved legacy slot: disturb_near */
    NULL, /* reserved legacy slot: disturb_panel */
    NULL, /* reserved legacy slot: disturb_state */
    NULL, /* reserved legacy slot: disturb_minor */
    NULL, /* reserved legacy slot: disturb_wakeup */
    NULL, /* reserved legacy slot: alert_hitpoint */
    NULL, /* reserved legacy slot: alert_failure */
    NULL, /* reserved legacy slot: verify_destroy */
    NULL, /* reserved legacy slot: verify_special */
    NULL, /* reserved legacy slot: allow_quantity */
    NULL, /* reserved legacy slot */
    "valorous_oath_auto_attack_safety", /* OPT_valorous_oath_auto_attack_safety */
    "visual_recognition", /* OPT_visual_recognition */
    "stealth_vision", /* OPT_stealth_vision */
    "sleep_icon", /* OPT_sleep_icon */
    "assassination_over_charge", /* OPT_assassination_over_charge */
    "pacifist_attack_warning", /* OPT_pacifist_attack_warning */
    "active_weapon_switch_confirm", /* OPT_active_weapon_switch_confirm */
    NULL, /* reserved legacy slot: view_torch_grids */
    NULL, /* reserved legacy slot: dungeon_align */
    NULL, /* reserved legacy slot: dungeon_stair */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: track_follow */
    NULL, /* reserved legacy slot: track_target */
    NULL, /* reserved legacy slot: track_target */
    NULL, /* reserved legacy slot: smart_cheat */
    NULL, /* reserved legacy slot: view_reduce_lite */
    NULL, /* reserved legacy slot: hidden_player */
    NULL, /* reserved legacy slot: avoid_abort */
    NULL, /* reserved legacy slot: avoid_other */
    NULL, /* reserved legacy slot: flush_failure */
    NULL, /* reserved legacy slot: flush_disturb */
    NULL, /* reserved legacy slot: flush_command */
    NULL, /* reserved legacy slot: fresh_before */
    NULL, /* reserved legacy slot: fresh_after */
    NULL, /* reserved legacy slot: fresh_message */
    NULL, /* reserved legacy slot: compress_savefile */
    "hilite_player", /* OPT_hilite_player */
    "hilite_target", /* OPT_hilite_target */
    "hilite_unwary", /* OPT_hilite_unwary */
    "solid_walls", /* OPT_solid_walls */
    "hybrid_walls", /* OPT_hybrid_walls */
    NULL, /* reserved legacy slot: easy_open */
    NULL, /* reserved legacy slot: easy_alter */
    NULL, /* reserved legacy slot: easy_floor */
    "instant_run", /* OPT_running_delay; legacy preference key */
    "center_player", /* OPT_center_player */
    "run_avoid_center", /* OPT_run_avoid_center */
    NULL, /* reserved legacy slot: scroll_target */
    NULL, /* obsolete 0.9.7: auto_more */
    "know_monster_info", /* OPT_know_monster_info */
    NULL, /* reserved legacy slot: auto_display_lists */
    "artifact_unique_color", /* OPT_artifact_unique_color */
    NULL, /* obsolete: easy_main_menu (Esc always opens the main menu) */
    "story_lists", /* OPT_story_lists */
    "story_lists_inven", /* OPT_story_lists_inven */
    "story_lists_equip", /* OPT_story_lists_equip */
    "display_hits", /* OPT_display_hits */
    "story_character_sheet", /* OPT_story_character_sheet */
    "story_lists_inven_pane", /* OPT_story_lists_inven_pane */
    "story_lists_equip_pane", /* OPT_story_lists_equip_pane */
    "story_monster_desc", /* OPT_story_monster_desc */
    "story_monster_desc_pane", /* OPT_story_monster_desc_pane */
    "disable_skeleton_note_tutorial", /* OPT_disable_skeleton_note_tutorial */
    "smaller_level_size", /* OPT_smaller_level_size */
    "more_stairs", /* OPT_more_stairs */
    "unidentified_items_slate", /* OPT_unidentified_items_slate */
    NULL, /* obsolete 0.9.7: space_acts_as_comma */
    "level_entry_narrative_mode", /* OPT_show_level_entry_banner */
    NULL, /* reserved legacy slot: ability_desc_mode */
    "vault_drop_frequency", /* OPT_vault_drop_frequency */
    "show_smithing_difficulty", /* OPT_show_smithing_difficulty */
    "show_smithing_difficulty_look", /* OPT_show_smithing_difficulty_look */
    NULL, /* reserved legacy slot */
    "partition_narrative_mode", /* OPT_show_partition_narrative */
    "noble_item_spawn_mode", /* OPT_noble_item_spawn_mode */
    NULL, /* obsolete 0.9.7: hide_left_panel */
    NULL, /* reserved legacy slot: banner_message_stairs */
    "show_level_generation_debug", /* OPT_show_level_generation_debug */
    "unlock_blitz_mode", /* OPT_unlock_blitz_mode */
    "look_objects_sort_by_difficulty", /* OPT_look_objects_sort_by_difficulty */
    "look_nearby_filter_default", /* OPT_look_nearby_filter_default */
    "show_elemental_item_rolls", /* OPT_show_elemental_item_rolls */
    NULL, /* obsolete 0.9.7: hidden_left_panel_mode */
    NULL, /* obsolete 0.9.7: top_status_line */
    "hide_supporting_panes_fullscreen", /* OPT_hide_supporting_panes_fullscreen */
    "narrative_banner_turns", /* OPT_narrative_banner_turns */
    "min_depth_timer_mode", /* OPT_min_depth_timer_mode */
    "song_list_sort_by_recent", /* OPT_song_list_sort_by_recent */
    NULL, /* reserved legacy slot: inventory_selection_square */
    "supply_menu_random_icons", /* OPT_supply_menu_random_icons */
    "supply_menu_hide_flavor_compact", /* OPT_supply_menu_hide_flavor_compact */
    "load_blitz_by_default", /* OPT_load_blitz_by_default */
    "mirror_player_tile_facing", /* OPT_mirror_player_tile_facing */
    "handcrafted_player_tile_facing", /* OPT_handcrafted_player_tile_facing */
    "story_object_desc", /* OPT_story_object_desc */
    "hide_secondary_action_ring", /* OPT_hide_secondary_action_ring */
    "mirror_monster_tile_facing", /* OPT_mirror_monster_tile_facing */
    "styled_player_health_bar", /* OPT_styled_player_health_bar */
    "styled_monster_health_bars", /* OPT_styled_monster_health_bars */
    "styled_monster_tile_health_bars", /* OPT_styled_monster_tile_health_bars */
    "pixel_monster_status_icons", /* OPT_pixel_monster_status_icons */
    "lockpick_minigame", /* OPT_lockpick_minigame */
    "chest_trap_minigame", /* OPT_chest_trap_minigame */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: birth_point_based */
    NULL, /* reserved legacy slot: birth_auto_roller */
    NULL, /* reserved legacy slot: birth_maximize */
    "birth_discon_stair", /* OPT_birth_discon_stair */
    "birth_ironman", /* OPT_birth_ironman */
    NULL, "birth_no_artefacts", /* OPT_birth_no_artefacts */
    "birth_fixed_exp", /* OPT_birth_fixed_exp */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: birth_force_small_lev */
    NULL, /* reserved legacy slot: birth_retain_squelch */
    NULL, /* reserved legacy slot: birth_no_quests */
    NULL, /* reserved legacy slot: birth_no_player ghosts */
    NULL, /* reserved legacy slot: birth_no_store_services */
    NULL, /* reserved legacy slot: birth_no_xtra_artefacts */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
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
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: adult_point_based */
    NULL, /* reserved legacy slot: adult_auto_roller */
    NULL, /* reserved legacy slot: adult_maximize */
    "adult_discon_stair", /* OPT_adult_discon_stair */
    "adult_ironman", /* OPT_adult_ironman */
    NULL, /* reserved legacy slot: adult_no_stores */
    "adult_no_artefacts", /* OPT_adult_no_artefacts */
    NULL, /* reserved legacy slot: adult_rand_artefacts */
    NULL, /* reserved legacy slot: adult_no_stacking */
    NULL, /* reserved legacy slot: adult_auto_notes */
    NULL, /* reserved legacy slot: adult_force_small_lev */
    NULL, /* reserved legacy slot: adult_retain_squelch */
    NULL, /* reserved legacy slot: adult_no_quests */
    NULL, /* reserved legacy slot: adult_no_player ghosts */
    NULL, /* reserved legacy slot: adult_no_store_services */
    NULL, /* reserved legacy slot: adult_no_xtra_artefacts */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    "score_peek", /* OPT_score_peek */
    "score_hear", /* OPT_score_hear */
    "score_room", /* OPT_score_room */
    "score_xtra", /* OPT_score_xtra */
    "score_know", /* OPT_score_know */
    "score_live", /* OPT_score_live */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL /* reserved legacy slot */
};

/*
 * Options -- descriptions (where defined)
 */
cptr option_desc[OPT_MAX] = {
    "Move with hjkl etc. (use ^ for underlying keys)", /* OPT_hjkl_movement */
    NULL, /* obsolete 0.9.7: quick_messages */
    "Use a keyset more closely based on Angband", /* OPT_angband_keyset */
    NULL, /* reserved legacy slot: carry_query */
    "Stop singing when you use the rest command", /* OPT_stop_singing_on_rest */
    NULL, /* reserved legacy slot: always_pickup */
    "Forgo bonus attacks on non-alert enemies", /* OPT_forgo_attacking_unwary */
    NULL, /* reserved legacy slot: depth_in_feet */
    NULL, /* reserved legacy slot: stack_force_notes */
    NULL, /* reserved legacy slot: stack_force_costs */
    NULL, /* reserved legacy slot: show_labels */
    NULL, /* reserved legacy slot: show_weights */
    NULL, /* obsolete 0.9.7: main_combat_rolls */
    NULL, /* reserved legacy slot: show_details */
    NULL, /* obsolete 0.9.7: system_beep */
    NULL, /* reserved legacy slot: show_flavors */
    NULL, /* reserved legacy slot: run_ignore_stairs */
    NULL, /* reserved legacy slot: run_ignore_doors */
    NULL, /* reserved legacy slot: run_cut_corners */
    NULL, /* reserved legacy slot: run_use_corners */
    NULL, /* reserved legacy slot: disturb_move */
    NULL, /* reserved legacy slot: disturb_near */
    NULL, /* reserved legacy slot: disturb_panel */
    NULL, /* reserved legacy slot: disturb_state */
    NULL, /* reserved legacy slot: disturb_minor */
    NULL, /* reserved legacy slot: disturb_wakeup */
    NULL, /* reserved legacy slot: alert_hitpoint */
    NULL, /* reserved legacy slot: alert_failure */
    NULL, /* reserved legacy slot: verify_destroy */
    NULL, /* reserved legacy slot: verify_special */
    NULL, /* reserved legacy slot: allow_quantity */
    NULL, /* reserved legacy slot */
    "Disable automatic attacks of fleeing enemies under Oath of Valor", /* OPT_valorous_oath_auto_attack_safety */
    "Smart monsters need light to visually recognize you", /* OPT_visual_recognition */
    "Stealth vision mode: show when monsters can see you", /* OPT_stealth_vision */
    "Show an overlay icon on sleeping monsters", /* OPT_sleep_icon */
    "On unaware targets, use Assassination instead of Charge bonuses", /* OPT_assassination_over_charge */
    "Warn before making direct attacks (useful for pacifist runs)", /* OPT_pacifist_attack_warning */
    "Confirm before switching between melee and ranged weapons", /* OPT_active_weapon_switch_confirm */
    NULL, /* reserved legacy slot: view_torch_grids */
    NULL, /* reserved legacy slot: dungeon_align */
    NULL, /* reserved legacy slot: dungeon_stair */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: track_follow */
    NULL, /* reserved legacy slot: track_target */
    NULL, /* reserved legacy slot: track_target */
    NULL, /* reserved legacy slot: smart_cheat */
    NULL, /* reserved legacy slot: view_reduce_lite */
    NULL, /* reserved legacy slot: hidden_player */
    NULL, /* reserved legacy slot: avoid_abort */
    NULL, /* reserved legacy slot: avoid_other */
    NULL, /* reserved legacy slot: flush_failure */
    NULL, /* reserved legacy slot: flush_disturb */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: fresh_before */
    NULL, /* reserved legacy slot: fresh_after */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: compress_savefile */
    "Highlight the player with the cursor", /* OPT_hilite_player */
    "Highlight the target with the cursor", /* OPT_hilite_target */
    "Highlight sleeping and unwary creatures", /* OPT_hilite_unwary */
    "Display walls as solid blocks", /* OPT_solid_walls */
    "Display walls as semi-solid", /* OPT_hybrid_walls */
    NULL, /* reserved legacy slot: easy_open */
    NULL, /* reserved legacy slot: easy_alter */
    NULL, /* reserved legacy slot: easy_floor */
    "Running delay in milliseconds", /* OPT_running_delay */
    "Center map continuously", /* OPT_center_player */
    "Avoid centering while running", /* OPT_run_avoid_center */
    NULL, /* reserved legacy slot: scroll_target */
    NULL, /* obsolete 0.9.7: auto_more */
    "Know all monster info", /* OPT_know_monster_info */
    NULL, /* reserved legacy slot: auto_display_lists */
    "Display artifacts in unique yellow color", /* OPT_artifact_unique_color */
    NULL, /* obsolete: easy_main_menu (Esc always opens the main menu) */
    "Render look/target lists with the story font", /* OPT_story_lists */
    "Render the inventory menu in the main window with the story font", /* OPT_story_lists_inven */
    "Render the equipment menu in the main window with the story font", /* OPT_story_lists_equip */
    "Display a mark when something gets hit", /* OPT_display_hits */
    "Render the character sheet with the story font", /* OPT_story_character_sheet */
    "Render the inventory pane with the story font", /* OPT_story_lists_inven_pane */
    "Render the equipment pane with the story font", /* OPT_story_lists_equip_pane */
    "Render monster description overlays with the story font", /* OPT_story_monster_desc */
    "Render monster descriptions in the monster pane with the story font", /* OPT_story_monster_desc_pane */
    "Disable tutorial messages in skeleton notes", /* OPT_disable_skeleton_note_tutorial */
    "Smaller level size (3 blocks smaller, min 6)", /* OPT_smaller_level_size */
    "More stairs (50% more; double max)", /* OPT_more_stairs */
    "Show unidentified items in slate color", /* OPT_unidentified_items_slate */
    NULL, /* obsolete 0.9.7: space_acts_as_comma */
    "Level entry narrative (banner with animation/banner without animation/message/off)", /* OPT_show_level_entry_banner */
    NULL, /* reserved legacy slot: ability_desc_mode */
    "Vault drop frequency (0=Normal, 1=Modest, 2=Scarce, 3=Meager, 4=Plentiful)", /* OPT_vault_drop_frequency */
    "Debug: Show {sd,wr} in item descriptions", /* OPT_show_smithing_difficulty */
    "Debug: Show {sd,wr} in look (L) sidebar and message", /* OPT_show_smithing_difficulty_look */
    "Welcome screen (0-6=fixed, 7=random)", /* OPT_intro_style */
    "Partition transition narrative (banner with animation/banner without animation/message/off)", /* OPT_show_partition_narrative */
    "Noble item spawns (0=good+/chests/human+elf skeletons, 1=also &/! vault drops)", /* OPT_noble_item_spawn_mode */
    NULL, /* obsolete 0.9.7: hide_left_panel */
    NULL, /* reserved legacy slot: banner_message_stairs */
    "Debug: Show detailed level-generation screen info and pause before play", /* OPT_show_level_generation_debug */
    "Unlock Blitz Mode after winning a tale", /* OPT_unlock_blitz_mode */
    "Sort look (L) objects by difficulty only (off = category, then difficulty)", /* OPT_look_objects_sort_by_difficulty */
    "Start look (l) with the nearby-only sidebar filter enabled", /* OPT_look_nearby_filter_default */
    "Debug: Show elemental item break rolls and target probabilities", /* OPT_show_elemental_item_rolls */
    NULL, /* obsolete 0.9.7: hidden_left_panel_mode */
    NULL, /* obsolete 0.9.7: top_status_line */
    "Hide supporting panes on full-screen menus when that frees space", /* OPT_hide_supporting_panes_fullscreen */
    "Narrative banner turns (0=dismiss banner on next input, 1-3=keep it visible for player turns)", /* OPT_narrative_banner_turns */
    "Minimum depth pace (0=normal, 1=relaxed [+30000], 2=harsh [-30000])", /* OPT_min_depth_timer_mode */
    "Sort the song menu by the songs most recently used this session", /* OPT_song_list_sort_by_recent */
    NULL, /* reserved legacy slot: inventory_selection_square */
    "Use random representative icons for supply groups", /* OPT_supply_menu_random_icons */
    "Hide flavor words in the compact supply list", /* OPT_supply_menu_hide_flavor_compact */
    "Load a living Blitz character by default when one exists", /* OPT_load_blitz_by_default */
    "Directional character animation", /* OPT_mirror_player_tile_facing */
    "Use handcrafted right-facing player tiles", /* OPT_handcrafted_player_tile_facing */
    "Render object description overlays with the story font", /* OPT_story_object_desc */
    "Hide the action wheel's secondary ring until its sector is hovered", /* OPT_hide_secondary_action_ring */
    "Apply directional or per-race random monster tile facing", /* OPT_mirror_monster_tile_facing */
    "Render the left-panel player health meter as a styled bar", /* OPT_styled_player_health_bar */
    "Render styled monster health bars in panes and overlays", /* OPT_styled_monster_health_bars */
    "Monster tile health bars (show/only damaged/off)", /* OPT_styled_monster_tile_health_bars */
    "Render monster sleep, sight, and alert indicators with SDL pixel overlays", /* OPT_pixel_monster_status_icons */
    "Use the lockpick and bash minigame for locked doors", /* OPT_lockpick_minigame */
    "Use the trap-search and disarm minigame for chests", /* OPT_chest_trap_minigame */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: birth_point_based */
    NULL, /* reserved legacy slot: birth_auto_roller */
    NULL, /* reserved legacy slot: birth_maximize */
    "Disconnected stairs", /* OPT_birth_discon_stair */
    "Straight down (no up stairs until endgame)", /* OPT_birth_ironman */
    NULL, "No artefacts", /* OPT_birth_no_artefacts */
    "Fixed XP - gain 50K at start and nothing after", /* OPT_birth_fixed_exp */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: birth_take_notes */
    NULL, /* reserved legacy slot: birth_force_small_lev */
    NULL, /* reserved legacy slot: birth_retain_squelch */
    NULL, /* reserved legacy slot: birth_no_quests */
    NULL, /* reserved legacy slot: birth_no_player ghosts */
    NULL, /* reserved legacy slot: birth_no_store_services */
    NULL, /* reserved legacy slot: birth_no_xtra_artefacts */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
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
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot: adult_point_based */
    NULL, /* reserved legacy slot: adult_auto_roller */
    NULL, /* reserved legacy slot: adult_maximize */
    "Disconnected stairs", /* OPT_adult_discon_stair */
    "Straight down (no up stairs until endgame)", /* OPT_adult_ironman */
    NULL, /* reserved legacy slot: adult_no_stores */
    "No artefacts", /* OPT_adult_no_artefacts */
    NULL, /* reserved legacy slot: adult_rand_artefacts */
    NULL, /* reserved legacy slot: adult_no_stacking */
    NULL, /* reserved legacy slot: adult_take_notes */
    NULL, /* reserved legacy slot: adult_force_small_lev */
    NULL, /* reserved legacy slot: adult_retain_squelch */
    NULL, /* reserved legacy slot: adult_no_quests */
    NULL, /* reserved legacy slot: adult_no_player ghosts */
    NULL, /* reserved legacy slot: adult_no_store_services */
    NULL, /* reserved legacy slot: adult_no_xtra_artefacts */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    "Score: Peek into object creation", /* OPT_score_peek */
    "Score: Peek into monster creation", /* OPT_score_hear */
    "Score: Peek into dungeon creation", /* OPT_score_room */
    "Score: Peek into something else", /* OPT_score_xtra */
    "Score: Know complete monster info", /* OPT_score_know */
    "Score: Allow player to avoid death", /* OPT_score_live */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL, /* reserved legacy slot */
    NULL /* reserved legacy slot */
};

/*
 * Options -- normal values
 */
const bool option_norm[OPT_MAX] = {
    false, /* OPT_hjkl_movement */
    false, /* obsolete 0.9.7: quick_messages */
    false, /* OPT_angband_keyset */
    false, /* reserved legacy slot: carry_query */
    true, /* OPT_stop_singing_on_rest */
    false, /* reserved legacy slot: always_pickup */
    true, /* OPT_forgo_attacking_unwary */
    false, /* reserved legacy slot: depth_in_feet */
    false, /* reserved legacy slot: stack_force_notes */
    false, /* reserved legacy slot: stack_force_costs */
    false, /* reserved legacy slot: show_labels */
    false, /* reserved legacy slot: show_weights */
    false, /* obsolete 0.9.7: main_combat_rolls */
    false, /* reserved legacy slot: show_details */
    false, /* obsolete 0.9.7: system_beep */
    false, /* reserved legacy slot: show_flavors */
    false, /* reserved legacy slot: run_ignore_stairs */
    false, /* reserved legacy slot: run_ignore_doors */
    false, /* reserved legacy slot: run_cut_corners */
    false, /* reserved legacy slot: run_use_corners */
    false, /* reserved legacy slot: disturb_move */
    false, /* reserved legacy slot: disturb_near */
    false, /* reserved legacy slot: disturb_panel */
    false, /* reserved legacy slot: disturb_state */
    false, /* reserved legacy slot: disturb_minor */
    false, /* reserved legacy slot: disturb_wakeup */
    false, /* reserved legacy slot: alert_hitpoint */
    false, /* reserved legacy slot: alert_failure */
    false, /* reserved legacy slot: verify_destroy */
    false, /* reserved legacy slot: verify_special */
    false, /* reserved legacy slot: allow_quantity */
    false, /* reserved legacy slot */
    true, /* OPT_valorous_oath_auto_attack_safety */
    true, /* OPT_visual_recognition */
    true, /* OPT_stealth_vision */
    true, /* OPT_sleep_icon */
    false, /* OPT_assassination_over_charge */
    false, /* OPT_pacifist_attack_warning */
    true, /* OPT_active_weapon_switch_confirm */
    false, /* reserved legacy slot: view_torch_grids */
    false, /* reserved legacy slot: dungeon_align */
    false, /* reserved legacy slot: dungeon_stair */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: track_follow */
    false, /* reserved legacy slot: track_target */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: view_reduce_lite */
    false, /* reserved legacy slot: hidden_player */
    false, /* reserved legacy slot: avoid_abort */
    false, /* reserved legacy slot: avoid_other */
    false, /* reserved legacy slot: flush_failure */
    false, /* reserved legacy slot: flush_disturb */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: fresh_before */
    false, /* reserved legacy slot: fresh_after */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: compress_savefile */
    false, /* OPT_hilite_player */
    false, /* OPT_hilite_target */
    false, /* OPT_hilite_unwary */
    true, /* OPT_solid_walls */
    false, /* OPT_hybrid_walls */
    false, /* reserved legacy slot: easy_open */
    false, /* reserved legacy slot: easy_alter */
    false, /* reserved legacy slot: easy_floor */
    false, /* OPT_running_delay; value is stored in running_delay_ms */
    false, /* OPT_center_player */
    false, /* OPT_run_avoid_center */
    false, /* reserved legacy slot: scroll_target */
    false, /* obsolete 0.9.7: auto_more */
    false, /* OPT_know_monster_info */
    false, /* reserved legacy slot: auto_display_lists */
    true, /* OPT_artifact_unique_color */
    true, /* obsolete: easy_main_menu (Esc always opens the main menu) */
    false, /* OPT_story_lists */
    true, /* OPT_story_lists_inven */
    true, /* OPT_story_lists_equip */
    true, /* OPT_display_hits */
    true, /* OPT_story_character_sheet */
    true, /* OPT_story_lists_inven_pane */
    true, /* OPT_story_lists_equip_pane */
    false, /* OPT_story_monster_desc */
    false, /* OPT_story_monster_desc_pane */
    false, /* OPT_disable_skeleton_note_tutorial */
    false, /* OPT_smaller_level_size */
    false, /* OPT_more_stairs */
    true, /* OPT_unidentified_items_slate */
    false, /* obsolete 0.9.7: space_acts_as_comma */
    true, /* OPT_show_level_entry_banner */
    false, /* reserved legacy slot: ability_desc_mode */
    false, /* OPT_vault_drop_frequency (default 0 via byte field) */
    false, /* OPT_show_smithing_difficulty */
    false, /* OPT_show_smithing_difficulty_look */
    false, /* reserved legacy slot */
    true, /* OPT_show_partition_narrative */
    false, /* OPT_noble_item_spawn_mode (default 0 via byte field) */
    false, /* obsolete 0.9.7: hide_left_panel */
    false, /* reserved legacy slot: banner_message_stairs */
    false, /* OPT_show_level_generation_debug */
    false, /* OPT_unlock_blitz_mode */
    false, /* OPT_look_objects_sort_by_difficulty */
    false, /* OPT_look_nearby_filter_default */
    false, /* OPT_show_elemental_item_rolls */
    false, /* obsolete 0.9.7: hidden_left_panel_mode */
    false, /* obsolete 0.9.7: top_status_line */
    true, /* OPT_hide_supporting_panes_fullscreen */
    false, /* OPT_narrative_banner_turns (default via byte field) */
    false, /* OPT_min_depth_timer_mode (default 0 via byte field) */
    true, /* OPT_song_list_sort_by_recent */
    false, /* reserved legacy slot: inventory_selection_square */
    false, /* OPT_supply_menu_random_icons */
    true, /* OPT_supply_menu_hide_flavor_compact */
    false, /* OPT_load_blitz_by_default */
    true, /* OPT_mirror_player_tile_facing */
    true, /* OPT_handcrafted_player_tile_facing */
    false, /* OPT_story_object_desc */
    true, /* OPT_hide_secondary_action_ring */
    true, /* OPT_mirror_monster_tile_facing */
    true, /* OPT_styled_player_health_bar */
    true, /* OPT_styled_monster_health_bars */
    true, /* OPT_styled_monster_tile_health_bars */
    true, /* OPT_pixel_monster_status_icons */
    true, /* OPT_lockpick_minigame */
    true, /* OPT_chest_trap_minigame */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: birth_point_based */
    false, /* reserved legacy slot: birth_auto_roller */
    false, /* reserved legacy slot: birth_maximize */
    false, /* OPT_birth_discon_stair */
    false, /* OPT_birth_ironman */
    false, /* reserved legacy slot */
    false, /* OPT_birth_no_artefacts */
    false, /* OPT_birth_fixed_exp */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: birth_force_small_lev */
    false, /* reserved legacy slot: birth_retain_squelch */
    false, /* reserved legacy slot: OPT_birth_no_quests */
    false, /* reserved legacy slot: birth_no_player ghosts */
    false, /* reserved legacy slot: birth_no_store_services */
    false, /* reserved legacy slot: birth_no_xtra_artefacts */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
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
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot: adult_point_based */
    false, /* reserved legacy slot: adult_auto_roller */
    false, /* reserved legacy slot: adult_maximize */
    false, /* OPT_adult_discon_stair */
    false, /* OPT_adult_ironman */
    false, /* reserved legacy slot: adult_no_stores */
    false, /* OPT_adult_no_artefacts */
    false, /* reserved legacy slot: adult_rand_artefacts */
    false, /* reserved legacy slot: adult_no_stacking */
    false, /* reserved legacy slot: adult_take_notes */
    false, /* reserved legacy slot: adult_force_small_lev */
    false, /* reserved legacy slot: adult_retain_squelch */
    false, /* reserved legacy slot: OPT_adult_no_quests */
    false, /* reserved legacy slot: adult_no_player ghosts */
    false, /* reserved legacy slot: adult_no_store_services */
    false, /* reserved legacy slot: adult_no_xtra_artefacts */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* OPT_score_peek */
    false, /* OPT_score_hear */
    false, /* OPT_score_room */
    false, /* OPT_score_xtra */
    false, /* OPT_score_know */
    false, /* OPT_score_live */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    false, /* reserved legacy slot */
    /* OPT_MAX sentinel uses zero-initialization */
};

/*
 * Option screen interface
 */
const byte option_page[OPT_PAGE_MAX][OPT_PAGE_PER] = {
    /*** User-Interface ***/

    { OPT_look_objects_sort_by_difficulty, OPT_look_nearby_filter_default,
        OPT_song_list_sort_by_recent, OPT_styled_player_health_bar,
        OPT_styled_monster_health_bars, OPT_styled_monster_tile_health_bars,
        OPT_hide_supporting_panes_fullscreen,
        OPT_hitpoint_warning,
        OPT_supply_menu_random_icons,
        OPT_supply_menu_hide_flavor_compact,
        OPT_hide_secondary_action_ring,
        OPT_show_level_generation_debug, OPT_show_elemental_item_rolls,
        OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Text options ***/

    { OPT_story_object_desc, OPT_story_monster_desc,
        OPT_story_monster_desc_pane, OPT_story_lists_inven_pane,
        OPT_story_lists_equip_pane,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Game-Play ***/

    { OPT_valorous_oath_auto_attack_safety, OPT_pacifist_attack_warning,
        OPT_active_weapon_switch_confirm,
        OPT_forgo_attacking_unwary, OPT_assassination_over_charge,
        OPT_lockpick_minigame, OPT_chest_trap_minigame,
        OPT_stop_singing_on_rest, OPT_visual_recognition, OPT_know_monster_info,
        OPT_disable_skeleton_note_tutorial, OPT_smaller_level_size, OPT_more_stairs,
        OPT_vault_drop_frequency, OPT_noble_item_spawn_mode,
        OPT_min_depth_timer_mode, OPT_load_blitz_by_default,
        OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Display ***/

    { OPT_stealth_vision, OPT_sleep_icon, OPT_pixel_monster_status_icons,
        OPT_artifact_unique_color, OPT_unidentified_items_slate,
        OPT_delay_factor, OPT_running_delay,
        OPT_mirror_player_tile_facing, OPT_mirror_monster_tile_facing,
        OPT_center_player,
        OPT_run_avoid_center, OPT_show_level_entry_banner,
        OPT_show_partition_narrative, OPT_narrative_banner_turns,
        OPT_intro_style, OPT_solid_walls, OPT_hybrid_walls,
        OPT_hilite_player, OPT_hilite_target, OPT_hilite_unwary,
        OPT_show_smithing_difficulty, OPT_show_smithing_difficulty_look,
        OPT_NONE, OPT_NONE },

    /*** Birth ***/

    { OPT_birth_discon_stair, OPT_birth_ironman, OPT_birth_no_artefacts,
        OPT_birth_fixed_exp, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Cheat ***/

    { OPT_cheat_peek, OPT_cheat_hear, OPT_cheat_room, OPT_cheat_xtra,
        OPT_cheat_know, OPT_cheat_monsters, OPT_cheat_noise,
        OPT_cheat_scent, OPT_cheat_light, OPT_cheat_skill_rolls,
        OPT_cheat_live, OPT_cheat_timestop, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE },

    /*** Sound ***/

    { OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE,
        OPT_NONE, OPT_NONE, OPT_NONE, OPT_NONE }
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
