/* File: init1.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"


/*
 * This file is used to initialize various variables and arrays for the
 * Angband game.  Note the use of "fd_read()" and "fd_write()" to bypass
 * the common limitation of "read()" and "write()" to only 32767 bytes
 * at a time.
 *
 * Several of the arrays for Angband are built from "template" files in
 * the "lib/file" directory, from which quick-load binary "image" files
 * are constructed whenever they are not present in the "lib/data"
 * directory, or if those files become obsolete, if we are allowed.
 *
 * Warning -- the "ascii" file parsers use a minor hack to collect the
 * name and text information in a single pass.  Thus, the game will not
 * be able to load any template file with more than 20K of names or 60K
 * of text, even though technically, up to 64K should be legal.
 *
 * Note that if "ALLOW_TEMPLATES" is not defined, then a lot of the code
 * in this file is compiled out, and the game will not run unless valid
 * "binary template files" already exist in "lib/data".  Thus, one can
 * compile Angband with ALLOW_TEMPLATES defined, run once to create the
 * "*.raw" files in "lib/data", and then quit, and recompile without
 * defining ALLOW_TEMPLATES, which will both save 20K and prevent people
 * from changing the ascii template files in potentially dangerous ways.
 *
 * The code could actually be removed and placed into a "stand-alone"
 * program, but that feels a little silly, especially considering some
 * of the platforms that we currently support.
 */

#ifdef ALLOW_TEMPLATES


#include "init.h"


/*** Helper arrays for parsing ascii template files ***/

/*
 * Monster Blow Methods
 */
static cptr r_info_blow_method[] =
{
	"",
	"HIT",
	"TOUCH",
	"XXX",
	"XXX",
	"CLAW",
	"BITE",
	"STING",
	"PECK",
	"WHIP",
	"XXX",
	"CRUSH",
	"ENGULF",
	"CRAWL",
	"THORN",
	"XXX",
	"XXX",
	"XXX",
	"XXX",
	"SPORE",
	"XXX",
	"XXX",
	"XXX",
	"XXX",
	"XXX",
	NULL
};


/*
 * Monster Blow Effects
 */
static cptr r_info_blow_effect[] =
{
	"",
	"HURT",
	"WOUND",
	"BATTER",
	"SHATTER",
	"UN_BONUS",
	"UN_POWER",
	"LOSE_MANA",
	"SLOW",
	"EAT_ITEM",
	"EAT_FOOD",
	"DARK",
	"HUNGER",
	"POISON",
	"ACID",
	"ELEC",
	"FIRE",
	"COLD",
	"BLIND",
	"CONFUSE",
	"TERRIFY",
	"ENTRANCE",
	"HALLU",
	"DISEASE",
	"LOSE_STR",
	"LOSE_DEX",
	"LOSE_CON",
	"LOSE_GRA",
	"LOSE_STR_CON",
	"LOSE_ALL",
	"DISARM",
	NULL
};


typedef struct flag_name flag_name;


struct flag_name
{
	cptr name; /* The name of the flag in the text file. */
	int set; /* The set into which the flag is to be sent. */
	u32b flag; /* The flag being set. */
};


#define TR1 0
#define TR2 1
#define TR3 2
#define RF1 3
#define RF2 4
#define RF3 5
#define RF4 6
#define RHF 7
#define VLT 8
#define MAX_FLAG_SETS	9



/*
 * Monster race flags for the race_info_flags1 structure
 */
static flag_name info_flags[] =
{

/*
 * Monster race flags 2
 */

	{"UNIQUE", RF1, RF1_UNIQUE},
	{"QUESTOR", RF1, RF1_QUESTOR},
	{"MALE", RF1, RF1_MALE},
	{"FEMALE", RF1, RF1_FEMALE},
	{"CHAR_CLEAR", RF1, RF1_CHAR_CLEAR},
	{"RF1XXX1", RF1, RF1_RF1XXX1},
	{"ATTR_CLEAR", RF1, RF1_ATTR_CLEAR},
	{"ATTR_MULTI", RF1, RF1_ATTR_MULTI},
	{"FORCE_DEPTH", RF1, RF1_FORCE_DEPTH},
	{"SPECIAL_GEN", RF1, RF1_SPECIAL_GEN},
	{"FRIEND", RF1, RF1_FRIEND},
	{"FRIENDS", RF1, RF1_FRIENDS},
	{"ESCORT", RF1, RF1_ESCORT},
	{"ESCORTS", RF1, RF1_ESCORTS},
	{"UNIQUE_FRIEND", RF1, RF1_UNIQUE_FRIEND},
	{"NEVER_BLOW", RF1, RF1_NEVER_BLOW},
	{"NEVER_MOVE", RF1, RF1_NEVER_MOVE},
	{"HIDDEN_MOVE", RF1, RF1_HIDDEN_MOVE},
	{"RAND_25", RF1, RF1_RAND_25},
	{"RAND_50", RF1, RF1_RAND_50},
	{"NO_CRIT", RF1, RF1_NO_CRIT},
	{"RES_CRIT", RF1, RF1_RES_CRIT},
	{"DROP_33", RF1, RF1_DROP_33},
	{"DROP_100", RF1, RF1_DROP_100},
	{"DROP_1D2", RF1, RF1_DROP_1D2},
	{"DROP_2D2", RF1, RF1_DROP_2D2},
	{"DROP_3D2", RF1, RF1_DROP_3D2},
	{"DROP_4D2", RF1, RF1_DROP_4D2},
	{"DROP_GOOD", RF1, RF1_DROP_GOOD},
	{"DROP_GREAT", RF1, RF1_DROP_GREAT},
	{"DROP_CHEST", RF1, RF1_DROP_CHEST},
	{"DROP_CHOSEN", RF1, RF1_DROP_CHOSEN},

	/*RF1 uber-flags*/
	{"DROP_UP_TO_10", RF1, RF1_DROP_UP_TO_10},
	{"DROP_UP_TO_12", RF1, RF1_DROP_UP_TO_12},
	{"DROP_UP_TO_14", RF1, RF1_DROP_UP_TO_14},
	{"DROP_UP_TO_16", RF1, RF1_DROP_UP_TO_16},
	{"DROP_UP_TO_18", RF1, RF1_DROP_UP_TO_18},
	{"DROP_UP_TO_20", RF1, RF1_DROP_UP_TO_20},



/*
 * Monster race flags 2
 */

	{"MINDLESS", RF2, RF2_MINDLESS},
	{"SMART", RF2, RF2_SMART},
	{"TERRITORIAL", RF2, RF2_TERRITORIAL},
	{"SHORT_SIGHTED", RF2, RF2_SHORT_SIGHTED},
	{"INVISIBLE", RF2, RF2_INVISIBLE},
	{"GLOW", RF2, RF2_GLOW},
	{"CRUEL_BLOW", RF2, RF2_CRUEL_BLOW},
	{"EXCHANGE_PLACES", RF2, RF2_EXCHANGE_PLACES},
	{"MULTIPLY", RF2, RF2_MULTIPLY},
	{"REGENERATE", RF2, RF2_REGENERATE},
	{"RIPOSTE", RF2, RF2_RIPOSTE},
	{"FLANKING", RF2, RF2_FLANKING},
	{"CLOUD_SURROUND", RF2, RF2_CLOUD_SURROUND},
	{"FLYING", RF2, RF2_FLYING},
	{"PASS_DOOR", RF2, RF2_PASS_DOOR},
	{"UNLOCK_DOOR", RF2, RF2_UNLOCK_DOOR},
	{"OPEN_DOOR", RF2, RF2_OPEN_DOOR},
	{"BASH_DOOR", RF2, RF2_BASH_DOOR},
	{"PASS_WALL", RF2, RF2_PASS_WALL},
	{"KILL_WALL", RF2, RF2_KILL_WALL},
	{"TUNNEL_WALL", RF2, RF2_TUNNEL_WALL},
	{"KILL_BODY", RF2, RF2_KILL_BODY},
	{"TAKE_ITEM", RF2, RF2_TAKE_ITEM},
	{"KILL_ITEM", RF2, RF2_KILL_ITEM},
	{"RF2XXX6", RF2, RF2_RF2XXX6},
	{"LOW_MANA_RUN", RF2, RF2_LOW_MANA_RUN},
	{"CHARGE", RF2, RF2_CHARGE},
	{"ELFBANE", RF2, RF2_ELFBANE},
	{"KNOCK_BACK", RF2, RF2_KNOCK_BACK},
	{"CRIPPLING", RF2, RF2_CRIPPLING},
	{"OPPORTUNIST", RF2, RF2_OPPORTUNIST},
	{"ZONE_OF_CONTROL", RF2, RF2_ZONE_OF_CONTROL},



/*
 * Monster race flags 3
 */

	{"ORC", RF3, RF3_ORC},
	{"TROLL", RF3, RF3_TROLL},
	{"SERPENT", RF3, RF3_SERPENT},
	{"DRAGON", RF3, RF3_DRAGON},
	{"RAUKO", RF3, RF3_RAUKO},
	{"UNDEAD", RF3, RF3_UNDEAD},
	{"SPIDER", RF3, RF3_SPIDER},
	{"WOLF", RF3, RF3_WOLF},
	{"RF3XXX1", RF3, RF3_RF3XXX1},
	{"RF3XXX2", RF3, RF3_RF3XXX2},
	{"RF3XXX3", RF3, RF3_RF3XXX3},
	{"RF3XXX4", RF3, RF3_RF3XXX4},
	{"HURT_LITE", RF3, RF3_HURT_LITE},
	{"STONE", RF3, RF3_STONE},
	{"HURT_FIRE", RF3, RF3_HURT_FIRE},
	{"HURT_COLD", RF3, RF3_HURT_COLD},
	{"RF3XXX5", RF3, RF3_RF3XXX5},
	{"RES_ELEC", RF3, RF3_RES_ELEC},
	{"RES_FIRE", RF3, RF3_RES_FIRE},
	{"RES_COLD", RF3, RF3_RES_COLD},
	{"RES_POIS", RF3, RF3_RES_POIS},
	{"RF3XXX6", RF3, RF3_RF3XXX6},
	{"RES_NETHR", RF3, RF3_RES_NETHR},
	{"RES_WATER", RF3, RF3_RES_WATER},
	{"RES_PLAS", RF3, RF3_RES_PLAS},
	{"RES_NEXUS", RF3, RF3_RES_NEXUS},
	{"RES_DISEN", RF3, RF3_RES_DISEN},
	{"NO_SLOW", RF3, RF3_NO_SLOW},
	{"NO_FEAR", RF3, RF3_NO_FEAR},
	{"NO_STUN", RF3, RF3_NO_STUN},
	{"NO_CONF", RF3, RF3_NO_CONF},
	{"NO_SLEEP", RF3, RF3_NO_SLEEP},


	/*RF3 uber-flags*/
	{"RES_ELEM", RF3, RF3_RES_ELEM},
	{"RES_ALL", RF3, RF3_RES_ALL},
	{"NO_CHARM", RF3, RF3_NO_CHARM},


/*
 * Object flags 1
 */
/*
 * Monster race flags 4
 */

	{"ARROW1", RF4, RF4_ARROW1},
	{"ARROW2", RF4, RF4_ARROW2},
	{"BOULDER", RF4, RF4_BOULDER},
	{"BRTH_FIRE", RF4, RF4_BRTH_FIRE},
	{"BRTH_COLD", RF4, RF4_BRTH_COLD},
	{"BRTH_POIS", RF4, RF4_BRTH_POIS},
	{"BRTH_DARK", RF4, RF4_BRTH_DARK},
	{"EARTHQUAKE", RF4, RF4_EARTHQUAKE},
	{"SHRIEK", RF4, RF4_SHRIEK},
	{"SCREECH", RF4, RF4_SCREECH},
	{"DARKNESS", RF4, RF4_DARKNESS},
	{"FORGET", RF4, RF4_FORGET},
	{"SCARE", RF4, RF4_SCARE},
	{"CONF", RF4, RF4_CONF},
	{"HOLD", RF4, RF4_HOLD},
	{"SLOW", RF4, RF4_SLOW},
	{"SNG_BINDING", RF4, RF4_SNG_BINDING},
	{"SNG_PIERCING", RF4, RF4_SNG_PIERCING},
	{"SNG_OATHS", RF4, RF4_SNG_OATHS},
    
	{"RF4XXX20", RF4, RF4_RF4XXX20},
	{"RF4XXX21", RF4, RF4_RF4XXX21},
	{"RF4XXX22", RF4, RF4_RF4XXX22},
	{"RF4XXX23", RF4, RF4_RF4XXX23},
	{"RF4XXX24", RF4, RF4_RF4XXX24},
	{"RF4XXX25", RF4, RF4_RF4XXX25},
	{"RF4XXX26", RF4, RF4_RF4XXX26},
	{"RF4XXX27", RF4, RF4_RF4XXX27},
	{"RF4XXX28", RF4, RF4_RF4XXX28},
	{"RF4XXX29", RF4, RF4_RF4XXX29},
	{"RF4XXX30", RF4, RF4_RF4XXX30},
	{"RF4XXX31", RF4, RF4_RF4XXX31},
	{"RF4XXX32", RF4, RF4_RF4XXX32},


/*object_flags*/

	{"STR", TR1, TR1_STR},
	{"DEX", TR1, TR1_DEX},
	{"CON", TR1, TR1_CON},
	{"GRA", TR1, TR1_GRA},
	{"NEG_STR", TR1, TR1_NEG_STR},
	{"NEG_DEX", TR1, TR1_NEG_DEX},
	{"NEG_CON", TR1, TR1_NEG_CON},
	{"NEG_GRA", TR1, TR1_NEG_GRA},
	{"MELEE", TR1, TR1_MEL},
	{"ARCHERY", TR1, TR1_ARC},
	{"STEALTH", TR1, TR1_STL},
	{"PERCEPTION", TR1, TR1_PER},
	{"WILL", TR1, TR1_WIL},
	{"SMITHING", TR1, TR1_SMT},
	{"SONG", TR1, TR1_SNG},
	{"TR1XXX1",	TR1, TR1_TR1XXX1},
	{"DAMAGE_SIDES", TR1, TR1_DAMAGE_SIDES},
	{"TUNNEL", TR1, TR1_TUNNEL},
	{"SHARPNESS", TR1, TR1_SHARPNESS},
	{"SHARPNESS2", TR1, TR1_SHARPNESS2},
	{"VAMPIRIC", TR1, TR1_VAMPIRIC},
	{"SLAY_ORC", TR1, TR1_SLAY_ORC},
	{"SLAY_TROLL", TR1, TR1_SLAY_TROLL},
	{"SLAY_WOLF", TR1, TR1_SLAY_WOLF},
	{"SLAY_SPIDER", TR1, TR1_SLAY_SPIDER},
	{"SLAY_UNDEAD", TR1, TR1_SLAY_UNDEAD},
	{"SLAY_RAUKO", TR1, TR1_SLAY_RAUKO},
	{"SLAY_DRAGON", TR1, TR1_SLAY_DRAGON},
	{"BRAND_ELEC", TR1, TR1_BRAND_ELEC},
	{"BRAND_FIRE", TR1, TR1_BRAND_FIRE},
	{"BRAND_COLD", TR1, TR1_BRAND_COLD},
	{"BRAND_POIS", TR1, TR1_BRAND_POIS},

	/*TR1 Uber-flags*/
	{"ALL_STATS", TR1, TR1_ALL_STATS},

/*
 * Object flags 2
 */

	{"SUST_STR", TR2, TR2_SUST_STR},
	{"SUST_DEX", TR2, TR2_SUST_DEX},
	{"SUST_CON", TR2, TR2_SUST_CON},
	{"SUST_GRA", TR2, TR2_SUST_GRA},
	{"RES_ELEC", TR2, TR2_RES_ELEC},
	{"RES_FIRE", TR2, TR2_RES_FIRE},
	{"RES_COLD", TR2, TR2_RES_COLD},
	{"RES_POIS", TR2, TR2_RES_POIS},
	{"RES_DARK", TR2, TR2_RES_DARK},
	{"RES_FEAR", TR2, TR2_RES_FEAR},
	{"RES_BLIND", TR2, TR2_RES_BLIND},
	{"RES_CONFU", TR2, TR2_RES_CONFU},
	{"RES_STUN",  TR2, TR2_RES_STUN},
	{"RES_HALLU",  TR2, TR2_RES_HALLU},
	{"RADIANCE", TR2, TR2_RADIANCE},
	{"SLOW_DIGEST", TR2, TR2_SLOW_DIGEST},
	{"LIGHT", TR2, TR2_LIGHT},
	{"REGEN", TR2, TR2_REGEN},
	{"SEE_INVIS", TR2, TR2_SEE_INVIS},
	{"FREE_ACT", TR2, TR2_FREE_ACT},
	{"TR2XXX1", TR2, TR2_TR2XXX1},
	{"SPEED", TR2, TR2_SPEED},
	{"FEAR", TR2, TR2_FEAR},
	{"HUNGER", TR2, TR2_HUNGER},
	{"DARKNESS", TR2, TR2_DARKNESS},
	{"SLOWNESS", TR2, TR2_SLOWNESS},
	{"DANGER", TR2, TR2_DANGER},
	{"AGGRAVATE", TR2, TR2_AGGRAVATE},
	{"HAUNTED", TR2, TR2_HAUNTED},
	{"VUL_COLD", TR2, TR2_VUL_COLD},
	{"VUL_FIRE", TR2, TR2_VUL_FIRE},
	{"VUL_POIS", TR2, TR2_VUL_POIS},

	/*TR2 Uber-flags*/
	{"SUST_STATS", TR2, TR2_SUST_STATS},
	{"RESISTANCE", TR2, TR2_RESISTANCE},

/*
 * Object flags 3
 */

	{"TR3XXX1", TR3, TR3_TR3XXX1},
	{"TR3XXX2", TR3, TR3_TR3XXX2},
	{"TR3XXX3", TR3, TR3_TR3XXX3},
	{"TR3XXX4", TR3, TR3_TR3XXX4},
	{"TR3XXX14", TR3, TR3_TR3XXX14},
	{"TR3XXX13", TR3, TR3_TR3XXX13},
	{"TR3XXX5", TR3, TR3_TR3XXX5},
	{"TR3XXX6", TR3, TR3_TR3XXX6},
	{"TR3XXX7", TR3, TR3_TR3XXX7},
	{"TR3XXX8", TR3, TR3_TR3XXX8},
	{"TR3XXX9", TR3, TR3_TR3XXX9},
	{"INDESTRUCTIBLE", TR3, TR3_INDESTRUCTIBLE},
	{"NO_SMITHING", TR3, TR3_NO_SMITHING},
	{"MITHRIL", TR3, TR3_MITHRIL},
	{"AXE", TR3, TR3_AXE},
	{"POLEARM", TR3, TR3_POLEARM},
	{"IGNORE_ACID", TR3, TR3_IGNORE_ACID},
	{"IGNORE_ELEC", TR3, TR3_IGNORE_ELEC},
	{"IGNORE_FIRE", TR3, TR3_IGNORE_FIRE},
	{"IGNORE_COLD", TR3, TR3_IGNORE_COLD},
	{"THROWING", TR3, TR3_THROWING},
	{"ENCHANTABLE", TR3, TR3_ENCHANTABLE},
	{"ACTIVATE", TR3, TR3_ACTIVATE},
	{"INSTA_ART", TR3, TR3_INSTA_ART},
	{"EASY_KNOW", TR3, TR3_EASY_KNOW},
	{"TR3XXX11", TR3, TR3_TR3XXX11},
	{"TR3XXX12", TR3, TR3_TR3XXX12},
	{"HAND_AND_A_HALF", TR3, TR3_HAND_AND_A_HALF},
	{"TWO_HANDED", TR3, TR3_TWO_HANDED},
	{"LIGHT_CURSE", TR3, TR3_LIGHT_CURSE},
	{"HEAVY_CURSE", TR3, TR3_HEAVY_CURSE},
	{"PERMA_CURSE",  TR3, TR3_PERMA_CURSE},

	{"IGNORE_ALL",  TR3, TR3_IGNORE_ALL},


/*
 * Race/House flags
 */
	{"BLADE_PROFICIENCY",  RHF, RHF_BLADE_PROFICIENCY},
	{"AXE_PROFICIENCY",  RHF, RHF_AXE_PROFICIENCY},
	{"MEL_AFFINITY",  RHF, RHF_MEL_AFFINITY},
	{"MEL_PENALTY",  RHF, RHF_MEL_PENALTY},
	{"ARC_AFFINITY",  RHF, RHF_ARC_AFFINITY},
	{"ARC_PENALTY",  RHF, RHF_ARC_PENALTY},
	{"EVN_AFFINITY",  RHF, RHF_EVN_AFFINITY},
	{"EVN_PENALTY",  RHF, RHF_EVN_PENALTY},
	{"STL_AFFINITY",  RHF, RHF_STL_AFFINITY},
	{"STL_PENALTY",  RHF, RHF_STL_PENALTY},
	{"PER_AFFINITY",  RHF, RHF_PER_AFFINITY},
	{"PER_PENALTY",  RHF, RHF_PER_PENALTY},
	{"WIL_AFFINITY",  RHF, RHF_WIL_AFFINITY},
	{"WIL_PENALTY",  RHF, RHF_WIL_PENALTY},
	{"SMT_AFFINITY",  RHF, RHF_SMT_AFFINITY},
	{"SMT_PENALTY",  RHF, RHF_SMT_PENALTY},
	{"SNG_AFFINITY",  RHF, RHF_SNG_AFFINITY},
	{"SNG_PENALTY",  RHF, RHF_SNG_PENALTY},
	{"RHFXXX19",  RHF, RHF_RHFXXX19},
	{"RHFXXX20",  RHF, RHF_RHFXXX20},
	{"RHFXXX21",  RHF, RHF_RHFXXX21},
	{"RHFXXX22",  RHF, RHF_RHFXXX22},
	{"RHFXXX23",  RHF, RHF_RHFXXX23},
	{"RHFXXX24",  RHF, RHF_RHFXXX24},
	{"RHFXXX21",  RHF, RHF_RHFXXX25},
	{"RHFXXX25",  RHF, RHF_RHFXXX26},
	{"RHFXXX26",  RHF, RHF_RHFXXX27},
	{"RHFXXX27",  RHF, RHF_RHFXXX28},
	{"RHFXXX28",  RHF, RHF_RHFXXX29},
	{"RHFXXX29",  RHF, RHF_RHFXXX29},
	{"RHFXXX30",  RHF, RHF_RHFXXX30},
	{"RHFXXX31",  RHF, RHF_RHFXXX31},
	{"RHFXXX32",  RHF, RHF_RHFXXX32},


    /*
     * Vault flags
     */
	{"TEST",            VLT, VLT_TEST},
	{"NO_ROTATION",     VLT, VLT_NO_ROTATION},
	{"TRAPS",           VLT, VLT_TRAPS},
	{"WEBS",            VLT, VLT_WEBS},
	{"LIGHT",           VLT, VLT_LIGHT},
	{"VLTXXXX6",  VLT, VLT_VLTXXXX6},
	{"VLTXXXX7",  VLT, VLT_VLTXXXX7},
	{"VLTXXXX8",  VLT, VLT_VLTXXXX8},
	{"VLTXXXX9",  VLT, VLT_VLTXXXX9},
	{"VLTXXX10",  VLT, VLT_VLTXXX10},
	{"VLTXXX11",  VLT, VLT_VLTXXX11},
	{"VLTXXX12",  VLT, VLT_VLTXXX12},
	{"VLTXXX13",  VLT, VLT_VLTXXX13},
	{"VLTXXX14",  VLT, VLT_VLTXXX14},
	{"VLTXXX15",  VLT, VLT_VLTXXX15},
	{"VLTXXX16",  VLT, VLT_VLTXXX16},
	{"VLTXXX17",  VLT, VLT_VLTXXX17},
	{"VLTXXX18",  VLT, VLT_VLTXXX18},
	{"VLTXXX19",  VLT, VLT_VLTXXX19},
	{"VLTXXX20",  VLT, VLT_VLTXXX20},
	{"VLTXXX21",  VLT, VLT_VLTXXX21},
	{"VLTXXX22",  VLT, VLT_VLTXXX22},
	{"VLTXXX23",  VLT, VLT_VLTXXX23},
	{"VLTXXX24",  VLT, VLT_VLTXXX24},
	{"VLTXXX21",  VLT, VLT_VLTXXX25},
	{"VLTXXX25",  VLT, VLT_VLTXXX26},
	{"VLTXXX26",  VLT, VLT_VLTXXX27},
	{"VLTXXX27",  VLT, VLT_VLTXXX28},
	{"VLTXXX28",  VLT, VLT_VLTXXX29},
	{"VLTXXX29",  VLT, VLT_VLTXXX29},
	{"VLTXXX30",  VLT, VLT_VLTXXX30},
	{"VLTXXX31",  VLT, VLT_VLTXXX31},
	{"VLTXXX32",  VLT, VLT_VLTXXX32}

};






/*
 * Activation type
 */
static cptr a_info_act[ACT_MAX] =
{
	"ILLUMINATION",
	"MAGIC_MAP",
	"CLAIRVOYANCE",
	"PROT_EVIL",
	"DISP_EVIL",
	"HEAL1",
	"HEAL2",
	"CURE_WOUNDS",
	"HASTE1",
	"HASTE2",
	"FIRE1",
	"FIRE2",
	"FIRE3",
	"FROST1",
	"FROST2",
	"FROST3",
	"FROST4",
	"FROST5",
	"ACID1",
	"RECHARGE1",
	"SLEEP",
	"LIGHTNING_BOLT",
	"ELEC2",
	"BANISHMENT",
	"MASS_BANISHMENT",
	"IDENTIFY_FULLY",
	"DRAIN_LIFE1",
	"DRAIN_LIFE2",
	"BIZZARE",
	"STAR_BALL",
	"RAGE_BLESS_RESIST",
	"PHASE",
	"TRAP_DOOR_DEST",
	"DETECT",
	"RESIST",
	"TELEPORT",
	"RESTORE_VOICE",
	"MISSILE",
	"ARROW",
	"REM_FEAR_POIS",
	"STINKING_CLOUD",
	"STONE_TO_MUD",
	"TELE_AWAY",
	"WOR",
	"CONFUSE",
	"PROBE",
	"FIREBRAND",
	"STARLIGHT",
	"MANA_BOLT",
	"BERSERKER",
	"RES_ACID",
	"RES_ELEC",
	"RES_FIRE",
	"RES_COLD",
	"RES_POIS"
};



/*** Initialize from ascii template files ***/


/*
 * Initialize an "*_info" array, by parsing an ascii "template" file
 */
errr init_info_txt(FILE *fp, char *buf, header *head,
                   parse_info_txt_func parse_info_txt_line)
{
	errr err;

	/* Not ready yet */
	bool okay = FALSE;

	/* Just before the first record */
	error_idx = -1;

	/* Just before the first line */
	error_line = 0;


	/* Prepare the "fake" stuff */
	head->name_size = 0;
	head->text_size = 0;

	/* Parse */
	while (0 == my_fgets(fp, buf, 1024))
	{
		/* Advance the line number */
		error_line++;

		/* Skip comments and blank lines */
		if (!buf[0] || (buf[0] == '#')) continue;

		/* Verify correct "colon" format */
		if (buf[1] != ':') return (PARSE_ERROR_GENERIC);


		/* Hack -- Process 'V' for "Version" */
		if (buf[0] == 'V')
		{
			int v1, v2, v3;

			/* Scan for the values */
			if ((3 != sscanf(buf+2, "%d.%d.%d", &v1, &v2, &v3)) ||
				(v1 != head->v_major) ||
				(v2 != head->v_minor) ||
				(v3 != head->v_patch))
			{
				return (PARSE_ERROR_OBSOLETE_FILE);
			}

			/* Okay to proceed */
			okay = TRUE;

			/* Continue */
			continue;
		}

		/* No version yet */
		if (!okay) return (PARSE_ERROR_OBSOLETE_FILE);

		/* Parse the line */
		if ((err = (*parse_info_txt_line)(buf, head)) != 0)
			return (err);
	}


	/* Complete the "name" and "text" sizes */
	if (head->name_size) head->name_size++;
	if (head->text_size) head->text_size++;


	/* No version yet */
	if (!okay) return (PARSE_ERROR_OBSOLETE_FILE);


	/* Success */
	return (0);
}


/*
 * Add a text to the text-storage and store offset to it.
 *
 * Returns FALSE when there isn't enough space available to store
 * the text.
 */
static bool add_text(u32b *offset, header *head, cptr buf)
{
	/* Hack -- Verify space */
	if (head->text_size + strlen(buf) + 8 > z_info->fake_text_size)
		return (FALSE);

	/* New text? */
	if (*offset == 0)
	{
		/* Advance and save the text index */
		*offset = ++head->text_size;
	}

	/* Append chars to the text */
	strcpy(head->text_ptr + head->text_size, buf);

	/* Advance the index */
	head->text_size += strlen(buf);

	/* Success */
	return (TRUE);
}


/*
 * Add a name to the name-storage and return an offset to it.
 *
 * Returns 0 when there isn't enough space available to store
 * the name.
 */
static u32b add_name(header *head, cptr buf)
{
	u32b index;

	/* Hack -- Verify space */
	if (head->name_size + strlen(buf) + 8 > z_info->fake_name_size)
		return (0);

	/* Advance and save the name index */
	index = ++head->name_size;

	/* Append chars to the names */
	strcpy(head->name_ptr + head->name_size, buf);

	/* Advance the index */
	head->name_size += strlen(buf);

	/* Return the name index */
	return (index);
}


/*
 * Initialize the "z_info" structure, by parsing an ascii "template" file
 */
errr parse_z_info(char *buf, header *head)
{
	maxima *z_info = head->info_ptr;

	/* Hack - Verify 'M:x:' format */
	if (buf[0] != 'M') return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	if (!buf[2]) return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	if (buf[3] != ':') return (PARSE_ERROR_UNDEFINED_DIRECTIVE);


	/* Process 'F' for "Maximum f_info[] index" */
	if (buf[2] == 'F')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->f_max = max;
	}

	/* Process 'K' for "Maximum k_info[] index" */
	else if (buf[2] == 'K')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->k_max = max;
	}

	/* Process 'B' for "Maximum b_info[] index" */
	else if (buf[2] == 'B')
	{
		int max;
		
		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);
		
		/* Save the value */
		z_info->b_max = max;
	}
	
	/* Process 'A' for "Maximum a_info[] index" */
	else if (buf[2] == 'A')
	{
		int art_special_max, art_normal_max, art_random_max, art_self_made_max;

		/* Scan for the value */
		if (4 != sscanf(buf+4, "%d:%d:%d:%d", &art_special_max,
					&art_normal_max, &art_random_max, &art_self_made_max)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		z_info->art_spec_max = art_special_max;
		z_info->art_norm_max = art_normal_max + art_special_max;
		z_info->art_rand_max = z_info->art_norm_max + art_random_max;
		z_info->art_self_made_max = z_info->art_rand_max + art_self_made_max;

		/* Total artefacts */
		z_info->art_max = art_special_max + art_normal_max + art_random_max + art_self_made_max;
	}

	/* Process 'E' for "Maximum e_info[] index" */
	else if (buf[2] == 'E')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->e_max = max;
	}

	/* Process 'G' for "Maximum e_info[] index" */
	else if (buf[2] == 'G')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->ghost_other_max = max;
	}

	/* Process 'R' for "Maximum r_info[] index" */
	else if (buf[2] == 'R')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->r_max = max;
	}


	/* Process 'V' for "Maximum v_info[] index" */
	else if (buf[2] == 'V')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->v_max = max;
	}


	/* Process 'P' for "Maximum p_info[] index" */
	else if (buf[2] == 'P')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->p_max = max;
	}

	/* Process 'C' for "Maximum c_info[] index" */
	else if (buf[2] == 'C')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->c_max = max;
	}

	/* Process 'H' for "Maximum h_info[] index" */
	else if (buf[2] == 'H')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->h_max = max;
	}

	/* Process 'Q' for "Maximum q_info[] index" */
	else if (buf[2] == 'Q')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->q_max = max;
	}

	/* Process 'L' for "Maximum flavor_info[] subindex" */
	else if (buf[2] == 'L')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->flavor_max = max;
	}

	/* Process 'O' for "Maximum o_list[] index" */
	else if (buf[2] == 'O')
	{
		int max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%d", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->o_max = max;
	}

	/* Process 'N' for "Fake name size" */
	else if (buf[2] == 'N')
	{
		long max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%ld", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->fake_name_size = max;
	}

	/* Process 'T' for "Fake text size" */
	else if (buf[2] == 'T')
	{
		long max;

		/* Scan for the value */
		if (1 != sscanf(buf+4, "%ld", &max)) return (PARSE_ERROR_GENERIC);

		/* Save the value */
		z_info->fake_text_size = max;
	}
	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}



/*
 * Initialize the "f_info" array, by parsing an ascii "template" file
 */
errr parse_f_info(char *buf, header *head)
{
	int i;

	char *s;

	/* Current entry */
	static feature_type *f_ptr = NULL;


	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		f_ptr = (feature_type*)head->info_ptr + i;

		/* Store the name */
		if (!(f_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);

		/* Default "mimic" */
		f_ptr->mimic = i;
	}

	/* Process 'M' for "Mimic" (one line only) */
	else if (buf[0] == 'M')
	{
		int mimic;

		/* There better be a current f_ptr */
		if (!f_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (1 != sscanf(buf+2, "%d",
			            &mimic)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		f_ptr->mimic = mimic;
	}

	/* Process 'G' for "Graphics" (one line only) */
	else if (buf[0] == 'G')
	{
		char d_char;
		int d_attr;

		/* There better be a current f_ptr */
		if (!f_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Paranoia */
		if (!buf[2]) return (PARSE_ERROR_GENERIC);
		if (!buf[3]) return (PARSE_ERROR_GENERIC);
		if (!buf[4]) return (PARSE_ERROR_GENERIC);

		/* Extract d_char */
		d_char = buf[2];

		/* If we have a longer string than expected ... */
		if (buf[5])
		{
			/* Advance "buf" on by 4 */
			buf += 4;

			/* Extract the colour */
			d_attr = color_text_to_attr(buf);
		}
		else
		{
			/* Extract the attr */
			d_attr = color_char_to_attr(buf[4]);
		}

		/* Paranoia */
		if (d_attr < 0) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		f_ptr->d_attr = d_attr;
		f_ptr->d_char = d_char;
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


/*
 * Grab one flag from a textual string
 */
static errr grab_one_flag(u32b **flag, cptr errstr, cptr what)
{
	uint i;

	/* Check flags */
	for (i = 0; i < N_ELEMENTS(info_flags); i++)
	{
		flag_name *f_ptr = info_flags+i;

		if (!flag[f_ptr->set]) continue;

		if (streq(what, f_ptr->name))
		{
			*(flag[f_ptr->set]) |= f_ptr->flag;
			return 0;
		}
	}

	/* Oops */
	msg_format("Unknown %s flag '%s'.", errstr, what);

	/* Error */
	return (-1);
}


/*
 * Grab one flag in an object_kind from a textual string
 */
static errr grab_one_kind_flag(object_kind *ptr, cptr what)
{
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[TR1] = &(ptr->flags1);
	f[TR2] = &(ptr->flags2);
	f[TR3] = &(ptr->flags3);
	return grab_one_flag(f, "object", what);
}




/*
 * Initialize the "k_info" array, by parsing an ascii "template" file
 */
errr parse_k_info(char *buf, header *head)
{
	int i;

	char *s, *t;

	/* Current entry */
	static object_kind *k_ptr = NULL;

	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		k_ptr = (object_kind*)head->info_ptr + i;

		/* Store the name */
		if (!(k_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	/* Process 'G' for "Graphics" (one line only) */
	else if (buf[0] == 'G')
	{
		char d_char;
		int d_attr;

		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Paranoia */
		if (!buf[2]) return (PARSE_ERROR_GENERIC);
		if (!buf[3]) return (PARSE_ERROR_GENERIC);
		if (!buf[4]) return (PARSE_ERROR_GENERIC);

		/* Extract d_char */
		d_char = buf[2];

		/* If we have a longer string than expected ... */
		if (buf[5])
		{
			/* Advance "buf" on by 4 */
			buf += 4;

			/* Extract the colour */
			d_attr = color_text_to_attr(buf);
		}
		else
		{
			/* Extract the attr */
			d_attr = color_char_to_attr(buf[4]);
		}

		/* Paranoia */
		if (d_attr < 0) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		k_ptr->d_attr = d_attr;
		k_ptr->d_char = d_char;
	}

	/* Process 'I' for "Info" (one line only) */
	else if (buf[0] == 'I')
	{
		int tval, sval, pval;

		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d",
			            &tval, &sval, &pval)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		k_ptr->tval = tval;
		k_ptr->sval = sval;
		k_ptr->pval = pval;
	}

	/* Process 'W' for "More Info" (one line only) */
	else if (buf[0] == 'W')
	{
		int level, extra, wgt;
		long cost;

		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%ld",
			            &level, &extra, &wgt, &cost)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		k_ptr->level = level;
		k_ptr->extra = extra;
		k_ptr->weight = wgt;
		k_ptr->cost = cost;
	}

	/* Process 'A' for "Allocation" (one line only) */
	else if (buf[0] == 'A')
	{
		int i;

		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* XXX Simply read each number following a colon */
		for (i = 0, s = buf+1; s && (s[0] == ':') && s[1]; ++i)
		{
			/* Sanity check */
			if (i > 3) return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

			/* Default chance */
			k_ptr->chance[i] = 1;

			/* Store the attack damage index */
			k_ptr->locale[i] = atoi(s+1);

			/* Find the slash */
			t = strchr(s+1, '/');

			/* Find the next colon */
			s = strchr(s+1, ':');

			/* If the slash is "nearby", use it */
			if (t && (!s || t < s))
			{
				int chance = atoi(t+1);
				if (chance > 0) k_ptr->chance[i] = chance;
			}
		}
	}

	/* Hack -- Process 'P' for "power" and such */
	else if (buf[0] == 'P')
	{
		int att, dd, ds, evn, pd, ps;

		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (6 != sscanf(buf+2, "%d:%dd%d:%d:%dd%d",
			            &att, &dd, &ds, &evn, &pd, &ps)) return (PARSE_ERROR_GENERIC);
		
		k_ptr->att = att;
		k_ptr->dd = dd;
		k_ptr->ds = ds;
		k_ptr->evn = evn;
		k_ptr->pd = pd;
		k_ptr->ps = ps;
		
	}

	/* Hack -- Process 'F' for flags */
	else if (buf[0] == 'F')
	{
		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while (*t == ' ' || *t == '|') t++;
			}

			/* Parse this entry */
			if (0 != grab_one_kind_flag(k_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}

	/* Process 'B' for "aBilities" (one line only) */
	else if (buf[0] == 'B')
	{
		int i;
		
		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* XXX Simply read each number following a colon */
		for (i = 0, s = buf+1; s && (s[0] == ':') && s[1]; ++i)
		{
			/* Sanity check */
			if (i > 3) return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);
			
			/* Default abilitynum */
			k_ptr->abilitynum[i] = 0;
			
			/* Store the skilltype */
			k_ptr->skilltype[i] = atoi(s+1);
			
			/* List this ability */
			k_ptr->abilities++;
			
			/* Find the slash */
			t = strchr(s+1, '/');
			
			/* Find the next colon */
			s = strchr(s+1, ':');
			
			/* If the slash is "nearby", use it */
			if (t && (!s || t < s))
			{
				int abilitynum = atoi(t+1);
				if (abilitynum > 0) k_ptr->abilitynum[i] = abilitynum;
			}
		}
	}
	
	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current k_ptr */
		if (!k_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&(k_ptr->text), head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}

/*
 * Grab one flag in a vault from a textual string
 *
 */
static errr grab_one_vault_flag(vault_type *ptr, cptr what)
{
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[VLT] = &(ptr->flags);
	return grab_one_flag(f, "vault", what);
}

/*
 * Initialize the "v_info" array, by parsing an ascii "template" file
 */
errr parse_v_info(char *buf, header *head)
{
	int i;
    
	char *s, *t;
    
	/* Current entry */
	static vault_type *v_ptr = NULL;
    
    
	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');
        
		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);
        
		/* Nuke the colon, advance to the name */
		*s++ = '\0';
        
		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);
        
		/* Get the index */
		i = atoi(buf+2);
        
		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        
		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);
        
		/* Save the index */
		error_idx = i;
        
		/* Point at the "info" */
		v_ptr = (vault_type*)head->info_ptr + i;
        
		/* Store the name */
		if (!(v_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}
    
	/* Process 'X' for "Extra info" (one line only) */
	else if (buf[0] == 'X')
	{
		int typ, depth, rarity;
		
		/* There better be a current v_ptr */
		if (!v_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d", &typ, &depth, &rarity)) return (PARSE_ERROR_GENERIC);
        
		/* Save the values */
		v_ptr->typ = typ;
		v_ptr->depth = depth;
		v_ptr->rarity = rarity;
		v_ptr->hgt = 0;
		v_ptr->wid = 0;
	}
    
	/* Hack -- Process 'F' for flags */
	else if (buf[0] == 'F')
	{
		/* There better be a current k_ptr */
		if (!v_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;
            
			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while (*t == ' ' || *t == '|') t++;
			}
            
			/* Parse this entry */
			if (0 != grab_one_vault_flag(v_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);
            
			/* Start the next entry */
			s = t;
		}
	}
    
	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
        
		/* There better be a current v_ptr */
		if (!v_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
		/* Get the text */
		s = buf+2;
        
        if (v_ptr->wid == 0)
        {
            v_ptr->wid = strlen(buf+2);
        }
        else if (v_ptr->wid != strlen(buf+2))
        {
            return (PARSE_ERROR_VAULT_NOT_RECTANGULAR);
        }
        
        
		/* Store the text */
		if (!add_text(&v_ptr->text, head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
        
		// note if there is a forge in the vault
		if (strchr(buf, '0')) v_ptr->forge = TRUE;
        
        // we've added another row of the vault
        v_ptr->hgt++;
        
        /* Check for maximum vault sizes */
		if ((v_ptr->typ == 6) && ((v_ptr->wid > 33) || (v_ptr->hgt > 22)))
			return (PARSE_ERROR_VAULT_TOO_BIG);
        
		if ((v_ptr->typ == 7) && ((v_ptr->wid > 33) || (v_ptr->hgt > 22)))
			return (PARSE_ERROR_VAULT_TOO_BIG);
        
		if ((v_ptr->typ == 8) && ((v_ptr->wid > 66) || (v_ptr->hgt > 44)))
			return (PARSE_ERROR_VAULT_TOO_BIG);
	}
    
	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}
    
	/* Success */
	return (0);
}


/*
 * Initialize the "b_info" array, by parsing an ascii "template" file
 */
errr parse_b_info(char *buf, header *head)
{
	int i;
	
	char *s, *t;
	
	static int cur_t = 0;
	
	/* Current entry */
	static ability_type *b_ptr = NULL;
	
	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');
		
		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);
		
		/* Nuke the colon, advance to the name */
		*s++ = '\0';
		
		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);
		
		/* Get the index */
		i = atoi(buf+2);
		
		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
		
		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);
		
		/* Save the index */
		error_idx = i;
		
		/* Point at the "info" */
		b_ptr = (ability_type*)head->info_ptr + i;
		
		/* Store the name */
		if (!(b_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);

		/* Start with the first of the tval indices */
		cur_t = 0;
	}
	
	/* Process 'I' for "Info" (one line only) */
	else if (buf[0] == 'I')
	{
		int skilltype, abilitynum, level;
		
		/* There better be a current k_ptr */
		if (!b_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d",
			            &skilltype, &abilitynum, &level)) return (PARSE_ERROR_GENERIC);
		
		/* Save the values */
		b_ptr->skilltype = skilltype;
		b_ptr->abilitynum = abilitynum;
		b_ptr->level = level;
	}
	
	/* Process 'P' for "Prerequisites" (one line only) */
	else if (buf[0] == 'P')
	{
		int i;
		
		/* There better be a current b_ptr */
		if (!b_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* XXX Simply read each number following a colon */
		for (i = 0, s = buf+1; s && (s[0] == ':') && s[1]; ++i)
		{
			/* Sanity check */
			if (i > 3) return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);
			
			/* Default abilitynum */
			b_ptr->prereq_abilitynum[i] = 0;
			
			/* Store the skilltype */
			b_ptr->prereq_skilltype[i] = atoi(s+1);
			
			/* List this prerequisite */
			b_ptr->prereqs++;
			
			/* Find the slash */
			t = strchr(s+1, '/');
			
			/* Find the next colon */
			s = strchr(s+1, ':');
			
			/* If the slash is "nearby", use it */
			if (t && (!s || t < s))
			{
				int prereq_abilitynum = atoi(t+1);
				if (prereq_abilitynum > 0) b_ptr->prereq_abilitynum[i] = prereq_abilitynum;
			}
		}
	}
		
	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current k_ptr */
		if (!b_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* Get the text */
		s = buf+2;
		
		/* Store the text */
		if (!add_text(&(b_ptr->text), head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	/* Process 'T' for "Types allowed" (up to five lines) */
	else if (buf[0] == 'T')
	{
		int tval, sval1, sval2;
		
		/* There better be a current b_ptr */
		if (!b_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d",
			            &tval, &sval1, &sval2)) return (PARSE_ERROR_GENERIC);
		
		/* Save the values */
		b_ptr->tval[cur_t] = (byte)tval;
		b_ptr->min_sval[cur_t] = (byte)sval1;
		b_ptr->max_sval[cur_t] = (byte)sval2;
		
		/* Increase counter for 'possible tval' index */
		cur_t++;
		
		/* Allow only a limited number of T: lines */
		if (cur_t > ABILITY_TVALS_MAX) return (PARSE_ERROR_GENERIC);
	}
	
	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}
	
	/* Success */
	return (0);
}


/*
 * Grab one flag in an artefact_type from a textual string
 */
static errr grab_one_artefact_flag(artefact_type *ptr, cptr what)
{
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[TR1] = &(ptr->flags1);
	f[TR2] = &(ptr->flags2);
	f[TR3] = &(ptr->flags3);
	return grab_one_flag(f, "object", what);
}



/*
 * Grab one activation from a textual string
 */
static errr grab_one_activation(artefact_type *a_ptr, cptr what)
{
	int i;

	/* Scan activations */
	for (i = 0; i < ACT_MAX; i++)
	{
		if (streq(what, a_info_act[i]))
		{
			a_ptr->activation = i;
			return (0);
		}
	}

	/* Oops */
	msg_format("Unknown artefact activation '%s'.", what);

	/* Error */
	return (PARSE_ERROR_GENERIC);
}



/*
 * Initialize the "a_info" array, by parsing an ascii "template" file
 */
errr parse_a_info(char *buf, header *head)
{
	int i;

	char *s, *t;

	/* Current entry */
	static artefact_type *a_ptr = NULL;

	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		a_ptr = (artefact_type*)head->info_ptr + i;

		/* Store the name */
		my_strcpy(a_ptr->name, s, MAX_LEN_ART_NAME);

		/* Ignore everything */
		a_ptr->flags3 |= (TR3_IGNORE_MASK);
		
		/* Sil-y: paranoia: make sure that the default values are 0 */
		a_ptr->d_attr = 0;
		a_ptr->d_char = 0;

	}

	/* Sil -- added this to allow for artefacts that look different to the base type */
	/* Process 'G' for "Graphics" (one line only) */
	else if (buf[0] == 'G')
	{
		char d_char;
		int d_attr;

		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Paranoia */
		if (!buf[2]) return (PARSE_ERROR_GENERIC);
		if (!buf[3]) return (PARSE_ERROR_GENERIC);
		if (!buf[4]) return (PARSE_ERROR_GENERIC);

		/* Extract d_char */
		d_char = buf[2];

		/* If we have a longer string than expected ... */
		if (buf[5])
		{
			/* Advance "buf" on by 4 */
			buf += 4;

			/* Extract the colour */
			d_attr = color_text_to_attr(buf);
		}
		else
		{
			/* Extract the attr */
			d_attr = color_char_to_attr(buf[4]);
		}

		/* Paranoia */
		if (d_attr < 0) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		a_ptr->d_attr = d_attr;
		a_ptr->d_char = d_char;
	}

	/* Process 'I' for "Info" (one line only) */
	else if (buf[0] == 'I')
	{
		int tval, sval, pval;

		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d",
			            &tval, &sval, &pval)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		a_ptr->tval = tval;
		a_ptr->sval = sval;
		a_ptr->pval = pval;
	}

	/* Process 'W' for "More Info" (one line only) */
	else if (buf[0] == 'W')
	{
		int level, rarity, wgt;
		long cost;

		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%ld",
			            &level, &rarity, &wgt, &cost)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		a_ptr->level = level;
		a_ptr->rarity = rarity;
		a_ptr->weight = wgt;
		a_ptr->cost = cost;
	}

	/* Process 'P' for "power" and such */
	else if (buf[0] == 'P')
	{
		int att, dd, ds, evn, pd, ps;

		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (6 != sscanf(buf+2, "%d:%dd%d:%d:%dd%d",
			            &att, &dd, &ds, &evn, &pd, &ps)) return (PARSE_ERROR_GENERIC);
		
		a_ptr->att = att;
		a_ptr->dd = dd;
		a_ptr->ds = ds;
		a_ptr->evn = evn;
		a_ptr->pd = pd;
		a_ptr->ps = ps;
	}

	/* Process 'F' for flags */
	else if (buf[0] == 'F')
	{
		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while ((*t == ' ') || (*t == '|')) t++;
			}

			/* Parse this entry */
			if (0 != grab_one_artefact_flag(a_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}

	/* Process 'A' for "Activation & time" */
	else if (buf[0] == 'A')
	{
		int ptime, prand;

		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Find the colon before the name */
		s = strchr(buf + 2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the activation */
		if (grab_one_activation(a_ptr, buf + 2)) return (PARSE_ERROR_GENERIC);

		/* Scan for the values */
		if (2 != sscanf(s, "%d:%d",
			            &ptime, &prand)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		a_ptr->time = ptime;
		a_ptr->randtime = prand;
	}

	/* Process 'B' for "aBilities" (one line only) */
	else if (buf[0] == 'B')
	{
		int i;
		
		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* XXX Simply read each number following a colon */
		for (i = 0, s = buf+1; s && (s[0] == ':') && s[1]; ++i)
		{
			/* Sanity check */
			if (i > 3) return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);
			
			/* Default abilitynum */
			a_ptr->abilitynum[i] = 0;
			
			/* Store the skilltype */
			a_ptr->skilltype[i] = atoi(s+1);
			
			/* List this ability */
			a_ptr->abilities++;
			
			/* Find the slash */
			t = strchr(s+1, '/');
			
			/* Find the next colon */
			s = strchr(s+1, ':');
			
			/* If the slash is "nearby", use it */
			if (t && (!s || t < s))
			{
				int abilitynum = atoi(t+1);
				if (abilitynum > 0) a_ptr->abilitynum[i] = abilitynum;
			}
		}
	}
	
	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current a_ptr */
		if (!a_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&a_ptr->text, head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


/*
 * Add a name to the probability tables
 */
static errr build_prob(char *name, names_type *n_ptr)
{
	int c_prev, c_cur, c_next;

	while (*name && !isalpha((unsigned char) *name))
      ++name;

	if (!*name)	return PARSE_ERROR_GENERIC;

	c_prev = c_cur = S_WORD;

	do
	{
		if (isalpha ((unsigned char) *name))
		{
			c_next = A2I (tolower ((unsigned char) *name));
			n_ptr->lprobs[c_prev][c_cur][c_next]++;
			n_ptr->ltotal[c_prev][c_cur]++;
			c_prev = c_cur;
			c_cur = c_next;
		}
	}
	while (*++name);

	n_ptr->lprobs[c_prev][c_cur][E_WORD]++;
	n_ptr->ltotal[c_prev][c_cur]++;

	return 0;
}

/*
 * Initialize the "n_info" array, by parsing an ascii "template" file
 */
errr parse_n_info(char *buf, header *head)
{
	names_type *n_ptr = head->info_ptr;

	/*
	 * This function is called once, when the raw file does not exist.
	 * If you want to initialize some stuff before parsing the txt file
 	 * you can do:
	 *
	 * static int do_init = 1;
	 *
	 * if (do_init)
	 * {
	 *    do_init = 0;
	 *    ...
	 *    do_stuff_with_n_ptr
	 *    ...
	 * }
	 *
	 */

	if (buf[0] == 'N')
	{
    	return build_prob (buf + 2, n_ptr);
	}

 	/*
	 * If you want to do something after parsing the file you can add
	 * a special directive at the end of the txt file, like:
	 *
	 * else
	 * if (buf[0] == 'X')          (Only at the end of the txt file)
	 * {
	 *    ...
	 *    do_something_else_with_n_ptr
	 *    ...
	 * }
	 *
	 */
	else
	{
    	/* Oops */
    	return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
  	}
}


/*
 * Grab one flag in a special item_type from a textual string
 */
static bool grab_one_ego_item_flag(ego_item_type *ptr, cptr what)
{
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[TR1] = &(ptr->flags1);
	f[TR2] = &(ptr->flags2);
	f[TR3] = &(ptr->flags3);
	return grab_one_flag(f, "object", what);
}

/*
 * Initialize the "e_info" array, by parsing an ascii "template" file
 */
errr parse_e_info(char *buf, header *head)
{
	int i;

	char *s, *t;

	/* Current entry */
	static ego_item_type *e_ptr = NULL;

	static int cur_t = 0;


	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		e_ptr = (ego_item_type*)head->info_ptr + i;

		/* Store the name */
		if (!(e_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);

		/* Start with the first of the tval indices */
		cur_t = 0;
	}

	/* Process 'W' for "More Info" (one line only) */
	else if (buf[0] == 'W')
	{
		int level, rarity, max_level;
		long cost;

		/* There better be a current e_ptr */
		if (!e_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%ld",
			            &level, &rarity, &max_level, &cost)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		e_ptr->level = level;
		e_ptr->rarity = rarity;
		e_ptr->max_level = max_level;
		e_ptr->cost = cost;
	}

	/* Process 'T' for "Types allowed" (up to three lines) */
	else if (buf[0] == 'T')
	{
		int tval, sval1, sval2;

		/* There better be a current e_ptr */
		if (!e_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d",
			            &tval, &sval1, &sval2)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		e_ptr->tval[cur_t] = (byte)tval;
		e_ptr->min_sval[cur_t] = (byte)sval1;
		e_ptr->max_sval[cur_t] = (byte)sval2;

		/* Increase counter for 'possible tval' index */
		cur_t++;

		/* Allow only a limited number of T: lines */
		if (cur_t > EGO_TVALS_MAX) return (PARSE_ERROR_GENERIC);
	}

	/* Hack -- Process 'C' for "creation" */
	else if (buf[0] == 'C')
	{
		int max_att, to_dd, to_ds, max_evn, to_pd, to_ps, pv;

		/* There better be a current e_ptr */
		if (!e_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (7 != sscanf(buf+2, "%d:%d:%d:%d:%d:%d:%d",
			            &max_att, &to_dd, &to_ds, &max_evn, &to_pd, &to_ps, &pv)) return (PARSE_ERROR_GENERIC);

		e_ptr->max_att = max_att;
		e_ptr->to_dd = to_dd;
		e_ptr->to_ds = to_ds;
		e_ptr->max_evn = max_evn;
		e_ptr->to_pd = to_pd;
		e_ptr->to_ps = to_ps;
		e_ptr->max_pval = pv;
	}

	/* Process 'B' for "aBilities" (one line only) */
	else if (buf[0] == 'B')
	{
		int i;
		
		/* There better be a current e_ptr */
		if (!e_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* XXX Simply read each number following a colon */
		for (i = 0, s = buf+1; s && (s[0] == ':') && s[1]; ++i)
		{
			/* Sanity check */
			if (i > 3) return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);
			
			/* Default abilitynum */
			e_ptr->abilitynum[i] = 0;
			
			/* Store the skilltype */
			e_ptr->skilltype[i] = atoi(s+1);
			
			/* List this ability */
			e_ptr->abilities++;
			
			/* Find the slash */
			t = strchr(s+1, '/');
			
			/* Find the next colon */
			s = strchr(s+1, ':');
			
			/* If the slash is "nearby", use it */
			if (t && (!s || t < s))
			{
				int abilitynum = atoi(t+1);
				if (abilitynum > 0) e_ptr->abilitynum[i] = abilitynum;
			}
		}
	}

	/* Hack -- Process 'F' for flags */
	else if (buf[0] == 'F')
	{
		/* There better be a current e_ptr */
		if (!e_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while ((*t == ' ') || (*t == '|')) t++;
			}

			/* Parse this entry */
			if (0 != grab_one_ego_item_flag(e_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}

	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current e_ptr */
		if (!e_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&e_ptr->text, head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


/*
 * Grab one (basic) flag in a monster_race from a textual string
 */
static errr grab_one_basic_flag(monster_race *ptr, cptr what)
{
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[RF1] = &(ptr->flags1);
	f[RF2] = &(ptr->flags2);
	f[RF3] = &(ptr->flags3);
	return grab_one_flag(f, "monster", what);
}



/*
 * Grab one (spell) flag in a monster_race from a textual string
 */
static errr grab_one_spell_flag(monster_race *ptr, cptr what)
{
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[RF4] = &(ptr->flags4);
	return grab_one_flag(f, "monster", what);
}





/*
 * Initialize the "r_info" array, by parsing an ascii "template" file
 */
errr parse_r_info(char *buf, header *head)
{
	int i;

	char *s, *t;

	/* Current entry */
	static monster_race *r_ptr = NULL;


	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		r_ptr = (monster_race*)head->info_ptr + i;

		/* Store the name */
		if (!(r_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&(r_ptr->text), head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	/* Process 'G' for "Graphics" (one line only) */
	else if (buf[0] == 'G')
	{
		char d_char;
		int d_attr;

		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Paranoia */
		if (!buf[2]) return (PARSE_ERROR_GENERIC);
		if (!buf[3]) return (PARSE_ERROR_GENERIC);
		if (!buf[4]) return (PARSE_ERROR_GENERIC);

		/* Extract d_char */
		d_char = buf[2];

		/* If we have a longer string than expected ... */
		if (buf[5])
		{
			/* Advance "buf" on by 4 */
			buf += 4;

			/* Extract the colour */
			d_attr = color_text_to_attr(buf);
		}
		else
		{
			/* Extract the attr */
			d_attr = color_char_to_attr(buf[4]);
		}

		/* Paranoia */
		if (d_attr < 0) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		r_ptr->d_attr = d_attr;
		r_ptr->d_char = d_char;
	}

	/* Process 'I' for "Info" (one line only) */
	else if (buf[0] == 'I')
	{
		int spd, hp1, hp2, light;

		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the other values */
		if (4 != sscanf(buf+2, "%d:%dd%d:%d",
			            &spd, &hp1, &hp2, &light)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		r_ptr->speed = spd;
		r_ptr->hdice = hp1;
		r_ptr->hside = hp2;
		r_ptr->light = light;
	}

	/* Process 'W' for "More Info" (one line only) */
	else if (buf[0] == 'W')
	{
		int lev, rar;

		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (2 != sscanf(buf+2, "%d:%d",
			            &lev, &rar)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		r_ptr->level = lev;
		r_ptr->rarity = rar;
	}

	/* Process 'A' for "Alertness Info" (one line only) */
	else if (buf[0] == 'A')
	{
		int sleep, per, stl, wil;
		
		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%d",
			            &sleep, &per, &stl, &wil)) return (PARSE_ERROR_GENERIC);
		
		/* Save the values */
		r_ptr->sleep = sleep;
		r_ptr->per = per;
		r_ptr->stl = stl;
		r_ptr->wil = wil;
	}

	/* Process 'P' for "Protection Info" (one line only) */
	else if (buf[0] == 'P')
	{
		int evn, pd = 0, ps = 0, n;

		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
		
		/* Scan for the values */
		n = sscanf(buf+2, "[%d,%dd%d]", &evn, &pd, &ps);
		if ((n != 1) && (n != 3)) return (PARSE_ERROR_GENERIC);
		
//		if (3 != sscanf(buf+2, "[%d,%dd%d]",
//						&evn, &pd, &ps)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		r_ptr->evn = evn;
		r_ptr->pd = pd;
		r_ptr->ps = ps;
	}

	/* Process 'B' for "Blows" */
	else if (buf[0] == 'B')
	{
		int n1, n2, n;
		int att, dd, ds;

		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Find the next empty blow slot (if any) */
		for (i = 0; i < MONSTER_BLOW_MAX; i++) if (!r_ptr->blow[i].method) break;

		/* Oops, no more slots */
		if (i == MONSTER_BLOW_MAX) return (PARSE_ERROR_GENERIC);

		/* Analyze the first field */
		for (s = t = buf+2; *t && (*t != ':'); t++) /* loop */;

		/* Terminate the field (if necessary) */
		if (*t == ':') *t++ = '\0';

		/* Analyze the method */
		for (n1 = 0; r_info_blow_method[n1]; n1++)
		{
			if (streq(s, r_info_blow_method[n1])) break;
		}

		/* Invalid method */
		if (!r_info_blow_method[n1]) return (PARSE_ERROR_GENERIC);

		/* Analyze the second field */
		for (s = t; *t && (*t != ':'); t++) /* loop */;

		/* Terminate the field (if necessary) */
		if (*t == ':') *t++ = '\0';

		/* Analyze effect */
		for (n2 = 0; r_info_blow_effect[n2]; n2++)
		{
			if (streq(s, r_info_blow_effect[n2])) break;
		}

		/* Invalid effect */
		if (!r_info_blow_effect[n2]) return (PARSE_ERROR_GENERIC);

		// reset values
		dd = 0;
		ds = 0;
		
		n = sscanf(t, "(%d,%dd%d)", &att, &dd, &ds);
		if ((n != 1) && (n != 3)) return (PARSE_ERROR_GENERIC);

		//s = t;
		
		/* Scan for the values */
		//if (1 != sscanf(t, "(%d)", &att))
		//{
		//	t = s;
		//	if (3 != sscanf(t, "(%d,%dd%d)", &att, &dd, &ds)) return (PARSE_ERROR_GENERIC);
		//}

		/* Analyze the third field */
		//for (s = t; *t && (*t != 'd'); t++) /* loop */;

		/* Terminate the field (if necessary) */
		//if (*t == 'd') *t++ = '\0';

		/* Save the method */
		r_ptr->blow[i].method = n1;

		/* Save the effect */
		r_ptr->blow[i].effect = n2;

		/* Extract the damage dice and sides */
		r_ptr->blow[i].att = att;
		r_ptr->blow[i].dd = dd;
		r_ptr->blow[i].ds = ds;
	}

	/* Process 'F' for "Basic Flags" (multiple lines) */
	else if (buf[0] == 'F')
	{
		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while (*t == ' ' || *t == '|') t++;
			}

			/* Parse this entry */
			if (0 != grab_one_basic_flag(r_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}

	/* Process 'S' for "Spell Flags" (multiple lines) */
	else if (buf[0] == 'S')
	{
		/* There better be a current r_ptr */
		if (!r_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while ((*t == ' ') || (*t == '|')) t++;
			}

			/* XXX Hack -- Read spell frequency */
			if ((r_ptr->freq_ranged == 0) &&
			    (1 == sscanf(s, "SPELL_PCT_%d", &i)))
			{
				/* Sanity check */
				if ((i < 1) || (i > 100))
					return (PARSE_ERROR_INVALID_SPELL_FREQ);

				/* Extract a "frequency" */
				r_ptr->freq_ranged = i;

				/* Start at next entry */
				s = t;

				/* Continue */
				continue;
			}

			/* Read spell power. */
			if ((r_ptr->spell_power == 0) &&
			    (1 == sscanf(s, "POW_%d", &i)))
			{
				/* Save spell power. */
				r_ptr->spell_power = i;


				/* Start at next entry */
				s = t;

				/* Continue */
				continue;
			}

			/* Parse this entry */
			if (0 != grab_one_spell_flag(r_ptr, s))
				return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


/*
 * Grab one flag in a player_race from a textual string
 *
 * Sil:  these used to be the TR1, TR2 and TR3 flags,
 *       but we now use the race/house flags (RHF).
 */
static errr grab_one_race_flag(player_race *ptr, cptr what)
{	
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[RHF] = &(ptr->flags);
	return grab_one_flag(f, "player", what);
	
}

/*
 * Grab one flag in a player_house from a textual string
 *
 * Sil:  these used to be the TR1, TR2 and TR3 flags,
 *       but we now use the race/house flags (RHF).
 */
static errr grab_one_house_flag(player_house *ptr, cptr what)
{	
	u32b *f[MAX_FLAG_SETS];
	C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
	f[RHF] = &(ptr->flags);
	return grab_one_flag(f, "player", what);
	
}




/*
 * Initialize the "p_info" array, by parsing an ascii "template" file
 */
errr parse_p_info(char *buf, header *head)
{
	int i, j;

	char *s, *t;

	/* Current entry */
	static player_race *pr_ptr = NULL;
	static int cur_equip = 0;


	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		pr_ptr = (player_race*)head->info_ptr + i;

		/* Store the name */
		if (!(pr_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);
			
		cur_equip = 0;
	}

	/* Process 'S' for "Stats" (one line only) */
	else if (buf[0] == 'S')
	{
		int adj;

		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Start the string */
		s = buf+1;

		/* For each stat */
		for (j = 0; j < A_MAX; j++)
		{
			/* Find the colon before the subindex */
			s = strchr(s, ':');

			/* Verify that colon */
			if (!s) return (PARSE_ERROR_GENERIC);

			/* Nuke the colon, advance to the subindex */
			*s++ = '\0';

			/* Get the value */
			adj = atoi(s);

			/* Save the value */
			pr_ptr->r_adj[j] = adj;

			/* Next... */
			continue;
		}
	}

	/* Hack -- Process 'I' for "info" and such */
	else if (buf[0] == 'I')
	{
		int hist, b_age, m_age;

		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (3 != sscanf(buf+2, "%d:%d:%d",
			            &hist, &b_age, &m_age)) return (PARSE_ERROR_GENERIC);

		pr_ptr->hist = hist;
		pr_ptr->b_age = b_age;
		pr_ptr->m_age = m_age;
	}

	/* Hack -- Process 'H' for "Height" */
	else if (buf[0] == 'H')
	{
		int m_b_ht, m_m_ht, f_b_ht, f_m_ht;

		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%d",
			            &m_b_ht, &m_m_ht, &f_b_ht, &f_m_ht)) return (PARSE_ERROR_GENERIC);

		pr_ptr->m_b_ht = m_b_ht;
		pr_ptr->m_m_ht = m_m_ht;
		pr_ptr->f_b_ht = f_b_ht;
		pr_ptr->f_m_ht = f_m_ht;
	}

	/* Hack -- Process 'W' for "Weight" */
	else if (buf[0] == 'W')
	{
		int m_b_wt, m_m_wt, f_b_wt, f_m_wt;

		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%d",
			            &m_b_wt, &m_m_wt, &f_b_wt, &f_m_wt)) return (PARSE_ERROR_GENERIC);

		pr_ptr->m_b_wt = m_b_wt;
		pr_ptr->m_m_wt = m_m_wt;
		pr_ptr->f_b_wt = f_b_wt;
		pr_ptr->f_m_wt = f_m_wt;
	}

	/* Hack -- Process 'F' for flags */
	else if (buf[0] == 'F')
	{
		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while ((*t == ' ') || (*t == '|')) t++;
			}

			/* Parse this entry */
			if (0 != grab_one_race_flag(pr_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}

	/* Process 'E' for "Starting Equipment" */
	else if (buf[0] == 'E')
	{
		int tval, sval, min, max;

		start_item *e_ptr;

		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Access the item */
		e_ptr = &pr_ptr->start_items[cur_equip];

		/* Scan for the values */
		if (4 != sscanf(buf+2, "%d:%d:%d:%d",
			            &tval, &sval, &min, &max)) return (PARSE_ERROR_GENERIC);

		if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
			return (PARSE_ERROR_INVALID_ITEM_NUMBER);

		/* Save the values */
		e_ptr->tval = tval;
		e_ptr->sval = sval;
		e_ptr->min = min;
		e_ptr->max = max;

		/* Next item */
		cur_equip++;

		/* Limit number of starting items */
		if (cur_equip > MAX_START_ITEMS)
			return (PARSE_ERROR_GENERIC);
	}
	
	/* Hack -- Process 'C' for house choices */
	else if (buf[0] == 'C')
	{
		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while ((*t == ' ') || (*t == '|')) t++;
			}

			/* Hack - Parse this entry */
			pr_ptr->choice |= (1 << atoi(s));

			/* Start the next entry */
			s = t;
		}
	}

	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current pr_ptr */
		if (!pr_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&(pr_ptr->text), head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


/*
 * Initialize the "c_info" array, by parsing an ascii "template" file
 */
errr parse_c_info(char *buf, header *head)
{
	int i, j;

	char *s, *t;

	/* Current entry */
	static player_house *ph_ptr = NULL;

	static int cur_title = 0;
	static int cur_equip = 0;


	/* Process 'N' for "New/Number/Name" */
	if (buf[0] == 'N')
	{
		/* Find the colon before the name */
		s = strchr(buf+2, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Get the index */
		i = atoi(buf+2);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		ph_ptr = (player_house*)head->info_ptr + i;

		/* Store the name */
		if (!(ph_ptr->name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);

		/* No titles and equipment yet */
		cur_title = 0;
		cur_equip = 0;
	}
	
	/* Process 'A' for "Alternate Name" */
	else if (buf[0] == 'A')
	{
		/* Find the colon before the name */
		s = strchr(buf, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Store the name */
		if (!(ph_ptr->alt_name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	/* Process 'B' for "Short Name" */
	else if (buf[0] == 'B')
	{
		/* Find the colon before the name */
		s = strchr(buf, ':');

		/* Verify that colon */
		if (!s) return (PARSE_ERROR_GENERIC);

		/* Nuke the colon, advance to the name */
		*s++ = '\0';

		/* Paranoia -- require a name */
		if (!*s) return (PARSE_ERROR_GENERIC);

		/* Store the name */
		if (!(ph_ptr->short_name = add_name(head, s)))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	/* Process 'S' for "Stats" (one line only) */
	else if (buf[0] == 'S')
	{
		int adj;

		/* There better be a current ph_ptr */
		if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Start the string */
		s = buf+1;

		/* For each stat */
		for (j = 0; j < A_MAX; j++)
		{
			/* Find the colon before the subindex */
			s = strchr(s, ':');

			/* Verify that colon */
			if (!s) return (PARSE_ERROR_GENERIC);

			/* Nuke the colon, advance to the subindex */
			*s++ = '\0';

			/* Get the value */
			adj = atoi(s);

			/* Save the value */
			ph_ptr->h_adj[j] = adj;

			/* Next... */
			continue;
		}
	}

	/* Hack -- Process 'F' for flags */
	else if (buf[0] == 'F')
	{
		/* There better be a current pr_ptr */
		if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Parse every entry textually */
		for (s = buf + 2; *s; )
		{
			/* Find the end of this entry */
			for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */;

			/* Nuke and skip any dividers */
			if (*t)
			{
				*t++ = '\0';
				while ((*t == ' ') || (*t == '|')) t++;
			}

			/* Parse this entry */
			if (0 != grab_one_house_flag(ph_ptr, s)) return (PARSE_ERROR_INVALID_FLAG);

			/* Start the next entry */
			s = t;
		}
	}
	
	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current ph_ptr */
		if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&(ph_ptr->text), head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}



/*
 * Initialize the "h_info" array, by parsing an ascii "template" file
 */
errr parse_h_info(char *buf, header *head)
{
	int i;

	char *s;

	/* Current entry */
	static hist_type *h_ptr = NULL;


	/* Process 'N' for "New/Number" */
	if (buf[0] == 'N')
	{
		int prv, nxt, prc, hou;

		/* Hack - get the index */
		i = error_idx + 1;

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		h_ptr = (hist_type*)head->info_ptr + i;

		/* Scan for the values */
		if (4 != sscanf(buf, "N:%d:%d:%d:%d",
			            &prv, &nxt, &prc, &hou)) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		h_ptr->chart = prv;
		h_ptr->next = nxt;
		h_ptr->roll = prc;
		h_ptr->house = hou;
	}

	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current h_ptr */
		if (!h_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Get the text */
		s = buf+2;

		/* Store the text */
		if (!add_text(&h_ptr->text, head, s))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}
	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


/*
 * Initialize the "flavor_info" array, by parsing an ascii "template" file
 */
errr parse_flavor_info(char *buf, header *head)
{
	int i;

	/* Current entry */
	static flavor_type *flavor_ptr;


	/* Process 'N' for "Number" */
	if (buf[0] == 'N')
	{
		int tval, sval;
		int result;

		/* Scan the value */
		result = sscanf(buf, "N:%d:%d:%d", &i, &tval, &sval);

		/* Either two or three values */
		if ((result != 2) && (result != 3)) return (PARSE_ERROR_GENERIC);

		/* Verify information */
		if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

		/* Verify information */
		if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

		/* Save the index */
		error_idx = i;

		/* Point at the "info" */
		flavor_ptr = (flavor_type*)head->info_ptr + i;

		/* Save the tval */
		flavor_ptr->tval = (byte)tval;

		/* Save the sval */
		if (result == 2)
		{
			/* Megahack - unknown sval */
			flavor_ptr->sval = SV_UNKNOWN;
		}
		else
			flavor_ptr->sval = (byte)sval;
	}

	/* Process 'G' for "Graphics" */
	else if (buf[0] == 'G')
	{
		char d_char;
		int d_attr;

		/* There better be a current flavor_ptr */
		if (!flavor_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Paranoia */
		if (!buf[2]) return (PARSE_ERROR_GENERIC);
		if (!buf[3]) return (PARSE_ERROR_GENERIC);
		if (!buf[4]) return (PARSE_ERROR_GENERIC);

		/* Extract d_char */
		d_char = buf[2];

		/* If we have a longer string than expected ... */
		if (buf[5])
		{
			/* Advance "buf" on by 4 */
			buf += 4;

			/* Extract the colour */
			d_attr = color_text_to_attr(buf);
		}
		else
		{
			/* Extract the attr */
			d_attr = color_char_to_attr(buf[4]);
		}

		/* Paranoia */
		if (d_attr < 0) return (PARSE_ERROR_GENERIC);

		/* Save the values */
		flavor_ptr->d_attr = d_attr;
		flavor_ptr->d_char = d_char;
	}

	/* Process 'D' for "Description" */
	else if (buf[0] == 'D')
	{
		/* There better be a current flavor_ptr */
		if (!flavor_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

		/* Paranoia */
		if (!buf[1]) return (PARSE_ERROR_GENERIC);
		if (!buf[2]) return (PARSE_ERROR_GENERIC);

		/* Store the text */
		if (!add_text(&flavor_ptr->text, head, buf + 2))
			return (PARSE_ERROR_OUT_OF_MEMORY);
	}

	else
	{
		/* Oops */
		return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
	}

	/* Success */
	return (0);
}


#else	/* ALLOW_TEMPLATES */

#ifdef MACINTOSH
static int i = 0;
#endif

#endif	/* ALLOW_TEMPLATES */
