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
typedef struct player_sex player_sex;
typedef struct player_race player_race;
typedef struct player_house player_house;
typedef struct hist_type hist_type;
typedef struct player_other player_other;
typedef struct player_type player_type;
typedef struct start_item start_item;
typedef struct names_type names_type;
typedef struct flavor_type flavor_type;
typedef struct editing_buffer editing_buffer;
typedef struct autoinscription autoinscription;

/**** Available structs ****/


/*
 * Information about maximal indices of certain arrays
 * Actually, these are not the maxima, but the maxima plus one
 */
struct maxima
{
	u32b fake_text_size;
	u32b fake_name_size;

	u16b f_max;				/* Max size for "f_info[]" */
	u16b k_max;				/* Max size for "k_info[]" */
	u16b b_max;				/* Max size for "b_info[]" */
	u16b art_max;			/* Max size for "a_info[]" */
	u16b e_max;				/* Max size for "e_info[]" */
	u16b r_max;				/* Max size for "r_info[]" */
	u16b v_max;				/* Max size for "v_info[]" */
	u16b p_max;				/* Max size for "p_info[]" */
	u16b h_max;				/* Max size for "h_info[]" */
	u16b c_max;				/* Max size for "c_info[]" */
	u16b q_max;				/* Max size for "q_info[]" */
	u16b flavor_max;		/* Max size for "flavor_info[]" */
	u16b o_max;				/* Max size for "o_list[]" */
	u16b ghost_other_max;	/* Max maintainer player ghost templates */
	u16b art_spec_max;		/* Max number of special artefacts */
	u16b art_norm_max;		/* Max number for normal artefacts (special - normal) */
	u16b art_rand_max;		/* Max number of random artefacts */
	u16b art_self_made_max; /* Max number of self-made artefacts */
};


/*
 * Information about terrain "features"
 */
struct feature_type
{
	u32b name;			/* Name (offset) */
	u32b text;			/* Text (offset) */

	byte mimic;			/* Feature to mimic */

	byte extra;			/* Extra byte (unused) */

	s16b unused;		/* Extra bytes (unused) */


	byte d_attr;		/* Default feature attribute */
	char d_char;		/* Default feature character */


	byte x_attr;		/* Desired feature attribute */
	char x_char;		/* Desired feature character */
};


/*
 * Information about object "kinds", including player knowledge.
 *
 * Only "aware" and "tried" are saved in the savefile
 */
struct object_kind
{
	u32b name;			/* Name (offset) */
	u32b text;			/* Text (offset) */

	byte tval;			/* Object type */
	byte sval;			/* Object sub type */

	s16b pval;			/* Object extra info */

	s16b att;			/* Bonus to hit */
	s16b evn;			/* Sil - Bonus to evasion */

	byte dd, ds;		/* Damage dice/sides */
	byte pd, ps;		/* Sil - Protection dice/sides */

	s16b weight;		/* Weight */

	s32b cost;			/* Object "base cost" */

	u32b flags1;		/* Flags, set 1 */
	u32b flags2;		/* Flags, set 2 */
	u32b flags3;		/* Flags, set 3 */

	byte locale[4];		/* Allocation level(s) */
	byte chance[4];		/* Allocation chance(s) */

	byte abilities;	    // Number of abilities
	byte skilltype[4];  // Skill-types for the granted abilities
	byte abilitynum[4]; // Ability numbers for these

	byte level;			/* Level */
	byte extra;			/* Something */


	byte d_attr;		/* Default object attribute */
	char d_char;		/* Default object character */


	byte x_attr;		/* Desired object attribute */
	char x_char;		/* Desired object character */


	u16b flavor;		/* Special object flavor (or zero) */

	bool aware;			/* The player is "aware" of the item's effects */

	bool tried;			/* The player has "tried" one of the items */

	byte squelch;		/* Squelch setting for the particular item */

	bool everseen;		/* Used to despoilify squelch menus */
};



/*
 * Information about abilities.
 */
struct ability_type
{
	u32b name;			/* Name (offset) */
	u32b text;			/* Text (offset) */
	
	byte skilltype;		/* Skill type */
	byte abilitynum;	/* Ability number within a skill */
	
	byte level;						/* Prerequisite skill level */
	byte prereqs;					/* Number of prerequisite abilities */
	byte prereq_skilltype[4];		/* Skill type (for prerequisites) */
	byte prereq_abilitynum[4];		/* The ability within that skill (for prerequisites) */

	byte tval[ABILITY_TVALS_MAX];		/* Legal tval */
	byte min_sval[ABILITY_TVALS_MAX];	/* Minimum legal sval */
	byte max_sval[ABILITY_TVALS_MAX];	/* Maximum legal sval */
};


/*
 * Information about "artefacts".
 *
 * Note that the save-file only writes "cur_num" and "found_num" to the savefile,
 * except for the random artefacts
 *
 * Note that "max_num" is always "1" (if that artefact "exists")
 */
struct artefact_type
{
	char name[MAX_LEN_ART_NAME];	/* Name */
	u32b text;						/* Description (offset) */

	byte tval;			/* Artefact type */
	byte sval;			/* Artefact sub type */

	s16b pval;			/* Artefact extra info */

	s16b att;			/* Bonus to hit */
	s16b evn;			/* Bonus to evasion */

	byte dd, ds;		/* Damage when hits */
	byte pd, ps;		/* Protection dice and sides */

	s16b weight;		/* Weight */

	s32b cost;			/* Artefact "cost" */

	u32b flags1;		/* Artefact Flags, set 1 */
	u32b flags2;		/* Artefact Flags, set 2 */
	u32b flags3;		/* Artefact Flags, set 3 */

	byte level;			/* Artefact level */
	byte rarity;		/* Artefact rarity */

	byte cur_num;		/* Number created (0 or 1) */
	byte found_num;		/* Number found (0 or 1) */
	byte max_num;		/* Unused (should be "1") */

	byte activation;	/* Activation to use */
	u16b time;			/* Activation time */
	u16b randtime;		/* Activation time dice */
	
	byte d_attr;		/* Default artefact attribute */
	char d_char;		/* Default artefact character */

	byte abilities;	    // Number of abilities
	byte skilltype[4];  // Skill-types for the granted abilities
	byte abilitynum[4]; // Ability numbers for these
};


/*
 * Information about special items.
 */
struct ego_item_type
{
	u32b name;			/* Name (offset) */
	u32b text;			/* Description (offset) */

	s32b cost;			/* Ego-item "cost" */

	u32b flags1;		/* Ego-Item Flags, set 1 */
	u32b flags2;		/* Ego-Item Flags, set 2 */
	u32b flags3;		/* Ego-Item Flags, set 3 */

	byte level;			/* Minimum level */
	byte max_level;		/* Maximum level */
	byte rarity;		/* Object rarity */

	byte tval[EGO_TVALS_MAX];		/* Legal tval */
	byte min_sval[EGO_TVALS_MAX];	/* Minimum legal sval */
	byte max_sval[EGO_TVALS_MAX];	/* Maximum legal sval */
	
	byte abilities;	    // Number of abilities
	byte skilltype[4];  // Skill-types for the granted abilities
	byte abilitynum[4]; // Ability numbers for these
	
	byte max_att;		/* Maximum to-hit bonus */
	byte to_dd;			/* bonus damge dice */
	byte to_ds;			/* bonus damage sides */
	byte max_evn;		/* Maximum to-e bonus */
	byte to_pd;			/* bonus protection dice */
	byte to_ps;			/* bonus protection sides */
	byte max_pval;		/* Maximum pval */

	bool aware;			/* Has its type been detected this game? */
	bool everseen;		/* Do not spoil squelch menus */
	bool squelch;		/* Squelch this special item */
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
	u32b name;				/* Name (offset) */
	u32b text;				/* Text (offset) */

	byte hdice;				/* Creatures hit dice count */
	byte hside;				/* Creatures hit dice sides */

	s16b evn;				/* Bonus to evasion */
	byte pd;				/* Protection dice */
	byte ps;				/* Protection sides */

	byte speed;				/* Speed (normally 110) */
	s16b light;				/* Light/Dark radius (if any) */

	s16b sleep;				/* Starting penalty to alertness */
	s16b per;				/* Perception */
	s16b stl;				/* Stealth */
	s16b wil;				/* Will */

	s16b extra;				/* Unused (for now) */

	byte freq_ranged;		/* Ranged attack frequency */
	byte spell_power;		/* Power of (damage-dealing) spells */
	u32b mon_power;        	/* Monster Power Rating */

#ifdef ALLOW_DATA_DUMP

	u32b mon_eval_hp;		/*evaluated hitpoint power of monster*/
	u32b mon_eval_dam;		/*evaluated damage power of monster*/

#endif /*ALLOW_DATA_DUMP*/

	u32b flags1;			/* Flags 1 (general) */
	u32b flags2;			/* Flags 2 (abilities) */
	u32b flags3;			/* Flags 3 (race/resist) */
	u32b flags4;			/* Flags 4 ('spells') */

	monster_blow blow[MONSTER_BLOW_MAX]; /* Up to four blows per round */

	byte level;				/* Level of creature */
	byte rarity;			/* Rarity of creature */

	byte d_attr;			/* Default monster attribute */
	char d_char;			/* Default monster character */

	byte x_attr;			/* Desired monster attribute */
	char x_char;			/* Desired monster character */

	byte max_num;			/* Maximum population allowed per level */
	byte cur_num;			/* Monster population on current level */
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
	s16b deaths;			/* Count deaths from this monster */

	s16b psights;			/* Count sightings of this monster in this life */
	s16b tsights;			/* Count sightings of this monster in all lives */

	s16b pkills;			/* Count monsters killed in this life */
	s16b tkills;			/* Count monsters killed in all lives */

	byte notice;			/* Number of times it has been seen noticing the player */
	byte ignore;			/* Number of times it has been seen not noticing the player */

	byte drop_item;			/* Max number of items dropped at once */

	byte ranged;			/* Observed ranged attacks */
	byte mana;				/* Max mana */
	byte spell_power;		/* Power of (damage-dealing) spells */

	byte blows[MONSTER_BLOW_MAX]; /* Number of times each blow type was seen */

	u32b flags1;			/* Observed racial flags */
	u32b flags2;			/* Observed racial flags */
	u32b flags3;			/* Observed racial flags */
	u32b flags4;			/* Observed racial flags */
};



/*
 * Information about "vault generation"
 */
struct vault_type
{
	u32b name;			/* Name (offset) */
	u32b text;			/* Text (offset) */

	byte typ;			/* Vault type */

	byte depth;			/* Vault rating */

	byte rarity;		/* Vault rarity */

	byte hgt;			/* Vault height */
	byte wid;			/* Vault width */

	byte forge;			/* Is there a forge in it? */
    
	u32b flags;			/* Vault Flags (ie VLT flags) */
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
	s16b k_idx;			/* Kind index (zero if "dead") */
	
	s16b image_k_idx;	/* Hallucinatory kind index */

	byte iy;			/* Y-position on map, or zero */
	byte ix;			/* X-position on map, or zero */

	byte tval;			/* Item type (from kind) */
	byte sval;			/* Item sub-type (from kind) */

	s16b pval;			/* Item extra-parameter */

	byte discount;		/* Discount (if any) */

	byte number;		/* Number of items */

	s16b weight;		/* Item weight */

	byte name1;			/* Artefact type, if any */
	byte name2;			/* Ego-Item type, if any */

	byte pickup;		/* Auto pick up this item next time it is stepped on */

	byte xtra1;			/* Extra info type */

	s16b att;			/* Bonus to attack */
	s16b evn;			/* Bonus to evasion */

	byte dd, ds;		/* Damage dice/sides */
	byte pd, ps;		/* Protection dice/sides */
	
	s16b timeout;		/* Timeout Counter */

	u32b ident;			/* Special flags (was byte) */

	byte marked;		/* Object is marked */

	u16b obj_note;		/* Inscription index */

	s16b next_o_idx;	/* Next object in stack (if any) */

	s16b held_m_idx;	/* Monster holding us (if any) */

	byte abilities;	    // Number of abilities
	byte skilltype[8];  // Skill-types for the granted abilities (8 = 4 for object base + 4 for special or artefact)
	byte abilitynum[8]; // Ability numbers for these
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
	s16b r_idx;			/* Monster race index */

	s16b image_r_idx;	/* Monster race index (when hallucinating) */

	byte fy;			/* Y location on map */
	byte fx;			/* X location on map */

	s16b hp;			/* Current Hit points */
	s16b maxhp;			/* Max Hit points */

	s16b alertness;		/* Positive numbers can move and act, negative are unwary or asleep */
	byte skip_next_turn;/* used to skip the monster's next turn in various circumstances */
	byte skip_this_turn;/* used to make sure the monster doesn't do anything this turn (Song of Mastery) */

	byte mspeed;		/* Monster "speed" */
	byte energy;		/* Monster "energy" */

	byte stunned;		/* Monster is stunned */
	byte confused;		/* Monster is confused */
	s16b slowed;		/* Monster is slowed */
	s16b hasted;		/* Monster is hasted */

	byte stance;		/* Fleeing, Timid, Cautious, Aggressive */
	s16b morale;		/* Overall morale */
	s16b tmp_morale;	/* Temporary modifier to morale */

	byte cdis;			/* Current dis from player */

	u32b mflag;			/* Extra monster flags */

	bool ml;			/* Monster is "visible" */
	
	byte noise;			/* amount of noise monster made this turn */

	byte encountered;	/* Whether the monster has been encountered yet */

	s16b hold_o_idx;	/* Object being held (if any) */

	byte target_y;		/* Monster target */
	byte target_x;
	
	s16b wandering_idx;	/* Where the monster is wandering while unwary (if anywhere) */
	byte wandering_dist;/* The distance to the destination */

	byte min_range;		/* What is the closest we want to be? */  /* Not saved */
	byte best_range;	/* How close do we want to be? */  /* Not saved */

	byte mana;          /* Current mana level */
    byte song;          /* Current song */

    s16b consecutive_attacks;               /* How many times it has attacked the player in a row immediately prior to now */
    s16b turns_stationary;                  /* How many times it has stayed still in a row immediately prior to now */
    byte previous_action[ACTION_MAX];       /* What the monster did on its previous turns */
    
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
	s16b index;		/* The actual index */

	byte level;		/* Base dungeon level */
	byte prob1;		/* Probability, pass 1 */
	byte prob2;		/* Probability, pass 2 */
	byte prob3;		/* Probability, pass 3 */

	u16b total;		/* Unused for now */
};



/*
 * A store owner
 */
struct owner_type
{
	u32b owner_name;	/* Name (offset) */

	s16b max_cost;		/* Purse limit */

	byte max_inflate;	/* Inflation (max) */
	byte min_inflate;	/* Inflation (min) */

	byte haggle_per;	/* Haggle unit */

	byte insult_max;	/* Insult limit */

	byte owner_race;	/* Owner race */
};




/*
 * A store, with an owner, various state flags, a current stock
 * of items, and a table of items that are often purchased.
 */
struct store_type
{
	byte owner;				/* Owner index */

	s16b insult_cur;		/* Insult counter */

	s16b good_buy;			/* Number of "good" buys */
	s16b bad_buy;			/* Number of "bad" buys */

	s32b store_open;		/* Closed until this turn */

	byte stock_num;			/* Stock -- Number of entries */
	s16b stock_size;		/* Stock -- Total Size of Array */
	object_type *stock;		/* Stock -- Actual stock items */

};


/*
 * Player sex info
 */
struct player_sex
{
	cptr title;			/* Type of sex */

	cptr winner;		/* Name of winner */
};


/*
 * Starting equipment entry
 */
struct start_item
{
	byte tval;	/* Item's tval */
	byte sval;	/* Item's sval */
	byte min;	/* Minimum starting amount */
	byte max;	/* Maximum starting amount */

};


/*
 * Player racial info
 */
struct player_race
{
	u32b name;			/* Name (offset) */
	u32b text;			/* Description (offset) */

	s16b r_adj[A_MAX];	/* Racial stat bonuses */

	int b_age;			/* base age */
	int m_age;			/* mod age */

	byte m_b_ht;		/* base height (males) */
	byte m_m_ht;		/* mod height (males) */
	byte m_b_wt;		/* base weight (males) */
	byte m_m_wt;		/* mod weight (males) */

	byte f_b_ht;		/* base height (females) */
	byte f_m_ht;		/* mod height (females) */
	byte f_b_wt;		/* base weight (females) */
	byte f_m_wt;		/* mod weight (females) */

	s16b choice;		/* Legal house choices */

	start_item start_items[MAX_START_ITEMS];	/* The starting inventory */

	s16b hist;			/* Starting history index */

	u32b flags;			/* Racial Flags (ie RHF flags) */
};


/*
 * Player house info
 */
struct player_house
{
	u32b name;			/* Name (offset)           eg 'House of Feanor' */
	u32b alt_name;		/* Alternate Name (offset) eg 'Feanor's House'  */
	u32b short_name;	/* Short Name (offset)     eg 'Feanor'          */
	u32b text;			/* Descrption (offset) */

	s16b h_adj[A_MAX];	/* House stat bonuses */

	u32b flags;			/* House Flags (ie RHF flags) */
};


/*
 * Player background information
 */
struct hist_type
{
	u32b text;			    /* Text (offset) */

	byte roll;			    /* Frequency of this entry */
	byte chart;			    /* Chart index */
	byte next;			    /* Next chart index */
	byte house;			    /* House to associate with */
};



/*
 * Some more player information
 *
 * This information is retained across player lives
 */
struct player_other
{
	char full_name[32];		/* Full name */
	char base_name[32];		/* Base name */

	bool opt[OPT_MAX];		/* Options */

	u32b window_flag[ANGBAND_TERM_MAX];	/* Window flags */

	byte hitpoint_warn;		/* Hitpoint warning (0 to 9) */

	byte delay_factor;		/* Delay factor (0 to 9) */
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
	s16b py;			/* Player location */
	s16b px;			/* Player location */

	byte psex;			/* Sex index */
	byte prace;			/* Race index */
	byte phouse;		/* House index */
	
	s16b game_type;		/* Whether this is a normal game (=0), tutorial (<0), puzzle (>0) */

	s16b age;			/* Character's age */
	s16b ht;			/* Height */
	s16b wt;			/* Weight */
	s16b sc;			/* Social Class */

	s16b max_depth;		/* Max depth */
	s16b depth;			/* Cur depth */

	s32b new_exp;		/* New experience */
	s32b exp;			/* Total experience */

	s32b encounter_exp;		/* Total experience from ecountering monsters */
	s32b kill_exp;			/* Total experience from killing monsters */
	s32b descent_exp;		/* Total experience from descending to new levels */
	s32b ident_exp;			/* Total experience from identifying objects */

	s16b mhp;			/* Max hit pts */
	s16b chp;			/* Cur hit pts */
	u16b chp_frac;		/* Cur hit frac (times 2^16) */

	s16b msp;			/* Max mana pts */
	s16b csp;			/* Cur mana pts */
	u16b csp_frac;		/* Cur mana frac (times 2^16) */

	s16b stat_base[A_MAX];		/* The base ('internal') stat values */
	s16b stat_drain[A_MAX];		/* The negative modifier from stat drain */

	s16b skill_base[S_MAX];		/* The base skill values */

	byte innate_ability[S_MAX][ABILITIES_MAX];	/* Whether or not you personally have each ability */
	byte active_ability[S_MAX][ABILITIES_MAX];	/* Whether or not you are using each ability */
	
	s16b last_attack_m_idx;				// m_idx of the monster attacked last round (if any)
	s16b consecutive_attacks;			// number of rounds spent attacking this monster
	s16b bane_type;						// the monster type you have specialized against
	byte previous_action[ACTION_MAX];	// stores the previous actions you have taken
	byte focused;						// stores whether you are currently focusing for an attack

	s16b fast;			/* Timed -- Fast */
	s16b slow;			/* Timed -- Slow */
	s16b blind;			/* Timed -- Blindness */
	s16b entranced;		/* Timed -- Entrancement */
	s16b confused;		/* Timed -- Confusion */
	s16b afraid;		/* Timed -- Fear */
	s16b image;			/* Timed -- Hallucination */
	s16b poisoned;		/* Timed -- Poisoned */
	s16b cut;			/* Timed -- Cut */
	s16b stun;			/* Timed -- Stun */
	s16b darkened;		/* Timed -- magical darkness */

	s16b rage;			/* Timed -- Rage */
	s16b tmp_str;		/* Timed -- Strength */
	s16b tmp_dex;		/* Timed -- Dexterity */
	s16b tmp_con;		/* Timed -- Constitution */
	s16b tmp_gra;		/* Timed -- Grace */
	s16b tim_invis;		/* Timed -- See Invisible */

	s16b oppose_fire;	/* Timed -- oppose heat */
	s16b oppose_cold;	/* Timed -- oppose cold */
	s16b oppose_pois;	/* Timed -- oppose poison */

	s16b word_recall;	/* Word of recall counter */

	s16b energy;		/* Current energy */

	s16b food;			/* Current nutrition */

	u16b stairs_taken;	/* The number of times stairs have been used */
	u16b staircasiness;	/* Gets higher when stairs are taken and slowly decays */

	u16b forge_drought;	/* The number of turns since the last forge was generated */
	u16b forge_count;	/* The number of forges that have been generated */

	byte stealth_mode;	/* Stealth mode */
	byte climbing;      /* The player is climbing over a chasm */

	byte self_made_arts;	/* Number of self-made artefacts so far */

	byte song1;			/* Current song */
	byte song2;			/* Current minor theme */
	s16b wrath;			/* The counter for the song of slaying */
	s16b song_duration;	/* The duration of the current song */

	s16b player_hp[PY_MAX_LEVEL];	/* HP Array */

	char died_from[80];		/* Cause of death */
	char history[250];		/* Initial history */

	byte truce;				/* Player will not be attacked initially at 1000ft */
	byte crown_hint;		/* Player has been told about the Iron Crown */
	byte crown_shatter;		/* Player has had a weapon shattered by the Crown */
	byte cursed;			/* Player has been cursed by taking a third Silmaril */
	byte on_the_run;		/* Player is on the run from Angband */
	byte morgoth_slain;		/* Player has slain Morgoth */
	byte morgoth_hits;		/* Number of big hits against Morgoth */
	u16b escaped;			/* Player has escaped Angband */
	u16b panic_save;		/* Panic save */

	u16b noscore;			/* Cheating flags */

	bool is_dead;			/* Player is dead */

	bool wizard;			/* Player is in wizard mode */

	s16b smithing_leftover; /* Turns needed to finish making the current item */
	bool unique_forge_made; /* Has the unique forge been generated */
	bool unique_forge_seen; /* Has the unique forge been encountered */

	s16b greater_vaults[MAX_GREATER_VAULTS];	// Which greater vaults have been generated?

	/*** Temporary fields ***/
	
	bool leaping;           // the player is currently in the air

	byte ripostes;			// number of ripostes since your last turn (should have a max of one)

	byte was_entranced;		// stores whether you have just woken up from entrancement
	byte skip_next_turn;	// stores whether you need to skip your next turn

	byte have_ability[S_MAX][ABILITIES_MAX];	/* Whether or not you have each ability (including from items) */

	bool playing;			/* True if player is playing the game */
	bool restoring;			/* True if player is restoring a game */
	bool leaving;			/* True if player is leaving the game */

	s16b create_stair;		/* Create a staircase on next level */
	s16b create_rubble;		/* Create rubble on next level */

	s16b wy;				/* Dungeon panel */
	s16b wx;				/* Dungeon panel */

	byte cur_map_hgt;		/* Current dungeon level hight */
	byte cur_map_wid;		/* Current dungeon level width */

	s32b total_weight;		/* Total weight being carried */

	s16b inven_cnt;			/* Number of items in inventory */
	s16b equip_cnt;			/* Number of items in equipment */

	s16b target_set;		/* Target flag */
	s16b target_who;		/* Target identity */
	s16b target_row;		/* Target location */
	s16b target_col;		/* Target location */

	s16b health_who;		/* Health bar trackee */

	s16b monster_race_idx;	/* Monster race trackee */

	s16b object_kind_idx;	/* Object kind trackee */

	s16b energy_use;		/* Energy use this turn */

	s16b resting;			/* Resting counter */
	s16b smithing;			/* Smithing counter */
	s16b running;			/* Running counter */
	bool automaton;         /* Player is AI controlled? */

	s16b run_cur_dir;		/* Direction we are running */
	s16b run_old_dir;		/* Direction we came from */
	bool run_unused;		/* Unused (padding field) */
	bool run_open_area;		/* Looking for an open area */
	bool run_break_right;	/* Looking for a break (right) */
	bool run_break_left;	/* Looking for a break (left) */

	s16b command_cmd;		/* Gives identity of current command */
	s16b command_arg;		/* Gives argument of current command */
	s16b command_rep;		/* Gives repetition of current command */
	s16b command_dir;		/* Gives direction of current command */

	s16b command_see;		/* See "cmd1.c" */
	s16b command_wrk;		/* See "cmd1.c" */

	s16b command_new;		/* Hack -- command chaining XXX XXX */

	s16b get_item_mode;		/* Hack -- Gives the mode of the current item selection */
	
	s16b new_spells;		/* Number of spells available */

	bool heavy_wield;	/* Heavy weapon */

	s16b cur_light;		/* Radius of light (if any) */
	s16b old_light;		/* Radius of light last turn (if any) */

	u32b notice;		/* Special Updates (bit flags) */
	u32b update;		/* Pending Updates (bit flags) */
	u32b redraw;		/* Normal Redraws (bit flags) */
	u32b window;		/* Window Redraws (bit flags) */

	s16b stat_use[A_MAX];	/* Current modified stats --  includes equipment and temporary mods */
	s16b skill_use[S_MAX];	/* Current modified skills -- includes all mods */

	bool force_forge;		/* Force the generation of a forge on this level */

	/*** Extracted fields ***/

	s16b stat_equip_mod[A_MAX];		/* Equipment stat bonuses */
	s16b stat_misc_mod[A_MAX];		/* Misc stat bonuses */

	s16b skill_stat_mod[S_MAX];		/* Stat stat bonuses */
	s16b skill_equip_mod[S_MAX];	/* Equipment skill bonuses */
	s16b skill_misc_mod[S_MAX];		/* Misc stat bonuses */

	int resist_cold;	/* Resist cold */
	int resist_fire;	/* Resist fire */
	int resist_pois;	/* Resist poison */

	int resist_fear;	/* Resist fear */
	int resist_blind;	/* Resist blindness */
	int resist_confu;	/* Resist confusion */
	int resist_stun;	/* Resist stunning */
	int resist_hallu;	/* Resist hallucination */

	int sustain_str;	/* Keep strength */
	int sustain_dex;	/* Keep dexterity */
	int sustain_con;	/* Keep constitution */
	int sustain_gra;	/* Keep grace */

	int regenerate;     /* Regeneration */
	int telepathy;		/* Telepathy */
	int see_inv;		/* See invisible */
	int free_act;		/* Free action */

	int danger;		/* Dangerous monster creation */
	int aggravate;		/* Aggravate monsters */
	int cowardice;		/* Occasionally become afraid on taking damage */
	int haunted;		/* Occasionally attract wraiths to your level */

	s16b to_mdd;		/* Bonus to melee damage dice */
	s16b mdd;			/* Total melee damage dice */
	s16b to_mds;		/* Bonus to melee damage sides */
	s16b mds;			/* Total melee damage sides */
	
	s16b offhand_mel_mod;	/* Modifier to off-hand melee score (relative to main hand) */
	s16b mdd2;				/* Total melee damage dice for off-hand weapon */
	s16b to_ads;			/* Bonus to archery damage sides */
	s16b mds2;				/* Total melee damage sides for off-hand weapon */

	s16b add;			/* Total archery damage dice */
	s16b ads;			/* Total archery damage sides */

	s16b old_p_min;		/* old Minimum protection roll, to test for changes to it */
	s16b old_p_max;		/* old Maximum protection roll, to test for changes to it */

	s16b dig;			/* Digging ability */

	byte ammo_tval;		/* Ammo variety */

	s16b pspeed;		/* Current speed */
	s16b hunger;		/* Hunger rate */
	
	byte artefacts;		/* Number of artefacts generated so far */
};


/*
 * Semi-Portable High Score List Entry (128 bytes)
 *
 * All fields listed below are null terminated ascii strings.
 *
 * In addition, the "number" fields are right justified, and
 * space padded, to the full available length (minus the "null").
 *
 * Note that "string comparisons" are thus valid on "pts".
 */

typedef struct high_score high_score;

struct high_score
{
	char what[8];		/* Version info (string) */

	char pts[5];		/* obsolete */

	char turns[10];		/* Turns Taken (number) */

	char day[10];		/* Time stamp (string) */

	char who[16];		/* Player Name (string) */

	char uid[8];		/* Player UID (number) */

	char sex[2];		/* Player Sex (string) */
	char p_r[3];		/* Player Race (number) */
	char p_h[3];		/* Player House (number) */

	char cur_lev[4];		/* Current Player Level (number) */
	char cur_dun[4];		/* Current Dungeon Level (number) */
	char max_dun[4];		/* Max Dungeon Level (number) */

	char how[50];		/* Method of death (string) */
	
	char silmarils[2];		/* Number of Silmarils (number) */
	char morgoth_slain[2];	/* Has player slain Morgoth (t/f) */
	char escaped[2];		/* Has player escaped (t/f) */
};


// A type to contain information on a combat roll for printing

typedef struct combat_roll combat_roll;

struct combat_roll
{
	int att_type;			/* The type of attack (COMBAT_ROLL_NONE or COMBAT_ROLL_ROLL or COMBAT_ROLL_AUTO) */
	int dam_type;			/* The type of damage (GF_HURT, GF_FIRE etc) */

	char attacker_char;		/* The symbol of the attacker */
	byte attacker_attr;		/* Default attribute of the attacker */
	char defender_char;		/* The symbol of the defender */
	byte defender_attr;		/* Default attribute of the defender */
	int att;				/* The attack bonus */
	int att_roll;			/* The attack roll (d20 value) */
	int evn;				/* The evasion bonus */
	int evn_roll;			/* The evasion roll (d20 value */
	
	int dd;					/* The number of damage dice */
	int ds;					/* The number of damage sides */
	int dam;				/* The total damage rolled */
	int pd;					/* The number of protection dice */
	int ps;					/* The number of protection sides */
	int prot;				/* The total protection rolled */

	int prt_percent;		/* The percentage of protection that is effective (eg 100 normally) */
	bool melee;				/* Was it a melee attack? (used for working out if blocking is effective) */
};

struct flavor_type
{
	u32b text;      /* Text (offset) */

	byte tval;      /* Associated object type */
	byte sval;      /* Associated object sub-type */

	byte d_attr;    /* Default flavor attribute */
	char d_char;    /* Default flavor character */

	byte x_attr;    /* Desired flavor attribute */
	char x_char;    /* Desired flavor character */
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
	char *buf;
};


/*structure of letter probabilitiesfor the random name generator*/
struct names_type
{
	u16b lprobs[S_WORD+1][S_WORD+1][S_WORD+1];
	u16b ltotal[S_WORD+1][S_WORD+1];
};

/*Information for object auto-inscribe*/
struct autoinscription
{
	s16b	kindIdx;
	s16b	inscriptionIdx;
};


