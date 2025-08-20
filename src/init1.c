/* File: init1.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "h-define.h"
#include "z-form.h" 
/* Forward declaration for init2 and local placement */
errr parse_style_levels(char* buf, header* head);

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
#include "metarun.h"

/* run-type template loader ---------------------------------------- */
header rt_head;       /* the one and only definition */

/*** Helper arrays for parsing ascii template files ***/

/*
 * Monster Blow Methods
 */
static cptr r_info_blow_method[]
    = { "", "HIT", "TOUCH", "XXX", "XXX", "CLAW", "BITE", "STING", "PECK",
          "WHIP", "XXX", "CRUSH", "ENGULF", "CRAWL", "THORN", "XXX", "XXX",
          "XXX", "XXX", "SPORE", "XXX", "XXX", "XXX", "XXX", "XXX", NULL };

/*
 * Monster Blow Effects
 */
static cptr r_info_blow_effect[] = { "", "HURT", "WOUND", "BATTER", "SHATTER",
    "UN_BONUS", "UN_POWER", "LOSE_MANA", "SLOW", "EAT_ITEM", "EAT_FOOD", "DARK",
    "HUNGER", "POISON", "ACID", "ELEC", "FIRE", "COLD", "BLIND", "CONFUSE",
    "TERRIFY", "ENTRANCE", "HALLU", "DISEASE", "LOSE_STR", "LOSE_DEX",
    "LOSE_CON", "LOSE_GRA", "LOSE_STR_CON", "LOSE_ALL", "DISARM", NULL };

#define TR1 0
#define TR2 1
#define TR3 2
#define RF1 3
#define RF2 4
#define RF3 5
#define RF4 6
#define RHF 7
#define VLT 8
#define CUR 9
#define UNQ 10
#define MAX_FLAG_SETS 11

/*
 * Monster race flags for the race_info_flags1 structure
 */
static flag_name info_flags[] = {

    /*
     * Monster race flags 2
     */

    { "UNIQUE", RF1, RF1_UNIQUE }, { "QUESTOR", RF1, RF1_QUESTOR },
    { "MALE", RF1, RF1_MALE }, { "FEMALE", RF1, RF1_FEMALE },
    { "CHAR_CLEAR", RF1, RF1_CHAR_CLEAR }, { "PEACEFUL", RF1, RF1_PEACEFUL },
    { "ATTR_CLEAR", RF1, RF1_ATTR_CLEAR },
    { "ATTR_MULTI", RF1, RF1_ATTR_MULTI },
    { "FORCE_DEPTH", RF1, RF1_FORCE_DEPTH },
    { "SPECIAL_GEN", RF1, RF1_SPECIAL_GEN }, { "FRIEND", RF1, RF1_FRIEND },
    { "FRIENDS", RF1, RF1_FRIENDS }, { "ESCORT", RF1, RF1_ESCORT },
    { "ESCORTS", RF1, RF1_ESCORTS },
    { "UNIQUE_FRIEND", RF1, RF1_UNIQUE_FRIEND },
    { "NEVER_BLOW", RF1, RF1_NEVER_BLOW },
    { "NEVER_MOVE", RF1, RF1_NEVER_MOVE },
    { "HIDDEN_MOVE", RF1, RF1_HIDDEN_MOVE }, { "RAND_25", RF1, RF1_RAND_25 },
    { "RAND_50", RF1, RF1_RAND_50 }, { "NO_CRIT", RF1, RF1_NO_CRIT },
    { "RES_CRIT", RF1, RF1_RES_CRIT }, { "DROP_33", RF1, RF1_DROP_33 },
    { "DROP_100", RF1, RF1_DROP_100 }, { "DROP_1D2", RF1, RF1_DROP_1D2 },
    { "DROP_2D2", RF1, RF1_DROP_2D2 }, { "DROP_3D2", RF1, RF1_DROP_3D2 },
    { "DROP_4D2", RF1, RF1_DROP_4D2 }, { "DROP_GOOD", RF1, RF1_DROP_GOOD },
    { "DROP_GREAT", RF1, RF1_DROP_GREAT },
    { "DROP_CHEST", RF1, RF1_DROP_CHEST },
    { "DROP_CHOSEN", RF1, RF1_DROP_CHOSEN },

    /*RF1 uber-flags*/
    { "DROP_UP_TO_10", RF1, RF1_DROP_UP_TO_10 },
    { "DROP_UP_TO_12", RF1, RF1_DROP_UP_TO_12 },
    { "DROP_UP_TO_14", RF1, RF1_DROP_UP_TO_14 },
    { "DROP_UP_TO_16", RF1, RF1_DROP_UP_TO_16 },
    { "DROP_UP_TO_18", RF1, RF1_DROP_UP_TO_18 },
    { "DROP_UP_TO_20", RF1, RF1_DROP_UP_TO_20 },

    /*
     * Monster race flags 2
     */

    { "MINDLESS", RF2, RF2_MINDLESS }, { "SMART", RF2, RF2_SMART },
    { "TERRITORIAL", RF2, RF2_TERRITORIAL },
    { "SHORT_SIGHTED", RF2, RF2_SHORT_SIGHTED },
    { "INVISIBLE", RF2, RF2_INVISIBLE }, { "GLOW", RF2, RF2_GLOW },
    { "CRUEL_BLOW", RF2, RF2_CRUEL_BLOW },
    { "EXCHANGE_PLACES", RF2, RF2_EXCHANGE_PLACES },
    { "MULTIPLY", RF2, RF2_MULTIPLY }, { "REGENERATE", RF2, RF2_REGENERATE },
    { "RIPOSTE", RF2, RF2_RIPOSTE }, { "FLANKING", RF2, RF2_FLANKING },
    { "CLOUD_SURROUND", RF2, RF2_CLOUD_SURROUND },
    { "FLYING", RF2, RF2_FLYING }, { "PASS_DOOR", RF2, RF2_PASS_DOOR },
    { "UNLOCK_DOOR", RF2, RF2_UNLOCK_DOOR },
    { "OPEN_DOOR", RF2, RF2_OPEN_DOOR }, { "BASH_DOOR", RF2, RF2_BASH_DOOR },
    { "PASS_WALL", RF2, RF2_PASS_WALL }, { "KILL_WALL", RF2, RF2_KILL_WALL },
    { "TUNNEL_WALL", RF2, RF2_TUNNEL_WALL },
    { "KILL_BODY", RF2, RF2_KILL_BODY }, { "TAKE_ITEM", RF2, RF2_TAKE_ITEM },
    { "KILL_ITEM", RF2, RF2_KILL_ITEM }, { "RF2XXX6", RF2, RF2_RF2XXX6 },
    { "LOW_MANA_RUN", RF2, RF2_LOW_MANA_RUN }, { "CHARGE", RF2, RF2_CHARGE },
    { "ELFBANE", RF2, RF2_ELFBANE }, { "KNOCK_BACK", RF2, RF2_KNOCK_BACK },
    { "CRIPPLING", RF2, RF2_CRIPPLING },
    { "OPPORTUNIST", RF2, RF2_OPPORTUNIST },
    { "ZONE_OF_CONTROL", RF2, RF2_ZONE_OF_CONTROL },

    /*
     * Monster race flags 3
     */

    { "ORC", RF3, RF3_ORC }, { "TROLL", RF3, RF3_TROLL },
    { "SERPENT", RF3, RF3_SERPENT }, { "DRAGON", RF3, RF3_DRAGON },
    { "RAUKO", RF3, RF3_RAUKO }, { "UNDEAD", RF3, RF3_UNDEAD },
    { "SPIDER", RF3, RF3_SPIDER }, { "WOLF", RF3, RF3_WOLF },
    { "MAN", RF3, RF3_MAN }, { "ELF", RF3, RF3_ELF },
    { "RF3XXX3", RF3, RF3_RF3XXX3 }, { "RF3XXX4", RF3, RF3_RF3XXX4 },
    { "HURT_LITE", RF3, RF3_HURT_LITE }, { "STONE", RF3, RF3_STONE },
    { "HURT_FIRE", RF3, RF3_HURT_FIRE }, { "HURT_COLD", RF3, RF3_HURT_COLD },
    { "RF3XXX5", RF3, RF3_RF3XXX5 }, { "RES_ELEC", RF3, RF3_RES_ELEC },
    { "RES_FIRE", RF3, RF3_RES_FIRE }, { "RES_COLD", RF3, RF3_RES_COLD },
    { "RES_POIS", RF3, RF3_RES_POIS }, { "RF3XXX6", RF3, RF3_RF3XXX6 },
    { "RES_NETHR", RF3, RF3_RES_NETHR }, { "RES_WATER", RF3, RF3_RES_WATER },
    { "RES_PLAS", RF3, RF3_RES_PLAS }, { "RES_NEXUS", RF3, RF3_RES_NEXUS },
    { "RES_DISEN", RF3, RF3_RES_DISEN }, { "NO_SLOW", RF3, RF3_NO_SLOW },
    { "NO_FEAR", RF3, RF3_NO_FEAR }, { "NO_STUN", RF3, RF3_NO_STUN },
    { "NO_CONF", RF3, RF3_NO_CONF }, { "NO_SLEEP", RF3, RF3_NO_SLEEP },

    /*RF3 uber-flags*/
    { "RES_ELEM", RF3, RF3_RES_ELEM }, { "RES_ALL", RF3, RF3_RES_ALL },
    { "NO_CHARM", RF3, RF3_NO_CHARM },

    /*
     * Object flags 1
     */
    /*
     * Monster race flags 4
     */

    { "ARROW1", RF4, RF4_ARROW1 }, { "ARROW2", RF4, RF4_ARROW2 },
    { "BOULDER", RF4, RF4_BOULDER }, { "BRTH_FIRE", RF4, RF4_BRTH_FIRE },
    { "BRTH_COLD", RF4, RF4_BRTH_COLD }, { "BRTH_POIS", RF4, RF4_BRTH_POIS },
    { "BRTH_DARK", RF4, RF4_BRTH_DARK }, { "EARTHQUAKE", RF4, RF4_EARTHQUAKE },
    { "SHRIEK", RF4, RF4_SHRIEK }, { "SCREECH", RF4, RF4_SCREECH },
    { "DARKNESS", RF4, RF4_DARKNESS }, { "FORGET", RF4, RF4_FORGET },
    { "SCARE", RF4, RF4_SCARE }, { "CONF", RF4, RF4_CONF },
    { "HOLD", RF4, RF4_HOLD }, { "SLOW", RF4, RF4_SLOW },
    { "HATCH_SPIDER", RF4, RF4_HATCH_SPIDER }, { "DIM", RF4, RF4_DIM },
    { "SNG_BINDING", RF4, RF4_SNG_BINDING },
    { "SNG_PIERCING", RF4, RF4_SNG_PIERCING },
    { "SNG_OATHS", RF4, RF4_SNG_OATHS },

    { "RF4XXX22", RF4, RF4_RF4XXX22 }, { "RF4XXX23", RF4, RF4_RF4XXX23 },
    { "THROW_WEB", RF4, RF4_THROW_WEB }, { "RALLY", RF4, RF4_RALLY },
    { "RF4XXX26", RF4, RF4_RF4XXX26 }, { "RF4XXX27", RF4, RF4_RF4XXX27 },
    { "RF4XXX28", RF4, RF4_RF4XXX28 }, { "RF4XXX29", RF4, RF4_RF4XXX29 },
    { "RF4XXX30", RF4, RF4_RF4XXX30 }, { "RF4XXX31", RF4, RF4_RF4XXX31 },
    { "RF4XXX32", RF4, RF4_RF4XXX32 },

    /*object_flags*/

    { "STR", TR1, TR1_STR }, { "DEX", TR1, TR1_DEX }, { "CON", TR1, TR1_CON },
    { "GRA", TR1, TR1_GRA }, { "NEG_STR", TR1, TR1_NEG_STR },
    { "NEG_DEX", TR1, TR1_NEG_DEX }, { "NEG_CON", TR1, TR1_NEG_CON },
    { "NEG_GRA", TR1, TR1_NEG_GRA }, { "MELEE", TR1, TR1_MEL },
    { "ARCHERY", TR1, TR1_ARC }, { "STEALTH", TR1, TR1_STL },
    { "PERCEPTION", TR1, TR1_PER }, { "WILL", TR1, TR1_WIL },
    { "SMITHING", TR1, TR1_SMT }, { "SONG", TR1, TR1_SNG },
    { "DAMAGE_SIDES", TR1, TR1_DAMAGE_SIDES }, { "TUNNEL", TR1, TR1_TUNNEL },
    { "SHARPNESS", TR1, TR1_SHARPNESS }, { "SHARPNESS2", TR1, TR1_SHARPNESS2 },
    { "VAMPIRIC", TR1, TR1_VAMPIRIC }, { "SLAY_ORC", TR1, TR1_SLAY_ORC },
    { "SLAY_TROLL", TR1, TR1_SLAY_TROLL }, { "SLAY_WOLF", TR1, TR1_SLAY_WOLF },
    { "SLAY_SPIDER", TR1, TR1_SLAY_SPIDER },
    { "SLAY_UNDEAD", TR1, TR1_SLAY_UNDEAD },
    { "SLAY_RAUKO", TR1, TR1_SLAY_RAUKO },
    { "SLAY_DRAGON", TR1, TR1_SLAY_DRAGON },
    { "SLAY_MAN_OR_ELF", TR1, TR1_SLAY_MAN_OR_ELF },
    { "BRAND_ELEC", TR1, TR1_BRAND_ELEC },
    { "BRAND_FIRE", TR1, TR1_BRAND_FIRE },
    { "BRAND_COLD", TR1, TR1_BRAND_COLD },
    { "BRAND_POIS", TR1, TR1_BRAND_POIS },

    /*TR1 Uber-flags*/
    { "ALL_STATS", TR1, TR1_ALL_STATS },

    /*
     * Object flags 2
     */

    { "SUST_STR", TR2, TR2_SUST_STR }, { "SUST_DEX", TR2, TR2_SUST_DEX },
    { "SUST_CON", TR2, TR2_SUST_CON }, { "SUST_GRA", TR2, TR2_SUST_GRA },
    { "RES_ELEC", TR2, TR2_RES_ELEC }, { "RES_FIRE", TR2, TR2_RES_FIRE },
    { "RES_COLD", TR2, TR2_RES_COLD }, { "RES_POIS", TR2, TR2_RES_POIS },
    { "RES_BLEED", TR2, TR2_RES_BLEED }, { "RES_FEAR", TR2, TR2_RES_FEAR },
    { "RES_BLIND", TR2, TR2_RES_BLIND }, { "RES_CONFU", TR2, TR2_RES_CONFU },
    { "RES_STUN", TR2, TR2_RES_STUN }, { "RES_HALLU", TR2, TR2_RES_HALLU },
    { "RADIANCE", TR2, TR2_RADIANCE }, { "SLOW_DIGEST", TR2, TR2_SLOW_DIGEST },
    { "LIGHT", TR2, TR2_LIGHT }, { "REGEN", TR2, TR2_REGEN },
    { "SEE_INVIS", TR2, TR2_SEE_INVIS }, { "FREE_ACT", TR2, TR2_FREE_ACT },
    { "TRAITOR", TR2, TR2_TRAITOR }, { "SPEED", TR2, TR2_SPEED },
    { "FEAR", TR2, TR2_FEAR }, { "HUNGER", TR2, TR2_HUNGER },
    { "DARKNESS", TR2, TR2_DARKNESS }, { "SLOWNESS", TR2, TR2_SLOWNESS },
    { "DANGER", TR2, TR2_DANGER }, { "AGGRAVATE", TR2, TR2_AGGRAVATE },
    { "HAUNTED", TR2, TR2_HAUNTED }, { "VUL_COLD", TR2, TR2_VUL_COLD },
    { "VUL_FIRE", TR2, TR2_VUL_FIRE }, { "VUL_POIS", TR2, TR2_VUL_POIS },

    /*TR2 Uber-flags*/
    { "RESISTANCE", TR2, TR2_RESISTANCE },

    /*
     * Object flags 3
     */

    { "DAMAGED", TR3, TR3_DAMAGED }, { "CHEAT_DEATH", TR3, TR3_CHEAT_DEATH },
    { "STAND_FAST", TR3, TR3_STAND_FAST }, { "ACCURATE", TR3, TR3_ACCURATE },
    { "CUMBERSOME", TR3, TR3_CUMBERSOME },
    { "AVOID_TRAPS", TR3, TR3_AVOID_TRAPS }, { "MEDIC", TR3, TR3_MEDIC },
    { "TR3XXX6", TR3, TR3_TR3XXX6 }, { "TR3XXX7", TR3, TR3_TR3XXX7 },
    { "TR3XXX8", TR3, TR3_TR3XXX8 }, { "TR3XXX9", TR3, TR3_TR3XXX9 },
    { "TR3XX10", TR3, TR3_TR3XX10 }, { "NO_SMITHING", TR3, TR3_NO_SMITHING },
    { "MITHRIL", TR3, TR3_MITHRIL }, { "AXE", TR3, TR3_AXE },
    { "POLEARM", TR3, TR3_POLEARM }, { "IGNORE_ACID", TR3, TR3_IGNORE_ACID },
    { "IGNORE_ELEC", TR3, TR3_IGNORE_ELEC },
    { "IGNORE_FIRE", TR3, TR3_IGNORE_FIRE },
    { "IGNORE_COLD", TR3, TR3_IGNORE_COLD }, { "THROWING", TR3, TR3_THROWING },
    { "ENCHANTABLE", TR3, TR3_ENCHANTABLE }, { "ACTIVATE", TR3, TR3_ACTIVATE },
    { "INSTA_ART", TR3, TR3_INSTA_ART }, { "EASY_KNOW", TR3, TR3_EASY_KNOW },
    { "MORE_SPECIAL", TR3, TR3_MORE_SPECIAL },
    { "TR3XXX12", TR3, TR3_TR3XXX12 },
    { "HAND_AND_A_HALF", TR3, TR3_HAND_AND_A_HALF },
    { "TWO_HANDED", TR3, TR3_TWO_HANDED },
    { "LIGHT_CURSE", TR3, TR3_LIGHT_CURSE },
    { "HEAVY_CURSE", TR3, TR3_HEAVY_CURSE },
    { "PERMA_CURSE", TR3, TR3_PERMA_CURSE },

    { "IGNORE_ALL", TR3, TR3_IGNORE_ALL },

    /*
     * Race/House flags
     */
    { "BOW_PROFICIENCY", RHF, RHF_BOW_PROFICIENCY },
    { "AXE_PROFICIENCY", RHF, RHF_AXE_PROFICIENCY },
    { "MEL_AFFINITY", RHF, RHF_MEL_AFFINITY },
    { "MEL_PENALTY", RHF, RHF_MEL_PENALTY },
    { "ARC_AFFINITY", RHF, RHF_ARC_AFFINITY },
    { "ARC_PENALTY", RHF, RHF_ARC_PENALTY },
    { "EVN_AFFINITY", RHF, RHF_EVN_AFFINITY },
    { "EVN_PENALTY", RHF, RHF_EVN_PENALTY },
    { "STL_AFFINITY", RHF, RHF_STL_AFFINITY },
    { "STL_PENALTY", RHF, RHF_STL_PENALTY },
    { "PER_AFFINITY", RHF, RHF_PER_AFFINITY },
    { "PER_PENALTY", RHF, RHF_PER_PENALTY },
    { "WIL_AFFINITY", RHF, RHF_WIL_AFFINITY },
    { "WIL_PENALTY", RHF, RHF_WIL_PENALTY },
    { "SMT_AFFINITY", RHF, RHF_SMT_AFFINITY },
    { "SMT_PENALTY", RHF, RHF_SMT_PENALTY },
    { "SNG_AFFINITY", RHF, RHF_SNG_AFFINITY },
    { "SNG_PENALTY", RHF, RHF_SNG_PENALTY },
    { "GIFTERU", RHF, RHF_GIFTERU }, { "KINSLAYER", RHF, RHF_KINSLAYER },
    { "CURSE", RHF, RHF_CURSE }, { "TREACHERY", RHF, RHF_TREACHERY },
    { "FREE", RHF, RHF_FREE }, { "MOR_CURSE", RHF, RHF_MOR_CURSE },
    { "RHFXXX25", RHF, RHF_RHFXXX26 }, { "RHFXXX26", RHF, RHF_RHFXXX27 },
    { "RHFXXX27", RHF, RHF_RHFXXX28 }, { "RHFXXX28", RHF, RHF_RHFXXX29 },
    { "RHFXXX29", RHF, RHF_RHFXXX29 }, { "RHFXXX30", RHF, RHF_RHFXXX30 },
    { "RHFXXX31", RHF, RHF_RHFXXX31 }, { "RHFXXX32", RHF, RHF_RHFXXX32 },

    /*
     * Vault flags
     */
    { "TEST", VLT, VLT_TEST }, { "NO_ROTATION", VLT, VLT_NO_ROTATION },
    { "TRAPS", VLT, VLT_TRAPS }, { "WEBS", VLT, VLT_WEBS },
    { "LIGHT", VLT, VLT_LIGHT }, { "SURFACE", VLT, VLT_SURFACE },
    { "VLTXXXX7", VLT, VLT_VLTXXXX7 }, { "VLTXXXX8", VLT, VLT_VLTXXXX8 },
    { "VLTXXXX9", VLT, VLT_VLTXXXX9 }, { "VLTXXX10", VLT, VLT_VLTXXX10 },
    { "VLTXXX11", VLT, VLT_VLTXXX11 }, { "VLTXXX12", VLT, VLT_VLTXXX12 },
    { "VLTXXX13", VLT, VLT_VLTXXX13 }, { "VLTXXX14", VLT, VLT_VLTXXX14 },
    { "VLTXXX15", VLT, VLT_VLTXXX15 }, { "VLTXXX16", VLT, VLT_VLTXXX16 },
    { "VLTXXX17", VLT, VLT_VLTXXX17 }, { "VLTXXX18", VLT, VLT_VLTXXX18 },
    { "VLTXXX19", VLT, VLT_VLTXXX19 }, { "VLTXXX20", VLT, VLT_VLTXXX20 },
    { "VLTXXX21", VLT, VLT_VLTXXX21 }, { "VLTXXX22", VLT, VLT_VLTXXX22 },
    { "VLTXXX23", VLT, VLT_VLTXXX23 }, { "VLTXXX24", VLT, VLT_VLTXXX24 },
    { "VLTXXX21", VLT, VLT_VLTXXX25 }, { "VLTXXX25", VLT, VLT_VLTXXX26 },
    { "VLTXXX26", VLT, VLT_VLTXXX27 }, { "VLTXXX27", VLT, VLT_VLTXXX28 },
    { "VLTXXX28", VLT, VLT_VLTXXX29 }, { "VLTXXX29", VLT, VLT_VLTXXX29 },
    { "VLTXXX30", VLT, VLT_VLTXXX30 }, { "VLTXXX31", VLT, VLT_VLTXXX31 },
    { "VLTXXX32", VLT, VLT_VLTXXX32 },

    /*
     * Curse flags
     */

    { "NOCHOICE", CUR, CUR_NOCHOICE }, { "WEAK", CUR, CUR_WEAK }, 
    { "MONSTERHP", CUR, CUR_MON_HP }, { "MONSTERHP_U", CUR, CUR_U_MON_HP },
    { "MON_STL", CUR, CUR_MON_STL }, { "MON_PER", CUR, CUR_MON_PER },
    { "MON_WIL", CUR, CUR_MON_WIL }, { "MON_ARM_DICE", CUR, CUR_MON_ARM_DICE },
    { "MON_ARM_SIDE", CUR, CUR_MON_ARM_SIDE }, {"NOSTART", CUR, CUR_NOSTART}, 
    {"SMITHCURSE", CUR, CUR_SMITHCURSE}, {"FINDCURSE", CUR, CUR_FINDCURSE},
    {"LIGHTR", CUR, CUR_LIGHTR},{"LIGHTP", CUR, CUR_LIGHTP},
    {"DEATH", CUR, CUR_DEATH},{"TRAPS", CUR, CUR_TRAPS},
    {"MON_NUM", CUR, CUR_MON_NUM},{"HUNGER", CUR, CUR_HUNGER},
    { "HALLU", CUR, CUR_HALLU},
    
    // Unique flags
    {"EARENDIL", UNQ, UNQ_EARENDIL}, { "SMT_FEANOR", UNQ, UNQ_SMT_FEANOR },
    { "WIL_FIN", UNQ, UNQ_WIL_FIN }, { "SNG_FIN", UNQ, UNQ_SNG_FIN },
    { "SNG_LUT", UNQ, UNQ_SNG_LUT }, { "WIL_TUOR", UNQ, UNQ_WIL_TUOR },
    { "SNG_MEL", UNQ, UNQ_SNG_MEL }, { "SMT_TELCHAR", UNQ, UNQ_SMT_TELCHAR },
    { "SMT_GAMIL", UNQ, UNQ_SMT_GAMIL }, { "SNG_HURIN", UNQ, UNQ_SNG_HURIN },
    { "SNG_THINGOL", UNQ, UNQ_SNG_THINGOL }, { "MIM", UNQ, UNQ_MIM },
    { "SMT_EOL", UNQ, UNQ_SMT_EOL }, { "MEL_MAEDHROS", UNQ, UNQ_MEL_MAEDHROS }, { "WIL_TURIN", UNQ, UNQ_WIL_TURIN }

};


/*
 * Activation type
 */
static cptr a_info_act[ACT_MAX] = { "ILLUMINATION", "MAGIC_MAP", "CLAIRVOYANCE",
    "PROT_EVIL", "DISP_EVIL", "HEAL1", "HEAL2", "CURE_WOUNDS", "HASTE1",
    "HASTE2", "FIRE1", "FIRE2", "FIRE3", "FROST1", "FROST2", "FROST3", "FROST4",
    "FROST5", "ACID1", "RECHARGE1", "SLEEP", "LIGHTNING_BOLT", "ELEC2",
    "BANISHMENT", "MASS_BANISHMENT", "IDENTIFY_FULLY", "DRAIN_LIFE1",
    "DRAIN_LIFE2", "BIZZARE", "STAR_BALL", "RAGE_BLESS_RESIST", "PHASE",
    "TRAP_DOOR_DEST", "DETECT", "RESIST", "TELEPORT", "RESTORE_VOICE",
    "MISSILE", "ARROW", "REM_FEAR_POIS", "STINKING_CLOUD", "STONE_TO_MUD",
    "TELE_AWAY", "WOR", "CONFUSE", "PROBE", "FIREBRAND", "STARLIGHT",
    "MANA_BOLT", "BERSERKER", "RES_ACID", "RES_ELEC", "RES_FIRE", "RES_COLD",
    "RES_POIS" };
    
/*** Initialize from ascii template files ***/

/*
 * Initialize an "*_info" array, by parsing an ascii "template" file
 */
errr init_info_txt(
    FILE* fp, char* buf, header* head, parse_info_txt_func parse_info_txt_line)
{
    errr err;

    /* Not ready yet */
    bool okay = false;

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
        if (!buf[0] || (buf[0] == '#'))
            continue;

        /* Verify correct "colon" format */
        if (buf[1] != ':')
            return (PARSE_ERROR_GENERIC);

        /* Hack -- Process 'V' for "Version" */
        if (buf[0] == 'V')
        {
            int v1, v2, v3;

            /* Scan for the values */
            if ((3 != sscanf(buf + 2, "%d.%d.%d", &v1, &v2, &v3))
                || (v1 != head->v_major) || (v2 != head->v_minor)
                || (v3 != head->v_patch))
            {
                return (PARSE_ERROR_OBSOLETE_FILE);
            }

            /* Okay to proceed */
            okay = true;

            /* Continue */
            continue;
        }

        /* No version yet */
        if (!okay)
            return (PARSE_ERROR_OBSOLETE_FILE);

        /* Parse the line */
        if ((err = (*parse_info_txt_line)(buf, head)) != 0)
            return (err);
    }

    /* Complete the "name" and "text" sizes */
    if (head->name_size)
        head->name_size++;
    if (head->text_size)
        head->text_size++;

    /* No version yet */
    if (!okay)
        return (PARSE_ERROR_OBSOLETE_FILE);

    /* Success */
    return (0);
}

/* ---- helpers ------------------------------------------------------ */
static const char *rank_name(int lvl)            /* -2…+2 → text */
{
    switch (lvl) {
        case  2: return "Mastery";
        case  1: return "Affinity";
        case  0: return "";
        case -1: return "Penalty";
        case -2: return "Grand Penalty";
    }
    return "";
}

/* ------------------------------------------------------------------ *
 *  combined_level() – return the net Affinity/Mastery/Penalty value
 *  (-2 … +2) for one skill, summing contributions from
 *       • race-or-house RHF flags (counted once even if both set)
 *       • active curses  (each curse can add its own +1 / –1)
 *  Affinity = +1, Penalty = –1, Mastery/Grand Penalty = ±2.
 * ------------------------------------------------------------------ */
static int combined_level(int skill)
{
    /*  lookup table: [S_*] → {AFFINITY-bit, PENALTY-bit}  */
    static const struct { u32b aff, pen; } tbl[] = {
        { RHF_MEL_AFFINITY, RHF_MEL_PENALTY },   /* S_MEL = 0 */
        { RHF_ARC_AFFINITY, RHF_ARC_PENALTY },   /* S_ARC */
        { RHF_EVN_AFFINITY, RHF_EVN_PENALTY },   /* S_EVN */
        { RHF_STL_AFFINITY, RHF_STL_PENALTY },   /* S_STL */
        { RHF_PER_AFFINITY, RHF_PER_PENALTY },   /* S_PER */
        { RHF_WIL_AFFINITY, RHF_WIL_PENALTY },   /* S_WIL */
        { RHF_SMT_AFFINITY, RHF_SMT_PENALTY },   /* S_SMT */
        { RHF_SNG_AFFINITY, RHF_SNG_PENALTY }    /* S_SNG */
    };

    /* masks --------------------------------------------------------- */
    u32b rhf  = p_info[p_ptr->prace].flags |      /* race OR house      */
                c_info[p_ptr->phouse].flags;
    u32b cur  = curse_flag_mask();                /* all active curses  */

    /* tally --------------------------------------------------------- */
    int v = 0;
    if (rhf & tbl[skill].aff)  v++;
    if (rhf & tbl[skill].pen)  v--;
    if (cur & tbl[skill].aff)  v++;               /* curse stack        */
    if (cur & tbl[skill].pen)  v--;

    /* clamp to –2 … +2 as per spec ---------------------------------- */
    if (v >  2) v =  2;
    if (v < -2) v = -2;
    return v;
}

static byte rank_colour(int lvl)         /* –2 … +2 */
{
    if (lvl ==  2) return TERM_L_GREEN;     /* Mastery        */
    if (lvl ==  1) return TERM_GREEN;   /* Affinity       */
    if (lvl == -1) return TERM_RED;     /* Penalty        */
    if (lvl == -2) return TERM_L_RED;       /* Grand penalty  */
    return TERM_WHITE;                    /* Neutral / ?    */
}

/* short names for on-screen tokens */
static const char *skill_tag(int s)
{
    static const char *tags[] =
        { "MEL", "ARC", "EVN", "STL", "PER", "WIL", "SMT", "SNG" };
    return tags[s];
}


/*-----------------------------------------------------------------*
 *  dbg_show_active_flags() – show only the flags that are ON
 *  for the current character (i386-safe C89).
 *-----------------------------------------------------------------*/
void dbg_show_active_flags(void)
{
    player_race  *rp_ptr = &p_info[p_ptr->prace];
    player_house *hp_ptr = &c_info[p_ptr->phouse];

    /* live masks --------------------------------------------------- */
    u32b rhf_bits  = rp_ptr->flags | hp_ptr->flags;  /* race OR house  */
    u32b unq_bits  = hp_ptr->flags_u;                /* house-unique   */

    struct { int set; u32b bits; cptr tag; byte clr; } grp[] = {
        { RHF, rhf_bits, "RHF (Race/House flags)",   TERM_L_GREEN },
        { UNQ, unq_bits, "UNQ (Unique-house flags)", TERM_L_BLUE  },
        { CUR, 0,        "CUR (Curse flags)",        TERM_L_RED   },
    };

    const int BUF_LEN = 79;           /* 80-col safety                */
    char  buf[BUF_LEN + 1];
    int   row = 2;

    Term_clear();
#ifdef DEBUG_CURSES
    Term_putstr(0, row++, -1, TERM_YELLOW,
                "*** DEBUG: meta-run DEBUG - a:add-curse  c:clear-all  e:+death  1-3:+sils  any:key:exit ***");
#endif

    for (size_t g = 0; g < N_ELEMENTS(grp); g++)
    {
        Term_putstr(0, row++, -1, grp[g].clr, grp[g].tag);

        buf[0] = '\0';
        size_t pos = 0;

        for (size_t i = 0; i < N_ELEMENTS(info_flags); i++)
        {
            flag_name *f = info_flags + i;
            if (f->set != grp[g].set) continue;

            /* -------- check whether THIS bit is on -------- */
            bool present = false;
            if (f->set == CUR)
                present = (curse_flag_count_cur(f->flag) > 0);
            else
                present = (grp[g].bits & f->flag) != 0;
            if (!present) continue;
            /* ---------------------------------------------- */

            size_t need = (pos ? 2 : 0) + strlen(f->name);
            if (pos + need > BUF_LEN)
            {
                Term_putstr(0, row++, -1, TERM_WHITE, buf);
                pos = 0;  buf[0] = '\0';
            }

            if (pos) { buf[pos++] = ','; buf[pos++] = ' '; }
            memcpy(buf + pos, f->name, need - (pos ? 2 : 0));
            pos += strlen(f->name);
            buf[pos] = '\0';
        }

        if (pos) Term_putstr(0, row++, -1, TERM_WHITE, buf);
        row++;                      /* blank line between groups */
    }


/* ---------- RHF flags coming specifically from active curses --------- */
{
    Term_putstr(0, row++, -1, TERM_ORANGE,
                "CUR  RHF (RHF flags coming from active curses)");

    const int BUF_LEN = 79;
    char buf[BUF_LEN + 1]; buf[0] = '\0';
    size_t pos = 0;

    for (size_t i = 0; i < N_ELEMENTS(info_flags); i++)
    {
        flag_name *f = info_flags + i;
        if (f->set != RHF) continue;
        if (curse_flag_count_rhf(f->flag) <= 0) continue;

        size_t need = (pos ? 2 : 0) + strlen(f->name);
        if (pos + need > BUF_LEN)
        {
            Term_putstr(0, row++, -1, TERM_WHITE, buf);
            pos = 0; buf[0] = '\0';
        }
        if (pos) { buf[pos++] = ','; buf[pos++] = ' '; }
        memcpy(buf + pos, f->name, need - (pos ? 2 : 0));
        pos += strlen(f->name);
        buf[pos] = '\0';
    }

    if (pos) Term_putstr(0, row++, -1, TERM_WHITE, buf);
    row++;  /* blank line */
}

/* ---------- RHF Mastery / Penalty summary ------------------------- */
{
    Term_putstr(0, row++, -1, TERM_SLATE, "Combined skill ranks:");

    int col = 2;                              /* left margin */
    for (int s = 0; s < S_MAX; s++)
    {
        int lvl = combined_level(s);
        if (!lvl) continue;                   /* skip neutral */

        char tok[20];
        strnfmt(tok, sizeof tok, "%s:%s", skill_tag(s), rank_name(lvl));

        /* wrap at column 78: */
        if (col + (int)strlen(tok) > 78) { row++; col = 2; }

        Term_putstr(col, row, -1, rank_colour(lvl), tok);
        col += strlen(tok);

        /* comma + space unless last item */
        if (s < S_MAX - 1) {
            Term_putch(col++, row, TERM_WHITE, ',');
            Term_putch(col++, row, TERM_WHITE, ' ');
        }
    }
    row += 2;                                 /* blank line */
}


/* =====================  active curses  =========================== */
{
    Term_putstr(0, row++, -1, TERM_L_RED,
                "Active curses (id : name [stacks])");

    for (int id = 0; id < z_info->cu_max; id++)
    {
        byte cnt = CURSE_GET(id);
        if (!cnt) continue;                       /* skip empty slots */

#ifdef DEBUG_CURSES
        /* Show the P: effect text and use a slightly shorter name field
           so the whole line fits on an 80-col screen.                 */
        const char *pow = cu_text + cu_info[id].power;   /* effect blurb */
        c_put_str(TERM_WHITE,
                  format("  %2d : %-26s [%d]  – %s",
                         id,
                         cu_name + cu_info[id].name,
                         cnt,
                         pow),
                  row++, 2);
#else
        /* Release build: classic 3 columns, no effect text */
        c_put_str(TERM_WHITE,
                  format("  %2d : %-30s [%d]",
                         id,
                         cu_name + cu_info[id].name,
                         cnt),
                  row++, 2);
#endif
    }
    row++;                                          /* blank line */

#ifdef DEBUG_CURSES
    Term_putstr(0, row++, -1, TERM_L_DARK,
                "[a] add random curse   [x] clear all   [e] +death   [1-3] escape   [any other] quit");
#else
    Term_putstr(0, row++, -1, TERM_L_DARK,
                "[a] add random curse   [any key] quit");
#endif
}

/* ----------------------------------------------------------------- */

/* ===================  key-handling loop  ========================= */
    while (true)
    {
        int ch = inkey();

        if (ch == 'a')            /* add one random curse */
        {
            int id = menu_choose_one_curse(0);
            add_curse_stack(id);
            dbg_show_active_flags();          /* redraw */
            break;
        }
#ifdef DEBUG_CURSES
        else if (ch == 'x')       /* clear all curses */
        {
            if (get_check("Erase ALL curses for this meta-run? "))
            {
                metarun_clear_all_curses();
                dbg_show_active_flags();
                break;
            }
            continue;
        }
        else if (ch == 'e')       /* +1 death shortcut */
        {
            metarun_update_on_exit(true,false, 0);
            dbg_show_active_flags();
            break;
        }
        else if (ch == 'r')       /* clear scores */
        {
            clear_scorefile();
            dbg_show_active_flags();
            break;
        }
        else if (ch >= '1' && ch <= '3')  /* +n Silmarils */
        {
            do_cmd_escape((byte)(ch - '0'));
            dbg_show_active_flags();
            break;
        }
        else if (ch == 'd')  /* debug menu */
        {
            
        /* If the player opens the debug menu from debug curses UI from the character sheet,
        * make sure the save is marked as tempered/debug (noscore 0x0008)
        * so metarun finalization will purge it just like the Ctrl-A debug path.
        */
        if (!(p_ptr->noscore & 0x0008)) {
            p_ptr->noscore |= 0x0008;
            log_info("Debug curses UI enabled (noscore=0x%04X, savefile='%s')",
                    (unsigned)p_ptr->noscore, savefile);
        }
            do_cmd_debug();
            dbg_show_active_flags();
            break;
        }
#endif
         break;                    /* any other key exits */
     }
/* ================================================================ */
}



/*
 * Add a text to the text-storage and store offset to it.
 *
 * Returns false when there isn't enough space available to store
 * the text.
 */
static bool add_text(u32b* offset, header* head, cptr buf)
{
    /* Hack -- Verify space */
    if (head->text_size + strlen(buf) + 8 > z_info->fake_text_size)
        return (false);

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

    // head->text_ptr[head->text_size] = '\0';

    /* Success */
    return (true);
}

/*
 * Add a name to the name-storage and return an offset to it.
 *
 * Returns 0 when there isn't enough space available to store
 * the name.
 */
static u32b add_name(header* head, cptr buf)
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
errr parse_z_info(char* buf, header* head)
{
    maxima* z_info = head->info_ptr;

    /* Hack - Verify 'M:x:' format */
    if (buf[0] != 'M')
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    if (!buf[2])
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    if (buf[3] != ':')
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);

    /* Process 'F' for "Maximum f_info[] index" */
    if (buf[2] == 'F')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->f_max = max;
    }

    /* Process 'K' for "Maximum k_info[] index" */
    else if (buf[2] == 'K')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->k_max = max;
    }

    /* Process 'B' for "Maximum b_info[] index" */
    else if (buf[2] == 'B')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->b_max = max;
    }

    /* Process 'A' for "Maximum a_info[] index" */
    else if (buf[2] == 'A')
    {
        int art_special_max, art_normal_max, art_random_max, art_self_made_max;

        /* Scan for the value */
        if (4
            != sscanf(buf + 4, "%d:%d:%d:%d", &art_special_max, &art_normal_max,
                &art_random_max, &art_self_made_max))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        z_info->art_spec_max = art_special_max;
        z_info->art_norm_max = art_normal_max + art_special_max;
        z_info->art_rand_max = z_info->art_norm_max + art_random_max;
        z_info->art_self_made_max = z_info->art_rand_max + art_self_made_max;

        /* Total artefacts */
        z_info->art_max = art_special_max + art_normal_max + art_random_max
            + art_self_made_max;
    }

    /* Process 'E' for "Maximum e_info[] index" */
    else if (buf[2] == 'E')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->e_max = max;
    }

    /* Process 'G' for "Maximum e_info[] index" */
    else if (buf[2] == 'G')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->ghost_other_max = max;
    }

    /* Process 'R' for "Maximum r_info[] index" */
    else if (buf[2] == 'R')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->r_max = max;
    }

    /* Process 'V' for "Maximum v_info[] index" */
    else if (buf[2] == 'V')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->v_max = max;
    }

    /* Process 'P' for "Maximum p_info[] index" */
    else if (buf[2] == 'P')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->p_max = max;
    }

    /* Process 'C' for "Maximum c_info[] index" */
    else if (buf[2] == 'C')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->c_max = max;
    }

    /* Process 'H' for "Maximum h_info[] index" */
    else if (buf[2] == 'H')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->h_max = max;
    }

    /* Process 'S' for "Maximum st_info[] index" */

    else if (buf[2] == 'S')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->st_max = max;
    }

    /* Process 'U' for "Maximum cu_info[] index" */

    else if (buf[2] == 'U')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->cu_max = max;
    }

    /* Process 'Q' for "Maximum q_info[] index" */
    else if (buf[2] == 'Q')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->q_max = max;
    }

    /* Process 'L' for "Maximum flavor_info[] subindex" */
    else if (buf[2] == 'L')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->flavor_max = max;
    }

    /* Process 'O' for "Maximum o_list[] index" */
    else if (buf[2] == 'O')
    {
        int max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%d", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->o_max = max;
    }

    /* Process 'N' for "Fake name size" */
    else if (buf[2] == 'N')
    {
        long max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%ld", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->fake_name_size = max;
    }

    /* Process 'T' for "Fake text size" */
    else if (buf[2] == 'T')
    {
        long max;

        /* Scan for the value */
        if (1 != sscanf(buf + 4, "%ld", &max))
            return (PARSE_ERROR_GENERIC);

        /* Save the value */
        z_info->fake_text_size = max;
    }
    /* M:R:<number_of_runtypes> ----------------------------------------- */
    else if (buf[2] == 'Y')
    {
        z_info->rt_max = (u16b)atoi(buf + 4);
    }
    /* M:Z:<number_of_styles> */
    else if (buf[2] == 'Z')
    {
        z_info->style_max = (u16b)atoi(buf + 4);
    }
    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/* ====================  runtypes.txt parser  ===================== */

static runtype_type *rt_ptr = NULL;

errr parse_rt_info(char *buf, header *head)
{
    /* N:<index>:<name> ------------------------------------------- */
    if (buf[0] == 'N')
    {
        int idx;
        char *s = strchr(buf+2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        idx = atoi(buf+2);

        /* normal sequential checks */
        if (idx <= error_idx) return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (idx >= head->info_num) return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = idx;

        rt_ptr = ((runtype_type*)head->info_ptr) + idx;
        WIPE(rt_ptr, runtype_type);
        rt_ptr->id = idx;
        strncpy(rt_ptr->name, s, sizeof(rt_ptr->name)-1);
        return 0;
    }

    /* C:b0|b1|b2 default-curses mask ----------------------------- */
    if (buf[0] == 'C')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        
        /* Initialize curse_stacks array to 0 */
        memset(rt_ptr->curse_stacks, 0, sizeof(rt_ptr->curse_stacks));
        rt_ptr->start_curses = 0;
        
        /* Parse curse specifications */
        char *tok = strtok(buf+2, "|");
        while (tok)
        {
            char *colon = strchr(tok, ':');
            if (colon)
            {
                /* Format: curse_id:stack_count */
                *colon = '\0';
                int curse_id = atoi(tok);
                int stack_count = atoi(colon + 1);
                
                if (curse_id >= 0 && curse_id < 32 && stack_count > 0 && stack_count <= 255)
                {
                    rt_ptr->start_curses |= (1UL << curse_id);
                    rt_ptr->curse_stacks[curse_id] = (byte)stack_count;
                }
            }
            else
            {
                /* Legacy format: just curse_id (default to 1 stack) */
                int curse_id = atoi(tok);
                if (curse_id >= 0 && curse_id < 32)
                {
                    rt_ptr->start_curses |= (1UL << curse_id);
                    rt_ptr->curse_stacks[curse_id] = 1;
                }
            }
            tok = strtok(NULL, "|");
        }
        return 0;
    }

    /* U:TERM_RED  (colour) --------------------------------------- */
    if (buf[0] == 'U')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        rt_ptr->colour = (byte)color_text_to_attr(buf+2);
        return 0;
    }
    /* W:<num>  – win condition (Silmarils target) ---------------- */
    if (buf[0] == 'W')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        int v = atoi(buf + 2);
        if (v < 1) v = 1;
        if (v > 127) v = 127;
        rt_ptr->win_con = (byte)v;
        return 0;
    }

    /* L:<num>  – lose condition (allowed deaths) ------------------ */
    if (buf[0] == 'L')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        int v = atoi(buf + 2);
        if (v < 1) v = 1;
        if (v > 127) v = 127;
        rt_ptr->lose_con = (byte)v;
        return 0;
    }

    /* H:*  or  H:i|j|k  – applicable heroes mask (0..63) ---------- */
    if (buf[0] == 'H')
    {
        if (!rt_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        char *arg = buf + 2;
        /* '*' means all heroes */
        if (*arg == '*') {
            for (int w = 0; w < FLAG_WORDS; ++w) rt_ptr->heroes[w] = 0xFFFFFFFFu;
            /* trim bits above 64 just in case */
            if (FLAG_WORDS > 2) for (int w = 2; w < FLAG_WORDS; ++w) rt_ptr->heroes[w] = 0;
            return 0;
        }
        /* otherwise a | separated list of indices */
        for (char *tok = strtok(arg, "|"); tok; tok = strtok(NULL, "|"))
        {
            int idx = atoi(tok);
            if (0 <= idx && idx < 64) {
                int w = idx >> 5, b = idx & 31;
                rt_ptr->heroes[w] |= (1u << b);
            }
        }
        return 0;
    }


    /* ignore unknown / comment lines                              */
    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

/* ====================  style.txt parser  ===================== */

static style_type* stl_ptr = NULL;
/* Global defaults for vein overlay when a style omits Y: */
static byte g_default_vein_row = 19;
static byte g_default_vein_col = 0;

/* Optional fixed color key for overlays (if provided) */
static bool g_overlay_key_enabled = false;
static byte g_overlay_key_r = 255, g_overlay_key_g = 0, g_overlay_key_b = 255; /* default magenta */

/* Accessors used by rendering */
byte get_default_vein_row(void);
byte get_default_vein_col(void);
byte get_default_vein_row(void) { return g_default_vein_row; }
byte get_default_vein_col(void) { return g_default_vein_col; }
bool get_overlay_key_enabled(void);
void get_overlay_key_rgb(byte* r, byte* g, byte* b);
bool get_overlay_key_enabled(void) { return g_overlay_key_enabled; }
void get_overlay_key_rgb(byte* r, byte* g, byte* b) { if (r) *r = g_overlay_key_r; if (g) *g = g_overlay_key_g; if (b) *b = g_overlay_key_b; }

/* Forward decl for per-style message parser (M:) */
static errr parse_style_message_line(char* buf);

errr parse_style_info(char* buf, header* head)
{
    /* Note: L:/U: moved to style-levels.txt for clarity. */
    /* E:<row>:<col> or DY:<row>:<col> — default vein overlay tile (used if a style omits Y:) */
    {
        const char* p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if (((p[0] == 'D' || p[0] == 'd') && (p[1] == 'Y' || p[1] == 'y') && p[2] == ':') ||
            ((p[0] == 'E' || p[0] == 'e') && p[1] == ':' ))
        {
            const char* q = (p[0] == 'E' || p[0] == 'e') ? (p + 2) : (p + 3);
            int r, c; if (2 != sscanf(q, "%d:%d", &r, &c)) return PARSE_ERROR_GENERIC;
            g_default_vein_row = (byte)r; g_default_vein_col = (byte)c; return 0;
        }
    }

    /* EK:R:G:B — optional explicit color key for overlays (e.g., 255:0:255) */
    {
        const char* p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if ((p[0] == 'E' || p[0] == 'e') && (p[1] == 'K' || p[1] == 'k') && p[2] == ':')
        {
            int r, g, b; if (3 != sscanf(p + 3, "%d:%d:%d", &r, &g, &b)) return PARSE_ERROR_GENERIC;
            g_overlay_key_r = (byte)r; g_overlay_key_g = (byte)g; g_overlay_key_b = (byte)b; g_overlay_key_enabled = true; return 0;
        }
    }
    /* N:<index>:<name> */
    if (buf[0] == 'N')
    {
        int idx;
        char* s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
    idx = atoi(buf + 2);

        if (idx <= error_idx) return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (idx >= head->info_num) return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = idx;

    stl_ptr = ((style_type*)head->info_ptr) + idx;
    WIPE(stl_ptr, style_type);
    /* Initialize counts */
    stl_ptr->floor_count = 0;
    stl_ptr->door_count = 0;
        stl_ptr->name = add_name(head, s);
    if (idx == 0) { /* styles only here */ }
        return 0;
    }

    if (!stl_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* G:<group_id> (1..6) */
    if (buf[0] == 'G')
    {
        stl_ptr->group = (byte)atoi(buf + 2);
        return 0;
    }
    /* W:row:col  (wall) */
    if (buf[0] == 'W')
    {
        int r, c; if (2 != sscanf(buf + 2, "%d:%d", &r, &c)) return PARSE_ERROR_GENERIC;
        stl_ptr->wall_row = (byte)r; stl_ptr->wall_col = (byte)c; return 0;
    }
    /* Y:row:col  (vein) — use 'Y' since 'V' is reserved for file version */
    if (buf[0] == 'Y')
    {
        int r, c; if (2 != sscanf(buf + 2, "%d:%d", &r, &c)) return PARSE_ERROR_GENERIC;
    stl_ptr->vein_row = (byte)r; stl_ptr->vein_col = (byte)c; stl_ptr->vein_defined = true; return 0;
    }
    /* F:row:col [row:col ...]  (floor) — multiple allowed; tokens separated by spaces */
    if (buf[0] == 'F')
    {
        const char* p = buf + 2;
        int added = 0;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '#') break;
            int r = -1, c = -1; int n = 0;
            /* parse r:c */
            if (sscanf(p, "%d:%d%n", &r, &c, &n) == 2) {
                if (stl_ptr->floor_count == 0) { stl_ptr->floor_row = (byte)r; stl_ptr->floor_col = (byte)c; }
                if (stl_ptr->floor_count < 8) {
                    stl_ptr->floor_rowv[stl_ptr->floor_count] = (byte)r;
                    stl_ptr->floor_colv[stl_ptr->floor_count] = (byte)c;
                    stl_ptr->floor_count++;
                    added++;
                }
                p += n;
            } else {
                /* stop on invalid token */
                break;
            }
        }
        if (!added) return PARSE_ERROR_GENERIC;
        return 0;
    }
    /* D:row:col [row:col ...]  (door base) — multiple allowed; tokens separated by spaces */
    if (buf[0] == 'D')
    {
        const char* p = buf + 2;
        int added = 0;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '#') break;
            int r = -1, c = -1; int n = 0;
            if (sscanf(p, "%d:%d%n", &r, &c, &n) == 2) {
                if (stl_ptr->door_count == 0) { stl_ptr->door_row = (byte)r; stl_ptr->door_col = (byte)c; }
                if (stl_ptr->door_count < 8) {
                    stl_ptr->door_rowv[stl_ptr->door_count] = (byte)r;
                    stl_ptr->door_colv[stl_ptr->door_count] = (byte)c;
                    stl_ptr->door_count++;
                    added++;
                }
                p += n;
            } else {
                break;
            }
        }
        if (!added) return PARSE_ERROR_GENERIC;
        return 0;
    }

    /* M: <text> — per-style message (banner) */
    if (buf[0] == 'M')
    {
        /* Reuse the message parser; it uses error_idx as current style index */
        return parse_style_message_line(buf);
    }

    return PARSE_ERROR_UNDEFINED_DIRECTIVE;
}

/* ====================  style-levels.txt parser  ===================== */

errr parse_style_levels(char* buf, header* head)
{
    (void)head; /* unused */
    /* Version header clears accumulated rules to allow reparse safely */
    if (buf[0] == 'V') {
        styles_rules_clear();
        styles_vault_rules_clear();
        styles_default_vault_clear();
    log_info("parse_style_levels: Version header encountered, cleared existing rules");
        return 0;
    }
    /* Comments or blank lines */
    if (buf[0] == '#' || buf[0] == '\0') return 0;

    /* L:depth: ... (exact)  or  L:min:max: ... (range)  -> level style rules */
    if (buf[0] == 'L')
    {
        int min_d = 0, max_d = 0;
        char* s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        min_d = atoi(buf + 2);
        /* Decide if this is a range by checking for a ':' before any space after the first ':' */
        char* first_space = strchr(s, ' ');
        char* second_colon = strchr(s, ':');
        char* t;
        if (second_colon && (!first_space || second_colon < first_space)) {
            /* Range form: L:min:max: ...  --> s points to max followed by ':' */
            *second_colon = '\0';
            max_d = atoi(s);
            t = second_colon + 1;
        } else {
            /* Exact form: L:depth: ... */
            max_d = min_d;
            t = s;
        }
        int sidx[64]; int wt[64]; int n = 0;
        while (*t)
        {
            while (*t == ' ') t++;
            if (!*t) break;
            char* e = t; while (*e && *e != ' ') e++;
            char hold = *e; if (*e) *e = '\0';
            char* c = strchr(t, ':'); if (!c) { if (hold) *e = hold; break; }
            *c = '\0';
            int si = atoi(t); int w = atoi(c + 1);
            if (si >= 0 && w > 0 && n < 64) { sidx[n] = si; wt[n] = w; n++; }
            if (hold) { *e = hold; t = e + 1; } else break;
        }
        if (n > 0) {
            /* Apply to each depth in the range (min_d..max_d), inclusive */
            if (min_d < 1) min_d = 1;
            if (max_d > 31) max_d = 31;
            for (int d = min_d; d <= max_d; ++d) {
                styles_add_level_rule(d, 0, sidx, wt, n);
            }
            log_info("parse_style_levels: L:%d..%d with %d entries (first sidx=%d w=%d)",
                min_d, max_d, n, sidx[0], wt[0]);
        }
        return 0;
    }
    /* U:depth: sidx:weight ...   or   U:*: sidx:weight ... (default vault styles) */
    if (buf[0] == 'U')
    {
        char* s = strchr(buf + 2, ':'); if (!s) return PARSE_ERROR_GENERIC; *s++ = '\0';
        const char* depth_tok = buf + 2;
        int sidx[64]; int wt[64]; int n = 0;
        while (*s)
        {
            while (*s == ' ') s++;
            if (!*s) break;
            char* e = s; while (*e && *e != ' ') e++;
            char hold = *e; if (*e) *e = '\0';
            char* c = strchr(s, ':'); if (!c) { if (hold) *e = hold; break; }
            *c = '\0';
            int si = (s[0] == '*' && s[1] == '\0') ? -1 : atoi(s);
            int w = atoi(c + 1);
            if (w > 0 && n < 64) { sidx[n] = si; wt[n] = w; n++; }
            if (hold) { *e = hold; s = e + 1; } else break;
        }
        if (depth_tok[0] == '*' && depth_tok[1] == '\0') {
            styles_default_vault_clear();
            for (int i = 0; i < n; ++i) styles_default_vault_add(sidx[i], wt[i]);
            log_info("parse_style_levels: U:* default with %d entries (first=%d:%d)", n, sidx[0], wt[0]);
        } else {
            int d = atoi(depth_tok);
            if (n > 0) {
                styles_set_vault_rule(d, sidx, wt, n);
                log_info("parse_style_levels: U:%d with %d entries (first=%d:%d)", d, n, sidx[0], wt[0]);
            }
        }
        return 0;
    }
    return 0;
}

/* ====================  style display strings (per-style M:)  ===================== */

/* Per-style banner strings (by style index, allow multiple sayings) */
#define MAX_STYLE_MSG 8
static char* g_style_display_text[128][MAX_STYLE_MSG];
static byte  g_style_display_count[128];

/* Accessor for per-style banner */
const char* styles_get_style_display(int sidx)
{
    if (sidx < 0 || sidx >= 128) return NULL;
    byte n = g_style_display_count[sidx];
    if (!n) return NULL;
    int pick = (n == 1) ? 0 : rand_int(n);
    return g_style_display_text[sidx][pick];
}

/* Extend style.txt parser to support per-style messages via M: */
/* In-record form:  M: <text>         (applies to current style error_idx) */
/* Global form:     M:<idx>: <text>   (applies to the given style index)   */
static errr parse_style_message_line(char* buf)
{
    if (buf[0] != 'M') return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    char* s = NULL;
    int idx = -1;
    if (buf[1] == ':')
    {
        /* Could be M:<idx>: or M: <text> (no idx) */
        char* p = buf + 2;
        while (*p == ' ' || *p == '\t') p++;
        if (*p >= '0' && *p <= '9')
        {
            /* M:<idx>:<text> */
            char* c = strchr(p, ':'); if (!c) return PARSE_ERROR_GENERIC; *c++ = '\0';
            idx = atoi(p);
            s = c;
        }
        else
        {
            /* M: <text> (use current error_idx) */
            idx = error_idx;
            s = p;
        }
    }
    else return PARSE_ERROR_GENERIC;

    /* Trim leading spaces */
    while (*s == ' ' || *s == '\t') s++;
    /* Strip optional surrounding quotes */
    if (*s == '"' || *s == '\'') { char q = *s++; char* e = strrchr(s, q); if (e) *e = '\0'; }
    /* Trim trailing comment */
    char* h = strchr(s, '#'); if (h) *h = '\0';
    for (char* t = s + strlen(s) - 1; t >= s && (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n'); --t) *t = '\0';

    if (idx < 0 || idx >= 128) return PARSE_ERROR_GENERIC;
    if (g_style_display_count[idx] >= MAX_STYLE_MSG) {
        log_debug("parse_style_message_line: style %d message list full, dropping: '%s'", idx, s);
        return 0;
    }
    /* Append */
    g_style_display_text[idx][g_style_display_count[idx]] = string_make(s);
    g_style_display_count[idx]++;
    log_debug("parse_style_message_line: added style %d message #%d", idx, g_style_display_count[idx]);
    return 0;
}

/* Clear all loaded per-style banner messages (free and NULL them). */
void styles_clear_display_messages(void)
{
    for (int i = 0; i < 128; ++i)
    {
        for (int j = 0; j < MAX_STYLE_MSG; ++j) {
            if (g_style_display_text[i][j]) {
                string_free(g_style_display_text[i][j]);
                g_style_display_text[i][j] = NULL;
            }
        }
        g_style_display_count[i] = 0;
    }
}

/* Reload only M: lines from style.txt so banners are available even if RAW cache was used. */
void styles_reload_messages_from_text(void)
{
    char path[1024];
    FILE* fp;
    char buf[1024];
    /* Start clean to avoid stale/duplicate entries */
    styles_clear_display_messages();

    /* Build full path to lib/edit/style.txt */
    path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "style"));
    fp = my_fopen(path, "r");
    if (!fp)
    {
        log_info("styles_reload_messages_from_text: couldn't open %s", path);
        return;
    }

    /* We need to maintain the current style index (error_idx) for in-record M: lines */
    /* error_idx is the conventional global parser index in this translation unit */
    error_idx = -1;

    while (my_fgets(fp, buf, sizeof(buf)) == 0)
    {
        /* Trim leading spaces */
        char* s = buf;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\0' || *s == '#') continue;
        if (*s == 'N')
        {
            /* N:<idx>:<name>  — capture idx to set error_idx */
            char* colon = strchr(s + 2, ':');
            if (!colon) continue;
            *colon = '\0';
            int idx = atoi(s + 2);
            error_idx = idx;
            continue;
        }
        if (*s == 'M')
        {
            /* Pass through to the same message parser to support both M:<idx>: and in-record M: */
            (void)parse_style_message_line(s);
            continue;
        }
        /* Ignore other lines */
    }
    my_fclose(fp);
    log_info("styles_reload_messages_from_text: loaded per-style messages from text");
}


/*
 * Initialize the "f_info" array, by parsing an ascii "template" file
 */
errr parse_f_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static feature_type* f_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!f_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (1 != sscanf(buf + 2, "%d", &mimic))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        f_ptr->mimic = mimic;
    }

    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current f_ptr */
        if (!f_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

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
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

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
static errr grab_one_flag(u32b** flag, cptr errstr, cptr what)
{
    uint i;

    /* Check flags */
    for (i = 0; i < N_ELEMENTS(info_flags); i++)
    {
        flag_name* f_ptr = info_flags + i;

        if (!flag[f_ptr->set])
            continue;

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
static errr grab_one_kind_flag(object_kind* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    return grab_one_flag(f, "object", what);
}

/**********************************************************************
 * Grab a single RHF and CUR flag for a curse (used by “F:” and "U:" lines in curses.txt)
 **********************************************************************/
static errr grab_one_curse_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[RHF] = &(cu_ptr->flags);   /* write into the new word we added */
    return grab_one_flag(f, "curse", what);
}

static errr grab_one_curse_unique_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[CUR] = &(cu_ptr->flags_u);   /* write into the new word we added */
    return grab_one_flag(f, "curse unique", what);
}

/*
 * Initialize the "k_info" array, by parsing an ascii "template" file
 */
errr parse_k_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static object_kind* k_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

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
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        k_ptr->d_attr = d_attr;
        k_ptr->d_char = d_char;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int tval, sval, pval;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval, &pval))
            return (PARSE_ERROR_GENERIC);

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
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%ld", &level, &extra, &wgt, &cost))
            return (PARSE_ERROR_GENERIC);

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
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default chance */
            k_ptr->chance[i] = 1;

            /* Store the attack damage index */
            k_ptr->locale[i] = atoi(s + 1);

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int chance = atoi(t + 1);
                if (chance > 0)
                    k_ptr->chance[i] = chance;
            }
        }
    }

    /* Hack -- Process 'P' for "power" and such */
    else if (buf[0] == 'P')
    {
        int att, dd, ds, evn, pd, ps;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (6
            != sscanf(
                buf + 2, "%d:%dd%d:%d:%dd%d", &att, &dd, &ds, &evn, &pd, &ps))
            return (PARSE_ERROR_GENERIC);

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
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while (*t == ' ' || *t == '|')
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_kind_flag(k_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int i;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            k_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            k_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            k_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    k_ptr->abilitynum[i] = abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

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
static errr grab_one_vault_flag(vault_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[VLT] = &(ptr->flags);
    return grab_one_flag(f, "vault", what);
}

/*
 * Initialize the "v_info" array, by parsing an ascii "template" file
 */
errr parse_v_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static vault_type* v_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        v_ptr = (vault_type*)head->info_ptr + i;

    /* Initialize default values */
        v_ptr->color = 0; /* Default to depth color */
    v_ptr->style_count = 0;
    for (int j = 0; j < 16; ++j) { v_ptr->style_idx[j] = -1; v_ptr->style_weight[j] = 0; }

        /* Store the name */
        if (!(v_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'X' for "Extra info" (one line only) */
    else if (buf[0] == 'X')
    {
        int typ, depth, rarity;

        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &typ, &depth, &rarity))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        v_ptr->typ = typ;
        v_ptr->depth = depth;
        v_ptr->rarity = rarity;
        v_ptr->hgt = 0;
        v_ptr->wid = 0;
        /* Note: Don't reset color here - it may have been set by a C: line */
    }

    /* Process 'C' for "Color" (one line only) */
    else if (buf[0] == 'C')
    {
        int color;

        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the color value */
        if (1 != sscanf(buf + 2, "%d", &color))
            return (PARSE_ERROR_GENERIC);

        /* Verify color range (0-255) */
        if (color < 0 || color > 255)
            return (PARSE_ERROR_GENERIC);

        /* Save the color value */
        v_ptr->color = color;
    }

    /* Process 'S' for Styles: one or more pairs "sidx:weight" separated by spaces
     * sidx may be:
     *   <number>  => exact style index
     *   *          => the already generated level style (sentinel -1)
     *   $          => any random style suitable for this depth's floors (sentinel -2)
     */
    else if (buf[0] == 'S')
    {
        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        char* s = buf + 2;
        char* tok;
        while (*s)
        {
            while (*s == ' ') s++;
            if (!*s) break;
            tok = s;
            while (*s && *s != ' ') s++;
            if (*s) { *s = '\0'; s++; }
            /* parse tok as sidx:weight; '*' means sidx == -1 (level styles)
             * and '$' means sidx == -2 (any style available at this depth) */
            char* colon = strchr(tok, ':');
            if (!colon) return PARSE_ERROR_GENERIC;
            *colon = '\0';
            int sidx;
            if (tok[0] == '*' && tok[1] == '\0') sidx = -1;        /* level style */
            else if (tok[0] == '$' && tok[1] == '\0') sidx = -2;   /* any depth-available style */
            else sidx = atoi(tok);
            int w = atoi(colon + 1);
            if (v_ptr->style_count < 16 && sidx >= -2 && w > 0)
            {
                v_ptr->style_idx[v_ptr->style_count] = (s16b)sidx;
                v_ptr->style_weight[v_ptr->style_count] = (s16b)w;
                v_ptr->style_count++;
            }
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current k_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while (*t == ' ' || *t == '|')
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_vault_flag(v_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current v_ptr */
        if (!v_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        if (v_ptr->wid == 0)
        {
            v_ptr->wid = strlen(buf + 2);
        }
        else if (v_ptr->wid != strlen(buf + 2))
        {
            return (PARSE_ERROR_VAULT_NOT_RECTANGULAR);
        }

        /* Store the text */
        if (!add_text(&v_ptr->text, head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        // note if there is a forge in the vault
        if (strchr(buf, '0'))
            v_ptr->forge = true;

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
errr parse_b_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    static int cur_t = 0;

    /* Current entry */
    static ability_type* b_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &skilltype, &abilitynum, &level))
            return (PARSE_ERROR_GENERIC);

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
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            b_ptr->prereq_abilitynum[i] = 0;

            /* Store the skilltype */
            b_ptr->prereq_skilltype[i] = atoi(s + 1);

            /* List this prerequisite */
            b_ptr->prereqs++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int prereq_abilitynum = atoi(t + 1);
                if (prereq_abilitynum > 0)
                    b_ptr->prereq_abilitynum[i] = prereq_abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(b_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Types allowed" (up to five lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        b_ptr->tval[cur_t] = (byte)tval;
        b_ptr->min_sval[cur_t] = (byte)sval1;
        b_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;

        /* Allow only a limited number of T: lines */
        if (cur_t > ABILITY_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);
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
static errr grab_one_artefact_flag(artefact_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    return grab_one_flag(f, "object", what);
}

/*
 * Grab one activation from a textual string
 */
static errr grab_one_activation(artefact_type* a_ptr, cptr what)
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
errr parse_a_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static artefact_type* a_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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

    /* Sil -- added this to allow for artefacts that look different to the base
     * type */
    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

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
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->d_attr = d_attr;
        a_ptr->d_char = d_char;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int tval, sval, pval;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval, &pval))
            return (PARSE_ERROR_GENERIC);

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
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%ld", &level, &rarity, &wgt, &cost))
            return (PARSE_ERROR_GENERIC);

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
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (6
            != sscanf(
                buf + 2, "%d:%dd%d:%d:%dd%d", &att, &dd, &ds, &evn, &pd, &ps))
            return (PARSE_ERROR_GENERIC);

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
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_artefact_flag(a_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'A' for "Activation & time" */
    else if (buf[0] == 'A')
    {
        int ptime, prand;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the activation */
        if (grab_one_activation(a_ptr, buf + 2))
            return (PARSE_ERROR_GENERIC);

        /* Scan for the values */
        if (2 != sscanf(s, "%d:%d", &ptime, &prand))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->time = ptime;
        a_ptr->randtime = prand;
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int i;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            a_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            a_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            a_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    a_ptr->abilitynum[i] = abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

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
static errr build_prob(char* name, names_type* n_ptr)
{
    int c_prev, c_cur, c_next;

    while (*name && !isalpha((unsigned char)*name))
        ++name;

    if (!*name)
        return PARSE_ERROR_GENERIC;

    c_prev = c_cur = S_WORD;

    do
    {
        if (isalpha((unsigned char)*name))
        {
            c_next = A2I(tolower((unsigned char)*name));
            n_ptr->lprobs[c_prev][c_cur][c_next]++;
            n_ptr->ltotal[c_prev][c_cur]++;
            c_prev = c_cur;
            c_cur = c_next;
        }
    } while (*++name);

    n_ptr->lprobs[c_prev][c_cur][E_WORD]++;
    n_ptr->ltotal[c_prev][c_cur]++;

    return 0;
}

/*
 * Initialize the "n_info" array, by parsing an ascii "template" file
 */
errr parse_n_info(char* buf, header* head)
{
    names_type* n_ptr = head->info_ptr;

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
        return build_prob(buf + 2, n_ptr);
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
static bool grab_one_ego_item_flag(ego_item_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    return grab_one_flag(f, "object", what);
}

/*
 * Initialize the "e_info" array, by parsing an ascii "template" file
 */
errr parse_e_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static ego_item_type* e_ptr = NULL;

    static int cur_t = 0;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4
            != sscanf(
                buf + 2, "%d:%d:%d:%ld", &level, &rarity, &max_level, &cost))
            return (PARSE_ERROR_GENERIC);

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
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        e_ptr->tval[cur_t] = (byte)tval;
        e_ptr->min_sval[cur_t] = (byte)sval1;
        e_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;

        /* Allow only a limited number of T: lines */
        if (cur_t > EGO_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);
    }

    /* Hack -- Process 'C' for "creation" */
    else if (buf[0] == 'C')
    {
        int max_att, to_dd, to_ds, max_evn, to_pd, to_ps, pv;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (7
            != sscanf(buf + 2, "%d:%d:%d:%d:%d:%d:%d", &max_att, &to_dd, &to_ds,
                &max_evn, &to_pd, &to_ps, &pv))
            return (PARSE_ERROR_GENERIC);

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
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            e_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            e_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            e_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    e_ptr->abilitynum[i] = abilitynum;
            }
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_ego_item_flag(e_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

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
static errr grab_one_basic_flag(monster_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[RF1] = &(ptr->flags1);
    f[RF2] = &(ptr->flags2);
    f[RF3] = &(ptr->flags3);
    return grab_one_flag(f, "monster", what);
}

/*
 * Grab one (spell) flag in a monster_race from a textual string
 */
static errr grab_one_spell_flag(monster_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));
    f[RF4] = &(ptr->flags4);
    return grab_one_flag(f, "monster", what);
}

/*
 * Initialize the "r_info" array, by parsing an ascii "template" file
 */
errr parse_r_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static monster_race* r_ptr = NULL;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

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
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

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
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        r_ptr->d_attr = d_attr;
        r_ptr->d_char = d_char;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int spd, hp1, hp2, light;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the other values */
        if (4 != sscanf(buf + 2, "%d:%dd%d:%d", &spd, &hp1, &hp2, &light))
            return (PARSE_ERROR_GENERIC);

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
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &lev, &rar))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        r_ptr->level = lev;
        r_ptr->rarity = rar;
    }

    /* Process 'A' for "Alertness Info" (one line only) */
    else if (buf[0] == 'A')
    {
        int sleep, per, stl, wil;

        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &sleep, &per, &stl, &wil))
            return (PARSE_ERROR_GENERIC);

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
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        n = sscanf(buf + 2, "[%d,%dd%d]", &evn, &pd, &ps);
        if ((n != 1) && (n != 3))
            return (PARSE_ERROR_GENERIC);

        //		if (3 != sscanf(buf+2, "[%d,%dd%d]",
        //						&evn, &pd, &ps)) return
        //(PARSE_ERROR_GENERIC);

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
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Find the next empty blow slot (if any) */
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
            if (!r_ptr->blow[i].method)
                break;

        /* Oops, no more slots */
        if (i == MONSTER_BLOW_MAX)
            return (PARSE_ERROR_GENERIC);

        /* Analyze the first field */
        for (s = t = buf + 2; *t && (*t != ':'); t++) /* loop */
            ;

        /* Terminate the field (if necessary) */
        if (*t == ':')
            *t++ = '\0';

        /* Analyze the method */
        for (n1 = 0; r_info_blow_method[n1]; n1++)
        {
            if (streq(s, r_info_blow_method[n1]))
                break;
        }

        /* Invalid method */
        if (!r_info_blow_method[n1])
            return (PARSE_ERROR_GENERIC);

        /* Analyze the second field */
        for (s = t; *t && (*t != ':'); t++) /* loop */
            ;

        /* Terminate the field (if necessary) */
        if (*t == ':')
            *t++ = '\0';

        /* Analyze effect */
        for (n2 = 0; r_info_blow_effect[n2]; n2++)
        {
            if (streq(s, r_info_blow_effect[n2]))
                break;
        }

        /* Invalid effect */
        if (!r_info_blow_effect[n2])
            return (PARSE_ERROR_GENERIC);

        // reset values
        dd = 0;
        ds = 0;

        n = sscanf(t, "(%d,%dd%d)", &att, &dd, &ds);
        if ((n != 1) && (n != 3))
            return (PARSE_ERROR_GENERIC);

        // s = t;

        /* Scan for the values */
        // if (1 != sscanf(t, "(%d)", &att))
        //{
        //	t = s;
        //	if (3 != sscanf(t, "(%d,%dd%d)", &att, &dd, &ds)) return
        //(PARSE_ERROR_GENERIC);
        //}

        /* Analyze the third field */
        // for (s = t; *t && (*t != 'd'); t++) /* loop */;

        /* Terminate the field (if necessary) */
        // if (*t == 'd') *t++ = '\0';

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
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while (*t == ' ' || *t == '|')
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_basic_flag(r_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'S' for "Spell Flags" (multiple lines) */
    else if (buf[0] == 'S')
    {
        /* There better be a current r_ptr */
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* XXX Hack -- Read spell frequency */
            if ((r_ptr->freq_ranged == 0)
                && (1 == sscanf(s, "SPELL_PCT_%d", &i)))
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
            if ((r_ptr->spell_power == 0) && (1 == sscanf(s, "POW_%d", &i)))
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
static errr grab_one_race_flag(player_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
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

    return grab_one_flag(f, "player house", what);
}

static errr grab_one_house_uflag(player_house *ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    C_WIPE(f, MAX_FLAG_SETS, sizeof(u32b*));

    f[UNQ] = &(ptr->flags_u);      /* NEW: accept unique-flag word */

    return grab_one_flag(f, "player house", what);
}

/*
 * Initialize the "p_info" array, by parsing an ascii "template" file
 */
errr parse_p_info(char* buf, header* head)
{
    int i, j;

    char *s, *t;

    /* Current entry */
    static player_race* pr_ptr = NULL;
    static int cur_equip = 0;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Start the string */
        s = buf + 1;

        /* For each stat */
        for (j = 0; j < A_MAX; j++)
        {
            /* Find the colon before the subindex */
            s = strchr(s, ':');

            /* Verify that colon */
            if (!s)
                return (PARSE_ERROR_GENERIC);

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
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &hist, &b_age, &m_age))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->hist = hist;
        pr_ptr->b_age = b_age;
        pr_ptr->m_age = m_age;
    }

    /* Hack -- Process 'H' for "Height" */
    else if (buf[0] == 'H')
    {
        int b_ht, m_ht;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &b_ht, &m_ht))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->b_ht = b_ht;
        pr_ptr->m_ht = m_ht;
    }

    /* Hack -- Process 'W' for "Weight" */
    else if (buf[0] == 'W')
    {
        int b_wt, m_wt;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &b_wt, &m_wt))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->b_wt = b_wt;
        pr_ptr->m_wt = m_wt;
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_race_flag(pr_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'E' for "Starting Equipment" */
    else if (buf[0] == 'E')
    {
        int tval, sval, min, max;

        start_item* e_ptr;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Access the item */
        e_ptr = &pr_ptr->start_items[cur_equip];

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
            return (PARSE_ERROR_GENERIC);

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
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            int bit = atoi(s);   // Converts the string (e.g. "42") to an int
            if (bit >= 0 && bit < FLAG_COUNT) {
                int word = bit / 32;        // Which 32-bit slot (0 or 1)
                int shift = bit % 32;       // Which bit in that slot (0–31)
                pr_ptr->choice[word] |= (1U << shift);  // Set the bit
            } else {
                // Invalid flag index
            }

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

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
// errr parse_c_info(char* buf, header* head)
// {
//     int i, j;
//     static int cur_equip = 0;

//     char *s, *t;

//     /* Current entry */
//     static player_house* ph_ptr = NULL;

//     log_debug("Parsing houses");

//     /* Process 'N' for "New/Number/Name" */
//     if (buf[0] == 'N')
//     {
//         char *s;
//         int   idx, j;

//         /* Find the colon before the name */
//         s = strchr(buf + 2, ':');
//         if (!s) return (PARSE_ERROR_GENERIC);

//         /* Split and advance to the name text */
//         *s++ = '\0';
//         if (!*s) return (PARSE_ERROR_GENERIC);

//         /* Parse the index */
//         idx = atoi(buf + 2);
//         if (idx <= error_idx)            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
//         if (idx >= head->info_num)       return (PARSE_ERROR_TOO_MANY_ENTRIES);
//         error_idx = idx;

//         /* Point at this slot */
//         ph_ptr = (player_house*)head->info_ptr + idx;

//         /* Store the name offset */
//         if (!(ph_ptr->name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);

//         /* Debug: announce new house and its name */
//         log_debug("New house #%d: \"%s\"", idx,
//                 head->name_ptr + ph_ptr->name);

//         /* Sentinel‐initialize all ability slots to “empty” */
//         for (j = 0; j < HOUSE_ABILITY_MAX; j++)
//         {
//             ph_ptr->a_adj[j][0] = -1;
//             ph_ptr->a_adj[j][1] = -1;
//         }
//         log_debug("  a_adj slots 0..%d set to -1", HOUSE_ABILITY_MAX - 1);
//     }

//     /* Process 'A' for "Alternate Name" */
//     else if (buf[0] == 'A')
//     {
//         /* Find the colon before the name */
//         s = strchr(buf, ':');

//         /* Verify that colon */
//         if (!s)
//             return (PARSE_ERROR_GENERIC);

//         /* Nuke the colon, advance to the name */
//         *s++ = '\0';

//         /* Paranoia -- require a name */
//         if (!*s)
//             return (PARSE_ERROR_GENERIC);

//         /* Store the name */
//         if (!(ph_ptr->alt_name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }

//     /* Process 'B' for "Short Name" */
//     else if (buf[0] == 'B')
//     {
//         /* Find the colon before the name */
//         s = strchr(buf, ':');

//         /* Verify that colon */
//         if (!s)
//             return (PARSE_ERROR_GENERIC);

//         /* Nuke the colon, advance to the name */
//         *s++ = '\0';

//         /* Paranoia -- require a name */
//         if (!*s)
//             return (PARSE_ERROR_GENERIC);

//         /* Store the name */
//         if (!(ph_ptr->short_name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }

//     /* Process 'S' for "Stats" (one line only) */
//     else if (buf[0] == 'S')
//     {
//         int adj;

//         /* There better be a current ph_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Start the string */
//         s = buf + 1;

//         /* For each stat */
//         for (j = 0; j < A_MAX; j++)
//         {
//             /* Find the colon before the subindex */
//             s = strchr(s, ':');

//             /* Verify that colon */
//             if (!s)
//                 return (PARSE_ERROR_GENERIC);

//             /* Nuke the colon, advance to the subindex */
//             *s++ = '\0';

//             /* Get the value */
//             adj = atoi(s);

//             /* Save the value */
//             ph_ptr->h_adj[j] = adj;

//             /* Next... */
//             continue;
//         }
//     }

//     /* Hack -- Process 'F' for flags */
//     else if (buf[0] == 'F')
//     {
//         /* There better be a current pr_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Parse every entry textually */
//         for (s = buf + 2; *s;)
//         {
//             /* Find the end of this entry */
//             for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
//                 ;

//             /* Nuke and skip any dividers */
//             if (*t)
//             {
//                 *t++ = '\0';
//                 while ((*t == ' ') || (*t == '|'))
//                     t++;
//             }

//             /* Parse this entry */
//             if (0 != grab_one_house_flag(ph_ptr, s))
//                 return (PARSE_ERROR_INVALID_FLAG);

//             /* Start the next entry */
//             s = t;
//         }
//     }

//     /* ------------------------------------------------------------ */
//     /* U: list of Unique flags        */
//     /* ------------------------------------------------------------ */
//     else if (buf[0] == 'U')
//     {
//         if (!ph_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

//         for (s = buf + 2; *s; )
//         {
//             /* token = [^ or |]*  */
//             for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
//             if (*t)
//             {
//                 *t++ = '\0';
//                 while ((*t == ' ') || (*t == '|')) t++;
//             }

//             if (grab_one_house_uflag(ph_ptr, s))
//                 return PARSE_ERROR_INVALID_FLAG;

//             s = t;
//         }
//     }

//         /* Process 'E' for "Starting Equipment" */
//     else if (buf[0] == 'E')
//     {
//         int tval, sval, min, max;

//         start_item* e_ptr;

//         /* There better be a current pr_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Access the item */
//         e_ptr = &ph_ptr->start_items[cur_equip];

//         /* Scan for the values */
//         if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
//             return (PARSE_ERROR_GENERIC);

//         if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
//             return (PARSE_ERROR_INVALID_ITEM_NUMBER);

//         /* Save the values */
//         e_ptr->tval = tval;
//         e_ptr->sval = sval;
//         e_ptr->min = min;
//         e_ptr->max = max;

//         /* Next item */
//         cur_equip++;

//         /* Limit number of starting items */
//         if (cur_equip > MAX_START_ITEMS)
//             return (PARSE_ERROR_GENERIC);
//     }


//     /* Process 'D' for "Description" */
//     else if (buf[0] == 'D')
//     {
//         /* There better be a current ph_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Get the text */
//         s = buf + 2;

//         /* Store the text */
//         if (!add_text(&(ph_ptr->text), head, s))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }
//     /* Process 'C' for house ability entries */
//     else if (buf[0] == 'C')
//     {
//         char *t = buf + 1;
//         int   pair = 0;

//         if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Debug: which house we’re parsing into */
//         log_debug("Parsing abilities for house \"%s\"…",
//                 head->name_ptr + ph_ptr->name);

//         /* Read up to HOUSE_ABILITY_MAX of “:stat:ability” pairs */
//         while (pair < HOUSE_ABILITY_MAX)
//         {
//             /* stat */
//             t = strchr(t, ':');
//             if (!t) break;
//             *t++ = '\0';
//             ph_ptr->a_adj[pair][0] = (s16b)atoi(t);

//             /* ability */
//             t = strchr(t, ':');
//             if (!t) break;
//             *t++ = '\0';
//             ph_ptr->a_adj[pair][1] = (s16b)atoi(t);

//             log_debug("  parsed slot %d -> stat=%d ability=%d",
//                     pair,
//                     ph_ptr->a_adj[pair][0],
//                     ph_ptr->a_adj[pair][1]);

//             pair++;
//         }

//         log_debug("  total %d ability pairs parsed", pair);
//     }

//     else
//     {
//         /* Oops */
//         return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
//     }

//     /* Success */
//     return (0);
// }

errr parse_c_info(char* buf, header* head)
{
    int j;
    static int cur_equip = 0;

    char *s, *t;

    /* Current entry */
    static player_house* ph_ptr = NULL;

    log_debug("Parsing characters");

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        char *s;
        int   idx, j;

        /* Find the colon before the name */
        s = strchr(buf + 2, ':');
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Split and advance to the name text */
        *s++ = '\0';
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Parse the index */
        idx = atoi(buf + 2);
        if (idx <= error_idx)            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (idx >= head->info_num)       return (PARSE_ERROR_TOO_MANY_ENTRIES);
        error_idx = idx;

        /* Point at this slot */
        ph_ptr = (player_house*)head->info_ptr + idx;

        /* RESET equipment counter for new house */
        cur_equip = 0;

        /* Store the name offset */
        if (!(ph_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Debug: announce new house and its name */
        log_debug("New character #%d: \"%s\"", idx,
                head->name_ptr + ph_ptr->name);

        /* Sentinel‐initialize all ability slots to "empty" */
        for (j = 0; j < HOUSE_ABILITY_MAX; j++)
        {
            ph_ptr->a_adj[j][0] = -1;
            ph_ptr->a_adj[j][1] = -1;
        }
        log_debug("  a_adj slots 0..%d set to -1", HOUSE_ABILITY_MAX - 1);

        /* Initialize starting items array */
        for (j = 0; j < MAX_START_ITEMS; j++)
        {
            ph_ptr->start_items[j].tval = 0;
            ph_ptr->start_items[j].sval = 0;
            ph_ptr->start_items[j].min = 0;
            ph_ptr->start_items[j].max = 0;
        }
        log_debug("  start_items array initialized");
    }

    /* Process 'A' for "Alternate Name" */
    else if (buf[0] == 'A')
    {
        /* Find the colon before the name */
        s = strchr(buf, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Store the name */
        if (!(ph_ptr->alt_name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'B' for "Start String" */
    else if (buf[0] == 'B')
    {
        /* Find the colon before the name */
        s = strchr(buf, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Store the name */
        if (!(ph_ptr->start_string = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stats" (one line only) */
    else if (buf[0] == 'S')
    {
        int adj;

        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Start the string */
        s = buf + 1;

        /* For each stat */
        for (j = 0; j < A_MAX; j++)
        {
            /* Find the colon before the subindex */
            s = strchr(s, ':');

            /* Verify that colon */
            if (!s)
                return (PARSE_ERROR_GENERIC);

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
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_house_flag(ph_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* ------------------------------------------------------------ */
    /* U: list of Unique flags        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'U')
    {
        if (!ph_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_house_uflag(ph_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }

        /* Process 'E' for "Starting Equipment" */
    else if (buf[0] == 'E')
    {
        int tval, sval, min, max;

        start_item* e_ptr;

        /* There better be a current pr_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Check if we've exceeded the maximum number of items */
        if (cur_equip >= MAX_START_ITEMS)
        {
            log_debug("Warning: Too many starting items for house (max %d), ignoring", MAX_START_ITEMS);
            return (PARSE_ERROR_GENERIC);
        }

        /* Access the item */
        e_ptr = &ph_ptr->start_items[cur_equip];

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
            return (PARSE_ERROR_GENERIC);

        if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
            return (PARSE_ERROR_INVALID_ITEM_NUMBER);

        /* Save the values */
        e_ptr->tval = tval;
        e_ptr->sval = sval;
        e_ptr->min = min;
        e_ptr->max = max;

        /* Debug: show what we parsed */
        log_debug("  Equipment slot %d: tval=%d sval=%d min=%d max=%d", 
                cur_equip, tval, sval, min, max);

        /* Next item */
        cur_equip++;
    }


    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(ph_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
    /* Process 'C' for house ability entries */
    else if (buf[0] == 'C')
    {
        char *t = buf + 2; /* Skip 'C:' */
        int   pair = 0;
        char *stat_start, *ability_start;

        if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Debug: which house we're parsing into */
        log_debug("Parsing abilities for house \"%s\" from line: %s",
                head->name_ptr + ph_ptr->name, buf);

        /* Read up to HOUSE_ABILITY_MAX of ":stat:ability" pairs */
        while (pair < HOUSE_ABILITY_MAX && t && *t)
        {
            /* Find first colon for stat */
            if (*t == ':') t++; /* Skip leading colon if present */
            stat_start = t;
            
            t = strchr(t, ':');
            if (!t) break;
            *t++ = '\0';
            
            if (!*stat_start) break; /* Empty stat */
            ph_ptr->a_adj[pair][0] = (s16b)atoi(stat_start);

            /* Find second colon for ability */
            ability_start = t;
            t = strchr(t, ':');
            if (t) {
                *t++ = '\0';
            }
            
            if (!*ability_start) break; /* Empty ability */
            ph_ptr->a_adj[pair][1] = (s16b)atoi(ability_start);

            log_debug("  parsed slot %d -> stat=%d ability=%d",
                    pair,
                    ph_ptr->a_adj[pair][0],
                    ph_ptr->a_adj[pair][1]);

            pair++;
            
            /* If no more colons, we're done */
            if (!t) break;
        }

        log_debug("  total %d ability pairs parsed", pair);
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
errr parse_h_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static hist_type* h_ptr = NULL;

    /* Process 'N' for "New/Number" */
    if (buf[0] == 'N')
    {
        int prv, nxt, prc, hou;

        /* Hack - get the index */
        i = error_idx + 1;

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        h_ptr = (hist_type*)head->info_ptr + i;

        /* Scan for the values */
        if (4 != sscanf(buf, "N:%d:%d:%d:%d", &prv, &nxt, &prc, &hou))
            return (PARSE_ERROR_GENERIC);

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
        if (!h_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

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
 * Initialize the "st_info" array, by parsing an ascii "template" file
 */
errr parse_st_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static story_type* st_ptr = NULL;

    /* Process 'N' for "New/Number" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        st_ptr = (story_type*)head->info_ptr + i;
        WIPE(st_ptr, story_type);

        /* Store the name */
        if (!(st_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Sensible defaults */
        st_ptr->st_type  = 0;
        st_ptr->order    = 0;
        st_ptr->runtypes = 0;   /* 0 == ALL runtypes */
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current st_ptr */
        if (!st_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&st_ptr->text, head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
    /* Process 'T' for type (byte) */
    else if (buf[0] == 'T')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        int t;
        if (1 != sscanf(buf + 2, "%d", &t)) return (PARSE_ERROR_GENERIC);
        if (t < 0 || t > 255)               return (PARSE_ERROR_OUT_OF_BOUNDS);
        st_ptr->st_type = (byte)t;
    }
    /* Process 'O' for order (byte) */
    else if (buf[0] == 'O')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        int o;
        if (1 != sscanf(buf + 2, "%d", &o)) return (PARSE_ERROR_GENERIC);
        if (o < 0 || o > 255)               return (PARSE_ERROR_OUT_OF_BOUNDS);
        st_ptr->order = (byte)o;
    }
    /* Process 'R' for runtypes: "*" or "i|j|k" */
    else if (buf[0] == 'R')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        s = buf + 2;
        while (*s == ' ' || *s == '\t') s++;

        /* "*" => all runtypes (store 0 to mean ALL) */
        if (*s == '*')
        {
            st_ptr->runtypes = 0;  /* wildcard */
        }
        else
        {
            u32b mask = 0;
            char *tok = strtok(s, "|");
            while (tok)
            {
                int bit = atoi(tok);
                if (bit < 0 || bit >= 32)
                {
                    /* silently ignore out-of-range bits */
                }
                else
                {
                    mask |= (1UL << bit);
                }
                tok = strtok(NULL, "|");
            }
            st_ptr->runtypes = mask;
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

/**********************************************************************
 * Initialise the “cu_info” array by parsing curses.txt
 **********************************************************************/
errr parse_cu_info(char *buf, header *head)
{
    int   i, j;
    char *s, *t;

    /* Current entry */
    static curse_type *cu_ptr = NULL;

    /* ------------------------------------------------------------ */
    /* N: idx : name                                                */
    /* ------------------------------------------------------------ */
    if (buf[0] == 'N')
    {
        /* Find name delimiter */
        s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        if (!*s) return PARSE_ERROR_GENERIC;

        i = atoi(buf + 2);
        if (i <= error_idx)          return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (i >= head->info_num)     return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = i;

        cu_ptr = ((curse_type *)head->info_ptr) + i;

        /* Reset fresh record */
        WIPE(cu_ptr, curse_type);       /* clears the record  */
                                                     /* flags included     */
        cu_ptr->weight = 1;      /* sensible defaults           */
        cu_ptr->max_stacks = 0;  /* 0 = unlimited               */

        if (!(cu_ptr->name = add_name(head, s)))     
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* C: stat adjustments  or  S: (old name kept for back-compat)  */
    /* ------------------------------------------------------------ */
    else if ((buf[0] == 'C') || (buf[0] == 'S'))
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        s = buf + 1;                     /* points to first ':' */
        for (j = 0; j < A_MAX; j++)
        {
            s = strchr(s, ':');
            if (!s) return PARSE_ERROR_GENERIC;
            *s++ = '\0';
            cu_ptr->cu_adj[j] = atoi(s);
        }
    }

    /* ------------------------------------------------------------ */
    /* F: list of RHF flags (MEL_PENALTY | SWORD_AFFINITY …)        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'F')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_curse_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* U: list of CUR flags        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'U')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_curse_unique_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }

    /* ------------------------------------------------------------ */
    /* A: weight / max_stacks   (e.g. 3/5 means weight=3, max=5)    */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'A')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        /* default is "1/0" so zero-initialised files still work    */
        cu_ptr->weight     = 1;
        cu_ptr->max_stacks = 0;

        char *s = buf + 2;
        char *t = strchr(s, '/');
        if (!t) return PARSE_ERROR_GENERIC;

        *t++ = '\0';
        cu_ptr->weight     = (byte)atoi(s);
        cu_ptr->max_stacks = (byte)atoi(t);
    }


    /* ------------------------------------------------------------ */
    /* D: description line(s)                                       */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'D')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->text), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* P: power/effect description                                   */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'P')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->power), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* anything else is an error                                    */
    /* ------------------------------------------------------------ */
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return 0;
}

/*
 * Initialize the "flavor_info" array, by parsing an ascii "template" file
 */
errr parse_flavor_info(char* buf, header* head)
{
    int i;

    /* Current entry */
    static flavor_type* flavor_ptr;

    /* Process 'N' for "Number" */
    if (buf[0] == 'N')
    {
        int tval, sval;
        int result;

        /* Scan the value */
        result = sscanf(buf, "N:%d:%d:%d", &i, &tval, &sval);

        /* Either two or three values */
        if ((result != 2) && (result != 3))
            return (PARSE_ERROR_GENERIC);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

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
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

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
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        flavor_ptr->d_attr = d_attr;
        flavor_ptr->d_char = d_char;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[1])
            return (PARSE_ERROR_GENERIC);
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);

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

#else /* ALLOW_TEMPLATES */

#ifdef MACINTOSH
static int i = 0;
#endif

#endif /* ALLOW_TEMPLATES */
