/* File: variable.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * Hack -- Link a copyright message into the executable
 */
cptr copyright =
	"Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Keoneke\n"
	"\n"
	"This software may be copied and distributed for educational, research,\n"
	"and not for profit purposes provided that this copyright and statement\n"
	"are included in all such copies.  Other copyrights may also apply.\n";


/*
 * Executable version
 */
byte version_major = VERSION_MAJOR;
byte version_minor = VERSION_MINOR;
byte version_patch = VERSION_PATCH;
byte version_extra = VERSION_EXTRA;

/*
 * Savefile version
 */
byte sf_major;			/* Savefile's "version_major" */
byte sf_minor;			/* Savefile's "version_minor" */
byte sf_patch;			/* Savefile's "version_patch" */
byte sf_extra;			/* Savefile's "version_extra" */

/*
 * Savefile information
 */
u32b sf_xtra;			/* Operating system info */
u32b sf_when;			/* Time when savefile created */
u16b sf_lives;			/* Number of past "lives" with this file */
u16b sf_saves;			/* Number of "saves" during this life */

/*
 * Run-time arguments
 */
bool arg_fiddle;			/* Command arg -- Request fiddle mode */
bool arg_wizard;			/* Command arg -- Request wizard mode */
bool arg_sound;				/* Command arg -- Request special sounds */
bool arg_graphics;			/* Command arg -- Request graphics mode */
bool arg_force_original;	/* Command arg -- Request original keyset */
bool arg_force_roguelike;	/* Command arg -- Request roguelike keyset */

/*
 * Various things
 */

bool character_generated;	/* The character exists */
bool character_dungeon;		/* The character has a dungeon */
bool character_loaded;		/* The character was loaded from a savefile and is living */
bool character_loaded_dead;		/* The character was loaded from a savefile while dead */
bool character_saved;		/* The character was just saved to a savefile */

s16b character_icky;		/* Depth of the game in special mode */
s16b character_xtra;		/* Depth of the game in startup mode */

u32b seed_randart;		/* Hack -- consistent random artefacts */

u32b seed_flavor;		/* Hack -- consistent object colors */

s16b num_repro;			/* Current reproducer count */
s16b object_level;		/* Current object creation level */
s16b monster_level;		/* Current monster creation level */

char summon_kin_type;		/* Hack -- See summon_specific() */

s32b turn;				/* Current game turn */
s32b playerturn;		/* Current player turn */

bool do_feeling;			/* Hack -- Level feeling counter */

bool use_sound;			/* The "sound" mode is enabled */
int use_graphics;		/* The "graphics" mode is enabled */
bool use_bigtile = FALSE;

s16b image_count;  		/* Grids until next random image    */
                  		/* Optimizes the hallucination code */

s16b signal_count;		/* Hack -- Count interrupts */

bool msg_flag;			/* Player has pending message */

bool inkey_base;		/* See the "inkey()" function */
bool inkey_xtra;		/* See the "inkey()" function */
bool inkey_scan;		/* See the "inkey()" function */
bool inkey_flag;		/* See the "inkey()" function */
bool hide_cursor;		/* See the "inkey()" function */

byte object_generation_mode;/* Hack -- use different depth check, prevent embedded chests */

bool shimmer_monsters;	/* Hack -- optimize multi-hued monsters */
bool shimmer_objects;	/* Hack -- optimize multi-hued objects */
bool repair_mflag_show;	/* Hack -- repair monster flags (show) */
bool repair_mflag_mark;	/* Hack -- repair monster flags (mark) */

s16b o_max = 1;			/* Number of allocated objects */
s16b o_cnt = 0;			/* Number of live objects */

s16b mon_max = 1;	/* Number of allocated monsters */
s16b mon_cnt = 0;	/* Number of live monsters */


/*
 *  Most of the extra Sil variables...
 */

bool waiting_for_command = FALSE; // whether we are currently waiting for a command

bool skill_gain_in_progress = FALSE; // whether we are currently in the skill-gain screen

bool save_game_quietly = FALSE; // whether we are currently trying to save the game without displaying a message

bool stop_stealth_mode = FALSE; // whether there has been a signal that we need to abort stealth mode

char mini_screenshot_char[7][7];  // Characters in a mini-screenshot array
byte mini_screenshot_attr[7][7];  // Colours in a mini-screenshot array

bool use_background_colors = FALSE;



/*
 * TRUE if process_command() is a repeated call.
 */
bool command_repeating = FALSE;


/*
 * Dungeon variables
 */

byte feeling;			/* Most recent feeling */
s16b rating;			/* Level's current rating */

bool good_item_flag;	/* True if "Artefact" on this level */

bool closing_flag;		/* Dungeon is closing */


/*
 * Player info
 */
int player_uid;
int player_euid;
int player_egid;


/*
 * Buffer to hold the current savefile name
 */
char savefile[1024];


/*
 * Number of active macros.
 */
s16b macro__num;

/*
 * Array of macro patterns [MACRO_MAX]
 */
cptr *macro__pat;

/*
 * Array of macro actions [MACRO_MAX]
 */
cptr *macro__act;


/*
 * The array[ANGBAND_TERM_MAX] of window pointers
 */
term *angband_term[ANGBAND_TERM_MAX];


/*
 * The array[ANGBAND_TERM_MAX] of window names (modifiable?)
 *
 * ToDo: Make the names independent of ANGBAND_TERM_MAX.
 */
char angband_term_name[ANGBAND_TERM_MAX][16] =
{
	VERSION_NAME,
	"Inventory",
	"Equipment",
	"Combat Rolls",
	"Recall",
	"Character",
	"Messages",
	"Monster List"
};

int max_macrotrigger = 0;
cptr macro_template = NULL;
cptr macro_modifier_chr;
cptr macro_modifier_name[MAX_MACRO_MOD];
cptr macro_trigger_name[MAX_MACRO_TRIGGER];
cptr macro_trigger_keycode[2][MAX_MACRO_TRIGGER];



/*
 * Global table of color definitions (mostly zeros)
 */
byte angband_color_table[256][4] =
{
	{0x00, 0x00, 0x00, 0x00},	/* TERM_DARK */
	{0x00, 0xFF, 0xFF, 0xFF},	/* TERM_WHITE */
	{0x00, 0x80, 0x80, 0x80},	/* TERM_SLATE */
	{0x00, 0xFF, 0x80, 0x00},	/* TERM_ORANGE */
	{0x00, 0xC0, 0x00, 0x00},	/* TERM_RED */
	{0x00, 0x00, 0x80, 0x40},	/* TERM_GREEN */
	{0x00, 0x00, 0x40, 0xFF},	/* TERM_BLUE */
	{0x00, 0x80, 0x40, 0x00},	/* TERM_UMBER */
	{0x00, 0x50, 0x50, 0x50},	/* TERM_L_DARK */
	{0x00, 0xC0, 0xC0, 0xC0},	/* TERM_L_WHITE */
	{0x00, 0xA0, 0x00, 0xFF},	/* TERM_VIOLET */
	{0x00, 0xFF, 0xFF, 0x00},	/* TERM_YELLOW */
	{0x00, 0xFF, 0x60, 0x60},	/* TERM_L_RED */
	{0x00, 0x00, 0xFF, 0x00},	/* TERM_L_GREEN */
	{0x00, 0x00, 0xFF, 0xFF},	/* TERM_L_BLUE */
	{0x00, 0xC0, 0x80, 0x40},	/* TERM_L_UMBER */

	{0x00, 0x30, 0x30, 0x30},	/* TERM_DARK1 */
	{0x00, 0xC0, 0xC0, 0xC0},	/* TERM_WHITE1 */
	{0x00, 0xA0, 0xA0, 0xA0},	/* TERM_SLATE1 */
	{0x00, 0xDC, 0x64, 0x00},	/* TERM_ORANGE1 */
	{0x00, 0xF0, 0x00, 0x00},	/* TERM_RED1 */
	{0x00, 0x00, 0x70, 0x00},	/* TERM_GREEN1 */
	{0x00, 0x00, 0x80, 0xFF},	/* TERM_BLUE1 */
	{0x00, 0xC8, 0x64, 0x00},	/* TERM_UMBER1 */
	{0x00, 0x78, 0x64, 0x64},	/* TERM_L_DARK1 */
	{0x00, 0xE8, 0xD0, 0xC0},	/* TERM_L_WHITE1 */
	{0x00, 0x60, 0x00, 0xFF},	/* TERM_VIOLET1 */
	{0x00, 0xC8, 0xC8, 0x00},	/* TERM_YELLOW1 */
	{0x00, 0xB4, 0x46, 0x32},	/* TERM_L_RED1 */
	{0x00, 0x00, 0xDC, 0x64},	/* TERM_L_GREEN1 */
	{0x00, 0x64, 0xAA, 0xC8},	/* TERM_L_BLUE1 */
	{0x00, 0xC8, 0xAA, 0x46}	/* TERM_L_UMBER1 */	
};


/*
 * Standard sound (and message) names
 */
const cptr angband_sound_name[SOUND_MAX] =
{
	"",
	"hit",
	"miss",
	"flee",
	"drop",
	"kill",
	"level",
	"death",
	"study",
	"teleport",
	"shoot",
	"quaff",
	"zap",
	"walk",
	"tpother",
	"hitwall",
	"eat",
	"store1",
	"store2",
	"store3",
	"store4",
	"dig",
	"opendoor",
	"shutdoor",
	"tplevel",
	"bell",
	"nothing_to_open",
	"lockpick_fail",
	"stairs",
	"hitpoint_warn",
};


/*
 * Array[VIEW_MAX] used by "update_view()"
 */
int view_n = 0;
u16b *view_g;

/*
 * Arrays[TEMP_MAX] used for various things
 *
 * Note that temp_g shares memory with temp_x and temp_y.
 */
int temp_n = 0;
u16b *temp_g;
byte *temp_y;
byte *temp_x;


/*
 * Array[DUNGEON_HGT][256] of cave grid info flags (padded)
 *
 * This array is padded to a width of 256 to allow fast access to elements
 * in the array via "grid" values (see the GRID() macros).
 */
u16b (*cave_info)[256];

/*
 * Array[DUNGEON_HGT][DUNGEON_WID] of cave grid feature codes
 */
byte (*cave_feat)[MAX_DUNGEON_WID];

/*
 * Array[DUNGEON_HGT][DUNGEON_WID] of cave grid light level
 */
s16b (*cave_light)[MAX_DUNGEON_WID];


/*
 * Array[DUNGEON_HGT][DUNGEON_WID] of cave grid object indexes
 *
 * Note that this array yields the index of the top object in the stack of
 * objects in a given grid, using the "next_o_idx" field in that object to
 * indicate the next object in the stack, and so on, using zero to indicate
 * "nothing".  This array replicates the information contained in the object
 * list, for efficiency, providing extremely fast determination of whether
 * any object is in a grid, and relatively fast determination of which objects
 * are in a grid.
 */
s16b (*cave_o_idx)[MAX_DUNGEON_WID];

/*
 * Array[DUNGEON_HGT][DUNGEON_WID] of cave grid monster indexes
 *
 * Note that this array yields the index of the monster or player in a grid,
 * where negative numbers are used to represent the player, positive numbers
 * are used to represent a monster, and zero is used to indicate "nobody".
 * This array replicates the information contained in the monster list and
 * the player structure, but provides extremely fast determination of which,
 * if any, monster or player is in any given grid.
 */
s16b (*cave_m_idx)[MAX_DUNGEON_WID];

/*
 * Table of avergae monster power.
 * Used to help determine a suitable quest monster.
 */

u32b mon_power_ave[MAX_DEPTH][CREATURE_TYPE_MAX];


/*
 * Arrays[NUM_FLOWS][DUNGEON_HGT][DUNGEON_WID] of cave grid flow "cost" values
 */
byte cave_cost[MAX_FLOWS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

/*
 * Array[DUNGEON_HGT][DUNGEON_WID] of cave grid flow "when" stamps
 */
byte (*cave_when)[MAX_DUNGEON_WID];

/*
 * Current scent age marker.  Counts down from 250 to 0 and then loops.
 */
int scent_when = 250;


/*
 * Centerpoints of the last flow (noise) rebuild and the last flow update.
 */
byte flow_center_y[MAX_FLOWS];
byte flow_center_x[MAX_FLOWS];
byte update_center_y[MAX_FLOWS];
byte update_center_x[MAX_FLOWS];

/*
 * Wandering monsters will often pause at their destination for a while
 */
s16b wandering_pause[MAX_FLOWS];


/*
 * Represents the modified stealth_score for the player this round.
 */
s16b stealth_score = 0;

/*
 * Has the player attacked anyone this round? Has anyone attacked the player?
 */
bool player_attacked = FALSE;
bool attacked_player = FALSE;


/*
 * Array[z_info->o_max] of dungeon objects
 */
object_type *o_list;

/*
 * Array[MAX_MONSTERS] of dungeon monsters
 */
monster_type *mon_list;


/*
 * Array[z_info->r_max] of monster lore
 */
monster_lore *l_list;


/*
 * Array[INVEN_TOTAL] of objects in the player's inventory
 */
object_type *inventory;


/*
 * The size of "alloc_kind_table" (at most z_info->k_max * 4)
 */
s16b alloc_kind_size;

/*
 * The array[alloc_kind_size] of entries in the "kind allocator table"
 */
alloc_entry *alloc_kind_table;


/*
 * The size of the "alloc_ego_table"
 */
s16b alloc_ego_size;

/*
 * The array[alloc_ego_size] of entries in the "ego allocator table"
 */
alloc_entry *alloc_ego_table;


/*
 * The size of "alloc_race_table" (at most z_info->r_max)
 */
s16b alloc_race_size;

/*
 * The array[alloc_race_size] of entries in the "race allocator table"
 */
alloc_entry *alloc_race_table;


/*
 * Specify attr/char pairs for visual special effects
 * Be sure to use "index & 0xFF" to avoid illegal access
 */
byte misc_to_attr[256];
char misc_to_char[256];


/*
 * Specify color for inventory item text display (by tval)
 * Be sure to use "index & 0x7F" to avoid illegal access
 */
byte tval_to_attr[128];


/*
 * Current (or recent) macro action
 */
char macro_buffer[1024];


/*
 * Keymaps for each "mode" associated with each keypress.
 */
cptr keymap_act[KEYMAP_MODES][256];



/*** Player information ***/

/*
 * Pointer to the player tables (sex, race, house, magic)
 */
const player_sex *sp_ptr;
const player_race *rp_ptr;
player_house *hp_ptr;

/*
 * The player other record (static)
 */
static player_other player_other_body;

/*
 * Pointer to the player other record
 */
player_other *op_ptr = &player_other_body;

/*
 * The player info record (static)
 */
static player_type player_type_body;

/*
 * Pointer to the player info record
 */
player_type *p_ptr = &player_type_body;


/*
 * Structure (not array) of size limits
 */
maxima *z_info;

/*
 * The vault generation arrays
 */
vault_type *v_info;
char *v_name;
char *v_text;

/*
 * The terrain feature arrays
 */
feature_type *f_info;
char *f_name;
char *f_text;

/*
 * The object kind arrays
 */
object_kind *k_info;
char *k_name;
char *k_text;

/*
 * The ability arrays
 */
ability_type *b_info;
char *b_name;
char *b_text;

/*
 * The artefact arrays
 */
artefact_type *a_info;
char *a_text;

/*
 * The random name generator tables
 */
names_type *n_info;

/*
 * The special item arrays
 */
ego_item_type *e_info;
char *e_name;
char *e_text;


/*
 * The monster race arrays
 */
monster_race *r_info;
char *r_name;
char *r_text;


/*
 * The player race arrays
 */
player_race *p_info;
char *p_name;
char *p_text;

/*
 * The player house arrays
 */
player_house *c_info;
char *c_name;
char *c_text;

/*
 * The player history arrays
 */
hist_type *h_info;
char *h_text;

/*
 * The object flavor arrays
 */
flavor_type *flavor_info;
char *flavor_name;
char *flavor_text;


/*
 * The combat roll array for displaying past combat rolls
 */
combat_roll combat_rolls[2][MAX_COMBAT_ROLLS];
int combat_number;
int combat_number_old;
int turns_since_combat;
char combat_roll_special_char;
byte combat_roll_special_attr;

/*
 * Hacky variables for ignoring a square during project_path() function
 */
bool project_path_ignore;
int project_path_ignore_y;
int project_path_ignore_x;

/*
 * Hack -- The special Angband "System Suffix"
 * This variable is used to choose an appropriate "pref-xxx" file
 */
cptr ANGBAND_SYS = "xxx";

/*
 * Hack -- The special Angband "Graphics Suffix"
 * This variable is used to choose an appropriate "graf-xxx" file
 */
cptr ANGBAND_GRAF = "old";

/*
 * Path name: The main "lib" directory
 * This variable is not actually used anywhere in the code
 */
cptr ANGBAND_DIR;

/*
 * High score files (binary)
 * These files may be portable between platforms
 */
cptr ANGBAND_DIR_APEX;

/*
 * Bone files for player ghosts (ascii)
 * These files are portable between platforms
 */
cptr ANGBAND_DIR_BONE;

/*
 * Binary image files for the "*_info" arrays (binary)
 * These files are not portable between platforms
 */
cptr ANGBAND_DIR_DATA;

/*
 * Textual template files for the "*_info" arrays (ascii)
 * These files are portable between platforms
 */
cptr ANGBAND_DIR_EDIT;

/*
 * Various extra files (ascii)
 * These files may be portable between platforms
 */
cptr ANGBAND_DIR_FILE;

/*
 * Help files (normal) for the online help (ascii)
 * These files are portable between platforms
 */
cptr ANGBAND_DIR_HELP;

/*
 * Help files (spoilers) for the online help (ascii)
 * These files are portable between platforms
 */
cptr ANGBAND_DIR_INFO;

/*
 * Savefiles for current characters (binary)
 * These files are portable between platforms
 */
cptr ANGBAND_DIR_SAVE;

/*
 * Default user "preference" files (ascii)
 * These files are rarely portable between platforms
 */
cptr ANGBAND_DIR_PREF;

/*
 * User defined "preference" files (ascii)
 * These files are rarely portable between platforms
 */
cptr ANGBAND_DIR_USER;

/*
 * Various extra files (binary)
 * These files are rarely portable between platforms
 */
cptr ANGBAND_DIR_XTRA;

/*
 * Script files
 * These files are portable between platforms
 */
cptr ANGBAND_DIR_SCRIPT;


/*
 * Total Hack -- allow all items to be listed (even empty ones)
 * This is only used by "do_cmd_inven_e()" and is cleared there.
 */
bool item_tester_full;


/*
 * Here is a "pseudo-hook" used during calls to "get_item()" and
 * "show_inven()" and "show_equip()", and the choice window routines.
 */
byte item_tester_tval;


/*
 * Here is a "hook" used during calls to "get_item()" and
 * "show_inven()" and "show_equip()", and the choice window routines.
 */
bool (*item_tester_hook)(const object_type*);



/*
 * Current "comp" function for ang_sort()
 */
bool (*ang_sort_comp)(const void *u, const void *v, int a, int b);


/*
 * Current "swap" function for ang_sort()
 */
void (*ang_sort_swap)(void *u, void *v, int a, int b);



/*
 * Hack -- function hook to restrict "get_mon_num_prep()" function
 */
bool (*get_mon_num_hook)(int r_idx);



/*
 * Hack -- function hook to restrict "get_obj_num_prep()" function
 */
bool (*get_obj_num_hook)(int k_idx);


void (*object_info_out_flags)(const object_type *o_ptr, u32b *f1, u32b *f2, u32b *f3);


/*
 * Hack - the destination file for text_out_to_file.
 */
FILE *text_out_file = NULL;


/*
 * Hack -- function hook to output (colored) text to the
 * screen or to a file.
 */
void (*text_out_hook)(byte a, cptr str);


/*
 * Hack -- Where to wrap the text when using text_out().  Use the default
 * value (for example the screen width) when 'text_out_wrap' is 0.
 */
int text_out_wrap = 0;


/*
 * Hack -- Indentation for the text when using text_out().
 */
int text_out_indent = 0;


/*
 * Use transparent tiles
 */
bool use_transparency = FALSE;


/*
 * Buffer to hold the character's notes
 */
char notes_buffer[NOTES_LENGTH];


 /* Two variables that limit rogue stealing and creation of traps.
 * Cleared when a level is created. {From Oangband} -JG
 */
byte recent_failed_thefts;
byte num_trap_on_level;

/*occasionally allow chance of different inventory in a store*/
byte allow_altered_inventory;


autoinscription* inscriptions = 0;
u16b inscriptionsCount = 0;


/* The bones file a restored player ghost should use to collect extra
 * flags, a sex, and a unique name.  This also indicates that there is
 * a ghost active.  -LM-
 */
byte bones_selector;

/*
 * The player ghost template index. -LM-
 */
int r_ghost;

/*
 * The player ghost name is stored here for quick reference by the
 * description function.  -LM-
 */
char ghost_name[80];


/*
 * The type (if any) of the player ghost's personalized string, and
 * the string itself. -LM-
 */
int ghost_string_type = 0;
char ghost_string[80];

/*
 * The name of the current greater vault, if any. -DG-
 */
char g_vault_name[80];
