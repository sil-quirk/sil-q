/* File: types.h */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

/*
 * Note that "char" may or may not be signed, and that "signed char"
 * may or may not work on all machines.  So always use "s16b" or "s32b"
 * for signed values.  Also, note that unsigned values cause math problems
 * in many cases, so try to only use "u16b" and "u32b" for "bit flags",
 * unless you really need the extra bit of information, or you really
 * need to restrict yourself to a single byte for storage reasons.
 *
 * Also, if possible, attempt to restrict yourself to sub-fields of
 * known size (use "s16b" or "s32b" instead of "int", and "byte" instead
 * of "bool"), and attempt to align all fields along four-byte words, to
 * optimize storage issues on 32-bit machines.  Also, avoid "bit flags"
 * since these increase the code size and slow down execution.  When
 * you need to store bit flags, use one byte per flag, or, where space
 * is an issue, use a "byte" or "u16b" or "u32b", and add special code
 * to access the various bit flags.
 *
 * Many of these structures were developed to reduce the number of global
 * variables, facilitate structured program design, allow the use of ascii
 * template files, simplify access to indexed data, or facilitate efficient
 * clearing of many variables at once.
 *
 * Note that certain data is saved in multiple places for efficient access,
 * and when modifying the data in one place it must also be modified in the
 * other places, to prevent the creation of inconsistant data.
 */

/**** Available Types ****/

/*
 * An array of 256 byte's
 */
typedef byte byte_256[256];

/*
 * An array of 256 u16b's
 */
typedef u16b u16b_256[256];

/*
 * An array of MAX_DUNGEON_WID byte's
 */
typedef byte byte_wid[MAX_DUNGEON_WID];

/*
 * An array of MAX_DUNGEON_WID s16b's
 */
typedef s16b s16b_wid[MAX_DUNGEON_WID];

/**** Available Structs ****/

typedef struct maxima maxima;
typedef struct feature_type feature_type;
typedef struct object_kind object_kind;
typedef struct ability_type ability_type;
typedef struct artefact_type artefact_type;
typedef struct ego_item_type ego_item_type;
typedef struct monster_blow monster_blow;
typedef struct monster_race monster_race;
typedef struct monster_lore monster_lore;
typedef struct vault_type vault_type;
typedef struct object_type object_type;
typedef struct monster_type monster_type;
typedef struct alloc_entry alloc_entry;
typedef struct owner_type owner_type;
typedef struct store_type store_type;
typedef struct player_race player_race;
typedef struct player_character player_house;
typedef struct hist_type hist_type;
typedef struct story_type story_type;
typedef struct curse_type curse_type;
typedef struct major_blessing_type major_blessing_type;
typedef struct player_other player_other;
typedef struct player_type player_type;
typedef struct start_item start_item;
typedef struct names_type names_type;
typedef struct flavor_type flavor_type;
typedef struct editing_buffer editing_buffer;
typedef struct autoinscription autoinscription;
typedef struct style_type style_type;
typedef struct quest_type quest_type;
typedef struct oath_type oath_type;

/**** Available structs ****/

/*
 * Information about maximal indices of certain arrays
 * Actually, these are not the maxima, but the maxima plus one
 */
struct maxima
{
    u32b fake_text_size;
    u32b fake_name_size;

    u16b f_max; /* Max size for "f_info[]" */
    u16b k_max; /* Max size for "k_info[]" */
    u16b b_max; /* Max size for "b_info[]" */
    u16b art_max; /* Max size for "a_info[]" */
    u16b e_max; /* Max size for "e_info[]" */
    u16b r_max; /* Max size for "r_info[]" */
    u16b v_max; /* Max size for "v_info[]" */
    u16b p_max; /* Max size for "p_info[]" */
    u16b h_max; /* Max size for "h_info[]" */
    u16b st_max; /* Max size for "st_info[]" */
    u16b cu_max; /* Max size for "cu_info[]" */
    u16b mb_max; /* Max size for "major blessing info[]" */
    u16b c_max; /* Max size for "c_info[]" */
    u16b quest_max; /* Max size for "quest_info[]" */
    u16b oath_max; /* Max size for "oath_info[]" */
    u16b flavor_max; /* Max size for "flavor_info[]" */
    u16b o_max; /* Max size for "o_info[]" */
    u16b ghost_other_max; /* Max maintainer player ghost templates */
    u16b art_spec_max; /* Max number of special artefacts */
    u16b art_norm_max; /* Max number for normal artefacts (special - normal) */
    u16b art_rand_max; /* Max number of random artefacts */
    u16b art_self_made_max; /* Max number of self-made artefacts */
    u16b rt_max;           /* ↑ total run-type records                         */
    u16b style_max;        /* Max size for "style_info[]" */
};

/*
 * Information about terrain "features"
 */
struct feature_type
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) */

    byte mimic; /* Feature to mimic */

    byte extra; /* Extra byte (unused) */

    s16b unused; /* Extra bytes (unused) */

    byte d_attr; /* Default feature attribute */
    char d_char; /* Default feature character */

    byte x_attr; /* Desired feature attribute */
    char x_char; /* Desired feature character */
};

/*
 * Information about object "kinds", including player knowledge.
 *
 * Only "aware" and "tried" are saved in the savefile
 */
struct object_kind
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) */

    byte tval; /* Object type */
    byte sval; /* Object sub type */

    s16b pval; /* Object extra info */

    s16b att; /* Bonus to hit */
    s16b evn; /* Sil - Bonus to evasion */

    byte dd, ds; /* Damage dice/sides */
    byte pd, ps; /* Sil - Protection dice/sides */

    s16b weight; /* Weight */

    s32b cost; /* Object "base cost" */

    u32b flags1; /* Flags, set 1 */
    u32b flags2; /* Flags, set 2 */
    u32b flags3; /* Flags, set 3 */

    byte locale[4]; /* Allocation level(s) */
    byte chance[4]; /* Allocation chance(s) */

    byte abilities; // Number of abilities
    byte skilltype[4]; // Skill-types for the granted abilities
    byte abilitynum[4]; // Ability numbers for these

    byte level; /* Level */
    byte extra; /* Something */

    byte d_attr; /* Default object attribute */
    char d_char; /* Default object character */

    byte x_attr; /* Desired object attribute */
    char x_char; /* Desired object character */

    u16b flavor; /* Special object flavor (or zero) */

    bool aware; /* The player is "aware" of the item's effects */

    bool tried; /* The player has "tried" one of the items */

    byte squelch; /* Squelch setting for the particular item */

    bool everseen; /* Used to despoilify squelch menus */
};

/*
 * Information about abilities.
 */
struct ability_type
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) */

    byte skilltype; /* Skill type */
    byte abilitynum; /* Ability number within a skill */

    byte level; /* Prerequisite skill level */
    byte prereqs; /* Number of prerequisite abilities */
    byte prereq_skilltype[4]; /* Skill type (for prerequisites) */
    byte prereq_abilitynum[4]; /* The ability within that skill (for
                                  prerequisites) */

    byte tval[ABILITY_TVALS_MAX]; /* Legal tval */
    byte min_sval[ABILITY_TVALS_MAX]; /* Minimum legal sval */
    byte max_sval[ABILITY_TVALS_MAX]; /* Maximum legal sval */
};

/*
 * Information about "artefacts".
 *
 * Note that the save-file only writes "cur_num" and "found_num" to the
 * savefile, except for the random artefacts
 *
 * Note that "max_num" is always "1" (if that artefact "exists")
 */
struct artefact_type
{
    char name[MAX_LEN_ART_NAME]; /* Name */
    u32b text; /* Description (offset) */

    byte tval; /* Artefact type */
    byte sval; /* Artefact sub type */

    s16b pval; /* Artefact extra info */

    s16b att; /* Bonus to hit */
    s16b evn; /* Bonus to evasion */

    byte dd, ds; /* Damage when hits */
    byte pd, ps; /* Protection dice and sides */

    s16b weight; /* Weight */

    s32b cost; /* Artefact "cost" */

    u32b flags1; /* Artefact Flags, set 1 */
    u32b flags2; /* Artefact Flags, set 2 */
    u32b flags3; /* Artefact Flags, set 3 */

    byte level; /* Artefact level */
    byte rarity; /* Artefact rarity */

    byte cur_num; /* Number created (0 or 1) */
    byte found_num; /* Number found (0 or 1) */
    byte max_num; /* Unused (should be "1") */

    byte activation; /* Activation to use */
    u16b time; /* Activation time */
    u16b randtime; /* Activation time dice */

    byte d_attr; /* Default artefact attribute */
    char d_char; /* Default artefact character */

    byte abilities; // Number of abilities
    byte skilltype[4]; // Skill-types for the granted abilities
    byte abilitynum[4]; // Ability numbers for these
};

/*
 * Information about special items.
 */
struct ego_item_type
{
    u32b name; /* Name (offset) */
    u32b text; /* Description (offset) */

    s32b cost; /* Ego-item "cost" */

    u32b flags1; /* Ego-Item Flags, set 1 */
    u32b flags2; /* Ego-Item Flags, set 2 */
    u32b flags3; /* Ego-Item Flags, set 3 */

    byte level; /* Minimum level */
    byte max_level; /* Maximum level */
    byte rarity; /* Object rarity */

    byte tval[EGO_TVALS_MAX]; /* Legal tval */
    byte min_sval[EGO_TVALS_MAX]; /* Minimum legal sval */
    byte max_sval[EGO_TVALS_MAX]; /* Maximum legal sval */

    byte abilities; // Number of abilities
    byte skilltype[4]; // Skill-types for the granted abilities
    byte abilitynum[4]; // Ability numbers for these

    byte max_att; /* Maximum to-hit bonus */
    byte to_dd; /* bonus damge dice */
    byte to_ds; /* bonus damage sides */
    byte max_evn; /* Maximum to-e bonus */
    byte to_pd; /* bonus protection dice */
    byte to_ps; /* bonus protection sides */
    byte max_pval; /* Maximum pval */

    bool aware; /* Has its type been detected this game? */
    bool everseen; /* Do not spoil squelch menus */
    bool squelch; /* Squelch this special item */
};

/*
 * Monster blow structure
 *
 *	- Method (RBM_*)
 *	- Effect (RBE_*)
 *	- Damage Dice
 *	- Damage Sides
 */
struct monster_blow
{
    byte method;
    byte effect;
    s16b att;
    byte dd;
    byte ds;
};

/*
 * Monster "race" information, including racial memories
 *
 * Note that "d_attr" and "d_char" are used for MORE than "visual" stuff.
 *
 * Note that "x_attr" and "x_char" are used ONLY for "visual" stuff.
 *
 * Note that "cur_num" (and "max_num") represent the number of monsters
 * of the given race currently on (and allowed on) the current level.
 * This information yields the "dead" flag for Unique monsters.
 *
 * Note that "max_num" is reset when a new player is created.
 * Note that "cur_num" is reset when a new level is created.
 *
 * Maybe "x_attr", "x_char", "cur_num", and "max_num" should
 * be moved out of this array since they are not read from
 * "monster.txt".
 */
struct monster_race
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) */

    byte hdice; /* Creatures hit dice count */
    byte hside; /* Creatures hit dice sides */

    s16b evn; /* Bonus to evasion */
    byte pd; /* Protection dice */
    byte ps; /* Protection sides */

    byte speed; /* Speed (normally 110) */
    s16b light; /* Light/Dark radius (if any) */

    s16b sleep; /* Starting penalty to alertness */
    s16b per; /* Perception */
    s16b stl; /* Stealth */
    s16b wil; /* Will */

    s16b extra; /* Unused (for now) */

    byte freq_ranged; /* Ranged attack frequency */
    byte spell_power; /* Power of (damage-dealing) spells */
    u32b mon_power; /* Monster Power Rating */

    u32b flags1; /* Flags 1 (general) */
    u32b flags2; /* Flags 2 (abilities) */
    u32b flags3; /* Flags 3 (race/resist) */
    u32b flags4; /* Flags 4 ('spells') */

    monster_blow blow[MONSTER_BLOW_MAX]; /* Up to four blows per round */

    byte level; /* Level of creature */
    byte rarity; /* Rarity of creature */

    byte d_attr; /* Default monster attribute */
    char d_char; /* Default monster character */

    byte x_attr; /* Desired monster attribute */
    char x_char; /* Desired monster character */

    byte max_num; /* Maximum population allowed per level */
    byte cur_num; /* Monster population on current level */
};

/*
 * Monster "lore" information
 *
 * Note that these fields are related to the "monster recall" and can
 * be scrapped if space becomes an issue, resulting in less "complete"
 * monster recall (no knowledge of spells, etc). XXX XXX XXX
 *
 * ToDo: The "r_" prefix is no longer needed and should be removed.
 */
struct monster_lore
{
    s16b deaths; /* Count deaths from this monster */

    s16b psights; /* Count sightings of this monster in this life */
    s16b tsights; /* Count sightings of this monster in all lives */

    s16b pkills; /* Count monsters killed in this life */
    s16b tkills; /* Count monsters killed in all lives */

    byte notice; /* Number of times it has been seen noticing the player */
    byte ignore; /* Number of times it has been seen not noticing the player */

    byte drop_item; /* Max number of items dropped at once */

    byte ranged; /* Observed ranged attacks */
    byte mana; /* Max mana */
    byte spell_power; /* Power of (damage-dealing) spells */

    byte blows[MONSTER_BLOW_MAX]; /* Number of times each blow type was seen */

    u32b flags1; /* Observed racial flags */
    u32b flags2; /* Observed racial flags */
    u32b flags3; /* Observed racial flags */
    u32b flags4; /* Observed racial flags */
};

/*
 * Information about "vault generation"
 */
struct vault_type
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) */

    byte typ; /* Vault type */

    byte depth; /* Vault rating */

    byte rarity; /* Vault rarity */

    byte hgt; /* Vault height */
    byte wid; /* Vault width */

    byte forge; /* Is there a forge in it? */

    byte color; /* Wall color (0 = use depth default) */

    u32b flags; /* Vault Flags (ie VLT flags) */

    /* Optional style weights for this vault */
    byte style_count;            /* number of entries in arrays below */
    s16b style_idx[16];          /* style indices from style.txt */
    s16b style_weight[16];       /* corresponding weights */
};

/*
 * Object information, for a specific object.
 *
 * Note that a "discount" on an item is permanent and never goes away.
 *
 * Note that inscriptions are now handled via the "quark_str()" function
 * applied to the "note" field, which will return NULL if "note" is zero.
 *
 * Note that "object" records are "copied" on a fairly regular basis,
 * and care must be taken when handling such objects.
 *
 * Note that "object flags" must now be derived from the object kind,
 * the artefact and special item indexes, and the two "xtra" fields.
 *
 * Each cave grid points to one (or zero) objects via the "o_idx"
 * field (above).  Each object then points to one (or zero) objects
 * via the "next_o_idx" field, forming a singly linked list, which
 * in game terms, represents a "stack" of objects in the same grid.
 *
 * Each monster points to one (or zero) objects via the "hold_o_idx"
 * field (below).  Each object then points to one (or zero) objects
 * via the "next_o_idx" field, forming a singly linked list, which
 * in game terms, represents a pile of objects held by the monster.
 *
 * The "held_m_idx" field is used to indicate which monster, if any,
 * is holding the object.  Objects being held have "ix=0" and "iy=0".
 */
struct object_type
{
    s16b k_idx; /* Kind index (zero if "dead") */

    s16b image_k_idx; /* Hallucinatory kind index */

    byte iy; /* Y-position on map, or zero */
    byte ix; /* X-position on map, or zero */

    byte tval; /* Item type (from kind) */
    byte sval; /* Item sub-type (from kind) */

    s16b pval; /* Item extra-parameter */

    byte discount; /* Discount (if any) */

    byte number; /* Number of items */

    s16b weight; /* Item weight */

    byte name1; /* Artefact type, if any */
    byte name2; /* Ego-Item type, if any */

    byte pickup; /* Auto pick up this item next time it is stepped on */
    s16b pickup_slot; /* Preferred inventory slot when auto-picked */

    byte xtra1; /* Extra info type */

    s16b att; /* Bonus to attack */
    s16b evn; /* Bonus to evasion */

    byte dd, ds; /* Damage dice/sides */
    byte pd, ps; /* Protection dice/sides */

    s16b timeout; /* Timeout Counter */

    u32b ident; /* Special flags (was byte) */

    byte marked; /* Object is marked */

    u16b obj_note; /* Inscription index */

    s16b next_o_idx; /* Next object in stack (if any) */

    s16b held_m_idx; /* Monster holding us (if any) */

    byte abilities; // Number of abilities
    byte skilltype[8]; // Skill-types for the granted abilities (8 = 4 for
                       // object base + 4 for special or artefact)
    byte abilitynum[8]; // Ability numbers for these

    s32b unused1; // Room for expansion without breaking savefiles
    s32b unused2; // Room for expansion without breaking savefiles
    s32b unused3; // Room for expansion without breaking savefiles
    s32b unused4; // Room for expansion without breaking savefiles
};

/*
 * Monster information, for a specific monster.
 *
 * Note: fy, fx constrain dungeon size to 256x256
 *
 * The "hold_o_idx" field points to the first object of a stack
 * of objects (if any) being carried by the monster (see above).
 */
struct monster_type
{
    s16b r_idx; /* Monster race index */

    s16b image_r_idx; /* Monster race index (when hallucinating) */

    byte fy; /* Y location on map */
    byte fx; /* X location on map */

    s16b hp; /* Current Hit points */
    s16b maxhp; /* Max Hit points */

    s16b alertness; /* Positive numbers can move and act, negative are unwary or
                       asleep */
    byte skip_next_turn; /* used to skip the monster's next turn in various
                            circumstances */
    byte skip_this_turn; /* used to make sure the monster doesn't do anything
                            this turn (Song of Mastery) */

    byte mspeed; /* Monster "speed" */
    byte energy; /* Monster "energy" */

    byte stunned; /* Monster is stunned */
    byte confused; /* Monster is confused */
    s16b slowed; /* Monster is slowed */
    s16b hasted; /* Monster is hasted */

    byte stance; /* Fleeing, Timid, Cautious, Aggressive */
    s16b morale; /* Overall morale */
    s16b tmp_morale; /* Temporary modifier to morale */

    byte cdis; /* Current dis from player */

    u32b mflag; /* Extra monster flags */

    bool ml; /* Monster is "visible" */

    byte noise; /* amount of noise monster made this turn */

    byte encountered; /* Whether the monster has been encountered yet */

    s16b hold_o_idx; /* Object being held (if any) */

    byte target_y; /* Monster target */
    byte target_x;

    s16b wandering_idx; /* Where the monster is wandering while unwary (if
                           anywhere) */
    byte wandering_dist; /* The distance to the destination */

    byte min_range; /* What is the closest we want to be? */ /* Not saved */
    byte best_range; /* How close do we want to be? */ /* Not saved */

    byte mana; /* Current mana level */
    byte song; /* Current song */
    byte song_contest_stacks; /* Stacks accumulated from Song of Contest */
    byte song_lament_stacks; /* Stacks accumulated from Song of Lament */
    byte song_lockout_timer; /* Turns before monster can sing again */
    byte song_hp_loss_lo; /* Low byte of cumulative Song HP penalty */
    s32b song_contest_last_turn; /* Last player turn Contest stack changed */
    s32b song_lament_last_turn; /* Last player turn Lament stack changed */
    s16b song_will_penalty; /* Permanent Will penalty from duels */
    s16b song_stealth_penalty; /* Permanent Stealth penalty from duels */
    s16b song_evasion_penalty; /* Permanent Evasion penalty from duels */
    byte song_armor_dice_penalty; /* Permanent armour dice penalty */
    byte song_hp_loss_hi; /* High byte of cumulative Song HP penalty */
    byte song_contest_completed; /* 1 if Contest duel completed (won or lost), 0 otherwise */
    byte song_lament_completed; /* 1 if Lament duel completed (won or lost), 0 otherwise */

    s16b consecutive_attacks; /* How many times it has attacked the player in a
                                 row immediately prior to now */
    s16b turns_stationary; /* How many times it has stayed still in a row
                              immediately prior to now */

    byte blow_dd_reduction[MONSTER_BLOW_MAX]; /* Reduction applied to blow damage dice */
    byte blow_ds_reduction[MONSTER_BLOW_MAX]; /* Reduction applied to blow damage sides */
    byte armor_ps_reduction; /* Reduction applied to protection sides */
    byte shatter_padding[3]; /* Reserved for future shattering data */

    byte previous_action[ACTION_MAX]; /* What the monster did on its previous
                                         turns */
};

/*
 * An entry for the object/monster allocation functions
 *
 * Pass 1 is determined from allocation information
 * Pass 2 is determined from allocation restriction
 * Pass 3 is determined from allocation calculation
 */
struct alloc_entry
{
    s16b index; /* The actual index */

    byte level; /* Base dungeon level */
    byte prob1; /* Probability, pass 1 */
    byte prob2; /* Probability, pass 2 */
    byte prob3; /* Probability, pass 3 */

    u16b total; /* Unused for now */
};

/*
 * A store owner
 */
struct owner_type
{
    u32b owner_name; /* Name (offset) */

    s16b max_cost; /* Purse limit */

    byte max_inflate; /* Inflation (max) */
    byte min_inflate; /* Inflation (min) */

    byte haggle_per; /* Haggle unit */

    byte insult_max; /* Insult limit */

    byte owner_race; /* Owner race */
};

/*
 * A store, with an owner, various state flags, a current stock
 * of items, and a table of items that are often purchased.
 */
struct store_type
{
    byte owner; /* Owner index */

    s16b insult_cur; /* Insult counter */

    s16b good_buy; /* Number of "good" buys */
    s16b bad_buy; /* Number of "bad" buys */

    s32b store_open; /* Closed until this turn */

    byte stock_num; /* Stock -- Number of entries */
    s16b stock_size; /* Stock -- Total Size of Array */
    object_type* stock; /* Stock -- Actual stock items */
};

/*
 * Starting equipment entry
 */
struct start_item
{
    byte tval; /* Item's tval */
    byte sval; /* Item's sval */
    byte min; /* Minimum starting amount */
    byte max; /* Maximum starting amount */
};

/*
 * Player racial info
 */
struct player_race
{
    u32b name; /* Name (offset) */
    u32b text; /* Description (offset) */

    s16b r_adj[A_MAX]; /* Racial stat bonuses */

    int b_age; /* base age */
    int m_age; /* mod age */

    byte b_ht; /* base height */
    byte m_ht; /* mod height */
    byte b_wt; /* base weight */
    byte m_wt; /* mod weight */

    u32b choice[FLAG_WORDS]; /* Legal house choices */

    start_item start_items[MAX_START_ITEMS]; /* The starting inventory */

    s16b hist; /* Starting history index */

    u32b flags; /* Racial Flags (ie RHF flags) */
};

/*
 * Player house info
 */
struct player_character
{
    u32b name; /* Name (offset)           eg 'House of Feanor' */
    u32b alt_name; /* Alternate Name (offset) eg 'Feanor's House'  */
    u32b start_string; /* Short Name (offset)     eg 'Feanor'          */
    u32b text; /* Descrption (offset) */

    s16b h_adj[A_MAX]; /* House stat bonuses */
    /** Up to N “stat:ability” pairs. */
    s16b  a_adj[HOUSE_ABILITY_MAX][2]; /* [i][0]=stat, [i][1]=ability] */

    u32b flags; /* House Flags (ie RHF flags) */
    u32b flags_u; /* House Flags (ie RHF flags) */
    byte power; /* Power rating: 0=weak, 1=average, 2=powerful, 3=very powerful */
    start_item start_items[MAX_START_ITEMS];       /* NEW: bonus kit */
};

/*
 * Player background information
 */
struct hist_type
{
    u32b text; /* Text (offset) */

    byte roll; /* Frequency of this entry */
    byte chart; /* Chart index */
    byte next; /* Next chart index */
    byte house; /* House to associate with */
};

// Storylines

struct story_type
{
    u32b name;     /* Name (offset) */
    u32b text;     /* Description (offset) */

    byte st_type;  /* user-defined type */
    byte order;    /* display / sequencing order */
    u32b runtypes; /* bitmask of allowed run types; 0 => applies to ALL */
};

// Curses

/* Existing … */
typedef struct curse_type              /* one entry in cu_info[]          */
{
    s16b             name;             /* index in cu_name */ 
    s16b             blessing_name;    /* blessing name index */ 
    u32b             text;             /* offset in the big text pool  */
    u32b             blessing_text;    /* blessing description offset  */
    u32b             power;            /* NEW – offset of P:-effect text       */
    u32b             blessing_power;   /* offset of blessing effect text       */
    s16b             cu_adj[A_MAX];    /* stat adjustments  */
    u32b             flags;            /* RHF flags contributed by curse */
    u32b             blessing_flags;   /* RHF flags contributed by blessing */
    u32b             flags_u;          /* CUR flags contributed by curse */
    u32b             blessing_flags_u; /* CUR flags contributed by blessing */
    byte  weight;              /* selection weight   (default 1)  */
    byte  max_stacks;          /* hard cap per meta-run (0 = ∞)   */    
}
curse_type;

/*
 * Major blessing definitions (data-driven metarun upgrades)
 */
struct major_blessing_type
{
    s16b name;         /* Display name offset (mb_name)                 */
    u32b short_desc;   /* Short descriptor for stats listing             */
    u32b detail_desc;  /* Menu description / tooltip                     */
    u32b unlock_msg;   /* Message shown when the blessing is unlocked    */
    byte effect;       /* Effect enum (metarun_major_effect)             */
    byte cost;         /* Blessing point cost                            */
    byte reserved[2];  /* Alignment / future expansion                   */
};


#define RUNTYPE_BLESSING_THRESHOLD_MODES 3

enum runtype_blessing_mode {
    RUNTYPE_BLESSING_MODE_NORMAL = 0,
    RUNTYPE_BLESSING_MODE_EASIER = 1,
    RUNTYPE_BLESSING_MODE_HARDER = 2,
    RUNTYPE_BLESSING_MODE_COUNT = RUNTYPE_BLESSING_THRESHOLD_MODES
};

typedef struct runtype_type {
    u16b id;
    char name[32];
    u64b start_curses;             /* default curses mask (bits 0..63)      */
    byte curse_stacks[METAR_CURSE_SLOTS];         /* stack count for each curse (0 = disabled) */
    byte colour;                   /* display colour (TERM_*)               */
    byte win_con;                  /* target Silmarils to win (default 15)  */
    u16b blessing_threshold_modes[RUNTYPE_BLESSING_THRESHOLD_MODES]; /* score pool per blessing tier */
    u32b heroes[FLAG_WORDS];       /* applicable heroes (max 64)            */
} runtype_type;

/*
 * Depth-based visual style definition (data-driven from lib/edit/style.txt)
 * Each style belongs to a group (GREY/GREEN/BLUE/RED/PURPLE/BLACK) and
 * provides tile coordinates for walls, veins, floors, and a base door tile.
 */
struct style_type {
    u32b name;               /* Name (offset) */
    byte group;              /* GROUP_* identifier */
    /* Microchasm atlas coordinates: row/col for each element */
    byte wall_row,  wall_col;
    byte vein_row,  vein_col;
    /* Floors: support multiple options (first used if no selection) */
    byte floor_row, floor_col;            /* legacy single values */
    byte floor_count;                     /* number of floor variants */
    byte floor_rowv[8], floor_colv[8];    /* up to 8 variants */
    /* Doors: support multiple options (first used if no selection) */
    byte door_row,  door_col;             /* legacy base tile; open +1, broken +2 */
    byte door_count;                      /* number of door variants */
    byte door_rowv[8], door_colv[8];      /* up to 8 variants */
    bool vein_defined;       /* true if Y: was specified in style.txt */
};

/*
 * Information about quest types
 */
struct quest_type
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) - legacy field for compatibility */
    u32b init_text; /* Initialization text (I:) (offset) */
    u32b completion_text; /* Completion text (W:) (offset) */
    u32b title_text; /* Title text (T:) (offset) */
    u32b challenge_text; /* Challenge text (C:) (offset) */
    
    byte quest_num; /* Quest index (TULKAS, AULE, MANDOS, NIENA) */
    byte difficulty; /* Difficulty level */
    byte reward_type; /* Type of reward (ability, item, etc.) */
    byte reward_value; /* Specific reward identifier */
    byte oath_id; /* Associated oath ID (links to oath_info array) */
    byte quest_type; /* Quest type (Y: field - 0=vault, 1=roulet) */
    byte stat_bonuses[4]; /* Stat bonuses (S: field - str:dex:con:gra) */
    byte skill_type; /* Skill type for bonuses (K: field first part) */
    byte skill_bonus; /* Skill bonus amount (K: field second part) */
    byte ability_type; /* Special ability type (A: field first part) */
    byte ability_id; /* Special ability ID (A: field second part) */
    
    /* Quest State Mapping (V: and M: fields) */
    u32b quest_state_var; /* Quest state variable name (V: field) (offset) */
    u32b metarun_quest_id; /* Metarun quest ID (M: field) (offset) */
    
    /* Parametric Formula System (P: field) */
    byte formula_type; /* 0=hardcoded, 1=linear_decay, 2=scaled_range, 3=fixed_percent */
    float formula_params[4]; /* Parameters for formula calculation */
    byte depth_min; /* Minimum depth for formula */
    byte depth_max; /* Maximum depth for formula */
    
    /* Eligibility Requirements (E: field) */
    byte eligibility_type; /* 0=none, 1=skill_min, 2=skill_max, 3=depth_range, 4=stat_min */
    byte eligibility_skill; /* Skill type for skill-based requirements */
    byte eligibility_value; /* Minimum/maximum value for requirement */
    byte eligibility_depth_min; /* Minimum depth for depth-based requirements */
    byte eligibility_depth_max; /* Maximum depth for depth-based requirements */
    
    u32b flags; /* Quest flags */
};

/*
 * Information about oath types 
 */
struct oath_type
{
    u32b name; /* Name (offset) */
    u32b text; /* Text (offset) */
    u32b pledge_text; /* Pledge text (P:) (offset) */
    u32b forbidden_text; /* Forbidden action text (F:) (offset) */
    u32b reward_text; /* Reward description text (R:) (offset) */
    u32b confirmation_prompt; /* Confirmation prompt text (offset) */
    u32b curse_message; /* Curse message text (offset) */
    u32b permanent_message; /* Permanent consequence message (offset) */
    u32b death_message; /* Death/escape message (offset) */
    u32b banned_text; /* Birth screen banned text (offset) */
    
    byte oath_num; /* Oath index (MERCY, SILENCE, IRON, SMITH) */
    byte difficulty; /* Difficulty level */
    byte restrictions; /* Oath restrictions flags */
    byte reward_type; /* Type of reward */
    byte reward_value; /* Specific reward identifier */
    byte stat_bonuses[4]; /* Stat bonuses (S: field - str:dex:con:gra) */
    byte skill_type; /* Skill type for bonuses (K: field first part) */
    byte skill_bonus; /* Skill bonus amount (K: field second part) */
    u32b flags; /* Oath flags */
};

/*
 * Some more player information
 *
 * This information is retained across player lives
 */
struct player_other
{
    char full_name[32]; /* Full name */
    char base_name[32]; /* Base name */

    bool opt[OPT_MAX]; /* Options */

    u32b window_flag[ANGBAND_TERM_MAX]; /* Window flags */

    byte hitpoint_warn; /* Hitpoint warning (0 to 9) */

    byte delay_factor; /* Delay factor (0 to 9) */

    byte main_combat_rolls; /* Main terminal combat rolls (0-3) */
};

/*
 * Most of the "player" information goes here.
 *
 * This stucture gives us a large collection of player variables.
 *
 * This entire structure is wiped when a new character is born.
 *
 * This structure is more or less laid out so that the information
 * which must be saved in the savefile precedes all the information
 * which can be recomputed as needed.
 */
struct player_type
{
    s16b py; /* Player location */
    s16b px; /* Player location */

    byte prace; /* Race index */
    byte phouse; /* House index */

    s16b game_type; /* Whether this is a normal game (=0), tutorial (<0), puzzle
                       (>0) */

    s16b age; /* Character's age */
    s16b ht; /* Height */
    s16b wt; /* Weight */
    s16b morgoth_state; /* Spare */

    s16b max_depth; /* Max depth */
    s16b depth; /* Cur depth */

    s32b new_exp; /* New experience */
    s32b exp; /* Total experience */

    s32b encounter_exp; /* Total experience from ecountering monsters */
    s32b kill_exp; /* Total experience from killing monsters */
    s32b descent_exp; /* Total experience from descending to new levels */
    s32b ident_exp; /* Total experience from identifying objects */

    s16b mhp; /* Max hit pts */
    s16b chp; /* Cur hit pts */
    u16b chp_frac; /* Cur hit frac (times 2^16) */

    s16b msp; /* Max mana pts */
    s16b csp; /* Cur mana pts */
    u16b csp_frac; /* Cur mana frac (times 2^16) */

    s16b stat_base[A_MAX]; /* The base ('internal') stat values */
    s16b stat_drain[A_MAX]; /* The negative modifier from stat drain */

    s16b skill_base[S_MAX]; /* The base skill values */

    byte innate_ability[S_MAX][ABILITIES_MAX]; /* Whether or not you personally
                                                  have each ability */
    byte active_ability[S_MAX][ABILITIES_MAX]; /* Whether or not you are using
                                                  each ability */

    s16b last_attack_m_idx; // m_idx of the monster attacked last round (if any)
    s16b consecutive_attacks; // number of rounds spent attacking this monster
    s16b bane_type; // the monster type you have specialized against
    byte previous_action[ACTION_MAX]; // stores the previous actions you have
                                      // taken
    byte focused; // stores whether you are currently focusing for an attack

    s16b fast; /* Timed -- Fast */
    s16b slow; /* Timed -- Slow */
    s16b blind; /* Timed -- Blindness */
    s16b entranced; /* Timed -- Entrancement */
    s16b confused; /* Timed -- Confusion */
    s16b afraid; /* Timed -- Fear */
    s16b image; /* Timed -- Hallucination */
    s16b poisoned; /* Timed -- Poisoned */
    s16b cut; /* Timed -- Cut */
    s16b stun; /* Timed -- Stun */
    s16b darkened; /* Timed -- magical darkness */

    s16b rage; /* Timed -- Rage */
    s16b tmp_str; /* Timed -- Strength */
    s16b tmp_dex; /* Timed -- Dexterity */
    s16b tmp_con; /* Timed -- Constitution */
    s16b tmp_gra; /* Timed -- Grace */
    s16b tim_invis; /* Timed -- See Invisible */

    s16b oppose_fire; /* Timed -- oppose heat */
    s16b oppose_cold; /* Timed -- oppose cold */
    s16b oppose_pois; /* Timed -- oppose poison */

    s16b tmp_per; /* Timed -- Perception */

    s16b song_challenge_effect; /* Timed -- Song of Challenge lingering debuff */
    s16b song_elbereth_effect; /* Timed -- Song of Elbereth lingering debuff */

    s16b energy; /* Current energy */

    s16b food; /* Current nutrition */

    u16b stairs_taken; /* The number of times stairs have been used */
    u16b
        staircasiness; /* Gets higher when stairs are taken and slowly decays */

    u16b fixed_forge_count; /* The number of mandatory forges that have been
                               generated */
    u16b forge_count; /* The number of forges that have been generated */

    byte stealth_mode; /* Stealth mode */
    byte climbing; /* The player is climbing over a chasm */

    byte self_made_arts; /* Number of self-made artefacts so far */

    s16b vengeance; /* Vengeance counter */

    byte song1; /* Current song */
    byte song2; /* Current minor theme */
    s16b song_duration; /* The duration of the current song */
    s16b song_target_idx; /* Current targeted monster for duel songs */
    byte song_target_song; /* Which song the current target applies to */
    byte song_lockout_timer; /* Turns before singing allowed again */
    byte song_contest_player_stacks; /* Player stack count for Song of Contest */
    byte song_duel_pad; /* Padding for alignment */
    s32b song_contest_last_turn; /* Last turn player stack changed */

    s16b player_hp[PY_MAX_LEVEL]; /* HP Array */

    char died_from[80]; /* Cause of death */
    char history[550]; /* Initial history */

    byte truce; /* Player will not be attacked initially at 1000ft */
    byte crown_hint; /* Player has been told about the Iron Crown */
    byte crown_shatter; /* DEPRECATED - kept for save compatibility */
    byte crown_shatter_sil2; /* Weapon shattered attempting 2nd Silmaril */
    byte crown_shatter_sil3; /* Weapon shattered attempting 3rd Silmaril */
    byte cursed; /* Player has been cursed by taking a third Silmaril */
    byte on_the_run; /* Player is on the run from Angband */
    byte morgoth_slain; /* Player has slain Morgoth */
    byte morgoth_hits; /* Number of big hits against Morgoth */
    u16b escaped; /* Player has escaped Angband */
    u16b panic_save; /* Panic save */

    u16b noscore; /* Cheating flags */

    bool is_dead; /* Player is dead */

    bool wizard; /* Player is in wizard mode */

    s16b smithing_leftover; /* Turns needed to finish making the current item */
    bool unique_forge_made; /* Has the unique forge been generated */
    bool unique_forge_seen; /* Has the unique forge been encountered */

    s16b greater_vaults[MAX_GREATER_VAULTS]; // Which greater vaults have been
                                             // generated?

    /*** Temporary fields ***/

    bool leaping; // the player is currently in the air
    bool knocked_back; // stores whether the player was knocked back last turn

    byte ripostes; // number of ripostes since your last turn (should have a max
                   // of one)

    byte was_entranced; // stores whether you have just woken up from
                        // entrancement
    byte skip_next_turn; // stores whether you need to skip your next turn

    byte have_ability[S_MAX]
                     [ABILITIES_MAX]; /* Whether or not you have each
                                         ability (including from items) */

    bool playing; /* true if player is playing the game */
    bool restoring; /* true if player is restoring a game */
    bool leaving; /* true if player is leaving the game */
    bool quit_to_menu; /* true if player wants to quit to menu instead of exiting */

    s16b create_stair; /* Create a staircase on next level */
    s16b create_rubble; /* Create rubble on next level */

    s16b wy; /* Dungeon panel */
    s16b wx; /* Dungeon panel */

    byte cur_map_hgt; /* Current dungeon level hight */
    byte cur_map_wid; /* Current dungeon level width */

    s32b total_weight; /* Total weight being carried */

    s16b inven_cnt; /* Number of items in inventory */
    s16b equip_cnt; /* Number of items in equipment */

    s16b target_set; /* Target flag */
    s16b target_who; /* Target identity */
    s16b target_row; /* Target location */
    s16b target_col; /* Target location */

    s16b health_who; /* Health bar trackee */

    s16b monster_race_idx; /* Monster race trackee */

    s16b object_kind_idx; /* Object kind trackee */

    s16b energy_use; /* Energy use this turn */

    s16b resting; /* Resting counter */
    s16b smithing; /* Smithing counter */
    s16b fletching; /* Fletching counter */
    s16b running; /* Running counter */

    s16b fletch_item; /* Item we are currently fletching. */

    s16b run_cur_dir; /* Direction we are running */
    s16b run_old_dir; /* Direction we came from */
    bool run_open_area; /* Looking for an open area */
    bool run_break_right; /* Looking for a break (right) */
    bool run_break_left; /* Looking for a break (left) */

    s16b command_cmd; /* Gives identity of current command */
    s16b command_arg; /* Gives argument of current command */
    s16b command_rep; /* Gives repetition of current command */
    s16b command_dir; /* Gives direction of current command */

    s16b command_see; /* See "cmd1.c" */
    s16b command_wrk; /* See "cmd1.c" */

    s16b command_new; /* Hack -- command chaining XXX XXX */

    s16b get_item_mode; /* Hack -- Gives the mode of the current item selection
                         */

    s16b cur_light; /* Radius of light (if any) */
    s16b old_light; /* Radius of light last turn (if any) */

    u32b notice; /* Special Updates (bit flags) */
    u32b update; /* Pending Updates (bit flags) */
    u32b redraw; /* Normal Redraws (bit flags) */
    u32b window; /* Window Redraws (bit flags) */

    s16b stat_use[A_MAX]; /* Current modified stats --  includes equipment and
                             temporary mods */
    s16b skill_use[S_MAX]; /* Current modified skills -- includes all mods */

    bool force_forge; /* Force the generation of a forge on this level */

    /*** Extracted fields ***/

    s16b stat_equip_mod[A_MAX]; /* Equipment stat bonuses */
    s16b stat_misc_mod[A_MAX]; /* Misc stat bonuses */

    s16b skill_stat_mod[S_MAX]; /* Stat stat bonuses */
    s16b skill_equip_mod[S_MAX]; /* Equipment skill bonuses */
    s16b skill_misc_mod[S_MAX]; /* Misc stat bonuses */

    int resist_cold; /* Resist cold */
    int resist_fire; /* Resist fire */
    int resist_pois; /* Resist poison */

    int resist_bleed; /* Resist bleeding */

    int resist_fear; /* Resist fear */
    int resist_blind; /* Resist blindness */
    int resist_confu; /* Resist confusion */
    int resist_stun; /* Resist stunning */
    int resist_hallu; /* Resist hallucination */

    int sustain_str; /* Keep strength */
    int sustain_dex; /* Keep dexterity */
    int sustain_con; /* Keep constitution */
    int sustain_gra; /* Keep grace */

    int regenerate; /* Regeneration */
    int telepathy; /* Telepathy */
    int see_inv; /* See invisible */
    int free_act; /* Free action */

    int danger; /* Dangerous monster creation */
    int aggravate; /* Aggravate monsters */
    int cowardice; /* Occasionally become afraid on taking damage */
    int haunted; /* Occasionally attract wraiths to your level */
    int stand_fast; /* Resist being moved by enemies with knock back and
                       exchange places. */
    int avoid_traps; /* Avoid traps. */

    s16b to_mdd; /* Bonus to melee damage dice */
    s16b mdd; /* Total melee damage dice */
    s16b to_mds; /* Bonus to melee damage sides */
    s16b mds; /* Total melee damage sides */

    s16b offhand_mel_mod; /* Modifier to off-hand melee score (relative to main
                             hand) */
    s16b mdd2; /* Total melee damage dice for off-hand weapon */
    s16b to_ads; /* Bonus to archery damage sides */
    s16b mds2; /* Total melee damage sides for off-hand weapon */

    s16b add; /* Total archery damage dice */
    s16b ads; /* Total archery damage sides */

    s16b old_p_min; /* old Minimum protection roll, to test for changes to it */
    s16b old_p_max; /* old Maximum protection roll, to test for changes to it */

    s16b dig; /* Digging ability */

    byte ammo_tval; /* Ammo variety */

    s16b pspeed; /* Current speed */
    s16b hunger; /* Hunger rate */

    byte artefacts; /* Number of artefacts generated so far */

    bool killed_enemy_with_arrow;

    byte unused1; /* was sex - so unused byte race/house player info */

    byte oath_type; /* which oath the player has chosen to keep */
    byte oaths_broken; /* which possible oaths the player has broken */

    byte tulkas_quest; /* Tulkas quest state */
    s16b tulkas_target_r_idx; /* Target unique monster for Tulkas quest */
    s16b tulkas_prize_a_idx; /* Artifact prize for Tulkas quest */
    byte tulkas_quest_complete; /* Whether quest is completed but reward not given */
    /* Aule quest tracking */
    byte aule_quest;           /* Aule quest state (AULE_QUEST_*) */
    byte aule_forge_y;         /* Y coord of Aule's forge (for validation) */
    byte aule_forge_x;         /* X coord of Aule's forge */
    byte aule_reserved;        /* padding */
    s16b aule_level;           /* Dungeon depth where forge resides */
    s16b aule_last_object_diff;/* Difficulty of last forged object (for logging) */
    /* Mandos quest tracking */
    byte mandos_quest;         /* Mandos quest state (MANDOS_QUEST_*) */
    byte mandos_vault_y;       /* Y coord of Mandos' vault center */
    byte mandos_vault_x;       /* X coord of Mandos' vault center */
    byte mandos_monsters_remaining; /* Number of monsters left to clear */
    s16b mandos_level;         /* Dungeon depth where vault resides */
    s16b mandos_reserved;      /* padding */
    /* Niena quest tracking */
    byte niena_quest;          /* Niena quest state (NIENA_QUEST_*) */
    byte niena_monsters_seen;  /* Number of monsters seen during quest */
    byte niena_monsters_killed; /* Number of monsters killed during quest */
    byte niena_reserved;       /* padding */
    s16b niena_level;          /* Dungeon depth where quest is active */
    s16b niena_reserved2;      /* padding */
    /* Orome quest tracking */
    byte orome_quest;          /* Orome quest state (OROME_QUEST_*) */
    byte orome_target_type;    /* Monster type to hunt (1=wolf, 2=spider, 3=serpent, 4=vampire) */
    s16b orome_killed_count;   /* Number of target monsters killed */
    s16b orome_target_count;   /* Required number to kill (100/80/60/30) */
    /* New global monster type counters for Orome quest */
    s16b orome_wolves_killed;  /* Total wolves killed (any type) */
    s16b orome_spiders_killed; /* Total spiders killed (any type) */
    s16b orome_serpents_killed; /* Total serpents killed (any type) */
    s16b orome_vampires_killed; /* Total vampires killed (any type) */
    /* Generic quest/vault tracking */
    byte quest_vault_used;     /* Has a quest-designated vault generated this game */
    byte quest_reserved[15];   /* quest_reserved[0] = any quest spawned flag (run-wide); rest reserved */
};

/* scores.raw header version == core game version (no independent bumping) */
#define SCORE_FILE_VERSION_MAJOR VERSION_MAJOR
#define SCORE_FILE_VERSION_MINOR VERSION_MINOR
#define SCORE_FILE_VERSION_PATCH VERSION_PATCH
#define SCORE_FILE_VERSION_EXTRA VERSION_EXTRA

/*
 * Version header for scores.raw file (16 bytes)
 */
typedef struct score_file_header
{
    byte version_major;  /* Mirrors VERSION_MAJOR */
    byte version_minor;  /* Mirrors VERSION_MINOR */
    byte version_patch;  /* Mirrors VERSION_PATCH */
    byte version_extra;  /* Mirrors VERSION_EXTRA */
    u32b entry_count;    /* Number of score entries in file */
    u32b reserved[2];    /* Reserved for future use */
} score_file_header;

/*
 * High Score List Entry (on-disk record length = 133 bytes)
 *
 * All fields are fixed-length, null-terminated ascii strings with any
 * unused trailing bytes left as zeros. The historical comment claimed
 * 128 bytes; additions (longer 'how' field + extra flags) increased
 * this without updating the note. We freeze the on-disk layout at the
 * sum of the declared field lengths (8+5+10+10+16+8+2+3+3+4+4+4+50+2+2+2=133).
 *
 * Portability: we need a packed representation without relying on
 * non‑standard attributes under non-GNU compilers (e.g. MSVC). We use
 * #pragma pack for MSVC and GCC/Clang attribute elsewhere. If neither
 * is available we accept potential padding (in which case add manual
 * serialization before shipping to that platform).
 */

#if defined(_MSC_VER)
#pragma pack(push,1)
#endif

struct high_score
{
    char what[8];        /* Version info (string) */
    char pts[5];         /* Net curse count: total(curses) minus total(blessings) (right-aligned decimal, version_extra >= 6) */
    char turns[10];      /* Turns Taken (number) */
    char day[10];        /* Time stamp (string) */
    char who[16];        /* Player Name (string) */
    char uid[8];         /* Player UID (number) */
    char unused[2];      /* Was sex */
    char p_r[3];         /* Player Race (number) */
    char p_h[3];         /* Player House (number) */
    char cur_lev[4];     /* Unique monsters killed (number) */
    char cur_dun[4];     /* Current Dungeon Level (number) */
    char max_dun[4];     /* Max Dungeon Level (number) */
    char how[50];        /* Method of death (string) */
    char silmarils[2];   /* Number of Silmarils (number) */
    char morgoth_slain[2]; /* Has player slain Morgoth (t/f) */
    char escaped[2];     /* Has player escaped (t/f) */
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((packed))
#endif
;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

typedef struct high_score high_score;

/* C89-friendly compile-time size check (negative array size => error) */
typedef char high_score_size_must_be_133[(sizeof(struct high_score) == 133) ? 1 : -1];



typedef struct combat_roll combat_roll;

struct combat_roll
{
    int att_type; /* The type of attack (COMBAT_ROLL_NONE or COMBAT_ROLL_ROLL or
                     COMBAT_ROLL_AUTO) */
    int dam_type; /* The type of damage (GF_HURT, GF_FIRE etc) */

    char attacker_char; /* The symbol of the attacker */
    byte attacker_attr; /* Default attribute of the attacker */
    char defender_char; /* The symbol of the defender */
    byte defender_attr; /* Default attribute of the defender */
    bool is_attacker_player; /* TRUE if the attacker is the player */
    bool is_defender_player; /* TRUE if the defender is the player */
    int att; /* The attack bonus */
    int att_roll; /* The attack roll (d20 value) */
    int evn; /* The evasion bonus */
    int evn_roll; /* The evasion roll (d20 value */

    int dd; /* The number of damage dice */
    int ds; /* The number of damage sides */
    int dam; /* The total damage rolled */
    int pd; /* The number of protection dice */
    int ps; /* The number of protection sides */
    int prot; /* The total protection rolled */

    int prt_percent; /* The percentage of protection that is effective (eg 100
                        normally) */
    bool melee; /* Was it a melee attack? (used for working out if blocking is
                   effective) */
};

typedef struct combat_history_round combat_history_round;

struct combat_history_round
{
    int turn_count;           /* The turn number when this round occurred */
    int num_rolls;            /* Number of combat rolls in this round */
    combat_roll rolls[MAX_COMBAT_ROLLS]; /* The actual combat rolls */
};

struct flavor_type
{
    u32b text; /* Text (offset) */

    byte tval; /* Associated object type */
    byte sval; /* Associated object sub-type */

    byte d_attr; /* Default flavor attribute */
    char d_char; /* Default flavor character */

    byte x_attr; /* Desired flavor attribute */
    char x_char; /* Desired flavor character */
};

/*
 * The structure editing_buffer allows to quickly insert and delete text at
 * every position of a string. It is based on the Emacs editor.
 * It has a internal buffer, a fake "cursor" and a gap that begins at this
 * "cursor".
 * Maybe the most important operation is "set_position". It moves the gap
 * at any position of the buffer. Because of it, insertions and deletions
 * are really fast operations.
 *
 * This is a representation of the buffer:
 *
 * xxxxxxx.ooooxxxx             x: text
 *         |                    o: gap (it must be '\0')
 *         pos ("cursor")       .: the character before the "cursor"
 *
 * This is the same buffer after moving the "cursor" one position to the left:
 *
 * xxxxxxxoooo.xxxx             note the new position of "."
 *        |
 *        pos ("cursor")
 */
struct editing_buffer
{
    /* Public fields, Read ONLY */
    size_t pos;

    /* Private fields */
    size_t gap_size, max_size;
    char* buf;
};

/*structure of letter probabilitiesfor the random name generator*/
struct names_type
{
    u16b lprobs[S_WORD + 1][S_WORD + 1][S_WORD + 1];
    u16b ltotal[S_WORD + 1][S_WORD + 1];
};

/*Information for object auto-inscribe*/
struct autoinscription
{
    s16b kindIdx;
    s16b inscriptionIdx;
};

typedef struct header        header;        /* <<< add this line */

extern runtype_type *runtype_info;   /* NEW — allocated by init_info() */
extern header        rt_head;        /* NEW — loader header            */

#ifndef UI_NAV_H
#define UI_NAV_H

/* Navigation results bubbled up from UI screens */
typedef enum {
    NAV_OK = 0,     /* proceed normally */
    NAV_BACK,       /* step back within current flow */
    NAV_TO_MAIN,    /* abort flow and return to main menu */
    NAV_QUIT        /* exit program */
} NavResult;

/* Result of play_game() */
typedef enum {
    PLAY_DONE = 0,  /* return to main menu */
    PLAY_QUIT       /* exit program */
} PlayResult;

#endif /* UI_NAV_H */

typedef struct flag_name flag_name;

struct flag_name
{
    cptr name; /* The name of the flag in the text file. */
    int set; /* The set into which the flag is to be sent. */
    u32b flag; /* The flag being set. */
};
