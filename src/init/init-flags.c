#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "init-parse-internal.h"
#include "init-object-bonuses.h"
#include <ctype.h>

#ifdef ALLOW_TEMPLATES
#define TR1 0
#define TR2 1
#define TR3 2
#define TR4 3
#define RF1 4
#define RF2 5
#define RF3 6
#define RF4 7
#define RHF 8
#define VLT 9
#define CUR 10
#define UNQ 11
#define MAX_FLAG_SETS 12

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
    { "KILL_ITEM", RF2, RF2_KILL_ITEM }, { "DROP_SUPERB", RF2, RF2_DROP_SUPERB },
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
    { "HAS_WEAPON", RF3, RF3_HAS_WEAPON }, { "DROP_1D3", RF3, RF3_DROP_1D3 },
    { "RF3XXX4", RF3, RF3_RF3XXX4 },
    { "HURT_LITE", RF3, RF3_HURT_LITE }, { "STONE", RF3, RF3_STONE },
    { "HURT_FIRE", RF3, RF3_HURT_FIRE }, { "HURT_COLD", RF3, RF3_HURT_COLD },
    { "HAS_ARMOUR", RF3, RF3_HAS_ARMOUR }, { "RES_ELEC", RF3, RF3_RES_ELEC },
    { "RES_FIRE", RF3, RF3_RES_FIRE }, { "RES_COLD", RF3, RF3_RES_COLD },
    { "RES_POIS", RF3, RF3_RES_POIS }, { "RF3XXX6", RF3, RF3_RF3XXX6 },
    { "DROP_ARTEFACT", RF3, RF3_DROP_ARTEFACT },
    { "GIANT", RF3, RF3_GIANT }, { "CAT", RF3, RF3_CAT },
    { "HORROR", RF3, RF3_HORROR }, { "VAMPIRE", RF3, RF3_VAMPIRE },
    { "SPECIAL_VAULT_ONLY", RF3, RF3_SPECIAL_VAULT_ONLY },
    { "RF3XXX7", RF3, RF3_RF3XXX7 }, { "NO_SLOW", RF3, RF3_NO_SLOW },
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

    { "DWARFBANE", RF4, RF4_DWARFBANE },
    { "RF4XXX22", RF4, RF4_RF4XXX22 },
    { "EDAINBANE", RF4, RF4_EDAINBANE },
    { "RF4XXX23", RF4, RF4_RF4XXX23 },
    { "THROW_WEB", RF4, RF4_THROW_WEB }, { "RALLY", RF4, RF4_RALLY },
    { "NOLDORBANE", RF4, RF4_NOLDORBANE },
    { "RF4XXX26", RF4, RF4_RF4XXX26 },
    { "SINDARBANE", RF4, RF4_SINDARBANE },
    { "RF4XXX27", RF4, RF4_RF4XXX27 },
    { "RF4XXX28", RF4, RF4_RF4XXX28 }, { "RF4XXX29", RF4, RF4_RF4XXX29 },
    { "RF4XXX30", RF4, RF4_RF4XXX30 }, { "RF4XXX31", RF4, RF4_RF4XXX31 },
    { "RF4XXX32", RF4, RF4_RF4XXX32 },

    /* Object flag table.
     * Keep the TRn selector and TRn_* constant aligned; tools/check_flag_tables.py
     * validates this because bit positions intentionally overlap across TR sets.
     */

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
    { "SLAY_SERPENT", TR4, TR4_SLAY_SERPENT },
    { "SLAY_VAMPIRE", TR4, TR4_SLAY_VAMPIRE },
    { "SLAY_HORROR", TR4, TR4_SLAY_HORROR },
    { "SLAY_CAT", TR4, TR4_SLAY_CAT },
    { "SLAY_GIANT", TR4, TR4_SLAY_GIANT },
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
    { "STAR_IRON", TR3, TR3_STAR_IRON }, { "EASY_ID", TR3, TR3_EASY_ID },
    { "DIF_ID", TR3, TR3_DIF_ID }, { "OATH_BOOST", TR3, TR3_OATH_BOOST },
    { "TR3XXX9", TR3, TR3_TR3XXX9 }, { "TR3XX10", TR3, TR3_TR3XX10 },
    { "OATH_NEGATE", TR3, TR3_OATH_NEGATE },
    { "NO_SMITHING", TR3, TR3_NO_SMITHING },
    { "MITHRIL", TR3, TR3_MITHRIL }, { "AXE", TR3, TR3_AXE },
    { "POLEARM", TR3, TR3_POLEARM }, { "IGNORE_ACID", TR3, TR3_IGNORE_ACID },
    { "IGNORE_ELEC", TR3, TR3_IGNORE_ELEC },
    { "IGNORE_FIRE", TR3, TR3_IGNORE_FIRE },
    { "IGNORE_COLD", TR3, TR3_IGNORE_COLD }, { "THROWING", TR3, TR3_THROWING },
    { "ENCHANTABLE", TR3, TR3_ENCHANTABLE }, { "ACTIVATE", TR3, TR3_ACTIVATE },
    { "INSTA_ART", TR3, TR3_INSTA_ART }, { "EASY_KNOW", TR3, TR3_EASY_KNOW },
    { "MORE_SPECIAL", TR3, TR3_MORE_SPECIAL },
    { "WILL_DRAIN", TR3, TR3_WILL_DRAIN },
    { "HAND_AND_A_HALF", TR3, TR3_HAND_AND_A_HALF },
    { "TWO_HANDED", TR3, TR3_TWO_HANDED },
    { "LIGHT_CURSE", TR3, TR3_LIGHT_CURSE },
    { "HEAVY_CURSE", TR3, TR3_HEAVY_CURSE },
    { "PERMA_CURSE", TR3, TR3_PERMA_CURSE },

    { "IGNORE_ALL", TR3, TR3_IGNORE_ALL },

    /*
     * Object flags 4
     */
    { "UNLIGHT", TR4, TR4_UNLIGHT },
    { "ARMOR_SHATTER", TR4, TR4_ARMOR_SHATTER },
    { "DEPTH_SCALE_PS", TR4, TR4_DEPTH_SCALE_PS },
    { "PAIRED", TR4, TR4_PAIRED },
    { "SUBTLETY_THROW", TR4, TR4_SUBTLETY_THROW },
    { "BREAKS_PERMA_CURSE", TR4, TR4_BREAKS_PERMA_CURSE },
    { "LESS_SPECIAL", TR4, TR4_LESS_SPECIAL },
    { "NOBLE_ITEM",   TR4, TR4_NOBLE_ITEM },
    { "EVIL_ITEM",    TR4, TR4_EVIL_ITEM },
    { "JINX",         TR4, TR4_JINX },
    { "DEEP_CALL",    TR4, TR4_DEEP_CALL },
    { "MIN_DEPTH_SPEED", TR4, TR4_DEEP_CALL },
    { "NO_PREFIX",    TR4, TR4_NO_PREFIX },
    { "PROT_FIRE",    TR4, TR4_PROT_FIRE },
    { "PROT_COLD",    TR4, TR4_PROT_COLD },
    { "PROT_POIS",    TR4, TR4_PROT_POIS },
    { "PROT_DARK",    TR4, TR4_PROT_DARK },
    { "WEIGHT",       TR4, TR4_WEIGHT },
    { "NEG_WEIGHT",   TR4, TR4_NEG_WEIGHT },
    { "LIGHT_ARMOR",  TR4, TR4_LIGHT_ARMOR },

    /*
     * Race/Character flags
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
    { "SMT_FEANOR", RHF, RHF_SMT_FEANOR },
    { "GIFTERU", RHF, RHF_GIFTERU }, { "KINSLAYER", RHF, RHF_KINSLAYER },
    { "CURSE", RHF, RHF_CURSE }, { "TREACHERY", RHF, RHF_TREACHERY },
    { "FREE", RHF, RHF_FREE }, { "MOR_CURSE", RHF, RHF_MOR_CURSE },
    { "KHELED_ZARAM", RHF, RHF_KHELED_ZARAM },
    { "RHFXXX27", RHF, RHF_RHFXXX27 }, { "RHFXXX28", RHF, RHF_RHFXXX28 },
    { "RHFXXX29", RHF, RHF_RHFXXX29 }, { "RHFXXX30", RHF, RHF_RHFXXX30 },
    { "RHFXXX31", RHF, RHF_RHFXXX31 }, { "RHFXXX32", RHF, RHF_RHFXXX32 },

    /*
     * Vault flags
     */
    { "TEST", VLT, VLT_TEST }, { "NO_ROTATION", VLT, VLT_NO_ROTATION },
    { "TRAPS", VLT, VLT_TRAPS }, { "WEBS", VLT, VLT_WEBS },
    { "LIGHT", VLT, VLT_LIGHT }, { "SURFACE", VLT, VLT_SURFACE },
    { "QUEST", VLT, VLT_QUEST }, { "VLTXXXX8", VLT, VLT_VLTXXXX8 },
    { "VLTXXXX9", VLT, VLT_VLTXXXX9 }, { "VLTXXX10", VLT, VLT_VLTXXX10 },
    { "VLTXXX11", VLT, VLT_VLTXXX11 }, { "VLTXXX12", VLT, VLT_VLTXXX12 },
    { "VLTXXX13", VLT, VLT_VLTXXX13 }, { "VLTXXX14", VLT, VLT_VLTXXX14 },
    { "VLTXXX15", VLT, VLT_VLTXXX15 }, { "VLTXXX16", VLT, VLT_VLTXXX16 },
    { "VLTXXX17", VLT, VLT_VLTXXX17 }, { "VLTXXX18", VLT, VLT_VLTXXX18 },
    { "VLTXXX19", VLT, VLT_VLTXXX19 }, { "VLTXXX20", VLT, VLT_VLTXXX20 },
    { "VLTXXX21", VLT, VLT_VLTXXX21 }, { "VLTXXX22", VLT, VLT_VLTXXX22 },
    { "VLTXXX23", VLT, VLT_VLTXXX23 }, { "VLTXXX24", VLT, VLT_VLTXXX24 },
    { "VLTXXX25", VLT, VLT_VLTXXX25 }, { "VLTXXX26", VLT, VLT_VLTXXX26 },
    { "VLTXXX27", VLT, VLT_VLTXXX27 }, { "VLTXXX28", VLT, VLT_VLTXXX28 },
    { "VLTXXX29", VLT, VLT_VLTXXX29 },
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
    { "RES_FEAR_SHIFT", CUR, CUR_RES_FEAR_SHIFT },
    { "RES_STUN_SHIFT", CUR, CUR_RES_STUN_SHIFT },
    { "RES_CONFU_SHIFT", CUR, CUR_RES_CONFU_SHIFT },
    { "RES_HALLU_SHIFT", CUR, CUR_RES_HALLU_SHIFT },
    { "RES_POIS_SHIFT", CUR, CUR_RES_POIS_SHIFT },
    { "RES_FIRE_SHIFT", CUR, CUR_RES_FIRE_SHIFT },
    { "RES_COLD_SHIFT", CUR, CUR_RES_COLD_SHIFT },
    { "MDS_SHIFT", CUR, CUR_MDS_SHIFT },
    { "CRIT_THRESH_SHIFT", CUR, CUR_CRIT_THRESH_SHIFT },
    { "ARMOR_SIDE_SHIFT", CUR, CUR_ARMOR_SIDE_SHIFT },
    { "IDENT_DIFF", CUR, CUR_IDENT_DIFF },
    { "CHEST_WOOD", CUR, CUR_CHEST_WOOD },
    { "ABILITY_COST", CUR, CUR_ABILITY_COST },
    
    // Unique flags
    {"EARENDIL", UNQ, UNQ_EARENDIL}, { "SMT_FEANOR", UNQ, UNQ_SMT_FEANOR },
    { "WIL_FIN", UNQ, UNQ_WIL_FIN }, { "SNG_FIN", UNQ, UNQ_SNG_FIN },
    { "SNG_LUT", UNQ, UNQ_SNG_LUT }, { "WIL_TUOR", UNQ, UNQ_WIL_TUOR },
    { "SNG_MEL", UNQ, UNQ_SNG_MEL }, { "SMT_TELCHAR", UNQ, UNQ_SMT_TELCHAR },
    { "SMT_GAMIL", UNQ, UNQ_SMT_GAMIL }, { "SNG_HURIN", UNQ, UNQ_SNG_HURIN },
    { "SNG_THINGOL", UNQ, UNQ_SNG_THINGOL }, { "MIM", UNQ, UNQ_MIM },
    { "SMT_EOL", UNQ, UNQ_SMT_EOL }, { "MEL_MAEDHROS", UNQ, UNQ_MEL_MAEDHROS }, { "WIL_TURIN", UNQ, UNQ_WIL_TURIN },
    { "SMT_CELEBRIMBOR", UNQ, UNQ_SMT_CELEBRIMBOR }, { "SNG_TURGON", UNQ, UNQ_SNG_TURGON },
    { "MINSTREL", UNQ, UNQ_MINSTREL }, { "WOVEN_MASTER", UNQ, UNQ_WOVEN_MASTER }

};


/*
 * Activation type
 */

static const char *rank_name(int lvl)            /* -2...+2 -> text */
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
 *  combined_level() - return the net Affinity/Mastery/Penalty value
 *  (-2 ... +2) for one skill, summing contributions from
 *       - race-or-character RHF flags (counted once even if both set)
 *       - active curses  (each curse can add its own +1 / -1)
 *  Affinity = +1, Penalty = -1, Mastery/Grand Penalty = +/-2.
 * ------------------------------------------------------------------ */
static int combined_level(int skill)
{
    /*  lookup table: [S_*] -> {AFFINITY-bit, PENALTY-bit}  */
    static const struct { u32b aff, pen; } tbl[] = {
        { RHF_MEL_AFFINITY, RHF_MEL_PENALTY },   /* S_MEL = 0 */
        { RHF_ARC_AFFINITY, RHF_ARC_PENALTY },   /* S_ARC */
        { RHF_EVN_AFFINITY, RHF_EVN_PENALTY },   /* S_EVN */
        { RHF_STL_AFFINITY, RHF_STL_PENALTY },   /* S_STL */
        { RHF_PER_AFFINITY, RHF_PER_PENALTY },   /* S_PER */
        { RHF_WIL_AFFINITY, RHF_WIL_PENALTY },   /* S_WIL */
        { RHF_SMT_AFFINITY, RHF_SMT_PENALTY },   /* S_SMT */
        { RHF_SNG_AFFINITY, RHF_SNG_PENALTY },   /* S_SNG */
        { 0, 0 }                                 /* S_SPC - no flags yet */
    };

    /* masks --------------------------------------------------------- */
    u32b rhf  = p_info[p_ptr->prace].flags |      /* race OR character      */
                c_info[p_ptr->pcharacter].flags;
    u32b cur  = curse_flag_mask();                /* all active curses  */

    /* tally --------------------------------------------------------- */
    int v = 0;
    if (rhf & tbl[skill].aff)  v++;
    if (rhf & tbl[skill].pen)  v--;
    if (cur & tbl[skill].aff)  v++;               /* curse stack        */
    if (cur & tbl[skill].pen)  v--;

    /* clamp to -2 ... +2 as per spec ---------------------------------- */
    if (v >  2) v =  2;
    if (v < -2) v = -2;
    return v;
}

static byte rank_colour(int lvl)         /* -2 ... +2 */
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
 *  dbg_show_active_flags() - show only the flags that are ON
 *  for the current character (i386-safe C89).
 *-----------------------------------------------------------------*/
void dbg_show_active_flags(void)
{
    player_race  *rp_ptr = &p_info[p_ptr->prace];
    character_profile *current_character_profile = &c_info[p_ptr->pcharacter];

    /* live masks --------------------------------------------------- */
    u32b rhf_bits  = rp_ptr->flags | current_character_profile->flags;  /* race OR character  */
    u32b unq_bits  = current_character_profile->flags_u;                /* character-unique   */

    struct { int set; u32b bits; cptr tag; byte clr; } grp[] = {
        { RHF, rhf_bits, "RHF (Race/Character flags)",   TERM_L_GREEN },
        { UNQ, unq_bits, "UNQ (Unique-character flags)", TERM_L_BLUE  },
        { CUR, 0,        "CUR (Curse flags)",        TERM_L_RED   },
    };

    const int BUF_LEN = 79;           /* 80-col safety                */
    char  buf[BUF_LEN + 1];
    int   row = 2;

    Term_clear();
#ifdef DEBUG_CURSES
    Term_putstr(0, row++, -1, TERM_YELLOW,
                "*** DEBUG: Tale debug - a:add-curse  c:clear-all  e:+death  1-3:+sils  any:key:exit ***");
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
                "Active curses & blessings (id : name [stacks])");

    for (int id = 0; id < z_info->cu_max; id++)
    {
        int cnt = CURSE_GET(id);
        if (!cnt) continue;                       /* skip empty slots */
        bool is_curse = (cnt > 0);
        int magnitude = is_curse ? cnt : -cnt;
        cptr display_name = cu_name + (is_curse ? cu_info[id].name
                                                : (cu_info[id].blessing_name ? cu_info[id].blessing_name
                                                                             : cu_info[id].name));
#ifdef DEBUG_CURSES
        const char *pow = is_curse ? (cu_text + cu_info[id].power)
                                   : (cu_info[id].blessing_power ? (cu_text + cu_info[id].blessing_power) : NULL);
        c_put_str(is_curse ? TERM_WHITE : TERM_L_GREEN,
                  format("  %2d : %-26s [%d]  - %s",
                         id,
                         display_name,
                         magnitude,
                         pow ? pow : ""),
                  row++, 2);
#else
        c_put_str(is_curse ? TERM_WHITE : TERM_L_GREEN,
                  format("  %2d : %-30s [%d]",
                         id,
                         display_name,
                         magnitude),
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
            if (get_check("Erase ALL curses for this tale? "))
            {
                metarun_clear_all_curses();
                dbg_show_active_flags();
                break;
            }
            continue;
        }
        else if (ch == 'e')       /* +1 death shortcut */
        {
            metarun_update_on_exit(true, false, 0, 0);
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
        * so metarun finalization will purge it just like the Ctrl-Y debug path.
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

errr grab_one_flag(u32b** flag, cptr errstr, cptr what)
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

#endif /* ALLOW_TEMPLATES */
